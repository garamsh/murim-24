#include "world.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
// Pi as 355/113 (Zu Chongzhi), within 3e-7 of it, for areas in integers.
constexpr std::uint64_t PI_NUMERATOR = 355;
constexpr std::uint64_t PI_DENOMINATOR = 113;

// A move crosses at most one cell border per axis, so the flow field vets
// every cell a body enters.
constexpr bool strides_fit_a_cell() {
  bool fit = true;
  for (const Tier& tier : TIERS) {
    fit = fit && SPEED * tier.interval <= CELL_SIZE;
  }
  return fit;
}
static_assert(strides_fit_a_cell(), "a tier's stride must not exceed a cell");
static_assert(TIERS.back().reach == std::numeric_limits<std::int64_t>::max(),
              "the far tier must reach every body");

// The most uses one strike can make: one per tick of the longest step.
constexpr std::uint64_t longest_interval() {
  std::uint64_t longest = 0;
  for (const Tier& tier : TIERS) {
    longest = std::max(longest, tier.interval);
  }
  return longest;
}
constexpr std::uint64_t MAX_USES = longest_interval();
constexpr std::uint64_t UINT64_LIMIT =
    std::numeric_limits<std::uint64_t>::max();

// The damage a body takes in one tick is at most every agent striking, each
// with every use, at every agent of the body struck.
static_assert(MAX_BODY_DAMAGE <=
                  UINT64_LIMIT / MAX_AGENTS / MAX_USES / MAX_AGENTS,
              "the damage a body takes in a tick must fit 64 bits");
static_assert(std::numeric_limits<std::uint32_t>::max() <=
                  UINT64_LIMIT / MAX_AGENTS / MAX_USES,
              "the damage a structure takes in a strike must fit 64 bits");
// wound() multiplies count by strength and adds strength to round up.
static_assert(MAX_STRENGTH <= (UINT64_LIMIT - MAX_STRENGTH) / MAX_AGENTS,
              "a body's count times its strength must fit 64 bits");
// A cooldown leaves half the range to the tick count it is added to.
static_assert(MAX_COOLDOWN <= UINT64_LIMIT / 2 / MAX_USES,
              "a step's cooldowns must fit beside the tick count");

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

