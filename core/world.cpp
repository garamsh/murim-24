#include "world.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "spatial_hash.hpp"

namespace murim {
namespace {

// SplitMix64 (Steele, Lea and Flood, 2014): the state advances by a fixed odd
// increment and the output is a bijective mix of it.
constexpr std::uint64_t SPLITMIX_INCREMENT = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t MIX_MULTIPLIER_1 = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t MIX_MULTIPLIER_2 = 0x94D049BB133111EBULL;
constexpr unsigned MIX_SHIFT_1 = 30U;
constexpr unsigned MIX_SHIFT_2 = 27U;
constexpr unsigned MIX_SHIFT_3 = 31U;
// Cells beyond its arrival ring a person may be jostled and still count as
// standing still.
constexpr std::uint32_t STANDING_SLACK = 2;

constexpr std::uint64_t UINT64_LIMIT =
    std::numeric_limits<std::uint64_t>::max();

// The damage a person takes in one tick is at most every other living
// person's strike.
static_assert(MAX_BODY_DAMAGE <= UINT64_LIMIT / MAX_PEOPLE,
              "the damage a person takes in a tick must fit 64 bits");
// A cooldown leaves half the range to the tick count it is added to.
static_assert(MAX_COOLDOWN <= UINT64_LIMIT / 2,
              "a cooldown must fit beside the tick count");

std::uint64_t mix(std::uint64_t value) {
  value = (value ^ (value >> MIX_SHIFT_1)) * MIX_MULTIPLIER_1;
  value = (value ^ (value >> MIX_SHIFT_2)) * MIX_MULTIPLIER_2;
  return value ^ (value >> MIX_SHIFT_3);
}

std::int64_t distance_squared(Position origin, Position point) {
  const std::int64_t offset_x = std::int64_t{point.x} - origin.x;
  const std::int64_t offset_y = std::int64_t{point.y} - origin.y;
  return (offset_x * offset_x) + (offset_y * offset_y);
}

// The range a separation, range or radius must lie in.
bool reachable(std::int32_t units) { return units >= 0 && units <= MAX_REACH; }

// The centre of each structure's footprint.
std::vector<Position> centres_of(const std::vector<Structure>& structures) {
  std::vector<Position> centres;
  centres.reserve(structures.size());
  for (const Structure& structure : structures) {
    centres.push_back(
        Position{(structure.cell.x * CELL_SIZE) +
                     (structure.footprint.width * CELL_SIZE / 2),
                 (structure.cell.y * CELL_SIZE) +
                     (structure.footprint.height * CELL_SIZE / 2)});
  }
  return centres;
}

// Blocks the footprint of every structure that blocks on `map`. Returns false
// at the first footprint of a size outside [1, MAX_FOOTPRINT] or covering a
// cell that is not passable or that an earlier structure covers.
bool block_footprints(Map& map, const std::vector<Structure>& structures) {
  const auto spans = [](std::int32_t side) {
    return side >= 1 && side <= MAX_FOOTPRINT;
  };
  Map covered = map;
  for (const Structure& structure : structures) {
    if (!spans(structure.footprint.width) ||
        !spans(structure.footprint.height)) {
      return false;
    }
    for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
      if (!covered.passable(cell)) {
        return false;
      }
      covered.block(cell);
      if (structure.blocks) {
        map.block(cell);
      }
    }
  }
  return true;
}

// Whether the disc of `radius` around `centre` reaches a sub-cell unit of the
// structure's footprint: the footprint's nearest point lies within it.
bool reaches(const Structure& structure, Position centre, std::int32_t radius) {
  const auto gap = [](std::int64_t point, std::int32_t first,
                      std::int32_t cells) {
    const std::int64_t low = std::int64_t{first} * CELL_SIZE;
    const std::int64_t high = ((std::int64_t{first} + cells) * CELL_SIZE) - 1;
    return std::max({low - point, std::int64_t{0}, point - high});
  };
  const std::int64_t gap_x =
      gap(centre.x, structure.cell.x, structure.footprint.width);
  const std::int64_t gap_y =
      gap(centre.y, structure.cell.y, structure.footprint.height);
  return (gap_x * gap_x) + (gap_y * gap_y) <=
         std::int64_t{radius} * std::int64_t{radius};
}

std::int32_t sign(std::int64_t value) {
  return static_cast<std::int32_t>(value > 0) -
         static_cast<std::int32_t>(value < 0);
}

// A push on a person, in sub-cell units along each axis.
struct Push {
  std::int64_t x;
  std::int64_t y;
};

// The integer square root, rounded down.
std::int64_t root(std::int64_t value) {
  std::int64_t low = 0;
  constexpr std::int64_t ABOVE_ANY_ROOT = std::int64_t{1} << 32;
  std::int64_t high = std::min<std::int64_t>(value, ABOVE_ANY_ROOT);
  while (low < high) {
    const std::int64_t middle = (low + high + 1) / 2;
    if (middle <= value / middle) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  return low;
}

// The push a person at `from` takes from one at `there`, closer than the
// separation: away along the line between them, the harder the closer. Two on
// one point part along `lean`.
Push repulsion(Position from, Position there, std::int64_t separation,
               Step lean) {
  const std::int64_t offset_x = std::int64_t{from.x} - there.x;
  const std::int64_t offset_y = std::int64_t{from.y} - there.y;
  const std::int64_t distance =
      root((offset_x * offset_x) + (offset_y * offset_y));
  if (distance == 0) {
    return Push{lean.x * separation, lean.y * separation};
  }
  const std::int64_t give = separation - distance;
  return Push{offset_x * give / distance, offset_y * give / distance};
}

// The push a person standing still at `from` takes from one at `there` on
// the move along `way`: it steps aside, across that way, to its own side of
// it, or, exactly in it, to `side`.
Push aside(Position from, Position there, std::int64_t separation, Step way,
           std::int64_t side) {
  const std::int64_t offset_x = std::int64_t{from.x} - there.x;
  const std::int64_t offset_y = std::int64_t{from.y} - there.y;
  const std::int64_t give =
      separation - root((offset_x * offset_x) + (offset_y * offset_y));
  const auto across = [side](std::int64_t offset) {
    return offset != 0 ? sign(offset) : side;
  };
  return Push{way.y != 0 ? across(offset_x) * give : 0,
              way.x != 0 ? across(offset_y) * give : 0};
}

// Moves from `from` one step along `direction`, one axis at a time, into
// cells the flow field reaches only.
Position stepped(const FlowField& field, Position from, Step direction) {
  const auto reached = [&field](Position position) {
    return field.distance(cell_of(position)) != FlowField::UNREACHABLE;
  };
  const Position along_x{from.x + (direction.x * SPEED), from.y};
  const Position moved_x = reached(along_x) ? along_x : from;
  const Position along_y{moved_x.x, moved_x.y + (direction.y * SPEED)};
  return reached(along_y) ? along_y : moved_x;
}

// Folds everything `person` holds into `fold`; `absent` stands for no soul.
template <typename Fold>
void fold_person(const Fold& fold, const Person& person, std::size_t absent) {
  fold(person.position.x);
  fold(person.position.y);
  fold(person.health);
  fold(person.ready_at);
  fold(person.home.has_value());
  fold(person.home.value_or(0));
  fold(person.born);
  fold(person.family);
  for (const std::size_t parent : person.parents) {
    fold(parent);
  }
  for (std::size_t discipline = 0; discipline < person.talent.size();
       ++discipline) {
    fold(person.talent.at(discipline).growth);
    fold(person.talent.at(discipline).limit);
    fold(person.attainment.at(discipline));
  }
  fold(person.lifespan.value_or(0));
  fold(person.soul.value_or(absent));
  fold(person.sex);
  fold(person.given);
  for (const std::size_t spouse : person.spouses) {
    fold(spouse);
  }
  for (const std::size_t child : person.children) {
    fold(child);
  }
  fold(person.hunger);
}

// Folds a soul's history into `fold`.
template <typename Fold>
void fold_soul(const Fold& fold, const Soul& soul) {
  fold(soul.lives);
  for (const bool practised : soul.practised) {
    fold(practised);
  }
}

// Whether a footprint spans 1 to MAX_FOOTPRINT cells along each side.
bool spans(Footprint footprint) {
  const auto side = [](std::int32_t cells) {
    return cells >= 1 && cells <= MAX_FOOTPRINT;
  };
  return side(footprint.width) && side(footprint.height);
}

// Whether `layout` places only known plans, inside `area`, over no other
// placement and off the area's centre.
bool lays_out(const Growth& growth) {
  const Footprint area = growth.settlement;
  std::vector<bool> covered(static_cast<std::size_t>(area.width) *
                                static_cast<std::size_t>(area.height),
                            false);
  const auto cover = [&area, &covered](Cell cell) {
    const bool inside = cell.x >= 0 && cell.y >= 0 && cell.x < area.width &&
                        cell.y < area.height;
    if (!inside) {
      return false;
    }
    const std::size_t slot = (static_cast<std::size_t>(cell.y) *
                              static_cast<std::size_t>(area.width)) +
                             static_cast<std::size_t>(cell.x);
    const bool free = !covered.at(slot);
    covered.at(slot) = true;
    return free;
  };
  const auto placed = [&growth, &cover](const Placement& placement) {
    if (placement.plan >= growth.plans.size()) {
      return false;
    }
    const std::vector<Cell> cells =
        cells_of(placement.offset, growth.plans.at(placement.plan).footprint);
    return std::all_of(cells.begin(), cells.end(), cover);
  };
  return std::all_of(growth.layout.begin(), growth.layout.end(), placed) &&
         cover(Cell{area.width / 2, area.height / 2});
}

// Throws std::invalid_argument when `growth` checks at no interval, presses
// above PER_MILLE, reaches outside [0, MAX_REACH], or lays a footprint or
// placement it cannot.
void check_growth(const Growth& growth) {
  const auto unknown = [&growth](std::size_t plan) {
    return plan >= growth.plans.size();
  };
  const bool reaches_out =
      !reachable(growth.reach) || !reachable(growth.vicinity) ||
      !reachable(growth.spacing) || !reachable(growth.claim);
  const bool misshapen =
      !spans(growth.settlement) ||
      std::any_of(growth.plans.begin(), growth.plans.end(),
                  [](const Plan& plan) { return !spans(plan.footprint); });
  if (growth.interval == 0 || growth.pressure > PER_MILLE || reaches_out ||
      misshapen ||
      std::any_of(growth.builds.begin(), growth.builds.end(), unknown) ||
      !lays_out(growth)) {
    throw std::invalid_argument(
        "growth must check at an interval, press within a thousand per "
        "mille, reach within MAX_REACH, and lay footprints of 1 to "
        "MAX_FOOTPRINT cells from known plans inside its settlement, apart "
        "and off its centre");
  }
}

// How many marked cells of a grid lie under a rectangle, read from a
// summed-area table in four lookups.
class Tally {
 public:
  Tally(const std::vector<bool>& marked, std::int32_t width,
        std::int32_t height)
      : width_(width),
        height_(height),
        sums_((static_cast<std::size_t>(width) + 1) *
                  (static_cast<std::size_t>(height) + 1),
              0) {
    for (Cell cell{0, 0}; cell.y < height; ++cell.y) {
      for (cell.x = 0; cell.x < width; ++cell.x) {
        const std::size_t index = (static_cast<std::size_t>(cell.y) *
                                   static_cast<std::size_t>(width)) +
                                  static_cast<std::size_t>(cell.x);
        sums_.at(slot(cell.x + 1, cell.y + 1)) =
            static_cast<std::int32_t>(marked.at(index)) +
            sums_.at(slot(cell.x, cell.y + 1)) +
            sums_.at(slot(cell.x + 1, cell.y)) - sums_.at(slot(cell.x, cell.y));
      }
    }
  }

  // Whether `area` from `corner` lies on the grid with no cell marked.
  [[nodiscard]] bool clear(Cell corner, Footprint area) const {
    return corner.x >= 0 && corner.y >= 0 && corner.x + area.width <= width_ &&
           corner.y + area.height <= height_ &&
           count(corner.x, corner.y, corner.x + area.width,
                 corner.y + area.height) == 0;
  }

  // Whether no cell within `margin` of `area` from `corner`, on the grid, is
  // marked.
  [[nodiscard]] bool clear_around(Cell corner, Footprint area,
                                  std::int32_t margin) const {
    return count(std::max(corner.x - margin, 0), std::max(corner.y - margin, 0),
                 std::min(corner.x + area.width + margin, width_),
                 std::min(corner.y + area.height + margin, height_)) == 0;
  }

 private:
  [[nodiscard]] std::size_t slot(std::int32_t column, std::int32_t row) const {
    return (static_cast<std::size_t>(row) *
            (static_cast<std::size_t>(width_) + 1)) +
           static_cast<std::size_t>(column);
  }

  // The marked cells in columns [left, right) of rows [top, bottom).
  [[nodiscard]] std::int32_t count(std::int32_t left, std::int32_t top,
                                   std::int32_t right,
                                   std::int32_t bottom) const {
    return sums_.at(slot(right, bottom)) - sums_.at(slot(left, bottom)) -
           sums_.at(slot(right, top)) + sums_.at(slot(left, top));
  }

  std::int32_t width_;
  std::int32_t height_;
  std::vector<std::int32_t> sums_;
};

// Twice the squared distance between the centre of `area` from `corner` and
// the centre of cell `seat`, in half cells, so it stays an integer.
std::int64_t remoteness(Cell corner, Footprint area, Cell seat) {
  const std::int64_t offset_x = (2 * std::int64_t{corner.x}) + area.width -
                                (2 * std::int64_t{seat.x}) - 1;
  const std::int64_t offset_y = (2 * std::int64_t{corner.y}) + area.height -
                                (2 * std::int64_t{seat.y}) - 1;
  return (offset_x * offset_x) + (offset_y * offset_y);
}

// Whether `there` lies within `reach` cells of `here` along each axis.
bool within(Cell here, Cell there, std::int32_t reach) {
  return std::abs(there.x - here.x) <= reach &&
         std::abs(there.y - here.y) <= reach;
}

// Throws std::invalid_argument when `life` could draw a talent above
// MAX_TALENT or a lifespan above MAX_LIFESPAN_DAYS, or gives a newborn no
// health or more than MAX_HEALTH.
void check_life(const Life& life) {
  const auto too_talented = [&life](const DisciplineDraw& draw) {
    return std::uint64_t{draw.growth.mean} + draw.growth.spread > MAX_TALENT ||
           std::uint64_t{draw.limit.mean} + draw.limit.spread +
                   life.discipline_widening + life.upper_widening >
               MAX_TALENT;
  };
  const bool too_long = life.lifespan && std::uint64_t{life.lifespan->mean} +
                                                 life.lifespan->spread >
                                             MAX_LIFESPAN_DAYS;
  if (std::any_of(life.disciplines.begin(), life.disciplines.end(),
                  too_talented) ||
      too_long || life.newborn_health == 0 ||
      life.newborn_health > MAX_HEALTH) {
    throw std::invalid_argument(
        "life must draw talents and lifespans within their limits, and give "
        "newborns health");
  }
  const bool too_old =
      life.founding_age &&
      std::uint64_t{life.founding_age->mean} + life.founding_age->spread >
          MAX_LIFESPAN_DAYS;
  const auto unkind = [](const Kinship& kinship) {
    const auto inverted = [](Range range) { return range.low > range.high; };
    return inverted(kinship.marrying_age) || inverted(kinship.bearing_age) ||
           kinship.marriage_chance > PER_MILLE ||
           kinship.birth_chance > PER_MILLE || kinship.neighbourhood < 0 ||
           kinship.neighbourhood > MAX_REACH;
  };
  if (too_old || (life.kinship && unkind(*life.kinship))) {
    throw std::invalid_argument(
        "a founding age must lie within the longest lifespan, and kinship's "
        "ages run upward, its chances within a thousand per mille and its "
        "neighbourhood within reach");
  }
  if (life.growth) {
    check_growth(*life.growth);
  }
}

// The range a spread draws over.
Range range_of(Spread values) {
  return Range{values.mean - std::min(values.mean, values.spread),
               values.mean + values.spread};
}

// Folds what `village` holds, its residents and who ruined it into `fold`.
template <typename Fold, typename Village>
void fold_village(const Fold& fold, const Village& village) {
  fold(village.spec.residents);
  fold(village.destroyer.has_value());
  fold(village.destroyer ? village.destroyer->army : 0);
  fold(village.destroyer ? village.destroyer->faction : 0);
  fold(village.stock);
  fold(village.party.has_value());
  fold(village.party.value_or(0));
  fold(village.raiding.has_value());
  fold(village.raiding.value_or(0));
  for (const Holding& holding : village.spec.holdings) {
    fold(holding.housing);
    fold(holding.storage);
    fold(holding.harvest);
  }
}

}  // namespace

World::World(Seed seed, Map map, std::vector<ArmySpec> armies,
             std::vector<Structure> structures, Rules rules,
             std::vector<VillageSpec> villages,
             std::vector<Leadership> leadership, Life life)
    : rng_state_(static_cast<std::uint64_t>(seed)),
      map_(std::move(map)),
      rules_(std::move(rules)),
      structures_(std::move(structures)),
      structure_index_(centres_of(structures_)),
      life_(std::move(life)) {
  check_life(life_);
  if (rules_.ticks_per_day == 0) {
    throw std::invalid_argument("a day must last at least one tick");
  }
  const std::size_t factions = rules_.hostile.size();
  const auto ragged = [factions](const std::vector<bool>& row) {
    return row.size() != factions;
  };
  const auto invalid_skill = [](const Skill& skill) {
    return !reachable(skill.range) || !reachable(skill.radius) ||
           skill.body_damage > MAX_BODY_DAMAGE || skill.cooldown > MAX_COOLDOWN;
  };
  const auto invalid_recruit = [this](const Recruit& recruit) {
    return recruit.health == 0 || recruit.health > MAX_HEALTH ||
           !map_.passable(recruit.cell);
  };
  const auto invalid_army = [&](const ArmySpec& army) {
    return army.skills.empty() || army.faction >= factions ||
           !map_.passable(army.objective) ||
           std::any_of(army.skills.begin(), army.skills.end(), invalid_skill) ||
           std::any_of(army.members.begin(), army.members.end(),
                       invalid_recruit);
  };
  // Footprints block their cells before any recruit is checked against them.
  const bool placed = block_footprints(map_, structures_);
  std::size_t recruits = 0;
  for (const ArmySpec& army : armies) {
    recruits += army.members.size();
  }
  if (!reachable(rules_.separation) || recruits > MAX_PEOPLE ||
      std::any_of(rules_.hostile.begin(), rules_.hostile.end(), ragged) ||
      !placed || std::any_of(armies.begin(), armies.end(), invalid_army)) {
    throw std::invalid_argument(
        "separation, range and radius must lie in [0, MAX_REACH], hostility "
        "must be square, the world must hold at most MAX_PEOPLE people, each "
        "with health up to MAX_HEALTH on a passable cell, every army must "
        "have skills within MAX_BODY_DAMAGE and MAX_COOLDOWN and a known "
        "faction, and every structure must cover 1 to MAX_FOOTPRINT cells "
        "along each side, all passable and covered by no other structure");
  }
  for (const Structure& structure : structures_) {
    structure_extent_ =
        std::max({structure_extent_, structure.footprint.width * CELL_SIZE,
                  structure.footprint.height * CELL_SIZE});
  }
  // The founders who start as households, by army: two for each household
  // of the village the army houses.
  std::vector<std::size_t> paired(armies.size(), 0);
  for (const VillageSpec& spec : villages) {
    if (spec.residents < armies.size()) {
      paired.at(spec.residents) = std::size_t{2} * spec.households;
    }
  }
  std::size_t families = armies.size();
  for (ArmySpec& army : armies) {
    enlist(army, paired.at(armies_.size()), families);
  }
  // Each family, one for every army the world starts with and every head of
  // a household, takes a surname.
  for (std::size_t family = 0;
       family < families && !life_.names.surnames.empty(); ++family) {
    surnames_.push_back(next_random() % life_.names.surnames.size());
  }
  village_of_.resize(structures_.size());
  plans_.resize(structures_.size());
  if (life_.growth && life_.growth->stances.size() > rules_.hostile.size()) {
    throw std::invalid_argument(
        "growth must give stances for no more factions than hostility names");
  }
  for (VillageSpec& spec : villages) {
    add_village(std::move(spec));
  }
  const auto unknown = [this](const Leadership& faction) {
    return faction.leader && *faction.leader >= people_.size();
  };
  if (leadership.size() > rules_.hostile.size() ||
      std::any_of(leadership.begin(), leadership.end(), unknown)) {
    throw std::invalid_argument(
        "leaders must be people the world creates, of no more factions than "
        "hostility names");
  }
  leadership_ = std::move(leadership);
  leadership_.resize(rules_.hostile.size());
}

// Creates the army `army` describes, its members born of no parents, by the
// same rule as any birth, into the family of the army; the first `pairs` of
// them, two to a household, are a man and a woman who each head a family of
// their own, numbered from `families` on, and marry. Founders were born
// before the world began, by the founding age.
void World::enlist(ArmySpec& army, std::size_t pairs, std::size_t& families) {
  std::vector<std::size_t> every_skill(army.skills.size());
  std::iota(every_skill.begin(), every_skill.end(), std::size_t{0});
  std::vector<std::size_t> roster;
  for (const Recruit& recruit : army.members) {
    const bool housed = roster.size() < pairs;
    const bool wife = housed && roster.size() % 2 == 1;
    std::optional<Sex> sex;
    if (housed) {
      sex = wife ? Sex::FEMALE : Sex::MALE;
    }
    roster.push_back(people_.size());
    Person person;
    person.position = centre_of(recruit.cell);
    person.health = recruit.health;
    person.family = housed ? families++ : armies_.size();
    found(person, sex);
    if (wife) {
      const std::size_t husband = people_.size() - 1;
      person.spouses.push_back(husband);
      people_.at(husband).spouses.push_back(people_.size());
    }
    people_.push_back(std::move(person));
  }
  FlowField field{map_, army.objective};
  reachable_from(field, roster);
  armies_.push_back(Army{army.faction, std::move(army.skills),
                         Order{army.objective, std::move(every_skill)},
                         std::move(field), std::move(roster)});
}

// Adds the village `spec` describes, its storage full, making it its
// residents' home. Throws std::invalid_argument when it names a structure
// that does not exist or another village names, an army that does not exist
// or another village houses, or holdings that are not one for each of its
// structures, or more households than its residents pair into.
void World::add_village(VillageSpec spec) {
  const auto claimed = [this](std::size_t structure) {
    return structure >= village_of_.size() ||
           village_of_.at(structure).has_value();
  };
  const auto housed = [&spec](const Village& village) {
    return village.spec.residents == spec.residents;
  };
  const bool unheld =
      !spec.holdings.empty() && spec.holdings.size() != spec.structures.size();
  if (spec.residents >= armies_.size() ||
      std::any_of(spec.structures.begin(), spec.structures.end(), claimed) ||
      std::any_of(villages_.begin(), villages_.end(), housed) || unheld ||
      std::size_t{2} * spec.households >
          armies_.at(spec.residents).roster.size()) {
    throw std::invalid_argument(
        "a village must name existing structures no other village names, "
        "an existing army no other village houses as its residents, what "
        "each of its structures holds, if anything, and no more households "
        "than its residents pair into");
  }
  for (const std::size_t structure : spec.structures) {
    village_of_.at(structure) = villages_.size();
  }
  for (const std::size_t member : armies_.at(spec.residents).roster) {
    people_.at(member).home = villages_.size();
  }
  villages_.push_back(Village{std::move(spec), std::nullopt, 0, std::nullopt,
                              std::nullopt, std::nullopt});
  // A village starts with its storage full.
  villages_.back().stock = held(villages_.size() - 1, Stage::DAMAGED).storage;
}

// Throws when one of `members` stands where `field` cannot reach.
void World::reachable_from(const FlowField& field,
                           const std::vector<std::size_t>& members) const {
  const auto cut_off = [this, &field](std::size_t member) {
    return field.distance(cell_of(people_.at(member).position)) ==
           FlowField::UNREACHABLE;
  };
  if (std::any_of(members.begin(), members.end(), cut_off)) {
    throw std::invalid_argument(
        "an army's objective must be reachable from where each member stands");
  }
}

void World::order(std::size_t army, Order order,
                  std::optional<std::size_t> challenge) {
  const auto lacking = [this, army](std::size_t skill) {
    return skill >= armies_.at(army).skills.size();
  };
  if (army >= armies_.size() || !map_.passable(order.objective) ||
      order.skills.empty() ||
      std::any_of(order.skills.begin(), order.skills.end(), lacking)) {
    throw std::invalid_argument(
        "an order must name an army, a passable objective and at least one "
        "of the army's skills");
  }
  Army& ordered = armies_.at(army);
  FlowField field{map_, order.objective};
  reachable_from(field, ordered.roster);
  if (challenge) {
    start_duel(army, *challenge);
  }
  ordered.field = std::move(field);
  ordered.order = std::move(order);
}

// Starts a duel between army `army`'s lead and faction `faction`'s leader, or
// throws when the faction's succession refuses the challenge.
void World::start_duel(std::size_t army, std::size_t faction) {
  const ChallengeRefusal refused = challenge_refusal(army, faction);
  const Leadership none{};
  const Leadership& led =
      faction < leadership_.size() ? leadership_.at(faction) : none;
  if (refused != ChallengeRefusal::NONE || !led.leader || !led.succession) {
    throw std::invalid_argument(
        "a challenge must name a faction that passes its lead by duel and has "
        "a leader, from an army whose lead may challenge it, outside a duel");
  }
  duels_.push_back(Fight{faction, armies_.at(army).roster.front(), *led.leader,
                         *led.succession});
}

ChallengeRefusal World::challenge_refusal(std::size_t army,
                                          std::size_t faction) const {
  if (army >= armies_.size()) {
    throw std::invalid_argument("a challenge must come from an army");
  }
  return refusal(armies_.at(army), faction);
}

ChallengeRefusal World::refusal(const Army& challenging,
                                std::size_t faction) const {
  const Leadership none{};
  const Leadership& led =
      faction < leadership_.size() ? leadership_.at(faction) : none;
  if (!led.succession) {
    return ChallengeRefusal::NO_SUCCESSION;
  }
  if (!led.leader) {
    return ChallengeRefusal::NO_LEADER;
  }
  if (challenging.roster.empty()) {
    return ChallengeRefusal::NO_CHALLENGER;
  }
  if (led.succession->members_only && challenging.faction != faction) {
    return ChallengeRefusal::NOT_A_MEMBER;
  }
  if (challenging.roster.front() == *led.leader) {
    return ChallengeRefusal::IS_LEADER;
  }
  if (duel(faction)) {
    return ChallengeRefusal::DUEL_IN_PROGRESS;
  }
  return ChallengeRefusal::NONE;
}

std::optional<Duellists> World::duel(std::size_t faction) const {
  const auto fought = [faction](const Fight& fight) {
    return fight.faction == faction;
  };
  const auto found = std::find_if(duels_.begin(), duels_.end(), fought);
  if (found == duels_.end()) {
    return std::nullopt;
  }
  return Duellists{found->challenger, found->leader};
}

void World::conscript(std::size_t person, std::size_t army) {
  const auto serving = [person](const Army& candidate) {
    return std::find(candidate.roster.begin(), candidate.roster.end(),
                     person) != candidate.roster.end();
  };
  const auto from = std::find_if(armies_.begin(), armies_.end(), serving);
  if (army >= armies_.size() || from == armies_.end()) {
    throw std::invalid_argument(
        "only a living person can be conscripted, into an existing army");
  }
  reachable_from(armies_.at(army).field, {person});
  from->roster.erase(
      std::find(from->roster.begin(), from->roster.end(), person));
  armies_.at(army).roster.push_back(person);
}

void World::disband(std::size_t army) {
  const std::vector<std::size_t> members = armies_.at(army).roster;
  for (const std::size_t member : members) {
    const std::optional<std::size_t>& home = people_.at(member).home;
    if (home) {
      conscript(member, villages_.at(*home).spec.residents);
    }
  }
}

void World::tick() {
  rng_state_ += SPLITMIX_INCREMENT;
  ++tick_count_;
  std::vector<Position> positions;
  std::vector<std::size_t> people;
  std::vector<std::size_t> owners;
  // Each living person's index in the crowd.
  std::vector<std::size_t> slot(people_.size());
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      slot.at(member) = people.size();
      positions.push_back(people_.at(member).position);
      people.push_back(member);
      owners.push_back(army);
    }
  }
  const Crowd crowd{SpatialHash{std::move(positions)}, std::move(people),
                    std::move(owners)};
  std::vector<std::uint64_t> damage(crowd.people.size(), 0);
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    if (people_.at(crowd.people.at(index)).ready_at <= tick_count_) {
      strike(index, crowd, damage);
    }
  }
  // Damage lands after everyone has struck, so the order people strike in
  // does not decide who strikes. The dead leave their army's roster.
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    Person& struck = people_.at(crowd.people.at(index));
    struck.health -= std::min(damage.at(index), struck.health);
  }
  settle_duels();
  age();
  if (life_.husbandry && tick_count_ % rules_.ticks_per_day == 0) {
    husband(*life_.husbandry);
  }
  // A soul whose body died this tick is born again, into the army it served.
  const std::vector<Passing> reborn = passings();
  for (Army& army : armies_) {
    army.roster.erase(std::remove_if(army.roster.begin(), army.roster.end(),
                                     [this](std::size_t member) {
                                       return people_.at(member).health == 0;
                                     }),
                      army.roster.end());
  }
  // Each person's pull toward its objective, from where it stood: none
  // within its arrival ring, as many cells from the objective as the square
  // root of its rank in its army. A person within two cells of its ring
  // counts as standing still, so a crowd jostled a little stays standing.
  std::vector<Step> pulls(crowd.people.size(), Step{0, 0});
  std::vector<bool> standing(crowd.people.size(), true);
  for (const Army& army : armies_) {
    for (std::size_t rank = 0; rank < army.roster.size(); ++rank) {
      const std::size_t index = slot.at(army.roster.at(rank));
      const Cell cell = cell_of(crowd.hash.position(index));
      const auto ring =
          static_cast<std::uint32_t>(root(static_cast<std::int64_t>(rank)));
      const std::uint32_t distance = army.field.distance(cell);
      pulls.at(index) = distance > ring ? army.field.step(cell) : Step{0, 0};
      standing.at(index) = distance <= ring + STANDING_SLACK;
    }
  }
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    march(army, crowd, slot, pulls, standing);
  }
  // The newborn first act on the next tick.
  for (const Passing& passing : reborn) {
    rebirth(passing);
  }
  const std::uint64_t day = tick_count_ / rules_.ticks_per_day;
  if (life_.growth && tick_count_ % rules_.ticks_per_day == 0 &&
      day % life_.growth->interval == 0) {
    grow(*life_.growth);
  }
  if (life_.kinship && tick_count_ % rules_.ticks_per_day == 0) {
    kin(*life_.kinship);
  }
}

