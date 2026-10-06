#include "world.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr int TICKS = 10'000;
constexpr int ARRIVAL_TICKS = 3'000;
constexpr std::int32_t SEPARATION = 72;
constexpr std::int32_t FAR_AWAY = 1'000'000;
constexpr std::size_t OPEN_SIDE = 16;
constexpr std::array<std::uint32_t, 3> STAGE_FLOORS{75, 25, 1};
constexpr std::uint64_t NEVER_AGAIN = 1'000'000;

// A skill that never finds a target: nothing stands at distance 0 from its
// user except the user.
constexpr Skill IDLE{0, 0, 0, 0, 0};

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

std::int64_t distance_squared(Position origin, Position point) {
  const std::int64_t offset_x = std::int64_t{point.x} - origin.x;
  const std::int64_t offset_y = std::int64_t{point.y} - origin.y;
  return (offset_x * offset_x) + (offset_y * offset_y);
}

std::int64_t closest_pair_squared(const std::vector<Body>& bodies) {
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  for (std::size_t i = 0; i < bodies.size(); ++i) {
    for (std::size_t j = i + 1; j < bodies.size(); ++j) {
      closest = std::min(closest, distance_squared(bodies.at(i).position,
                                                   bodies.at(j).position));
    }
  }
  return closest;
}

// One faction at peace with itself.
Rules peaceful(std::int32_t separation) {
  return Rules{separation, {{false}}, STAGE_FLOORS};
}

// Two factions at war with each other.
Rules at_war(std::int32_t separation) {
  return Rules{separation, {{false, true}, {true, false}}, STAGE_FLOORS};
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

// Four armies of two hostile factions headed for the opposite corners of
// crossing_map(), so they meet near the gaps.
std::vector<ArmySpec> crossing_armies() {
  const Skill strike{2 * CELL_SIZE, CELL_SIZE, 1, 4, 24};
  const Skill blast{3 * CELL_SIZE, 2 * CELL_SIZE, 2, 9, 64};
  const std::vector<ArmySpec> armies{
      {{1, 1}, {46, 46}, 12, 1200, 0, {strike, blast}},
      {{46, 1}, {1, 46}, 30, 3010, 1, {strike}},
      {{1, 46}, {46, 1}, 5, 500, 1, {blast, strike}},
      {{46, 46}, {1, 1}, 2, 207, 0, {blast}},
  };
  return armies;
}

// Structures standing on and beside both gaps of crossing_map().
std::vector<Structure> crossing_structures() {
  const std::vector<Structure> structures{
      {{23, 10}, 100}, {{24, 10}, 100}, {{25, 10}, 100},
      {{23, 37}, 100}, {{24, 37}, 100}, {{25, 37}, 100},
  };
  return structures;
}

// Expects every army to have crossed tiers both ways, left its start and
// lost strength, and a structure to have broken, or the run did not exercise
// what equal hashes are meant to cover.
void expect_crossing_exercised(const World& world,
                               const std::vector<bool>& expanded,
                               const std::vector<bool>& collapsed) {
  const std::vector<ArmySpec> armies = crossing_armies();
  for (std::size_t army = 0; army < armies.size(); ++army) {
    EXPECT_TRUE(expanded.at(army) && collapsed.at(army)) << "army " << army;
    EXPECT_FALSE(in(armies.at(army).start, world.bodies(army).front().position))
        << "army " << army;
    EXPECT_LT(totals(world.bodies(army)).strength, armies.at(army).strength)
        << "army " << army;
  }
  bool broken = false;
  for (std::size_t structure = 0; structure < crossing_structures().size();
       ++structure) {
    broken = broken || world.stage(structure) != Stage::INTACT;
  }
  EXPECT_TRUE(broken);
}

// Runs crossing_armies() for TICKS ticks while the focus visits each army's
// start for FOCUS_DWELL ticks in turn.
StateHash run_crossing(Seed seed) {
  constexpr int FOCUS_DWELL = 500;
  const std::vector<ArmySpec> armies = crossing_armies();
  World world{seed, Map{crossing_map()}, armies, crossing_structures(),
              at_war(SEPARATION)};
  std::vector<bool> expanded(armies.size(), false);
  std::vector<bool> collapsed(armies.size(), false);
  for (int i = 0; i < TICKS; ++i) {
    const auto visited =
        static_cast<std::size_t>(i / FOCUS_DWELL) % armies.size();
    world.set_focus(centre_of(armies.at(visited).start));
    world.tick();
    for (std::size_t army = 0; army < armies.size(); ++army) {
      const std::size_t size = world.bodies(army).size();
      expanded.at(army) = expanded.at(army) || size > 1;
      collapsed.at(army) = collapsed.at(army) || size == 1;
    }
  }
  expect_crossing_exercised(world, expanded, collapsed);
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
            0xEF1ADB54473AC222ULL);
}

