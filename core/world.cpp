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
#include <string>
#include <tuple>
#include <utility>
#include <variant>
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
static_assert(MAX_BODY_DAMAGE <= UINT64_LIMIT / PEOPLE_CEILING,
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

// The sub-cell unit of the structure's footprint nearest `point`.
Position nearest_point(const Structure& structure, Position point) {
  const auto clamp = [](std::int32_t along, std::int32_t first,
                        std::int32_t cells) {
    const std::int64_t low = std::int64_t{first} * CELL_SIZE;
    const std::int64_t high = ((std::int64_t{first} + cells) * CELL_SIZE) - 1;
    return static_cast<std::int32_t>(
        std::clamp<std::int64_t>(along, low, high));
  };
  return Position{clamp(point.x, structure.cell.x, structure.footprint.width),
                  clamp(point.y, structure.cell.y, structure.footprint.height)};
}

// Whether the disc of `radius` around `centre` reaches a sub-cell unit of the
// structure's footprint: the footprint's nearest point lies within it.
bool reaches(const Structure& structure, Position centre, std::int32_t radius) {
  return distance_squared(centre, nearest_point(structure, centre)) <=
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

// `point` brought within `range` of `from`, along the line between them.
Position within_range(Position from, Position point, std::int32_t range) {
  std::int64_t offset_x = std::int64_t{point.x} - from.x;
  std::int64_t offset_y = std::int64_t{point.y} - from.y;
  const std::int64_t reach = range;
  if (std::abs(offset_x) <= reach && std::abs(offset_y) <= reach &&
      (offset_x * offset_x) + (offset_y * offset_y) <= reach * reach) {
    return point;
  }
  // Halving keeps the direction and the squares below within 64 bits.
  while (std::abs(offset_x) > MAX_REACH || std::abs(offset_y) > MAX_REACH) {
    offset_x /= 2;
    offset_y /= 2;
  }
  const std::int64_t squared = (offset_x * offset_x) + (offset_y * offset_y);
  // Rounded up, so the point brought in lies within range.
  std::int64_t length = root(squared);
  length += static_cast<std::int64_t>(length * length < squared);
  return Position{
      static_cast<std::int32_t>(from.x + (offset_x * reach / length)),
      static_cast<std::int32_t>(from.y + (offset_y * reach / length))};
}

// The nearest of the people offered from `from`, those ahead of `facing`,
// within a quarter turn either side, before those behind, and the first
// offered among equals.
class Nearest {
 public:
  Nearest(Position from, Step facing) : from_(from), facing_(facing) {}

  // Offers the person at `at`, the `order`th in the order people are offered.
  void offer(std::size_t order, Position where) {
    const std::int64_t offset_x = std::int64_t{where.x} - from_.x;
    const std::int64_t offset_y = std::int64_t{where.y} - from_.y;
    const Key key{(offset_x * facing_.x) + (offset_y * facing_.y) < 0,
                  (offset_x * offset_x) + (offset_y * offset_y), order};
    if (!best_ || key < *best_) {
      best_ = key;
    }
  }

  // The order of the nearest offered, if anyone was.
  [[nodiscard]] std::optional<std::size_t> found() const {
    return best_ ? std::optional<std::size_t>{std::get<2>(*best_)}
                 : std::nullopt;
  }

 private:
  // Behind the facing, the distance squared, and the order offered.
  using Key = std::tuple<bool, std::int64_t, std::size_t>;

  Position from_;
  Step facing_;
  std::optional<Key> best_;
};

// The person a cast at `target` is aimed at, if it is aimed at one.
std::optional<std::size_t> person_aimed_at(const Target& target) {
  const auto* aimed = std::get_if<AtPerson>(&target);
  return aimed == nullptr ? std::nullopt
                          : std::optional<std::size_t>{aimed->person};
}

// The kind the crowd's index marks a person of faction `faction` by: one bit
// per faction, factions past the mask's width sharing bits, so a search for
// a faction's kind may visit people of another but never misses its own.
std::uint64_t kind_of(std::size_t faction) {
  constexpr std::size_t KIND_BITS = 64;
  return std::uint64_t{1} << (faction % KIND_BITS);
}

// The degrees in a quarter turn.
constexpr std::int32_t QUARTER_TURN_DEGREES = 90;

// The ground one use of a skill of any shape but TARGETED covers, from its
// user at `from` toward `toward`: a disc around `toward`, which a SELF skill
// is always used toward its user's own position for; an arc of the range's
// reach ahead of the way to `toward`; or the points within the radius of a
// line the range long toward it.
class Swath {
 public:
  Swath(const Skill& skill, Position from, Position toward)
      : shape_(skill.shape),
        from_(from),
        range_(skill.range),
        radius_(skill.radius),
        centre_(toward),
        facing_{toward.x - from.x, toward.y - from.y} {
    if (shape_ == Shape::LINE) {
      const std::int64_t length = root(distance_squared(from, toward));
      end_ =
          length == 0
              ? from
              : Position{
                    static_cast<std::int32_t>(
                        from.x + (facing_.x * std::int64_t{range_} / length)),
                    static_cast<std::int32_t>(
                        from.y + (facing_.y * std::int64_t{range_} / length))};
      length_ = root(distance_squared(from, end_));
    }
  }

  // The point its shape reaches as far as: a line's far end, and for any
  // other shape the point it was used toward.
  [[nodiscard]] Position end() const {
    return shape_ == Shape::LINE ? end_ : centre_;
  }

  // The centre and reach of a query that finds every point it covers.
  [[nodiscard]] Position centre() const {
    return shape_ == Shape::ARC || shape_ == Shape::LINE ? from_ : centre_;
  }
  [[nodiscard]] std::int32_t extent() const {
    switch (shape_) {
      case Shape::ARC:
        return range_;
      case Shape::LINE:
        return range_ + radius_;
      case Shape::AREA:
      case Shape::SELF:
      case Shape::TARGETED:
        break;
    }
    return radius_;
  }

  [[nodiscard]] bool covers(Position point) const {
    const std::int64_t offset_x = std::int64_t{point.x} - from_.x;
    const std::int64_t offset_y = std::int64_t{point.y} - from_.y;
    switch (shape_) {
      case Shape::ARC:
        // A point ahead makes no obtuse angle with the facing: a quarter turn
        // either side.
        static_assert(ARC_HALF_WIDTH_DEGREES == QUARTER_TURN_DEGREES,
                      "an arc covers the half-disc ahead of its facing");
        return distance_squared(from_, point) <=
                   std::int64_t{range_} * range_ &&
               (offset_x * facing_.x) + (offset_y * facing_.y) >= 0;
      case Shape::LINE: {
        const std::int64_t along_x = std::int64_t{end_.x} - from_.x;
        const std::int64_t along_y = std::int64_t{end_.y} - from_.y;
        const std::int64_t along = (offset_x * along_x) + (offset_y * along_y);
        const std::int64_t radius = radius_;
        if (along <= 0) {
          return distance_squared(from_, point) <= radius * radius;
        }
        if (along >= length_ * length_) {
          return distance_squared(end_, point) <= radius * radius;
        }
        return std::abs((offset_x * along_y) - (offset_y * along_x)) <=
               radius * length_;
      }
      case Shape::AREA:
      case Shape::SELF:
      case Shape::TARGETED:
        break;
    }
    return distance_squared(centre_, point) <= std::int64_t{radius_} * radius_;
  }

  // Whether it covers the point of the structure's footprint nearest the
  // point of its own nearest the footprint's centre.
  [[nodiscard]] bool reaches(const Structure& structure) const {
    if (shape_ != Shape::LINE) {
      const Position near = shape_ == Shape::ARC ? from_ : centre_;
      return covers(nearest_point(structure, near));
    }
    const std::int64_t middle_x =
        (std::int64_t{structure.cell.x} * CELL_SIZE) +
        (std::int64_t{structure.footprint.width} * CELL_SIZE / 2);
    const std::int64_t middle_y =
        (std::int64_t{structure.cell.y} * CELL_SIZE) +
        (std::int64_t{structure.footprint.height} * CELL_SIZE / 2);
    const std::int64_t along_x = std::int64_t{end_.x} - from_.x;
    const std::int64_t along_y = std::int64_t{end_.y} - from_.y;
    // How far along the line the footprint's centre lies, within its length.
    const std::int64_t along =
        length_ == 0
            ? 0
            : std::clamp<std::int64_t>((((middle_x - from_.x) * along_x) +
                                        ((middle_y - from_.y) * along_y)) /
                                           length_,
                                       0, length_);
    const Position near =
        length_ == 0 ? from_
                     : Position{static_cast<std::int32_t>(
                                    from_.x + (along_x * along / length_)),
                                static_cast<std::int32_t>(
                                    from_.y + (along_y * along / length_))};
    return covers(nearest_point(structure, near));
  }

 private:
  Shape shape_;
  Position from_;
  std::int32_t range_;
  std::int32_t radius_;
  Position centre_;
  Step facing_;
  Position end_{};
  std::int64_t length_ = 0;
};

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
// cells the flow field reaches only; a diagonal step at DIAGONAL_SPEED on
// each axis.
Position stepped(const FlowField& field, Position from, Step direction) {
  const auto reached = [&field](Position position) {
    return field.distance(cell_of(position)) != FlowField::UNREACHABLE;
  };
  const std::int32_t speed =
      direction.x != 0 && direction.y != 0 ? DIAGONAL_SPEED : SPEED;
  const Position along_x{from.x + (direction.x * speed), from.y};
  const Position moved_x = reached(along_x) ? along_x : from;
  const Position along_y{moved_x.x, moved_x.y + (direction.y * speed)};
  return reached(along_y) ? along_y : moved_x;
}

// Folds who faction `faction`'s sects admit and how it elects its leader.
template <typename Fold>
void fold_selection(const Fold& fold, const Leadership& faction) {
  const Admission admission = faction.admission.value_or(Admission{});
  fold(faction.admission.has_value());
  fold(admission.age);
  fold(admission.talent);
  fold(admission.attainment);
  fold(admission.reach);
  fold(admission.chance);
  fold(admission.elder_share);
  fold(faction.election.has_value());
  fold(faction.election.value_or(Election{}).term);
}

// Folds order `order` into `fold`; its attack intent's target only when it
// holds one, so a world with no intent keeps its hash.
template <typename Fold>
void fold_order(const Fold& fold, const Order& order) {
  fold(order.objective.x);
  fold(order.objective.y);
  if (order.target) {
    fold(order.target.has_value());
    fold(*order.target);
  }
  for (const std::size_t skill : order.skills) {
    fold(skill);
  }
}

// Folds skill `skill` into `fold`; its qi cost only when it has one, and its
// shape only when not an area, so a world of free area skills hashes as
// before.
template <typename Fold>
void fold_skill(const Fold& fold, const Skill& skill) {
  fold(skill.range);
  fold(skill.radius);
  fold(skill.body_damage);
  fold(skill.structure_damage);
  fold(skill.cooldown);
  if (skill.qi_cost != 0) {
    fold(skill.qi_cost);
  }
  if (skill.shape != Shape::AREA) {
    fold(static_cast<std::uint8_t>(skill.shape));
  }
}

// Folds manual `manual`'s art and place.
template <typename Fold>
void fold_manual(const Fold& fold, const Manual& manual) {
  fold(manual.art);
  fold(manual.place.index());
  if (const auto* held = std::get_if<HeldBySect>(&manual.place)) {
    fold(held->sect);
  }
  if (const auto* cell = std::get_if<Cell>(&manual.place)) {
    fold(cell->x);
    fold(cell->y);
  }
}

// The indices an event's happening names, in their order.
std::vector<std::size_t> named(const Founded& what) {
  return {what.village, what.faction};
}
std::vector<std::size_t> named(const Raided& what) {
  return {what.village, what.owner, what.raider};
}
std::vector<std::size_t> named(const Ruined& what) {
  return {what.village, what.owner, what.destroyer.army,
          what.destroyer.faction};
}
std::vector<std::size_t> named(const SectFell& what) {
  return {what.sect, what.faction};
}
std::vector<std::size_t> named(const LeaderChanged& what) {
  return {what.faction, what.leader, static_cast<std::size_t>(what.by)};
}
std::vector<std::size_t> named(const Born& what) { return {what.person}; }
std::vector<std::size_t> named(const Died& what) { return {what.person}; }

// Folds event `event` into `fold`.
template <typename Fold>
void fold_event(const Fold& fold, const Event& event) {
  fold(event.sequence);
  fold(event.tick);
  fold(event.cell.x);
  fold(event.cell.y);
  fold(event.what.index());
  std::visit(
      [&fold](const auto& what) {
        for (const std::size_t index : named(what)) {
          fold(index);
        }
      },
      event.what);
}

// Folds step `step` of progress into `fold`.
// Folds the rules' reaches and capacity; the talk reach and the sight only
// when set, and the capacity only when other than MAX_PEOPLE, so a world where
// no one talks or closes on a foe, at the default capacity, keeps its hash.
template <typename Fold>
void fold_reaches(const Fold& fold, const Rules& rules) {
  fold(rules.rumour_reach);
  if (rules.talk_reach != 0) {
    fold(rules.talk_reach);
  }
  if (rules.sight != 0) {
    fold(rules.sight);
  }
  if (rules.people_capacity != MAX_PEOPLE) {
    fold(rules.people_capacity);
  }
}

template <typename Fold>
void fold_progress(const Fold& fold, const Progress& step) {
  fold(step.sequence);
  fold(step.tick);
  fold(step.what.index());
  if (const auto* learned = std::get_if<Learned>(&step.what)) {
    fold(learned->person);
    fold(learned->art);
    fold(learned->skill);
    fold(static_cast<std::size_t>(learned->by));
  } else {
    fold(std::get<RealmGained>(step.what).person);
    fold(std::get<RealmGained>(step.what).rank);
  }
}

// Folds into `fold` each declaration standing, each army a driver steers
// among `armies`, and the diplomacy, each only while held, so a world with
// none keeps its hash.
template <typename Fold, typename Armies>
void fold_relations(const Fold& fold,
                    const std::vector<std::optional<Declared>>& declarations,
                    const Armies& armies,
                    const std::optional<Diplomacy>& diplomacy) {
  for (std::size_t index = 0; index < declarations.size(); ++index) {
    if (const std::optional<Declared>& standing = declarations.at(index)) {
      fold(index);
      fold(standing->what);
      fold(standing->by);
      fold(standing->tick);
    }
  }
  for (std::size_t army = 0; army < armies.size(); ++army) {
    if (armies.at(army).steered) {
      fold(army);
    }
  }
  if (diplomacy) {
    fold(diplomacy->war_margin);
    fold(diplomacy->war_chance);
    fold(diplomacy->peace_chance);
    fold(diplomacy->hold_days);
  }
}

// Folds everything `person` holds into `fold`; `absent` stands for no soul,
// and its qi is folded where the world's people `cultivates`.
template <typename Fold>
void fold_person(const Fold& fold, const Person& person, std::size_t absent,
                 bool cultivates) {
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
  fold(person.sect.value_or(absent));
  fold(person.sex);
  fold(person.given);
  for (const std::size_t spouse : person.spouses) {
    fold(spouse);
  }
  for (const std::size_t child : person.children) {
    fold(child);
  }
  fold(person.hunger);
  // Folded only when held, so a world where no one cultivates or knows an
  // art hashes as before.
  if (cultivates) {
    fold(person.cultivation.growth);
    fold(person.cultivation.limit);
    fold(person.qi);
    fold(person.qi_capacity);
  }
  // Folded only when held, so a world without trade hashes as before.
  if (person.silver != 0 || !person.held.empty()) {
    fold(person.silver);
    for (const std::uint64_t held : person.held) {
      fold(held);
    }
  }
  if (!person.known.empty()) {
    fold(person.known.size());
    for (const Known& known : person.known) {
      fold(known.art);
      fold(known.skill);
      fold(known.ready_at);
    }
  }
  // Folded only when given, so a world no one casts in hashes as before.
  if (person.casting) {
    fold(person.casting->skill);
    const Target& target = person.casting->target;
    fold(target.index());
    if (const auto* point = std::get_if<Point>(&target)) {
      fold(point->position.x);
      fold(point->position.y);
    }
    if (const auto* aimed = std::get_if<AtPerson>(&target)) {
      fold(aimed->person);
    }
  }
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

// Whether a skill lies outside the limits a world accepts.
bool invalid_skill(const Skill& skill) {
  return !reachable(skill.range) || !reachable(skill.radius) ||
         skill.body_damage > MAX_BODY_DAMAGE || skill.cooldown > MAX_COOLDOWN ||
         skill.qi_cost > MAX_TALENT;
}

// Throws std::invalid_argument when `life`'s cultivation, realms, arts or
// learning lie outside the limits the constructor documents.
void check_arts(const Life& life) {
  const auto excessive = [](Spread values) {
    return std::uint64_t{values.mean} + values.spread > MAX_TALENT;
  };
  const bool bad_cultivation =
      life.cultivation && (excessive(life.cultivation->growth) ||
                           excessive(life.cultivation->limit) ||
                           life.cultivation->recovery > PER_MILLE ||
                           life.cultivation->recovery_ticks == 0);
  bool unordered = false;
  for (std::size_t realm = 0; realm < life.realms.size(); ++realm) {
    unordered =
        unordered || life.realms.at(realm).might > MAX_MIGHT ||
        (realm > 0 && life.realms.at(realm).qi < life.realms.at(realm - 1).qi);
  }
  const auto stray_art = [&life](const Art& art) {
    return art.discipline >= life.disciplines.size() ||
           std::any_of(art.skills.begin(), art.skills.end(),
                       [&life](const ArtSkill& taught) {
                         return invalid_skill(taught.skill) ||
                                taught.rank > life.realms.size();
                       });
  };
  const Learning& learning = life.learning;
  const bool bad_learning = learning.comprehension > PER_MILLE ||
                            learning.teaching > PER_MILLE ||
                            learning.reading_cost > MAX_TALENT ||
                            !reachable(learning.reading_reach) ||
                            learning.orientation_ease > PER_MILLE ||
                            learning.polarity_ease > PER_MILLE ||
                            learning.orientation_potency > PER_MILLE ||
                            learning.polarity_potency > PER_MILLE;
  if (bad_cultivation || unordered || bad_learning ||
      std::any_of(life.arts.begin(), life.arts.end(), stray_art)) {
    throw std::invalid_argument(
        "cultivation must draw within MAX_TALENT and recover within PER_MILLE "
        "on a tick or more, realms ascend with a might within MAX_MIGHT, arts "
        "name known disciplines and hold valid skills of known ranks, and "
        "learning's chances, eases and potencies lie within PER_MILLE");
  }
}

// Throws std::invalid_argument when `trade` lies outside the limits the
// constructor documents, given `disciplines` drawn.
void check_trade(const Trade& trade, std::size_t disciplines) {
  const auto stray_ware = [disciplines](const Ware& ware) {
    return ware.price == 0 || ware.price > MAX_TALENT ||
           ware.health > MAX_HEALTH || ware.qi > MAX_TALENT ||
           ware.discipline >= disciplines;
  };
  if (std::any_of(trade.wares.begin(), trade.wares.end(), stray_ware) ||
      trade.purse > MAX_TALENT || trade.wage > MAX_TALENT ||
      trade.wounded > PER_MILLE || trade.drained > PER_MILLE) {
    throw std::invalid_argument(
        "a ware must cost 1 to MAX_TALENT silver, restore health within "
        "MAX_HEALTH and qi within MAX_TALENT, and name a discipline drawn; a "
        "purse and wage lie within MAX_TALENT and the shares within "
        "PER_MILLE");
  }
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
  check_arts(life);
  if (life.diplomacy && (life.diplomacy->war_chance > PER_MILLE ||
                         life.diplomacy->peace_chance > PER_MILLE)) {
    throw std::invalid_argument(
        "diplomacy's chances must lie within a thousand per mille");
  }
  if (life.trade) {
    check_trade(*life.trade, life.disciplines.size());
  }
}

// The range a spread draws over.
Range range_of(Spread values) {
  return Range{values.mean - std::min(values.mean, values.spread),
               values.mean + values.spread};
}

// Whether `what` names a person past `people`, an art or skill `life` lacks,
// or a realm outside its realms.
bool stray_advance(const Advance& what, std::size_t people, const Life& life) {
  if (const auto* learned = std::get_if<Learned>(&what)) {
    return learned->person >= people || learned->art >= life.arts.size() ||
           learned->skill >= life.arts.at(learned->art).skills.size();
  }
  const auto& gained = std::get<RealmGained>(what);
  return gained.person >= people || gained.rank == 0 ||
         gained.rank > life.realms.size();
}

// Whether `person` holds more qi than its store, a store past MAX_TALENT, or
// a known skill of no art `life` holds.
bool stray_cultivation(const Person& person, const Life& life) {
  const auto unknown = [&life](const Known& known) {
    return known.art >= life.arts.size() ||
           known.skill >= life.arts.at(known.art).skills.size();
  };
  return person.qi > person.qi_capacity || person.qi_capacity > MAX_TALENT ||
         std::any_of(person.known.begin(), person.known.end(), unknown);
}

// Whether `person` holds a count for other than each of `life`'s wares.
bool stray_purse(const Person& person, const Life& life) {
  return person.held.size() != (life.trade ? life.trade->wares.size() : 0);
}

// The discipline of `person`'s highest talent limit, the first among equals;
// 0 when it draws no talent.
std::size_t best_discipline(const Person& person) {
  return static_cast<std::size_t>(
      std::max_element(person.talent.begin(), person.talent.end(),
                       [](const Aptitude& left, const Aptitude& right) {
                         return left.limit < right.limit;
                       }) -
      person.talent.begin());
}

// Folds the trade's data into `fold`, only when given, so a world without
// one keeps its hash.
template <typename Fold>
void fold_trade(const Fold& fold, const std::optional<Trade>& trade) {
  if (!trade) {
    return;
  }
  for (const Ware& ware : trade->wares) {
    fold(ware.price);
    fold(ware.health);
    fold(ware.qi);
    fold(ware.discipline);
  }
  fold(trade->purse);
  fold(trade->wage);
  fold(trade->wounded);
  fold(trade->drained);
}

// Whether every one of `indices` names one of `count` records.
bool all_below(const std::vector<std::size_t>& indices, std::size_t count) {
  return std::all_of(indices.begin(), indices.end(),
                     [count](std::size_t index) { return index < count; });
}

// Whether `index`, if there is one, names one of `count` records.
bool below(std::optional<std::size_t> index, std::size_t count) {
  return !index || *index < count;
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
    fold(holding.arms);
    fold(holding.tends);
    fold(holding.edge);
    fold(holding.healing);
  }
}

}  // namespace

World::World(Seed seed, Map map, std::vector<ArmySpec> armies,
             std::vector<Structure> structures, Rules rules,
             std::vector<VillageSpec> villages,
             std::vector<Leadership> leadership, Life life,
             std::vector<SectSpec> sects)
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
  if (!reachable(rules_.separation) || !reachable(rules_.rumour_reach) ||
      !reachable(rules_.talk_reach) || !reachable(rules_.sight) ||
      rules_.people_capacity > PEOPLE_CEILING ||
      recruits > rules_.people_capacity ||
      std::any_of(rules_.hostile.begin(), rules_.hostile.end(), ragged) ||
      !placed || std::any_of(armies.begin(), armies.end(), invalid_army)) {
    throw std::invalid_argument(
        "separation, rumour and talk reach, sight, range and radius must lie "
        "in "
        "[0, MAX_REACH], hostility "
        "must be square, the world must hold at most its rules' capacity of "
        "people, itself at most PEOPLE_CEILING, each "
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
  const auto homeless = [&villages](const ArmySpec& army) {
    return army.home && *army.home >= villages.size();
  };
  if (std::any_of(armies.begin(), armies.end(), homeless)) {
    throw std::invalid_argument("an army's home must be a village");
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
  declarations_.resize(rules_.hostile.size() * rules_.hostile.size());
  add_sects(std::move(sects));
}

World::World(Map map, Rules rules, Life life)
    : rng_state_(0),
      map_(std::move(map)),
      rules_(std::move(rules)),
      structure_index_(std::vector<Position>{}),
      life_(std::move(life)) {}

void World::restore() {
  check_life(life_);
  const std::size_t factions = rules_.hostile.size();
  const std::size_t people = people_.size();
  const std::size_t villages = villages_.size();
  const std::size_t structures = structures_.size();
  const std::size_t disciplines = life_.disciplines.size();
  const auto ragged = [factions](const std::vector<bool>& row) {
    return row.size() != factions;
  };
  const bool bad_rules =
      rules_.ticks_per_day == 0 || !reachable(rules_.separation) ||
      !reachable(rules_.rumour_reach) || !reachable(rules_.talk_reach) ||
      !reachable(rules_.sight) || rules_.people_capacity > PEOPLE_CEILING ||
      std::any_of(rules_.hostile.begin(), rules_.hostile.end(), ragged);
  const auto off_map = [this](const Structure& structure) {
    return !spans(structure.footprint) || structure.cell.x < 0 ||
           structure.cell.y < 0 ||
           structure.cell.x > map_.width() - structure.footprint.width ||
           structure.cell.y > map_.height() - structure.footprint.height;
  };
  // A cast names a person the world holds, if it names one.
  const auto aimed = [people](const Cast& cast) {
    const auto* aimed = std::get_if<AtPerson>(&cast.target);
    return aimed == nullptr || aimed->person < people;
  };
  const auto stray_person = [&](const Person& person) {
    const std::vector<NameEntry>& given =
        person.sex == Sex::FEMALE ? life_.names.female : life_.names.male;
    return !below(person.home, villages) ||
           !all_below(person.parents, people) ||
           !all_below(person.spouses, people) ||
           !all_below(person.children, people) ||
           !below(person.soul, souls_.size()) ||
           !below(person.sect, sects_.size()) || person.health > MAX_HEALTH ||
           (person.casting &&
            (person.health == 0 || !aimed(*person.casting))) ||
           person.talent.size() != disciplines ||
           person.attainment.size() != disciplines ||
           (!given.empty() && person.given >= given.size()) ||
           stray_cultivation(person, life_) || stray_purse(person, life_);
  };
  const auto stray_village = [&](const Village& village) {
    const VillageSpec& spec = village.spec;
    return !all_below(spec.structures, structures) ||
           spec.residents >= armies_.size() ||
           (!spec.holdings.empty() &&
            spec.holdings.size() != spec.structures.size()) ||
           (village.destroyer && (village.destroyer->army >= armies_.size() ||
                                  village.destroyer->faction >= factions)) ||
           !below(village.party, armies_.size()) ||
           !below(village.raiding, villages);
  };
  const std::size_t plans = life_.growth ? life_.growth->plans.size() : 0;
  const auto unplanned = [plans](std::optional<std::size_t> plan) {
    return !below(plan, plans);
  };
  const auto unvillaged = [villages](std::optional<std::size_t> village) {
    return !below(village, villages);
  };
  const bool bad_structures =
      std::any_of(structures_.begin(), structures_.end(), off_map) ||
      village_of_.size() != structures || plans_.size() != structures ||
      std::any_of(village_of_.begin(), village_of_.end(), unvillaged) ||
      std::any_of(plans_.begin(), plans_.end(), unplanned);
  const auto bad_faction = [&](const Leadership& faction) {
    const Admission admission = faction.admission.value_or(Admission{});
    return !below(faction.leader, people) || admission.chance > PER_MILLE ||
           !reachable(admission.reach) || admission.elder_share > PER_MILLE;
  };
  const auto stray_sect = [&](const Sect& sect) {
    return sect.faction >= factions || sect.seat >= villages;
  };
  const auto stray_manual = [&](const Manual& manual) {
    const auto* held = std::get_if<HeldBySect>(&manual.place);
    return manual.art >= life_.arts.size() ||
           (held != nullptr && held->sect >= sects_.size());
  };
  const auto stray_duel = [&](const Fight& fight) {
    return fight.faction >= factions || fight.challenger >= people ||
           fight.leader >= people;
  };
  const auto stray_soul = [disciplines](const Soul& soul) {
    return soul.practised.size() > disciplines;
  };
  const Manuals manuals = life_.manuals.value_or(Manuals{});
  const auto unknown = [this](const std::vector<std::size_t>& held) {
    return !all_below(held, life_.arts.size());
  };
  const bool bad_life =
      (life_.growth && life_.growth->stances.size() > factions) ||
      manuals.held.size() > sects_.size() ||
      std::any_of(manuals.held.begin(), manuals.held.end(), unknown) ||
      manuals.scatter_chance > PER_MILLE || !reachable(manuals.scatter_reach) ||
      !all_below(surnames_, life_.names.surnames.size());
  if (bad_rules || bad_life || bad_structures || !declarations_kept() ||
      !events_kept() || !progress_kept() || !rostered() ||
      living() > rules_.people_capacity ||
      std::any_of(people_.begin(), people_.end(), stray_person) ||
      std::any_of(villages_.begin(), villages_.end(), stray_village) ||
      leadership_.size() != factions ||
      std::any_of(leadership_.begin(), leadership_.end(), bad_faction) ||
      std::any_of(sects_.begin(), sects_.end(), stray_sect) ||
      std::any_of(manuals_.begin(), manuals_.end(), stray_manual) ||
      std::any_of(duels_.begin(), duels_.end(), stray_duel) ||
      std::any_of(souls_.begin(), souls_.end(), stray_soul)) {
    throw std::invalid_argument(
        "a restored world must keep the limits a world is created within, "
        "every reference must name a record it holds, and every living "
        "person must stand on exactly one roster");
  }
  structure_extent_ = 0;
  for (const Structure& structure : structures_) {
    structure_extent_ =
        std::max({structure_extent_, structure.footprint.width * CELL_SIZE,
                  structure.footprint.height * CELL_SIZE});
  }
  structure_index_ = SpatialHash{centres_of(structures_)};
}

// Whether the declarations are as declare() keeps them: one place for each
// two factions, each standing under the lower's row, made by one of them no
// later than now.
bool World::declarations_kept() const {
  const std::size_t factions = rules_.hostile.size();
  if (declarations_.size() != factions * factions) {
    return false;
  }
  for (std::size_t index = 0; index < declarations_.size(); ++index) {
    const std::optional<Declared>& standing = declarations_.at(index);
    const std::size_t one = index / factions;
    const std::size_t other = index % factions;
    if (standing &&
        (one >= other || (standing->by != one && standing->by != other) ||
         standing->tick > tick_count_)) {
      return false;
    }
  }
  return true;
}

// Whether the events kept are as tick() keeps them: at most
// EVENT_CAPACITY, oldest first, numbered one after another, none later than
// the world's tick, each naming records the world holds.
bool World::events_kept() const {
  const std::size_t factions = rules_.hostile.size();
  const std::size_t people = people_.size();
  const std::size_t villages = villages_.size();
  const auto stray_happening = [&](const Happening& what) {
    if (const auto* founded = std::get_if<Founded>(&what)) {
      return founded->village >= villages || founded->faction >= factions;
    }
    if (const auto* raided = std::get_if<Raided>(&what)) {
      return raided->village >= villages || raided->owner >= factions ||
             raided->raider >= factions;
    }
    if (const auto* ruined = std::get_if<Ruined>(&what)) {
      return ruined->village >= villages || ruined->owner >= factions ||
             ruined->destroyer.army >= armies_.size() ||
             ruined->destroyer.faction >= factions;
    }
    if (const auto* fell = std::get_if<SectFell>(&what)) {
      return fell->sect >= sects_.size() || fell->faction >= factions;
    }
    if (const auto* changed = std::get_if<LeaderChanged>(&what)) {
      return changed->faction >= factions || changed->leader >= people;
    }
    if (const auto* born = std::get_if<Born>(&what)) {
      return born->person >= people;
    }
    const auto* died = std::get_if<Died>(&what);
    return died == nullptr || died->person >= people;
  };
  bool bad_events = events_.size() > EVENT_CAPACITY;
  for (std::size_t index = 0; index < events_.size(); ++index) {
    const Event& event = events_.at(index);
    const bool unordered =
        index > 0 && (event.sequence != events_.at(index - 1).sequence + 1 ||
                      event.tick < events_.at(index - 1).tick);
    bad_events = bad_events || unordered || event.tick > tick_count_ ||
                 stray_happening(event.what);
  }
  return !bad_events;
}

// Whether every army's faction, skills, order and roster name what the world
// holds, and every living person stands on exactly one roster and no dead
// person on any.
bool World::rostered() const {
  const std::size_t people = people_.size();
  std::vector<std::size_t> rosters(people, 0);
  for (const Army& army : armies_) {
    if (army.faction >= rules_.hostile.size() || army.skills.empty() ||
        std::any_of(army.skills.begin(), army.skills.end(), invalid_skill) ||
        army.order.skills.empty() ||
        !all_below(army.order.skills, army.skills.size()) ||
        !all_below(army.roster, people) ||
        (army.order.target && *army.order.target >= people)) {
      return false;
    }
    for (const std::size_t member : army.roster) {
      rosters.at(member) += 1;
    }
  }
  for (std::size_t person = 0; person < people; ++person) {
    if (rosters.at(person) != (people_.at(person).health > 0 ? 1 : 0)) {
      return false;
    }
  }
  return true;
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
    person.home = army.home;
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

// Adds the sects `sects` describe, each founder joining its sect with its
// discipline of highest talent limit at its starting attainment, up to that
// limit, and gives each sect the manuals the life says it holds. Throws
// std::invalid_argument when an admission or a sect or the manuals are
// outside the limits the constructor documents.
void World::add_sects(std::vector<SectSpec> sects) {
  const auto bad_admission = [](const Leadership& faction) {
    const Admission admission = faction.admission.value_or(Admission{});
    return admission.chance > PER_MILLE || !reachable(admission.reach) ||
           admission.elder_share > PER_MILLE;
  };
  const auto stray = [this](const SectSpec& sect) {
    return sect.faction >= rules_.hostile.size() ||
           sect.seat >= villages_.size() ||
           std::any_of(sect.founders.begin(), sect.founders.end(),
                       [this, &sect](const Founder& founder) {
                         return founder.person >= people_.size() ||
                                faction_of(founder.person) != sect.faction;
                       });
  };
  const Manuals manuals = life_.manuals.value_or(Manuals{});
  const auto unknown = [this](const std::vector<std::size_t>& held) {
    return !all_below(held, life_.arts.size());
  };
  if (std::any_of(leadership_.begin(), leadership_.end(), bad_admission) ||
      std::any_of(sects.begin(), sects.end(), stray) ||
      manuals.held.size() > sects.size() ||
      std::any_of(manuals.held.begin(), manuals.held.end(), unknown) ||
      manuals.scatter_chance > PER_MILLE || !reachable(manuals.scatter_reach)) {
    throw std::invalid_argument(
        "an admission must have a chance and an elder share within PER_MILLE "
        "and a reach within MAX_REACH; a sect must name a known faction, an "
        "existing village and "
        "founders serving in its faction; and the manuals must name known "
        "arts for known sects, a chance within PER_MILLE and a reach "
        "within MAX_REACH");
  }
  for (SectSpec& sect : sects) {
    for (const Founder& founder : sect.founders) {
      Person& person = people_.at(founder.person);
      person.sect = sects_.size();
      if (!person.talent.empty()) {
        const std::size_t best = best_discipline(person);
        person.attainment.at(best) =
            std::min(founder.attainment, person.talent.at(best).limit);
      }
    }
    sects_.push_back(Sect{std::move(sect.name), sect.faction, sect.seat});
  }
  for (std::size_t sect = 0; sect < manuals.held.size(); ++sect) {
    for (const std::size_t taught : manuals.held.at(sect)) {
      manuals_.push_back(Manual{taught, HeldBySect{sect}});
    }
  }
}

// The highest attainment person `person` holds in any discipline.
std::uint32_t World::best_attainment(std::size_t person) const {
  const std::vector<std::uint32_t>& attainment = people_.at(person).attainment;
  return attainment.empty()
             ? 0
             : *std::max_element(attainment.begin(), attainment.end());
}

// The daily check of sects: a member serving in an army of another faction
// leaves its sect; a fallen sect's manuals each scatter or are lost; then the
// faction's people are admitted.
World::Living World::living_people() const {
  Living living{{}, std::vector<std::size_t>(people_.size())};
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      living.people.push_back(member);
      living.army_of.at(member) = army;
    }
  }
  std::sort(living.people.begin(), living.people.end());
  return living;
}

void World::keep_sects() {
  const Living living = living_people();
  std::vector<std::size_t> bereft;
  for (const std::size_t member : living.people) {
    std::optional<std::size_t>& sect = people_.at(member).sect;
    if (sect && armies_.at(living.army_of.at(member)).faction !=
                    sects_.at(*sect).faction) {
      bereft.push_back(*sect);
      sect.reset();
    }
  }
  note_falls(std::move(bereft));
  const Manuals scattering = life_.manuals.value_or(Manuals{});
  for (Manual& manual : manuals_) {
    const auto* held = std::get_if<HeldBySect>(&manual.place);
    if (held == nullptr || sect_stands(held->sect)) {
      continue;
    }
    const Cell centre = seat(sects_.at(held->sect).seat);
    std::vector<Cell> cells;
    const std::int32_t reach = scattering.scatter_reach;
    for (const Cell cell :
         cells_of(Cell{centre.x - reach, centre.y - reach},
                  Footprint{(2 * reach) + 1, (2 * reach) + 1})) {
      if (map_.passable(cell)) {
        cells.push_back(cell);
      }
    }
    const bool scatters =
        next_random() % PER_MILLE < scattering.scatter_chance && !cells.empty();
    manual.place = scatters ? std::variant<HeldBySect, Cell, Lost>{cells.at(
                                  next_random() % cells.size())}
                            : std::variant<HeldBySect, Cell, Lost>{Lost{}};
  }
  admit(living);
}

// What sect `sect` offers an applicant now: whether its seat is held, whether
// it has a living elder, its members and its cap.
World::Opening World::opening(std::size_t sect) const {
  return Opening{seat_held(sect), !sect_elders(sect).empty(),
                 sect_members(sect).size(), sect_cap(sect)};
}

// Why person `person`, serving in an army of `faction`, or in none once dead,
// may not join sect `sect`, as `open` finds it: the first refusal that holds,
// or NONE.
ApplicationRefusal World::assess(std::size_t person,
                                 std::optional<std::size_t> faction,
                                 std::size_t sect, const Opening& open) const {
  const Person& applicant = people_.at(person);
  if (!faction) {
    return ApplicationRefusal::DEAD;
  }
  if (applicant.sect) {
    return ApplicationRefusal::IN_A_SECT;
  }
  if (!applicant.home) {
    return ApplicationRefusal::NO_HOME;
  }
  const std::optional<Admission>& admission =
      leadership_.at(*faction).admission;
  if (*faction != sects_.at(sect).faction || !admission) {
    return ApplicationRefusal::NOT_OF_FACTION;
  }
  if (age_in_days(applicant) < static_cast<std::int64_t>(admission->age)) {
    return ApplicationRefusal::TOO_YOUNG;
  }
  bool able = false;
  for (std::size_t discipline = 0; discipline < applicant.talent.size();
       ++discipline) {
    able =
        able || (applicant.talent.at(discipline).limit >= admission->talent &&
                 applicant.attainment.at(discipline) >= admission->attainment);
  }
  if (!able) {
    return ApplicationRefusal::UNPROVEN;
  }
  if (!within(seat(*applicant.home), seat(sects_.at(sect).seat),
              admission->reach)) {
    return ApplicationRefusal::TOO_FAR;
  }
  if (!open.held) {
    return ApplicationRefusal::SEAT_LOST;
  }
  if (!open.elder) {
    return ApplicationRefusal::NO_ELDER;
  }
  if (open.members >= open.cap) {
    return ApplicationRefusal::FULL;
  }
  return ApplicationRefusal::NONE;
}

// Admits each living person of no sect, in index order, by its faction's
// admission's chance, to one of the sects it may join, drawn by the seed.
void World::admit(const Living& living) {
  std::vector<Opening> openings;
  openings.reserve(sects_.size());
  for (std::size_t sect = 0; sect < sects_.size(); ++sect) {
    openings.push_back(opening(sect));
  }
  for (const std::size_t candidate : living.people) {
    const std::size_t faction =
        armies_.at(living.army_of.at(candidate)).faction;
    const std::optional<Admission>& admission =
        leadership_.at(faction).admission;
    std::vector<std::size_t> sects;
    for (std::size_t sect = 0; sect < sects_.size(); ++sect) {
      if (assess(candidate, faction, sect, openings.at(sect)) ==
          ApplicationRefusal::NONE) {
        sects.push_back(sect);
      }
    }
    if (sects.empty() || !admission ||
        next_random() % (std::uint64_t{PER_MILLE} * DAYS_PER_YEAR) >=
            admission->chance) {
      continue;
    }
    const std::size_t joined = sects.at(next_random() % sects.size());
    people_.at(candidate).sect = joined;
    ++openings.at(joined).members;
  }
}

ApplicationRefusal World::application_refusal(std::size_t person,
                                              std::size_t sect) const {
  if (person >= people_.size() || sect >= sects_.size()) {
    throw std::invalid_argument("no such person or sect applies");
  }
  return assess(person, faction_of(person), sect, opening(sect));
}

void World::apply(std::size_t person, std::size_t sect) {
  if (application_refusal(person, sect) != ApplicationRefusal::NONE) {
    throw std::invalid_argument(
        "an applicant must meet its faction's admission and the sect have "
        "room");
  }
  people_.at(person).sect = sect;
}

// Records `what`, which happened at `cell` on this tick, letting the oldest
// event go once EVENT_CAPACITY are kept.
void World::record(Cell cell, Happening what) {
  const std::uint64_t sequence =
      events_.empty() ? 0 : events_.back().sequence + 1;
  if (events_.size() == EVENT_CAPACITY) {
    events_.erase(events_.begin());
  }
  events_.push_back(Event{sequence, tick_count_, cell, what});
}

// Records the fall of each of `sects` that no longer stands, once each; a
// sect's fall is noted when its last member leaves.
void World::note_falls(std::vector<std::size_t> sects) {
  std::sort(sects.begin(), sects.end());
  sects.erase(std::unique(sects.begin(), sects.end()), sects.end());
  for (const std::size_t sect : sects) {
    if (!sect_stands(sect)) {
      record(seat(sects_.at(sect).seat),
             SectFell{sect, sects_.at(sect).faction});
    }
  }
}

// Records the death of every person on a roster with no health left, and
// returns the sects each was a member of.
std::vector<std::size_t> World::record_deaths() {
  std::vector<std::size_t> bereft;
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      const Person& dead = people_.at(member);
      if (dead.health == 0) {
        record(cell_of(dead.position), Died{member});
        if (dead.sect) {
          bereft.push_back(*dead.sect);
        }
      }
    }
  }
  return bereft;
}