// The people carrying a soul who died this tick, each with the army it
// served in.
std::vector<World::Passing> World::passings() const {
  std::vector<Passing> reborn;
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      const Person& person = people_.at(member);
      if (person.health == 0 && person.soul) {
        reborn.push_back(Passing{member, army});
      }
    }
  }
  return reborn;
}

// Each member steps away from the people closer to it than the separation,
// the harder the closer each is, and otherwise along its pull. A person on the
// move does not give way to people standing still, who give way to it, so a
// standing crowd parts for a person walking out of it, nor to anyone behind
// it, so a crowd queues through a gap. A step enters any cell
// the flow field reaches. Positions are read from `crowd`, as the tick began,
// so the order people move in decides nothing.
void World::march(std::size_t army, const Crowd& crowd,
                  const std::vector<std::size_t>& slot,
                  const std::vector<Step>& pulls,
                  const std::vector<bool>& standing) {
  const Army& marching = armies_.at(army);
  const std::int64_t separation = rules_.separation;
  for (const std::size_t member : marching.roster) {
    const std::size_t self = slot.at(member);
    const Position from = crowd.hash.position(self);
    // Two people on one point part along x, the lower index westward, and a
    // person exactly in a mover's way steps aside to the side its index
    // fixes.
    const std::int64_t side = (member % 2 == 0) ? 1 : -1;
    const Step pull = pulls.at(self);
    Push push{0, 0};
    crowd.hash.visit_within(from, rules_.separation, [&](std::size_t other) {
      const Position there = crowd.hash.position(other);
      const std::int64_t ahead = ((std::int64_t{there.x} - from.x) * pull.x) +
                                 ((std::int64_t{there.y} - from.y) * pull.y);
      const bool ignored = other == self || (!standing.at(self) &&
                                             (standing.at(other) || ahead < 0));
      const bool yields = standing.at(self) && !standing.at(other);
      const Step lean{crowd.people.at(other) < member ? 1 : -1, 0};
      const Push felt =
          yields ? aside(from, there, separation, pulls.at(other), side)
                 : repulsion(from, there, separation, lean);
      push.x += ignored ? 0 : felt.x;
      push.y += ignored ? 0 : felt.y;
    });
    const Step direction{sign((push.x * 2) + pull.x),
                         sign((push.y * 2) + pull.y)};
    people_.at(member).position = stepped(marching.field, from, direction);
  }
}