// Keeps the focus `tier.reach` to the side of the army's lead body, so the
// army stays in that tier while it marches around the wall of walled_map().
void expect_arrival(const Tier& tier) {
  const ArmySpec army{{2, 1}, {13, 1}, 9, 90, 0, {IDLE}};
  World world{Seed{1}, Map{walled_map()}, {army}, {}, peaceful(SEPARATION)};
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
  const ArmySpec army{{8, 8}, {8, 8}, 50, 1003, 0, {IDLE}};
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, {army}, {}, peaceful(SEPARATION)};
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
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, {army}, {}, peaceful(separation)};
  world.set_focus(centre_of(army.objective));
  for (int i = 0; i < ARRIVAL_TICKS; ++i) {
    world.tick();
  }
  return world.bodies(0);
}

TEST(World, AgentsAtTheirObjectiveStayTheSeparationApart) {
  const ArmySpec army{{2, 2}, {12, 12}, 9, 90, 0, {IDLE}};
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

// Strength each single-agent target army starts with in the combat tests.
constexpr std::uint64_t TARGET_STRENGTH = 1'000'000;

// Returns the strength `target` lost in one near-tier tick in which `user`
// strikes it; both hold their start cells.
std::uint64_t strength_lost(const ArmySpec& user, const ArmySpec& target,
                            const Rules& rules) {
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, {user, target}, {}, rules};
  world.set_focus(centre_of(user.start));
  world.tick();
  return target.strength - totals(world.bodies(1)).strength;
}

TEST(World, OnlyHostileBodiesWithinRangeUseTheirSkills) {
  const Skill reach{2 * CELL_SIZE, 0, 1, 0, NEVER_AGAIN};
  const Skill short_reach{(2 * CELL_SIZE) - 1, 0, 1, 0, NEVER_AGAIN};
  const ArmySpec target{{4, 0}, {4, 0}, 1, TARGET_STRENGTH, 1, {IDLE}};
  const ArmySpec user{{2, 0}, {2, 0}, 1, 1, 0, {reach}};
  ArmySpec ally = target;
  ally.faction = 0;

  EXPECT_EQ(strength_lost(user, target, at_war(0)), 1U);
  // Controls: one unit short of the range, or a target of a faction the user
  // is not hostile to, and nothing is used.
  ArmySpec out_of_range = user;
  out_of_range.skills = {short_reach};
  EXPECT_EQ(strength_lost(out_of_range, target, at_war(0)), 0U);
  EXPECT_EQ(strength_lost(user, ally, at_war(0)), 0U);
}

// The cells (2..6, 0..6), each holding a single-agent hostile army in
// damaged_cells().
std::vector<Cell> target_grid() {
  constexpr std::int32_t FIRST_COLUMN = 2;
  constexpr std::int32_t LAST_COLUMN = 6;
  constexpr std::int32_t LAST_ROW = 6;
  std::vector<Cell> cells;
  for (Cell cell{FIRST_COLUMN, 0}; cell.y <= LAST_ROW; ++cell.y) {
    for (cell.x = FIRST_COLUMN; cell.x <= LAST_COLUMN; ++cell.x) {
      cells.push_back(cell);
    }
  }
  return cells;
}

using Cells = std::vector<std::pair<std::int32_t, std::int32_t>>;

