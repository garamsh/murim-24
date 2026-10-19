#include "world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

#include "areas.hpp"
#include "arts.hpp"
#include "attack.hpp"
#include "calendar.hpp"
#include "chart.hpp"
#include "coarse_graph.hpp"
#include "cultivation.hpp"
#include "diplomacy.hpp"
#include "event.hpp"
#include "flow_field.hpp"
#include "gear.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "layout.hpp"
#include "leadership.hpp"
#include "life.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "names.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "schedule.hpp"
#include "sect.hpp"
#include "skill.hpp"
#include "spatial_hash.hpp"
#include "talent.hpp"
#include "talk.hpp"
#include "trade.hpp"
#include "village.hpp"
#include "village_fields.hpp"

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
// The streams a person's errand draws from (ADR 0081): its wander phase, its
// spot at a place, and its spot within a cell of that in a wander period.
constexpr std::uint64_t PHASE_STREAM = 1;
constexpr std::uint64_t SPOT_STREAM = 2;
constexpr std::uint64_t WANDER_STREAM = 3;

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

// `map` with the footprints block_footprints blocks, so the coarse graph is
// drawn once, over the map they leave.
Map footprinted(Map map, const std::vector<Structure>& structures) {
  static_cast<void>(block_footprints(map, structures));
  return map;
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

// The integer square root of `value`, rounded down, and 0 for a value of 0
// or less.
std::int64_t root(std::int64_t value) {
  if (value <= 0) {
    return 0;
  }
  // The floating-point root is only a guess: it is corrected to the exact
  // floor in whole numbers, so every platform returns the same root.
  auto guess = static_cast<std::int64_t>(std::sqrt(static_cast<double>(value)));
  while (guess > 0 && guess > value / guess) {
    --guess;
  }
  while (guess + 1 <= value / (guess + 1)) {
    ++guess;
  }
  return guess;
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

// The cells `ring` cells from `centre` along the farther axis, row by row.
std::vector<Cell> ring_round(Cell centre, std::int32_t ring) {
  std::vector<Cell> cells;
  for (Cell cell{0, centre.y - ring}; cell.y <= centre.y + ring; ++cell.y) {
    for (cell.x = centre.x - ring; cell.x <= centre.x + ring; ++cell.x) {
      if (std::max(std::abs(cell.x - centre.x), std::abs(cell.y - centre.y)) ==
          ring) {
        cells.push_back(cell);
      }
    }
  }
  return cells;
}

// Folds where far army anchor `far` stands, or that the army is not far.
template <typename Fold>
void fold_far(const Fold& fold, const FarAnchor* far) {
  fold(far != nullptr);
  if (far != nullptr) {
    fold(far->area);
    fold(far->cell.x);
    fold(far->cell.y);
    fold(far->progress);
  }
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
  if (rules.map_sight != 0) {
    fold(rules.map_sight);
  }
}

// Folds soul `soul`'s chart, only while it holds something, so a world where
// no one has seen anything keeps its hash.
template <typename Fold>
void fold_chart(const Fold& fold, std::size_t soul, const Chart& chart) {
  if (chart.empty()) {
    return;
  }
  fold(soul);
  for (const ChartBlock& block : chart.blocks()) {
    fold(block.area);
    for (const std::uint8_t bits : block.bits) {
      fold(bits);
    }
  }
  const std::vector<std::optional<VillageMemory>>& villages = chart.villages();
  fold(villages.size());
  for (const std::optional<VillageMemory>& memory : villages) {
    fold(memory.has_value());
    if (memory) {
      fold(memory->owner);
      fold(memory->peopled);
      fold(memory->centre.x);
      fold(memory->centre.y);
      fold(static_cast<std::uint8_t>(memory->stage));
    }
  }
}

// Folds the attack tokens' rules, when set, and what each engaged person
// holds, so a world with neither keeps its hash.
template <typename Fold>
void fold_attacks(const Fold& fold, const std::optional<Tokens>& tokens,
                  const Attacks& attacks) {
  if (tokens) {
    fold(tokens->by_rank.size());
    for (const std::uint32_t granted : tokens->by_rank) {
      fold(granted);
    }
    fold(tokens->circling);
    fold(tokens->lease);
  }
  if (!attacks.any()) {
    return;
  }
  const std::vector<Engaged> engaged = attacks.engaged();
  fold(engaged.size());
  for (const Engaged& each : engaged) {
    const Engagement& held = each.engagement;
    fold(each.person);
    fold(held.foe);
    fold(static_cast<std::uint8_t>(held.ring));
    fold(held.slot.has_value());
    fold(held.slot.value_or(0));
    fold(held.token);
    fold(held.waiting_since);
    fold(held.token_since);
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
  // Folded for everyone, none included, whatever the packs hold (ADR 0081).
  fold(person.occupation.has_value());
  fold(person.occupation.value_or(0));
  // A slot for each of the gear's, so a world without gear hashes as before.
  for (const std::optional<std::size_t>& kind : person.worn) {
    fold(kind.has_value());
    fold(kind.value_or(0));
  }
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

// How many of the open sites drawn a founding tries to lay its core on, in
// turn from the one drawn, before it founds nothing at that check.
constexpr std::size_t FOUNDING_TRIES = 16;

// Whether cell `one` comes before cell `other` row by row, west to east.
bool row_major(Cell one, Cell other) {
  return one.y < other.y || (one.y == other.y && one.x < other.x);
}

// Whether `there` lies within `reach` cells of `here` along each axis.
bool within(Cell here, Cell there, std::int32_t reach) {
  return std::abs(there.x - here.x) <= reach &&
         std::abs(there.y - here.y) <= reach;
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

// Whether `person` wears other than a slot for each of `life`'s gear's, or a
// kind the gear lacks or one not worn in the slot it fills.
bool stray_worn(const Person& person, const Life& life) {
  static const Gear none{};
  const Gear& gear = life.gear ? *life.gear : none;
  if (person.worn.size() != gear.slots.size()) {
    return true;
  }
  for (std::size_t slot = 0; slot < gear.slots.size(); ++slot) {
    const std::optional<std::size_t> kind = person.worn.at(slot);
    if (kind &&
        (*kind >= gear.kinds.size() || gear.kinds.at(*kind).slot != slot)) {
      return true;
    }
  }
  return false;
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

// Folds `text` into `fold`: its length, then each byte.
template <typename Fold>
void fold_text(const Fold& fold, const std::string& text) {
  fold(text.size());
  for (const char each : text) {
    fold(static_cast<unsigned char>(each));
  }
}

template <typename Fold>
void fold_bands(const Fold& fold, const std::vector<Band>& schedule) {
  fold(schedule.size());
  for (const Band& band : schedule) {
    fold(band.from);
    fold(band.where);
  }
}

// Folds the occupations, or their absence, whatever the packs hold (ADR 0049).
template <typename Fold>
void fold_occupations(const Fold& fold,
                      const std::optional<Occupations>& occupations) {
  fold(occupations.has_value());
  if (!occupations) {
    return;
  }
  fold(occupations->list.size());
  for (const Occupation& occupation : occupations->list) {
    fold_text(fold, occupation.name);
    fold(occupation.openings.index());
    if (const auto* workplace =
            std::get_if<AtWorkplace>(&occupation.openings)) {
      fold_text(fold, workplace->kind);
      fold(workplace->per);
    } else if (const auto* square =
                   std::get_if<OnSquare>(&occupation.openings)) {
      fold(square->per_residents);
    }
    fold_bands(fold, occupation.schedule);
    fold(occupation.wares.size());
    for (const std::size_t ware : occupation.wares) {
      fold(ware);
    }
  }
  fold(occupations->working_age);
  fold(occupations->fallback);
  fold_bands(fold, occupations->idle);
  fold(occupations->waking.from);
  fold(occupations->waking.to);
}

// Folds the gear's data into `fold`, only when given, so a world without it
// keeps its hash.
template <typename Fold>
void fold_gear(const Fold& fold, const std::optional<Gear>& gear) {
  if (!gear) {
    return;
  }
  fold(gear->slots.size());
  for (const std::string& slot : gear->slots) {
    fold_text(fold, slot);
  }
  fold(gear->striking);
  fold(gear->families.size());
  for (const Family& family : gear->families) {
    fold_text(fold, family.name);
    fold(family.strike.has_value());
    fold_skill(fold, family.strike.value_or(Skill{}));
  }
  fold(gear->bare);
  fold(gear->needs.size());
  for (const std::vector<std::size_t>& needed : gear->needs) {
    fold(needed.size());
    for (const std::size_t family : needed) {
      fold(family);
    }
  }
  fold(gear->kinds.size());
  for (const ItemKind& kind : gear->kinds) {
    fold_text(fold, kind.name);
    fold(kind.slot);
    fold(kind.family);
    fold(kind.tier);
    fold(kind.damage.has_value());
    fold(kind.damage.value_or(0));
    fold(kind.edge);
    fold(kind.guard);
  }
  fold(gear->loadouts.size());
  for (const Loadout& loadout : gear->loadouts) {
    fold(loadout.faction.has_value());
    fold(loadout.faction.value_or(0));
    fold(loadout.founders);
    fold(loadout.soldiers);
    fold(loadout.slots.size());
    for (const std::vector<Choice>& choices : loadout.slots) {
      fold(choices.size());
      for (const Choice& choice : choices) {
        fold(choice.family.has_value());
        fold(choice.family.value_or(0));
        fold(choice.weight);
        fold(choice.art);
      }
    }
  }
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
  fold(village.tier);
  fold(village.centre.x);
  fold(village.centre.y);
  for (const Holding& holding : village.spec.holdings) {
    fold(holding.housing);
    fold(holding.storage);
    fold(holding.harvest);
    fold(holding.arms);
    fold(holding.tends);
    fold(holding.healing);
  }
  fold(village.spec.kinds.size());
  for (const std::string& kind : village.spec.kinds) {
    fold_text(fold, kind);
  }
}

}  // namespace

World::World(Seed seed, Map map, std::vector<ArmySpec> armies,
             std::vector<Structure> structures, Rules rules,
             std::vector<VillageSpec> villages,
             std::vector<Leadership> leadership, Life life,
             std::vector<SectSpec> sects)
    : rng_state_(static_cast<std::uint64_t>(seed)),
      seed_(static_cast<std::uint64_t>(seed)),
      map_(footprinted(map, structures)),
      graph_(map_),
      rules_(std::move(rules)),
      areas_(Footprint{map_.width(), map_.height()}),
      structures_(std::move(structures)),
      structure_index_(centres_of(structures_)),
      life_(std::move(life)) {
  check_life(life_);
  if (life_.gear) {
    check_gear(*life_.gear, life_.disciplines, rules_.hostile.size());
  }
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
    // With gear its people strike with their weapons, so an army may have
    // no skills of its own.
    return (army.skills.empty() && !life_.gear) || army.faction >= factions ||
           !map_.passable(army.objective) ||
           std::any_of(army.skills.begin(), army.skills.end(), invalid_skill) ||
           std::any_of(army.members.begin(), army.members.end(),
                       invalid_recruit);
  };
  // Footprints blocked their cells before any recruit is checked against
  // them, and before the coarse graph was drawn over the map they leave.
  const bool placed = block_footprints(map, structures_);
  std::size_t recruits = 0;
  for (const ArmySpec& army : armies) {
    recruits += army.members.size();
  }
  if (!reachable(rules_.separation) || !reachable(rules_.rumour_reach) ||
      !reachable(rules_.talk_reach) || !reachable(rules_.sight) ||
      !reachable(rules_.map_sight) || rules_.people_capacity > PEOPLE_CEILING ||
      recruits > rules_.people_capacity || !valid(rules_.tokens) ||
      std::any_of(rules_.hostile.begin(), rules_.hostile.end(), ragged) ||
      !placed || std::any_of(armies.begin(), armies.end(), invalid_army)) {
    throw std::invalid_argument(
        "separation, rumour and talk reach, sight, range and radius must lie "
        "in [0, MAX_REACH], hostility must be square, attack tokens must be "
        "valid, the world must hold at most its rules' capacity of people, "
        "itself at most PEOPLE_CEILING, each with health up to MAX_HEALTH on "
        "a passable cell, every army must "
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
  std::vector<bool> residents(armies.size(), false);
  for (const VillageSpec& spec : villages) {
    if (spec.residents < armies.size()) {
      paired.at(spec.residents) = std::size_t{2} * spec.households;
      residents.at(spec.residents) = true;
    }
  }
  std::vector<std::size_t> sect_factions;
  sect_factions.reserve(sects.size());
  for (const SectSpec& sect : sects) {
    sect_factions.push_back(sect.faction);
  }
  arm_factions(sect_factions);
  // Whether each person founds a sect, by person.
  std::vector<bool> founders;
  for (const SectSpec& sect : sects) {
    for (const Founder& founder : sect.founders) {
      founders.resize(std::max(founders.size(), founder.person + 1), false);
      founders.at(founder.person) = true;
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
    const std::size_t index = armies_.size();
    enlist(army, paired.at(index), residents.at(index), founders, families);
  }
  attacks_ = Attacks{people_.size()};
  // Each family, one for every army the world starts with and every head of
  // a household, takes a surname.
  for (std::size_t family = 0;
       family < families && !life_.names.surnames.empty(); ++family) {
    surnames_.push_back(next_random() % life_.names.surnames.size());
  }
  villages_ = Villages{structures_.size()};
  plans_.resize(structures_.size());
  if (life_.growth && life_.growth->stances.size() > rules_.hostile.size()) {
    throw std::invalid_argument(
        "growth must give stances for no more factions than hostility names");
  }
  for (VillageSpec& spec : villages) {
    add_village(std::move(spec));
  }
  leaders_ = Leaders{std::move(leadership), rules_.hostile.size(), people_};
  declarations_ = Declarations{rules_.hostile.size()};
  add_sects(std::move(sects));
  // The first day's assignment, so a world has its occupations from tick 0.
  assign_occupations();
  prepare_grounds();
}

World::World(Map map, Rules rules, Life life)
    : rng_state_(0),
      seed_(0),
      map_(std::move(map)),
      graph_(map_),
      rules_(std::move(rules)),
      areas_(Footprint{map_.width(), map_.height()}),
      structure_index_(std::vector<Position>{}),
      life_(std::move(life)) {}

// Whether village `village` holds a tier its growth lacks, or a centre off
// the map.
bool World::unplaced(const VillageRecord& village) const {
  const std::size_t tiers = life_.growth ? life_.growth->tiers.size() : 1;
  const Cell centre = village.centre;
  return village.tier >= tiers || centre.x < 0 || centre.y < 0 ||
         centre.x >= map_.width() || centre.y >= map_.height();
}

void World::restore() {
  check_life(life_);
  const std::size_t factions = rules_.hostile.size();
  if (life_.gear) {
    check_gear(*life_.gear, life_.disciplines, factions);
  }
  std::vector<std::size_t> sect_factions;
  sect_factions.reserve(sects_.size());
  for (const Sect& sect : sects_) {
    sect_factions.push_back(sect.faction);
  }
  arm_factions(sect_factions);
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
      !reachable(rules_.sight) || !reachable(rules_.map_sight) ||
      rules_.people_capacity > PEOPLE_CEILING || !valid(rules_.tokens) ||
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
           stray_cultivation(person, life_) || stray_purse(person, life_) ||
           stray_worn(person, life_) ||
           !below(person.occupation,
                  life_.occupations ? life_.occupations->list.size() : 0);
  };
  const std::size_t plans = life_.growth ? life_.growth->plans.size() : 0;
  const auto unplanned = [plans](std::optional<std::size_t> plan) {
    return !below(plan, plans);
  };
  const bool bad_structures =
      std::any_of(structures_.begin(), structures_.end(), off_map) ||
      plans_.size() != structures ||
      std::any_of(plans_.begin(), plans_.end(), unplanned);
  const auto stray_sect = [&](const Sect& sect) {
    return sect.faction >= factions || sect.seat >= villages;
  };
  const auto stray_manual = [&](const Manual& manual) {
    const auto* held = std::get_if<HeldBySect>(&manual.place);
    return manual.art >= life_.arts.size() ||
           (held != nullptr && held->sect >= sects_.size());
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
  if (bad_rules || bad_life || bad_structures ||
      !declarations_.kept(tick_count_) || !events_kept() || !progress_kept() ||
      !rostered() || living() > rules_.people_capacity ||
      std::any_of(people_.begin(), people_.end(), stray_person) ||
      !villages_.kept(structures_, armies_.size(), factions) ||
      !leaders_.kept(factions, people_) ||
      std::any_of(
          villages_.records().begin(), villages_.records().end(),
          [this](const VillageRecord& village) { return unplaced(village); }) ||
      std::any_of(sects_.begin(), sects_.end(), stray_sect) ||
      std::any_of(manuals_.begin(), manuals_.end(), stray_manual) ||
      std::any_of(souls_.begin(), souls_.end(), stray_soul)) {
    throw std::invalid_argument(
        "a restored world must keep the limits a world is created within, "
        "every reference must name a record it holds, and every living "
        "person must stand on exactly one roster");
  }
  index_rosters();
  structure_extent_ = 0;
  for (const Structure& structure : structures_) {
    structure_extent_ =
        std::max({structure_extent_, structure.footprint.width * CELL_SIZE,
                  structure.footprint.height * CELL_SIZE});
  }
  structure_index_ = SpatialHash{centres_of(structures_)};
  prepare_grounds();
}

// Whether the events kept are as tick() keeps them: at most
// EVENT_CAPACITY, oldest first, numbered one after another, none later than
// the world's tick, each naming records the world holds.
bool World::events_kept() const {
  const Records held{rules_.hostile.size(), people_.size(), villages_.size(),
                     armies_.size(), sects_.size()};
  return events_.kept(tick_count_, [&held](const Event& event) {
    return names_held(event.what, held);
  });
}

// Whether every army's faction, skills, order and roster name what the world
// holds, and every living person stands on exactly one roster and no dead
// person on any.
bool World::rostered() const {
  const std::size_t people = people_.size();
  std::vector<std::size_t> rosters(people, 0);
  for (const Army& army : armies_) {
    if (army.faction >= rules_.hostile.size() || drilled_skills(army) == 0 ||
        std::any_of(army.skills.begin(), army.skills.end(), invalid_skill) ||
        army.order.skills.empty() ||
        !all_below(army.order.skills, drilled_skills(army)) ||
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
void World::enlist(ArmySpec& army, std::size_t pairs, bool residents,
                   const std::vector<bool>& founders, std::size_t& families) {
  std::vector<std::size_t> every_skill(army.skills.size() +
                                       (life_.gear ? 1 : 0));
  std::iota(every_skill.begin(), every_skill.end(), std::size_t{0});
  std::vector<std::size_t> members;
  for (const Recruit& recruit : army.members) {
    const bool housed = members.size() < pairs;
    const bool wife = housed && members.size() % 2 == 1;
    std::optional<Sex> sex;
    if (housed) {
      sex = wife ? Sex::FEMALE : Sex::MALE;
    }
    members.push_back(people_.size());
    Person person;
    person.position = centre_of(recruit.cell);
    person.health = recruit.health;
    person.home = army.home;
    person.family = housed ? families++ : armies_.size();
    const std::size_t index = people_.size();
    found(person,
          Calling{army.faction, index < founders.size() && founders.at(index),
                  !residents},
          sex);
    if (wife) {
      const std::size_t husband = people_.size() - 1;
      person.spouses.push_back(husband);
      people_.at(husband).spouses.push_back(people_.size());
    }
    people_.push_back(std::move(person));
  }
  reachable_from(army.objective, members);
  armies_.push_back(Army{army.faction,
                         std::move(army.skills),
                         Order{army.objective, std::move(every_skill)},
                         Bound{},
                         {},
                         false});
  for (const std::size_t member : members) {
    enrol(member, armies_.size() - 1);
  }
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
  if (std::any_of(leaders_.factions().begin(), leaders_.factions().end(),
                  bad_admission) ||
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
  Living living;
  for (const Army& army : armies_) {
    living.people.insert(living.people.end(), army.roster.begin(),
                         army.roster.end());
  }
  std::sort(living.people.begin(), living.people.end());
  return living;
}

void World::keep_sects() {
  const Living living = living_people();
  std::vector<std::size_t> bereft;
  for (const std::size_t member : living.people) {
    std::optional<std::size_t>& sect = people_.at(member).sect;
    if (sect &&
        armies_.at(army_serving(member)).faction != sects_.at(*sect).faction) {
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
  const std::optional<Admission>& admission = leaders_.of(*faction).admission;
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
    const std::size_t faction = armies_.at(army_serving(candidate)).faction;
    const std::optional<Admission>& admission = leaders_.of(faction).admission;
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
  events_.add(Event{0, tick_count_, cell, what});
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

const std::vector<Event>& World::events() const { return events_.entries(); }

const std::vector<Progress>& World::progress() const {
  return progress_.entries();
}

// Notes `what`, a step of progress on this tick, letting the oldest go once
// PROGRESS_CAPACITY are kept.
void World::note(Advance what) {
  progress_.add(Progress{0, tick_count_, what});
}

// Whether the progress kept is as note() keeps it: at most
// PROGRESS_CAPACITY, oldest first, numbered one after another, none later
// than the world's tick, each naming a person, art, skill or realm the world
// holds.
bool World::progress_kept() const {
  return progress_.kept(tick_count_, [this](const Progress& step) {
    return names_held(step.what, people_.size(), life_.arts, life_.realms);
  });
}

std::vector<Event> World::rumours(std::size_t person) const {
  if (person >= people_.size()) {
    throw std::out_of_range("no such person hears rumours");
  }
  return murim::rumours(
      events_.entries(),
      Listener{person, faction_of(person), rules_.rumour_reach}, people_);
}

std::optional<std::size_t> World::talk_target(std::size_t person) const {
  const Position here = people_.at(person).position;
  std::optional<std::size_t> found;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (const Army& army : armies_) {
    for (const std::size_t other : army.roster) {
      const TalkRefusal refusal = talk_refusal(person, other);
      if (refusal != TalkRefusal::NONE && refusal != TalkRefusal::HOSTILE) {
        continue;
      }
      const std::int64_t apart =
          distance_squared(here, people_.at(other).position);
      if (apart < nearest) {
        nearest = apart;
        found = other;
      }
    }
  }
  return found;
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

const std::vector<Occupation>& World::occupations() const {
  static const std::vector<Occupation> none;
  return life_.occupations ? life_.occupations->list : none;
}

bool World::sells(std::size_t person, std::size_t ware) const {
  if (person >= people_.size() || ware >= wares().size()) {
    throw std::out_of_range("no such person sells no such ware");
  }
  const Person& seller = people_.at(person);
  if (seller.health == 0 || !seller.occupation || !life_.occupations) {
    return false;
  }
  const std::vector<std::size_t>& sold =
      life_.occupations->list.at(*seller.occupation).wares;
  return std::find(sold.begin(), sold.end(), ware) != sold.end();
}

std::optional<std::size_t> World::occupation_of(std::size_t person) const {
  return people_.at(person).occupation;
}

// Fills each village's occupations' openings from the living people whose
// home it is, and takes the occupation of each living person with none.
void World::assign_occupations() {
  if (!life_.occupations) {
    return;
  }
  const Occupations& occupations = *life_.occupations;
  std::vector<std::vector<std::size_t>> homed(villages_.size());
  for (std::size_t person = 0; person < people_.size(); ++person) {
    Person& each = people_.at(person);
    if (each.health == 0) {
      continue;
    }
    if (each.home) {
      homed.at(*each.home).push_back(person);
    } else if (each.occupation) {
      each.occupation.reset();
      bump_serving(person);
    }
  }
  const Stages built = stages();
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const VillageSpec& spec = villages_.at(village).spec;
    std::vector<std::string> standing;
    for (std::size_t index = 0; index < spec.kinds.size(); ++index) {
      if (built.of(spec.structures.at(index)) <= Stage::DAMAGED) {
        standing.push_back(spec.kinds.at(index));
      }
    }
    std::vector<Candidate> candidates;
    candidates.reserve(homed.at(village).size());
    for (const std::size_t person : homed.at(village)) {
      candidates.push_back(Candidate{
          age_in_years(person) >= std::int64_t{occupations.working_age},
          people_.at(person).occupation});
    }
    const std::vector<std::optional<std::size_t>> held =
        assign(occupations, candidates, standing);
    for (std::size_t index = 0; index < held.size(); ++index) {
      const std::size_t person = homed.at(village).at(index);
      if (people_.at(person).occupation != held.at(index)) {
        people_.at(person).occupation = held.at(index);
        bump_serving(person);
      }
    }
  }
}

// Whether village `village`'s residents follow their schedules (ADR 0081):
// with occupations, while their army is not steered, not far, and bound for
// the village's centre with no attack intent.
bool World::follows(std::size_t village) const {
  const VillageRecord& record = villages_.at(village);
  const Army& army = armies_.at(record.spec.residents);
  return life_.occupations && !army.steered && !far_off(army) &&
         !army.order.target && army.order.objective.x == record.centre.x &&
         army.order.objective.y == record.centre.y;
}

std::uint64_t World::army_epoch(std::size_t army) const {
  return army < army_epochs_.size() ? army_epochs_.at(army) : 0;
}

void World::bump_army(std::size_t army) {
  if (army >= army_epochs_.size()) {
    army_epochs_.resize(army + 1, 0);
  }
  ++army_epochs_.at(army);
}

void World::bump_village(std::size_t village) {
  if (village >= village_epochs_.size()) {
    village_epochs_.resize(village + 1, 0);
  }
  ++village_epochs_.at(village);
}

// Marks for finding again the lodging of the army person `person` serves
// in, if any, as its household or occupation changes.
void World::bump_serving(std::size_t person) {
  if (const std::optional<std::size_t> army = army_of(person)) {
    bump_army(*army);
  }
}

std::uint64_t World::layout_epoch(std::size_t village) const {
  return village < layout_epochs_.size() ? layout_epochs_.at(village) : 0;
}

void World::bump_layout(std::size_t village) {
  if (village >= layout_epochs_.size()) {
    layout_epochs_.resize(village + 1, 0);
  }
  ++layout_epochs_.at(village);
}

bool World::current(const Grounds& grounds, std::size_t village) const {
  const VillageRecord& record = villages_.at(village);
  return grounds.version != 0 && grounds.tier == record.tier &&
         grounds.places == 1 + record.spec.structures.size() &&
         grounds.layout == layout_epoch(village);
}

bool World::current(const Lodging& lodging, std::size_t village) const {
  const std::size_t army = villages_.at(village).spec.residents;
  const std::uint64_t epoch =
      village < village_epochs_.size() ? village_epochs_.at(village) : 0;
  return lodging.version != 0 && lodging.army == army &&
         lodging.army_epoch == army_epoch(army) &&
         lodging.village_epoch == epoch;
}

// Village `village`'s grounds: the cells of the square its tier keeps that
// are passable and under no structure of it, each blocking structure's door,
// and a window over its centre, square and structures with
// VillageFields::MARGIN cells round them, holding no field yet.
World::Grounds World::grounds_of(std::size_t village) const {
  const VillageRecord& record = villages_.at(village);
  const VillageSpec& spec = record.spec;
  Grounds grounds;
  grounds.tier = record.tier;
  grounds.places = 1 + spec.structures.size();
  grounds.layout = layout_epoch(village);
  std::int32_t left = record.centre.x;
  std::int32_t top = record.centre.y;
  std::int32_t right = record.centre.x + 1;
  std::int32_t bottom = record.centre.y + 1;
  const auto cover = [&](Cell corner, Footprint size) {
    left = std::min(left, corner.x);
    top = std::min(top, corner.y);
    right = std::max(right, corner.x + size.width);
    bottom = std::max(bottom, corner.y + size.height);
  };
  const Clearing kept = square(village);
  cover(kept.corner, kept.area);
  const auto covered = [&](Cell cell) {
    return std::any_of(
        spec.structures.begin(), spec.structures.end(), [&](std::size_t index) {
          const Structure& structure = structures_.at(index);
          return cell.x >= structure.cell.x && cell.y >= structure.cell.y &&
                 cell.x < structure.cell.x + structure.footprint.width &&
                 cell.y < structure.cell.y + structure.footprint.height;
        });
  };
  for (const Cell cell : cells_of(kept.corner, kept.area)) {
    if (map_.passable(cell) && !covered(cell)) {
      grounds.square.push_back(cell);
    }
  }
  for (const std::size_t index : spec.structures) {
    const Structure& structure = structures_.at(index);
    cover(structure.cell, structure.footprint);
    grounds.doors.push_back(
        structure.blocks ? door(map_, structure, record.centre) : std::nullopt);
  }
  const Cell corner{std::max(0, left - VillageFields::MARGIN),
                    std::max(0, top - VillageFields::MARGIN)};
  const Footprint size{
      std::min(map_.width(), right + VillageFields::MARGIN) - corner.x,
      std::min(map_.height(), bottom + VillageFields::MARGIN) - corner.y};
  grounds.fields = VillageFields{map_, corner, size, grounds.places};
  return grounds;
}

// Village `village`'s lodging: where each of its residents lives, by
// homes_of(), and works, by works_of(), as its standing structures give.
World::Lodging World::lodging_of(std::size_t village) const {
  const VillageSpec& spec = villages_.at(village).spec;
  Lodging lodging;
  lodging.army = spec.residents;
  lodging.army_epoch = army_epoch(spec.residents);
  lodging.village_epoch =
      village < village_epochs_.size() ? village_epochs_.at(village) : 0;
  const Stages built = stages();
  std::vector<bool> standing;
  standing.reserve(spec.structures.size());
  for (const std::size_t structure : spec.structures) {
    standing.push_back(built.of(structure) <= Stage::DAMAGED);
  }
  lodging.homes = homes_of(village, standing);
  lodging.works = works_of(village, standing);
  return lodging;
}

// Where each of village `village`'s residents lives, by roster rank, of its
// structures standing by `standing`: its households take those that house
// people, in order, by house(); a resident whose household none holds has
// none.
std::vector<std::optional<std::size_t>> World::homes_of(
    std::size_t village, const std::vector<bool>& standing) const {
  const VillageSpec& spec = villages_.at(village).spec;
  const std::vector<std::size_t>& roster = armies_.at(spec.residents).roster;
  std::vector<std::size_t> houses;
  std::vector<std::uint32_t> housing;
  for (std::size_t index = 0; index < spec.holdings.size(); ++index) {
    if (standing.at(index) && spec.holdings.at(index).housing > 0) {
      houses.push_back(index);
      housing.push_back(spec.holdings.at(index).housing);
    }
  }
  std::vector<std::size_t> rank(people_.size(), ABSENT);
  for (std::size_t each = 0; each < roster.size(); ++each) {
    rank.at(roster.at(each)) = each;
  }
  const std::vector<std::vector<std::size_t>> found =
      murim::households(roster, people_);
  std::vector<std::size_t> sizes;
  sizes.reserve(found.size());
  for (const std::vector<std::size_t>& household : found) {
    sizes.push_back(household.size());
  }
  const std::vector<std::optional<std::size_t>> taken = house(sizes, housing);
  std::vector<std::optional<std::size_t>> homes(roster.size());
  for (std::size_t household = 0; household < found.size(); ++household) {
    if (const std::optional<std::size_t>& home = taken.at(household)) {
      for (const std::size_t member : found.at(household)) {
        homes.at(rank.at(member)) = 1 + houses.at(*home);
      }
    }
  }
  return homes;
}

// Where each of village `village`'s residents works, by roster rank, of its
// structures standing by `standing`: holders of an occupation working at a
// kind take its standing structures of that kind in order, each as many as
// it opens places for, in roster order; holders of one working on the square
// or opening no place work on the square; anyone else has none.
std::vector<std::optional<std::size_t>> World::works_of(
    std::size_t village, const std::vector<bool>& standing) const {
  const VillageSpec& spec = villages_.at(village).spec;
  const std::vector<std::size_t>& roster = armies_.at(spec.residents).roster;
  if (!life_.occupations) {
    throw std::logic_error("only a world with occupations has workplaces");
  }
  const std::vector<Occupation>& list = life_.occupations->list;
  std::vector<std::optional<std::size_t>> works(roster.size());
  std::vector<std::size_t> taken(list.size(), 0);
  for (std::size_t each = 0; each < roster.size(); ++each) {
    const std::optional<std::size_t> held =
        people_.at(roster.at(each)).occupation;
    if (!held) {
      continue;
    }
    const auto* workplace = std::get_if<AtWorkplace>(&list.at(*held).openings);
    if (workplace == nullptr) {
      works.at(each) = 0;
      continue;
    }
    // The holder's place among its occupation's, and the workplace it falls
    // to, `per` holders to each.
    std::size_t place = taken.at(*held)++ / workplace->per;
    for (std::size_t index = 0; index < spec.kinds.size(); ++index) {
      if (!standing.at(index) || spec.kinds.at(index) != workplace->kind) {
        continue;
      }
      if (place == 0) {
        works.at(each) = 1 + index;
        break;
      }
      --place;
    }
  }
  return works;
}

// The cells a walk to place `place` of village `village` ends on: the
// square's free cells, a blocking structure's door, or the passable cells of
// one that blocks nothing.
std::vector<Cell> World::seeds_of(std::size_t village, const Grounds& grounds,
                                  std::size_t place) const {
  if (place == 0) {
    return grounds.square;
  }
  const Structure& structure =
      structures_.at(villages_.at(village).spec.structures.at(place - 1));
  if (structure.blocks) {
    const std::optional<Cell> entry = grounds.doors.at(place - 1);
    return entry ? std::vector<Cell>{*entry} : std::vector<Cell>{};
  }
  std::vector<Cell> cells;
  for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
    if (map_.passable(cell)) {
      cells.push_back(cell);
    }
  }
  return cells;
}

// Person `person`'s errand, its residents' roster rank `rank` in village
// `village`: in the band of its occupation's schedule, or the idle one, in
// force at the hour its wander period began, it goes home to its house, to work
// at its workplace, or to the square, the square wherever it has no house or
// workplace. Its spot there is drawn from SPOT_STREAM among the place's
// seeds, or, at a door, the passable cells within a cell of it; awake, its
// spot moves every wander period, an hour of the day, from its own phase, to a
// passable cell within a cell of that, drawn from WANDER_STREAM.
World::Errand World::errand_of(const Follower& follower, const Grounds& grounds,
                               const Lodging& lodging) const {
  const std::size_t person = follower.person;
  const std::size_t village = follower.village;
  Errand errand = key_of(follower, grounds, lodging);
  // The passable cells within a cell of `cell`, row by row.
  const auto near = [this](Cell cell) {
    std::vector<Cell> cells;
    for (const Cell each : cells_of(Cell{cell.x - 1, cell.y - 1}, {3, 3})) {
      if (map_.passable(each)) {
        cells.push_back(each);
      }
    }
    return cells;
  };
  std::vector<Cell> spots = seeds_of(village, grounds, errand.place);
  const bool at_door =
      errand.place != 0 && grounds.doors.at(errand.place - 1).has_value();
  if (at_door) {
    spots = near(spots.front());
  }
  const Cell base =
      spots.empty()
          ? villages_.at(village).centre
          : spots.at(errand_draw(seed_, person, SPOT_STREAM, errand.place) %
                     spots.size());
  errand.spot = base;
  if (errand.period != 0) {
    const std::vector<Cell> wander = near(base);
    if (!wander.empty()) {
      errand.spot =
          wander.at(errand_draw(seed_, person, WANDER_STREAM, errand.period) %
                    wander.size());
    }
  }
  return errand;
}

// Person `person`'s errand in village `village` with only its key found: the
// kind of place of the band of its occupation's schedule, or the idle one, in
// force at the hour its current wander period began, the place of that kind
// its lodging gives it, and that wander period, none while it is asleep at
// home outside the waking hours.
World::Errand World::key_of(const Follower& follower, const Grounds& grounds,
                            const Lodging& lodging) const {
  const std::size_t person = follower.person;
  if (!life_.occupations) {
    throw std::logic_error("only a world with occupations has schedules");
  }
  const Occupations& occupations = *life_.occupations;
  const std::optional<std::size_t> held = people_.at(person).occupation;
  const std::vector<Band>& schedule =
      held ? occupations.list.at(*held).schedule : occupations.idle;
  // A person's whole key is read at the hour its current wander period began,
  // each period an hour of the day beginning at its own phase, so its errand
  // changes only at its own period's start and a village's errands change over
  // a period's ticks rather than all on the hour; the world's first period
  // counts from its first tick.
  const std::uint64_t ticks = wander_ticks(rules_.ticks_per_day);
  const std::uint64_t phase =
      errand_draw(seed_, person, PHASE_STREAM, 0) % ticks;
  const std::uint64_t into = (tick_count_ + phase) % ticks;
  const std::uint64_t began =
      tick_count_ >= into ? tick_count_ - into : tick_count_;
  const std::uint32_t hour = hour_of(began, rules_.ticks_per_day);
  Errand errand;
  errand.village = follower.village;
  errand.grounds = grounds.version;
  errand.where = schedule.at(band_at(schedule, hour)).where;
  switch (errand.where) {
    case Where::HOME:
      errand.place = lodging.homes.at(follower.rank).value_or(0);
      break;
    case Where::WORK:
      errand.place = lodging.works.at(follower.rank).value_or(0);
      break;
    case Where::SQUARE:
      errand.place = 0;
      break;
  }
  const bool asleep =
      errand.where == Where::HOME && !awake(occupations.waking, hour);
  errand.period = asleep ? 0 : ((tick_count_ + phase) / ticks) + 1;
  return errand;
}

// Whether two errands were found for the same grounds of the same village,
// the same kind of place and place, and the same wander period: all that
// errand_of() reads besides the person.
bool World::same_key(const Errand& one, const Errand& other) {
  return one.village == other.village && one.grounds == other.grounds &&
         one.where == other.where && one.place == other.place &&
         one.period == other.period;
}

// Finds village `village`'s grounds again and builds the field of every
// one of its places: they are pure, so a field built before it is needed
// steps as one built when it is.
void World::find_grounds(std::size_t village) {
  Grounds& grounds = grounds_.at(village);
  grounds = grounds_of(village);
  grounds.version = ++versions_;
  for (std::size_t place = 0; place < grounds.places; ++place) {
    build_field(village, grounds, place);
  }
  field_builds_ += grounds.places;
}

// Finds the grounds of every village not yet found or no longer current:
// as a world is created or loaded, and after each growth check, where every
// structure is laid and every tier reached, so that no other tick builds
// their fields.
void World::prepare_grounds() {
  if (!life_.occupations) {
    return;
  }
  grounds_.resize(villages_.size());
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    if (!current(grounds_.at(village), village)) {
      find_grounds(village);
    }
  }
}

// Finds again the grounds, lodging and errand of every village whose
// residents follow their schedules, where what each is derived from has
// changed.
void World::follow_schedules() {
  if (!life_.occupations) {
    return;
  }
  grounds_.resize(villages_.size());
  lodgings_.resize(villages_.size());
  errands_.resize(people_.size());
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    if (!follows(village)) {
      continue;
    }
    if (!current(grounds_.at(village), village)) {
      find_grounds(village);
    }
    const Grounds& grounds = grounds_.at(village);
    Lodging& lodging = lodgings_.at(village);
    if (!current(lodging, village)) {
      lodging = lodging_of(village);
      lodging.version = ++versions_;
      ++lodging_finds_;
    }
    const std::vector<std::size_t>& roster =
        armies_.at(villages_.at(village).spec.residents).roster;
    for (std::size_t rank = 0; rank < roster.size(); ++rank) {
      const Follower follower{village, rank, roster.at(rank)};
      Errand& errand = errands_.at(follower.person);
      if (!same_key(key_of(follower, grounds, lodging), errand)) {
        errand = errand_of(follower, grounds, lodging);
        ++errand_finds_;
      }
      errand.followed = tick_count_;
    }
  }
}

// Builds the field of place `place` of village `village` into `grounds`,
// toward the place's seeds, aimed at their middle.
void World::build_field(std::size_t village, Grounds& grounds,
                        std::size_t place) const {
  const std::vector<Cell> seeds = seeds_of(village, grounds, place);
  std::int64_t sum_x = 0;
  std::int64_t sum_y = 0;
  for (const Cell seed : seeds) {
    sum_x += seed.x;
    sum_y += seed.y;
  }
  const auto count =
      std::max<std::int64_t>(static_cast<std::int64_t>(seeds.size()), 1);
  grounds.fields.build(place, seeds,
                       Cell{static_cast<std::int32_t>(sum_x / count),
                            static_cast<std::int32_t>(sum_y / count)});
}

// The step a person following `errand` takes from `cell`: none on its spot;
// straight toward it within STRAIGHT_REACH cells along each axis, or where
// its place's field gives no step; by the field where it reaches; and none to
// give outside the window the field covers, where its army's own field
// carries it to the village.
std::optional<Step> World::errand_pull(const Errand& errand, Cell cell) const {
  const std::int64_t off_x = std::int64_t{errand.spot.x} - cell.x;
  const std::int64_t off_y = std::int64_t{errand.spot.y} - cell.y;
  if (off_x == 0 && off_y == 0) {
    return Step{0, 0};
  }
  const Step straight{sign(off_x), sign(off_y)};
  if (std::max(std::abs(off_x), std::abs(off_y)) <= STRAIGHT_REACH) {
    return straight;
  }
  const VillageFields& fields = grounds_.at(errand.village).fields;
  if (!fields.reaches(errand.place, cell)) {
    return std::nullopt;
  }
  const Step step = fields.step(errand.place, cell);
  return step.x == 0 && step.y == 0 ? straight : step;
}

Activity World::activity(std::size_t person) const {
  const Person& each = people_.at(person);
  const std::optional<std::size_t> army = army_of(person);
  if (each.health == 0 || !army) {
    return Activity::IDLE;
  }
  const std::vector<VillageRecord>& records = villages_.records();
  const auto housed = std::find_if(records.begin(), records.end(),
                                   [&army](const VillageRecord& record) {
                                     return record.spec.residents == *army;
                                   });
  if (housed == records.end()) {
    return Activity::IDLE;
  }
  const auto village = static_cast<std::size_t>(housed - records.begin());
  if (!follows(village)) {
    return Activity::IDLE;
  }
  const std::vector<std::size_t>& roster = armies_.at(*army).roster;
  const auto rank = static_cast<std::size_t>(
      std::find(roster.begin(), roster.end(), person) - roster.begin());
  Grounds fresh_grounds;
  const Grounds* grounds = nullptr;
  if (village < grounds_.size() && current(grounds_.at(village), village)) {
    grounds = &grounds_.at(village);
  } else {
    fresh_grounds = grounds_of(village);
    grounds = &fresh_grounds;
  }
  Lodging fresh_lodging;
  const Lodging* lodging = nullptr;
  if (village < lodgings_.size() && current(lodgings_.at(village), village)) {
    lodging = &lodgings_.at(village);
  } else {
    fresh_lodging = lodging_of(village);
    lodging = &fresh_lodging;
  }
  const Errand errand =
      errand_of(Follower{village, rank, person}, *grounds, *lodging);
  const Cell cell = cell_of(each.position);
  if (std::max(std::abs(errand.spot.x - cell.x),
               std::abs(errand.spot.y - cell.y)) > 1) {
    return Activity::WALKING;
  }
  if (errand.where == Where::WORK && lodging->works.at(rank) == errand.place) {
    return Activity::WORKING;
  }
  if (errand.where == Where::HOME && lodging->homes.at(rank) == errand.place) {
    return Activity::AT_HOME;
  }
  return Activity::IDLE;
}

World::FieldUse World::schedule_fields() const {
  FieldUse use;
  use.builds = field_builds_;
  use.lodgings = lodging_finds_;
  use.errands = errand_finds_;
  for (const Grounds& grounds : grounds_) {
    use.fields += grounds.fields.built();
    use.bytes += grounds.fields.bytes();
  }
  return use;
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
    sellers_.clear();
    for (std::size_t person = 0; person < people_.size(); ++person) {
      if (sells(person, ware)) {
        sellers_.push_back(
            Seller{cell_of(people_.at(person).position), person});
      }
    }
    std::sort(sellers_.begin(), sellers_.end(),
              [](const Seller& one, const Seller& other) {
                return row_major(one.cell, other.cell) ||
                       (!row_major(other.cell, one.cell) &&
                        one.person < other.person);
              });
    for (const Army& army : armies_) {
      if (army.steered) {
        continue;
      }
      for (const std::size_t buyer : army.roster) {
        if (people_.at(buyer).held.at(ware) > 0) {
          continue;
        }
        if (const std::optional<std::size_t> seller =
                first_seller(buyer, ware)) {
          buy(buyer, *seller, ware);
        }
      }
    }
  }
}

// The first of sellers_ by index that `buyer` may buy `ware` from. Only a
// seller within talk reach along each axis can sell, so it reads only the
// rows in reach.
std::optional<std::size_t> World::first_seller(std::size_t buyer,
                                               std::size_t ware) const {
  const auto before = [](const Seller& seller, Cell cell) {
    return row_major(seller.cell, cell);
  };
  const std::int32_t reach = rules_.talk_reach;
  const Cell here = cell_of(people_.at(buyer).position);
  std::optional<std::size_t> first;
  for (std::int32_t row = here.y - reach; row <= here.y + reach; ++row) {
    for (auto seller = std::lower_bound(sellers_.begin(), sellers_.end(),
                                        Cell{here.x - reach, row}, before);
         seller != sellers_.end() && seller->cell.y == row &&
         seller->cell.x <= here.x + reach;
         ++seller) {
      if ((!first || seller->person < *first) &&
          buy_refusal(buyer, seller->person, ware) == BuyRefusal::NONE) {
        first = seller->person;
      }
    }
  }
  return first;
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
  if (chosen && leaders_.lead(faction, chosen)) {
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
      leaders_.of(sects_.at(sect).faction).admission;
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
  return seat_held(sect)
             ? villages_.held(sects_.at(sect).seat, Stage::DAMAGED, stages())
                   .pupils
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
  const std::size_t residents = spec.residents;
  const std::size_t roster =
      residents < armies_.size() ? armies_.at(residents).roster.size() : 0;
  const std::size_t village =
      villages_.add(std::move(spec), armies_.size(), roster, stages());
  for (const std::size_t member : armies_.at(residents).roster) {
    people_.at(member).home = village;
  }
  villages_.place(village, armies_.at(residents).order.objective);
  if (life_.growth) {
    villages_.reach(Reached{village, tier_for(*life_.growth, roster)});
  }
}

// Throws when one of `members` stands where no walk reaches `objective`, as
// the coarse graph joins cells (ADR 0043).
void World::reachable_from(Cell objective,
                           const std::vector<std::size_t>& members) const {
  const auto cut_off = [this, objective](std::size_t member) {
    return !graph_.joined(
        map_, Walk{cell_of(people_.at(member).position), objective});
  };
  if (std::any_of(members.begin(), members.end(), cut_off)) {
    throw std::invalid_argument(
        "an army's objective must be reachable from where each member stands");
  }
}

void World::order(std::size_t army, Order order,
                  std::optional<std::size_t> challenge) {
  const auto lacking = [this, army](std::size_t skill) {
    return skill >= drilled_skills(armies_.at(army));
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
  reachable_from(order.objective, ordered.roster);
  if (challenge) {
    start_duel(army, *challenge);
  }
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
  }
}

// Starts a duel between army `army`'s lead and faction `faction`'s leader, or
// throws when the faction's succession refuses the challenge.
void World::start_duel(std::size_t army, std::size_t faction) {
  if (army >= armies_.size()) {
    throw std::invalid_argument("a challenge must come from an army");
  }
  leaders_.challenge(challenging(army), faction);
}

// Army `army` as a challenge reads it: its faction and its lead, if any.
Challenging World::challenging(std::size_t army) const {
  const Army& challenger = armies_.at(army);
  return Challenging{challenger.faction, challenger.roster.empty()
                                             ? std::nullopt
                                             : std::optional<std::size_t>{
                                                   challenger.roster.front()}};
}

ChallengeRefusal World::challenge_refusal(std::size_t army,
                                          std::size_t faction) const {
  if (army >= armies_.size()) {
    throw std::invalid_argument("a challenge must come from an army");
  }
  return leaders_.refusal(challenging(army), faction);
}

std::optional<Duellists> World::duel(std::size_t faction) const {
  return leaders_.duel(faction);
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
  if (army >= armies_.size() || !army_of(person)) {
    throw std::invalid_argument(
        "only a living person can be conscripted, into an existing army");
  }
  // A far army is reached from anywhere: coming back, it stands every member
  // round its anchor.
  if (!far_off(armies_.at(army))) {
    reachable_from(armies_.at(army).order.objective, {person});
  }
  strike_off(person);
  enrol(person, army);
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
  if (tick_count_ % TICKS_PER_SECOND == 0) {
    retier();
  }
  std::vector<SpatialHash::Placed> placed;
  std::vector<std::size_t> people;
  std::vector<std::size_t> owners;
  // Each living person's index in the crowd.
  std::vector<std::size_t> slot(people_.size(), ABSENT);
  // A far army runs no per-tick rule, so it is not in the crowd.
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    if (far_off(armies_.at(army))) {
      continue;
    }
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
  strikes_.clear();
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    strike(index, crowd, damage);
  }
  // Damage lands after everyone has struck, so the order people strike in
  // does not decide who strikes. The dead leave their army's roster.
  land(crowd, damage);
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
  strike_off_dead();
  note_falls(std::move(bereft));
  end_intents(crowd);
  const std::vector<Closing> closings = close_in(crowd);
  bear_on_villages(crowd, closings);
  std::vector<Step> pulls(crowd.people.size(), Step{0, 0});
  std::vector<bool> standing(crowd.people.size(), true);
  std::vector<std::size_t> seeking(crowd.people.size(), ABSENT);
  follow_schedules();
  const std::vector<std::size_t>& fields = fields_of(crowd);
  steer(crowd, fields, closings, pulls, standing, seeking);
  // A far army holds no field, so it does not march.
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    march(army, crowd, fields, pulls, standing, seeking);
  }
  // The newborn first act on the next tick.
  for (const Passing& passing : reborn) {
    rebirth(passing);
  }
  const std::uint64_t day = tick_count_ / rules_.ticks_per_day;
  if (life_.growth && tick_count_ % rules_.ticks_per_day == 0 &&
      day % life_.growth->interval == 0) {
    grow(*life_.growth);
    // In the tick that laid them, so a lay's fields are not built in a tick
    // of their own.
    prepare_grounds();
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
    assign_occupations();
    if (life_.trade) {
      pay(*life_.trade);
      shop(*life_.trade);
    }
    hold_elections(day);
    if (life_.diplomacy) {
      negotiate(*life_.diplomacy);
    }
  }
  // Last, so a chart holds what stands at the tick's end.
  chart_steered();
  fields_.end_tick();
}

// The field each person of `crowd` steps by, by its index in the crowd: its
// area's, toward its army's objective. The list is held between ticks so a
// tick of no more people than the last allocates nothing for it.
const std::vector<std::size_t>& World::fields_of(const Crowd& crowd) {
  std::vector<std::size_t>& fields = crowd_fields_;
  fields.assign(crowd.people.size(), 0);
  // People of one army stand mostly in one area, so the field the last
  // person was given is looked up again only when the area or objective
  // changes.
  std::optional<std::pair<std::size_t, Cell>> last;
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    const Cell cell = cell_of(crowd.hash.position(index));
    const Cell objective = armies_.at(crowd.armies.at(index)).order.objective;
    const std::size_t area = graph_.area_of(cell);
    const bool same = last && last->first == area &&
                      last->second.x == objective.x &&
                      last->second.y == objective.y;
    fields.at(index) = same ? fields.at(index - 1)
                            : fields_.field_for(map_, graph_, cell, objective);
    last = std::pair<std::size_t, Cell>{area, objective};
  }
  return fields;
}

// Elects a leader, on day `day`, for each faction with an election whose
// leader is dead or missing, or whose term `day` completes.
void World::hold_elections(std::uint64_t day) {
  for (std::size_t faction = 0; faction < leaders_.factions().size();
       ++faction) {
    if (leaders_.elects_on(faction, people_, day)) {
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
// its reach; one with an attack or waiting slot closes on the slot, `seeking`
// its foe, by its index in the crowd, until it stands there, and one holding
// for a token stands.
void World::steer(const Crowd& crowd, const std::vector<std::size_t>& fields,
                  const std::vector<Closing>& closings,
                  std::vector<Step>& pulls, std::vector<bool>& standing,
                  std::vector<std::size_t>& seeking) const {
  for (const Army& army : armies_) {
    if (far_off(army)) {
      continue;
    }
    for (std::size_t rank = 0; rank < army.roster.size(); ++rank) {
      const std::size_t index = crowd.slot.at(army.roster.at(rank));
      const FlowField& field = fields_.field(fields.at(index));
      const Pull pull =
          pull_of(field, army, rank, cell_of(crowd.hash.position(index)));
      pulls.at(index) = pull.step;
      standing.at(index) = pull.standing;
      const Closing& close = closings.at(index);
      if (const std::optional<Step> engaged =
              closing(index, army, crowd, close)) {
        const bool still = engaged->x == 0 && engaged->y == 0;
        pulls.at(index) = *engaged;
        // One on its slot, or holding, stands, whatever its objective, so
        // people closing on slots beyond it pass it.
        const bool placed = !std::holds_alternative<OnFoe>(close.approach);
        standing.at(index) = still && (placed || standing.at(index));
        seeking.at(index) =
            placed && !still ? close.foe.value_or(ABSENT) : ABSENT;
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

// The pull on `army`'s member of rank `rank`, standing on `cell`:
// its schedule's, the weakest, while it follows one and stands where its
// schedule gives a pull, standing within a cell of its spot; and otherwise its
// army's `field`, none within its arrival ring, as many cells from the
// objective as the square root of its rank, standing within two cells of that
// ring.
World::Pull World::pull_of(const FlowField& field, const Army& army,
                           std::size_t rank, Cell cell) const {
  const std::size_t member = army.roster.at(rank);
  if (member < errands_.size() && errands_.at(member).followed == tick_count_) {
    const Errand& errand = errands_.at(member);
    if (const std::optional<Step> step = errand_pull(errand, cell)) {
      return Pull{*step, std::max(std::abs(errand.spot.x - cell.x),
                                  std::abs(errand.spot.y - cell.y)) <= 1};
    }
  }
  const auto ring =
      static_cast<std::uint32_t>(root(static_cast<std::int64_t>(rank)));
  const std::uint32_t distance = field.distance(cell);
  return Pull{distance > ring ? field.step(cell) : Step{0, 0},
              distance <= ring + STANDING_SLACK};
}

// Each member steps away from the people closer to it than the separation,
// the harder the closer each is, and otherwise along its pull. A person on the
// move does not give way to people standing still, who give way to it, so a
// standing crowd parts for a person walking out of it, nor to anyone behind
// it, so a crowd queues through a gap; and two people `seeking` attack slots
// round the same foe do not give way to each other, so crossing paths round
// it do not lock (ADR 0080). A step enters any cell
// the flow field reaches. Positions are read from `crowd`, as the tick began,
// so the order people move in decides nothing.
void World::march(std::size_t army, const Crowd& crowd,
                  const std::vector<std::size_t>& fields,
                  const std::vector<Step>& pulls,
                  const std::vector<bool>& standing,
                  const std::vector<std::size_t>& seeking) {
  const Army& marching = armies_.at(army);
  if (far_off(marching)) {
    return;
  }
  const std::int64_t separation = rules_.separation;
  for (const std::size_t member : marching.roster) {
    const std::size_t self = crowd.slot.at(member);
    const FlowField& field = fields_.field(fields.at(self));
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
      const bool ignored =
          other == self ||
          (!standing.at(self) && (standing.at(other) || ahead < 0)) ||
          (seeking.at(self) != ABSENT && seeking.at(self) == seeking.at(other));
      if (ignored) {
        return;
      }
      const bool yields = standing.at(self) && !standing.at(other);
      const Step lean{crowd.people.at(other) < member ? 1 : -1, 0};
      const Push felt =
          yields ? aside(from, there, separation, pulls.at(other), side)
                 : repulsion(from, there, separation, lean);
      push.x += felt.x;
      push.y += felt.y;
    });
    const Step direction{sign((push.x * 2) + pull.x),
                         sign((push.y * 2) + pull.y)};
    people_.at(member).position = stepped(field, from, direction);
  }
}

StateHash World::hash() const {
  std::uint64_t digest = mix(rng_state_ ^ mix(tick_count_) ^ mix(mix(seed_)));
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
    fold(structure.role);
  }
  for (const Person& person : people_) {
    fold_person(fold, person, people_.size(), life_.cultivation.has_value());
  }
  for (const Soul& soul : souls_) {
    fold_soul(fold, soul);
  }
  for (std::size_t soul = 0; soul < charts_.size(); ++soul) {
    fold_chart(fold, soul, charts_.at(soul));
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
    fold_far(fold, std::get_if<FarAnchor>(&army.course));
  }
  for (const AreaTier tier : areas_.tiers()) {
    fold(static_cast<std::uint8_t>(tier));
  }
  for (const Leadership& faction : leaders_.factions()) {
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
  for (const Fight& fight : leaders_.duels()) {
    fold(fight.faction);
    fold(fight.challenger);
    fold(fight.leader);
  }
  fold_relations(fold, declarations_.slots(), armies_, life_.diplomacy);
  for (const VillageRecord& village : villages_.records()) {
    fold_village(fold, village);
  }
  fold_reaches(fold, rules_);
  fold_trade(fold, life_.trade);
  fold_occupations(fold, life_.occupations);
  fold_gear(fold, life_.gear);
  for (const Event& event : events_.entries()) {
    fold_event(fold, event);
  }
  // Folded only when held, so a world where no one learns keeps its hash.
  for (const Progress& step : progress_.entries()) {
    fold_progress(fold, step);
  }
  fold_attacks(fold, rules_.tokens, attacks_);
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
  const auto all_in = [this, village](Stage wanted) {
    return villages_.all_in(village, wanted, stages());
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

// Ends every duel in which a duellist has no more than its faction's yield
// health left: the leader keeps the lead unless only the challenger stands,
// who then leads, and the factions at peace with it turn hostile to it if
// the duel says so.
void World::settle_duels() {
  for (const Won& won : leaders_.settle(people_)) {
    record(cell_of(people_.at(won.challenger).position),
           LeaderChanged{won.faction, won.challenger, ChosenBy::DUEL});
    if (won.allies_turn_hostile) {
      turn_allies_hostile(rules_.hostile, won.faction);
    }
  }
}

std::optional<std::size_t> World::leader(std::size_t faction) const {
  return leaders_.of(faction).leader;
}

bool World::passes_by_duel(std::size_t faction) const {
  return leaders_.of(faction).succession.has_value();
}

bool World::elects(std::size_t faction) const {
  return leaders_.of(faction).election.has_value();
}

bool World::hostile(std::size_t from, std::size_t target) const {
  return declarations_.hostile(from, target, rules_.hostile);
}

Relation World::relation(std::size_t faction, std::size_t other) const {
  return declarations_.relation(faction, other, rules_.hostile);
}

std::optional<Declared> World::declaration(std::size_t faction,
                                           std::size_t other) const {
  return declarations_.between(faction, other);
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
  if (!own || leaders_.of(*own).leader != person) {
    return DeclarationRefusal::NOT_THE_LEADER;
  }
  const std::uint64_t hold =
      life_.diplomacy
          ? std::uint64_t{life_.diplomacy->hold_days} * rules_.ticks_per_day
          : 0;
  return declarations_.refusal(*own, faction, what, rules_.hostile, tick_count_,
                               hold);
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
  declarations_.declare(*own, faction, Declared{what, *own, tick_count_});
}

void World::steer(std::size_t army, bool steered) {
  armies_.at(army).steered = steered;
}

bool World::steered(std::size_t army) const { return armies_.at(army).steered; }

void World::set_profile(std::optional<Profile> profile) { profile_ = profile; }

const Areas& World::areas() const { return areas_; }

const AreaFields& World::fields() const { return fields_; }

std::optional<FarAnchor> World::far(std::size_t army) const {
  const auto* anchor = std::get_if<FarAnchor>(&armies_.at(army).course);
  return anchor != nullptr ? std::optional<FarAnchor>{*anchor} : std::nullopt;
}

void World::retier() {
  std::vector<std::size_t> focus;
  std::vector<std::size_t> people(areas_.count(), 0);
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      const std::size_t area =
          areas_.area_of(cell_of(people_.at(member).position));
      ++people.at(area);
      if (army.steered) {
        focus.push_back(area);
      }
    }
  }
  // Each area once, however many steered people stand in it.
  std::sort(focus.begin(), focus.end());
  focus.erase(std::unique(focus.begin(), focus.end()), focus.end());
  if (profile_ && !focus.empty()) {
    areas_.recompute(Census{std::move(focus), std::move(people)}, *profile_);
  } else {
    areas_.make_live();
  }
  for (Army& army : armies_) {
    if (army.roster.empty()) {
      continue;
    }
    AreaTier nearest = AreaTier::FAR;
    for (const std::size_t member : army.roster) {
      nearest = std::min(
          nearest,
          areas_.tier(areas_.area_of(cell_of(people_.at(member).position))));
    }
    const auto* anchor = std::get_if<FarAnchor>(&army.course);
    if (nearest == AreaTier::FAR && anchor == nullptr) {
      go_far(army);
    } else if (nearest != AreaTier::FAR && anchor != nullptr) {
      come_back(army, *anchor);
    }
  }
}

// Hands `army` live to far (ADR 0077): every member keeps its whole record,
// the army takes its lead's area and cell as its anchor, its progress 0, and
// its field is dropped.
void World::go_far(Army& army) {
  const Cell lead = cell_of(people_.at(army.roster.front()).position);
  army.course = FarAnchor{areas_.area_of(lead), lead, 0};
}

// Hands `army` far to live (ADR 0077): its lead stands on the open cell
// nearest its anchor, and the rest, in roster order, on the open cells next
// nearest, nearer by distance and then by cell index; an open cell is
// passable and holds no one of an army that is not far. Every record but the
// members' positions carries over, and the army's field is rebuilt.
void World::come_back(Army& army, FarAnchor far) {
  const Cell anchor = far.cell;
  const auto width = static_cast<std::size_t>(map_.width());
  const auto index_of = [width](Cell cell) {
    return (static_cast<std::size_t>(cell.y) * width) +
           static_cast<std::size_t>(cell.x);
  };
  std::vector<bool> held(width * static_cast<std::size_t>(map_.height()),
                         false);
  for (const Army& other : armies_) {
    if (far_off(other) || &other == &army) {
      continue;
    }
    for (const std::size_t member : other.roster) {
      held.at(index_of(cell_of(people_.at(member).position))) = true;
    }
  }
  // Rings of cells round the anchor, nearest first, until every member
  // stands.
  std::size_t placed = 0;
  const std::int32_t widest = std::max(map_.width(), map_.height());
  for (std::int32_t ring = 0; ring <= widest && placed < army.roster.size();
       ++ring) {
    std::vector<Cell> open = ring_round(anchor, ring);
    open.erase(std::remove_if(open.begin(), open.end(),
                              [&](Cell cell) {
                                return !map_.passable(cell) ||
                                       held.at(index_of(cell));
                              }),
               open.end());
    std::sort(open.begin(), open.end(), [&](Cell one, Cell other) {
      const std::int64_t near_one =
          distance_squared(centre_of(one), centre_of(anchor));
      const std::int64_t near_other =
          distance_squared(centre_of(other), centre_of(anchor));
      return near_one != near_other ? near_one < near_other
                                    : index_of(one) < index_of(other);
    });
    for (const Cell cell : open) {
      if (placed == army.roster.size()) {
        break;
      }
      people_.at(army.roster.at(placed)).position = centre_of(cell);
      held.at(index_of(cell)) = true;
      ++placed;
    }
  }
  army.course = Bound{};
}

bool World::far_off(const Army& army) {
  return std::holds_alternative<FarAnchor>(army.course);
}

// How far army `army`'s order's skills that damage structures reach from a
// striker, range and radius, its members' basic strikes among them; 0 when
// none does.
std::int32_t World::structure_reach(const Army& army) const {
  std::int32_t farthest = 0;
  const auto note = [&farthest](const Skill& used) {
    if (used.structure_damage > 0) {
      farthest = std::max(farthest, used.range + used.radius);
    }
  };
  const std::size_t basic = life_.gear ? 1 : 0;
  for (const std::size_t skill : army.order.skills) {
    if (skill >= basic) {
      note(army.skills.at(skill - basic));
      continue;
    }
    for (const std::size_t member : army.roster) {
      note(usable(people_.at(member), army, skill));
    }
  }
  return farthest;
}

// The village army `army` bears on: one whose claim holds its order's
// objective, owned by a faction it is hostile to and not in ruins, when no
// driver steers it, it is not far and one of its order's skills damages
// structures; none otherwise. Of several, the one it is a raiding party
// against, if it is, and otherwise the first.
std::optional<std::size_t> World::bearing_on(std::size_t army) const {
  const Army& bearing = armies_.at(army);
  if (bearing.steered || far_off(bearing) || structure_reach(bearing) == 0) {
    return std::nullopt;
  }
  const auto borne = [&](std::size_t village) {
    return claims(village, bearing.order.objective) &&
           hostile(bearing.faction, owner(village)) &&
           !villages_.all_in(village, Stage::RUBBLE, stages());
  };
  std::optional<std::size_t> first;
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const VillageRecord& record = villages_.at(village);
    if (record.party == army && record.raiding && borne(*record.raiding)) {
      return record.raiding;
    }
    if (!first && borne(village)) {
      first = village;
    }
  }
  return first;
}

// Whether a structure of village `village` short of rubble lies within
// `reach` of `from`.
bool World::standing_within(std::size_t village, Position from,
                            std::int32_t reach) const {
  const std::vector<std::size_t>& own = villages_.at(village).spec.structures;
  return std::any_of(own.begin(), own.end(), [&](std::size_t index) {
    return stage(index) != Stage::RUBBLE &&
           reaches(structures_.at(index), from, reach);
  });
}

// Each army bearing on a village, with no member engaging a foe and its
// objective out of reach of every structure of it still standing, is ordered
// on to the open cell nearest the standing structure nearest its lead, so a
// band ruins what stands beyond its centre. A member in reach of one does not
// hold the band, which holds where it gathers, at its objective.
void World::bear_on_villages(const Crowd& crowd,
                             const std::vector<Closing>& closings) {
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    const Army& bearing = armies_.at(army);
    const std::optional<std::size_t> village = bearing_on(army);
    if (!village || bearing.roster.empty()) {
      continue;
    }
    const std::int32_t reach = structure_reach(bearing);
    const auto engaging = [&](std::size_t member) {
      const std::size_t index =
          member < crowd.slot.size() ? crowd.slot.at(member) : ABSENT;
      return index != ABSENT && closings.at(index).foe;
    };
    if (std::any_of(bearing.roster.begin(), bearing.roster.end(), engaging) ||
        standing_within(*village, centre_of(bearing.order.objective), reach)) {
      continue;
    }
    if (const std::optional<Cell> cell = next_objective(bearing, *village)) {
      order(army, Order{*cell, bearing.order.skills, bearing.order.target});
    }
  }
}

// The open cell inside village `village`'s claim that army `army`'s course
// reaches nearest the structure of the village short of rubble nearest its
// lead: of the nearest ring of cells round that structure's footprint holding
// one, the one nearest the lead. Inside the claim, so the army still bears on
// the village there. None when nothing stands or no such cell lies on the map.
std::optional<Cell> World::next_objective(const Army& army,
                                          std::size_t village) const {
  const Position lead = people_.at(army.roster.front()).position;
  const std::optional<std::size_t> standing = nearest_standing(village, lead);
  if (!standing || far_off(army)) {
    return std::nullopt;
  }
  const std::int32_t side = std::max(map_.width(), map_.height());
  for (std::int32_t ring = 0; ring <= side; ++ring) {
    if (const std::optional<Cell> cell =
            nearest_in_ring(structures_.at(*standing), ring, lead,
                            army.order.objective, village)) {
      return cell;
    }
  }
  return std::nullopt;
}

// The structure of village `village` short of rubble nearest `from`, ties by
// structure index; none when nothing stands.
std::optional<std::size_t> World::nearest_standing(std::size_t village,
                                                   Position from) const {
  std::optional<std::size_t> nearest;
  std::int64_t least = 0;
  for (const std::size_t index : villages_.at(village).spec.structures) {
    if (stage(index) == Stage::RUBBLE) {
      continue;
    }
    const std::int64_t apart =
        distance_squared(from, nearest_point(structures_.at(index), from));
    if (!nearest || apart < least || (apart == least && index < *nearest)) {
      nearest = index;
      least = apart;
    }
  }
  return nearest;
}

// Of the cells `ring` cells round `structure`'s footprint, the footprint
// itself at 0, the passable one inside village `village`'s claim that a walk
// joins to `objective` nearest `from`, ties by row and then column; none
// when there is no such cell.
std::optional<Cell> World::nearest_in_ring(const Structure& structure,
                                           std::int32_t ring, Position from,
                                           Cell objective,
                                           std::size_t village) const {
  const std::int32_t left = structure.cell.x - ring;
  const std::int32_t top = structure.cell.y - ring;
  const std::int32_t right =
      structure.cell.x + structure.footprint.width - 1 + ring;
  const std::int32_t bottom =
      structure.cell.y + structure.footprint.height - 1 + ring;
  std::optional<Cell> best;
  std::int64_t closest = 0;
  for (std::int32_t row = top; row <= bottom; ++row) {
    // The ring's top and bottom rows whole, its sides' two cells, and the
    // footprint itself whole.
    const bool whole = ring == 0 || row == top || row == bottom;
    const std::int32_t step = whole ? 1 : right - left;
    for (std::int32_t column = left; column <= right; column += step) {
      const Cell cell{column, row};
      const std::int64_t apart = distance_squared(from, centre_of(cell));
      if (map_.passable(cell) && claims(village, cell) &&
          graph_.joined(map_, Walk{cell, objective}) &&
          (!best || apart < closest)) {
        best = cell;
        closest = apart;
      }
    }
  }
  return best;
}

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
    const std::optional<std::size_t> leader = leaders_.of(faction).leader;
    const std::optional<std::size_t> army =
        leader ? army_of(*leader) : std::nullopt;
    if (!army || armies_.at(*army).steered) {
      continue;
    }
    for (std::size_t other = 0; other < factions; ++other) {
      const std::optional<Sought> wanted = declarations_.sought(
          diplomacy, faction, other, numbers, rules_.hostile);
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

std::size_t World::tier(std::size_t village) const {
  return villages_.at(village).tier;
}

std::optional<Settlement> World::settlement(std::size_t village) const {
  const std::optional<Cell> corner = villages_.at(village).founded;
  if (!corner || !life_.growth) {
    return std::nullopt;
  }
  return Settlement{life_.growth->settlement_kind, *corner,
                    life_.growth->tiers.front().square};
}

Stage World::stage(std::size_t structure) const {
  return stages().of(structure);
}

// The stage each structure is in now.
Stages World::stages() const {
  return Stages{structures_, rules_.stage_floors};
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
                   std::vector<std::uint64_t>& damage) {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  Person& striker = people_.at(self);
  const Army& own = armies_.at(army);
  const std::optional<Cast> cast = std::exchange(striker.casting, std::nullopt);
  if (cast) {
    const std::optional<std::size_t> aimed_at = person_aimed_at(cast->target);
    if (assess_cast(self, army, *cast, tick_count_) == CastRefusal::NONE &&
        (!aimed_at || may_aim(self, own, *aimed_at))) {
      use(user, crowd, cast->skill,
          landing(striker, usable(striker, own, cast->skill), cast->target),
          person_aimed_at(cast->target), damage);
    }
    return;
  }
  const std::vector<std::size_t>& choice = own.order.skills;
  const bool drilled = striker.ready_at <= tick_count_;
  const auto affordable = [&](const Known& known) {
    return known.ready_at <= tick_count_ &&
           striker.qi >=
               life_.arts.at(known.art).skills.at(known.skill).skill.qi_cost &&
           armed(striker, known);
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
          index = drilled_skills(own) + known;
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
    use(user, crowd, index, striker.position, std::nullopt, damage);
    return;
  }
  if (const std::optional<std::size_t> target = aim_of(user, skill, crowd)) {
    use(user, crowd, index, crowd.hash.position(*target),
        crowd.people.at(*target), damage);
  }
}

// The person at `user` of `crowd` faces with `skill`, by its index in the
// crowd: its army's attack intent's target while that lies in range, none
// while it lies out of range, and with no intent the nearest hostile person in
// range; in either case only one it may_aim() at. The crowd's order breaks
// ties, so the order the query visits people in does not decide the target.
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
        distance_squared(from, crowd.hash.position(aimed)) <= range * range &&
        may_aim(self, own, *own.order.target);
    return reached ? std::optional<std::size_t>{aimed} : std::nullopt;
  }
  return crowd.hash.nearest(
      foe_kinds(self, own), from, skill.range, [&](std::size_t other) {
        const std::size_t target = crowd.people.at(other);
        return foes(self, army, target, crowd.armies.at(other)) &&
               may_aim(self, own, target);
      });
}

// Whether person `self`, serving in `own`, may aim a use at person `target`
// (ADR 0068): always while the rules set no tokens, as the steered person,
// or toward the other of its duel; otherwise only holding one of `target`'s
// tokens.
bool World::may_aim(std::size_t self, const Army& own,
                    std::size_t target) const {
  if (!rules_.tokens || own.steered || leaders_.duelling(self, target)) {
    return true;
  }
  const std::optional<Engagement> held = attacks_.engagement(self);
  return held && held->foe == target && held->token;
}

// The one rule every skill resolves through, whoever uses it and however it
// was aimed: each use of skill `index`, among the user's, toward `toward`,
// spends its qi and hits once every hostile person its shape covers, or for
// a targeted skill the person `locked` alone, as much harder as the might
// of its realm gives and, for an art skill, the potency of its nature and the
// edge of its weapon, and every structure its shape reaches.
void World::use(std::size_t user, const Crowd& crowd, std::size_t index,
                Position toward, std::optional<std::size_t> locked,
                std::vector<std::uint64_t>& damage) {
  const std::size_t army = crowd.armies.at(user);
  const std::size_t self = crowd.people.at(user);
  Person& striker = people_.at(self);
  const Army& own = armies_.at(army);
  const Skill& skill = usable(striker, own, index);
  const std::optional<std::size_t> known =
      index >= drilled_skills(own)
          ? std::optional<std::size_t>{index - drilled_skills(own)}
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
  const std::uint32_t rank = rank_of(striker, life_);
  const std::uint64_t might =
      rank > 0 ? life_.realms.at(rank - 1).might : PER_MILLE;
  std::uint64_t body_damage = skill.body_damage * might / PER_MILLE;
  if (art) {
    const std::uint64_t edge =
        life_.gear ? edge_of(*life_.gear, striker.worn) : PER_MILLE;
    body_damage = body_damage * agreement(striker, *art, true, life_) /
                  PER_MILLE * edge / PER_MILLE;
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

// Lands `damage` on each person of `crowd`, less what it wears guards
// against.
void World::land(const Crowd& crowd, const std::vector<std::uint64_t>& damage) {
  for (std::size_t index = 0; index < crowd.people.size(); ++index) {
    Person& struck = people_.at(crowd.people.at(index));
    const std::uint64_t guard =
        life_.gear ? guard_of(*life_.gear, struck.worn) : 0;
    struck.health -= std::min(
        damage.at(index) * (PER_MILLE - guard) / PER_MILLE, struck.health);
  }
}

// Skill `skill` of person `person`, serving in `army`: with gear its basic
// strike, then the army's skills, followed by the art skills it knows.
Skill World::usable(const Person& person, const Army& army,
                    std::size_t skill) const {
  const std::size_t basic = life_.gear ? 1 : 0;
  if (const std::optional<Gear>& gear = life_.gear; gear && skill == 0) {
    return basic_strike(*gear, person.worn);
  }
  if (skill < drilled_skills(army)) {
    return army.skills.at(skill - basic);
  }
  const Known& known = person.known.at(skill - drilled_skills(army));
  return life_.arts.at(known.art).skills.at(known.skill).skill;
}

// How many skills a person serving in `army` has before those it knows: with
// gear its basic strike, and the army's.
std::size_t World::drilled_skills(const Army& army) const {
  return army.skills.size() + (life_.gear ? 1 : 0);
}

// Whether person `person`, serving in `army`, may use its skill `skill` on
// tick `due`.
bool World::ready(const Person& person, std::size_t skill, const Army& army,
                  std::uint64_t due) const {
  const std::uint64_t ready_at =
      skill < drilled_skills(army)
          ? person.ready_at
          : person.known.at(skill - drilled_skills(army)).ready_at;
  return ready_at <= due;
}

// Whether the person at `index` of `crowd`, pulled along `pull`, has a
// living foe on the move, not `standing`, ahead of it along the pull within
// its reach.
bool World::held(std::size_t index, Step pull,
                 const std::vector<bool>& standing, const Crowd& crowd) const {
  if (pull.x == 0 && pull.y == 0) {
    return false;
  }
  const std::size_t member = crowd.people.at(index);
  const std::size_t army = crowd.armies.at(index);
  const Position from = crowd.hash.position(index);
  // Only people of a kind the person may strike can hold it, so the search
  // passes over the places holding none.
  return crowd.hash.any_within(
      foe_kinds(member, armies_.at(army)), from,
      reach(people_.at(member), armies_.at(army)), [&](std::size_t other) {
        const Position there = crowd.hash.position(other);
        const std::int64_t ahead = ((std::int64_t{there.x} - from.x) * pull.x) +
                                   ((std::int64_t{there.y} - from.y) * pull.y);
        return ahead > 0 && !standing.at(other) &&
               people_.at(crowd.people.at(other)).health > 0 &&
               foes(member, army, crowd.people.at(other),
                    crowd.armies.at(other));
      });
}

// The foe the person at `index` of `crowd`, serving in `army`, engages, as
// its index in the crowd: its attack intent's target, whoever steers the army,
// or else the nearest living foe within the rules' sight, the first in the
// crowd's order among equals, inside its home village's claim unless its army
// marches on an objective beyond it or it has no home; while the rules set
// tokens, the nearest such foe with a token free or granted to it, if any,
// is engaged first (ADR 0068), `granting` holding those with one free. None
// when a driver steers its army and it holds no intent, it reaches no farther
// than itself, or no foe is in sight.
std::optional<std::size_t> World::engaged_foe(
    std::size_t index, const Army& army, const Crowd& crowd,
    const std::vector<bool>& granting) const {
  const std::size_t member = crowd.people.at(index);
  if (army.order.target) {
    const std::size_t aimed = crowd.slot.at(*army.order.target);
    return aimed == ABSENT ? std::nullopt : std::optional<std::size_t>{aimed};
  }
  if (army.steered || rules_.sight == 0 ||
      reach(people_.at(member), army) == 0) {
    return std::nullopt;
  }
  // At home, it defends its village's claim; on the march, it fights what
  // it meets.
  const std::size_t own = crowd.armies.at(index);
  const std::optional<std::size_t> home = people_.at(member).home;
  const bool marching = !home || !claims(*home, army.order.objective);
  const auto engageable = [&](std::size_t other) {
    return (marching || claims(*home, cell_of(crowd.hash.position(other)))) &&
           foes(member, own, crowd.people.at(other), crowd.armies.at(other));
  };
  const auto nearest = [&](const auto& accept) {
    return crowd.hash.nearest(foe_kinds(member, armies_.at(own)),
                              crowd.hash.position(index),
                              rules_.sight * CELL_SIZE, accept);
  };
  if (!rules_.tokens) {
    return nearest(engageable);
  }
  // With tokens, a foe with one free or granted to it first (ADR 0068), and
  // else the nearest foe of all. Until the search accepts a foe it asks of
  // every foe in sight, so when it accepts none, the nearest engageable foe
  // it was asked of, the lowest index among equals, is the nearest of all,
  // and one search finds both.
  const std::optional<Engagement> held = attacks_.engagement(member);
  const Position from = crowd.hash.position(index);
  std::optional<std::size_t> any;
  std::int64_t any_distance = 0;
  const std::optional<std::size_t> open = nearest([&](std::size_t other) {
    if (granting.at(other) ||
        (held && held->token && held->foe == crowd.people.at(other))) {
      return engageable(other);
    }
    const std::int64_t distance =
        distance_squared(from, crowd.hash.position(other));
    if ((!any || distance < any_distance ||
         (distance == any_distance && other < *any)) &&
        engageable(other)) {
      any = other;
      any_distance = distance;
    }
    return false;
  });
  return open ? open : any;
}

// By index in `crowd`, whether the person there holds fewer of its tokens
// than it grants by the rules' token table.
std::vector<bool> World::granting(const Crowd& crowd) const {
  std::vector<bool> granting(crowd.people.size(), false);
  if (rules_.tokens) {
    const std::vector<std::uint32_t>& by_rank = rules_.tokens->by_rank;
    for (std::size_t index = 0; index < crowd.people.size(); ++index) {
      const std::size_t person = crowd.people.at(index);
      granting.at(index) =
          attacks_.tokens_held(person) <
          by_rank.at(std::min<std::size_t>(rank_of(people_.at(person), life_),
                                           by_rank.size() - 1));
    }
  }
  return granting;
}

// Each living person's foe and how it closes on it, by its index in `crowd`.
// While the rules set attack tokens, each engaging a foe within its waiting
// reach takes a token or waits for one, and a slot round the foe on the ring
// it strikes or waits from (ADR 0068), each offered only where uncrowded
// (ADR 0080, and for waiting slots as the nearest case); a striker closes on
// its slot round the ring. The steered person engaging its intent's target
// claims a token ahead of the queue, and a duellist needs none toward the
// other.
std::vector<World::Closing> World::close_in(const Crowd& crowd) {
  std::vector<Closing> closings(crowd.people.size());
  engaging_.clear();
  const std::vector<bool> free = granting(crowd);
  for (const Army& army : armies_) {
    if (far_off(army)) {
      continue;
    }
    for (const std::size_t member : army.roster) {
      const std::size_t index = crowd.slot.at(member);
      const std::optional<std::size_t> foe =
          engaged_foe(index, army, crowd, free);
      closings.at(index).foe = foe;
      if (rules_.tokens && foe) {
        const std::size_t target = crowd.people.at(*foe);
        const std::vector<std::uint32_t>& by_rank = rules_.tokens->by_rank;
        const std::uint32_t tokens = by_rank.at(std::min<std::size_t>(
            rank_of(people_.at(target), life_), by_rank.size() - 1));
        Claim claim = Claim::QUEUED;
        if (leaders_.duelling(member, target)) {
          claim = Claim::EXEMPT;
        } else if (army.steered && army.order.target) {
          claim = Claim::FIRST;
        }
        engaging_.push_back(Engaging{member, target,
                                     reach(people_.at(member), army),
                                     crowd.hash.position(index),
                                     crowd.hash.position(*foe), tokens, claim});
      }
    }
  }
  // Someone other than the foe and those engaging it stands within the
  // separation of a slot (ADR 0080).
  const Crowded crowded = [&](Position point, std::size_t foe) {
    bool crowding = false;
    crowd.hash.visit_within(point, rules_.separation, [&](std::size_t other) {
      const std::size_t person = crowd.people.at(other);
      const std::optional<std::size_t> theirs = closings.at(other).foe;
      crowding = crowding || (person != foe && people_.at(person).health > 0 &&
                              (!theirs || crowd.people.at(*theirs) != foe));
    });
    return crowding;
  };
  if (rules_.tokens) {
    attacks_.assign(engaging_, map_, rules_.separation, *rules_.tokens,
                    static_cast<std::uint32_t>(tick_count_), crowded);
  }
  for (const Engaging& each : engaging_) {
    closings.at(crowd.slot.at(each.person)).approach = each.approach;
  }
  return closings;
}

// The pull of the person at `index` of `crowd`, serving in `army`, engaging
// `close`'s foe: none while it holds, stands on its waiting slot, stands on
// its slot with the foe within its reach, or, with no slot, while the foe
// lies within its reach, so it stands and fights; and otherwise the step
// toward the point it closes on, its slot, a slot on its way round the ring
// or its waiting slot, or the foe without one.
// The step is the one of the eight nearest the line to that point, open as a
// flow field's is: into a passable cell, and on a diagonal only where both
// cells beside it are passable; an attack intent's chase whose diagonal is
// shut steps along an open axis instead. Nothing, so it keeps its objective,
// when it engages no foe or no step is open.
std::optional<Step> World::closing(std::size_t index, const Army& army,
                                   const Crowd& crowd,
                                   const Closing& close) const {
  if (!close.foe) {
    return std::nullopt;
  }
  const std::size_t member = crowd.people.at(index);
  const std::int64_t reached = reach(people_.at(member), army);
  const Position from = crowd.hash.position(index);
  const Position foe_at = crowd.hash.position(*close.foe);
  const bool in_reach = distance_squared(from, foe_at) <= reached * reached;
  const auto arrived = [from](Position point) {
    return distance_squared(from, point) <= std::int64_t{ON_SLOT} * ON_SLOT;
  };
  Position there = foe_at;
  bool stands = in_reach;
  if (const auto* slot = std::get_if<ToSlot>(&close.approach)) {
    there = slot->at;
    stands = in_reach && arrived(there);
  } else if (const auto* waiting = std::get_if<ToWait>(&close.approach)) {
    there = waiting->at;
    stands = arrived(there);
  } else if (std::holds_alternative<Hold>(close.approach)) {
    stands = true;
  }
  if (stands) {
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
    farthest = std::max(farthest, usable(person, army, skill).range);
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
         leaders_.duelling(person, other);
}

// The kinds the crowd's index marks the people person `person`, serving in
// `army`, may strike by: those of the factions hostile to its army's, or every
// kind while it fights a duel, whose foe may be of any faction.
std::uint64_t World::foe_kinds(std::size_t person, const Army& army) const {
  if (leaders_.in_duel(person)) {
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
  if (cast.skill >= drilled_skills(own) + caster.known.size()) {
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
  if (cast.skill >= drilled_skills(own) &&
      !armed(caster, caster.known.at(cast.skill - drilled_skills(own)))) {
    return CastRefusal::WRONG_WEAPON;
  }
  return CastRefusal::NONE;
}

// Whether person `person` wears the weapon its known skill `known` needs.
bool World::armed(const Person& person, const Known& known) const {
  return !life_.gear || armed_for(*life_.gear, person.worn,
                                  life_.arts.at(known.art).discipline);
}

std::optional<std::size_t> World::army_of(std::size_t person) const {
  return person < serving_.size() ? serving_.at(person) : std::nullopt;
}

// Appends person `person` to army `army`'s roster.
void World::enrol(std::size_t person, std::size_t army) {
  armies_.at(army).roster.push_back(person);
  bump_army(army);
  if (person >= serving_.size()) {
    serving_.resize(person + 1);
  }
  serving_.at(person) = army;
}

// The army whose roster holds person `person`. Throws std::logic_error when
// none does, for a person the caller holds to be living.
std::size_t World::army_serving(std::size_t person) const {
  const std::optional<std::size_t> army = army_of(person);
  if (!army) {
    throw std::logic_error("a living person must stand on a roster");
  }
  return *army;
}

// Takes person `person` off the roster it stands on. Throws std::logic_error
// when the roster serving_ names does not hold it.
void World::strike_off(std::size_t person) {
  const std::size_t army = army_serving(person);
  std::vector<std::size_t>& roster = armies_.at(army).roster;
  const auto held = std::find(roster.begin(), roster.end(), person);
  if (held == roster.end()) {
    throw std::logic_error("a person's army must hold it on its roster");
  }
  roster.erase(held);
  bump_army(army);
  serving_.at(person).reset();
}

// Takes every dead person off the roster it stands on.
void World::strike_off_dead() {
  const auto dead = [this](std::size_t member) {
    return people_.at(member).health == 0;
  };
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    std::vector<std::size_t>& roster = armies_.at(army).roster;
    for (const std::size_t member : roster) {
      if (dead(member)) {
        serving_.at(member).reset();
        // A spouse left unwed may join its parents' household.
        for (const std::size_t spouse : people_.at(member).spouses) {
          bump_serving(spouse);
        }
      }
    }
    const auto alive = std::remove_if(roster.begin(), roster.end(), dead);
    if (alive != roster.end()) {
      roster.erase(alive, roster.end());
      bump_army(army);
    }
  }
}

// Gives army `army` everything of `refitted` but its roster, which it keeps.
void World::refit(std::size_t army, Army refitted) {
  refitted.roster = std::move(armies_.at(army).roster);
  armies_.at(army) = std::move(refitted);
}

// Sets each person's army from the rosters, which rostered() has checked.
void World::index_rosters() {
  serving_.assign(people_.size(), std::nullopt);
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (const std::size_t member : armies_.at(army).roster) {
      serving_.at(member) = army;
    }
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
  const std::optional<std::size_t> home = villages_.village_of(structure);
  const bool held =
      home && !armies_.at(villages_.at(*home).spec.residents).roster.empty();
  if (held && (effect > 0) != hostile(striker.faction, owner(*home))) {
    return false;
  }
  Structure& shifted = structures_.at(structure);
  // A terraced field, on rough cells, is raised at the growth's terrace per
  // mille of the labour.
  const std::vector<Cell> cells = cells_of(shifted.cell, shifted.footprint);
  const bool terraced =
      life_.growth &&
      std::any_of(cells.begin(), cells.end(), [this](Cell cell) {
        return map_.passable(cell) && !map_.buildable(cell);
      });
  if (terraced && effect < 0) {
    effect = std::min<std::int64_t>(
        effect * PER_MILLE / std::int64_t{life_.growth->terrace}, -1);
  }
  const std::int64_t before = shifted.durability;
  const bool stood = stage(structure) != Stage::RUBBLE;
  const bool used = stage(structure) <= Stage::DAMAGED;
  const std::int64_t ceiling =
      std::max<std::int64_t>(before, rules_.stage_floors.front());
  shifted.durability = static_cast<std::uint32_t>(
      std::clamp(before - effect, std::int64_t{0}, ceiling));
  if (!home) {
    return effect != 0;
  }
  // A house or workplace that falls or is raised moves who lives or works
  // there.
  if (used != (stage(structure) <= Stage::DAMAGED)) {
    bump_village(*home);
  }
  const VillageChange change = villages_.shifted(
      *home, stood, shifted.durability > before, held, striker, stages());
  if (change == VillageChange::RUINED) {
    record(seat(*home), Ruined{*home, owner(*home), striker});
  }
  if (change == VillageChange::RETAKEN) {
    for (const std::size_t member : armies_.at(striker.army).roster) {
      people_.at(member).home = home;
    }
  }
  return effect != 0;
}

// Draws `person`'s talent and lifespan, for a soul of `soul` history, its
// sex unless `sex` gives it, and with gear the loadout of its `calling`: the
// one rule every birth follows.
void World::bear(Person& person, const Soul& soul, const Calling& calling,
                 std::optional<Sex> sex) {
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
  dress(person, calling);
}

// Sets, by faction, the weapon family an art's-weapon choice gives, from the
// faction of each of the world's `sects`, the manuals they start with and the
// gear: the family its sect's art needs.
void World::arm_factions(const std::vector<std::size_t>& sects) {
  art_families_.clear();
  if (!life_.gear) {
    return;
  }
  std::vector<std::size_t> disciplines;
  disciplines.reserve(life_.arts.size());
  for (const Art& art : life_.arts) {
    disciplines.push_back(art.discipline);
  }
  art_families_ =
      art_families(*life_.gear, rules_.hostile.size(), sects,
                   life_.manuals ? life_.manuals->held
                                 : std::vector<std::vector<std::size_t>>{},
                   disciplines);
}

// With gear, dresses `person`, the next the world holds, in the first
// loadout that fits its `calling`, drawing one choice by weight for each slot
// the loadout lists. Each draw is a counter-based hash of the seed, the
// person and the slot, so dressing draws nothing from the world's stream.
void World::dress(Person& person, const Calling& calling) {
  person.worn.clear();
  if (!life_.gear) {
    return;
  }
  person.worn.assign(life_.gear->slots.size(), std::nullopt);
  const Loadout* fit = loadout_for(*life_.gear, calling.faction,
                                   calling.founder, calling.soldier);
  for (std::size_t slot = 0; fit != nullptr && slot < fit->slots.size();
       ++slot) {
    const std::vector<Choice>& choices = fit->slots.at(slot);
    std::uint64_t total = 0;
    for (const Choice& choice : choices) {
      total += choice.weight;
    }
    if (total == 0) {
      continue;
    }
    const std::uint64_t draw = mix(mix(mix(seed_) ^ people_.size()) ^ slot);
    std::uint64_t drawn = draw % total;
    for (const Choice& choice : choices) {
      if (drawn >= choice.weight) {
        drawn -= choice.weight;
        continue;
      }
      const std::optional<std::size_t> family =
          choice.art && calling.faction < art_families_.size()
              ? art_families_.at(calling.faction)
              : choice.family;
      // A family gives a kind of its lowest tier, the same draw picking
      // among several.
      const std::vector<std::size_t> lowest =
          family ? lowest_tier(*life_.gear, *family)
                 : std::vector<std::size_t>{};
      if (!lowest.empty()) {
        person.worn.at(slot) = lowest.at(mix(draw) % lowest.size());
      }
      break;
    }
  }
}

// Bears founder `person`, of `sex` if given, by the one birth rule, born the
// founding age before the world began, if the life gives one.
void World::found(Person& person, const Calling& calling,
                  std::optional<Sex> sex) {
  bear(person, Soul{}, calling, sex);
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
  const std::size_t family_army = villages_.at(village).spec.residents;
  bear(person, soul,
       Calling{armies_.at(family_army).faction, false, army != family_army});
  const std::size_t born = people_.size();
  for (const std::size_t parent : person.parents) {
    people_.at(parent).children.push_back(born);
  }
  people_.push_back(std::move(person));
  attacks_.grow_to(people_.size());
  record(cell_of(people_.at(born).position), Born{born});
  // The soul's army takes its new family's faction and skills.
  Army& serving = armies_.at(army);
  const Army& family = armies_.at(villages_.at(village).spec.residents);
  enrol(born, army);
  serving.faction = family.faction;
  serving.skills = family.skills;
  serving.order.skills.resize(drilled_skills(serving));
  std::iota(serving.order.skills.begin(), serving.order.skills.end(),
            std::size_t{0});
  const Cell birthplace = cell_of(people_.at(born).position);
  serving.order.objective = birthplace;
}

std::size_t World::faction(std::size_t army) const {
  return armies_.at(army).faction;
}

const std::vector<Skill>& World::skills(std::size_t army) const {
  return armies_.at(army).skills;
}

std::size_t World::drilled(std::size_t army) const {
  return drilled_skills(armies_.at(army));
}

std::size_t World::skill_count(std::size_t person) const {
  const Person& caster = people_.at(person);
  const std::optional<std::size_t> army = army_of(person);
  return army ? drilled_skills(armies_.at(*army)) + caster.known.size() : 0;
}

Skill World::skill_of(std::size_t person, std::size_t skill) const {
  return usable(people_.at(person), armies_.at(army_with(person, skill)),
                skill);
}

std::uint64_t World::ready_at(std::size_t person, std::size_t skill) const {
  const std::size_t drilled =
      drilled_skills(armies_.at(army_with(person, skill)));
  const Person& caster = people_.at(person);
  return skill < drilled ? caster.ready_at
                         : caster.known.at(skill - drilled).ready_at;
}

// The army person `person` serves in, when it has skill `skill`. Throws
// std::out_of_range when it has no such skill.
std::size_t World::army_with(std::size_t person, std::size_t skill) const {
  const std::optional<std::size_t> army = army_of(person);
  if (!army || skill >= drilled_skills(armies_.at(*army)) +
                            people_.at(person).known.size()) {
    throw std::out_of_range("the person has no such skill");
  }
  return *army;
}

std::uint32_t World::rank(std::size_t person) const {
  return rank_of(people_.at(person), life_);
}

std::optional<Nature> World::nature(std::size_t person) const {
  return nature_of(people_.at(person), life_);
}

std::optional<std::size_t> World::heart_art(std::size_t person) const {
  return heart_of(people_.at(person));
}

std::uint64_t World::art_agreement(std::size_t person, std::size_t art) const {
  return agreement(people_.at(person), art, false, life_);
}

bool World::within_realm(std::size_t person, std::size_t art,
                         std::size_t skill) const {
  return murim::within_realm(people_.at(person), art, skill, life_);
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
      const std::uint32_t before = rank_of(person, life_);
      murim::cultivate(person);
      const std::uint32_t after = rank_of(person, life_);
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
  const std::uint32_t rank = rank_of(person, life_);
  for (const std::size_t art : practised(person)) {
    const std::optional<std::size_t> skill =
        lesson(person, art, Lesson::COMPREHENDED, rank, life_);
    if (!skill) {
      continue;
    }
    if (next_random() % PER_MILLE < comprehension_chance(person, art, life_)) {
      person.known.push_back(Known{art, *skill, 0});
      note(Learned{learner, art, *skill, LearnedBy::COMPREHENSION});
    }
  }
}

// Every living person's qi recovers its cultivation's recovery per mille of
// its capacity, rounded up, up to its capacity.
void World::recover(const Cultivation& cultivation) {
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      murim::recover(people_.at(member), cultivation);
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
        if (distance > reach * reach || !may_read(reader, manual.art, life_)) {
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
    reader.qi -= reading_cost(reader, manual.art, life_);
    const std::uint32_t rank = rank_of(reader, life_);
    while (const std::optional<std::size_t> skill =
               lesson(reader, manual.art, Lesson::TAUGHT, rank, life_)) {
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
      teacher = std::max(teacher, rank_of(people_.at(elder), life_));
    }
    if (teacher == 0) {
      continue;
    }
    for (const std::size_t member : sect_members(sect)) {
      Person& pupil = people_.at(member);
      const std::uint32_t rank = std::min(teacher, rank_of(pupil, life_));
      for (const Manual& manual : manuals_) {
        const auto* held = std::get_if<HeldBySect>(&manual.place);
        if (held == nullptr || held->sect != sect) {
          continue;
        }
        const std::optional<std::size_t> skill =
            lesson(pupil, manual.art, Lesson::TAUGHT, rank, life_);
        if (!skill) {
          continue;
        }
        const std::uint64_t chance =
            teaching_chance(pupil, manual.art, teacher, life_);
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
  charts_.emplace_back(Footprint{map_.width(), map_.height()});
  return souls_.size() - 1;
}

std::optional<std::size_t> World::soul_army(std::size_t soul) const {
  for (std::size_t person = people_.size(); person-- > 0;) {
    if (people_.at(person).soul == soul) {
      return army_of(person);
    }
  }
  return std::nullopt;
}

const Soul& World::soul(std::size_t soul) const { return souls_.at(soul); }

const Attacks& World::attacks() const { return attacks_; }

const Chart& World::chart(std::size_t soul) const { return charts_.at(soul); }

// Charts, for each living person carrying a soul in an army a driver steers,
// the cells within the map sight of its cell and each village in view.
void World::chart_steered() {
  if (rules_.map_sight == 0) {
    return;
  }
  for (const Army& army : armies_) {
    if (!army.steered) {
      continue;
    }
    for (const std::size_t member : army.roster) {
      const Person& person = people_.at(member);
      if (!person.soul || person.health == 0) {
        continue;
      }
      Chart& chart = charts_.at(*person.soul);
      const Cell here = cell_of(person.position);
      chart.see(here, rules_.map_sight);
      for (std::size_t village = 0; village < villages_.size(); ++village) {
        if (in_view(structures_, villages_.at(village).spec.structures, here,
                    rules_.map_sight)) {
          chart.remember(village, memory_of(village));
        }
      }
    }
  }
}

// Village `village` as it stands: its owner, claim centre, whether it has
// living residents, and whether every structure is intact, rubble, or neither.
VillageMemory World::memory_of(std::size_t village) const {
  const auto all_in = [this, village](Stage wanted) {
    return villages_.all_in(village, wanted, stages());
  };
  VillageStage stage = VillageStage::DAMAGED;
  if (all_in(Stage::RUBBLE)) {
    stage = VillageStage::RUINS;
  } else if (all_in(Stage::INTACT)) {
    stage = VillageStage::STANDING;
  }
  return VillageMemory{owner(village), residents_of(village) > 0, stage,
                       seat(village)};
}

std::int64_t World::age_in_years(std::size_t person) const {
  return age_in_days(people_.at(person)) /
         static_cast<std::int64_t>(DAYS_PER_YEAR);
}

std::optional<std::size_t> World::faction_of(std::size_t person) const {
  const std::optional<std::size_t> army = army_of(person);
  if (!army) {
    return std::nullopt;
  }
  return armies_.at(*army).faction;
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
  for (const Army& army : armies_) {
    alive.insert(alive.end(), army.roster.begin(), army.roster.end());
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
      bear_child(mother, kinship);
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
  bump_serving(person);
  bump_serving(chosen);
}

// Bears a child to `mother` and her husband by the one birth rule, into the
// family, home and army of the parent the lineage follows, at its side; with
// balanced sexes, of the sex scarcer among its home's unmarried.
void World::bear_child(std::size_t mother, const Kinship& kinship) {
  const std::size_t father = people_.at(mother).spouses.back();
  const std::size_t lineage = kinship.fathers_family ? father : mother;
  const Person& parent = people_.at(lineage);
  Person child;
  child.position = parent.position;
  child.health = life_.newborn_health;
  child.home = parent.home;
  child.family = parent.family;
  child.parents = {father, mother};
  const std::size_t serving = army_serving(lineage);
  const bool soldier =
      !child.home || villages_.at(*child.home).spec.residents != serving;
  bear(child, Soul{}, Calling{armies_.at(serving).faction, false, soldier},
       kinship.balanced_sexes && child.home ? scarcer_sex(*child.home, kinship)
                                            : std::nullopt);
  const std::size_t born = people_.size();
  people_.at(father).children.push_back(born);
  people_.at(mother).children.push_back(born);
  people_.push_back(std::move(child));
  attacks_.grow_to(people_.size());
  record(cell_of(people_.at(born).position), Born{born});
  enrol(born, serving);
}

std::size_t World::cap(std::size_t village) const {
  return villages_.cap(village, stages(), life_.husbandry,
                       rules_.people_capacity);
}

// Each village's healing from its standing structures, as its
// people and the growth's self-reliance let them serve.
Service World::served() const {
  if (villages_.empty()) {
    return {};
  }
  return villages_.served(
      homes(), life_.growth ? life_.growth->self_reliance : 0, stages());
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

std::uint64_t World::harvest(std::size_t village) const {
  return villages_.held(village, Stage::INTACT, stages()).harvest;
}

std::size_t World::residents_of(std::size_t village) const {
  return homes().at(village);
}

std::optional<FieldState> World::field_state(std::size_t structure) const {
  return villages_.field_state(structure, stages(), date().season);
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
  const std::vector<bool> fed =
      villages_.feed(population, husbandry.consumption, harvest, stages());
  std::vector<std::size_t> unfed;
  for (const Army& army : armies_) {
    for (const std::size_t member : army.roster) {
      Person& person = people_.at(member);
      if (person.health == 0 || !person.home) {
        continue;
      }
      person.hunger = fed.at(*person.home) ? 0 : person.hunger + 1;
      if (person.hunger >= husbandry.hunger_days) {
        unfed.push_back(member);
      }
    }
  }
  std::sort(unfed.begin(), unfed.end());
  for (const std::size_t person : unfed) {
    migrate_or_starve(person, population, husbandry.migration_reach);
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

// Moves person `person`, unfed too long, from its army to the residents of
// a village drawn by the seed among those within the migration reach of its
// home with room under their cap and food in store, or ends its life.
void World::migrate_or_starve(std::size_t person,
                              std::vector<std::size_t>& population,
                              std::int32_t reach) {
  Person& hungry = people_.at(person);
  const std::size_t home = hungry.home.value_or(0);
  const Cell from = seat(home);
  std::vector<std::size_t> refuges;
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    const Cell there = seat(village);
    if (village != home && std::abs(there.x - from.x) <= reach &&
        std::abs(there.y - from.y) <= reach &&
        villages_.shelters(village, population.at(village), stages(),
                           life_.husbandry, rules_.people_capacity)) {
      refuges.push_back(village);
    }
  }
  if (refuges.empty()) {
    hungry.health = 0;
    return;
  }
  const std::size_t refuge = refuges.at(next_random() % refuges.size());
  strike_off(person);
  enrol(person, villages_.at(refuge).spec.residents);
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

std::optional<std::string> World::laid_kind(std::size_t structure) const {
  const std::optional<std::size_t> plan = plans_.at(structure);
  if (!plan || !life_.growth) {
    return std::nullopt;
  }
  return life_.growth->plans.at(*plan).kind;
}

std::vector<std::optional<std::size_t>> World::territory() const {
  const Footprint size{map_.width(), map_.height()};
  if (!life_.growth) {
    return claimed(size, 0, {});
  }
  std::vector<std::pair<Cell, std::size_t>> centres;
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    if (!armies_.at(villages_.at(village).spec.residents).roster.empty()) {
      centres.emplace_back(seat(village), owner(village));
    }
  }
  return claimed(size, life_.growth->claim, centres);
}

std::vector<std::optional<std::size_t>> World::remembered_territory(
    std::size_t soul) const {
  return charts_.at(soul).territory(
      life_.growth ? std::optional<std::int32_t>{life_.growth->claim}
                   : std::nullopt);
}

// Every growth check: each village recalls a raid that is over, then, with
// living residents, lays a square kind its tiers name that it lacks or, while
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
    villages_.reach(Reached{village, tier_for(growth, people)});
    const bool peopled =
        people > 0 &&
        !armies_.at(villages_.at(village).spec.residents).roster.empty();
    if (peopled && lay_square_kind(village, growth)) {
      continue;
    }
    const bool pressed =
        peopled &&
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
  const VillageRecord& raiding = villages_.at(village);
  if (!raiding.party) {
    return;
  }
  const std::size_t party = *raiding.party;
  if (villages_.houses(party)) {
    villages_.release(village);
    return;
  }
  if (!raiding.raiding) {
    return;
  }
  const std::size_t target = *raiding.raiding;
  const Army& out = armies_.at(party);
  const bool over =
      out.roster.empty() ||
      armies_.at(villages_.at(target).spec.residents).roster.empty() ||
      !hostile(out.faction, owner(target));
  if (!over) {
    return;
  }
  villages_.end_raid(village);
  const std::vector<std::size_t> members = out.roster;
  for (const std::size_t member : members) {
    const std::optional<std::size_t> home = people_.at(member).home;
    if (!home) {
      continue;
    }
    const Army& residents = armies_.at(villages_.at(*home).spec.residents);
    // Far residents are reached from anywhere: coming back, they stand round
    // their anchor.
    const bool reached =
        far_off(residents) ||
        graph_.joined(map_, Walk{cell_of(people_.at(member).position),
                                 residents.order.objective});
    if (!reached) {
      continue;
    }
    strike_off(member);
    enrol(member, villages_.at(*home).spec.residents);
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
  for (std::size_t village = 0; village < villages_.size(); ++village) {
    std::vector<Clearing> clearings = villages_.at(village).spec.clearings;
    if (life_.growth) {
      clearings.push_back(square(village));
    }
    for (const Clearing& clearing : clearings) {
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

// Lays the first square kind of village `village`'s tier and every tier
// below it, in order, that none of its structures has at any stage and that
// has a site, while every structure of its own is intact (ADR 0082). Returns
// whether it laid one.
bool World::lay_square_kind(std::size_t village, const Growth& growth) {
  if (!intact(village)) {
    return false;
  }
  const std::vector<std::string>& kinds = villages_.at(village).spec.kinds;
  for (std::size_t tier = 0; tier <= villages_.at(village).tier; ++tier) {
    for (const std::size_t plan : growth.tiers.at(tier).square_kinds) {
      const bool held = std::find(kinds.begin(), kinds.end(),
                                  growth.plans.at(plan).kind) != kinds.end();
      if (!held && lay_plan(village, plan, growth)) {
        return true;
      }
    }
  }
  return false;
}

// Whether every structure of village `village`'s own is intact.
bool World::intact(std::size_t village) const {
  const VillageSpec& spec = villages_.at(village).spec;
  return std::all_of(spec.structures.begin(), spec.structures.end(),
                     [this](std::size_t structure) {
                       return stage(structure) == Stage::INTACT;
                     });
}

// Lays the plan of the growth's builds that most raises village `village`'s
// scarcest holding, in people, while every structure of its own is intact.
// Returns whether it laid one.
bool World::lay_building(std::size_t village, const Growth& growth) {
  if (!intact(village)) {
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
  const std::array<std::uint64_t, HOLDINGS> holds = in_people(
      villages_.held(village, Stage::DAMAGED, stages()), growth.self_reliance);
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
  return chosen && lay_plan(village, *chosen, growth);
}

// Lays plan `plan` for village `village` as rubble, placed by its role within
// its tier's vicinity on a cell clear of any structure, village's clearing or
// unbuildable cell. Returns whether it laid it.
bool World::lay_plan(std::size_t village, std::size_t plan,
                     const Growth& growth) {
  const Plan& laying = growth.plans.at(plan);
  const std::optional<Cell> best =
      place(lot(), site(village, growth), laying.footprint, laying.role,
            homes(village),
            placement_draw(seed_, village,
                           villages_.at(village).spec.structures.size()));
  if (!best) {
    return false;
  }
  const std::optional<std::vector<std::size_t>> laid =
      lay({Laid{plan, *best}}, growth, {}, seat(village));
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
  const Footprint area = growth.tiers.front().square;
  const std::size_t faction = owner(village);
  const std::vector<Cell> free = open_sites(village, growth);
  if (free.empty()) {
    return false;
  }
  const Lot ground_lot = lot();
  const std::size_t first = next_random() % free.size();
  std::optional<Cell> centre;
  std::optional<std::vector<std::size_t>> laid;
  for (std::size_t tried = 0;
       tried < std::min(free.size(), FOUNDING_TRIES) && !laid; ++tried) {
    const Cell site = free.at((first + tried) % free.size());
    const std::optional<std::vector<Laid>> layout =
        core_layout(site, growth, ground_lot);
    if (layout) {
      laid = lay(*layout, growth, settlers, site);
      centre = site;
    }
  }
  if (!laid || !centre) {
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
  const std::size_t army = form(village, settlers, *centre, std::nullopt);
  const std::size_t founded =
      villages_.found(army, square_corner(*centre, area));
  villages_.place(founded, *centre);
  attach(founded, *laid, growth);
  record(*centre, Founded{founded, faction});
  return true;
}

// The centres a village founded from village `village` may take: within
// the growth's reach of its own, its first tier's square on free buildable
// land no one stands on and `spacing` clear of anything built, reachable,
// held by no other faction, and open by `apart` to every village's centre.
std::vector<Cell> World::open_sites(std::size_t village,
                                    const Growth& growth) const {
  const Footprint area = growth.tiers.front().square;
  const Cell home = seat(village);
  const std::size_t faction = owner(village);
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
  std::vector<Cell> centres;
  centres.reserve(villages_.size());
  for (const VillageRecord& other : villages_.records()) {
    centres.push_back(other.centre);
  }
  const auto open_at = [&](Cell centre) {
    const Cell corner = square_corner(centre, area);
    const std::vector<Cell> cells = cells_of(corner, area);
    return blocked.clear(corner, area) &&
           covered.clear_around(corner, area, growth.spacing) &&
           graph_.joined(map_, Walk{home, centre}) &&
           open_site(centre, centres, growth.apart) &&
           std::none_of(cells.begin(), cells.end(), claimed);
  };
  std::vector<Cell> free;
  for (Cell centre{0, home.y - growth.reach}; centre.y <= home.y + growth.reach;
       ++centre.y) {
    for (centre.x = home.x - growth.reach; centre.x <= home.x + growth.reach;
         ++centre.x) {
      if (open_at(centre)) {
        free.push_back(centre);
      }
    }
  }
  return free;
}

// The growth's core placed by role round `centre` on `ground`, its first
// tier's square kept clear: square first, then homes, then edges, so its
// fields lie beyond its houses. Nothing when a plan finds no place.
std::optional<std::vector<World::Laid>> World::core_layout(
    Cell centre, const Growth& growth, const Lot& ground) const {
  const Tier& first = growth.tiers.front();
  std::vector<std::size_t> core = growth.core;
  std::stable_sort(
      core.begin(), core.end(), [&growth](std::size_t one, std::size_t other) {
        return growth.plans.at(one).role < growth.plans.at(other).role;
      });
  const Townsite site{centre, first.square, first.vicinity};
  Lot staked = ground;
  build_on(staked, square_corner(centre, first.square), first.square, false);
  std::vector<Laid> layout;
  std::vector<Structure> homes;
  for (const std::size_t plan : core) {
    const Plan& laying = growth.plans.at(plan);
    const std::optional<Cell> corner =
        place(staked, site, laying.footprint, laying.role, homes,
              placement_draw(seed_, villages_.size(), layout.size()));
    if (!corner) {
      return std::nullopt;
    }
    build_on(staked, *corner, laying.footprint, laying.blocks);
    if (laying.role == Role::HOME) {
      homes.push_back(
          Structure{*corner, 0, laying.footprint, laying.blocks, Role::HOME});
    }
    layout.push_back(Laid{plan, *corner});
  }
  return layout;
}

// The land village `village`'s structures are placed on: every cell as the
// ground, its structures and clearings, and those standing or bound there
// leave it.
Lot World::lot() const {
  const Ground marked = ground();
  Lot made{Footprint{map_.width(), map_.height()}, {}, {}};
  made.plots.reserve(marked.covered.size());
  made.passable.reserve(marked.covered.size());
  for (Cell cell{0, 0}; cell.y < map_.height(); ++cell.y) {
    for (cell.x = 0; cell.x < map_.width(); ++cell.x) {
      const std::size_t index = made.plots.size();
      const bool passable = map_.passable(cell);
      Plot plot = Plot::FREE;
      if (marked.covered.at(index)) {
        plot = Plot::BUILT;
      } else if (!passable) {
        plot = Plot::BLOCKED;
      } else if (marked.taken.at(index)) {
        plot = Plot::TAKEN;
      } else if (marked.rough.at(index)) {
        plot = Plot::ROUGH;
      }
      made.plots.push_back(plot);
      made.passable.push_back(passable);
    }
  }
  return made;
}

// Where village `village` grows: its centre, and its tier's square and
// vicinity.
Townsite World::site(std::size_t village, const Growth& growth) const {
  const Tier& tier = growth.tiers.at(villages_.at(village).tier);
  return Townsite{villages_.at(village).centre, tier.square, tier.vicinity};
}

// The square village `village`'s tier keeps clear round its centre.
Clearing World::square(std::size_t village) const {
  const Footprint area =
      life_.growth ? life_.growth->tiers.at(villages_.at(village).tier).square
                   : Footprint{1, 1};
  return Clearing{square_corner(villages_.at(village).centre, area), area};
}

// Village `village`'s structures whose role is a home.
std::vector<Structure> World::homes(std::size_t village) const {
  std::vector<Structure> found;
  for (const std::size_t structure : villages_.at(village).spec.structures) {
    if (structures_.at(structure).role == Role::HOME) {
      found.push_back(structures_.at(structure));
    }
  }
  return found;
}

// Sends the growth's raiders from village `village`'s residents, while no
// party of its is out, against the nearest village of a faction its owner is
// hostile to, with living residents and its centre within reach, the lowest
// among equals. Returns whether it sent them.
bool World::send_raid(std::size_t village, const Growth& growth) {
  const VillageRecord& raiding = villages_.at(village);
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
  const Cell aimed = seat(*target);
  const auto cut_off = [this, aimed](std::size_t member) {
    return !graph_.joined(map_,
                          Walk{cell_of(people_.at(member).position), aimed});
  };
  if (raiders.empty() || std::any_of(raiders.begin(), raiders.end(), cut_off)) {
    return false;
  }
  const std::size_t party =
      form(village, raiders, seat(*target), raiding.party);
  villages_.send(village, Raid{party, *target});
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
    if (people_.at(member).health > 0 && !leaders_.leads(member)) {
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
// mover unable to reach `destination`. The coarse graph is rebuilt where the
// new map changed, and every field follows it. Returns the new structures, or
// nothing when it laid none.
std::optional<std::vector<std::size_t>> World::lay(
    const std::vector<Laid>& laid, const Growth& growth,
    const std::vector<std::size_t>& movers, Cell destination) {
  Map built = map_;
  std::vector<Cell> blocked;
  for (const Laid& each : laid) {
    const Plan& plan = growth.plans.at(each.plan);
    for (const Cell cell : cells_of(each.cell, plan.footprint)) {
      if (plan.blocks) {
        built.block(cell);
        blocked.push_back(cell);
      }
    }
  }
  // The graph is rebuilt in place, and put back when the structures would
  // cut someone off.
  CoarseGraph::Undo undo = graph_.rebuild(built, blocked);
  const auto reaches = [this, &built](Cell objective,
                                      const std::vector<std::size_t>& members) {
    return std::all_of(members.begin(), members.end(), [&](std::size_t member) {
      return graph_.joined(
          built, Walk{cell_of(people_.at(member).position), objective});
    });
  };
  const bool cut_off =
      std::any_of(armies_.begin(), armies_.end(),
                  [&reaches](const Army& army) {
                    return !far_off(army) &&
                           !reaches(army.order.objective, army.roster);
                  }) ||
      (!movers.empty() && !reaches(destination, movers));
  if (cut_off) {
    graph_.revert(std::move(undo));
    return std::nullopt;
  }
  map_ = std::move(built);
  fields_.changed(map_, graph_, blocked);
  // A village whose window a structure is laid on finds its grounds again;
  // the rest read no cell the structure covers.
  for (std::size_t village = 0; village < grounds_.size(); ++village) {
    const Grounds& grounds = grounds_.at(village);
    if (grounds.version != 0 &&
        std::any_of(laid.begin(), laid.end(), [&](const Laid& each) {
          return grounds.fields.overlaps(each.cell,
                                         growth.plans.at(each.plan).footprint);
        })) {
      bump_layout(village);
    }
  }
  std::vector<std::size_t> made;
  for (const Laid& each : laid) {
    const Plan& plan = growth.plans.at(each.plan);
    made.push_back(structures_.size());
    structures_.push_back(
        Structure{each.cell, 0, plan.footprint, plan.blocks, plan.role});
    villages_.add_structure();
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
  for (const std::size_t structure : structures) {
    const Plan& plan = growth.plans.at(plans_.at(structure).value_or(0));
    villages_.attach(village, structure, plan.holding, plan.kind);
  }
}

// Moves `members` off village `village`'s residents into army `into`, or a
// new army when none is given, of the residents' faction and skills, bound
// for `objective` with all of them. Returns the army.
std::size_t World::form(std::size_t village,
                        const std::vector<std::size_t>& members, Cell objective,
                        std::optional<std::size_t> into) {
  const std::size_t from = villages_.at(village).spec.residents;
  for (const std::size_t member : members) {
    strike_off(member);
  }
  std::vector<std::size_t> every_skill(drilled_skills(armies_.at(from)));
  std::iota(every_skill.begin(), every_skill.end(), std::size_t{0});
  Army formed{armies_.at(from).faction,
              armies_.at(from).skills,
              Order{objective, std::move(every_skill)},
              Bound{},
              {},
              false};
  if (into) {
    refit(*into, std::move(formed));
  } else {
    armies_.push_back(std::move(formed));
  }
  const std::size_t army = into.value_or(armies_.size() - 1);
  for (const std::size_t member : members) {
    enrol(member, army);
  }
  return army;
}

}  // namespace murim
