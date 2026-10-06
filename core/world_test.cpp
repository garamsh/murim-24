#include "world.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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
            0xC4F8BB5BCC33A95ULL);
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

}  // namespace
}  // namespace murim