// Fires one skill of `radius` from (0, 3) at target_grid() and returns the
// cells of the armies it damaged.
Cells damaged_cells(std::int32_t radius) {
  std::vector<ArmySpec> armies{
      {{0, 3}, {0, 3}, 1, 1, 0, {{3 * CELL_SIZE, radius, 1, 0, NEVER_AGAIN}}}};
  for (const Cell cell : target_grid()) {
    armies.push_back(ArmySpec{cell, cell, 1, TARGET_STRENGTH, 1, {IDLE}});
  }
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(0)};
  world.set_focus(centre_of(armies.front().start));
  world.tick();
  Cells damaged;
  for (std::size_t army = 1; army < armies.size(); ++army) {
    if (totals(world.bodies(army)).strength < TARGET_STRENGTH) {
      damaged.emplace_back(armies.at(army).start.x, armies.at(army).start.y);
    }
  }
  return damaged;
}

// The cells of target_grid() whose centres lie within `radius` of the
// nearest target's, (2, 3), found by scanning every one.
Cells cells_inside(std::int32_t radius) {
  Cells inside;
  for (const Cell cell : target_grid()) {
    if (distance_squared(centre_of({2, 3}), centre_of(cell)) <=
        std::int64_t{radius} * radius) {
      inside.emplace_back(cell.x, cell.y);
    }
  }
  return inside;
}

TEST(World, AreaSkillHitsExactlyTheBodiesInsideItsShape) {
  const std::int32_t radius = 2 * CELL_SIZE;
  const std::pair<std::int32_t, std::int32_t> edge{4, 3};

  const auto hit = damaged_cells(radius);
  EXPECT_EQ(hit, cells_inside(radius));
  // The body on (4, 3) sits exactly on the edge, so the shape takes it in.
  EXPECT_NE(std::find(hit.begin(), hit.end(), edge), hit.end());
  // Control: one unit less radius leaves that body just outside.
  const auto narrower = damaged_cells(radius - 1);
  EXPECT_EQ(narrower, cells_inside(radius - 1));
  EXPECT_EQ(std::find(narrower.begin(), narrower.end(), edge), narrower.end());
}

TEST(World, AreaSkillWalksAStructureThroughEveryStage) {
  constexpr std::uint32_t DURABILITY = 100;
  const ArmySpec user{{0, 0}, {0, 0}, 1, 1, 0, {{2 * CELL_SIZE, 0, 0, 30, 1}}};
  const ArmySpec target{{2, 0}, {2, 0}, 1, TARGET_STRENGTH, 1, {IDLE}};
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {user, target},
              {{{2, 0}, DURABILITY}},
              at_war(0)};
  world.set_focus(centre_of(user.start));

  std::vector<Stage> stages{world.stage(0)};
  for (int i = 0; i < ARRIVAL_TICKS && stages.back() != Stage::RUBBLE; ++i) {
    world.tick();
    if (world.stage(0) != stages.back()) {
      stages.push_back(world.stage(0));
    }
  }
  const std::vector<Stage> every{Stage::INTACT, Stage::DAMAGED,
                                 Stage::DESTROYED, Stage::RUBBLE};
  EXPECT_EQ(stages, every);
  EXPECT_EQ(world.structure(0).durability, 0U);
}

TEST(World, SkillOutcomeScalesWithTheAgentsItsUserStandsFor) {
  constexpr std::uint64_t DAMAGE = 7;
  constexpr std::uint32_t AGENTS = 5;
  const Skill skill{4 * CELL_SIZE, 0, DAMAGE, 0, NEVER_AGAIN};
  const ArmySpec target{{3, 0}, {3, 0}, 1, TARGET_STRENGTH, 1, {IDLE}};
  // A single agent, as the player's body is.
  const ArmySpec one{{0, 0}, {0, 0}, 1, 1, 0, {skill}};
  const ArmySpec five{{0, 0}, {0, 0}, AGENTS, AGENTS, 0, {skill}};
  const auto lost = [&target](const ArmySpec& user, Position focus) {
    World world{
        Seed{1}, Map{open_map(OPEN_SIDE)}, {user, target}, {}, at_war(0)};
    world.set_focus(focus);
    // The far tier acts every TIERS.back().interval ticks.
    for (std::uint64_t i = 0; i < TIERS.back().interval; ++i) {
      world.tick();
    }
    const std::size_t bodies = world.bodies(0).size();
    return std::pair{bodies,
                     target.strength - totals(world.bodies(1)).strength};
  };
  const Position near = centre_of(one.start);
  const Position far{FAR_AWAY, FAR_AWAY};

  EXPECT_EQ(lost(one, near), std::pair(std::size_t{1}, DAMAGE));
  // Five agents striking one by one, and the aggregate standing for them
  // striking once, do the same damage: five times one agent's.
  EXPECT_EQ(lost(five, near), std::pair(std::size_t{AGENTS}, AGENTS * DAMAGE));
  EXPECT_EQ(lost(five, far), std::pair(std::size_t{1}, AGENTS * DAMAGE));
}

