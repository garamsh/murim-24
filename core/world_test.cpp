#include "world.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr int TICKS = 10'000;
constexpr int ARRIVAL_TICKS = 3'000;
constexpr std::int32_t SEPARATION = 72;
constexpr std::int32_t FAR_AWAY = 1'000'000;
constexpr std::size_t OPEN_SIDE = 16;

struct Totals {
  std::uint64_t count;
  std::uint64_t strength;
};

Totals totals(const std::vector<Body>& bodies) {
  Totals sum{0, 0};
  for (const Body& body : bodies) {
    sum.count += body.count;
    sum.strength += body.strength;
  }
  return sum;
}

bool operator==(const Totals& left, const Totals& right) {
  return left.count == right.count && left.strength == right.strength;
}

bool in(Cell cell, Position position) {
  const Cell holding = cell_of(position);
  return holding.x == cell.x && holding.y == cell.y;
}

std::int64_t closest_pair_squared(const std::vector<Body>& bodies) {
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  for (std::size_t i = 0; i < bodies.size(); ++i) {
    for (std::size_t j = i + 1; j < bodies.size(); ++j) {
      const std::int64_t offset_x =
          std::int64_t{bodies.at(i).position.x} - bodies.at(j).position.x;
      const std::int64_t offset_y =
          std::int64_t{bodies.at(i).position.y} - bodies.at(j).position.y;
      closest =
          std::min(closest, (offset_x * offset_x) + (offset_y * offset_y));
    }
  }
  return closest;
}

std::vector<std::string> open_map(std::size_t side) {
  return {side, std::string(side, '.')};
}

// A 16 by 9 map with a wall down column 8 and one gap at the bottom row.
std::vector<std::string> walled_map() {
  return {
      "........#.......", "........#.......", "........#.......",
      "........#.......", "........#.......", "........#.......",
      "........#.......", "........#.......", "................",
  };
}

// A 48 by 48 map split by a wall down column 24 with gaps at rows 10 and 37.
std::vector<std::string> crossing_map() {
  constexpr std::size_t SIDE = 48;
  constexpr std::size_t UPPER_GAP = 10;
  constexpr std::size_t LOWER_GAP = 37;
  std::vector<std::string> rows = open_map(SIDE);
  for (std::size_t row = 0; row < SIDE; ++row) {
    rows.at(row).at(SIDE / 2) =
        row == UPPER_GAP || row == LOWER_GAP ? '.' : '#';
  }
  return rows;
}

// Four armies headed for the opposite corners of crossing_map().
std::vector<ArmySpec> crossing_armies() {
  const std::vector<ArmySpec> armies{
      {{1, 1}, {46, 46}, 12, 120},
      {{46, 1}, {1, 46}, 30, 301},
      {{1, 46}, {46, 1}, 5, 50},
      {{46, 46}, {1, 1}, 2, 7},
  };
  return armies;
}

// Runs crossing_armies() for TICKS ticks while the focus visits each corner
// for FOCUS_DWELL ticks in turn.
StateHash run_crossing(Seed seed) {
  constexpr int FOCUS_DWELL = 500;
  const std::vector<ArmySpec> armies = crossing_armies();
  World world{seed, Map{crossing_map()}, armies, SEPARATION};
  std::vector<bool> expanded(armies.size(), false);
  std::vector<bool> collapsed(armies.size(), false);
  for (int i = 0; i < TICKS; ++i) {
    const auto corner =
        static_cast<std::size_t>(i / FOCUS_DWELL) % armies.size();
    world.set_focus(centre_of(armies.at(corner).start));
    world.tick();
    for (std::size_t army = 0; army < armies.size(); ++army) {
      const std::size_t size = world.bodies(army).size();
      expanded.at(army) = expanded.at(army) || size == armies.at(army).count;
      collapsed.at(army) = collapsed.at(army) || size == 1;
    }
  }
  // Every army must have crossed tiers both ways and left its start, or the
  // run did not exercise what equal hashes are meant to cover.
  for (std::size_t army = 0; army < armies.size(); ++army) {
    EXPECT_TRUE(expanded.at(army)) << "army " << army;
    EXPECT_TRUE(collapsed.at(army)) << "army " << army;
    EXPECT_FALSE(in(armies.at(army).start, world.bodies(army).front().position))
        << "army " << army;
  }
  return world.hash();
}