const std::vector<Event>& World::events() const { return events_; }

const std::vector<Progress>& World::progress() const { return progress_; }

// Notes `what`, a step of progress on this tick, letting the oldest go once
// PROGRESS_CAPACITY are kept.
void World::note(Advance what) {
  const std::uint64_t sequence =
      progress_.empty() ? 0 : progress_.back().sequence + 1;
  if (progress_.size() == PROGRESS_CAPACITY) {
    progress_.erase(progress_.begin());
  }
  progress_.push_back(Progress{sequence, tick_count_, what});
}

// Whether the progress kept is as note() keeps it: at most
// PROGRESS_CAPACITY, oldest first, numbered one after another, none later
// than the world's tick, each naming a person, art, skill or realm the world
// holds.
bool World::progress_kept() const {
  bool bad = progress_.size() > PROGRESS_CAPACITY;
  for (std::size_t index = 0; index < progress_.size(); ++index) {
    const Progress& step = progress_.at(index);
    const bool unordered =
        index > 0 && (step.sequence != progress_.at(index - 1).sequence + 1 ||
                      step.tick < progress_.at(index - 1).tick);
    bad = bad || unordered || step.tick > tick_count_ ||
          stray_advance(step.what, people_.size(), life_);
  }
  return !bad;
}

// Whether `other` is `one`'s kin: itself, of its family, its spouse, parent
// or child.
bool World::related(std::size_t one, std::size_t other) const {
  const Person& self = people_.at(one);
  const auto among = [other](const std::vector<std::size_t>& people) {
    return std::find(people.begin(), people.end(), other) != people.end();
  };
  return one == other || self.family == people_.at(other).family ||
         among(self.spouses) || among(self.parents) || among(self.children);
}

