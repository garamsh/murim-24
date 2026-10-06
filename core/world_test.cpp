#include "world.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"
#include "spatial_hash.hpp"

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

// Structures diagonal to both gaps of crossing_map(), clear of the cells
// leading into them.
std::vector<Structure> crossing_structures() {
  const std::vector<Structure> structures{
      {{23, 9}, 100},  {{23, 11}, 100}, {{25, 11}, 100},
      {{23, 36}, 100}, {{25, 36}, 100}, {{25, 38}, 100},
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
            0x4C50A33638415407ULL);
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

// The lead body of each of the first `armies` armies.
std::vector<Position> leads(const World& world, std::size_t armies) {
  std::vector<Position> positions;
  positions.reserve(armies);
  for (std::size_t army = 0; army < armies; ++army) {
    positions.push_back(world.bodies(army).front().position);
  }
  return positions;
}

// Runs `ticks` ticks and returns, for each army of `specs`, the ticks on
// which its lead body moved.
std::vector<std::vector<std::uint64_t>> ticks_moved(
    World& world, const std::vector<ArmySpec>& specs, std::uint64_t ticks) {
  const std::size_t armies = specs.size();
  std::vector<std::vector<std::uint64_t>> moved(armies);
  for (std::uint64_t tick = 1; tick <= ticks; ++tick) {
    const std::vector<Position> before = leads(world, armies);
    world.tick();
    const std::vector<Position> after = leads(world, armies);
    for (std::size_t army = 0; army < armies; ++army) {
      if (after.at(army).x != before.at(army).x ||
          after.at(army).y != before.at(army).y) {
        moved.at(army).push_back(tick);
      }
    }
  }
  return moved;
}

TEST(World, ArmiesOfATierMoveEachAtItsOwnPhase) {
  constexpr std::int32_t LAST_COLUMN = 14;
  constexpr std::int32_t FOCUS_COLUMN = -12;
  constexpr std::uint64_t STEPS = 10;
  constexpr std::uint64_t INTERVAL = TIERS.at(1).interval;
  std::vector<ArmySpec> armies;
  armies.reserve(INTERVAL);
  for (std::int32_t row = 0; row < static_cast<std::int32_t>(INTERVAL); ++row) {
    armies.push_back(
        ArmySpec{{2, 3 * row}, {LAST_COLUMN, 3 * row}, 1, 1, 0, {IDLE}});
  }
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, peaceful(SEPARATION)};
  // Every army stays within the middle tier's reach of this focus, and
  // beyond the near tier's.
  world.set_focus(Position{FOCUS_COLUMN * CELL_SIZE, 0});
  const auto moved = ticks_moved(world, armies, STEPS * INTERVAL);

  // Each army moved, and only on the ticks its phase picks.
  for (std::size_t army = 0; army < armies.size(); ++army) {
    ASSERT_FALSE(moved.at(army).empty()) << "army " << army;
    const auto off_phase = [army](std::uint64_t tick) {
      return (tick + army) % INTERVAL != 0;
    };
    EXPECT_TRUE(
        std::none_of(moved.at(army).begin(), moved.at(army).end(), off_phase))
        << "army " << army;
  }
  // Control: no two armies of the tier moved on the same tick.
  EXPECT_NE(moved.at(0).front(), moved.at(1).front());
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

TEST(World, EquallyNearTargetsGoToTheLowestIndex) {
  const ArmySpec user{{4, 4}, {4, 4}, 1,
                      1,      0,      {{3 * CELL_SIZE, 0, 1, 0, NEVER_AGAIN}}};
  // The lower index stands east, where a query visits it last.
  const ArmySpec east{{6, 4}, {6, 4}, 1, TARGET_STRENGTH, 1, {IDLE}};
  const ArmySpec west{{2, 4}, {2, 4}, 1, TARGET_STRENGTH, 1, {IDLE}};
  // The two targets must stand equally near, or there is no tie to break.
  ASSERT_EQ(distance_squared(centre_of(user.start), centre_of(east.start)),
            distance_squared(centre_of(user.start), centre_of(west.start)));
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, {user, east, west}, {}, at_war(0)};
  world.set_focus(centre_of(user.start));
  world.tick();

  EXPECT_EQ(totals(world.bodies(1)).strength, TARGET_STRENGTH - 1);
  // Control: the other target, as near, is untouched.
  EXPECT_EQ(totals(world.bodies(2)).strength, TARGET_STRENGTH);
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

TEST(World, StrikeOfTheLargestRadiusWalksNoMoreCellsThanTheMap) {
  const ArmySpec user{
      {3, 3}, {3, 3}, 1, 1, 0, {{2 * CELL_SIZE, MAX_REACH, 1, 0, NEVER_AGAIN}}};
  const ArmySpec target{{4, 3}, {4, 3}, 1, TARGET_STRENGTH, 1, {IDLE}};
  // The walk of the strike's area query over the bodies the world indexes,
  // which stand at their start cells' centres.
  const SpatialHash crowd{{centre_of(user.start), centre_of(target.start)}};
  const auto [first, last] = crowd.walk(centre_of(target.start), MAX_REACH);
  const std::int64_t cells = (std::int64_t{last.x} - first.x + 1) *
                             (std::int64_t{last.y} - first.y + 1);
  // Fatal, so an unbounded walk fails here rather than stalling the strike.
  ASSERT_LE(cells, std::int64_t{OPEN_SIDE * OPEN_SIDE});

  EXPECT_EQ(strength_lost(user, target, at_war(0)), 1U);
}

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

void run_ticks(World& world, std::uint64_t ticks) {
  for (std::uint64_t i = 0; i < ticks; ++i) {
    world.tick();
  }
}

// Two armies on neighbouring cells, far from the focus: one agent with
// `user_skill`, and the rest of MAX_AGENTS as its target with MAX_STRENGTH.
std::vector<ArmySpec> at_the_limits(const Skill& user_skill) {
  constexpr std::uint32_t TARGETS = MAX_AGENTS - 1;
  const std::vector<ArmySpec> armies{
      {{3, 3}, {3, 3}, 1, MAX_STRENGTH, 0, {user_skill}},
      {{4, 3}, {4, 3}, TARGETS, MAX_STRENGTH, 1, {IDLE}},
  };
  return armies;
}

// The separation of at_the_limits(): one sub-cell unit per agent, so a
// small disc covers every target agent.
constexpr std::int32_t PACKED = 1;

// The skill of at_the_limits(): the most damage, a disc whose area,
// 355 * 600^2 / 113 = 1130973 squares, covers all 2^20 - 1 target agents,
// and a cooldown of 1 so a far step holds 16 uses.
constexpr Skill LIMIT_SKILL{2 * CELL_SIZE, 600, MAX_BODY_DAMAGE, 0, 1};

TEST(World, StrikeAtTheLargestAcceptedValuesKeepsTotalsExact) {
  const std::vector<ArmySpec> armies = at_the_limits(LIMIT_SKILL);
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(PACKED)};
  world.set_focus(Position{FAR_AWAY, FAR_AWAY});
  run_ticks(world, TIERS.back().interval);

  // 2^19 damage, times 16 uses, times 2^20 - 1 agents struck, is
  // 2^43 - 2^23: the target keeps 2^23 strength, and its count falls in
  // proportion to one agent.
  const std::uint64_t dealt =
      MAX_BODY_DAMAGE * TIERS.back().interval * armies.at(1).count;
  const Totals kept = totals(world.bodies(1));
  EXPECT_EQ(kept.strength, MAX_STRENGTH - dealt);
  EXPECT_EQ(kept.count, 1U);
  EXPECT_TRUE(totals(world.bodies(0)) ==
              (Totals{armies.at(0).count, armies.at(0).strength}));
}