// Blocks every structure's footprint on `map`. Returns false at the first
// footprint of a size outside [1, MAX_FOOTPRINT] or covering a cell that is
// not passable, which includes one an earlier structure covers.
bool block_footprints(Map& map, const std::vector<Structure>& structures) {
  const auto spans = [](std::int32_t side) {
    return side >= 1 && side <= MAX_FOOTPRINT;
  };
  for (const Structure& structure : structures) {
    if (!spans(structure.footprint.width) ||
        !spans(structure.footprint.height)) {
      return false;
    }
    for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
      if (!map.passable(cell)) {
        return false;
      }
      map.block(cell);
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

const Tier& tier_at(Position focus, Position lead) {
  const std::int64_t distance =
      std::max(std::abs(std::int64_t{lead.x} - focus.x),
               std::abs(std::int64_t{lead.y} - focus.y));
  return *std::find_if(
      TIERS.begin(), TIERS.end(),
      [distance](const Tier& tier) { return distance <= tier.reach; });
}

// Moves one axis at a time, and only where the move does not lengthen the
// path to the objective, so no body enters a blocked cell or backs away.
Position advance(const FlowField& field, Position from, Step direction,
                 std::int32_t stride) {
  const std::uint32_t limit = field.distance(cell_of(from));
  const Position along_x{from.x + (direction.x * stride), from.y};
  const Position moved_x =
      field.distance(cell_of(along_x)) <= limit ? along_x : from;
  const Position along_y{moved_x.x, moved_x.y + (direction.y * stride)};
  return field.distance(cell_of(along_y)) <= limit ? along_y : moved_x;
}

// Reads every neighbour from `crowd`, the positions as the tick began, so the
// order bodies move in does not change where they go.
void march(std::vector<Body>& bodies, std::int32_t stride,
           const FlowField& field, const SpatialHash& crowd,
           std::int32_t separation) {
  for (Body& body : bodies) {
    std::int64_t away_x = 0;
    std::int64_t away_y = 0;
    crowd.visit_within(body.position, separation, [&](std::size_t other) {
      away_x += body.position.x - crowd.position(other).x;
      away_y += body.position.y - crowd.position(other).y;
    });
    // A body crowded by neighbours steps away from them; any other follows
    // the flow field.
    const Step direction = away_x != 0 || away_y != 0
                               ? Step{sign(away_x), sign(away_y)}
                               : field.step(cell_of(body.position));
    body.position = advance(field, body.position, direction, stride);
  }
}

// Lowers strength by `damage`, and count in proportion, rounded up: a body
// with strength left keeps an agent, and none once its strength is gone.
void wound(Body& body, std::uint64_t damage) {
  const std::uint64_t before = std::max<std::uint64_t>(body.strength, 1);
  body.strength -= std::min(damage, body.strength);
  body.count = static_cast<std::uint32_t>(
      ((std::uint64_t{body.count} * body.strength) + before - 1) / before);
}

// Drops dead bodies, keeping one so an army always has a lead, and moves
// living ones to the front. Then, unless the living bodies already number
// what the tier allows, merges them into their totals and splits those into
// at most `max_bodies` bodies on a lattice across the lead body's cell. Count
// and strength are divided with the remainder spread one unit at a time, so
// the totals carry over exactly; every new body waits for the latest
// cooldown among the old ones.
void regroup(std::vector<Body>& bodies, std::uint32_t max_bodies) {
  const auto living =
      std::stable_partition(bodies.begin(), bodies.end(),
                            [](const Body& body) { return body.count > 0; });
  bodies.erase(std::max(living, bodies.begin() + 1), bodies.end());
  std::uint32_t count = 0;
  std::uint64_t strength = 0;
  std::uint64_t ready_at = 0;
  for (const Body& body : bodies) {
    count += body.count;
    strength += body.strength;
    ready_at = std::max(ready_at, body.ready_at);
  }
  const std::uint32_t pieces = std::max(1U, std::min(count, max_bodies));
  if (pieces == bodies.size()) {
    return;
  }
  std::uint32_t side = 1;
  while (std::uint64_t{side} * side < pieces) {
    ++side;
  }
  const std::int32_t spacing = CELL_SIZE / static_cast<std::int32_t>(side);
  const Cell cell = cell_of(bodies.front().position);
  const Position corner{cell.x * CELL_SIZE, cell.y * CELL_SIZE};
  bodies.clear();
  for (std::uint32_t piece = 0; piece < pieces; ++piece) {
    const auto column = static_cast<std::int32_t>(piece % side);
    const auto row = static_cast<std::int32_t>(piece / side);
    bodies.push_back(Body{
        Position{corner.x + (column * spacing) + (spacing / 2),
                 corner.y + (row * spacing) + (spacing / 2)},
        (count / pieces) + static_cast<std::uint32_t>(piece < count % pieces),
        (strength / pieces) +
            static_cast<std::uint64_t>(piece < strength % pieces),
        ready_at});
  }
}

}  // namespace

World::World(Seed seed, Map map, std::vector<ArmySpec> armies,
             std::vector<Structure> structures, Rules rules)
    : rng_state_(static_cast<std::uint64_t>(seed)),
      map_(std::move(map)),
      rules_(std::move(rules)),
      structures_(std::move(structures)),
      structure_index_(centres_of(structures_)) {
  const std::size_t factions = rules_.hostile.size();
  const auto ragged = [factions](const std::vector<bool>& row) {
    return row.size() != factions;
  };
  const auto invalid_skill = [](const Skill& skill) {
    return !reachable(skill.range) || !reachable(skill.radius) ||
           skill.body_damage > MAX_BODY_DAMAGE || skill.cooldown > MAX_COOLDOWN;
  };
  const auto invalid_army = [this, factions,
                             invalid_skill](const ArmySpec& army) {
    return army.count == 0 || army.strength < army.count ||
           army.strength > MAX_STRENGTH || army.skills.empty() ||
           army.faction >= factions || !map_.passable(army.start) ||
           !map_.passable(army.objective) ||
           std::any_of(army.skills.begin(), army.skills.end(), invalid_skill);
  };
  // Footprints block their cells before any army is checked against them.
  const bool placed = block_footprints(map_, structures_);
  std::uint64_t agents = 0;
  for (const ArmySpec& army : armies) {
    agents += army.count;
  }
  if (!reachable(rules_.separation) || agents > MAX_AGENTS ||
      std::any_of(rules_.hostile.begin(), rules_.hostile.end(), ragged) ||
      !placed || std::any_of(armies.begin(), armies.end(), invalid_army)) {
    throw std::invalid_argument(
        "separation, range and radius must lie in [0, MAX_REACH], hostility "
        "must be square, armies must hold at most MAX_AGENTS agents together, "
        "and every army must hold agents, strength up to MAX_STRENGTH and "
        "skills within MAX_BODY_DAMAGE and MAX_COOLDOWN of a known faction "
        "and stand on passable cells, and every structure must cover 1 to "
        "MAX_FOOTPRINT cells along each side, all passable and covered by no "
        "other structure");
  }
  for (const Structure& structure : structures_) {
    structure_extent_ =
        std::max({structure_extent_, structure.footprint.width * CELL_SIZE,
                  structure.footprint.height * CELL_SIZE});
  }
  for (ArmySpec& army : armies) {
    std::vector<std::size_t> every_skill(army.skills.size());
    std::iota(every_skill.begin(), every_skill.end(), std::size_t{0});
    armies_.push_back(
        Army{armies_.size(),
             army.faction,
             std::move(army.skills),
             Order{army.objective, std::move(every_skill)},
             FlowField{map_, army.objective},
             {Body{centre_of(army.start), army.count, army.strength, 0}}});
    if (armies_.back().field.distance(army.start) == FlowField::UNREACHABLE) {
      throw std::invalid_argument(
          "every army's objective must be reachable from its start");
    }
  }
}

void World::set_focus(Position focus) { focus_ = focus; }

void World::order(std::size_t army, Order order) {
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
  if (field.distance(cell_of(ordered.bodies.front().position)) ==
      FlowField::UNREACHABLE) {
    throw std::invalid_argument(
        "an order's objective must be reachable from where the army stands");
  }
  ordered.field = std::move(field);
  ordered.order = std::move(order);
}

void World::tick() {
  rng_state_ += SPLITMIX_INCREMENT;
  ++tick_count_;
  std::vector<std::uint64_t> intervals;
  std::vector<Position> positions;
  std::vector<std::size_t> owners;
  std::vector<std::uint32_t> counts;
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    std::vector<Body>& bodies = armies_.at(army).bodies;
    const Tier& tier = tier_at(focus_, bodies.front().position);
    regroup(bodies, tier.max_bodies);
    intervals.push_back(tier.interval);
    for (const Body& body : bodies) {
      positions.push_back(body.position);
      owners.push_back(army);
      counts.push_back(body.count);
    }
  }
  const Crowd crowd{SpatialHash{std::move(positions)}, std::move(owners),
                    std::move(counts), std::move(intervals)};
  std::vector<std::uint64_t> damage(crowd.armies.size(), 0);
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (Body& body : armies_.at(army).bodies) {
      if (tick_count_ % crowd.intervals.at(army) == 0 &&
          body.ready_at <= tick_count_) {
        strike(body, army, crowd, damage);
      }
    }
  }
  // Damage lands after every body has struck, so the order bodies strike in
  // does not decide who strikes.
  std::size_t index = 0;
  for (Army& army : armies_) {
    for (Body& body : army.bodies) {
      wound(body, damage.at(index++));
    }
  }
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    // Each army marches at its own phase within the interval, so a tier's
    // movement spreads over its ticks; strikes keep the tier's common tick,
    // where every army's step lands at once.
    const std::uint64_t interval = crowd.intervals.at(army);
    if ((tick_count_ + armies_.at(army).phase) % interval == 0) {
      march(armies_.at(army).bodies,
            SPEED * static_cast<std::int32_t>(interval), armies_.at(army).field,
            crowd.hash, rules_.separation);
    }
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
  fold(focus_.x);
  fold(focus_.y);
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
  }
  for (const Army& army : armies_) {
    fold(army.phase);
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
    fold(army.bodies.size());
    for (const Body& body : army.bodies) {
      fold(body.position.x);
      fold(body.position.y);
      fold(body.count);
      fold(body.strength);
      fold(body.ready_at);
    }
  }
  return StateHash{digest};
}

