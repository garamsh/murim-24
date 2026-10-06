#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "spatial_hash.hpp"

namespace murim {

/// Seed a world is created from; the same seed gives the same world.
enum class Seed : std::uint64_t {};

/// Digest of a world's whole state; equal states give equal hashes.
enum class StateHash : std::uint64_t {};

/// Part of an army: one agent, or the aggregate standing for all of them.
/// It may use a skill again from tick `ready_at`.
struct Body {
  Position position;
  std::uint32_t count;
  std::uint64_t strength;
  std::uint64_t ready_at;
};

/// An ability as data. A body uses it on the nearest living hostile body
/// within `range`; every hostile body within `radius` of that target, and
/// every structure whose cell centre lies there, takes the damage. The user
/// waits `cooldown` ticks between uses.
struct Skill {
  std::int32_t range;
  std::int32_t radius;
  std::uint64_t body_damage;
  std::uint32_t structure_damage;
  std::uint64_t cooldown;
};

/// An army as a world is created with: one aggregate at `start`, bound for
/// `objective`, of `faction`, choosing among `skills` by the world's seed.
struct ArmySpec {
  Cell start;
  Cell objective;
  std::uint32_t count;
  std::uint64_t strength;
  std::size_t faction;
  std::vector<Skill> skills;
};

/// What an army does: march toward `objective`, and at each strike use one
/// of `skills`, indices into the army's own skills, drawn by the seed. An
/// army created from an ArmySpec follows its objective with all its skills
/// until it is given another order.
struct Order {
  Cell objective;
  std::vector<std::size_t> skills;
};

/// A breakable object standing on a map cell.
struct Structure {
  Cell cell;
  std::uint32_t durability;
};

/// How broken a structure is, from durability by the floors in Rules.
enum class Stage : std::uint8_t { INTACT, DAMAGED, DESTROYED, RUBBLE };

/// The largest range, radius and separation a world accepts, in sub-cell
/// units; it keeps the products of their squares within 64 bits.
constexpr std::int32_t MAX_REACH = std::int32_t{1} << 26;

/// The most agents all of a world's armies may hold together.
constexpr std::uint64_t MAX_AGENTS = std::uint64_t{1} << 20;

/// The most strength one army may hold.
constexpr std::uint64_t MAX_STRENGTH = std::uint64_t{1} << 43;

/// The most damage a skill may do to a body.
constexpr std::uint64_t MAX_BODY_DAMAGE = std::uint64_t{1} << 19;

/// The longest cooldown a skill may have, in ticks.
constexpr std::uint64_t MAX_COOLDOWN = std::uint64_t{1} << 32;

/// The data a world's rules read.
struct Rules {
  /// Sub-cell units moving bodies keep apart; 0 lets them overlap. It is
  /// also the side of the square an agent of an aggregate stands on.
  std::int32_t separation;
  /// `hostile[a][b]`: whether bodies of faction `a` attack those of `b`.
  std::vector<std::vector<bool>> hostile;
  /// The lowest durability of the intact, damaged and destroyed stages; a
  /// structure below all three is rubble.
  std::array<std::uint32_t, 3> stage_floors;
};

/// A level of detail. An army whose lead body lies within `reach` sub-cell
/// units of the focus (Chebyshev distance) moves and strikes every
/// `interval` ticks, as at most `max_bodies` bodies. It strikes on the ticks
/// the interval divides, with every army of its tier, and moves at its own
/// phase within the interval, set by its index, so a tier's armies do not
/// all move on one tick.
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
  /// Throws std::invalid_argument when the separation, or a skill's range or
  /// radius, lies outside [0, MAX_REACH], `hostile` is not square, an army
  /// is empty, has less strength than agents or more than MAX_STRENGTH, no
  /// skills or a faction outside `hostile`, the armies together hold more
  /// than MAX_AGENTS agents, a skill does more than MAX_BODY_DAMAGE to a body
  /// or waits longer than MAX_COOLDOWN, or an army or a structure stands on a
  /// cell that is not passable. Within these limits no damage, count,
  /// strength, durability or cooldown arithmetic exceeds its type; damage to
  /// structures and durability need no limit below their 32-bit types.
  World(Seed seed, Map map, std::vector<ArmySpec> armies,
        std::vector<Structure> structures, Rules rules);

  /// Places the point the tiers are measured from.
  void set_focus(Position focus);

  /// Gives army `army` an order, which it follows from the next tick on.
  /// Throws std::invalid_argument when there is no such army, the objective
  /// is not passable, or the skills are empty or name one the army lacks.
  void order(std::size_t army, Order order);

  /// Advances the world by one step.
  void tick();

  /// Returns the hash of the current state.
  [[nodiscard]] StateHash hash() const;

  /// Returns the bodies of army `army`, numbered in creation order; the
  /// first is its lead body.
  [[nodiscard]] const std::vector<Body>& bodies(std::size_t army) const;

  /// Returns structure `structure`, numbered in creation order.
  [[nodiscard]] const Structure& structure(std::size_t structure) const;

  /// Returns the stage structure `structure` is in.
  [[nodiscard]] Stage stage(std::size_t structure) const;

 private:
  struct Army {
    std::uint64_t phase;
    std::size_t faction;
    std::vector<Skill> skills;
    Order order;
    FlowField field;
    std::vector<Body> bodies;
  };

  /// Every body as the tick began, indexed together, with its army, its
  /// count, and its army's tier interval.
  struct Crowd {
    SpatialHash hash;
    std::vector<std::size_t> armies;
    std::vector<std::uint32_t> counts;
    std::vector<std::uint64_t> intervals;
  };

  [[nodiscard]] std::uint64_t next_random();
  void strike(Body& user, std::size_t army, const Crowd& crowd,
              std::vector<std::uint64_t>& damage);

  std::uint64_t rng_state_;
  std::uint64_t tick_count_ = 0;
  Map map_;
  Rules rules_;
  Position focus_{0, 0};
  std::vector<Army> armies_;
  std::vector<Structure> structures_;
  SpatialHash structure_index_;
};

}  // namespace murim