// Whether event `event` reaches person `person`, of `faction` if it serves
// in an army, as a rumour.
bool World::heard_by(const Event& event, std::size_t person,
                     std::optional<std::size_t> faction) const {
  if (within(cell_of(people_.at(person).position), event.cell,
             rules_.rumour_reach)) {
    return true;
  }
  const auto ours = [faction](std::size_t other) { return faction == other; };
  if (const auto* born = std::get_if<Born>(&event.what)) {
    return related(person, born->person);
  }
  if (const auto* died = std::get_if<Died>(&event.what)) {
    return related(person, died->person);
  }
  if (const auto* founded = std::get_if<Founded>(&event.what)) {
    return ours(founded->faction);
  }
  if (const auto* raided = std::get_if<Raided>(&event.what)) {
    return ours(raided->owner) || ours(raided->raider);
  }
  if (const auto* ruined = std::get_if<Ruined>(&event.what)) {
    return ours(ruined->owner) || ours(ruined->destroyer.faction);
  }
  if (const auto* fell = std::get_if<SectFell>(&event.what)) {
    return ours(fell->faction);
  }
  const auto* changed = std::get_if<LeaderChanged>(&event.what);
  return changed != nullptr && ours(changed->faction);
}

std::vector<Event> World::rumours(std::size_t person) const {
  if (person >= people_.size()) {
    throw std::out_of_range("no such person hears rumours");
  }
  const std::optional<std::size_t> faction = faction_of(person);
  std::vector<Event> heard;
  for (auto event = events_.rbegin(); event != events_.rend(); ++event) {
    if (heard_by(*event, person, faction)) {
      heard.push_back(*event);
    }
  }
  return heard;
}

