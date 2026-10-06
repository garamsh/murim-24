#include "world.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
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
    for (const std::size_t other : crowd.within(body.position, separation)) {
      away_x += body.position.x - crowd.position(other).x;
      away_y += body.position.y - crowd.position(other).y;
    }
    // A body crowded by neighbours steps away from them; any other follows
    // the flow field.
    const Step direction = away_x != 0 || away_y != 0
                               ? Step{sign(away_x), sign(away_y)}
                               : field.step(cell_of(body.position));
    body.position = advance(field, body.position, direction, stride);
  }
}

// Lowers strength by `damage`, and count in proportion, rounded up so a body
// keeps one agent per unit of strength at least and none once it has none.
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
      structures_(std::move(structures)) {
  const std::size_t factions = rules_.hostile.size();
  const auto ragged = [factions](const std::vector<bool>& row) {
    return row.size() != factions;
  };
  const auto invalid_army = [this, factions](const ArmySpec& army) {
    return army.count == 0 || army.strength < army.count ||
           army.skills.empty() || army.faction >= factions ||
           !map_.passable(army.start) || !map_.passable(army.objective);
  };
  const auto invalid_structure = [this](const Structure& structure) {
    return !map_.passable(structure.cell);
  };
  if (rules_.separation < 0 ||
      std::any_of(rules_.hostile.begin(), rules_.hostile.end(), ragged) ||
      std::any_of(armies.begin(), armies.end(), invalid_army) ||
      std::any_of(structures_.begin(), structures_.end(), invalid_structure)) {
    throw std::invalid_argument(
        "the separation must not be negative, hostility must be square, and "
        "every army must hold agents, strength and skills of a known faction "
        "and stand, like every structure, on passable cells");
  }
  for (ArmySpec& army : armies) {
    armies_.push_back(
        Army{army.objective,
             army.faction,
             std::move(army.skills),
             FlowField{map_, army.objective},
             {Body{centre_of(army.start), army.count, army.strength, 0}}});
  }
}

void World::set_focus(Position focus) { focus_ = focus; }

void World::tick() {
  rng_state_ += SPLITMIX_INCREMENT;
  ++tick_count_;
  std::vector<std::uint64_t> intervals;
  std::vector<Position> positions;
  std::vector<std::size_t> owners;
  std::vector<bool> alive;
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    std::vector<Body>& bodies = armies_.at(army).bodies;
    const Tier& tier = tier_at(focus_, bodies.front().position);
    regroup(bodies, tier.max_bodies);
    intervals.push_back(tier.interval);
    for (const Body& body : bodies) {
      positions.push_back(body.position);
      owners.push_back(army);
      alive.push_back(body.strength > 0);
    }
  }
  const Crowd crowd{SpatialHash{std::move(positions)}, std::move(owners),
                    std::move(alive)};
  std::vector<std::uint64_t> damage(crowd.armies.size(), 0);
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    for (Body& body : armies_.at(army).bodies) {
      if (tick_count_ % intervals.at(army) == 0 &&
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
    const std::uint64_t interval = intervals.at(army);
    if (tick_count_ % interval == 0) {
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
    fold(structure.durability);
  }
  for (const Army& army : armies_) {
    fold(army.objective.x);
    fold(army.objective.y);
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

// The one rule every skill resolves through, whoever uses it: an agent, an
// aggregate or the player's body differ only in `user.count`.
void World::strike(Body& user, std::size_t army, const Crowd& crowd,
                   std::vector<std::uint64_t>& damage) {
  const Army& own = armies_.at(army);
  const Skill& skill = own.skills.at(next_random() % own.skills.size());
  const auto hostile = [this, &own, &crowd](std::size_t other) {
    return rules_.hostile.at(own.faction)
        .at(armies_.at(crowd.armies.at(other)).faction);
  };
  std::optional<std::size_t> target;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (const std::size_t other :
       crowd.hash.within(user.position, skill.range)) {
    const std::int64_t distance =
        distance_squared(user.position, crowd.hash.position(other));
    if (crowd.alive.at(other) && hostile(other) && distance < nearest) {
      nearest = distance;
      target = other;
    }
  }
  if (!target) {
    return;
  }
  const Position centre = crowd.hash.position(*target);
  for (const std::size_t other : crowd.hash.within(centre, skill.radius)) {
    if (hostile(other)) {
      damage.at(other) += skill.body_damage * user.count;
    }
  }
  const std::uint64_t broken =
      std::uint64_t{skill.structure_damage} * user.count;
  const std::int64_t radius_squared = std::int64_t{skill.radius} * skill.radius;
  for (Structure& structure : structures_) {
    if (distance_squared(centre, centre_of(structure.cell)) <= radius_squared) {
      structure.durability -= static_cast<std::uint32_t>(
          std::min<std::uint64_t>(broken, structure.durability));
    }
  }
  user.ready_at = tick_count_ + skill.cooldown;
}

}  // namespace murim