StateHash World::hash() const {
  std::uint64_t digest = mix(rng_state_ ^ mix(tick_count_));
  const auto fold = [&digest](auto value) {
    digest = mix(digest ^ static_cast<std::uint64_t>(value));
  };
  fold(rules_.separation);
  for (const std::vector<bool>& row : rules_.hostile) {
    for (const bool hostile : row) {
      fold(hostile);
    }
  }
  for (const std::uint32_t floor : rules_.stage_floors) {
    fold(floor);
  }
  for (Cell cell{0, 0}; cell.y < map_.height(); ++cell.y) {
    for (cell.x = 0; cell.x < map_.width(); ++cell.x) {
      fold(map_.passable(cell));
    }
  }
  for (const Structure& structure : structures_) {
    fold(structure.cell.x);
    fold(structure.cell.y);
    fold(structure.footprint.width);
    fold(structure.footprint.height);
    fold(structure.durability);
    fold(structure.blocks);
  }
  for (const Person& person : people_) {
    fold_person(fold, person, people_.size());
  }
  for (const Soul& soul : souls_) {
    fold_soul(fold, soul);
  }
  for (const std::size_t surname : surnames_) {
    fold(surname);
  }
  fold(rules_.ticks_per_day);
  for (const Army& army : armies_) {
    fold(army.order.objective.x);
    fold(army.order.objective.y);
    for (const std::size_t skill : army.order.skills) {
      fold(skill);
    }
    fold(army.faction);
    for (const Skill& skill : army.skills) {
      fold(skill.range);
      fold(skill.radius);
      fold(skill.body_damage);
      fold(skill.structure_damage);
      fold(skill.cooldown);
    }
    fold(army.roster.size());
    for (const std::size_t member : army.roster) {
      fold(member);
    }
  }
  for (const Leadership& faction : leadership_) {
    fold(faction.leader.has_value());
    fold(faction.leader.value_or(0));
    const Duel rule = faction.succession.value_or(Duel{false, 0, false});
    fold(faction.succession.has_value());
    fold(rule.members_only);
    fold(rule.yield_health);
    fold(rule.allies_turn_hostile);
  }
  for (const Fight& fight : duels_) {
    fold(fight.faction);
    fold(fight.challenger);
    fold(fight.leader);
  }
  for (const Village& village : villages_) {
    fold_village(fold, village);
  }
  return StateHash{digest};
}