TalkRefusal World::talk_refusal(std::size_t person, std::size_t other) const {
  if (person >= people_.size() || other >= people_.size()) {
    throw std::invalid_argument("no such person talks to no such person");
  }
  if (people_.at(person).health == 0 || people_.at(other).health == 0) {
    return TalkRefusal::DEAD;
  }
  if (person == other) {
    return TalkRefusal::SELF;
  }
  if (!within(cell_of(people_.at(person).position),
              cell_of(people_.at(other).position), rules_.talk_reach)) {
    return TalkRefusal::OUT_OF_REACH;
  }
  const std::optional<std::size_t> own = faction_of(person);
  const std::optional<std::size_t> theirs = faction_of(other);
  if (own && theirs) {
    const Relation standing = relation(*own, *theirs);
    if (standing == Relation::HOSTILE || standing == Relation::AT_WAR) {
      return TalkRefusal::HOSTILE;
    }
  }
  return TalkRefusal::NONE;
}

std::vector<Offer> World::offers(std::size_t person) const {
  if (people_.at(person).health == 0) {
    return {};
  }
  std::vector<Offer> offered{Offer::NEWS};
  for (std::size_t ware = 0; ware < wares().size(); ++ware) {
    if (sells(person, ware)) {
      offered.push_back(Offer::TRADE);
      break;
    }
  }
  return offered;
}

const std::vector<Ware>& World::wares() const {
  static const std::vector<Ware> none;
  return life_.trade ? life_.trade->wares : none;
}

bool World::sells(std::size_t person, std::size_t ware) const {
  if (person >= people_.size() || ware >= wares().size()) {
    throw std::out_of_range("no such person sells no such ware");
  }
  const Person& seller = people_.at(person);
  const Ware& sold = wares().at(ware);
  return seller.health > 0 && !seller.talent.empty() &&
         best_discipline(seller) == sold.discipline;
}

BuyRefusal World::buy_refusal(std::size_t buyer, std::size_t seller,
                              std::size_t ware) const {
  if (buyer >= people_.size() || seller >= people_.size() ||
      ware >= wares().size()) {
    throw std::invalid_argument("no such person buys no such ware");
  }
  const TalkRefusal talk = talk_refusal(buyer, seller);
  if (talk == TalkRefusal::DEAD) {
    return BuyRefusal::DEAD;
  }
  if (talk != TalkRefusal::NONE) {
    return BuyRefusal::CANNOT_TALK;
  }
  if (!sells(seller, ware)) {
    return BuyRefusal::NOT_SOLD;
  }
  if (people_.at(buyer).silver < wares().at(ware).price) {
    return BuyRefusal::NO_SILVER;
  }
  return BuyRefusal::NONE;
}

void World::buy(std::size_t buyer, std::size_t seller, std::size_t ware) {
  if (buy_refusal(buyer, seller, ware) != BuyRefusal::NONE) {
    throw std::invalid_argument(
        "a buyer must be able to talk to a seller of the ware and pay for it");
  }
  const std::uint64_t price = wares().at(ware).price;
  people_.at(buyer).silver -= price;
  people_.at(seller).silver += price;
  ++people_.at(buyer).held.at(ware);
}

ConsumeRefusal World::consume_refusal(std::size_t person,
                                      std::size_t ware) const {
  if (person >= people_.size() || ware >= wares().size()) {
    throw std::invalid_argument("no such person consumes no such ware");
  }
  const Person& consumer = people_.at(person);
  if (consumer.health == 0) {
    return ConsumeRefusal::DEAD;
  }
  if (consumer.held.at(ware) == 0) {
    return ConsumeRefusal::NONE_HELD;
  }
  const Ware& taken = wares().at(ware);
  const bool heals = taken.health > 0 && consumer.health < life_.newborn_health;
  const bool restores = taken.qi > 0 && consumer.qi < consumer.qi_capacity;
  return heals || restores ? ConsumeRefusal::NONE : ConsumeRefusal::FULL;
}

void World::consume(std::size_t person, std::size_t ware) {
  if (consume_refusal(person, ware) != ConsumeRefusal::NONE) {
    throw std::invalid_argument(
        "a consumer must live, hold the ware and gain from it");
  }
  Person& consumer = people_.at(person);
  const Ware& taken = wares().at(ware);
  --consumer.held.at(ware);
  if (consumer.health < life_.newborn_health) {
    consumer.health =
        std::min(life_.newborn_health, consumer.health + taken.health);
  }
  consumer.qi = static_cast<std::uint32_t>(std::min<std::uint64_t>(
      consumer.qi_capacity, std::uint64_t{consumer.qi} + taken.qi));
}

// Once a day, every living person with a home earns the trade's wage.
void World::pay(const Trade& trade) {
  for (Person& person : people_) {
    if (person.health > 0 && person.home) {
      person.silver += trade.wage;
    }
  }
}

// Once a day, every living person no driver steers buys one of each ware it
// holds none of from the first seller, by index, it may buy it from.
void World::shop(const Trade& trade) {
  for (std::size_t ware = 0; ware < trade.wares.size(); ++ware) {
    std::vector<std::size_t> sellers;
    for (std::size_t person = 0; person < people_.size(); ++person) {
      if (sells(person, ware)) {
        sellers.push_back(person);
      }
    }
    for (const Army& army : armies_) {
      if (army.steered) {
        continue;
      }
      for (const std::size_t buyer : army.roster) {
        if (people_.at(buyer).held.at(ware) > 0) {
          continue;
        }
        const auto seller = std::find_if(
            sellers.begin(), sellers.end(), [&](std::size_t candidate) {
              return buy_refusal(buyer, candidate, ware) == BuyRefusal::NONE;
            });
        if (seller != sellers.end()) {
          buy(buyer, *seller, ware);
        }
      }
    }
  }
}

// Each tick, every living person no driver steers consumes the first ware it
// may that restores health while it is wounded, and the first that restores
// qi while it is drained, by the trade's shares.
void World::dose(const Trade& trade) {
  const auto take_first = [this, &trade](std::size_t member, auto restores) {
    for (std::size_t ware = 0; ware < trade.wares.size(); ++ware) {
      if (restores(trade.wares.at(ware)) &&
          consume_refusal(member, ware) == ConsumeRefusal::NONE) {
        consume(member, ware);
        return;
      }
    }
  };
  for (const Army& army : armies_) {
    if (army.steered) {
      continue;
    }
    for (const std::size_t member : army.roster) {
      const Person& person = people_.at(member);
      const bool wounded =
          person.health * PER_MILLE <= life_.newborn_health * trade.wounded;
      const bool drained = std::uint64_t{person.qi} * PER_MILLE <=
                           std::uint64_t{person.qi_capacity} * trade.drained;
      const auto restores_health = [](const Ware& ware) {
        return ware.health > 0;
      };
      const auto restores_qi = [](const Ware& ware) { return ware.qi > 0; };
      if (wounded) {
        take_first(member, restores_health);
      }
      if (drained) {
        take_first(member, restores_qi);
      }
    }
  }
}

// Makes the master of faction `faction`'s sects with the most favour its
// leader, each elder of its sects favouring its own sect's master; ties go to
// the higher best attainment, then the lower index. A faction whose sects
// have no master keeps its leader.
void World::elect(std::size_t faction) {
  std::optional<std::size_t> chosen;
  std::size_t most = 0;
  for (std::size_t sect = 0; sect < sects_.size(); ++sect) {
    const std::optional<std::size_t> master = sect_master(sect);
    if (sects_.at(sect).faction != faction || !master) {
      continue;
    }
    const std::size_t favour = sect_elders(sect).size();
    const bool better =
        !chosen || favour > most ||
        (favour == most &&
         (best_attainment(*master) > best_attainment(*chosen) ||
          (best_attainment(*master) == best_attainment(*chosen) &&
           *master < *chosen)));
    if (better) {
      chosen = master;
      most = favour;
    }
  }
  if (chosen && chosen != leadership_.at(faction).leader) {
    leadership_.at(faction).leader = chosen;
    record(cell_of(people_.at(*chosen).position),
           LeaderChanged{faction, *chosen, ChosenBy::ELECTION});
  }
}

std::size_t World::sect_count() const { return sects_.size(); }

std::optional<std::size_t> World::sect_of(std::size_t person) const {
  return people_.at(person).sect;
}

const std::string& World::sect_name(std::size_t sect) const {
  return sects_.at(sect).name;
}

