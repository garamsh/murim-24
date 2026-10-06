#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"

namespace murim {

/// Seed a world is created from; the same seed gives the same world.
enum class Seed : std::uint64_t {};

/// Digest of a world's whole state; equal states give equal hashes.
enum class StateHash : std::uint64_t {};

/// Part of an army: one agent, or the aggregate standing for all of them.
struct Body {
  Position position;
  std::uint32_t count;
  std::uint64_t strength;
};

/// An army as a world is created with: one aggregate at `start`, bound for
/// `objective`.
struct ArmySpec {
  Cell start;
  Cell objective;
  std::uint32_t count;
  std::uint64_t strength;
};

/// A level of detail. An army whose lead body lies within `reach` sub-cell
/// units of the focus (Chebyshev distance) moves every `interval` ticks, as
/// at most `max_bodies` bodies.
struct Tier {
  std::int64_t reach;
  std::uint64_t interval;
  std::uint32_t max_bodies;
};

/// The tiers from the focus outward: near, middle and far.
constexpr std::array<Tier, 3> TIERS{{
    {std::int64_t{8} * CELL_SIZE, 1, std::numeric_limits<std::uint32_t>::max()},
    {std::int64_t{32} * CELL_SIZE, 4,
     std::numeric_limits<std::uint32_t>::max()},
    {std::numeric_limits<std::int64_t>::max(), 16, 1},
}};

/// Sub-cell units a body moves per tick; a tier moving every `interval` ticks
/// moves `interval` times as far at once.
constexpr std::int32_t SPEED = 16;

/// The simulated world, advanced one deterministic step at a time.
class World {
 public:
  /// Creates a world whose bodies keep `separation` sub-cell units apart
  /// while they move; 0 lets them overlap. Throws std::invalid_argument when
  /// `separation` is negative, or an army is empty or starts or ends on a
  /// cell that is not passable.
  World(Seed seed, Map map, const std::vector<ArmySpec>& armies,
        std::int32_t separation);

  /// Places the point the tiers are measured from.
  void set_focus(Position focus);

  /// Advances the world by one step.
  void tick();

  /// Returns the hash of the current state.
  [[nodiscard]] StateHash hash() const;

  /// Returns the bodies of army `army`, numbered in creation order; the
  /// first is its lead body.
  [[nodiscard]] const std::vector<Body>& bodies(std::size_t army) const;

 private:
  struct Army {
    Cell objective;
    FlowField field;
    std::vector<Body> bodies;
  };

  std::uint64_t rng_state_;
  std::uint64_t tick_count_ = 0;
  Map map_;
  std::int32_t separation_;
  Position focus_{0, 0};
  std::vector<Army> armies_;
};

}  // namespace murim