std::size_t World::population() const { return people_.size(); }

std::size_t World::living() const {
  std::size_t living = 0;
  for (const Army& army : armies_) {
    living += army.roster.size();
  }
  return living;
}

std::optional<Name> World::name(std::size_t person) const {
  const Person& named = people_.at(person);
  const std::vector<NameEntry>& given =
      named.sex == Sex::FEMALE ? life_.names.female : life_.names.male;
  if (named.family >= surnames_.size() || given.empty()) {
    return std::nullopt;
  }
  return Name{life_.names.surnames.at(surnames_.at(named.family)),
              given.at(named.given)};
}

const Person& World::person(std::size_t person) const {
  return people_.at(person);
}

const std::vector<std::size_t>& World::roster(std::size_t army) const {
  return armies_.at(army).roster;
}

const Structure& World::structure(std::size_t structure) const {
  return structures_.at(structure);
}

VillageState World::village_state(std::size_t village) const {
  const VillageSpec& spec = villages_.at(village).spec;
  const auto all_in = [this, &spec](Stage wanted) {
    return std::all_of(
        spec.structures.begin(), spec.structures.end(),
        [this, wanted](std::size_t index) { return stage(index) == wanted; });
  };
  if (all_in(Stage::RUBBLE)) {
    return VillageState::RUINS;
  }
  const std::size_t owned_by = owner(village);
  const auto threatens = [this, &spec, owned_by](const Army& army) {
    return rules_.hostile.at(army.faction).at(owned_by) &&
           std::any_of(army.skills.begin(), army.skills.end(),
                       [this, &spec, &army](const Skill& skill) {
                         return skill.structure_damage > 0 &&
                                within_reach(army, spec,
                                             skill.range + skill.radius);
                       });
  };
  if (std::any_of(armies_.begin(), armies_.end(), threatens)) {
    return VillageState::RAIDED;
  }
  return all_in(Stage::INTACT) ? VillageState::PROSPEROUS
                               : VillageState::REBUILDING;
}

// Whether a living member of `army` stands within `reach` of a structure of
// the village.
bool World::within_reach(const Army& army, const VillageSpec& spec,
                         std::int32_t reach) const {
  return std::any_of(
      army.roster.begin(), army.roster.end(), [&](std::size_t member) {
        return std::any_of(spec.structures.begin(), spec.structures.end(),
                           [&](std::size_t index) {
                             return reaches(structures_.at(index),
                                            people_.at(member).position, reach);
                           });
      });
}

bool World::duelling(std::size_t one, std::size_t other) const {
  return std::any_of(
      duels_.begin(), duels_.end(), [one, other](const Fight& fight) {
        return (fight.challenger == one && fight.leader == other) ||
               (fight.challenger == other && fight.leader == one);
      });
}

// Ends every duel in which a duellist has no more than its faction's yield
// health left: the leader keeps the lead unless only the challenger stands,
// who then leads, and the factions at peace with it turn hostile to it if
// the duel says so.
void World::settle_duels() {
  const auto down = [this](const Fight& fight, std::size_t duellist) {
    return people_.at(duellist).health <= fight.rule.yield_health;
  };
  const auto fallen = [&down](const Fight& fight) {
    return down(fight, fight.challenger) || down(fight, fight.leader);
  };
  for (const Fight& fight : duels_) {
    const Duel& rule = fight.rule;
    const bool won =
        down(fight, fight.leader) && !down(fight, fight.challenger);
    if (!won) {
      continue;
    }
    leadership_.at(fight.faction).leader = fight.challenger;
    std::vector<std::vector<bool>>& hostility = rules_.hostile;
    for (std::size_t other = 0; other < hostility.size(); ++other) {
      const bool ally = other != fight.faction &&
                        !hostility.at(other).at(fight.faction) &&
                        !hostility.at(fight.faction).at(other);
      hostility.at(other).at(fight.faction) =
          hostility.at(other).at(fight.faction) ||
          (ally && rule.allies_turn_hostile);
    }
  }
  duels_.erase(std::remove_if(duels_.begin(), duels_.end(), fallen),
               duels_.end());
}

std::optional<std::size_t> World::leader(std::size_t faction) const {
  return leadership_.at(faction).leader;
}

bool World::hostile(std::size_t from, std::size_t target) const {
  return rules_.hostile.at(from).at(target);
}

std::size_t World::owner(std::size_t village) const {
  return armies_.at(villages_.at(village).spec.residents).faction;
}

std::optional<Destroyer> World::destroyer(std::size_t village) const {
  return villages_.at(village).destroyer;
}

std::optional<Settlement> World::settlement(std::size_t village) const {
  const std::optional<Cell> corner = villages_.at(village).founded;
  if (!corner || !life_.growth) {
    return std::nullopt;
  }
  return Settlement{life_.growth->settlement_kind, *corner,
                    life_.growth->settlement};
}

Stage World::stage(std::size_t structure) const {
  const std::uint32_t durability = structures_.at(structure).durability;
  return static_cast<Stage>(std::count_if(
      rules_.stage_floors.begin(), rules_.stage_floors.end(),
      [durability](std::uint32_t floor) { return durability < floor; }));
}

std::uint64_t World::next_random() {
  rng_state_ += SPLITMIX_INCREMENT;
  return mix(rng_state_);
}