std::size_t World::sect_faction(std::size_t sect) const {
  return sects_.at(sect).faction;
}

std::size_t World::sect_seat(std::size_t sect) const {
  return sects_.at(sect).seat;
}

bool World::seat_held(std::size_t sect) const {
  const Sect& held = sects_.at(sect);
  return village_state(held.seat) != VillageState::RUINS &&
         owner(held.seat) == held.faction;
}

std::vector<std::size_t> World::sect_members(std::size_t sect) const {
  std::vector<std::size_t> members;
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      if (people_.at(member).sect == sect) {
        members.push_back(member);
      }
    }
  }
  std::sort(members.begin(), members.end());
  return members;
}

std::vector<std::size_t> World::sect_elders(std::size_t sect) const {
  const std::optional<Admission>& admission =
      leadership_.at(sects_.at(sect).faction).admission;
  std::vector<std::size_t> elders;
  if (!admission) {
    return elders;
  }
  std::vector<std::size_t> members = sect_members(sect);
  std::stable_sort(members.begin(), members.end(),
                   [this](std::size_t left, std::size_t right) {
                     return best_attainment(left) > best_attainment(right);
                   });
  const std::size_t share = members.size() * admission->elder_share / PER_MILLE;
  const std::size_t count =
      std::min(members.size(), std::max<std::size_t>(share, 1));
  elders.assign(members.begin(),
                members.begin() + static_cast<std::ptrdiff_t>(count));
  std::sort(elders.begin(), elders.end());
  return elders;
}

std::optional<std::size_t> World::sect_master(std::size_t sect) const {
  std::optional<std::size_t> master;
  for (const std::size_t elder : sect_elders(sect)) {
    if (!master || best_attainment(elder) > best_attainment(*master)) {
      master = elder;
    }
  }
  return master;
}

std::size_t World::sect_cap(std::size_t sect) const {
  return seat_held(sect) ? held(sects_.at(sect).seat, Stage::DAMAGED).pupils
                         : 0;
}

bool World::sect_stands(std::size_t sect) const {
  return std::any_of(
      armies_.begin(), armies_.end(), [this, sect](const Army& army) {
        return std::any_of(army.roster.begin(), army.roster.end(),
                           [this, sect](std::size_t member) {
                             return people_.at(member).sect == sect;
                           });
      });
}

const std::vector<Manual>& World::manuals() const { return manuals_; }

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
  if (order.target &&
      attack_refusal(army, *order.target) != AttackRefusal::NONE) {
    throw std::invalid_argument(
        "an attack intent must name a living foe of the army's lead within "
        "sight of one of its members");
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

AttackRefusal World::attack_refusal(std::size_t army,
                                    std::size_t person) const {
  if (army >= armies_.size()) {
    throw std::invalid_argument("no such army");
  }
  const std::vector<std::size_t>& roster = armies_.at(army).roster;
  if (roster.empty()) {
    return AttackRefusal::NO_MEMBER;
  }
  const std::optional<std::size_t> served =
      person < people_.size() ? army_of(person) : std::nullopt;
  if (!served || people_.at(person).health == 0) {
    return AttackRefusal::NO_TARGET;
  }
  if (!foes(roster.front(), army, person, *served)) {
    return AttackRefusal::NOT_A_FOE;
  }
  return sighted(roster, people_.at(person).position)
             ? AttackRefusal::NONE
             : AttackRefusal::OUT_OF_SIGHT;
}

std::optional<std::size_t> World::target(std::size_t army) const {
  return armies_.at(army).order.target;
}

Cell World::objective(std::size_t army) const {
  return armies_.at(army).order.objective;
}

std::vector<std::size_t> World::foes_in_sight(std::size_t person) const {
  if (person >= people_.size()) {
    throw std::invalid_argument("no such person");
  }
  const std::optional<std::size_t> army = army_of(person);
  if (!army || people_.at(person).health == 0) {
    return {};
  }
  const Position from = people_.at(person).position;
  const std::int64_t sight = std::int64_t{rules_.sight} * CELL_SIZE;
  std::vector<std::pair<std::int64_t, std::size_t>> seen;
  for (std::size_t other_army = 0; other_army < armies_.size(); ++other_army) {
    for (const std::size_t other : armies_.at(other_army).roster) {
      const std::int64_t distance =
          distance_squared(from, people_.at(other).position);
      if (distance <= sight * sight && foes(person, *army, other, other_army)) {
        seen.emplace_back(distance, other);
      }
    }
  }
  std::sort(seen.begin(), seen.end());
  std::vector<std::size_t> nearest_first;
  nearest_first.reserve(seen.size());
  for (const auto& [distance, other] : seen) {
    nearest_first.push_back(other);
  }
  return nearest_first;
}

// Whether a member of `roster` has `point` within the rules' sight.
bool World::sighted(const std::vector<std::size_t>& roster,
                    Position point) const {
  const std::int64_t sight = std::int64_t{rules_.sight} * CELL_SIZE;
  return std::any_of(roster.begin(), roster.end(), [&](std::size_t member) {
    return distance_squared(people_.at(member).position, point) <=
           sight * sight;
  });
}

// Ends each attack intent whose target is dead, no foe of its army's lead,
// or out of every member's sight, positions read as the tick began, as hold()
// does.
void World::end_intents(const Crowd& crowd) {
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    Army& chasing = armies_.at(army);
    Order& order = chasing.order;
    if (!order.target) {
      continue;
    }
    const std::size_t aimed = crowd.slot.at(*order.target);
    const std::vector<std::size_t>& roster = chasing.roster;
    const bool held =
        aimed != ABSENT && people_.at(*order.target).health > 0 &&
        !roster.empty() &&
        foes(roster.front(), army, *order.target, crowd.armies.at(aimed)) &&
        sighted(roster, crowd.hash.position(aimed));
    if (!held) {
      hold(chasing);
    }
  }
}

void World::end_intent(std::size_t army) {
  Army& chasing = armies_.at(army);
  if (chasing.order.target) {
    hold(chasing);
  }
}

// Drops army `chasing`'s attack intent and has it hold where its lead stands,
// its objective that cell, rather than walk back to the objective it chased
// from.
void World::hold(Army& chasing) {
  Order& order = chasing.order;
  order.target.reset();
  if (!chasing.roster.empty()) {
    order.objective = cell_of(people_.at(chasing.roster.front()).position);
    chasing.field = FlowField{map_, order.objective};
  }
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

void World::cast(std::size_t person, std::size_t skill, const Target& target) {
  if (cast_refusal(person, skill, target) != CastRefusal::NONE) {
    throw std::invalid_argument(
        "a cast must come from a living person, ready on the next tick, of a "
        "skill its army has, at a living person within its range");
  }
  people_.at(person).casting = Cast{skill, target};
}

CastRefusal World::cast_refusal(std::size_t person, std::size_t skill,
                                const Target& target) const {
  if (person >= people_.size()) {
    throw std::invalid_argument("no such person casts");
  }
  return assess_cast(person, army_of(person), Cast{skill, target},
                     tick_count_ + 1);
}

std::optional<std::size_t> World::aim(std::size_t person, std::size_t skill,
                                      Step facing) const {
  if (person >= people_.size() || std::abs(facing.x) > MAX_REACH ||
      std::abs(facing.y) > MAX_REACH) {
    throw std::invalid_argument(
        "an aim must be a person's, facing no farther than MAX_REACH");
  }
  // One who could never cast the skill, being dead or without it, aims at
  // no one; one short of qi or cooling down aims as it will once it may.
  const std::optional<std::size_t> army = army_of(person);
  const CastRefusal never =
      assess_cast(person, army, Cast{skill, Itself{}}, UINT64_LIMIT);
  if (!army || never == CastRefusal::DEAD || never == CastRefusal::NO_SKILL) {
    return std::nullopt;
  }
  const Position from = people_.at(person).position;
  const std::int64_t range =
      usable(people_.at(person), armies_.at(*army), skill).range;
  // Offered in the order the tick's strikes visit people.
  Nearest nearest{from, facing};
  std::vector<std::size_t> offered;
  for (std::size_t other_army = 0; other_army < armies_.size(); ++other_army) {
    for (const std::size_t other : armies_.at(other_army).roster) {
      const Position where = people_.at(other).position;
      if (distance_squared(from, where) <= range * range &&
          foes(person, *army, other, other_army)) {
        nearest.offer(offered.size(), where);
        offered.push_back(other);
      }
    }
  }
  const std::optional<std::size_t> found = nearest.found();
  return found ? std::optional<std::size_t>{offered.at(*found)} : std::nullopt;
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
  std::vector<SpatialHash::Placed> placed;
  std::vector<std::size_t> people;
  std::vector<std::size_t> owners;
  // Each living person's index in the crowd.
  std::vector<std::size_t> slot(people_.size(), ABSENT);
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      slot.at(member) = people.size();
      // Marked by the faction of the army each served in as the tick began,
      // not by relation: foe_kinds() reads relations live at each search,
      // and no army changes faction before the last search, in steer(), so
      // the marks hold for the whole tick.
      placed.push_back(SpatialHash::Placed{people_.at(member).position,
                                           kind_of(armies_.at(army).faction)});
      people.push_back(member);
      owners.push_back(army);
    }
  }
  const Crowd crowd{SpatialHash{std::move(placed)}, std::move(people),
                    std::move(owners), std::move(slot)};
  std::vector<std::uint64_t> damage(crowd.people.size(), 0);
  const std::vector<std::uint32_t> edges = served().edge;
  strikes_.clear();
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    strike(index, crowd, edges, damage);
  }
  // Damage lands after everyone has struck, so the order people strike in
  // does not decide who strikes. The dead leave their army's roster.
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    Person& struck = people_.at(crowd.people.at(index));
    struck.health -= std::min(damage.at(index), struck.health);
  }
  if (life_.trade) {
    dose(*life_.trade);
  }
  settle_duels();
  age();
  if (life_.husbandry && tick_count_ % rules_.ticks_per_day == 0) {
    husband(*life_.husbandry);
  }
  // A soul whose body died this tick is born again, into the army it served.
  const std::vector<Passing> reborn = passings();
  std::vector<std::size_t> bereft = record_deaths();
  for (Army& army : armies_) {
    army.roster.erase(std::remove_if(army.roster.begin(), army.roster.end(),
                                     [this](std::size_t member) {
                                       return people_.at(member).health == 0;
                                     }),
                      army.roster.end());
  }
  note_falls(std::move(bereft));
  end_intents(crowd);
  std::vector<Step> pulls(crowd.people.size(), Step{0, 0});
  std::vector<bool> standing(crowd.people.size(), true);
  steer(crowd, crowd.slot, pulls, standing);
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    march(army, crowd, crowd.slot, pulls, standing);
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
  train();
  if (!sects_.empty() && tick_count_ % rules_.ticks_per_day == 0) {
    keep_sects();
    teach();
  }
  if (tick_count_ % rules_.ticks_per_day == 0) {
    tend();
    if (life_.trade) {
      pay(*life_.trade);
      shop(*life_.trade);
    }
    hold_elections(day);
    if (life_.diplomacy) {
      negotiate(*life_.diplomacy);
    }
  }
}