// Whether the constructor refuses `armies` with std::invalid_argument.
bool refused(const std::vector<ArmySpec>& armies) {
  try {
    const World world{
        Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(PACKED)};
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

TEST(World, InputJustPastALimitIsRefused) {
  std::vector<ArmySpec> agents = at_the_limits(LIMIT_SKILL);
  ++agents.at(1).count;
  std::vector<ArmySpec> strength = at_the_limits(LIMIT_SKILL);
  ++strength.at(1).strength;
  Skill damage = LIMIT_SKILL;
  ++damage.body_damage;
  Skill cooldown = LIMIT_SKILL;
  cooldown.cooldown = MAX_COOLDOWN + 1;
  Skill longest = LIMIT_SKILL;
  longest.cooldown = MAX_COOLDOWN;

  // Control: the values at every limit are accepted.
  EXPECT_FALSE(refused(at_the_limits(longest)));
  EXPECT_TRUE(refused(agents));
  EXPECT_TRUE(refused(strength));
  EXPECT_TRUE(refused(at_the_limits(damage)));
  EXPECT_TRUE(refused(at_the_limits(cooldown)));
}

TEST(World, AreaSkillWalksAStructureThroughEveryStage) {
  constexpr std::uint32_t DURABILITY = 100;
  // The disc around the target reaches just into the structure beside it.
  const ArmySpec user{
      {0, 0}, {0, 0}, 1, 1, 0, {{2 * CELL_SIZE, CELL_SIZE / 2, 0, 30, 1}}};
  const ArmySpec target{{2, 0}, {2, 0}, 1, TARGET_STRENGTH, 1, {IDLE}};
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {user, target},
              {{{3, 0}, DURABILITY}},
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

// Strength each matchup army starts with: 12 agents of 100.
constexpr std::uint32_t MATCHUP_COUNT = 12;
constexpr std::uint64_t MATCHUP_STRENGTH = 1200;

// Fights an army using `first` against an equal one using `second`, on
// neighbouring cells with the focus at `focus`, until a side falls or TICKS
// pass, and returns the strength each side has left.
std::array<std::uint64_t, 2> fight(const Skill& first, const Skill& second,
                                   Position focus) {
  const std::vector<ArmySpec> armies{
      {{3, 3}, {3, 3}, MATCHUP_COUNT, MATCHUP_STRENGTH, 0, {first}},
      {{4, 3}, {4, 3}, MATCHUP_COUNT, MATCHUP_STRENGTH, 1, {second}},
  };
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(SEPARATION)};
  world.set_focus(focus);
  for (int i = 0; i < TICKS && totals(world.bodies(0)).count > 0 &&
                  totals(world.bodies(1)).count > 0;
       ++i) {
    world.tick();
  }
  return {totals(world.bodies(0)).strength, totals(world.bodies(1)).strength};
}

// The side with strength left, and how much it kept, as the outcome a
// matchup must reach in every tier.
struct Outcome {
  std::size_t winner;
  std::uint64_t kept;
};

Outcome outcome(const std::array<std::uint64_t, 2>& left) {
  const std::size_t winner = left.at(0) > 0 ? 0 : 1;
  return Outcome{winner, left.at(winner)};
}

// Runs one matchup near the focus and far from it, and expects the same side
// to win with the strength it keeps within a quarter of the start.
void expect_same_outcome(const Skill& first, const Skill& second,
                         std::size_t expected_winner) {
  constexpr std::uint64_t TOLERANCE = MATCHUP_STRENGTH / 4;
  const auto near = fight(first, second, centre_of({3, 3}));
  const auto far = fight(first, second, Position{FAR_AWAY, FAR_AWAY});
  // The battles must have ended, or there is no winner to compare.
  ASSERT_TRUE(near.at(0) == 0 || near.at(1) == 0);
  ASSERT_TRUE(far.at(0) == 0 || far.at(1) == 0);
  const Outcome near_outcome = outcome(near);
  const Outcome far_outcome = outcome(far);
  EXPECT_EQ(near_outcome.winner, expected_winner);
  EXPECT_EQ(far_outcome.winner, expected_winner);
  EXPECT_LE(std::max(near_outcome.kept, far_outcome.kept) -
                std::min(near_outcome.kept, far_outcome.kept),
            TOLERANCE)
      << "near kept " << near_outcome.kept << ", far kept " << far_outcome.kept;
}

// A quick single-target skill: 2 damage every tick, 2 per agent per tick.
constexpr Skill JAB{2 * CELL_SIZE, 0, 2, 0, 1};

TEST(World, AreaSkillBeatsSingleTargetSkillNearAndFarAlike) {
  // 1 damage every 2 ticks to every agent within a cell's width, which
  // covers the whole of the other army: up to 6 per agent per tick.
  const Skill sweep{2 * CELL_SIZE, CELL_SIZE, 1, 0, 2};
  expect_same_outcome(JAB, sweep, 1);
}

TEST(World, QuickSkillBeatsSlowSkillNearAndFarAlike) {
  // 24 damage every 20 ticks, 1.2 per agent per tick; its cooldown is longer
  // than the far tier's 16-tick step.
  const Skill heave{2 * CELL_SIZE, 0, 24, 0, 20};
  expect_same_outcome(JAB, heave, 0);
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

// An order given before tick `tick` of a run.
struct LoggedOrder {
  std::uint64_t tick;
  std::size_t army;
  Order order;
};

// Ticks `world` `ticks` times as a driving adapter does: gives each logged
// order before its tick, and places the focus on army `steered`'s lead body
// before every tick. Returns the orders given, as a recording.
std::vector<LoggedOrder> drive(World& world, std::size_t steered,
                               const std::vector<LoggedOrder>& log,
                               std::uint64_t ticks) {
  std::vector<LoggedOrder> given;
  for (std::uint64_t tick = 0; tick < ticks; ++tick) {
    for (const LoggedOrder& logged : log) {
      if (logged.tick == tick) {
        world.order(logged.army, logged.order);
        given.push_back(logged);
      }
    }
    world.set_focus(world.bodies(steered).front().position);
    world.tick();
  }
  return given;
}

constexpr std::size_t STEERED = 0;
constexpr std::size_t ORDERS_SIDE = 24;
constexpr std::uint64_t ORDER_TICKS = 1'500;
constexpr Cell STEERED_HOME{2, 2};
constexpr Cell HOSTILE_HOME{12, 12};
constexpr Cell BESIDE_HOSTILE{12, 10};
constexpr Seed ORDERS_SEED{7};
constexpr std::uint64_t HOSTILE_STRENGTH = 6'000;
constexpr std::uint32_t DURABILITY = 100;

// One agent at STEERED_HOME with a skill that spares structures and one that
// breaks them, bound for `objective`; six hostile agents holding
// HOSTILE_HOME, with a structure under them.
World orders_world(Cell objective) {
  const Skill strike{3 * CELL_SIZE, 0, 50, 0, 10};
  const Skill smash{3 * CELL_SIZE, CELL_SIZE, 50, 40, 10};
  const Skill guard{3 * CELL_SIZE, 0, 1, 0, 10};
  const std::vector<ArmySpec> armies{
      {STEERED_HOME, objective, 1, 100'000, 0, {strike, smash}},
      {HOSTILE_HOME, HOSTILE_HOME, 6, HOSTILE_STRENGTH, 1, {guard}},
  };
  return World{ORDERS_SEED,
               Map{open_map(ORDERS_SIDE)},
               armies,
               {{{HOSTILE_HOME.x, HOSTILE_HOME.y + 1}, DURABILITY}},
               at_war(SEPARATION)};
}

TEST(World, OrderedBodyMatchesAnNpcFollowingTheSameOrderFromData) {
  World npc = orders_world(BESIDE_HOSTILE);
  drive(npc, STEERED, {}, ORDER_TICKS);
  World ordered = orders_world(STEERED_HOME);
  drive(ordered, STEERED, {{0, STEERED, {BESIDE_HOSTILE, {0, 1}}}},
        ORDER_TICKS);

  // Setup: the body left home and the hostile army took damage, so the run
  // covered moving and striking.
  EXPECT_FALSE(in(STEERED_HOME, ordered.bodies(STEERED).front().position));
  EXPECT_LT(totals(ordered.bodies(1)).strength, HOSTILE_STRENGTH);
  EXPECT_EQ(npc.hash(), ordered.hash());
  // Control: the same order with one skill fewer is another run.
  World narrower = orders_world(STEERED_HOME);
  drive(narrower, STEERED, {{0, STEERED, {BESIDE_HOSTILE, {0}}}}, ORDER_TICKS);
  EXPECT_NE(npc.hash(), narrower.hash());
}

TEST(World, OrderedBodyStrikesOnlyWithTheSkillsItIsOrderedToUse) {
  World smashing = orders_world(STEERED_HOME);
  drive(smashing, STEERED, {{0, STEERED, {BESIDE_HOSTILE, {1}}}}, ORDER_TICKS);
  EXPECT_LT(smashing.structure(0).durability, DURABILITY);

  // Control: ordered to the skill that spares structures, the body still
  // strikes, and the structure stands untouched.
  World striking = orders_world(STEERED_HOME);
  drive(striking, STEERED, {{0, STEERED, {BESIDE_HOSTILE, {0}}}}, ORDER_TICKS);
  EXPECT_LT(totals(striking.bodies(1)).strength, HOSTILE_STRENGTH);
  EXPECT_EQ(striking.structure(0).durability, DURABILITY);
}

TEST(World, TiersExpandAndCollapseAroundTheSteeredBody) {
  constexpr std::size_t SIDE = 64;
  constexpr std::uint64_t LEG_TICKS = 1'200;
  const Cell home{2, 4};
  const ArmySpec steered{home, home, 1, 100, 0, {IDLE}};
  const ArmySpec camp{{56, 4}, {56, 4}, 20, 2'000, 0, {IDLE}};
  const auto run = [&](bool follow) {
    World world{Seed{3},
                Map{open_map(SIDE)},
                {steered, camp},
                {},
                peaceful(SEPARATION)};
    world.set_focus(centre_of(home));
    std::vector<std::size_t> sizes;
    for (const Cell objective : {Cell{54, 4}, home}) {
      world.order(STEERED, {objective, {0}});
      for (std::uint64_t tick = 0; tick < LEG_TICKS; ++tick) {
        world.set_focus(follow ? world.bodies(STEERED).front().position
                               : centre_of(home));
        world.tick();
        sizes.push_back(world.bodies(1).size());
      }
    }
    return sizes;
  };

  const std::vector<std::size_t> following = run(true);
  EXPECT_EQ(following.front(), 1U);
  EXPECT_GT(*std::max_element(following.begin(), following.end()), 1U);
  EXPECT_EQ(following.back(), 1U);
  // Control: with the focus left at home the camp never expands.
  const std::vector<std::size_t> staying = run(false);
  EXPECT_EQ(*std::max_element(staying.begin(), staying.end()), 1U);
}

TEST(World, ReplayingARecordedOrderLogGivesTheSameHash) {
  const std::vector<LoggedOrder> issued{
      {0, STEERED, {BESIDE_HOSTILE, {0, 1}}},
      {400, STEERED, {Cell{6, 14}, {1}}},
      {900, STEERED, {BESIDE_HOSTILE, {0}}},
  };
  World live = orders_world(STEERED_HOME);
  const std::vector<LoggedOrder> recorded =
      drive(live, STEERED, issued, ORDER_TICKS);
  ASSERT_EQ(recorded.size(), issued.size());
  World replayed = orders_world(STEERED_HOME);
  drive(replayed, STEERED, recorded, ORDER_TICKS);
  EXPECT_EQ(live.hash(), replayed.hash());

  // Control: the same log with one order a tick later is another run.
  std::vector<LoggedOrder> late = recorded;
  late.at(1).tick += 1;
  World shifted = orders_world(STEERED_HOME);
  drive(shifted, STEERED, late, ORDER_TICKS);
  EXPECT_NE(live.hash(), shifted.hash());
}

TEST(World, OrderNamingNoArmyBlockedCellOrSkillItLacksIsRefused) {
  World world = orders_world(STEERED_HOME);
  EXPECT_NO_THROW(world.order(STEERED, {BESIDE_HOSTILE, {1}}));

  EXPECT_THROW(world.order(2, {BESIDE_HOSTILE, {0}}), std::invalid_argument);
  EXPECT_THROW(world.order(STEERED, {Cell{-1, 0}, {0}}), std::invalid_argument);
  EXPECT_THROW(world.order(STEERED, {BESIDE_HOSTILE, {}}),
               std::invalid_argument);
  EXPECT_THROW(world.order(STEERED, {BESIDE_HOSTILE, {2}}),
               std::invalid_argument);
}

constexpr Footprint HOUSE{3, 3};

// The durability left on a 3 by 3 house after a user strikes a target beside
// its west edge with a disc of `radius` around the target for ARRIVAL_TICKS.
std::uint32_t house_after_strikes(std::int32_t radius) {
  constexpr std::uint32_t DURABILITY = 1'000;
  const ArmySpec user{{0, 5}, {0, 5}, 1,
                      1,      0,      {{3 * CELL_SIZE, radius, 0, 1, 1}}};
  const ArmySpec target{{2, 5}, {2, 5}, 1, TARGET_STRENGTH, 1, {IDLE}};
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {user, target},
              {{{4, 4}, DURABILITY, HOUSE}},
              at_war(0)};
  world.set_focus(centre_of(user.start));
  run_ticks(world, ARRIVAL_TICKS);
  return world.structure(0).durability;
}

TEST(World, AreaSkillHitsAStructureThroughTheEdgeOfItsFootprint) {
  // From the target's centre to the house's west edge, the middle cell of
  // that edge being the nearest: two and a half cells less half a cell.
  constexpr std::int32_t TO_EDGE = (3 * CELL_SIZE) / 2;
  EXPECT_LT(house_after_strikes(TO_EDGE), 1'000U);
  // Control: one unit less radius stops just outside the footprint.
  EXPECT_EQ(house_after_strikes(TO_EDGE - 1), 1'000U);
}

// The cells an agent marching from (1, 5) to (12, 5) passes through, with
// `structures` standing on the map.
std::vector<Cell> march_cells(const std::vector<Structure>& structures) {
  const ArmySpec walker{{1, 5}, {12, 5}, 1, 1, 0, {IDLE}};
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {walker},
              structures,
              peaceful(SEPARATION)};
  world.set_focus(centre_of(walker.start));
  std::vector<Cell> cells;
  for (int i = 0; i < ARRIVAL_TICKS; ++i) {
    world.tick();
    cells.push_back(cell_of(world.bodies(0).front().position));
  }
  return cells;
}

bool covers(const Structure& structure, Cell cell) {
  return cell.x >= structure.cell.x && cell.y >= structure.cell.y &&
         cell.x < structure.cell.x + structure.footprint.width &&
         cell.y < structure.cell.y + structure.footprint.height;
}

TEST(World, BodiesWalkAroundAStructuresFootprint) {
  const Structure house{{5, 4}, 1, HOUSE};
  const auto inside_house = [&house](Cell cell) { return covers(house, cell); };
  const std::vector<Cell> around = march_cells({house});
  EXPECT_TRUE(in(Cell{12, 5}, centre_of(around.back())));
  EXPECT_TRUE(std::none_of(around.begin(), around.end(), inside_house));

  // Control: with no house the straight path crosses its footprint.
  const std::vector<Cell> straight = march_cells({});
  EXPECT_TRUE(std::any_of(straight.begin(), straight.end(), inside_house));
}

bool refused_on_walled_map(const std::vector<ArmySpec>& armies,
                           const std::vector<Structure>& structures) {
  try {
    const World world{Seed{1}, Map{walled_map()}, armies, structures,
                      peaceful(0)};
    return false;
  } catch (const std::invalid_argument&) {
    return true;
  }
}

TEST(World, FootprintsOverWallsArmiesOrEachOtherAreRefused) {
  const ArmySpec army{{0, 0}, {0, 0}, 1, 1, 0, {IDLE}};
  const Structure house{{1, 1}, 1, HOUSE};
  EXPECT_FALSE(refused_on_walled_map({army}, {house}));

  // Over the wall down column 8, over the army, over another structure,
  // and of no width.
  EXPECT_TRUE(refused_on_walled_map({army}, {{{6, 1}, 1, HOUSE}}));
  EXPECT_TRUE(refused_on_walled_map({army}, {{{0, 0}, 1, HOUSE}}));
  EXPECT_TRUE(refused_on_walled_map({army}, {house, {{3, 3}, 1, {1, 1}}}));
  EXPECT_TRUE(refused_on_walled_map({army}, {{{1, 1}, 1, {0, 3}}}));
}

TEST(World, ObjectiveTheArmyCannotReachIsRefused) {
  // A row of three cells sealing the wall's only gap, in the bottom row of
  // walled_map().
  const Structure seal{{7, 8}, 1, {3, 1}};
  const ArmySpec west{{1, 1}, {1, 1}, 1, 1, 0, {IDLE}};
  World world{Seed{1}, Map{walled_map()}, {west}, {seal}, peaceful(0)};
  EXPECT_NO_THROW(world.order(0, {Cell{3, 3}, {0}}));
  EXPECT_THROW(world.order(0, {Cell{13, 1}, {0}}), std::invalid_argument);

  // Control: created bound for the far side, the same army is refused, and
  // accepted with the gap open.
  const ArmySpec bound{{1, 1}, {13, 1}, 1, 1, 0, {IDLE}};
  EXPECT_TRUE(refused_on_walled_map({bound}, {seal}));
  EXPECT_FALSE(refused_on_walled_map({bound}, {}));
}

// A village of two 2 by 2 houses along the top of a 6 by 3 area from (8, 8),
// its residents of faction 0 below them; a raider of faction 1 at war with
// them; and a mender of faction 2, at peace with both.
constexpr std::size_t RESIDENTS = 0;
constexpr std::size_t RAIDER = 1;
constexpr std::size_t MENDER = 2;
constexpr Cell RAIDER_HOME{2, 9};
constexpr Cell MENDER_HOME{20, 20};
constexpr Cell IN_VILLAGE{11, 10};
constexpr std::uint32_t HOUSE_DURABILITY = 100;
constexpr std::uint64_t PHASE_TICKS = 2'000;
constexpr Seed VILLAGE_SEED{5};
// The houses' west columns, and the rows they cover.
constexpr std::int32_t WEST_HOUSE = 8;
constexpr std::int32_t EAST_HOUSE = 12;
constexpr std::int32_t HOUSE_TOP = 8;
constexpr Footprint SMALL_HOUSE{2, 2};
constexpr std::int64_t TORCH_REACH = std::int64_t{3} * CELL_SIZE;

VillageSpec small_village() { return VillageSpec{{0, 1}, RESIDENTS}; }

World village_world(std::uint32_t residents) {
  const Skill guard{3 * CELL_SIZE, 0, 1, 0, 15};
  // The raider strikes the residents, and torches what stands around it.
  const Skill strike{3 * CELL_SIZE, 0, 50, 0, 10};
  const Skill torch{0, 3 * CELL_SIZE, 0, 10, 10};
  const Skill mend{0, 3 * CELL_SIZE, 0, -2, 10};
  const std::vector<ArmySpec> armies{
      {{10, 10},
       {10, 10},
       residents,
       std::uint64_t{residents} * 100,
       0,
       {guard}},
      {RAIDER_HOME, RAIDER_HOME, 10, 100'000, 1, {strike, torch}},
      {MENDER_HOME, MENDER_HOME, 10, 100'000, 2, {mend}},
  };
  const Rules rules{
      SEPARATION,
      {{false, true, false}, {true, false, false}, {false, false, false}},
      STAGE_FLOORS};
  return World{VILLAGE_SEED,
               Map{open_map(ORDERS_SIDE)},
               armies,
               {{{WEST_HOUSE, HOUSE_TOP}, HOUSE_DURABILITY, SMALL_HOUSE},
                {{EAST_HOUSE, HOUSE_TOP}, HOUSE_DURABILITY, SMALL_HOUSE}},
               rules,
               {small_village()}};
}

// Whether the nearest point of the small house whose west column is `left`
// lies within the torch's reach of `position`.
bool torch_reaches(std::int32_t left, Position position) {
  const auto gap = [](std::int64_t point, std::int32_t first,
                      std::int32_t cells) {
    return std::max<std::int64_t>(
        {(std::int64_t{first} * CELL_SIZE) - point, 0,
         point - (((std::int64_t{first} + cells) * CELL_SIZE) - 1)});
  };
  const auto gap_x = gap(position.x, left, SMALL_HOUSE.width);
  const auto gap_y = gap(position.y, HOUSE_TOP, SMALL_HOUSE.height);
  return (gap_x * gap_x) + (gap_y * gap_y) <= TORCH_REACH * TORCH_REACH;
}

// The village's state worked out again from what the world shows: its
// houses' stages, and whether a living raider stands within its torch's
// reach of a house while the raider is at war with the owner.
VillageState derived(const World& world) {
  const auto all_in = [&world](Stage stage) {
    return world.stage(0) == stage && world.stage(1) == stage;
  };
  bool threatened = false;
  for (const Body& body : world.bodies(RAIDER)) {
    threatened = threatened || (body.count > 0 && world.owner(0) == 0 &&
                                (torch_reaches(WEST_HOUSE, body.position) ||
                                 torch_reaches(EAST_HOUSE, body.position)));
  }
  if (all_in(Stage::RUBBLE)) {
    return VillageState::RUINS;
  }
  if (threatened) {
    return VillageState::RAIDED;
  }
  return all_in(Stage::INTACT) ? VillageState::PROSPEROUS
                               : VillageState::REBUILDING;
}

// Ticks until `done` holds or PHASE_TICKS pass, recording each state the
// village enters and whether its state ever differed from derived().
void run_until(World& world, std::vector<VillageState>& states, bool& agreed,
               const std::function<bool()>& done) {
  for (std::uint64_t tick = 0; tick < PHASE_TICKS && !done(); ++tick) {
    world.set_focus(centre_of(IN_VILLAGE));
    world.tick();
    agreed = agreed && world.village_state(0) == derived(world);
    if (world.village_state(0) != states.back()) {
      states.push_back(world.village_state(0));
    }
  }
}

// The army and faction that ruined village `village`, or none.
std::pair<std::size_t, std::size_t> destroyer_of(const World& world,
                                                 std::size_t village) {
  constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();
  const Destroyer who =
      world.destroyer(village).value_or(Destroyer{NONE, NONE});
  return {who.army, who.faction};
}

TEST(World, VillageWalksFromProsperityThroughRuinsBackUnderItsRebuilder) {
  World world = village_world(4);
  std::vector<VillageState> states{world.village_state(0)};
  bool agreed = true;

  world.order(RAIDER, {IN_VILLAGE, {0, 1}});
  run_until(world, states, agreed, [&world] {
    return world.village_state(0) == VillageState::RUINS &&
           totals(world.bodies(RESIDENTS)).count == 0;
  });
  // A state stored when the ruins were reached.
  const VillageState stored = world.village_state(0);
  world.order(RAIDER, {RAIDER_HOME, {0, 1}});
  run_until(world, states, agreed, [&world] {
    return in(RAIDER_HOME, world.bodies(RAIDER).front().position);
  });
  world.order(MENDER, {IN_VILLAGE, {0}});
  run_until(world, states, agreed, [&world] {
    return world.village_state(0) == VillageState::PROSPEROUS;
  });

  const std::vector<VillageState> cycle{
      VillageState::PROSPEROUS, VillageState::RAIDED, VillageState::RUINS,
      VillageState::REBUILDING, VillageState::PROSPEROUS};
  EXPECT_EQ(states, cycle);
  EXPECT_TRUE(agreed);
  // Control: the state stored at the ruins no longer describes the village.
  EXPECT_EQ(stored, VillageState::RUINS);
  EXPECT_NE(stored, world.village_state(0));
  EXPECT_EQ(world.owner(0), 2U);
  EXPECT_EQ(destroyer_of(world, 0), (std::pair{RAIDER, std::size_t{1}}));
}

TEST(World, RepairRestoresDurabilityUpToTheIntactFloorAndNoFurther) {
  const Skill mend{0, 2 * CELL_SIZE, 0, -2, 10};
  const ArmySpec mender{{3, 3}, {3, 3}, 1, 1, 0, {mend}};
  constexpr std::uint32_t BROKEN = 30;
  constexpr std::uint32_t ABOVE_FLOOR = 100;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {mender},
              {{{4, 3}, BROKEN}, {{2, 3}, ABOVE_FLOOR}},
              peaceful(SEPARATION)};
  world.set_focus(centre_of(mender.start));
  run_ticks(world, ARRIVAL_TICKS);

  EXPECT_EQ(world.structure(0).durability, STAGE_FLOORS.front());
  // Control: a structure above the floor is left as it was.
  EXPECT_EQ(world.structure(1).durability, ABOVE_FLOOR);
}

TEST(World, RebuilderBecomesTheOwnerOnlyWhereNoResidentLives) {
  const auto owner_after_mending = [](std::uint32_t residents) {
    World world = village_world(residents);
    world.order(RAIDER, {IN_VILLAGE, {0, 1}});
    std::vector<VillageState> states{world.village_state(0)};
    bool agreed = true;
    run_until(world, states, agreed, [&world] {
      return world.village_state(0) == VillageState::RUINS;
    });
    world.order(RAIDER, {RAIDER_HOME, {0, 1}});
    world.order(MENDER, {IN_VILLAGE, {0}});
    run_until(world, states, agreed, [] { return false; });
    return std::pair{world.owner(0), totals(world.bodies(RESIDENTS)).count};
  };

  const auto [taken_over, none_left] = owner_after_mending(4);
  EXPECT_EQ(none_left, 0U);
  EXPECT_EQ(taken_over, 2U);
  // Control: residents too strong for the raid keep the village.
  const auto [kept, survivors] = owner_after_mending(400);
  EXPECT_GT(survivors, 0U);
  EXPECT_EQ(kept, 0U);
}

TEST(World, VillageNamingAMissingOrClaimedStructureOrArmyIsRefused) {
  const ArmySpec army{{0, 0}, {0, 0}, 1, 1, 0, {IDLE}};
  const std::vector<Structure> houses{{{4, 4}, 1}, {{6, 4}, 1}};
  const auto refused = [&](const std::vector<VillageSpec>& villages) {
    try {
      const World world{Seed{1},     Map{open_map(OPEN_SIDE)},
                        {army},      houses,
                        peaceful(0), villages};
      return false;
    } catch (const std::invalid_argument&) {
      return true;
    }
  };
  EXPECT_FALSE(refused({{{0, 1}, 0}}));

  // A structure the world lacks, one two villages name, an army it lacks.
  EXPECT_TRUE(refused({{{0, 2}, 0}}));
  EXPECT_TRUE(refused({{{0}, 0}, {{1}, 0}, {{0}, 0}}));
  EXPECT_TRUE(refused({{{0, 1}, 1}}));
}

// The durability left on a village's house after a torch-bearer of
// `faction` stands beside it, the village's residents being of faction 0.
std::uint32_t house_after_torch(std::size_t faction) {
  const Skill torch{0, 2 * CELL_SIZE, 0, 10, 10};
  const std::vector<ArmySpec> armies{
      {{1, 1}, {1, 1}, 1, 1, 0, {IDLE}},
      {{4, 5}, {4, 5}, 1, 1, faction, {torch}},
  };
  World world{Seed{1},   Map{open_map(OPEN_SIDE)},
              armies,    {{{4, 4}, HOUSE_DURABILITY}},
              at_war(0), {{{0}, 0}}};
  world.set_focus(centre_of(armies.at(1).start));
  run_ticks(world, ARRIVAL_TICKS);
  return world.structure(0).durability;
}

TEST(World, VillageWithLivingResidentsTakesNoDamageFromItsOwnFaction) {
  EXPECT_EQ(house_after_torch(0), HOUSE_DURABILITY);
  // Control: the same torch in hostile hands burns the house.
  EXPECT_LT(house_after_torch(1), HOUSE_DURABILITY);
}

}  // namespace
}  // namespace murim