// The one rule every skill resolves through, whoever uses it: each use hits
// every hostile person in its disc once, and every structure its disc
// reaches.
void World::strike(std::size_t user, const Crowd& crowd,
                   std::vector<std::uint64_t>& damage) {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  Person& striker = people_.at(self);
  const Army& own = armies_.at(army);
  const std::vector<std::size_t>& choice = own.order.skills;
  const Skill& skill = own.skills.at(choice.at(next_random() % choice.size()));
  // Duellists are hostile to each other whatever their factions.
  const auto hostile = [this, &own, &crowd, self](std::size_t other) {
    return rules_.hostile.at(own.faction)
               .at(armies_.at(crowd.armies.at(other)).faction) ||
           duelling(self, crowd.people.at(other));
  };
  // The nearest hostile person, the lowest index among equals, so the order
  // the query visits people in does not decide the target.
  const auto nearest_hostile = [&]() -> std::optional<Position> {
    std::optional<std::size_t> target;
    std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
    crowd.hash.visit_within(
        striker.position, skill.range, [&](std::size_t other) {
          const std::int64_t distance =
              distance_squared(striker.position, crowd.hash.position(other));
          const bool closer = distance < nearest || (distance == nearest &&
                                                     target && other < *target);
          if (hostile(other) && closer) {
            nearest = distance;
            target = other;
          }
        });
    return target ? std::optional<Position>{crowd.hash.position(*target)}
                  : std::nullopt;
  };
  // A skill of range 0 reaches only its user, so it lands where it stands.
  const std::optional<Position> aim =
      skill.range == 0 ? std::optional<Position>{striker.position}
                       : nearest_hostile();
  if (!aim) {
    return;
  }
  const Position centre = *aim;
  striker.ready_at = tick_count_ + std::max<std::uint64_t>(skill.cooldown, 1);
  // A use that hits anyone or anything practises the striker's best
  // discipline, its highest limit.
  bool hit = false;
  crowd.hash.visit_within(centre, skill.radius, [&](std::size_t other) {
    if (hostile(other)) {
      damage.at(other) += skill.body_damage;
      hit = true;
    }
  });
  structure_index_.visit_within(
      centre, skill.radius + structure_extent_, [&](std::size_t index) {
        if (reaches(structures_.at(index), centre, skill.radius)) {
          hit = shift(index, Destroyer{army, own.faction},
                      skill.structure_damage) ||
                hit;
        }
      });
  if (hit && !striker.talent.empty()) {
    const auto best = static_cast<std::size_t>(
        std::max_element(striker.talent.begin(), striker.talent.end(),
                         [](const Aptitude& left, const Aptitude& right) {
                           return left.limit < right.limit;
                         }) -
        striker.talent.begin());
    const Aptitude& aptitude = striker.talent.at(best);
    striker.attainment.at(best) =
        std::min(aptitude.limit, striker.attainment.at(best) + aptitude.growth);
  }
}

// Lowers a structure's durability by `effect`, or raises it by a negative
// one up to the intact floor, never lowering one above that floor by a
// repair. In a village with living residents an army damages only if it is
// hostile to the owner and repairs only if it is not; an empty village, or a
// structure in none, takes either. A village notes the army whose strike left
// its last standing structure in rubble, and takes as its residents the army
// that restores a structure while its residents have no living member; that
// army's members make it their home.
bool World::shift(std::size_t structure, Destroyer striker,
                  std::int64_t effect) {
  const std::optional<std::size_t>& home = village_of_.at(structure);
  const bool held =
      home && !armies_.at(villages_.at(*home).spec.residents).roster.empty();
  if (held &&
      (effect > 0) != rules_.hostile.at(striker.faction).at(owner(*home))) {
    return false;
  }
  Structure& shifted = structures_.at(structure);
  const std::int64_t before = shifted.durability;
  const bool stood = stage(structure) != Stage::RUBBLE;
  const std::int64_t ceiling =
      std::max<std::int64_t>(before, rules_.stage_floors.front());
  shifted.durability = static_cast<std::uint32_t>(
      std::clamp(before - effect, std::int64_t{0}, ceiling));
  if (!home) {
    return effect != 0;
  }
  Village& village = villages_.at(*home);
  const std::vector<std::size_t>& own = village.spec.structures;
  const bool ruined = std::all_of(
      own.begin(), own.end(),
      [this](std::size_t index) { return stage(index) == Stage::RUBBLE; });
  if (stood && ruined) {
    village.destroyer = striker;
  }
  if (shifted.durability > before &&
      armies_.at(village.spec.residents).roster.empty()) {
    village.spec.residents = striker.army;
    for (const std::size_t member : armies_.at(striker.army).roster) {
      people_.at(member).home = home;
    }
  }
  return effect != 0;
}

Range limit_range(const Life& life, const Soul& soul, std::size_t discipline) {
  const Spread& limit = life.disciplines.at(discipline).limit;
  const std::uint32_t lives = std::max<std::uint32_t>(soul.lives, 1);
  const bool practised =
      discipline < soul.practised.size() && soul.practised.at(discipline);
  const std::uint32_t wider = practised ? life.discipline_widening / lives : 0;
  const std::uint32_t higher = soul.lives > 0 ? life.upper_widening / lives : 0;
  const std::uint32_t reach = limit.spread + wider;
  return Range{limit.mean - std::min(limit.mean, reach),
               limit.mean + reach + higher};
}

// Draws `person`'s talent and lifespan, for a soul of `soul` history, and
// its sex unless `sex` gives it: the one rule every birth follows.
void World::bear(Person& person, const Soul& soul, std::optional<Sex> sex) {
  person.born = static_cast<std::int64_t>(tick_count_);
  person.talent.clear();
  for (std::size_t discipline = 0; discipline < life_.disciplines.size();
       ++discipline) {
    const std::uint32_t growth =
        draw(range_of(life_.disciplines.at(discipline).growth));
    const std::uint32_t limit = draw(limit_range(life_, soul, discipline));
    person.talent.push_back(Aptitude{growth, limit});
  }
  person.attainment.assign(person.talent.size(), 0);
  person.lifespan =
      life_.lifespan
          ? std::optional<std::uint64_t>{draw(range_of(*life_.lifespan))}
          : std::nullopt;
  if (sex) {
    person.sex = *sex;
  } else {
    person.sex = next_random() % 2 == 0 ? Sex::FEMALE : Sex::MALE;
  }
  const std::vector<NameEntry>& given =
      person.sex == Sex::FEMALE ? life_.names.female : life_.names.male;
  person.given = given.empty() ? 0 : next_random() % given.size();
}

// Bears founder `person`, of `sex` if given, by the one birth rule, born the
// founding age before the world began, if the life gives one.
void World::found(Person& person, std::optional<Sex> sex) {
  bear(person, Soul{}, sex);
  if (life_.founding_age) {
    person.born = -static_cast<std::int64_t>(
        std::uint64_t{draw(range_of(*life_.founding_age))} *
        rules_.ticks_per_day);
  }
}

// A draw from the seed over `range`.
std::uint32_t World::draw(Range range) {
  return range.low +
         static_cast<std::uint32_t>(
             next_random() % (std::uint64_t{range.high - range.low} + 1));
}

std::int64_t World::age_in_days(const Person& person) const {
  return (static_cast<std::int64_t>(tick_count_) - person.born) /
         static_cast<std::int64_t>(rules_.ticks_per_day);
}

// Ends the life of every living person whose age in days has passed its
// lifespan, extended by its attainment.
void World::age() {
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      Person& person = people_.at(member);
      std::uint64_t attained = 0;
      for (const std::uint32_t points : person.attainment) {
        attained += points;
      }
      const auto age = static_cast<std::uint64_t>(age_in_days(person));
      const bool spent =
          person.lifespan &&
          age >= *person.lifespan + (attained * life_.days_per_attainment);
      person.health = spent ? 0 : person.health;
    }
  }
}

// Bears person `dead`'s soul again in a village drawn by the seed among those
// with living residents: into the family of its first resident, whose first
// two residents are its parents, at the first's side, home in that village,
// serving in army `army` and bound for where it was born. The army takes the
// village's residents' faction and skills, all of them in its order. The soul
// first counts the life just ended and the disciplines practised in it.
void World::rebirth(Passing passing) {
  const std::size_t dead = passing.dead;
  const std::size_t army = passing.army;
  const std::optional<std::size_t> carried = people_.at(dead).soul;
  if (!carried) {
    return;
  }
  const std::size_t soul_index = *carried;
  Soul& soul = souls_.at(soul_index);
  soul.lives += 1;
  soul.practised.resize(life_.disciplines.size(), false);
  const std::vector<std::uint32_t>& attainment = people_.at(dead).attainment;
  for (std::size_t discipline = 0; discipline < attainment.size();
       ++discipline) {
    soul.practised.at(discipline) =
        soul.practised.at(discipline) || attainment.at(discipline) > 0;
  }
  std::vector<std::size_t> peopled;
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    if (!armies_.at(villages_.at(village).spec.residents).roster.empty()) {
      peopled.push_back(village);
    }
  }
  if (peopled.empty()) {
    return;
  }
  const std::size_t village = peopled.at(next_random() % peopled.size());
  const std::vector<std::size_t>& residents =
      armies_.at(villages_.at(village).spec.residents).roster;
  Person person;
  const Person& first = people_.at(residents.front());
  person.position = first.position;
  person.health = life_.newborn_health;
  person.home = village;
  person.family = first.family;
  person.parents.assign(
      residents.begin(),
      residents.begin() + static_cast<std::ptrdiff_t>(
                              std::min<std::size_t>(residents.size(), 2)));
  person.soul = soul_index;
  bear(person, soul);
  const std::size_t born = people_.size();
  for (const std::size_t parent : person.parents) {
    people_.at(parent).children.push_back(born);
  }
  people_.push_back(std::move(person));
  // The soul's army takes its new family's faction and skills.
  Army& serving = armies_.at(army);
  const Army& family = armies_.at(villages_.at(village).spec.residents);
  serving.roster.push_back(born);
  serving.faction = family.faction;
  serving.skills = family.skills;
  serving.order.skills.resize(serving.skills.size());
  std::iota(serving.order.skills.begin(), serving.order.skills.end(),
            std::size_t{0});
  const Cell birthplace = cell_of(people_.at(born).position);
  serving.field = FlowField{map_, birthplace};
  serving.order.objective = birthplace;
}

std::size_t World::faction(std::size_t army) const {
  return armies_.at(army).faction;
}

const std::vector<Skill>& World::skills(std::size_t army) const {
  return armies_.at(army).skills;
}

std::size_t World::ensoul(std::size_t person) {
  if (person >= people_.size() || people_.at(person).health == 0) {
    throw std::invalid_argument("only a living person can carry a soul");
  }
  people_.at(person).soul = souls_.size();
  souls_.push_back(Soul{0, std::vector<bool>(life_.disciplines.size(), false)});
  return souls_.size() - 1;
}

const Soul& World::soul(std::size_t soul) const { return souls_.at(soul); }

std::int64_t World::age_in_years(std::size_t person) const {
  return age_in_days(people_.at(person)) /
         static_cast<std::int64_t>(DAYS_PER_YEAR);
}

std::optional<std::size_t> World::faction_of(std::size_t person) const {
  for (const Army& army : armies_) {
    if (std::find(army.roster.begin(), army.roster.end(), person) !=
        army.roster.end()) {
      return army.faction;
    }
  }
  return std::nullopt;
}

Date World::date() const {
  const std::uint64_t day = tick_count_ / rules_.ticks_per_day;
  const std::uint64_t season = day / CALENDAR.days_per_season;
  return Date{(season / CALENDAR.seasons_per_year) + 1,
              season % CALENDAR.seasons_per_year,
              (day % CALENDAR.days_per_season) + 1};
}