// Elects a leader, on day `day`, for each faction with an election whose
// leader is dead or missing, or whose term `day` completes.
void World::hold_elections(std::uint64_t day) {
  for (std::size_t faction = 0; faction < leadership_.size(); ++faction) {
    const Leadership& lead = leadership_.at(faction);
    if (!lead.election) {
      continue;
    }
    const bool vacant = !lead.leader || people_.at(*lead.leader).health == 0;
    const std::uint32_t term = lead.election->term;
    if (vacant || (term > 0 && day % term == 0)) {
      elect(faction);
    }
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

// Each person's pull toward its objective, from where it stood: none
// within its arrival ring, as many cells from the objective as the square
// root of its rank in its army, and none while a living foe on the move
// within its reach lies ahead along it, so it holds and fights rather than
// walking past. A person within two cells of its ring counts as standing
// still, so a crowd jostled a little stays standing. One no driver steers,
// with a foe in sight, closes on it instead, or stands to fight one within
// its reach.
void World::steer(const Crowd& crowd, const std::vector<std::size_t>& slot,
                  std::vector<Step>& pulls, std::vector<bool>& standing) const {
  for (const Army& army : armies_) {
    for (std::size_t rank = 0; rank < army.roster.size(); ++rank) {
      const std::size_t index = slot.at(army.roster.at(rank));
      const Cell cell = cell_of(crowd.hash.position(index));
      const auto ring =
          static_cast<std::uint32_t>(root(static_cast<std::int64_t>(rank)));
      const std::uint32_t distance = army.field.distance(cell);
      pulls.at(index) = distance > ring ? army.field.step(cell) : Step{0, 0};
      standing.at(index) = distance <= ring + STANDING_SLACK;
      if (const std::optional<Step> engaged = closing(index, army, crowd)) {
        pulls.at(index) = *engaged;
        standing.at(index) =
            standing.at(index) && engaged->x == 0 && engaged->y == 0;
      }
    }
  }
  // Read from the pulls as they were, so the order people are held in
  // decides nothing.
  const std::vector<Step> drawn = pulls;
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    if (held(index, drawn.at(index), standing, crowd)) {
      pulls.at(index) = Step{0, 0};
    }
  }
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
    fold_person(fold, person, people_.size(), life_.cultivation.has_value());
  }
  for (const Soul& soul : souls_) {
    fold_soul(fold, soul);
  }
  for (const std::size_t surname : surnames_) {
    fold(surname);
  }
  fold(rules_.ticks_per_day);
  for (const Army& army : armies_) {
    fold_order(fold, army.order);
    fold(army.faction);
    for (const Skill& skill : army.skills) {
      fold_skill(fold, skill);
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
    fold_selection(fold, faction);
  }
  for (const Sect& sect : sects_) {
    fold(sect.faction);
    fold(sect.seat);
  }
  for (const Manual& manual : manuals_) {
    fold_manual(fold, manual);
  }
  for (const Fight& fight : duels_) {
    fold(fight.faction);
    fold(fight.challenger);
    fold(fight.leader);
  }
  fold_relations(fold, declarations_, armies_, life_.diplomacy);
  for (const Village& village : villages_) {
    fold_village(fold, village);
  }
  fold_reaches(fold, rules_);
  fold_trade(fold, life_.trade);
  for (const Event& event : events_) {
    fold_event(fold, event);
  }
  // Folded only when held, so a world where no one learns keeps its hash.
  for (const Progress& step : progress_) {
    fold_progress(fold, step);
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
    return hostile(army.faction, owned_by) &&
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
    record(cell_of(people_.at(fight.challenger).position),
           LeaderChanged{fight.faction, fight.challenger, ChosenBy::DUEL});
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

bool World::elects(std::size_t faction) const {
  return leadership_.at(faction).election.has_value();
}

bool World::hostile(std::size_t from, std::size_t target) const {
  const std::optional<Declared>& standing =
      declarations_.at(pair_of(from, target));
  if (standing) {
    return standing->what == Declaration::WAR;
  }
  return rules_.hostile.at(from).at(target);
}

std::size_t World::pair_of(std::size_t faction, std::size_t other) const {
  const std::size_t factions = rules_.hostile.size();
  if (faction >= factions || other >= factions) {
    throw std::out_of_range("no such faction");
  }
  return (std::min(faction, other) * factions) + std::max(faction, other);
}

Relation World::relation(std::size_t faction, std::size_t other) const {
  const std::optional<Declared>& standing =
      declarations_.at(pair_of(faction, other));
  if (faction == other) {
    return Relation::OWN;
  }
  if (standing) {
    return standing->what == Declaration::WAR ? Relation::AT_WAR
                                              : Relation::NEUTRAL;
  }
  return rules_.hostile.at(faction).at(other) ||
                 rules_.hostile.at(other).at(faction)
             ? Relation::HOSTILE
             : Relation::ALLY;
}

std::optional<Declared> World::declaration(std::size_t faction,
                                           std::size_t other) const {
  return declarations_.at(pair_of(faction, other));
}

DeclarationRefusal World::declaration_refusal(std::size_t person,
                                              std::size_t faction,
                                              Declaration what) const {
  if (person >= people_.size() || faction >= rules_.hostile.size()) {
    throw std::invalid_argument("no such person declares toward no faction");
  }
  if (people_.at(person).health == 0) {
    return DeclarationRefusal::DEAD;
  }
  const std::optional<std::size_t> own = faction_of(person);
  if (!own || leadership_.at(*own).leader != person) {
    return DeclarationRefusal::NOT_THE_LEADER;
  }
  if (*own == faction) {
    return DeclarationRefusal::SAME_FACTION;
  }
  const Relation standing = relation(*own, faction);
  if (what == Declaration::WAR && standing == Relation::AT_WAR) {
    return DeclarationRefusal::ALREADY_AT_WAR;
  }
  if (what == Declaration::PEACE &&
      (standing == Relation::ALLY || standing == Relation::NEUTRAL)) {
    return DeclarationRefusal::ALREADY_AT_PEACE;
  }
  const std::optional<Declared>& last =
      declarations_.at(pair_of(*own, faction));
  const std::uint64_t hold =
      life_.diplomacy
          ? std::uint64_t{life_.diplomacy->hold_days} * rules_.ticks_per_day
          : 0;
  if (last && tick_count_ - last->tick < hold) {
    return DeclarationRefusal::TOO_SOON;
  }
  return DeclarationRefusal::NONE;
}

void World::declare(std::size_t person, std::size_t faction, Declaration what) {
  const std::optional<std::size_t> own = faction_of(person);
  if (declaration_refusal(person, faction, what) != DeclarationRefusal::NONE ||
      !own) {
    throw std::invalid_argument(
        "a declaration must come from a living faction's leader, toward "
        "another faction, change how the two stand and come after the last "
        "has held");
  }
  declarations_.at(pair_of(*own, faction)) = Declared{what, *own, tick_count_};
}

// What faction `faction`'s leader seeks toward faction `other` by the
// diplomacy, given each faction's living `numbers`, and its chance a year:
// war on one with living people at peace with it, while its own number at
// least the margin of the other's, or peace with one at war with it, while
// its own are fewer; nothing otherwise.
std::optional<World::Sought> World::sought(
    const Diplomacy& diplomacy, std::size_t faction, std::size_t other,
    const std::vector<std::uint64_t>& numbers) const {
  if (other == faction || numbers.at(other) == 0) {
    return std::nullopt;
  }
  const Relation standing = relation(faction, other);
  if (standing == Relation::ALLY || standing == Relation::NEUTRAL) {
    if (numbers.at(faction) * PER_MILLE <
        numbers.at(other) * diplomacy.war_margin) {
      return std::nullopt;
    }
    return Sought{Declaration::WAR, diplomacy.war_chance};
  }
  if (standing == Relation::AT_WAR && numbers.at(faction) < numbers.at(other)) {
    return Sought{Declaration::PEACE, diplomacy.peace_chance};
  }
  return std::nullopt;
}

void World::steer(std::size_t army, bool steered) {
  armies_.at(army).steered = steered;
}

bool World::steered(std::size_t army) const { return armies_.at(army).steered; }

// Has each living leader no driver steers declare toward each other faction
// with living people, in index order, as the diplomacy says: war on one at
// peace with its faction while its own people number at least the margin of
// the other's, and peace with one at war with it while its own are fewer,
// each with its chance a year.
void World::negotiate(const Diplomacy& diplomacy) {
  const std::size_t factions = rules_.hostile.size();
  std::vector<std::uint64_t> numbers(factions, 0);
  for (const Army& army : armies_) {
    numbers.at(army.faction) += army.roster.size();
  }
  for (std::size_t faction = 0; faction < factions; ++faction) {
    const std::optional<std::size_t> leader = leadership_.at(faction).leader;
    const std::optional<std::size_t> army =
        leader ? army_of(*leader) : std::nullopt;
    if (!army || armies_.at(*army).steered) {
      continue;
    }
    for (std::size_t other = 0; other < factions; ++other) {
      const std::optional<Sought> wanted =
          sought(diplomacy, faction, other, numbers);
      if (wanted &&
          declaration_refusal(*leader, other, wanted->what) ==
              DeclarationRefusal::NONE &&
          next_random() % (std::uint64_t{PER_MILLE} * DAYS_PER_YEAR) <
              wanted->chance) {
        declare(*leader, other, wanted->what);
      }
    }
  }
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

// A person's use on this tick: the cast it was given, unless it lapses, or
// one of the skills it may use drawn by the seed at the nearest hostile
// person: its order's skills once their shared cooldown has passed, and each
// art skill it knows that is ready and that its qi pays for.
void World::strike(std::size_t user, const Crowd& crowd,
                   const std::vector<std::uint32_t>& edges,
                   std::vector<std::uint64_t>& damage) {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  Person& striker = people_.at(self);
  const Army& own = armies_.at(army);
  const std::optional<Cast> cast = std::exchange(striker.casting, std::nullopt);
  if (cast) {
    if (assess_cast(self, army, *cast, tick_count_) == CastRefusal::NONE) {
      use(user, crowd, cast->skill,
          landing(striker, usable(striker, own, cast->skill), cast->target),
          person_aimed_at(cast->target), edges, damage);
    }
    return;
  }
  const std::vector<std::size_t>& choice = own.order.skills;
  const bool drilled = striker.ready_at <= tick_count_;
  const auto affordable = [&](const Known& known) {
    return known.ready_at <= tick_count_ &&
           striker.qi >=
               life_.arts.at(known.art).skills.at(known.skill).skill.qi_cost;
  };
  const auto arts = static_cast<std::size_t>(
      std::count_if(striker.known.begin(), striker.known.end(), affordable));
  const std::size_t offered = (drilled ? choice.size() : 0) + arts;
  if (offered == 0) {
    return;
  }
  // The order's skills first, then the art skills in the order learned.
  std::size_t drawn = next_random() % offered;
  std::size_t index = 0;
  if (drilled && drawn < choice.size()) {
    index = choice.at(drawn);
  } else {
    drawn -= drilled ? choice.size() : 0;
    for (std::size_t known = 0; known < striker.known.size(); ++known) {
      if (affordable(striker.known.at(known))) {
        if (drawn == 0) {
          index = own.skills.size() + known;
          break;
        }
        --drawn;
      }
    }
  }
  const Skill& skill = usable(striker, own, index);
  // A self skill, or one of range 0, reaches only its user, so it lands where
  // it stands.
  if (skill.shape == Shape::SELF || skill.range == 0) {
    use(user, crowd, index, striker.position, std::nullopt, edges, damage);
    return;
  }
  if (const std::optional<std::size_t> target = aim_of(user, skill, crowd)) {
    use(user, crowd, index, crowd.hash.position(*target),
        crowd.people.at(*target), edges, damage);
  }
}

// The person at `user` of `crowd` faces with `skill`, by its index in the
// crowd: its army's attack intent's target while that lies in range, none
// while it lies out of range, and with no intent the nearest hostile person in
// range. The crowd's order breaks ties, so the order the query visits people
// in does not decide the target.
std::optional<std::size_t> World::aim_of(std::size_t user, const Skill& skill,
                                         const Crowd& crowd) const {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  const Position from = crowd.hash.position(user);
  const Army& own = armies_.at(army);
  if (own.order.target) {
    const std::size_t aimed = crowd.slot.at(*own.order.target);
    const std::int64_t range = skill.range;
    const bool reached =
        aimed != ABSENT &&
        distance_squared(from, crowd.hash.position(aimed)) <= range * range;
    return reached ? std::optional<std::size_t>{aimed} : std::nullopt;
  }
  return crowd.hash.nearest(
      foe_kinds(self, own), from, skill.range, [&](std::size_t other) {
        return foes(self, army, crowd.people.at(other), crowd.armies.at(other));
      });
}

// The one rule every skill resolves through, whoever uses it and however it
// was aimed: each use of skill `index`, among the user's, toward `toward`,
// spends its qi and hits once every hostile person its shape covers, or for
// a targeted skill the person `locked` alone, as much harder as the edge
// `edges` gives the user's home, the might of its realm and, for an art
// skill, the potency of its nature, and every structure its shape reaches.
void World::use(std::size_t user, const Crowd& crowd, std::size_t index,
                Position toward, std::optional<std::size_t> locked,
                const std::vector<std::uint32_t>& edges,
                std::vector<std::uint64_t>& damage) {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  Person& striker = people_.at(self);
  const Army& own = armies_.at(army);
  const Skill& skill = usable(striker, own, index);
  const std::optional<std::size_t> known =
      index >= own.skills.size()
          ? std::optional<std::size_t>{index - own.skills.size()}
          : std::nullopt;
  const std::uint64_t ready =
      tick_count_ + std::max<std::uint64_t>(skill.cooldown, 1);
  std::optional<std::size_t> art;
  if (known) {
    striker.known.at(*known).ready_at = ready;
    art = striker.known.at(*known).art;
  } else {
    striker.ready_at = ready;
  }
  striker.qi -= skill.qi_cost;
  const std::uint64_t edge = striker.home ? edges.at(*striker.home) : 0;
  const std::uint32_t rank = rank_of(striker);
  const std::uint64_t might =
      rank > 0 ? life_.realms.at(rank - 1).might : PER_MILLE;
  std::uint64_t body_damage =
      skill.body_damage * (PER_MILLE + edge) / PER_MILLE * might / PER_MILLE;
  if (art) {
    body_damage = body_damage * agreement(striker, *art, true) / PER_MILLE;
  }
  strikes_.push_back(Strike{self,
                            index,
                            skill.shape,
                            skill.range,
                            skill.radius,
                            skill.body_damage,
                            striker.position,
                            toward,
                            toward,
                            {}});
  const bool hit =
      cover(user, crowd, skill, toward, locked, body_damage, damage);
  // A use that hits anyone or anything practises its art's discipline, or,
  // for an army's skill, the striker's best discipline, its highest limit.
  if (hit && !striker.talent.empty()) {
    const std::size_t practised =
        art ? life_.arts.at(*art).discipline : best_discipline(striker);
    const Aptitude& aptitude = striker.talent.at(practised);
    striker.attainment.at(practised) = std::min(
        aptitude.limit, striker.attainment.at(practised) + aptitude.growth);
  }
}

// Whether one use of `skill` by the person at `user` of `crowd` toward
// `toward` hits anyone or anything: every hostile person its shape covers
// takes `body_damage` into `damage`, or for a targeted skill the person
// `locked` alone, noted among the people the use's record struck, and every
// structure its shape reaches its structure damage. The record also notes how
// far the shape reaches.
bool World::cover(std::size_t user, const Crowd& crowd, const Skill& skill,
                  Position toward, std::optional<std::size_t> locked,
                  std::uint64_t body_damage,
                  std::vector<std::uint64_t>& damage) {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  bool hit = false;
  if (skill.shape == Shape::TARGETED) {
    // Only the person locked on, through its slot in the crowd.
    const std::size_t other =
        locked && *locked < crowd.slot.size() ? crowd.slot.at(*locked) : ABSENT;
    if (other != ABSENT &&
        foes(self, army, crowd.people.at(other), crowd.armies.at(other))) {
      damage.at(other) += body_damage;
      strikes_.back().struck.push_back(crowd.people.at(other));
      hit = true;
    }
    return hit;
  }
  const Swath swath{skill, crowd.hash.position(user), toward};
  strikes_.back().end = swath.end();
  crowd.hash.visit_within(
      swath.centre(), swath.extent(), [&](std::size_t other) {
        if (swath.covers(crowd.hash.position(other)) &&
            foes(self, army, crowd.people.at(other), crowd.armies.at(other))) {
          damage.at(other) += body_damage;
          strikes_.back().struck.push_back(crowd.people.at(other));
          hit = true;
        }
      });
  const Destroyer striker{army, armies_.at(army).faction};
  structure_index_.visit_within(
      swath.centre(), swath.extent() + structure_extent_,
      [&](std::size_t index) {
        if (swath.reaches(structures_.at(index))) {
          hit = shift(index, striker, skill.structure_damage) || hit;
        }
      });
  return hit;
}

// Skill `skill` of person `person`, serving in `army`: the army's skills,
// followed by the art skills it knows.
const Skill& World::usable(const Person& person, const Army& army,
                           std::size_t skill) const {
  if (skill < army.skills.size()) {
    return army.skills.at(skill);
  }
  const Known& known = person.known.at(skill - army.skills.size());
  return life_.arts.at(known.art).skills.at(known.skill).skill;
}

// Whether person `person`, serving in `army`, may use its skill `skill` on
// tick `due`.
bool World::ready(const Person& person, std::size_t skill, const Army& army,
                  std::uint64_t due) {
  const std::uint64_t ready_at =
      skill < army.skills.size()
          ? person.ready_at
          : person.known.at(skill - army.skills.size()).ready_at;
  return ready_at <= due;
}

// How many of the realms person `person`'s qi capacity reaches.
std::uint32_t World::rank_of(const Person& person) const {
  return static_cast<std::uint32_t>(std::count_if(
      life_.realms.begin(), life_.realms.end(), [&person](const Realm& realm) {
        return person.qi_capacity >= realm.qi;
      }));
}

std::uint64_t World::agreement(const Person& person, std::size_t art,
                               bool potency) const {
  if (person.known.empty()) {
    return PER_MILLE;
  }
  const Nature& heart = life_.arts.at(person.known.front().art).nature;
  const Nature& nature = life_.arts.at(art).nature;
  const Learning& learning = life_.learning;
  std::uint64_t agreed = PER_MILLE;
  if (heart.orientation != nature.orientation) {
    agreed =
        agreed *
        (potency ? learning.orientation_potency : learning.orientation_ease) /
        PER_MILLE;
  }
  if (heart.polarity != nature.polarity) {
    agreed = agreed *
             (potency ? learning.polarity_potency : learning.polarity_ease) /
             PER_MILLE;
  }
  return agreed;
}

// The first skill of art `art` person `person` lacks, of rank `rank` or below,
// that it may learn `how`.
std::optional<std::size_t> World::lesson(const Person& person, std::size_t art,
                                         Lesson how, std::uint32_t rank) const {
  const std::vector<ArtSkill>& skills = life_.arts.at(art).skills;
  for (std::size_t skill = 0; skill < skills.size(); ++skill) {
    const bool known = std::any_of(
        person.known.begin(), person.known.end(), [&](const Known& held) {
          return held.art == art && held.skill == skill;
        });
    if (!known && skills.at(skill).rank <= rank &&
        (how == Lesson::TAUGHT || skills.at(skill).comprehensible)) {
      return skill;
    }
  }
  return std::nullopt;
}

// Whether the person at `index` of `crowd`, pulled along `pull`, has a
// living foe on the move, not `standing`, ahead of it along the pull within
// its reach.
bool World::held(std::size_t index, Step pull,
                 const std::vector<bool>& standing, const Crowd& crowd) const {
  bool engaged = false;
  if (pull.x == 0 && pull.y == 0) {
    return engaged;
  }
  const std::size_t member = crowd.people.at(index);
  const std::size_t army = crowd.armies.at(index);
  const Position from = crowd.hash.position(index);
  crowd.hash.visit_within(
      from, reach(people_.at(member), armies_.at(army)),
      [&](std::size_t other) {
        const Position there = crowd.hash.position(other);
        const std::int64_t ahead = ((std::int64_t{there.x} - from.x) * pull.x) +
                                   ((std::int64_t{there.y} - from.y) * pull.y);
        engaged = engaged || (ahead > 0 && !standing.at(other) &&
                              people_.at(crowd.people.at(other)).health > 0 &&
                              foes(member, army, crowd.people.at(other),
                                   crowd.armies.at(other)));
      });
  return engaged;
}

// The pull of the person at `index` of `crowd`, serving in `army`, engaged
// with a living foe within the rules' sight, inside its home village's claim
// unless its army marches on an objective beyond it or it has no home: none
// while a foe lies within its reach, so it stands and fights, and otherwise the
// step closing on the nearest, the first in the crowd's order among equals. The
// step is the one of the eight nearest the line to the foe, open as a flow
// field's is: into a passable cell, and on a diagonal only where both cells
// beside it are passable; an attack intent's chase whose diagonal is shut
// steps along an open axis instead. Nothing, so it keeps its objective, when a
// driver steers its army and it holds no intent, it reaches no farther than
// itself, no foe is in sight or no step is open.
std::optional<Step> World::closing(std::size_t index, const Army& army,
                                   const Crowd& crowd) const {
  const std::size_t member = crowd.people.at(index);
  const std::int64_t reached = reach(people_.at(member), army);
  const Position from = crowd.hash.position(index);
  const std::size_t own = crowd.armies.at(index);
  std::optional<std::size_t> foe;
  if (army.order.target) {
    // An attack intent closes on its target, whoever steers the army.
    const std::size_t aimed = crowd.slot.at(*army.order.target);
    foe = aimed == ABSENT ? std::nullopt : std::optional<std::size_t>{aimed};
  } else if (army.steered || rules_.sight == 0 || reached == 0) {
    return std::nullopt;
  } else {
    // At home, it defends its village's claim; on the march, it fights what
    // it meets.
    const std::optional<std::size_t> home = people_.at(member).home;
    const bool marching = !home || !claims(*home, army.order.objective);
    foe = crowd.hash.nearest(
        foe_kinds(member, armies_.at(own)), from, rules_.sight * CELL_SIZE,
        [&](std::size_t other) {
          return (marching ||
                  claims(*home, cell_of(crowd.hash.position(other)))) &&
                 foes(member, own, crowd.people.at(other),
                      crowd.armies.at(other));
        });
  }
  if (!foe) {
    return std::nullopt;
  }
  const Position there = crowd.hash.position(*foe);
  if (distance_squared(from, there) <= reached * reached) {
    return Step{0, 0};
  }
  const std::int64_t offset_x = std::int64_t{there.x} - from.x;
  const std::int64_t offset_y = std::int64_t{there.y} - from.y;
  // A component a twelfth over five of the other's or less is dropped, so
  // the step lies within an eighth of a turn of the line to the foe.
  constexpr std::int64_t SLOPE_RISE = 5;
  constexpr std::int64_t SLOPE_RUN = 12;
  const Step step{
      std::abs(offset_x) * SLOPE_RUN >= std::abs(offset_y) * SLOPE_RISE
          ? sign(offset_x)
          : 0,
      std::abs(offset_y) * SLOPE_RUN >= std::abs(offset_x) * SLOPE_RISE
          ? sign(offset_y)
          : 0};
  const Cell cell = cell_of(from);
  const auto open = [&](Step way) {
    return map_.passable(Cell{cell.x + way.x, cell.y + way.y}) &&
           map_.passable(Cell{cell.x + way.x, cell.y}) &&
           map_.passable(Cell{cell.x, cell.y + way.y});
  };
  if (open(step)) {
    return step;
  }
  if (!army.order.target || step.x == 0 || step.y == 0) {
    return std::nullopt;
  }
  // An attack intent's chase, its diagonal shut by a corner, takes the open
  // one of its two axes, the one along the longer offset first.
  const Step along_x{step.x, 0};
  const Step along_y{0, step.y};
  const bool x_first = std::abs(offset_x) >= std::abs(offset_y);
  for (const Step way :
       {x_first ? along_x : along_y, x_first ? along_y : along_x}) {
    if (open(way)) {
      return way;
    }
  }
  return std::nullopt;
}

// Whether cell `cell` lies within the growth's claim of village `village`'s
// centre, along each axis; with no growth a village claims its centre alone.
bool World::claims(std::size_t village, Cell cell) const {
  const Cell centre = seat(village);
  const std::int32_t claim = life_.growth ? life_.growth->claim : 0;
  return std::abs(cell.x - centre.x) <= claim &&
         std::abs(cell.y - centre.y) <= claim;
}

// The farthest person `person` of army `army` may strike on its own: the
// longest range of its order's skills and the art skills it knows.
std::int32_t World::reach(const Person& person, const Army& army) const {
  std::int32_t farthest = 0;
  for (const std::size_t skill : army.order.skills) {
    farthest = std::max(farthest, army.skills.at(skill).range);
  }
  for (const Known& known : person.known) {
    farthest = std::max(
        farthest, life_.arts.at(known.art).skills.at(known.skill).skill.range);
  }
  return farthest;
}

// Whether person `person` of army `army` strikes person `other` of army
// `other_army`: their factions are hostile, or the two fight a duel, whatever
// their factions.
bool World::foes(std::size_t person, std::size_t army, std::size_t other,
                 std::size_t other_army) const {
  return hostile(armies_.at(army).faction, armies_.at(other_army).faction) ||
         duelling(person, other);
}

// The kinds the crowd's index marks the people person `person`, serving in
// `army`, may strike by: those of the factions hostile to its army's, or every
// kind while it fights a duel, whose foe may be of any faction.
std::uint64_t World::foe_kinds(std::size_t person, const Army& army) const {
  const bool duellist =
      std::any_of(duels_.begin(), duels_.end(), [person](const Fight& fight) {
        return fight.challenger == person || fight.leader == person;
      });
  if (duellist) {
    return UINT64_LIMIT;
  }
  std::uint64_t kinds = 0;
  for (std::size_t faction = 0; faction < rules_.hostile.size(); ++faction) {
    if (hostile(army.faction, faction)) {
      kinds |= kind_of(faction);
    }
  }
  return kinds;
}

// Where a cast of `skill` by `caster` at `target` lands, or which way it
// goes.
Position World::landing(const Person& caster, const Skill& skill,
                        const Target& target) const {
  if (skill.shape == Shape::SELF) {
    return caster.position;
  }
  if (const auto* point = std::get_if<Point>(&target)) {
    return within_range(caster.position, point->position, skill.range);
  }
  if (const auto* aimed = std::get_if<AtPerson>(&target)) {
    return people_.at(aimed->person).position;
  }
  return caster.position;
}

// Why person `person`, serving in army `army` if it serves in one, may not
// make cast `cast` on tick `due`.
CastRefusal World::assess_cast(std::size_t person,
                               std::optional<std::size_t> army,
                               const Cast& cast, std::uint64_t due) const {
  const Person& caster = people_.at(person);
  if (caster.health == 0 || !army) {
    return CastRefusal::DEAD;
  }
  const Army& own = armies_.at(*army);
  if (cast.skill >= own.skills.size() + caster.known.size()) {
    return CastRefusal::NO_SKILL;
  }
  if (!ready(caster, cast.skill, own, due)) {
    return CastRefusal::ON_COOLDOWN;
  }
  const Skill& skill = usable(caster, own, cast.skill);
  if (caster.qi < skill.qi_cost) {
    return CastRefusal::NO_QI;
  }
  const auto* aimed = std::get_if<AtPerson>(&cast.target);
  if (aimed == nullptr && skill.shape == Shape::TARGETED) {
    return CastRefusal::NO_TARGET;
  }
  if (aimed != nullptr) {
    if (aimed->person >= people_.size() ||
        people_.at(aimed->person).health == 0) {
      return CastRefusal::NO_TARGET;
    }
    const std::int64_t range = skill.range;
    if (distance_squared(caster.position, people_.at(aimed->person).position) >
        range * range) {
      return CastRefusal::OUT_OF_RANGE;
    }
  }
  return CastRefusal::NONE;
}

std::optional<std::size_t> World::army_of(std::size_t person) const {
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    const std::vector<std::size_t>& roster = armies_.at(army).roster;
    if (std::find(roster.begin(), roster.end(), person) != roster.end()) {
      return army;
    }
  }
  return std::nullopt;
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
  if (held && (effect > 0) != hostile(striker.faction, owner(*home))) {
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
    record(seat(*home), Ruined{*home, owner(*home), striker});
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
  if (life_.cultivation) {
    const std::uint32_t growth = draw(range_of(life_.cultivation->growth));
    person.cultivation =
        Aptitude{growth, draw(range_of(life_.cultivation->limit))};
  }
  person.qi = 0;
  person.qi_capacity = 0;
  person.known.clear();
  person.silver = life_.trade ? life_.trade->purse : 0;
  person.held.assign(life_.trade ? life_.trade->wares.size() : 0, 0);
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
    const std::uint64_t days = draw(range_of(*life_.founding_age));
    person.born = -static_cast<std::int64_t>(days * rules_.ticks_per_day);
    // It cultivated every day it lived before the world began.
    person.qi_capacity = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        person.cultivation.limit, person.cultivation.growth * days));
    person.qi = person.qi_capacity;
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
  record(cell_of(people_.at(born).position), Born{born});
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

std::size_t World::skill_count(std::size_t person) const {
  const Person& caster = people_.at(person);
  const std::optional<std::size_t> army = army_of(person);
  return army ? armies_.at(*army).skills.size() + caster.known.size() : 0;
}

const Skill& World::skill_of(std::size_t person, std::size_t skill) const {
  return usable(people_.at(person), armies_.at(army_with(person, skill)),
                skill);
}

std::uint64_t World::ready_at(std::size_t person, std::size_t skill) const {
  const std::size_t drilled =
      armies_.at(army_with(person, skill)).skills.size();
  const Person& caster = people_.at(person);
  return skill < drilled ? caster.ready_at
                         : caster.known.at(skill - drilled).ready_at;
}

// The army person `person` serves in, when it has skill `skill`. Throws
// std::out_of_range when it has no such skill.
std::size_t World::army_with(std::size_t person, std::size_t skill) const {
  const std::optional<std::size_t> army = army_of(person);
  if (!army || skill >= armies_.at(*army).skills.size() +
                            people_.at(person).known.size()) {
    throw std::out_of_range("the person has no such skill");
  }
  return *army;
}

std::uint32_t World::rank(std::size_t person) const {
  return rank_of(people_.at(person));
}

std::optional<Nature> World::nature(std::size_t person) const {
  const Person& who = people_.at(person);
  return who.known.empty() ? std::nullopt
                           : std::optional<Nature>{
                                 life_.arts.at(who.known.front().art).nature};
}

const std::vector<Realm>& World::realms() const { return life_.realms; }

const std::vector<Art>& World::arts() const { return life_.arts; }

// What people's qi does on a tick: it recovers when due, manuals lying on a
// cell are read, and once a day stores grow.
void World::train() {
  if (life_.cultivation &&
      tick_count_ % life_.cultivation->recovery_ticks == 0) {
    recover(*life_.cultivation);
  }
  read_manuals();
  if (life_.cultivation && tick_count_ % rules_.ticks_per_day == 0) {
    cultivate();
  }
}

// Once a day, every living person's qi capacity grows by its cultivation's
// growth, up to its limit; each realm gained is noted, and on each it
// comprehends.
void World::cultivate() {
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      Person& person = people_.at(member);
      const std::uint32_t before = rank_of(person);
      person.qi_capacity =
          std::max(person.qi_capacity,
                   std::min(person.cultivation.limit,
                            person.qi_capacity + person.cultivation.growth));
      const std::uint32_t after = rank_of(person);
      for (std::uint32_t gained = before; gained < after; ++gained) {
        note(RealmGained{member, gained + 1});
        comprehend(member);
      }
    }
  }
}

// Person `learner` rolls for each art it knows, in the order it learned them,
// to comprehend a skill of it within its realm, noting each learned.
void World::comprehend(std::size_t learner) {
  Person& person = people_.at(learner);
  const std::uint32_t rank = rank_of(person);
  std::vector<std::size_t> practised;
  for (const Known& known : person.known) {
    if (std::find(practised.begin(), practised.end(), known.art) ==
        practised.end()) {
      practised.push_back(known.art);
    }
  }
  for (const std::size_t art : practised) {
    const std::optional<std::size_t> skill =
        lesson(person, art, Lesson::COMPREHENDED, rank);
    if (!skill) {
      continue;
    }
    const std::size_t discipline = life_.arts.at(art).discipline;
    const std::uint64_t mean =
        std::max<std::uint64_t>(life_.disciplines.at(discipline).limit.mean, 1);
    const std::uint64_t talent =
        person.talent.empty() ? mean : person.talent.at(discipline).limit;
    const std::uint64_t chance = life_.learning.comprehension * talent / mean *
                                 agreement(person, art, false) / PER_MILLE;
    if (next_random() % PER_MILLE < chance) {
      person.known.push_back(Known{art, *skill, 0});
      note(Learned{learner, art, *skill, LearnedBy::COMPREHENSION});
    }
  }
}

// Every living person's qi recovers its cultivation's recovery per mille of
// its capacity, rounded up, up to its capacity.
void World::recover(const Cultivation& cultivation) {
  const std::uint64_t recovery = cultivation.recovery;
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      Person& person = people_.at(member);
      const std::uint64_t regained =
          ((std::uint64_t{person.qi_capacity} * recovery) + PER_MILLE - 1) /
          PER_MILLE;
      person.qi = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(person.qi_capacity, person.qi + regained));
    }
  }
}

