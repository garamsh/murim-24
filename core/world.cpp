#include "world.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
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
void march(std::vector<Body>& bodies, const FlowField& field,
           const SpatialHash& crowd, std::size_t first, std::int32_t stride) {
  for (std::size_t index = 0; index < bodies.size(); ++index) {
    Body& body = bodies.at(index);
    std::int64_t away_x = 0;
    std::int64_t away_y = 0;
    for (const std::size_t other : crowd.neighbours(first + index)) {
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

// Merges the bodies into their totals and splits them into `max_bodies`
// bodies at most, laid on a lattice across the lead body's cell. Count and
// strength are divided with the remainder spread one unit at a time, so the
// totals carry over exactly.
void regroup(std::vector<Body>& bodies, std::uint32_t max_bodies) {
  std::uint32_t count = 0;
  std::uint64_t strength = 0;
  for (const Body& body : bodies) {
    count += body.count;
    strength += body.strength;
  }
  const std::uint32_t pieces = std::min(count, max_bodies);
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
            static_cast<std::uint64_t>(piece < strength % pieces)});
  }
}

}  // namespace

World::World(Seed seed, Map map, const std::vector<ArmySpec>& armies,
             std::int32_t separation)
    : rng_state_(static_cast<std::uint64_t>(seed)),
      map_(std::move(map)),
      separation_(separation) {
  const auto invalid = [this](const ArmySpec& army) {
    return army.count == 0 || !map_.passable(army.start) ||
           !map_.passable(army.objective);
  };
  if (separation < 0 || std::any_of(armies.begin(), armies.end(), invalid)) {
    throw std::invalid_argument(
        "separation must not be negative, and every army must hold agents "
        "and start and end on passable cells");
  }
  for (const ArmySpec& army : armies) {
    armies_.push_back(
        Army{army.objective,
             FlowField{map_, army.objective},
             {Body{centre_of(army.start), army.count, army.strength}}});
  }
}

void World::set_focus(Position focus) { focus_ = focus; }

void World::tick() {
  rng_state_ += SPLITMIX_INCREMENT;
  ++tick_count_;
  std::vector<Position> positions;
  for (const Army& army : armies_) {
    for (const Body& body : army.bodies) {
      positions.push_back(body.position);
    }
  }
  const SpatialHash crowd{std::move(positions), separation_};
  std::size_t first = 0;
  for (Army& army : armies_) {
    const Tier& tier = tier_at(focus_, army.bodies.front().position);
    if (tick_count_ % tier.interval == 0) {
      march(army.bodies, army.field, crowd, first,
            SPEED * static_cast<std::int32_t>(tier.interval));
    }
    first += army.bodies.size();
    regroup(army.bodies, tier.max_bodies);
  }
}

StateHash World::hash() const {
  std::uint64_t digest = mix(rng_state_ ^ mix(tick_count_));
  const auto fold = [&digest](auto value) {
    digest = mix(digest ^ static_cast<std::uint64_t>(value));
  };
  fold(separation_);
  fold(focus_.x);
  fold(focus_.y);
  for (Cell cell{0, 0}; cell.y < map_.height(); ++cell.y) {
    for (cell.x = 0; cell.x < map_.width(); ++cell.x) {
      fold(map_.passable(cell));
    }
  }
  for (const Army& army : armies_) {
    fold(army.objective.x);
    fold(army.objective.y);
    fold(army.bodies.size());
    for (const Body& body : army.bodies) {
      fold(body.position.x);
      fold(body.position.y);
      fold(body.count);
      fold(body.strength);
    }
  }
  return StateHash{digest};
}

const std::vector<Body>& World::bodies(std::size_t army) const {
  return armies_.at(army).bodies;
}

}  // namespace murim