// Once a day: those free to marry marry by the seed, then wives bear
// children, each in order of person.
void World::kin(const Kinship& kinship) {
  std::vector<std::size_t> alive;
  std::vector<std::size_t> army_of(people_.size());
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      alive.push_back(member);
      army_of.at(member) = army;
    }
  }
  std::sort(alive.begin(), alive.end());
  std::vector<std::size_t> population = homes();
  const auto chance = [this](std::uint64_t per_mille) {
    return next_random() % (std::uint64_t{PER_MILLE} * DAYS_PER_YEAR) <
           per_mille;
  };
  for (const std::size_t person : alive) {
    if (free_to_marry(person, kinship) && chance(kinship.marriage_chance)) {
      marry(person, alive, kinship);
    }
  }
  for (const std::size_t mother : alive) {
    if (!may_bear(mother, kinship) || living() >= MAX_PEOPLE) {
      continue;
    }
    // Births slow as the child's village nears its cap, and stop at it.
    const std::size_t lineage =
        kinship.fathers_family ? people_.at(mother).spouses.back() : mother;
    const std::optional<std::size_t> home = people_.at(lineage).home;
    const bool capped = life_.husbandry.has_value() && home.has_value();
    const std::size_t village = home.value_or(0);
    const std::uint64_t full = capped ? cap(village) : 1;
    const std::uint64_t room =
        capped ? full - std::min<std::uint64_t>(full, population.at(village))
               : 1;
    if (room > 0 && chance(kinship.birth_chance * room / full)) {
      bear_child(mother, army_of, kinship);
      population.at(village) += capped ? 1 : 0;
    }
  }
}

// Whether `mother` may bear a child today: a wife of bearing age, whose
// husband lives, rested since her last child.
bool World::may_bear(std::size_t mother, const Kinship& kinship) const {
  const Person& wife = people_.at(mother);
  const std::int64_t age = age_in_days(wife);
  const bool wed =
      !wife.spouses.empty() && people_.at(wife.spouses.back()).health > 0;
  const bool rested =
      wife.children.empty() ||
      static_cast<std::int64_t>(tick_count_) -
              people_.at(wife.children.back()).born >=
          static_cast<std::int64_t>(std::uint64_t{kinship.birth_spacing} *
                                    rules_.ticks_per_day);
  return wife.sex == Sex::FEMALE && wed && rested &&
         age >= kinship.bearing_age.low && age <= kinship.bearing_age.high;
}

// Whether living person `person` may marry: of marrying age, with a home and
// no living spouse.
bool World::free_to_marry(std::size_t person, const Kinship& kinship) const {
  const Person& single = people_.at(person);
  const Range ages = kinship.marrying_age;
  const std::int64_t age = age_in_days(single);
  return single.health > 0 && single.home.has_value() &&
         (single.spouses.empty() ||
          people_.at(single.spouses.back()).health == 0) &&
         age >= ages.low && age <= ages.high;
}

// Where village `village` lies: its centre, where its residents are bound.
Cell World::seat(std::size_t village) const {
  return armies_.at(villages_.at(village).spec.residents).order.objective;
}

// Marries `person` to a partner drawn by the seed among the `living` free to
// marry, of the other sex and another family, whose village is the same or
// one whose seat lies within the neighbourhood of its own.
void World::marry(std::size_t person, const std::vector<std::size_t>& living,
                  const Kinship& kinship) {
  const Person& self = people_.at(person);
  if (!self.home) {
    return;
  }
  const Cell here = seat(*self.home);
  std::vector<std::size_t> partners;
  for (const std::size_t other : living) {
    const Person& partner = people_.at(other);
    if (partner.sex != self.sex && partner.family != self.family &&
        partner.home && free_to_marry(other, kinship) &&
        std::abs(seat(*partner.home).x - here.x) <= kinship.neighbourhood &&
        std::abs(seat(*partner.home).y - here.y) <= kinship.neighbourhood) {
      partners.push_back(other);
    }
  }
  if (partners.empty()) {
    return;
  }
  const std::size_t chosen = partners.at(next_random() % partners.size());
  people_.at(person).spouses.push_back(chosen);
  people_.at(chosen).spouses.push_back(person);
}

// Bears a child to `mother` and her husband by the one birth rule, into the
// family, home and army of the parent the lineage follows, at its side.
void World::bear_child(std::size_t mother, std::vector<std::size_t>& army_of,
                       const Kinship& kinship) {
  const std::size_t father = people_.at(mother).spouses.back();
  const std::size_t lineage = kinship.fathers_family ? father : mother;
  const Person& parent = people_.at(lineage);
  Person child;
  child.position = parent.position;
  child.health = life_.newborn_health;
  child.home = parent.home;
  child.family = parent.family;
  child.parents = {father, mother};
  bear(child, Soul{});
  const std::size_t born = people_.size();
  people_.at(father).children.push_back(born);
  people_.at(mother).children.push_back(born);
  people_.push_back(std::move(child));
  army_of.push_back(army_of.at(lineage));
  armies_.at(army_of.at(lineage)).roster.push_back(born);
}

// What village `village`'s structures no worse than `worst` hold together,
// and the harvest of its intact fields.
Holding World::held(std::size_t village, Stage worst) const {
  const VillageSpec& spec = villages_.at(village).spec;
  Holding total;
  for (std::size_t index = 0; index < spec.holdings.size(); ++index) {
    const Holding& holding = spec.holdings.at(index);
    const Stage now = stage(spec.structures.at(index));
    if (now <= worst) {
      total.housing += holding.housing;
      total.storage += holding.storage;
    }
    total.harvest += now == Stage::INTACT ? holding.harvest : 0;
  }
  return total;
}

std::size_t World::cap(std::size_t village) const {
  if (!life_.husbandry) {
    return MAX_PEOPLE;
  }
  const Holding standing = held(village, Stage::DAMAGED);
  const std::uint64_t yearly = std::max<std::uint64_t>(
      std::uint64_t{life_.husbandry->consumption} * DAYS_PER_YEAR, 1);
  return static_cast<std::size_t>(
      std::min({std::uint64_t{standing.housing}, standing.harvest / yearly,
                standing.storage / yearly}));
}

std::uint64_t World::stock(std::size_t village) const {
  return villages_.at(village).stock;
}

std::size_t World::residents_of(std::size_t village) const {
  return homes().at(village);
}

std::optional<FieldState> World::field_state(std::size_t structure) const {
  const std::optional<std::size_t>& village = village_of_.at(structure);
  if (!village) {
    return std::nullopt;
  }
  const VillageSpec& spec = villages_.at(*village).spec;
  const auto place = static_cast<std::size_t>(
      std::find(spec.structures.begin(), spec.structures.end(), structure) -
      spec.structures.begin());
  if (place >= spec.holdings.size() || spec.holdings.at(place).harvest == 0) {
    return std::nullopt;
  }
  if (stage(structure) != Stage::INTACT) {
    return FieldState::BURNT;
  }
  return static_cast<FieldState>(date().season);
}

// How many living people make each village their home.
std::vector<std::size_t> World::homes() const {
  std::vector<std::size_t> population(villages_.size(), 0);
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      const Person& person = people_.at(member);
      if (person.health > 0 && person.home) {
        ++population.at(*person.home);
      }
    }
  }
  return population;
}

// Once a day: the harvest on the first day of winter, then each village
// feeds its people from its stock, or none of them, and those unfed too long
// move or die, in order of person.
void World::husband(const Husbandry& husbandry) {
  const std::uint64_t day =
      (tick_count_ / rules_.ticks_per_day) % DAYS_PER_YEAR;
  const bool harvest = day == CALENDAR.days_per_season * 3;
  if (harvest) {
    settle();
  }
  std::vector<std::size_t> population = homes();
  std::vector<bool> fed(villages_.size(), false);
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const Holding holds = held(village, Stage::DAMAGED);
    Village& feeding = villages_.at(village);
    feeding.stock = std::min<std::uint64_t>(
        feeding.stock + (harvest ? holds.harvest : 0), holds.storage);
    const std::uint64_t need =
        std::uint64_t{husbandry.consumption} * population.at(village);
    fed.at(village) = feeding.stock >= need;
    feeding.stock -= fed.at(village) ? need : feeding.stock;
  }
  std::vector<Unfed> unfed;
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      Person& person = people_.at(member);
      if (person.health == 0 || !person.home) {
        continue;
      }
      person.hunger = fed.at(*person.home) ? 0 : person.hunger + 1;
      if (person.hunger >= husbandry.hunger_days) {
        unfed.push_back(Unfed{member, army});
      }
    }
  }
  std::sort(unfed.begin(), unfed.end(),
            [](Unfed left, Unfed right) { return left.person < right.person; });
  for (const Unfed& each : unfed) {
    migrate_or_starve(each, husbandry.migration_reach, population);
  }
}

// Makes each village the home of its residents who make another their home,
// the settlers of a village founded from it, once a harvest finds its cap
// holding all its living residents.
void World::settle() {
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const std::vector<std::size_t>& residents =
        armies_.at(villages_.at(village).spec.residents).roster;
    const auto living = static_cast<std::size_t>(std::count_if(
        residents.begin(), residents.end(),
        [this](std::size_t member) { return people_.at(member).health > 0; }));
    const auto elsewhere = [this, village](std::size_t member) {
      return people_.at(member).home != village;
    };
    if (std::none_of(residents.begin(), residents.end(), elsewhere) ||
        cap(village) < living) {
      continue;
    }
    for (const std::size_t member : residents) {
      people_.at(member).home = village;
    }
  }
}

// Moves person `person`, unfed too long, from army `army` to the residents of
// a village drawn by the seed among those within the migration reach of its
// home with room under their cap and food in store, or ends its life.
void World::migrate_or_starve(Unfed unfed, std::int32_t reach,
                              std::vector<std::size_t>& population) {
  const std::size_t person = unfed.person;
  const std::size_t army = unfed.army;
  Person& hungry = people_.at(person);
  const std::size_t home = hungry.home.value_or(0);
  const Cell from = seat(home);
  std::vector<std::size_t> refuges;
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const Cell there = seat(village);
    if (village != home && std::abs(there.x - from.x) <= reach &&
        std::abs(there.y - from.y) <= reach &&
        population.at(village) < cap(village) &&
        villages_.at(village).stock > 0) {
      refuges.push_back(village);
    }
  }
  if (refuges.empty()) {
    hungry.health = 0;
    return;
  }
  const std::size_t refuge = refuges.at(next_random() % refuges.size());
  std::vector<std::size_t>& from_roster = armies_.at(army).roster;
  from_roster.erase(std::find(from_roster.begin(), from_roster.end(), person));
  armies_.at(villages_.at(refuge).spec.residents).roster.push_back(person);
  hungry.home = refuge;
  hungry.hunger = 0;
  --population.at(home);
  ++population.at(refuge);
}