// Each manual lying on a cell is read by the nearest living person within
// the reading reach of the cell's centre, the lower index among equals, who
// has the rank for a skill of its art it lacks and the qi to read it: it
// spends the reading's cost and learns every such skill, each noted. The
// manual stays.
void World::read_manuals() {
  for (const Manual& manual : manuals_) {
    const auto* cell = std::get_if<Cell>(&manual.place);
    if (cell == nullptr) {
      continue;
    }
    const Position centre = centre_of(*cell);
    const std::int64_t reach = life_.learning.reading_reach;
    std::optional<std::pair<std::int64_t, std::size_t>> nearest;
    for (const Army& army : armies_) {
      for (const std::size_t member : army.roster) {
        const Person& reader = people_.at(member);
        const std::int64_t distance = distance_squared(centre, reader.position);
        const std::uint64_t ease = agreement(reader, manual.art, false);
        if (distance > reach * reach || ease == 0 ||
            !lesson(reader, manual.art, Lesson::TAUGHT, rank_of(reader)) ||
            std::uint64_t{reader.qi} * ease <
                std::uint64_t{life_.learning.reading_cost} * PER_MILLE) {
          continue;
        }
        const std::pair<std::int64_t, std::size_t> key{distance, member};
        if (!nearest || key < *nearest) {
          nearest = key;
        }
      }
    }
    if (!nearest) {
      continue;
    }
    Person& reader = people_.at(nearest->second);
    reader.qi -= static_cast<std::uint32_t>(
        std::uint64_t{life_.learning.reading_cost} * PER_MILLE /
        agreement(reader, manual.art, false));
    const std::uint32_t rank = rank_of(reader);
    while (const std::optional<std::size_t> skill =
               lesson(reader, manual.art, Lesson::TAUGHT, rank)) {
      reader.known.push_back(Known{manual.art, *skill, 0});
      note(Learned{nearest->second, manual.art, *skill, LearnedBy::MANUAL});
    }
  }
}