TEST(World, SameSeedGivesSameHashAfterTenThousandTicks) {
  const Seed seed{42};

  EXPECT_EQ(run_crossing(seed), run_crossing(seed));
  // Control: a different seed must diverge, so equality is not a constant hash.
  EXPECT_NE(run_crossing(seed), run_crossing(Seed{43}));
}

// The value every supported compiler and platform must produce; a change to
// the simulation's rules changes it on purpose.
TEST(World, HashAfterTenThousandTicksIsTheSameOnEveryPlatform) {
  EXPECT_EQ(static_cast<std::uint64_t>(run_crossing(Seed{42})),
            0x26B5495190B0E0BFULL);
}

// Keeps the focus `tier.reach` to the side of the army's lead body, so the
// army stays in that tier while it marches around the wall of walled_map().
void expect_arrival(const Tier& tier) {
  const ArmySpec army{{2, 1}, {13, 1}, 9, 90};
  World world{Seed{1}, Map{walled_map()}, {army}, SEPARATION};
  for (std::uint64_t i = 1; i <= ARRIVAL_TICKS; ++i) {
    const Position lead = world.bodies(0).front().position;
    world.set_focus(
        Position{lead.x + static_cast<std::int32_t>(tier.reach), lead.y});
    const std::vector<Body> before = world.bodies(0);
    world.tick();
    const std::vector<Body>& after = world.bodies(0);
    // The army must be split into agents, which move only on the tier's
    // ticks; the first tick regroups rather than moves them.
    ASSERT_EQ(after.size(), army.count) << "tick " << i;
    const bool moved = after.front().position.x != before.front().position.x ||
                       after.front().position.y != before.front().position.y;
    ASSERT_TRUE(i == 1 || !moved || i % tier.interval == 0) << "tick " << i;
  }
  for (const Body& body : world.bodies(0)) {
    EXPECT_TRUE(in(army.objective, body.position))
        << body.position.x << "," << body.position.y;
  }
}

TEST(World, NearTierAgentsReachTheirObjectiveAroundAWall) {
  expect_arrival(TIERS.at(0));
}

TEST(World, MiddleTierAgentsReachTheirObjectiveAroundAWall) {
  expect_arrival(TIERS.at(1));
}

TEST(World, ArmyKeepsCountAndStrengthAcrossExpandAndCollapse) {
  const ArmySpec army{{8, 8}, {8, 8}, 50, 1003};
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, {army}, SEPARATION};
  const Totals created{army.count, army.strength};

  world.set_focus(Position{FAR_AWAY, FAR_AWAY});
  world.tick();
  ASSERT_EQ(world.bodies(0).size(), 1U);

  world.set_focus(centre_of(army.start));
  world.tick();
  const std::vector<Body> agents = world.bodies(0);
  ASSERT_EQ(agents.size(), army.count);
  EXPECT_TRUE(totals(agents) == created);
  // Control: the comparison catches a collapse that drops one agent.
  const std::vector<Body> one_dropped(agents.begin(), agents.end() - 1);
  EXPECT_FALSE(totals(one_dropped) == created);

  world.set_focus(Position{FAR_AWAY, FAR_AWAY});
  world.tick();
  ASSERT_EQ(world.bodies(0).size(), 1U);
  EXPECT_TRUE(totals(world.bodies(0)) == created);
}

// Marches a near-tier army across an open map and returns its agents after
// ARRIVAL_TICKS ticks.
std::vector<Body> arrive(const ArmySpec& army, std::int32_t separation) {
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, {army}, separation};
  world.set_focus(centre_of(army.objective));
  for (int i = 0; i < ARRIVAL_TICKS; ++i) {
    world.tick();
  }
  return world.bodies(0);
}

TEST(World, AgentsAtTheirObjectiveStayTheSeparationApart) {
  const ArmySpec army{{2, 2}, {12, 12}, 9, 90};
  const std::int64_t apart = std::int64_t{SEPARATION} * SEPARATION;
  const std::vector<Body> separated = arrive(army, SEPARATION);
  const std::vector<Body> overlapping = arrive(army, 0);

  // Both armies must have arrived as agents, or there is no crowd to measure.
  for (const std::vector<Body>& bodies : {separated, overlapping}) {
    ASSERT_EQ(bodies.size(), army.count);
    for (const Body& body : bodies) {
      ASSERT_TRUE(in(army.objective, body.position));
    }
  }
  EXPECT_GE(closest_pair_squared(separated), apart);
  // Control: without separation the flow field alone packs agents closer.
  EXPECT_LT(closest_pair_squared(overlapping), apart);
}

}  // namespace
}  // namespace murim