std::size_t World::army_count() const { return armies_.size(); }

std::size_t World::structure_count() const { return structures_.size(); }

std::size_t World::village_count() const { return villages_.size(); }

std::optional<std::size_t> World::plan_of(std::size_t structure) const {
  return plans_.at(structure);
}

std::vector<std::optional<std::size_t>> World::territory() const {
  const std::int32_t width = map_.width();
  std::vector<std::optional<std::size_t>> held(
      static_cast<std::size_t>(width) *
      static_cast<std::size_t>(map_.height()));
  if (!life_.growth) {
    return held;
  }
  const std::int32_t claim = life_.growth->claim;
  std::vector<std::int64_t> nearest(held.size(),
                                    std::numeric_limits<std::int64_t>::max());
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    if (armies_.at(villages_.at(village).spec.residents).roster.empty()) {
      continue;
    }
    const Cell centre = seat(village);
    for (Cell cell{0, std::max(centre.y - claim, 0)};
         cell.y <= std::min(centre.y + claim, map_.height() - 1); ++cell.y) {
      for (cell.x = std::max(centre.x - claim, 0);
           cell.x <= std::min(centre.x + claim, width - 1); ++cell.x) {
        const std::size_t slot = (static_cast<std::size_t>(cell.y) *
                                  static_cast<std::size_t>(width)) +
                                 static_cast<std::size_t>(cell.x);
        const std::int64_t offset_x = cell.x - centre.x;
        const std::int64_t offset_y = cell.y - centre.y;
        const std::int64_t distance =
            (offset_x * offset_x) + (offset_y * offset_y);
        if (distance < nearest.at(slot)) {
          nearest.at(slot) = distance;
          held.at(slot) = owner(village);
        }
      }
    }
  }
  return held;
}

// Every growth check: each village recalls a raid that is over, then, while
// it presses against its cap, takes the first expansion of its owner's stance
// it can, in order of village; a village founded in the check waits for the
// next.
void World::grow(const Growth& growth) {
  const std::vector<std::size_t> population = homes();
  const std::size_t villages = villages_.size();
  for (std::size_t village = 0; village < villages; ++village) {
    recall(village);
    const std::size_t owned_by = owner(village);
    const std::uint64_t people = population.at(village);
    const bool pressed =
        people > 0 &&
        !armies_.at(villages_.at(village).spec.residents).roster.empty() &&
        people * PER_MILLE >= std::uint64_t{cap(village)} * growth.pressure;
    if (!pressed || owned_by >= growth.stances.size()) {
      continue;
    }
    for (const Expansion expansion : growth.stances.at(owned_by)) {
      if (expand(village, expansion, growth)) {
        break;
      }
    }
  }
}

bool World::expand(std::size_t village, Expansion expansion,
                   const Growth& growth) {
  switch (expansion) {
    case Expansion::BUILD:
      return lay_building(village, growth);
    case Expansion::FOUND:
      return found_village(village, growth);
    case Expansion::RAID:
      return send_raid(village, growth);
  }
  return false;
}

// Ends village `village`'s raid once its party has no living member, its
// target's residents have none, or its target's owner is no longer one the
// party is hostile to: every member whose home's residents can reach it goes
// back to them. A party that became another village's residents is no
// longer the village's.
void World::recall(std::size_t village) {
  Village& raiding = villages_.at(village);
  if (!raiding.party) {
    return;
  }
  const std::size_t party = *raiding.party;
  const auto housed = [party](const Village& other) {
    return other.spec.residents == party;
  };
  if (std::any_of(villages_.begin(), villages_.end(), housed)) {
    raiding.party.reset();
    raiding.raiding.reset();
    return;
  }
  if (!raiding.raiding) {
    return;
  }
  const std::size_t target = *raiding.raiding;
  Army& out = armies_.at(party);
  const bool over =
      out.roster.empty() ||
      armies_.at(villages_.at(target).spec.residents).roster.empty() ||
      !rules_.hostile.at(out.faction).at(owner(target));
  if (!over) {
    return;
  }
  raiding.raiding.reset();
  const std::vector<std::size_t> members = out.roster;
  for (const std::size_t member : members) {
    const std::optional<std::size_t> home = people_.at(member).home;
    if (!home) {
      continue;
    }
    Army& residents = armies_.at(villages_.at(*home).spec.residents);
    if (residents.field.distance(cell_of(people_.at(member).position)) ==
        FlowField::UNREACHABLE) {
      continue;
    }
    out.roster.erase(std::find(out.roster.begin(), out.roster.end(), member));
    residents.roster.push_back(member);
  }
}

// Cells of the map: those a structure covers, those not buildable, and those
// an army's objective or a living person takes.
World::Ground World::ground() const {
  const auto width = static_cast<std::size_t>(map_.width());
  const std::size_t cells = width * static_cast<std::size_t>(map_.height());
  Ground marked{std::vector<bool>(cells, false),
                std::vector<bool>(cells, false),
                std::vector<bool>(cells, false)};
  const auto slot = [width](Cell cell) {
    return (static_cast<std::size_t>(cell.y) * width) +
           static_cast<std::size_t>(cell.x);
  };
  for (const Structure& structure : structures_) {
    for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
      marked.covered.at(slot(cell)) = true;
    }
  }
  for (Cell cell{0, 0}; cell.y < map_.height(); ++cell.y) {
    for (cell.x = 0; cell.x < map_.width(); ++cell.x) {
      marked.rough.at(slot(cell)) = !map_.buildable(cell);
    }
  }
  for (const Army& army : armies_) {
    marked.taken.at(slot(army.order.objective)) = true;
    for (const std::size_t member : army.roster) {
      marked.taken.at(slot(cell_of(people_.at(member).position))) = true;
    }
  }
  return marked;
}

// Lays the plan of the growth's builds that most raises village `village`'s
// scarcest holding, in people, while every structure of its own is intact:
// as rubble, on the free cells within the vicinity of its centre nearest it,
// a cell clear of any structure or unbuildable cell. Returns whether it laid
// one.
bool World::lay_building(std::size_t village, const Growth& growth) {
  const VillageSpec& spec = villages_.at(village).spec;
  const auto unfinished = [this](std::size_t structure) {
    return stage(structure) != Stage::INTACT;
  };
  if (std::any_of(spec.structures.begin(), spec.structures.end(), unfinished)) {
    return false;
  }
  const std::uint64_t yearly = std::max<std::uint64_t>(
      std::uint64_t{life_.husbandry ? life_.husbandry->consumption : 1} *
          DAYS_PER_YEAR,
      1);
  // Housing, food and storage, each in the people it holds.
  const auto in_people = [yearly](const Holding& holding) {
    return std::array<std::uint64_t, 3>{
        holding.housing, holding.harvest / yearly, holding.storage / yearly};
  };
  const std::array<std::uint64_t, 3> holds =
      in_people(held(village, Stage::DAMAGED));
  const auto scarcest = static_cast<std::size_t>(
      std::min_element(holds.begin(), holds.end()) - holds.begin());
  std::optional<std::size_t> chosen;
  std::uint64_t most = 0;
  for (const std::size_t plan : growth.builds) {
    const std::uint64_t gain =
        in_people(growth.plans.at(plan).holding).at(scarcest);
    if (gain > most) {
      most = gain;
      chosen = plan;
    }
  }
  if (!chosen) {
    return false;
  }
  const Footprint footprint = growth.plans.at(*chosen).footprint;
  const Ground marked = ground();
  std::vector<bool> open = marked.covered;
  for (std::size_t cell = 0; cell < open.size(); ++cell) {
    open.at(cell) = open.at(cell) || marked.rough.at(cell);
  }
  const Tally blocked{open, map_.width(), map_.height()};
  const Tally taken{marked.taken, map_.width(), map_.height()};
  const Cell centre = seat(village);
  const std::int32_t vicinity = growth.vicinity;
  std::optional<Cell> best;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (Cell corner{0, centre.y - vicinity};
       corner.y + footprint.height - 1 <= centre.y + vicinity; ++corner.y) {
    for (corner.x = centre.x - vicinity;
         corner.x + footprint.width - 1 <= centre.x + vicinity; ++corner.x) {
      const std::int64_t distance = remoteness(corner, footprint, centre);
      if (distance < nearest && blocked.clear(corner, footprint) &&
          blocked.clear_around(corner, footprint, 1) &&
          taken.clear(corner, footprint)) {
        nearest = distance;
        best = corner;
      }
    }
  }
  if (!best) {
    return false;
  }
  const std::optional<std::vector<std::size_t>> laid =
      lay({Laid{*chosen, *best}}, growth, {}, centre);
  if (!laid) {
    return false;
  }
  attach(village, *laid, growth);
  return true;
}