const std::vector<Body>& World::bodies(std::size_t army) const {
  return armies_.at(army).bodies;
}

const Structure& World::structure(std::size_t structure) const {
  return structures_.at(structure);
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

// The one rule every skill resolves through, whoever uses it. An agent, an
// aggregate and the player's body differ only in how many agents they stand
// for, and every tier in how many uses fall in one of its steps, so a battle
// comes out the same wherever the focus is.
void World::strike(Body& user, std::size_t army, const Crowd& crowd,
                   std::vector<std::uint64_t>& damage) {
  const Army& own = armies_.at(army);
  const std::vector<std::size_t>& choice = own.order.skills;
  const Skill& skill = own.skills.at(choice.at(next_random() % choice.size()));
  const auto hostile = [this, &own, &crowd](std::size_t other) {
    return rules_.hostile.at(own.faction)
        .at(armies_.at(crowd.armies.at(other)).faction);
  };
  // The nearest living hostile body, the lowest index among equals, so the
  // order the query visits bodies in does not decide the target.
  std::optional<std::size_t> target;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  crowd.hash.visit_within(user.position, skill.range, [&](std::size_t other) {
    const std::int64_t distance =
        distance_squared(user.position, crowd.hash.position(other));
    const bool closer = distance < nearest ||
                        (distance == nearest && target && other < *target);
    if (crowd.counts.at(other) > 0 && hostile(other) && closer) {
      nearest = distance;
      target = other;
    }
  });
  if (!target) {
    return;
  }
  // Uses the step holds: one per cooldown since the later of the user's
  // readiness and the start of its tier's step, so a body acting every 16
  // ticks uses a skill as often as one acting every tick.
  const std::uint64_t cooldown = std::max<std::uint64_t>(skill.cooldown, 1);
  const std::uint64_t interval = crowd.intervals.at(army);
  const std::uint64_t since =
      std::max(user.ready_at, tick_count_ + 1 - interval);
  const std::uint64_t uses = ((tick_count_ - since) / cooldown) + 1;
  user.ready_at = since + (uses * cooldown);
  const std::uint64_t force = std::uint64_t{user.count} * uses;
  // Agents the disc covers, with one agent on every separation square: its
  // area over a square's, pi taken as 355/113. A body takes the damage once
  // for each of its agents the disc covers, so an agent takes it once.
  const auto side = static_cast<std::uint64_t>(std::max(rules_.separation, 1));
  const auto reach = static_cast<std::uint64_t>(skill.radius);
  const std::uint64_t covered =
      1 + ((PI_NUMERATOR * reach * reach) / (PI_DENOMINATOR * side * side));
  const Position centre = crowd.hash.position(*target);
  crowd.hash.visit_within(centre, skill.radius, [&](std::size_t other) {
    if (hostile(other)) {
      damage.at(other) +=
          skill.body_damage * force *
          std::min<std::uint64_t>(crowd.counts.at(other), covered);
    }
  });
  const std::uint64_t broken = std::uint64_t{skill.structure_damage} * force;
  structure_index_.visit_within(
      centre, skill.radius + structure_extent_, [&](std::size_t index) {
        Structure& structure = structures_.at(index);
        if (reaches(structure, centre, skill.radius)) {
          structure.durability -= static_cast<std::uint32_t>(
              std::min<std::uint64_t>(broken, structure.durability));
        }
      });
}

}  // namespace murim