// Two equal hostile armies on neighbouring cells, holding their ground.
std::vector<ArmySpec> skirmish(std::uint64_t cooldown) {
  const Skill skill{2 * CELL_SIZE, CELL_SIZE / 2, 1, 0, cooldown};
  const std::vector<ArmySpec> armies{
      {{3, 3}, {3, 3}, 12, 120, 0, {skill}},
      {{4, 3}, {4, 3}, 12, 120, 1, {skill}},
  };
  return armies;
}

// Whether an army's totals hold together: no more agents than strength,
// none without strength, and no more strength than it was created with.
bool consistent(const Totals& now, const ArmySpec& created) {
  return now.count <= now.strength && (now.count == 0) == (now.strength == 0) &&
         now.strength <= created.strength;
}

// Fights skirmish() with the focus at `focus` until a side falls, and
// checks every side's totals stay consistent on every tick.
void expect_consistent_battle(Position focus) {
  const std::vector<ArmySpec> armies = skirmish(4);
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(SEPARATION)};
  world.set_focus(focus);
  bool fallen = false;
  for (int i = 0; i < TICKS && !fallen; ++i) {
    world.tick();
    for (std::size_t army = 0; army < armies.size(); ++army) {
      const Totals now = totals(world.bodies(army));
      EXPECT_TRUE(consistent(now, armies.at(army)))
          << "army " << army << " tick " << i;
      fallen = fallen || now.count == 0;
    }
  }
  EXPECT_TRUE(fallen);
}

TEST(World, BattleNearTheFocusEndsWithConsistentTotals) {
  expect_consistent_battle(centre_of(skirmish(0).front().start));
}

TEST(World, BattleFarFromTheFocusEndsWithConsistentTotals) {
  expect_consistent_battle(Position{FAR_AWAY, FAR_AWAY});
}

void run_ticks(World& world, std::uint64_t ticks) {
  for (std::uint64_t i = 0; i < ticks; ++i) {
    world.tick();
  }
}

// Both armies' totals, compared as a whole.
bool same_totals(const World& world, const std::array<Totals, 2>& expected) {
  return totals(world.bodies(0)) == expected.at(0) &&
         totals(world.bodies(1)) == expected.at(1);
}

TEST(World, ArmiesKeepCountAndStrengthWhenRegroupedMidBattle) {
  constexpr std::uint64_t COOLDOWN = 100;
  constexpr std::uint64_t BEFORE_COLLAPSE = 49;
  constexpr std::uint64_t COLLAPSED_FOR = 10;
  const std::vector<ArmySpec> armies = skirmish(COOLDOWN);
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(SEPARATION)};
  const Position near = centre_of(armies.front().start);
  world.set_focus(near);

  run_ticks(world, BEFORE_COLLAPSE);
  const std::array<Totals, 2> struck{totals(world.bodies(0)),
                                     totals(world.bodies(1))};
  // The first strikes landed, so the regrouping happens mid-battle.
  ASSERT_LT(struck.at(0).strength + struck.at(1).strength,
            armies.at(0).strength + armies.at(1).strength);

  world.set_focus(Position{FAR_AWAY, FAR_AWAY});
  world.tick();
  ASSERT_EQ(world.bodies(0).size(), 1U);
  EXPECT_TRUE(same_totals(world, struck));

  run_ticks(world, COLLAPSED_FOR);
  world.set_focus(near);
  world.tick();
  ASSERT_EQ(world.bodies(0).size(), struck.at(0).count);
  EXPECT_TRUE(same_totals(world, struck));

  // The battle goes on once the cooldown has passed.
  run_ticks(world, COOLDOWN - BEFORE_COLLAPSE - COLLAPSED_FOR);
  EXPECT_FALSE(same_totals(world, struck));
}

}  // namespace
}  // namespace murim