// Lays the growth's settlement as rubble on free buildable land drawn by the
// seed, its centre within reach of village `village`'s, `spacing` cells clear
// of any structure and held by no other faction, and sends the settlers there
// as the new village's residents; their homes stay until it can hold them.
// The founder heads a new family, a branch bearing its family's surname, and
// so do its children among the settlers. Returns whether it founded one.
bool World::found_village(std::size_t village, const Growth& growth) {
  const std::vector<std::size_t>& residents =
      armies_.at(villages_.at(village).spec.residents).roster;
  if (residents.size() <= growth.settlers || settling(village)) {
    return false;
  }
  std::vector<std::size_t> settlers = households(village);
  settlers.resize(std::min<std::size_t>(settlers.size(), growth.settlers));
  if (settlers.empty()) {
    return false;
  }
  const Footprint area = growth.settlement;
  const Cell middle{area.width / 2, area.height / 2};
  const Cell home = seat(village);
  const std::size_t faction = owner(village);
  const FlowField paths{map_, home};
  const Ground marked = ground();
  std::vector<bool> open = marked.covered;
  for (std::size_t cell = 0; cell < open.size(); ++cell) {
    open.at(cell) =
        open.at(cell) || marked.rough.at(cell) || marked.taken.at(cell);
  }
  const Tally blocked{open, map_.width(), map_.height()};
  const Tally covered{marked.covered, map_.width(), map_.height()};
  const std::vector<std::optional<std::size_t>> held = territory();
  const auto claimed = [this, &held, faction](Cell cell) {
    const std::optional<std::size_t> holder =
        held.at((static_cast<std::size_t>(cell.y) *
                 static_cast<std::size_t>(map_.width())) +
                static_cast<std::size_t>(cell.x));
    return holder && *holder != faction;
  };
  std::vector<Cell> free;
  for (Cell corner{0, home.y - growth.reach - middle.y};
       corner.y <= home.y + growth.reach - middle.y; ++corner.y) {
    for (corner.x = home.x - growth.reach - middle.x;
         corner.x <= home.x + growth.reach - middle.x; ++corner.x) {
      const Cell centre{corner.x + middle.x, corner.y + middle.y};
      if (!blocked.clear(corner, area) ||
          !covered.clear_around(corner, area, growth.spacing) ||
          paths.distance(centre) == FlowField::UNREACHABLE) {
        continue;
      }
      const std::vector<Cell> cells = cells_of(corner, area);
      if (std::none_of(cells.begin(), cells.end(), claimed)) {
        free.push_back(corner);
      }
    }
  }
  if (free.empty()) {
    return false;
  }
  const Cell corner = free.at(next_random() % free.size());
  const Cell centre{corner.x + middle.x, corner.y + middle.y};
  std::vector<Laid> layout;
  layout.reserve(growth.layout.size());
  for (const Placement& placement : growth.layout) {
    layout.push_back(Laid{placement.plan, Cell{corner.x + placement.offset.x,
                                               corner.y + placement.offset.y}});
  }
  const std::optional<std::vector<std::size_t>> laid =
      lay(layout, growth, settlers, centre);
  if (!laid) {
    return false;
  }
  // The founder's family branches: a new family under its surname.
  std::size_t families = surnames_.size();
  for (const Person& person : people_) {
    families = std::max(families, person.family + 1);
  }
  const std::size_t founder = settlers.front();
  if (!surnames_.empty()) {
    surnames_.push_back(surnames_.at(people_.at(founder).family));
  }
  for (const std::size_t settler : settlers) {
    const std::vector<std::size_t>& parents = people_.at(settler).parents;
    if (settler == founder ||
        std::find(parents.begin(), parents.end(), founder) != parents.end()) {
      people_.at(settler).family = families;
    }
  }
  const std::size_t army = form(village, settlers, centre, std::nullopt);
  villages_.push_back(Village{VillageSpec{{}, army, {}}, std::nullopt, 0,
                              std::nullopt, std::nullopt, corner});
  attach(villages_.size() - 1, *laid, growth);
  return true;
}

// Sends the growth's raiders from village `village`'s residents, while no
// party of its is out, against the nearest village of a faction its owner is
// hostile to, with living residents and its centre within reach, the lowest
// among equals. Returns whether it sent them.
bool World::send_raid(std::size_t village, const Growth& growth) {
  const Village& raiding = villages_.at(village);
  const std::vector<std::size_t>& residents =
      armies_.at(raiding.spec.residents).roster;
  if (raiding.raiding || residents.size() <= growth.raiders) {
    return false;
  }
  const Cell home = seat(village);
  const std::size_t faction = owner(village);
  std::optional<std::size_t> target;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (std::size_t other = 0; other < villages_.size(); ++other) {
    const Cell there = seat(other);
    const std::int64_t offset_x = there.x - home.x;
    const std::int64_t offset_y = there.y - home.y;
    const std::int64_t distance = (offset_x * offset_x) + (offset_y * offset_y);
    if (other != village && rules_.hostile.at(faction).at(owner(other)) &&
        !armies_.at(villages_.at(other).spec.residents).roster.empty() &&
        within(home, there, growth.reach) && distance < nearest) {
      nearest = distance;
      target = other;
    }
  }
  if (!target) {
    return false;
  }
  std::vector<std::size_t> raiders = levy(village);
  raiders.resize(std::min<std::size_t>(raiders.size(), growth.raiders));
  const FlowField field{map_, seat(*target)};
  const auto cut_off = [this, &field](std::size_t member) {
    return field.distance(cell_of(people_.at(member).position)) ==
           FlowField::UNREACHABLE;
  };
  if (raiders.empty() || std::any_of(raiders.begin(), raiders.end(), cut_off)) {
    return false;
  }
  const std::size_t party =
      form(village, raiders, seat(*target), raiding.party);
  villages_.at(village).party = party;
  villages_.at(village).raiding = target;
  return true;
}

// Whether people whose home is village `village` serve as another village's
// residents: settlers of a village founded from it.
bool World::settling(std::size_t village) const {
  for (std::size_t other = 0; other < villages_.size(); ++other) {
    const std::vector<std::size_t>& residents =
        armies_.at(villages_.at(other).spec.residents).roster;
    const auto homed_here = [this, village](std::size_t member) {
      return people_.at(member).home == village;
    };
    if (other != village &&
        std::any_of(residents.begin(), residents.end(), homed_here)) {
      return true;
    }
  }
  return false;
}

// The living residents of village `village`, in roster order, who lead no
// faction: those it may send away.
std::vector<std::size_t> World::levy(std::size_t village) const {
  std::vector<std::size_t> levied;
  for (const std::size_t member :
       armies_.at(villages_.at(village).spec.residents).roster) {
    const auto leads = [member](const Leadership& faction) {
      return faction.leader == member;
    };
    if (people_.at(member).health > 0 &&
        std::none_of(leadership_.begin(), leadership_.end(), leads)) {
      levied.push_back(member);
    }
  }
  return levied;
}

// Village `village`'s residents who lead no faction, in the order they would
// settle: a founder, the first of the sex the lineage follows, then its living
// spouse and unmarried children among them, then the others in roster order.
// Empty when there is no founder.
std::vector<std::size_t> World::households(std::size_t village) const {
  const std::vector<std::size_t> free = levy(village);
  const Sex lineage =
      !life_.kinship || life_.kinship->fathers_family ? Sex::MALE : Sex::FEMALE;
  const auto heads = [this, lineage](std::size_t member) {
    return people_.at(member).sex == lineage;
  };
  const auto founder = std::find_if(free.begin(), free.end(), heads);
  if (founder == free.end()) {
    return {};
  }
  const Person& head = people_.at(*founder);
  std::vector<std::size_t> settlers{*founder};
  const auto unwed = [this](std::size_t member) {
    const std::vector<std::size_t>& spouses = people_.at(member).spouses;
    return spouses.empty() || people_.at(spouses.back()).health == 0;
  };
  const auto kin = [&](std::size_t member) {
    const bool spouse = !head.spouses.empty() && head.spouses.back() == member;
    const bool child = std::find(head.children.begin(), head.children.end(),
                                 member) != head.children.end() &&
                       unwed(member);
    return member != *founder && (spouse || child);
  };
  std::copy_if(free.begin(), free.end(), std::back_inserter(settlers), kin);
  for (const std::size_t member : free) {
    if (std::find(settlers.begin(), settlers.end(), member) == settlers.end()) {
      settlers.push_back(member);
    }
  }
  return settlers;
}

// Lays each of `laid` as rubble, blocking the cells of those that block,
// unless that would leave an army's member unable to reach its objective or a
// mover unable to reach `destination`. Every army's flow field follows the new
// map. Returns the new structures, or nothing when it laid none.
std::optional<std::vector<std::size_t>> World::lay(
    const std::vector<Laid>& laid, const Growth& growth,
    const std::vector<std::size_t>& movers, Cell destination) {
  Map built = map_;
  for (const Laid& each : laid) {
    const Plan& plan = growth.plans.at(each.plan);
    for (const Cell cell : cells_of(each.cell, plan.footprint)) {
      if (plan.blocks) {
        built.block(cell);
      }
    }
  }
  const auto reaches = [this](const FlowField& field,
                              const std::vector<std::size_t>& members) {
    return std::all_of(
        members.begin(), members.end(), [this, &field](std::size_t member) {
          return field.distance(cell_of(people_.at(member).position)) !=
                 FlowField::UNREACHABLE;
        });
  };
  std::vector<FlowField> fields;
  fields.reserve(armies_.size());
  for (const Army& army : armies_) {
    fields.emplace_back(built, army.order.objective);
    if (!reaches(fields.back(), army.roster)) {
      return std::nullopt;
    }
  }
  if (!movers.empty() && !reaches(FlowField{built, destination}, movers)) {
    return std::nullopt;
  }
  map_ = std::move(built);
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    armies_.at(army).field = std::move(fields.at(army));
  }
  std::vector<std::size_t> made;
  for (const Laid& each : laid) {
    const Plan& plan = growth.plans.at(each.plan);
    made.push_back(structures_.size());
    structures_.push_back(Structure{each.cell, 0, plan.footprint, plan.blocks});
    village_of_.emplace_back();
    plans_.emplace_back(each.plan);
    structure_extent_ =
        std::max({structure_extent_, plan.footprint.width * CELL_SIZE,
                  plan.footprint.height * CELL_SIZE});
  }
  structure_index_ = SpatialHash{centres_of(structures_)};
  return made;
}

// Makes `structures`, laid from the growth's plans, village `village`'s, each
// holding what its plan holds.
void World::attach(std::size_t village,
                   const std::vector<std::size_t>& structures,
                   const Growth& growth) {
  VillageSpec& spec = villages_.at(village).spec;
  spec.holdings.resize(spec.structures.size());
  for (const std::size_t structure : structures) {
    village_of_.at(structure) = village;
    spec.structures.push_back(structure);
    spec.holdings.push_back(
        growth.plans.at(plans_.at(structure).value_or(0)).holding);
  }
}

// Moves `members` off village `village`'s residents into army `into`, or a
// new army when none is given, of the residents' faction and skills, bound
// for `objective` with all of them. Returns the army.
std::size_t World::form(std::size_t village,
                        const std::vector<std::size_t>& members, Cell objective,
                        std::optional<std::size_t> into) {
  const std::size_t from = villages_.at(village).spec.residents;
  std::vector<std::size_t>& roster = armies_.at(from).roster;
  for (const std::size_t member : members) {
    roster.erase(std::find(roster.begin(), roster.end(), member));
  }
  std::vector<std::size_t> every_skill(armies_.at(from).skills.size());
  std::iota(every_skill.begin(), every_skill.end(), std::size_t{0});
  Army formed{armies_.at(from).faction,
              armies_.at(from).skills,
              Order{objective, std::move(every_skill)},
              FlowField{map_, objective},
              {}};
  if (into) {
    formed.roster = std::move(armies_.at(*into).roster);
  }
  formed.roster.insert(formed.roster.end(), members.begin(), members.end());
  if (into) {
    armies_.at(*into) = std::move(formed);
    return *into;
  }
  armies_.push_back(std::move(formed));
  return armies_.size() - 1;
}

}  // namespace murim