// Once a day, each living member of a standing sect whose seat is held
// learns, with the teaching's chance a year for each rank of its sect's
// highest-ranked elder, the first skill it lacks of the first art its sect
// holds a manual of that has one within both their ranks, noted.
void World::teach() {
  for (std::size_t sect = 0; sect < sects_.size(); ++sect) {
    if (!seat_held(sect)) {
      continue;
    }
    std::uint32_t teacher = 0;
    for (const std::size_t elder : sect_elders(sect)) {
      teacher = std::max(teacher, rank_of(people_.at(elder)));
    }
    if (teacher == 0) {
      continue;
    }
    for (const std::size_t member : sect_members(sect)) {
      Person& pupil = people_.at(member);
      const std::uint32_t rank = std::min(teacher, rank_of(pupil));
      for (const Manual& manual : manuals_) {
        const auto* held = std::get_if<HeldBySect>(&manual.place);
        if (held == nullptr || held->sect != sect) {
          continue;
        }
        const std::optional<std::size_t> skill =
            lesson(pupil, manual.art, Lesson::TAUGHT, rank);
        if (!skill) {
          continue;
        }
        const std::uint64_t chance =
            std::uint64_t{life_.learning.teaching} * teacher *
            agreement(pupil, manual.art, false) / PER_MILLE;
        if (next_random() % (std::uint64_t{PER_MILLE} * DAYS_PER_YEAR) <
            chance) {
          pupil.known.push_back(Known{manual.art, *skill, 0});
          note(Learned{member, manual.art, *skill, LearnedBy::TEACHING});
        }
        break;
      }
    }
  }
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

Date World::date() const { return date_at(tick_count_); }

Date World::date_at(std::uint64_t tick) const {
  const std::uint64_t day = tick / rules_.ticks_per_day;
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
    if (!may_bear(mother, kinship) || living() >= rules_.people_capacity) {
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

// The sex fewer among the living unmarried whose home is village `village`
// and who are not past marrying age, children included; none when as many
// men as women.
std::optional<Sex> World::scarcer_sex(std::size_t village,
                                      const Kinship& kinship) const {
  std::int64_t men_over_women = 0;
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      const Person& person = people_.at(member);
      const bool wed = !person.spouses.empty() &&
                       people_.at(person.spouses.back()).health > 0;
      if (person.home != village || wed ||
          age_in_days(person) > kinship.marrying_age.high) {
        continue;
      }
      men_over_women += person.sex == Sex::MALE ? 1 : -1;
    }
  }
  if (men_over_women == 0) {
    return std::nullopt;
  }
  return men_over_women > 0 ? Sex::FEMALE : Sex::MALE;
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
// family, home and army of the parent the lineage follows, at its side; with
// balanced sexes, of the sex scarcer among its home's unmarried.
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
  bear(child, Soul{},
       kinship.balanced_sexes && child.home ? scarcer_sex(*child.home, kinship)
                                            : std::nullopt);
  const std::size_t born = people_.size();
  people_.at(father).children.push_back(born);
  people_.at(mother).children.push_back(born);
  people_.push_back(std::move(child));
  record(cell_of(people_.at(born).position), Born{born});
  army_of.push_back(army_of.at(lineage));
  armies_.at(army_of.at(lineage)).roster.push_back(born);
}

// What village `village`'s structures no worse than `worst` hold together,
// the greatest edge and healing among them, and the harvest of its intact
// fields.
Holding World::held(std::size_t village, Stage worst) const {
  const VillageSpec& spec = villages_.at(village).spec;
  Holding total;
  for (std::size_t index = 0; index < spec.holdings.size(); ++index) {
    const Holding& holding = spec.holdings.at(index);
    const Stage now = stage(spec.structures.at(index));
    if (now <= worst) {
      total.housing += holding.housing;
      total.storage += holding.storage;
      total.pupils += holding.pupils;
      total.arms += holding.arms;
      total.tends += holding.tends;
      total.edge = std::max(total.edge, holding.edge);
      total.healing = std::max(total.healing, holding.healing);
    }
    total.harvest += now == Stage::INTACT ? holding.harvest : 0;
  }
  return total;
}

std::size_t World::cap(std::size_t village) const {
  if (!life_.husbandry) {
    return rules_.people_capacity;
  }
  const Holding standing = held(village, Stage::DAMAGED);
  const std::uint64_t yearly = std::max<std::uint64_t>(
      std::uint64_t{life_.husbandry->consumption} * DAYS_PER_YEAR, 1);
  return static_cast<std::size_t>(
      std::min({std::uint64_t{standing.housing}, standing.harvest / yearly,
                standing.storage / yearly}));
}

// Each village's edge and healing from its standing structures, each kept
// only while the growth's self-reliance and what they arm, or tend, cover the
// people whose home it is.
World::Service World::served() const {
  if (villages_.empty()) {
    return {};
  }
  const std::vector<std::size_t> population = homes();
  const std::uint64_t reliance = life_.growth ? life_.growth->self_reliance : 0;
  Service service{std::vector<std::uint32_t>(villages_.size(), 0),
                  std::vector<std::uint32_t>(villages_.size(), 0)};
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const Holding standing = held(village, Stage::DAMAGED);
    const std::uint64_t people = population.at(village);
    if (reliance + standing.arms >= people) {
      service.edge.at(village) = standing.edge;
    }
    if (reliance + standing.tends >= people) {
      service.healing.at(village) = standing.healing;
    }
  }
  return service;
}

// Once a day, every living person whose home's workshops tend it recovers
// their healing, up to a newborn's health; no one's health is lowered.
void World::tend() {
  const std::vector<std::uint32_t> healing = served().healing;
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      Person& person = people_.at(member);
      if (person.health == 0 || !person.home ||
          person.health >= life_.newborn_health) {
        continue;
      }
      person.health = std::min<std::uint64_t>(
          person.health + healing.at(*person.home), life_.newborn_health);
    }
  }
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
      !hostile(out.faction, owner(target));
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

// Cells of the map: those a structure or a village's clearing covers, those
// not buildable, and those an army's objective or a living person takes.
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
  for (const Village& village : villages_) {
    for (const Clearing& clearing : village.spec.clearings) {
      for (const Cell cell : cells_of(clearing.corner, clearing.area)) {
        if (cell.x >= 0 && cell.y >= 0 && cell.x < map_.width() &&
            cell.y < map_.height()) {
          marked.covered.at(slot(cell)) = true;
        }
      }
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
// a cell clear of any structure, village's clearing or unbuildable cell.
// Returns whether it laid one.
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
  // Housing, food, storage, arms and tends, each in the people it holds,
  // the last two beyond the `reliance` a village holds with no workshop.
  constexpr std::size_t HOLDINGS = 5;
  const auto in_people = [yearly](const Holding& holding,
                                  std::uint64_t reliance) {
    return std::array<std::uint64_t, HOLDINGS>{
        holding.housing, holding.harvest / yearly, holding.storage / yearly,
        reliance + holding.arms, reliance + holding.tends};
  };
  const std::array<std::uint64_t, HOLDINGS> holds =
      in_people(held(village, Stage::DAMAGED), growth.self_reliance);
  const auto scarcest = static_cast<std::size_t>(
      std::min_element(holds.begin(), holds.end()) - holds.begin());
  std::optional<std::size_t> chosen;
  std::uint64_t most = 0;
  for (const std::size_t plan : growth.builds) {
    const std::uint64_t gain =
        in_people(growth.plans.at(plan).holding, 0).at(scarcest);
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
// of any structure or village's clearing and held by no other faction, and
// sends the settlers there as the new village's residents; their homes stay
// until it can hold them. The founder heads a new family, a branch bearing
// its family's surname, and so do its children among the settlers. Returns
// whether it founded one.
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
  villages_.push_back(Village{VillageSpec{{}, army, {}, 0, {}}, std::nullopt, 0,
                              std::nullopt, std::nullopt, corner});
  attach(villages_.size() - 1, *laid, growth);
  record(centre, Founded{villages_.size() - 1, faction});
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
    if (other != village && hostile(faction, owner(other)) &&
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
  record(seat(*target), Raided{*target, owner(*target), faction});
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
