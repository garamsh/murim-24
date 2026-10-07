#include "world.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "map.hpp"
#include "spatial_hash.hpp"

namespace murim {
namespace {

constexpr int TICKS = 10'000;
constexpr int ARRIVAL_TICKS = 3'000;
// People stand a cell apart, as wide as a body is drawn.
constexpr std::int32_t SEPARATION = CELL_SIZE;
constexpr std::size_t OPEN_SIDE = 16;
constexpr std::array<std::uint32_t, 3> STAGE_FLOORS{75, 25, 1};
constexpr std::uint64_t NEVER_AGAIN = 1'000'000;

// A skill that never finds a target: its range reaches only its user, who
// is hostile to no one, and it does nothing.
constexpr Skill IDLE{0, 0, 0, 0, 0};

bool in(Cell cell, Position position) {
  const Cell holding = cell_of(position);
  return holding.x == cell.x && holding.y == cell.y;
}

std::int64_t distance_squared(Position origin, Position point) {
  const std::int64_t offset_x = std::int64_t{point.x} - origin.x;
  const std::int64_t offset_y = std::int64_t{point.y} - origin.y;
  return (offset_x * offset_x) + (offset_y * offset_y);
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

// A head count of people, as an army is created with.
struct Muster {
  std::size_t people;
  std::uint64_t health;
};

// `muster.people` people of `muster.health` each on the passable cells of
// `rows` nearest `start`, by Chebyshev distance and then row by row.
std::vector<Recruit> recruits(const std::vector<std::string>& rows, Cell start,
                              Muster muster) {
  const auto passable = [&rows](Cell cell) {
    return cell.y >= 0 && cell.x >= 0 &&
           static_cast<std::size_t>(cell.y) < rows.size() &&
           static_cast<std::size_t>(cell.x) < rows.front().size() &&
           rows.at(static_cast<std::size_t>(cell.y))
                   .at(static_cast<std::size_t>(cell.x)) != '#';
  };
  std::vector<Recruit> members;
  for (std::int32_t ring = 0; members.size() < muster.people; ++ring) {
    const Cell corner{start.x - ring, start.y - ring};
    for (const Cell cell : cells_of(corner, {(2 * ring) + 1, (2 * ring) + 1})) {
      const bool on_ring = std::max(std::abs(cell.x - start.x),
                                    std::abs(cell.y - start.y)) == ring;
      if (on_ring && passable(cell) && members.size() < muster.people) {
        members.push_back(Recruit{cell, muster.health});
      }
    }
  }
  return members;
}

// An army of `people` people of `health` each around `start`, on `rows`.
ArmySpec army(const std::vector<std::string>& rows, Cell start, Cell objective,
              std::size_t people, std::uint64_t health, std::size_t faction,
              std::vector<Skill> skills) {
  return ArmySpec{objective, recruits(rows, start, Muster{people, health}),
                  faction, std::move(skills)};
}

// An army of `people` people around `start` on an open map.
ArmySpec army(Cell start, Cell objective, std::size_t people,
              std::uint64_t health, std::size_t faction,
              std::vector<Skill> skills) {
  return army(open_map(OPEN_SIDE), start, objective, people, health, faction,
              std::move(skills));
}

// The health army `army`'s living members have left together.
std::uint64_t health_of(const World& world, std::size_t army) {
  std::uint64_t health = 0;
  for (const std::size_t member : world.roster(army)) {
    health += world.person(member).health;
  }
  return health;
}

// Where army `army`'s living members stand.
std::vector<Position> positions(const World& world, std::size_t army) {
  std::vector<Position> standing;
  for (const std::size_t member : world.roster(army)) {
    standing.push_back(world.person(member).position);
  }
  return standing;
}

Position lead(const World& world, std::size_t army) {
  return world.person(world.roster(army).front()).position;
}

std::int64_t closest_pair_squared(const std::vector<Position>& standing) {
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  for (std::size_t i = 0; i < standing.size(); ++i) {
    for (std::size_t j = i + 1; j < standing.size(); ++j) {
      closest =
          std::min(closest, distance_squared(standing.at(i), standing.at(j)));
    }
  }
  return closest;
}

void run_ticks(World& world, std::uint64_t ticks) {
  for (std::uint64_t i = 0; i < ticks; ++i) {
    world.tick();
  }
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
  // From each corner to the opposite one: how many, of what health, of
  // which faction, with which skills.
  struct Crossing {
    Cell start;
    Cell objective;
    Muster muster;
    std::size_t faction;
    std::vector<Skill> skills;
  };
  const std::vector<Crossing> crossings{
      {{1, 1}, {46, 46}, {12, 10}, 0, {strike, blast}},
      {{46, 1}, {1, 46}, {30, 10}, 1, {strike}},
      {{1, 46}, {46, 1}, {5, 10}, 1, {blast, strike}},
      {{46, 46}, {1, 1}, {2, 10}, 0, {blast}},
  };
  const std::vector<std::string> rows = crossing_map();
  std::vector<ArmySpec> armies;
  armies.reserve(crossings.size());
  for (const Crossing& crossing : crossings) {
    armies.push_back(ArmySpec{crossing.objective,
                              recruits(rows, crossing.start, crossing.muster),
                              crossing.faction, crossing.skills});
  }
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

// Runs crossing_armies() for TICKS ticks, and expects every army to have
// lost a member and a structure to have broken, or the run did not exercise
// what equal hashes are meant to cover.
StateHash run_crossing(Seed seed) {
  const std::vector<ArmySpec> armies = crossing_armies();
  World world{seed, Map{crossing_map()}, armies, crossing_structures(),
              at_war(SEPARATION)};
  run_ticks(world, TICKS);
  for (std::size_t army = 0; army < armies.size(); ++army) {
    EXPECT_LT(world.roster(army).size(), armies.at(army).members.size())
        << "army " << army;
  }
  bool broken = false;
  for (std::size_t structure = 0; structure < crossing_structures().size();
       ++structure) {
    broken = broken || world.stage(structure) != Stage::INTACT;
  }
  EXPECT_TRUE(broken);
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
            0x17F3F597F63D7FEAULL);
}

TEST(World, PeopleReachTheirObjectiveAroundAWall) {
  const std::vector<std::string> rows = walled_map();
  const ArmySpec marching = army(rows, {2, 1}, {13, 1}, 9, 1, 0, {IDLE});
  World world{Seed{1}, Map{rows}, {marching}, {}, peaceful(SEPARATION)};
  run_ticks(world, ARRIVAL_TICKS);

  // Every member crossed the wall through the gap, and the first stands on
  // the objective.
  for (const Position where : positions(world, 0)) {
    EXPECT_GT(cell_of(where).x, 8);
  }
  EXPECT_TRUE(in(marching.objective, lead(world, 0)));
}

// Where an army of nine stands after ARRIVAL_TICKS at its objective, with
// `separation`.
std::vector<Position> arrive(std::int32_t separation) {
  const ArmySpec marching = army({2, 2}, {12, 12}, 9, 1, 0, {IDLE});
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, {marching}, {}, peaceful(separation)};
  run_ticks(world, ARRIVAL_TICKS);
  return positions(world, 0);
}

TEST(World, PeopleAtTheirObjectiveStandTheSeparationApart) {
  const std::int64_t apart = std::int64_t{SEPARATION} * SEPARATION;
  const std::vector<Position> separated = arrive(SEPARATION);
  // Setup: the army arrived, its lead within a cell of the objective.
  ASSERT_LE(distance_squared(separated.front(), centre_of({12, 12})),
            std::int64_t{CELL_SIZE} * CELL_SIZE);
  EXPECT_GE(closest_pair_squared(separated), apart);
  // Control: without separation the people pack closer.
  EXPECT_LT(closest_pair_squared(arrive(0)), apart);
}

// The closest any two members of a column of nine come while it marches
// east across a 32-cell map, once it has spread out from its start and
// before its first members near the objective and stand.
std::int64_t closest_while_marching(std::int32_t separation) {
  constexpr std::size_t SIDE = 32;
  constexpr int SPREAD_TICKS = 100;
  constexpr int MARCH_TICKS = 300;
  const std::vector<std::string> rows = open_map(SIDE);
  const ArmySpec column = army(rows, {2, 16}, {29, 16}, 9, 1, 0, {IDLE});
  World world{Seed{1}, Map{rows}, {column}, {}, peaceful(separation)};
  run_ticks(world, SPREAD_TICKS);
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  for (int tick = 0; tick < MARCH_TICKS; ++tick) {
    world.tick();
    closest = std::min(closest, closest_pair_squared(positions(world, 0)));
  }
  // Setup: the column marched, so the distances were measured on the move.
  EXPECT_GT(cell_of(lead(world, 0)).x, 10);
  return closest;
}

TEST(World, MarchingPeopleKeepNearlyTheSeparationApart) {
  // Within the distance one tick's step can close on each side.
  constexpr std::int64_t KEPT = SEPARATION - (2 * SPEED);
  EXPECT_GE(closest_while_marching(SEPARATION), KEPT * KEPT);
  // Control: without separation the column bunches far closer.
  EXPECT_LT(closest_while_marching(0), KEPT * KEPT);
}

TEST(World, APersonOrderedOutOfAStandingCrowdLeavesIt) {
  constexpr std::size_t SIDE = 24;
  constexpr Cell CROWD{12, 12};
  constexpr Cell OUTSIDE{20, 12};
  constexpr int EXIT_TICKS = 400;
  const std::vector<std::string> rows = open_map(SIDE);
  // A crowd of 24 standing on every cell around the centre, two rings deep,
  // and one person in its middle.
  constexpr Muster TWO_RINGS{25, 1};
  std::vector<Recruit> ring = recruits(rows, CROWD, TWO_RINGS);
  ring.erase(ring.begin());
  const std::vector<ArmySpec> armies{
      ArmySpec{CROWD, ring, 0, {IDLE}},
      ArmySpec{CROWD, {Recruit{CROWD, 1}}, 0, {IDLE}},
  };
  const auto after = [&](bool ordered) {
    World world{Seed{1}, Map{rows}, armies, {}, peaceful(SEPARATION)};
    if (ordered) {
      world.order(1, {OUTSIDE, {0}});
    }
    run_ticks(world, EXIT_TICKS);
    return lead(world, 1);
  };

  EXPECT_TRUE(in(OUTSIDE, after(true)));
  // Control: left without the order, the person stays in the crowd.
  EXPECT_LE(distance_squared(after(false), centre_of(CROWD)),
            std::int64_t{2} * CELL_SIZE * 2 * CELL_SIZE);
}

// The health `target` lost in one tick in which `user` strikes it; both
// hold their ground.
std::uint64_t health_lost(const ArmySpec& user, const ArmySpec& target,
                          const Rules& rules) {
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, {user, target}, {}, rules};
  world.tick();
  std::uint64_t start = 0;
  for (const Recruit& recruit : target.members) {
    start += recruit.health;
  }
  return start - health_of(world, 1);
}

// Health each single-person target starts with in the combat tests.
constexpr std::uint64_t TARGET_HEALTH = 1'000'000;

TEST(World, OnlyHostilePeopleWithinRangeUseTheirSkills) {
  const Skill reach{2 * CELL_SIZE, 0, 1, 0, NEVER_AGAIN};
  const Skill short_reach{(2 * CELL_SIZE) - 1, 0, 1, 0, NEVER_AGAIN};
  const ArmySpec target = army({4, 0}, {4, 0}, 1, TARGET_HEALTH, 1, {IDLE});
  const ArmySpec user = army({2, 0}, {2, 0}, 1, 1, 0, {reach});
  ArmySpec ally = target;
  ally.faction = 0;

  EXPECT_EQ(health_lost(user, target, at_war(0)), 1U);
  // Controls: one unit short of the range, or a target of a faction the user
  // is not hostile to, and nothing is used.
  ArmySpec out_of_range = user;
  out_of_range.skills = {short_reach};
  EXPECT_EQ(health_lost(out_of_range, target, at_war(0)), 0U);
  EXPECT_EQ(health_lost(user, ally, at_war(0)), 0U);
}

TEST(World, EquallyNearTargetsGoToTheLowestIndex) {
  const ArmySpec user =
      army({4, 4}, {4, 4}, 1, 1, 0, {{3 * CELL_SIZE, 0, 1, 0, NEVER_AGAIN}});
  // The lower index stands east, where a query visits it last.
  const ArmySpec east = army({6, 4}, {6, 4}, 1, TARGET_HEALTH, 1, {IDLE});
  const ArmySpec west = army({2, 4}, {2, 4}, 1, TARGET_HEALTH, 1, {IDLE});
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, {user, east, west}, {}, at_war(0)};
  world.tick();

  EXPECT_EQ(health_of(world, 1), TARGET_HEALTH - 1);
  // Control: the other target, as near, is untouched.
  EXPECT_EQ(health_of(world, 2), TARGET_HEALTH);
}

// The cells (2..6, 0..6), each holding a single-person hostile army in
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
  const ArmySpec user = army({3, 3}, {3, 3}, 1, 1, 0,
                             {{2 * CELL_SIZE, MAX_REACH, 1, 0, NEVER_AGAIN}});
  const ArmySpec target = army({4, 3}, {4, 3}, 1, TARGET_HEALTH, 1, {IDLE});
  // The walk of the strike's area query over the people the world indexes,
  // who stand at their start cells' centres.
  const SpatialHash crowd{{centre_of({3, 3}), centre_of({4, 3})}};
  const auto [first, last] = crowd.walk(centre_of({4, 3}), MAX_REACH);
  const std::int64_t cells = (std::int64_t{last.x} - first.x + 1) *
                             (std::int64_t{last.y} - first.y + 1);
  // Fatal, so an unbounded walk fails here rather than stalling the strike.
  ASSERT_LE(cells, std::int64_t{OPEN_SIDE * OPEN_SIDE});

  EXPECT_EQ(health_lost(user, target, at_war(0)), 1U);
}

// Fires one skill of `radius` from (0, 3) at target_grid() and returns the
// cells of the people it damaged.
Cells damaged_cells(std::int32_t radius) {
  std::vector<ArmySpec> armies{army(
      {0, 3}, {0, 3}, 1, 1, 0, {{3 * CELL_SIZE, radius, 1, 0, NEVER_AGAIN}})};
  for (const Cell cell : target_grid()) {
    armies.push_back(army(cell, cell, 1, TARGET_HEALTH, 1, {IDLE}));
  }
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(0)};
  world.tick();
  Cells damaged;
  for (std::size_t target = 1; target < armies.size(); ++target) {
    if (health_of(world, target) < TARGET_HEALTH) {
      const Cell cell = armies.at(target).members.front().cell;
      damaged.emplace_back(cell.x, cell.y);
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

TEST(World, AreaSkillHitsExactlyThePeopleInsideItsShape) {
  const std::int32_t radius = 2 * CELL_SIZE;
  const std::pair<std::int32_t, std::int32_t> edge{4, 3};

  const auto hit = damaged_cells(radius);
  EXPECT_EQ(hit, cells_inside(radius));
  // The person on (4, 3) stands exactly on the edge, so the shape takes it in.
  EXPECT_NE(std::find(hit.begin(), hit.end(), edge), hit.end());
  // Control: one unit less radius leaves that person just outside.
  const auto narrower = damaged_cells(radius - 1);
  EXPECT_EQ(narrower, cells_inside(radius - 1));
  EXPECT_EQ(std::find(narrower.begin(), narrower.end(), edge), narrower.end());
}

TEST(World, AStrikeHitsEachHostilePersonInItsDiscOnce) {
  constexpr std::uint64_t DAMAGE = 7;
  // Three hostile people in a row, every one inside the disc.
  const Skill sweep{3 * CELL_SIZE, 2 * CELL_SIZE, DAMAGE, 0, NEVER_AGAIN};
  const ArmySpec user = army({0, 3}, {0, 3}, 1, 1, 0, {sweep});
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const ArmySpec three{
      {3, 3},
      {Recruit{{2, 3}, TARGET_HEALTH}, Recruit{{3, 3}, TARGET_HEALTH},
       Recruit{{4, 3}, TARGET_HEALTH}},
      1,
      {IDLE}};
  World world{Seed{1}, Map{rows}, {user, three}, {}, at_war(0)};
  world.tick();

  for (const std::size_t member : world.roster(1)) {
    EXPECT_EQ(world.person(member).health, TARGET_HEALTH - DAMAGE);
  }
  // Control: the user, its own disc's centre aside, takes nothing.
  EXPECT_EQ(health_of(world, 0), 1U);
}

// Two people on neighbouring cells: one with `user_skill` and as much health
// as anyone may have, and its target with as much.
std::vector<ArmySpec> at_the_limits(const Skill& user_skill) {
  return {
      army({3, 3}, {3, 3}, 1, MAX_HEALTH, 0, {user_skill}),
      army({4, 3}, {4, 3}, 1, MAX_HEALTH, 1, {IDLE}),
  };
}

// The skill of at_the_limits(): the most damage, and a cooldown of 1.
constexpr Skill LIMIT_SKILL{2 * CELL_SIZE, 0, MAX_BODY_DAMAGE, 0, 1};

TEST(World, StrikeAtTheLargestAcceptedValuesKeepsHealthExact) {
  constexpr std::uint64_t STRIKES = 16;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              at_the_limits(LIMIT_SKILL),
              {},
              at_war(0)};
  run_ticks(world, STRIKES);

  EXPECT_EQ(health_of(world, 1), MAX_HEALTH - (MAX_BODY_DAMAGE * STRIKES));
  EXPECT_EQ(health_of(world, 0), MAX_HEALTH);
}

// Whether the constructor refuses `armies` with std::invalid_argument.
bool refused(const std::vector<ArmySpec>& armies) {
  try {
    const World world{Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(0)};
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

TEST(World, InputJustPastALimitIsRefused) {
  std::vector<ArmySpec> health = at_the_limits(LIMIT_SKILL);
  ++health.at(1).members.front().health;
  std::vector<ArmySpec> lifeless = at_the_limits(LIMIT_SKILL);
  lifeless.at(1).members.front().health = 0;
  Skill damage = LIMIT_SKILL;
  ++damage.body_damage;
  Skill cooldown = LIMIT_SKILL;
  cooldown.cooldown = MAX_COOLDOWN + 1;
  Skill longest = LIMIT_SKILL;
  longest.cooldown = MAX_COOLDOWN;
  // Control: the values at every limit are accepted.
  EXPECT_FALSE(refused(at_the_limits(longest)));
  EXPECT_TRUE(refused(health));
  EXPECT_TRUE(refused(lifeless));
  EXPECT_TRUE(refused(at_the_limits(damage)));
  EXPECT_TRUE(refused(at_the_limits(cooldown)));
}

// Whether a world of `people` people, standing on a map twice as wide as
// the open one, is created.
bool populated(std::size_t people) {
  const std::vector<std::string> rows = open_map(OPEN_SIDE * 2);
  try {
    const World world{Seed{1},
                      Map{rows},
                      {army(rows, {0, 0}, {0, 0}, people, 1, 0, {IDLE})},
                      {},
                      peaceful(0)};
  } catch (const std::invalid_argument&) {
    return false;
  }
  return true;
}

TEST(World, AWorldHoldsNoMoreThanMaxPeople) {
  EXPECT_FALSE(populated(MAX_PEOPLE + 1));
  // Control: as many as a world may hold are accepted.
  EXPECT_TRUE(populated(MAX_PEOPLE));
}

TEST(World, AreaSkillWalksAStructureThroughEveryStage) {
  constexpr std::uint32_t DURABILITY = 100;
  // The disc around the target reaches just into the structure beside it.
  const ArmySpec user =
      army({0, 0}, {0, 0}, 1, 1, 0, {{2 * CELL_SIZE, CELL_SIZE / 2, 0, 30, 1}});
  const ArmySpec target = army({2, 0}, {2, 0}, 1, TARGET_HEALTH, 1, {IDLE});
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {user, target},
              {{{3, 0}, DURABILITY}},
              at_war(0)};
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

// Fights twelve people using `first` against twelve using `second`, two
// blocks marching on one point, until a side falls or TICKS pass, and
// returns the health each side has left.
std::array<std::uint64_t, 2> fight(const Skill& first, const Skill& second) {
  constexpr std::size_t PEOPLE = 12;
  constexpr std::uint64_t HEALTH = 100;
  const std::vector<ArmySpec> armies{
      army({3, 7}, {7, 7}, PEOPLE, HEALTH, 0, {first}),
      army({11, 7}, {7, 7}, PEOPLE, HEALTH, 1, {second}),
  };
  World world{
      Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(SEPARATION)};
  for (int i = 0;
       i < TICKS && !world.roster(0).empty() && !world.roster(1).empty(); ++i) {
    world.tick();
  }
  return {health_of(world, 0), health_of(world, 1)};
}

TEST(World, AreaSkillBeatsSingleTargetSkill) {
  const Skill single{2 * CELL_SIZE, 0, 4, 0, 24};
  const Skill area{2 * CELL_SIZE, 2 * CELL_SIZE, 4, 0, 24};
  const std::array<std::uint64_t, 2> left = fight(area, single);
  EXPECT_GT(left.at(0), 0U);
  EXPECT_EQ(left.at(1), 0U);
}

TEST(World, QuickSkillBeatsSlowSkill) {
  const Skill quick{2 * CELL_SIZE, 0, 4, 0, 12};
  const Skill slow{2 * CELL_SIZE, 0, 4, 0, 24};
  const std::array<std::uint64_t, 2> left = fight(slow, quick);
  EXPECT_EQ(left.at(0), 0U);
  EXPECT_GT(left.at(1), 0U);
}

// A village of one house with three residents at (4, 4), their home; a
// field army of faction 0 with no members yet; and a hostile post of one
// person at (12, 4).
constexpr std::size_t HOME_ARMY = 0;
constexpr std::size_t FIELD = 1;
constexpr std::size_t POST = 2;
constexpr Cell HOME{4, 4};
constexpr Cell POST_CELL{12, 4};
constexpr std::uint64_t POST_HEALTH = 200;
constexpr std::uint32_t HOME_DURABILITY = 100;

World conscription_world() {
  const Skill strike{2 * CELL_SIZE, 0, 10, 0, 15};
  const std::vector<ArmySpec> armies{
      army(HOME, HOME, 3, 1'000, 0, {strike}),
      ArmySpec{HOME, {}, 0, {strike}},
      army(POST_CELL, POST_CELL, 1, POST_HEALTH, 1, {strike}),
  };
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               armies,
               {{{2, 2}, HOME_DURABILITY}},
               at_war(SEPARATION),
               {{{0}, HOME_ARMY, {}}}};
}

TEST(World, APersonKeepsItsIdentityThroughConscriptionBattleAndDisbanding) {
  World world = conscription_world();
  const std::size_t recruit = world.roster(HOME_ARMY).back();
  ASSERT_EQ(world.person(recruit).home, std::optional<std::size_t>{0});

  world.conscript(recruit, FIELD);
  EXPECT_EQ(world.roster(FIELD), std::vector<std::size_t>{recruit});
  world.order(FIELD, {POST_CELL, {0}});
  run_ticks(world, ARRIVAL_TICKS / 2);
  // The battle wounded the recruit and killed the post.
  EXPECT_LT(world.person(recruit).health, 1'000U);
  EXPECT_TRUE(world.roster(POST).empty());
  EXPECT_EQ(world.person(world.population() - 1).health, 0U);

  const std::uint64_t wounded = world.person(recruit).health;
  world.disband(FIELD);
  EXPECT_TRUE(world.roster(FIELD).empty());
  EXPECT_EQ(world.roster(HOME_ARMY).back(), recruit);
  // Control: the person home is the one conscripted, wounds and all.
  EXPECT_EQ(world.person(recruit).health, wounded);
  EXPECT_EQ(world.person(recruit).home, std::optional<std::size_t>{0});
}

TEST(World, ConscriptingAndDisbandingKeepEveryPersonOnOneRoster) {
  World world = conscription_world();
  const auto on_rosters = [&world] {
    return world.roster(HOME_ARMY).size() + world.roster(FIELD).size() +
           world.roster(POST).size();
  };
  const std::size_t start = on_rosters();
  const std::size_t stranger = world.roster(POST).front();
  const std::vector<std::size_t> residents = world.roster(HOME_ARMY);
  for (const std::size_t resident : residents) {
    world.conscript(resident, FIELD);
  }
  world.conscript(stranger, FIELD);
  EXPECT_EQ(world.roster(HOME_ARMY).size(), 0U);
  EXPECT_EQ(on_rosters(), start);

  world.disband(FIELD);
  EXPECT_EQ(world.roster(HOME_ARMY).size(), 3U);
  EXPECT_EQ(on_rosters(), start);
  // Control: a member with no home stays where it served.
  EXPECT_EQ(world.roster(FIELD), std::vector<std::size_t>{stranger});
}

TEST(World, OnlyALivingPersonCanBeConscripted) {
  World world = conscription_world();
  const std::size_t post = world.roster(POST).front();
  world.order(POST, {HOME, {0}});
  run_ticks(world, ARRIVAL_TICKS);
  // Setup: the post marched on the village and died.
  ASSERT_EQ(world.person(post).health, 0U);

  EXPECT_THROW(world.conscript(post, FIELD), std::invalid_argument);
  EXPECT_THROW(world.conscript(world.population(), FIELD),
               std::invalid_argument);
  // Control: a living resident is conscripted.
  EXPECT_NO_THROW(world.conscript(world.roster(HOME_ARMY).front(), FIELD));
}

// An order given before tick `tick` of a run.
struct LoggedOrder {
  std::uint64_t tick;
  std::size_t army;
  Order order;
};

// Ticks `world` `ticks` times as a driving adapter does, giving each logged
// order before its tick. Returns the orders given, as a recording.
std::vector<LoggedOrder> drive(World& world,
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
    world.tick();
  }
  return given;
}

constexpr std::size_t STEERED = 0;
constexpr std::size_t ORDERS_SIDE = 24;
constexpr std::uint64_t ORDER_TICKS = 1'500;
constexpr Cell STEERED_HOME{2, 2};
constexpr Cell HOSTILE_HOME{12, 12};
constexpr Cell BESIDE_HOSTILE{12, 9};
constexpr Seed ORDERS_SEED{7};
constexpr std::uint64_t HOSTILE_HEALTH = 1'000;
constexpr std::size_t HOSTILES = 6;
constexpr std::uint32_t DURABILITY = 100;

// One person at STEERED_HOME with a skill that spares structures and one
// that breaks them, bound for `objective`; six hostile people holding
// HOSTILE_HOME, with a structure between them and BESIDE_HOSTILE.
World orders_world(Cell objective) {
  const Skill strike{3 * CELL_SIZE, 0, 50, 0, 10};
  const Skill smash{3 * CELL_SIZE, CELL_SIZE, 50, 40, 10};
  const Skill guard{3 * CELL_SIZE, 0, 1, 0, 10};
  const std::vector<std::string> rows = open_map(ORDERS_SIDE);
  const std::vector<ArmySpec> armies{
      army(rows, STEERED_HOME, objective, 1, 100'000, 0, {strike, smash}),
      army(rows, HOSTILE_HOME, HOSTILE_HOME, HOSTILES, HOSTILE_HEALTH, 1,
           {guard}),
  };
  return World{ORDERS_SEED,
               Map{rows},
               armies,
               {{{HOSTILE_HOME.x, HOSTILE_HOME.y - 2}, DURABILITY}},
               at_war(SEPARATION)};
}

std::uint64_t hostile_start() { return HOSTILE_HEALTH * HOSTILES; }

TEST(World, OrderedBodyMatchesAnNpcFollowingTheSameOrderFromData) {
  World npc = orders_world(BESIDE_HOSTILE);
  drive(npc, {}, ORDER_TICKS);
  World ordered = orders_world(STEERED_HOME);
  drive(ordered, {{0, STEERED, {BESIDE_HOSTILE, {0, 1}}}}, ORDER_TICKS);

  // Setup: the person left home and the hostile army took damage, so the
  // run covered moving and striking.
  EXPECT_FALSE(in(STEERED_HOME, lead(ordered, STEERED)));
  EXPECT_LT(health_of(ordered, 1), hostile_start());
  EXPECT_EQ(npc.hash(), ordered.hash());
  // Control: the same order with one skill fewer is another run.
  World narrower = orders_world(STEERED_HOME);
  drive(narrower, {{0, STEERED, {BESIDE_HOSTILE, {0}}}}, ORDER_TICKS);
  EXPECT_NE(npc.hash(), narrower.hash());
}

TEST(World, OrderedBodyStrikesOnlyWithTheSkillsItIsOrderedToUse) {
  World smashing = orders_world(STEERED_HOME);
  drive(smashing, {{0, STEERED, {BESIDE_HOSTILE, {1}}}}, ORDER_TICKS);
  EXPECT_LT(smashing.structure(0).durability, DURABILITY);

  // Control: ordered to the skill that spares structures, the person still
  // strikes, and the structure stands untouched.
  World striking = orders_world(STEERED_HOME);
  drive(striking, {{0, STEERED, {BESIDE_HOSTILE, {0}}}}, ORDER_TICKS);
  EXPECT_LT(health_of(striking, 1), hostile_start());
  EXPECT_EQ(striking.structure(0).durability, DURABILITY);
}

TEST(World, ReplayingARecordedOrderLogGivesTheSameHash) {
  const std::vector<LoggedOrder> issued{
      {0, STEERED, {BESIDE_HOSTILE, {0, 1}}},
      {400, STEERED, {Cell{6, 14}, {1}}},
      {900, STEERED, {BESIDE_HOSTILE, {0}}},
  };
  World live = orders_world(STEERED_HOME);
  const std::vector<LoggedOrder> recorded = drive(live, issued, ORDER_TICKS);
  ASSERT_EQ(recorded.size(), issued.size());
  World replayed = orders_world(STEERED_HOME);
  drive(replayed, recorded, ORDER_TICKS);
  EXPECT_EQ(live.hash(), replayed.hash());

  // Control: the same log with one order a tick later is another run.
  std::vector<LoggedOrder> late = recorded;
  late.at(1).tick += 1;
  World shifted = orders_world(STEERED_HOME);
  drive(shifted, late, ORDER_TICKS);
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
  const ArmySpec user =
      army({0, 5}, {0, 5}, 1, 1, 0, {{3 * CELL_SIZE, radius, 0, 1, 1}});
  const ArmySpec target = army({2, 5}, {2, 5}, 1, TARGET_HEALTH, 1, {IDLE});
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {user, target},
              {{{4, 4}, DURABILITY, HOUSE}},
              at_war(0)};
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

// The cells a person marching from (1, 5) to (12, 5) passes through, with
// `structures` standing on the map.
std::vector<Cell> march_cells(const std::vector<Structure>& structures) {
  const ArmySpec walker = army({1, 5}, {12, 5}, 1, 1, 0, {IDLE});
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {walker},
              structures,
              peaceful(SEPARATION)};
  std::vector<Cell> cells;
  for (int i = 0; i < ARRIVAL_TICKS; ++i) {
    world.tick();
    cells.push_back(cell_of(lead(world, 0)));
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
  const ArmySpec one = army({0, 0}, {0, 0}, 1, 1, 0, {IDLE});
  const Structure house{{1, 1}, 1, HOUSE};
  EXPECT_FALSE(refused_on_walled_map({one}, {house}));

  // Over the wall down column 8, over the army, over another structure,
  // and of no width.
  EXPECT_TRUE(refused_on_walled_map({one}, {{{6, 1}, 1, HOUSE}}));
  EXPECT_TRUE(refused_on_walled_map({one}, {{{0, 0}, 1, HOUSE}}));
  EXPECT_TRUE(refused_on_walled_map({one}, {house, {{3, 3}, 1, {1, 1}}}));
  EXPECT_TRUE(refused_on_walled_map({one}, {{{1, 1}, 1, {0, 3}}}));
}

TEST(World, ObjectiveTheArmyCannotReachIsRefused) {
  // A row of three cells sealing the wall's only gap, in the bottom row of
  // walled_map().
  const Structure seal{{7, 8}, 1, {3, 1}};
  const ArmySpec west = army({1, 1}, {1, 1}, 1, 1, 0, {IDLE});
  World world{Seed{1}, Map{walled_map()}, {west}, {seal}, peaceful(0)};
  EXPECT_NO_THROW(world.order(0, {Cell{3, 3}, {0}}));
  EXPECT_THROW(world.order(0, {Cell{13, 1}, {0}}), std::invalid_argument);

  // Control: created bound for the far side, the same army is refused, and
  // accepted with the gap open.
  const ArmySpec bound = army({1, 1}, {13, 1}, 1, 1, 0, {IDLE});
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

// Health of each resident a raid can kill.
constexpr std::uint64_t RESIDENT_HEALTH = 100;

VillageSpec small_village() { return VillageSpec{{0, 1}, RESIDENTS, {}}; }

World village_world(std::uint64_t health) {
  const Skill guard{3 * CELL_SIZE, 0, 1, 0, 15};
  // The raider strikes the residents, and torches what stands around it.
  const Skill strike{3 * CELL_SIZE, 0, 50, 0, 10};
  const Skill torch{0, 3 * CELL_SIZE, 0, 10, 10};
  const Skill mend{0, 3 * CELL_SIZE, 0, -2, 10};
  // Four residents in the row below the houses.
  const std::vector<ArmySpec> armies{
      ArmySpec{{10, 10},
               {Recruit{{9, 10}, health}, Recruit{{10, 10}, health},
                Recruit{{11, 10}, health}, Recruit{{12, 10}, health}},
               0,
               {guard}},
      army(open_map(ORDERS_SIDE), RAIDER_HOME, RAIDER_HOME, 10, 100'000, 1,
           {strike, torch}),
      army(open_map(ORDERS_SIDE), MENDER_HOME, MENDER_HOME, 10, 100'000, 2,
           {mend}),
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
  for (const Position where : positions(world, RAIDER)) {
    threatened = threatened ||
                 (world.owner(0) == 0 && (torch_reaches(WEST_HOUSE, where) ||
                                          torch_reaches(EAST_HOUSE, where)));
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
  World world = village_world(RESIDENT_HEALTH);
  std::vector<VillageState> states{world.village_state(0)};
  bool agreed = true;

  world.order(RAIDER, {IN_VILLAGE, {0, 1}});
  run_until(world, states, agreed, [&world] {
    return world.village_state(0) == VillageState::RUINS &&
           world.roster(RESIDENTS).empty();
  });
  // A state stored when the ruins were reached.
  const VillageState stored = world.village_state(0);
  world.order(RAIDER, {RAIDER_HOME, {0, 1}});
  run_until(world, states, agreed,
            [&world] { return in(RAIDER_HOME, lead(world, RAIDER)); });
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
  const ArmySpec mender = army({3, 3}, {3, 3}, 1, 1, 0, {mend});
  constexpr std::uint32_t BROKEN = 30;
  constexpr std::uint32_t ABOVE_FLOOR = 100;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {mender},
              {{{4, 3}, BROKEN}, {{2, 3}, ABOVE_FLOOR}},
              peaceful(SEPARATION)};
  run_ticks(world, ARRIVAL_TICKS);

  EXPECT_EQ(world.structure(0).durability, STAGE_FLOORS.front());
  // Control: a structure above the floor is left as it was.
  EXPECT_EQ(world.structure(1).durability, ABOVE_FLOOR);
}

TEST(World, RebuilderBecomesTheOwnerOnlyWhereNoResidentLives) {
  const auto owner_after_mending = [](std::uint64_t health) {
    World world = village_world(health);
    world.order(RAIDER, {IN_VILLAGE, {0, 1}});
    std::vector<VillageState> states{world.village_state(0)};
    bool agreed = true;
    run_until(world, states, agreed, [&world] {
      return world.village_state(0) == VillageState::RUINS;
    });
    world.order(RAIDER, {RAIDER_HOME, {0, 1}});
    world.order(MENDER, {IN_VILLAGE, {0}});
    run_until(world, states, agreed, [] { return false; });
    return std::pair{world.owner(0), world.roster(RESIDENTS).size()};
  };

  const auto [taken_over, none_left] = owner_after_mending(RESIDENT_HEALTH);
  EXPECT_EQ(none_left, 0U);
  EXPECT_EQ(taken_over, 2U);
  // Control: residents too strong for the raid keep the village.
  const auto [kept, survivors] = owner_after_mending(TARGET_HEALTH);
  EXPECT_GT(survivors, 0U);
  EXPECT_EQ(kept, 0U);
}

TEST(World, VillageNamingAMissingOrClaimedStructureOrArmyIsRefused) {
  const ArmySpec one = army({0, 0}, {0, 0}, 1, 1, 0, {IDLE});
  const std::vector<Structure> houses{{{4, 4}, 1}, {{6, 4}, 1}};
  const auto refused = [&](const std::vector<VillageSpec>& villages) {
    try {
      const World world{Seed{1},     Map{open_map(OPEN_SIDE)},
                        {one},       houses,
                        peaceful(0), villages};
      return false;
    } catch (const std::invalid_argument&) {
      return true;
    }
  };
  EXPECT_FALSE(refused({{{0, 1}, 0, {}}}));

  // A structure the world lacks, one two villages name, an army it lacks.
  EXPECT_TRUE(refused({{{0, 2}, 0, {}}}));
  EXPECT_TRUE(refused({{{0}, 0, {}}, {{1}, 0, {}}, {{0}, 0, {}}}));
  EXPECT_TRUE(refused({{{0, 1}, 1, {}}}));
}

// The durability left on a village's house after a torch-bearer of
// `faction` stands beside it, the village's residents being of faction 0.
std::uint32_t house_after_torch(std::size_t faction) {
  const Skill torch{0, 2 * CELL_SIZE, 0, 10, 10};
  const std::vector<ArmySpec> armies{
      army({1, 1}, {1, 1}, 1, 1, 0, {IDLE}),
      army({4, 5}, {4, 5}, 1, 1, faction, {torch}),
  };
  World world{Seed{1},   Map{open_map(OPEN_SIDE)},
              armies,    {{{4, 4}, HOUSE_DURABILITY}},
              at_war(0), {{{0}, 0, {}}}};
  run_ticks(world, ARRIVAL_TICKS);
  return world.structure(0).durability;
}

TEST(World, VillageWithLivingResidentsTakesNoDamageFromItsOwnFaction) {
  EXPECT_EQ(house_after_torch(0), HOUSE_DURABILITY);
  // Control: the same torch in hostile hands burns the house.
  EXPECT_LT(house_after_torch(1), HOUSE_DURABILITY);
}

// A Cult of faction 1 led by person 0 at CULT_SEAT, with two members of its
// own, each an army of one: the player's body (person 1) and an NPC
// (person 2). Faction 0 is at war with the Cult; faction 2 is at peace with
// it, its ally. Person 3, of faction 0, and person 4, of faction 2, stand far
// off.
constexpr std::size_t ORTHODOX = 0;
constexpr std::size_t CULT = 1;
constexpr std::size_t ALLY = 2;
constexpr std::size_t PLAYER = 1;
constexpr std::size_t NPC = 2;
constexpr std::size_t OUTSIDER = 3;
constexpr Cell CULT_SEAT{10, 10};
constexpr std::uint64_t DUEL_TICKS = 2'000;

World cult_world(std::uint64_t challenger_health,
                 std::optional<std::size_t> cult_leader = 0) {
  constexpr Seed CULT_SEED{9};
  const Skill strike{2 * CELL_SIZE, 0, 50, 0, 10};
  const std::vector<std::string> rows = open_map(ORDERS_SIDE);
  const std::vector<ArmySpec> armies{
      ArmySpec{CULT_SEAT, {Recruit{CULT_SEAT, HOSTILE_HEALTH}}, CULT, {strike}},
      ArmySpec{{4, 10}, {Recruit{{4, 10}, challenger_health}}, CULT, {strike}},
      ArmySpec{{4, 12}, {Recruit{{4, 12}, challenger_health}}, CULT, {strike}},
      ArmySpec{{1, 22}, {Recruit{{1, 22}, 1}}, ORTHODOX, {IDLE}},
      ArmySpec{{22, 22}, {Recruit{{22, 22}, 1}}, ALLY, {IDLE}},
  };
  const Rules rules{
      SEPARATION,
      {{false, true, false}, {true, false, false}, {false, false, false}},
      STAGE_FLOORS};
  return World{
      CULT_SEED,
      Map{rows},
      armies,
      {},
      rules,
      {},
      {{}, {cult_leader, Duel{true, 0, true}, std::nullopt, std::nullopt}, {}}};
}

// Has army `army` challenge the Cult's leader at its seat and runs the duel
// out, then returns the Cult's leader.
std::optional<std::size_t> after_challenge(World& world, std::size_t army) {
  world.order(army, Order{CULT_SEAT, {0}}, CULT);
  run_ticks(world, DUEL_TICKS);
  return world.leader(CULT);
}

TEST(World, OnlyAMemberMayChallengeTheCultsLeader) {
  World world = cult_world(TARGET_HEALTH);
  EXPECT_THROW(world.order(OUTSIDER, Order{CULT_SEAT, {0}}, CULT),
               std::invalid_argument);
  // Nor does a faction without a duel take a challenge.
  EXPECT_THROW(world.order(PLAYER, Order{CULT_SEAT, {0}}, ORTHODOX),
               std::invalid_argument);
  // Control: a member's challenge is taken.
  EXPECT_NO_THROW(world.order(PLAYER, Order{CULT_SEAT, {0}}, CULT));
}

TEST(World, AChallengerWhoWinsLeadsAndTheLeadersAlliesTurnOnThem) {
  World won = cult_world(TARGET_HEALTH);
  ASSERT_FALSE(won.hostile(ALLY, CULT));
  EXPECT_EQ(after_challenge(won, PLAYER), std::optional<std::size_t>{1});
  // The duel was fought by the one skill rule: the old leader fell.
  EXPECT_EQ(won.person(0).health, 0U);
  EXPECT_TRUE(won.hostile(ALLY, CULT));
  EXPECT_TRUE(won.hostile(ORTHODOX, CULT));

  // Control: a challenger who loses dies, and nothing changes.
  World lost = cult_world(1);
  EXPECT_EQ(after_challenge(lost, PLAYER), std::optional<std::size_t>{0});
  EXPECT_EQ(lost.person(1).health, 0U);
  EXPECT_FALSE(lost.hostile(ALLY, CULT));
}

TEST(World, ThePlayerAndAnNpcChallengeThroughTheSameOrder) {
  World by_player = cult_world(TARGET_HEALTH);
  World by_npc = cult_world(TARGET_HEALTH);
  EXPECT_EQ(after_challenge(by_player, PLAYER), std::optional<std::size_t>{1});
  EXPECT_EQ(after_challenge(by_npc, NPC), std::optional<std::size_t>{2});
  // Control: without a challenge, members of one faction never fight.
  World quiet = cult_world(TARGET_HEALTH);
  quiet.order(PLAYER, Order{CULT_SEAT, {0}});
  run_ticks(quiet, DUEL_TICKS);
  EXPECT_EQ(quiet.leader(CULT), std::optional<std::size_t>{0});
  EXPECT_EQ(quiet.person(0).health, HOSTILE_HEALTH);
}

// Whether order() refuses army `army`'s challenge to faction `faction`
// exactly when challenge_refusal() names a reason, and that reason is
// `expected`.
bool refused_for(World& world, std::size_t army, std::size_t faction,
                 ChallengeRefusal expected) {
  bool thrown = false;
  if (world.challenge_refusal(army, faction) != expected) {
    return false;
  }
  try {
    world.order(army, Order{CULT_SEAT, {0}}, faction);
  } catch (const std::invalid_argument&) {
    thrown = true;
  }
  return thrown == (expected != ChallengeRefusal::NONE);
}

TEST(World, AChallengeIsRefusedForEachReasonTheOrderRefusesIt) {
  World world = cult_world(TARGET_HEALTH);
  EXPECT_TRUE(
      refused_for(world, PLAYER, ORTHODOX, ChallengeRefusal::NO_SUCCESSION));
  EXPECT_TRUE(
      refused_for(world, OUTSIDER, CULT, ChallengeRefusal::NOT_A_MEMBER));
  EXPECT_TRUE(refused_for(world, 0, CULT, ChallengeRefusal::IS_LEADER));
  // Control: a member may challenge, and the order starts the duel.
  EXPECT_TRUE(refused_for(world, PLAYER, CULT, ChallengeRefusal::NONE));
  EXPECT_TRUE(
      refused_for(world, NPC, CULT, ChallengeRefusal::DUEL_IN_PROGRESS));

  World leaderless = cult_world(TARGET_HEALTH, std::nullopt);
  EXPECT_TRUE(
      refused_for(leaderless, PLAYER, CULT, ChallengeRefusal::NO_LEADER));

  // A challenger who lost the duel died, and its army has no one left.
  World lost = cult_world(1);
  ASSERT_EQ(after_challenge(lost, PLAYER), std::optional<std::size_t>{0});
  EXPECT_TRUE(refused_for(lost, PLAYER, CULT, ChallengeRefusal::NO_CHALLENGER));
  EXPECT_THROW(static_cast<void>(lost.challenge_refusal(99, CULT)),
               std::invalid_argument);
}

TEST(World, APersonsFactionIsItsArmysAndNoneOnceDead) {
  World lost = cult_world(1);
  EXPECT_EQ(lost.faction_of(PLAYER), std::optional<std::size_t>{CULT});
  EXPECT_EQ(lost.faction_of(OUTSIDER), std::optional<std::size_t>{ORTHODOX});
  ASSERT_EQ(after_challenge(lost, PLAYER), std::optional<std::size_t>{0});
  // The challenger died in the duel and serves in no army; control: the NPC,
  // alive in the same faction, still has it.
  EXPECT_FALSE(lost.faction_of(PLAYER).has_value());
  EXPECT_EQ(lost.faction_of(NPC), std::optional<std::size_t>{CULT});
}

TEST(World, ADuelIsReadWhileItIsFoughtAndGoneOnceSettled) {
  World world = cult_world(TARGET_HEALTH);
  EXPECT_FALSE(world.duel(CULT).has_value());
  world.order(PLAYER, Order{CULT_SEAT, {0}}, CULT);
  const std::optional<Duellists> fought = world.duel(CULT);
  ASSERT_TRUE(fought.has_value());
  const Duellists both = fought.value_or(Duellists{NPC, NPC});
  EXPECT_EQ(both.challenger, PLAYER);
  EXPECT_EQ(both.leader, 0U);
  // Control: no duel is fought for a faction nobody challenged.
  EXPECT_FALSE(world.duel(ALLY).has_value());
  run_ticks(world, DUEL_TICKS);
  EXPECT_FALSE(world.duel(CULT).has_value());
}

constexpr std::uint64_t TICKS_PER_SEASON =
    TICKS_PER_DAY * CALENDAR.days_per_season;

TEST(World, TheCalendarFollowsTheTicks) {
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
              {},
              peaceful(0)};
  const auto when = [&world]() {
    const Date date = world.date();
    return std::array<std::uint64_t, 3>{date.year, date.season, date.day};
  };
  EXPECT_EQ(when(), (std::array<std::uint64_t, 3>{1, 0, 1}));
  run_ticks(world, TICKS_PER_DAY);
  EXPECT_EQ(when(), (std::array<std::uint64_t, 3>{1, 0, 2}));
  run_ticks(world, TICKS_PER_SEASON - TICKS_PER_DAY);
  EXPECT_EQ(when(), (std::array<std::uint64_t, 3>{1, 1, 1}));
  run_ticks(world, TICKS_PER_SEASON * (CALENDAR.seasons_per_year - 1));
  EXPECT_EQ(when(), (std::array<std::uint64_t, 3>{2, 0, 1}));
}

TEST(World, HowManyTicksMakeADayIsTheWorldsData) {
  Rules daily = peaceful(0);
  daily.ticks_per_day = 1;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
              {},
              daily};
  run_ticks(world, CALENDAR.days_per_season);
  EXPECT_EQ(world.date().season, 1U);
  // A day of no ticks is refused; control: a day of one tick is accepted.
  Rules endless = daily;
  endless.ticks_per_day = 0;
  EXPECT_THROW((World{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                      {},
                      endless}),
               std::invalid_argument);
  EXPECT_NO_THROW((World{Seed{1},
                         Map{open_map(OPEN_SIDE)},
                         {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                         {},
                         daily}));
}

// One discipline whose limit is drawn over [500 - spread, 500 + spread].
Life one_discipline(std::uint32_t limit_spread) {
  constexpr Spread GROWTH{10, 5};
  constexpr std::uint32_t LIMIT = 500;
  return Life{{{"sword", GROWTH, {LIMIT, limit_spread}}},
              1,
              std::nullopt,
              0,
              0,
              0,
              std::nullopt,
              std::nullopt,
              {},
              std::nullopt,
              std::nullopt,
              std::nullopt};
}

// The mean and variance of the limits drawn for 400 people born with
// `life`, times 1, as integers.
std::pair<std::int64_t, std::int64_t> limit_moments(const Life& life) {
  constexpr std::size_t BORN = 400;
  const std::vector<std::string> rows = open_map(OPEN_SIDE * 2);
  const World world{Seed{3},
                    Map{rows},
                    {army(rows, {16, 16}, {16, 16}, BORN, 1, 0, {IDLE})},
                    {},
                    peaceful(0),
                    {},
                    {},
                    life};
  std::int64_t sum = 0;
  std::int64_t squares = 0;
  for (std::size_t person = 0; person < world.population(); ++person) {
    const std::int64_t limit = world.person(person).talent.front().limit;
    sum += limit;
    squares += limit * limit;
  }
  EXPECT_EQ(world.population(), BORN);
  const auto count = static_cast<std::int64_t>(BORN);
  const std::int64_t mean = sum / count;
  return {mean, (squares / count) - (mean * mean)};
}

TEST(World, TalentIsDrawnAtBirthFromItsDistribution) {
  constexpr std::int64_t SPREAD = 200;
  // A uniform draw over 2 * SPREAD + 1 values: mean 500, variance
  // ((2 * SPREAD + 1)^2 - 1) / 12 = 13,400.
  constexpr std::int64_t VALUES = (2 * SPREAD) + 1;
  constexpr std::int64_t VARIANCE = ((VALUES * VALUES) - 1) / 12;
  const auto [mean, variance] = limit_moments(one_discipline(SPREAD));
  EXPECT_NEAR(static_cast<double>(mean), 500.0, 20.0);
  EXPECT_NEAR(static_cast<double>(variance), static_cast<double>(VARIANCE),
              static_cast<double>(VARIANCE) / 5);
  // Control: with no spread every person draws the mean.
  EXPECT_EQ(limit_moments(one_discipline(0)),
            (std::pair<std::int64_t, std::int64_t>{500, 0}));
}

// Whether a world of one person born with `life` is created.
bool lives_by(const Life& life) {
  try {
    const World world{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                      {},
                      peaceful(0),
                      {},
                      {},
                      life};
  } catch (const std::invalid_argument&) {
    return false;
  }
  return true;
}

TEST(World, LifeDrawingPastItsLimitsIsRefused) {
  Life limit = one_discipline(0);
  limit.disciplines.front().limit = Spread{MAX_TALENT - 2, 0};
  limit.discipline_widening = 1;
  limit.upper_widening = 1;
  limit.lifespan = Spread{MAX_LIFESPAN_DAYS - 1, 1};
  limit.newborn_health = MAX_HEALTH;
  Life talent = limit;
  ++talent.upper_widening;
  Life lifespan = limit;
  ++lifespan.lifespan->spread;
  Life healthless = limit;
  healthless.newborn_health = 0;
  // Control: life at every limit is accepted.
  EXPECT_TRUE(lives_by(limit));
  EXPECT_FALSE(lives_by(talent));
  EXPECT_FALSE(lives_by(lifespan));
  EXPECT_FALSE(lives_by(healthless));
}

// Kinship at its limits: ages that run from and to one day, chances of a
// thousand per mille, and the widest neighbourhood.
constexpr Kinship KIN_LIMITS{{1, 1},    MAX_REACH, PER_MILLE, {1, 1},
                             PER_MILLE, 0,         true};
// A founding age as long as the longest lifespan.
constexpr Spread FOUNDING_LIMIT{MAX_LIFESPAN_DAYS - 1, 1};

// One discipline with `kinship` and founders `founding` days old.
Life kin_with(const Kinship& kinship, Spread founding = FOUNDING_LIMIT) {
  Life life = one_discipline(0);
  life.founding_age = founding;
  life.kinship = kinship;
  return life;
}

TEST(World, KinshipPastItsLimitsIsRefused) {
  Kinship downward = KIN_LIMITS;
  downward.marrying_age = Range{2, 1};
  Kinship unbearing = KIN_LIMITS;
  unbearing.bearing_age = Range{2, 1};
  Kinship eager = KIN_LIMITS;
  ++eager.marriage_chance;
  Kinship fertile = KIN_LIMITS;
  ++fertile.birth_chance;
  Kinship far = KIN_LIMITS;
  far.neighbourhood = -1;
  // Control: kinship at every limit is accepted.
  EXPECT_TRUE(lives_by(kin_with(KIN_LIMITS)));
  EXPECT_FALSE(
      lives_by(kin_with(KIN_LIMITS, Spread{MAX_LIFESPAN_DAYS - 1, 2})));
  for (const Kinship& refused : {downward, unbearing, eager, fertile, far}) {
    EXPECT_FALSE(lives_by(kin_with(refused)));
  }
}

TEST(World, ThePeopleAWorldStartsWithAreBornBeforeItByTheFoundingAge) {
  constexpr std::uint32_t AGE = 30;
  Life life = one_discipline(0);
  life.founding_age = Spread{AGE, 0};
  const World founded{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                      {},
                      peaceful(0),
                      {},
                      {},
                      life};
  EXPECT_EQ(founded.person(0).born,
            -static_cast<std::int64_t>(AGE * TICKS_PER_DAY));
  // Control: without a founding age they are born as the world begins.
  EXPECT_EQ(World(Seed{1}, Map{open_map(OPEN_SIDE)},
                  {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})}, {}, peaceful(0))
                .person(0)
                .born,
            0);
}

// Whether every person of `world` was born to live `days` days.
bool everyone_lives(const World& world, std::uint64_t days) {
  for (std::size_t person = 0; person < world.population(); ++person) {
    if (world.person(person).lifespan != std::optional<std::uint64_t>{days}) {
      return false;
    }
  }
  return true;
}

TEST(World, APersonsAgeIsCountedInWholeYearsOfTheCalendar) {
  constexpr std::uint64_t DAYS_SHORT = 5;
  Life life = one_discipline(0);
  life.founding_age =
      Spread{static_cast<std::uint32_t>((2 * DAYS_PER_YEAR) + DAYS_SHORT), 0};
  Rules daily = peaceful(0);
  daily.ticks_per_day = 1;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
              {},
              daily,
              {},
              {},
              life};
  EXPECT_EQ(world.age_in_years(0), 2);
  // Control: a day short of the third year it is still two; on it, three.
  run_ticks(world, DAYS_PER_YEAR - DAYS_SHORT - 1);
  EXPECT_EQ(world.age_in_years(0), 2);
  run_ticks(world, 1);
  EXPECT_EQ(world.age_in_years(0), 3);
}

TEST(World, PeopleAgeOutUnlessAttainmentExtendsTheirLives) {
  // Everyone lives one day, and a day more for each point of attainment.
  Life life = one_discipline(0);
  life.lifespan = Spread{1, 0};
  life.days_per_attainment = 1;
  const Skill practice{2 * CELL_SIZE, 0, 1, 0, 10};
  const std::vector<ArmySpec> armies{
      army({1, 1}, {1, 1}, 1, 1, 0, {IDLE}),
      army({8, 8}, {8, 8}, 1, 1, 0, {practice}),
      army({9, 8}, {9, 8}, 1, TARGET_HEALTH, 1, {IDLE}),
  };
  World world{Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(0), {}, {},
              life};
  // Setup: everyone was born to live one day.
  ASSERT_TRUE(everyone_lives(world, 1));
  run_ticks(world, TICKS_PER_DAY + 1);

  // The idle person aged out on its first day.
  EXPECT_EQ(world.person(0).health, 0U);
  EXPECT_TRUE(world.roster(0).empty());
  // The one who practised has attainment, up to its limit, and lives on.
  EXPECT_GT(world.person(1).attainment.front(), 0U);
  EXPECT_LE(world.person(1).attainment.front(),
            world.person(1).talent.front().limit);
  EXPECT_GT(world.person(1).health, 0U);
}

TEST(World, PeopleTheWorldStartsWithBelongToTheFamilyOfTheirArmy) {
  const World world = conscription_world();
  for (std::size_t army = 0; army < 3; ++army) {
    for (const std::size_t member : world.roster(army)) {
      EXPECT_EQ(world.person(member).family, army);
      EXPECT_TRUE(world.person(member).parents.empty());
    }
  }
}

// conscription_world() with a soul: its post, of faction 1, has the soul and
// marches on the village, where the residents kill it.
World soul_world() {
  World world = conscription_world();
  world.ensoul(world.roster(POST).front());
  world.order(POST, {HOME, {0}});
  return world;
}

TEST(World, ASoulIsBornAgainIntoAFamilyOfTheSameWorld) {
  World world = soul_world();
  const std::size_t post = world.roster(POST).front();
  const std::size_t before = world.population();
  run_ticks(world, ARRIVAL_TICKS);

  // The post died, and its soul lives on in a new person, the world's last.
  ASSERT_EQ(world.person(post).health, 0U);
  ASSERT_EQ(world.population(), before + 1);
  const std::size_t reborn = before;
  const Person& person = world.person(reborn);
  EXPECT_EQ(person.soul, world.person(post).soul);
  ASSERT_TRUE(person.soul.has_value());
  EXPECT_EQ(world.soul(person.soul.value_or(0)).lives, 1U);
  EXPECT_EQ(world.roster(POST), std::vector<std::size_t>{reborn});
  // Born to residents of the village, its home, into their family.
  ASSERT_EQ(person.parents.size(), 2U);
  EXPECT_EQ(person.home, std::optional<std::size_t>{0});
  EXPECT_EQ(person.family, world.person(person.parents.front()).family);
  // Control: not the family of the body that died.
  EXPECT_NE(person.family, world.person(post).family);
  EXPECT_GT(person.born, 0);
}

TEST(World, ARebirthContinuesTheWorldInsteadOfResettingIt) {
  World world = soul_world();
  World twin = soul_world();
  run_ticks(world, ARRIVAL_TICKS);
  run_ticks(twin, ARRIVAL_TICKS);
  const World fresh = soul_world();

  // Setup: the soul was born again in both runs.
  ASSERT_EQ(world.population(), fresh.population() + 1);
  EXPECT_EQ(world.hash(), twin.hash());
  // Control: the world as created, before any of it happened, differs.
  EXPECT_NE(world.hash(), fresh.hash());
  EXPECT_GT(world.date().day, fresh.date().day);
}

TEST(World, PastLivesWidenTheTalentDrawByDisciplineLessWithEachLife) {
  constexpr std::uint32_t SPREAD = 200;
  constexpr std::uint32_t WIDENING = 200;
  constexpr std::uint32_t HIGHER = 100;
  Life life = one_discipline(SPREAD);
  life.disciplines.push_back(life.disciplines.front());
  life.discipline_widening = WIDENING;
  life.upper_widening = HIGHER;
  const auto range = [&life](std::uint32_t lives, bool practised) {
    const Range drawn = limit_range(life, Soul{lives, {practised, false}}, 0);
    return std::pair{drawn.low, drawn.high};
  };
  // No history: the discipline's own spread.
  EXPECT_EQ(range(0, false),
            (std::pair<std::uint32_t, std::uint32_t>{300, 700}));
  // One life practising it: 200 wider each way, and 100 higher.
  EXPECT_EQ(range(1, true),
            (std::pair<std::uint32_t, std::uint32_t>{100, 1'000}));
  // Two lives: each widening halves.
  EXPECT_EQ(range(2, true),
            (std::pair<std::uint32_t, std::uint32_t>{200, 850}));
  // Control: a discipline not practised gains only the higher top.
  const Range other = limit_range(life, Soul{1, {true, false}}, 1);
  EXPECT_EQ((std::pair{other.low, other.high}),
            (std::pair<std::uint32_t, std::uint32_t>{300, 800}));
}

// Husbandry for the economy tests: a measure of food a day each, ten days'
// hunger, and a migration reach across the open map.
constexpr Husbandry FED{1, 10, 16};
constexpr std::uint32_t INTACT = 100;
// Where the first economy test village's four residents stand.
constexpr Cell SQUARE{6, 2};
constexpr std::size_t FOUNDERS = 4;
// Housing for ten and for twenty.
constexpr std::uint32_t FOR_TEN = 10;
constexpr std::uint32_t FOR_TWENTY = 20;
constexpr std::uint32_t BURNT = 50;

// What one economy test village holds: its house, its granary, and its
// field, and how much durability the field has.
struct Holdings {
  Holding house;
  Holding granary;
  Holding field;
  std::uint32_t field_durability = INTACT;
};

// Two villages on the open map, each of a house, a granary and a field, in a
// row: the first along row 1 with FOUNDERS residents around SQUARE, the
// second along row 10 with `others` residents around the cell nine below. Days
// last a tick, so a test runs a year in DAYS_PER_YEAR ticks.
World economy_world(const Holdings& first, const Holdings& second,
                    std::size_t others,
                    std::optional<Husbandry> husbandry = FED) {
  constexpr std::int32_t SECOND_ROW = 10;
  const auto row = [](std::int32_t row, const Holdings& holds) {
    return std::vector<Structure>{
        Structure{{1, row}, INTACT, {1, 1}, true},
        Structure{{2, row}, INTACT, {1, 1}, true},
        Structure{{3, row}, holds.field_durability, {1, 1}, false}};
  };
  std::vector<Structure> structures = row(1, first);
  for (const Structure& structure : row(SECOND_ROW, second)) {
    structures.push_back(structure);
  }
  Rules daily = peaceful(0);
  daily.ticks_per_day = 1;
  Life life = one_discipline(0);
  life.husbandry = husbandry;
  const Cell second_square{SQUARE.x, SECOND_ROW + 1};
  const std::vector<std::size_t> first_structures{0, 1, 2};
  const std::vector<std::size_t> second_structures{3, 4, 5};
  return World{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(SQUARE, SQUARE, FOUNDERS, 1, 0, {IDLE}),
       army(second_square, second_square, others, 1, 0, {IDLE})},
      structures,
      daily,
      {VillageSpec{
           first_structures, 0, {first.house, first.granary, first.field}},
       VillageSpec{
           second_structures, 1, {second.house, second.granary, second.field}}},
      {},
      life};
}

// A village of `housing`, `storage` and a field of `harvest`.
Holdings holding(std::uint32_t housing, std::uint32_t storage,
                 std::uint32_t harvest) {
  return Holdings{Holding{housing, 0, 0}, Holding{0, storage, 0},
                  Holding{0, 0, harvest}};
}

TEST(World, AVillagesCapIsTheLeastOfItsHousingFoodAndStorage) {
  // A year's food for one person is DAYS_PER_YEAR measures.
  constexpr std::uint32_t TEN_YEARS = 10 * DAYS_PER_YEAR;
  constexpr std::uint32_t FIVE_YEARS = 5 * DAYS_PER_YEAR;
  constexpr std::uint32_t FOUR_YEARS = 4 * DAYS_PER_YEAR;
  const Holdings other = holding(1, TEN_YEARS, TEN_YEARS);
  const auto cap_of = [&other](const Holdings& first) {
    return economy_world(first, other, 1).cap(0);
  };
  EXPECT_EQ(cap_of(holding(3, TEN_YEARS, TEN_YEARS)), 3U);
  EXPECT_EQ(cap_of(holding(FOR_TWENTY, TEN_YEARS, FIVE_YEARS)), 5U);
  EXPECT_EQ(cap_of(holding(FOR_TWENTY, FOUR_YEARS, TEN_YEARS)), 4U);
  // Derived on read: a burnt field feeds no one.
  Holdings burnt = holding(FOR_TWENTY, TEN_YEARS, FIVE_YEARS);
  burnt.field_durability = BURNT;
  EXPECT_EQ(cap_of(burnt), 0U);
  // Control: without husbandry no village is capped below the bound.
  EXPECT_EQ(
      economy_world(holding(3, TEN_YEARS, TEN_YEARS), other, 1, std::nullopt)
          .cap(0),
      MAX_PEOPLE);
}

TEST(World, AFieldBlocksNoOneAndATorchBurnsIt) {
  const Holdings fields = holding(1, DAYS_PER_YEAR, DAYS_PER_YEAR);
  World world = economy_world(fields, fields, 1);
  // The field, at (3, 1), is no obstacle: an order onto it is accepted.
  EXPECT_NO_THROW(world.order(0, {{3, 1}, {0}}));
  EXPECT_EQ(world.field_state(2), std::optional<FieldState>{FieldState::SOWN});
  // Control: the house beside it, which blocks, refuses the same order.
  EXPECT_THROW(world.order(0, {{1, 1}, {0}}), std::invalid_argument);
  EXPECT_FALSE(world.field_state(0).has_value());
  // A torch of a hostile faction, used beside the field, burns it.
  const Skill torch{0, 2 * CELL_SIZE, 0, BURNT, 1};
  World raided{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army(SQUARE, SQUARE, 1, 1, 0, {IDLE}),
                army({4, 1}, {4, 1}, 1, 1, 1, {torch})},
               {Structure{{3, 1}, INTACT, {1, 1}, false}},
               at_war(0),
               {VillageSpec{{0}, 0, {Holding{0, 0, DAYS_PER_YEAR}}}}};
  run_ticks(raided, 2);
  EXPECT_EQ(raided.field_state(0),
            std::optional<FieldState>{FieldState::BURNT});
}

TEST(World, AVillageWithoutAHarvestRunsOutOfFoodAndItsPeopleMove) {
  // Four people eat 600 measures in 150 days; the harvest comes on day 91.
  constexpr std::uint32_t STORED = 600;
  constexpr std::uint64_t DAYS = 200;
  Holdings starving = holding(FOR_TEN, STORED, STORED);
  starving.field_durability = BURNT;
  const Holdings refuge = holding(FOR_TEN, 2 * STORED, STORED);
  World world = economy_world(starving, refuge, 1);
  run_ticks(world, DAYS);
  EXPECT_EQ(world.residents_of(0), 0U);
  EXPECT_EQ(world.residents_of(1), 5U);
  EXPECT_EQ(world.living(), 5U);
  // Control: with its field intact, the harvest feeds the village through.
  World harvested = economy_world(holding(FOR_TEN, STORED, STORED), refuge, 1);
  run_ticks(harvested, DAYS);
  EXPECT_EQ(harvested.residents_of(0), 4U);
}

TEST(World, TheHungryWithNowhereToGoDie) {
  constexpr std::uint32_t STORED = 600;
  constexpr std::uint64_t DAYS = 200;
  Holdings starving = holding(FOR_TEN, STORED, STORED);
  starving.field_durability = BURNT;
  // The only other village houses one, and one lives there.
  World world = economy_world(starving, holding(1, STORED, STORED), 1);
  run_ticks(world, DAYS);
  EXPECT_EQ(world.residents_of(0), 0U);
  EXPECT_EQ(world.living(), 1U);
  // Control: with room there, they moved instead (the test above), and here
  // the same village fed lives on.
  World fed = economy_world(holding(FOR_TEN, STORED, STORED),
                            holding(1, STORED, STORED), 1);
  run_ticks(fed, DAYS);
  EXPECT_EQ(fed.living(), 5U);
}

// The growth tests' villages: A, of faction 0, with residents around
// GROWING_SEAT, and B, of faction 1, with residents around NEIGHBOUR_SEAT, on
// a 32-cell open map. Days last a tick and growth is checked every day.
constexpr std::size_t GROWTH_SIDE = 32;
constexpr Cell GROWING_SEAT{6, 4};
constexpr Cell NEIGHBOUR_SEAT{24, 24};
constexpr std::size_t VILLAGE_A = 0;
constexpr std::size_t VILLAGE_B = 1;
constexpr std::uint32_t TEN_YEARS_FOOD = 10 * DAYS_PER_YEAR;
constexpr std::uint32_t HOUSE_FOR_FOUR = 4;
// A mend that restores 10 durability within 6 cells, and a guard's strike
// of 10 within 3 cells, each every tick.
constexpr std::int32_t MEND_RADIUS = 6 * CELL_SIZE;
constexpr Skill MEND{0, MEND_RADIUS, 0, -10, 1};
constexpr Skill GUARD{3 * CELL_SIZE, 0, 10, 0, 1};
constexpr std::uint64_t SETTLING_TICKS = 400;
// Each village's three structures, a house, a granary and a field; the
// settlement's side.
constexpr std::size_t STRUCTURES_AT_START = 6;
constexpr std::int32_t SETTLEMENT_SIDE = 5;

// What the growth tests vary: A's stance, the pressure, the reach, the plans
// A builds, whether B's faction is hostile to A's, and B's residents'
// health.
struct Growing {
  std::vector<Expansion> stance;
  std::uint32_t pressure = PER_MILLE;
  std::int32_t reach = 24;
  std::vector<std::size_t> builds{0, 1};
  bool hostile = true;
  std::uint64_t neighbour_health = 1;
  std::size_t founders = FOUNDERS;
};

// Growth of three plans, a granary, a house for four and a field, a 5 x 5
// settlement of one of each, two settlers and two raiders.
Growth growth_of(const Growing& growing) {
  Growth growth;
  growth.interval = 1;
  growth.pressure = growing.pressure;
  growth.reach = growing.reach;
  growth.vicinity = 4;
  growth.spacing = 1;
  growth.claim = 4;
  growth.plans = {Plan{"granary", {1, 1}, true, Holding{0, TEN_YEARS_FOOD, 0}},
                  Plan{"house", {2, 2}, true, Holding{HOUSE_FOR_FOUR, 0, 0}},
                  Plan{"field", {2, 2}, false, Holding{0, 0, TEN_YEARS_FOOD}}};
  growth.builds = growing.builds;
  growth.settlement = {SETTLEMENT_SIDE, SETTLEMENT_SIDE};
  growth.settlement_kind = "hamlet";
  growth.layout = {Placement{1, {0, 0}}, Placement{0, {3, 0}},
                   Placement{2, {0, 3}}};
  growth.settlers = 2;
  growth.raiders = 2;
  growth.stances = {growing.stance, {}};
  return growth;
}

// Village A of four residents housed for four, with ten years' food stored
// and harvested, so housing is its scarcest holding, and village B of four,
// on open ground but for `rough` cells, which bodies cross and nothing is
// built on.
World growth_world(const Growing& growing,
                   const std::vector<Cell>& rough = {}) {
  const auto row = [](Cell from) {
    return std::vector<Structure>{
        Structure{from, INTACT, {1, 1}, true},
        Structure{{from.x + 1, from.y}, INTACT, {1, 1}, true},
        Structure{{from.x + 2, from.y}, INTACT, {1, 1}, false}};
  };
  constexpr Cell NEIGHBOUR_ROW{20, 20};
  std::vector<Structure> structures = row({1, 1});
  for (const Structure& structure : row(NEIGHBOUR_ROW)) {
    structures.push_back(structure);
  }
  const std::vector<Holding> holdings{Holding{HOUSE_FOR_FOUR, 0, 0},
                                      Holding{0, TEN_YEARS_FOOD, 0},
                                      Holding{0, 0, TEN_YEARS_FOOD}};
  Rules daily =
      growing.hostile
          ? at_war(SEPARATION)
          : Rules{SEPARATION, {{false, false}, {false, false}}, STAGE_FLOORS};
  daily.ticks_per_day = 1;
  Life life = one_discipline(0);
  life.husbandry = FED;
  life.growth = growth_of(growing);
  std::vector<std::string> rows = open_map(GROWTH_SIDE);
  for (const Cell cell : rough) {
    rows.at(static_cast<std::size_t>(cell.y))
        .at(static_cast<std::size_t>(cell.x)) = tile_of(Footing::ROUGH);
  }
  return World{Seed{3},
               Map{rows},
               {army(rows, GROWING_SEAT, GROWING_SEAT, growing.founders, 1, 0,
                     {MEND, GUARD}),
                army(rows, NEIGHBOUR_SEAT, NEIGHBOUR_SEAT, FOUNDERS,
                     growing.neighbour_health, 1, {IDLE})},
               structures,
               daily,
               {VillageSpec{{0, 1, 2}, 0, holdings},
                VillageSpec{{3, 4, STRUCTURES_AT_START - 1}, 1, holdings}},
               {},
               life};
}

// Whether `cell` lies within four cells of `centre` along each axis.
bool within_four(Cell cell, Cell centre) {
  constexpr std::int32_t NEAR = 4;
  return std::abs(cell.x - centre.x) <= NEAR &&
         std::abs(cell.y - centre.y) <= NEAR;
}

// How many armies `growing`'s growth world holds after its first check.
std::size_t grown_armies(const Growing& growing) {
  World world = growth_world(growing);
  run_ticks(world, 1);
  return world.army_count();
}

TEST(World, APressedVillageBuildsWhatItLacksAndRaisesItByRepair) {
  World world = growth_world(Growing{{Expansion::BUILD}});
  ASSERT_EQ(world.cap(VILLAGE_A), FOUNDERS);
  run_ticks(world, 1);
  // Housing is scarcest, so of the granary and the house it lays the house,
  // as rubble, beside its centre.
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  constexpr std::size_t BUILT = STRUCTURES_AT_START;
  EXPECT_EQ(world.plan_of(BUILT), std::optional<std::size_t>{1});
  EXPECT_EQ(world.stage(BUILT), Stage::RUBBLE);
  EXPECT_TRUE(within_four(world.structure(BUILT).cell, GROWING_SEAT));
  EXPECT_EQ(world.cap(VILLAGE_A), FOUNDERS);
  // The residents' mend raises it by the repair rule, and the housing grows.
  constexpr std::uint64_t RAISING_TICKS = 50;
  run_ticks(world, RAISING_TICKS);
  EXPECT_EQ(world.stage(BUILT), Stage::INTACT);
  EXPECT_EQ(world.cap(VILLAGE_A), 2 * FOUNDERS);
  // Control: no longer pressed, it built nothing more.
  EXPECT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  // Control: three people under a cap of four do not press it, and nothing
  // is built.
  Growing short_of_cap{{Expansion::BUILD}};
  short_of_cap.founders = FOUNDERS - 1;
  World unpressed = growth_world(short_of_cap);
  run_ticks(unpressed, 1);
  EXPECT_EQ(unpressed.structure_count(), STRUCTURES_AT_START);
  // Control: residents ordered to strike only with their guard, the second
  // of their skills, leave what they lay in rubble.
  World unraised = growth_world(Growing{{Expansion::BUILD}});
  const std::size_t residents_a = 0;
  unraised.order(residents_a, Order{GROWING_SEAT, {1}});
  run_ticks(unraised, RAISING_TICKS + 1);
  ASSERT_EQ(unraised.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(unraised.stage(BUILT), Stage::RUBBLE);
  EXPECT_EQ(unraised.cap(VILLAGE_A), FOUNDERS);
}

TEST(World, APressedVillageBuildsOnlyOnBuildableCells) {
  // Control: on open ground the house is laid on the cells nearest the
  // centre.
  World open = growth_world(Growing{{Expansion::BUILD}});
  run_ticks(open, 1);
  ASSERT_EQ(open.structure_count(), STRUCTURES_AT_START + 1);
  const Structure nearest = open.structure(STRUCTURES_AT_START);
  // With rough ground, as hills are, over those cells, it is laid elsewhere,
  // still beside the centre, on none of them.
  const std::vector<Cell> hills = cells_of(nearest.cell, nearest.footprint);
  World world = growth_world(Growing{{Expansion::BUILD}}, hills);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  const Structure built = world.structure(STRUCTURES_AT_START);
  EXPECT_TRUE(within_four(built.cell, GROWING_SEAT));
  for (const Cell cell : cells_of(built.cell, built.footprint)) {
    EXPECT_TRUE(std::none_of(
        hills.begin(), hills.end(),
        [cell](Cell hill) { return hill.x == cell.x && hill.y == cell.y; }))
        << "(" << cell.x << ", " << cell.y << ")";
  }
}

// Whether `person` is a man.
bool male(const World& world, std::size_t person) {
  return world.person(person).sex == Sex::MALE;
}

// Village A's world after its first check under a stance to found, with the
// seed's draw checked to have given A a man to found the village.
World founded() {
  World world = growth_world(Growing{{Expansion::FOUND}});
  const std::vector<std::size_t>& before = world.roster(VILLAGE_A);
  const bool founder =
      std::any_of(before.begin(), before.end(),
                  [&world](std::size_t person) { return male(world, person); });
  EXPECT_TRUE(founder);
  run_ticks(world, 1);
  return world;
}

constexpr std::size_t FOUNDED = 2;

TEST(World, APressedVillageFoundsAVillageOnFreeLand) {
  const std::vector<std::optional<std::size_t>> unclaimed =
      growth_world(Growing{{Expansion::FOUND}}).territory();
  const World world = founded();
  ASSERT_EQ(world.village_count(), 3U);
  const std::size_t settlers = world.army_count() - 1;
  ASSERT_EQ(world.roster(settlers).size(), 2U);
  EXPECT_EQ(world.roster(VILLAGE_A).size(), FOUNDERS - 2);
  EXPECT_EQ(world.owner(FOUNDED), 0U);
  // The founder heads a new family, held by no one who stayed.
  const std::size_t founder = world.roster(settlers).front();
  const std::size_t family = world.person(founder).family;
  EXPECT_TRUE(male(world, founder));
  EXPECT_GE(family, 2U);
  const std::vector<std::size_t>& stayed = world.roster(VILLAGE_A);
  EXPECT_TRUE(std::none_of(stayed.begin(), stayed.end(),
                           [&world, family](std::size_t person) {
                             return world.person(person).family == family;
                           }));
  // Its faction's territory grew where it was founded.
  EXPECT_NE(world.territory(), unclaimed);
  // Control: with no free land within its reach, it founds nothing.
  Growing hemmed{{Expansion::FOUND}};
  hemmed.reach = 0;
  World nowhere = growth_world(hemmed);
  run_ticks(nowhere, 1);
  EXPECT_EQ(nowhere.village_count(), 2U);
  EXPECT_EQ(nowhere.roster(VILLAGE_A).size(), FOUNDERS);
}

TEST(World, AFoundedVillageNamesTheLandItWasLaidOutOn) {
  const World world = founded();
  const std::optional<Settlement> land = world.settlement(FOUNDED);
  ASSERT_TRUE(land.has_value());
  const Settlement laid_on = land.value_or(Settlement{"", {-1, -1}, {0, 0}});
  EXPECT_EQ(laid_on.kind, "hamlet");
  EXPECT_EQ(laid_on.area.width, SETTLEMENT_SIDE);
  EXPECT_EQ(laid_on.area.height, SETTLEMENT_SIDE);
  // Each placement of the layout stands at its offset from the corner.
  const Growth growth = growth_of(Growing{{Expansion::FOUND}});
  for (const Placement& placement : growth.layout) {
    bool laid = false;
    for (std::size_t structure = 0; structure < world.structure_count();
         ++structure) {
      const Cell cell = world.structure(structure).cell;
      laid = laid || (world.plan_of(structure) == placement.plan &&
                      cell.x == laid_on.corner.x + placement.offset.x &&
                      cell.y == laid_on.corner.y + placement.offset.y);
    }
    EXPECT_TRUE(laid);
  }
  // Control: a village the world was created with was founded on no land.
  EXPECT_FALSE(world.settlement(VILLAGE_A).has_value());
}

TEST(World, SettlersMakeTheirVillageHomeAtAHarvestThatFindsItHoldingThem) {
  World world = founded();
  ASSERT_EQ(world.village_count(), 3U);
  // Until then the settlers keep their home, and the new village holds
  // nothing.
  EXPECT_EQ(world.residents_of(FOUNDED), 0U);
  EXPECT_EQ(world.residents_of(VILLAGE_A), FOUNDERS);
  EXPECT_EQ(world.cap(FOUNDED), 0U);
  // They raise it; a harvest then finds it holding them, makes it their
  // home, and it feeds them as any village does.
  run_ticks(world, SETTLING_TICKS);
  EXPECT_EQ(world.residents_of(FOUNDED), 2U);
  EXPECT_EQ(world.residents_of(VILLAGE_A), FOUNDERS - 2);
  EXPECT_GE(world.cap(FOUNDED), 2U);
  EXPECT_GT(world.stock(FOUNDED), 0U);
}

// Ticks `world` until village B's residents are struck down, for at most
// 2,000 ticks. Returns the cell where party `party`'s lead last stood while
// one of them lived.
Cell raid_out(World& world, std::size_t party) {
  constexpr std::uint64_t MARCH_LIMIT = 2'000;
  Cell reached = GROWING_SEAT;
  for (std::uint64_t tick = 0;
       tick < MARCH_LIMIT && !world.roster(VILLAGE_B).empty(); ++tick) {
    reached = cell_of(lead(world, party));
    world.tick();
  }
  return reached;
}

TEST(World, APressedVillageRaidsAHostileNeighbourAndItsPartyComesHome) {
  Growing raiding{{Expansion::RAID}};
  raiding.neighbour_health = GUARD.body_damage;
  World world = growth_world(raiding);
  run_ticks(world, 1);
  ASSERT_EQ(world.army_count(), 3U);
  constexpr std::size_t PARTY = 2;
  EXPECT_EQ(world.roster(PARTY).size(), 2U);
  EXPECT_EQ(world.faction(PARTY), 0U);
  // The party marches on village B and strikes its residents down.
  const Cell reached = raid_out(world, PARTY);
  ASSERT_TRUE(world.roster(VILLAGE_B).empty());
  EXPECT_TRUE(within_four(reached, NEIGHBOUR_SEAT));
  // The raid is over, so at that check the party went home to its village's
  // residents.
  EXPECT_TRUE(world.roster(PARTY).empty());
  EXPECT_EQ(world.roster(VILLAGE_A).size(), FOUNDERS);
}

TEST(World, NoVillageRaidsANeighbourAtPeaceOrBeyondItsReach) {
  // The same village raids a hostile neighbour within its reach (above).
  ASSERT_EQ(grown_armies(Growing{{Expansion::RAID}}), 3U);
  Growing peace{{Expansion::RAID}};
  peace.hostile = false;
  EXPECT_EQ(grown_armies(peace), 2U);
  Growing far{{Expansion::RAID}};
  far.reach = NEIGHBOUR_SEAT.x - GROWING_SEAT.x - 1;
  EXPECT_EQ(grown_armies(far), 2U);
}

// What a check of `growing`'s growth world did: laid a structure, founded a
// village, or sent a party.
struct Grown {
  bool built;
  bool founded;
  bool raided;
};

Grown grown(const Growing& growing) {
  World world = growth_world(growing);
  run_ticks(world, 1);
  return Grown{world.structure_count() > STRUCTURES_AT_START &&
                   world.village_count() == 2,
               world.village_count() > 2,
               world.army_count() > 2 && world.village_count() == 2};
}

TEST(World, TheStanceOrdersTheChoicesAndTheCapAndNeighboursDecideThem) {
  // Each choice is open to the same village; its stance picks the first.
  const Grown builds = grown(Growing{{Expansion::BUILD, Expansion::RAID}});
  EXPECT_TRUE(builds.built && !builds.founded && !builds.raided);
  const Grown raids = grown(Growing{{Expansion::RAID, Expansion::BUILD}});
  EXPECT_TRUE(!raids.built && !raids.founded && raids.raided);
  const Grown founds = grown(Growing{{Expansion::FOUND, Expansion::BUILD}});
  EXPECT_TRUE(!founds.built && founds.founded && !founds.raided);
  // With only a granary to build while housing is scarcest, building raises
  // nothing it lacks, and it raids instead.
  Growing granary_only{{Expansion::BUILD, Expansion::RAID}};
  granary_only.builds = {0};
  const Grown lacking = grown(granary_only);
  EXPECT_TRUE(!lacking.built && lacking.raided);
  // With no hostile neighbour, it neither raids nor grows otherwise.
  Growing peaceful_only{{Expansion::RAID}};
  peaceful_only.hostile = false;
  const Grown idle = grown(peaceful_only);
  EXPECT_TRUE(!idle.built && !idle.founded && !idle.raided);
  // Control: a faction with no stance grows no village.
  const Grown still = grown(Growing{{}});
  EXPECT_TRUE(!still.built && !still.founded && !still.raided);
}

// Whether a world whose life grows by `growth` is accepted.
bool grows_by(const Growth& growth) {
  try {
    Life life = one_discipline(0);
    life.growth = growth;
    static_cast<void>(World{
        Seed{1}, Map{open_map(OPEN_SIDE)}, {}, {}, at_war(0), {}, {}, life});
    return true;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

TEST(World, GrowthPastItsLimitsIsRefused) {
  const Growth valid = growth_of(Growing{});
  EXPECT_TRUE(grows_by(valid));
  Growth never = valid;
  never.interval = 0;
  EXPECT_FALSE(grows_by(never));
  Growth overpressed = valid;
  overpressed.pressure = PER_MILLE + 1;
  EXPECT_FALSE(grows_by(overpressed));
  Growth beyond = valid;
  beyond.reach = MAX_REACH + 1;
  EXPECT_FALSE(grows_by(beyond));
  Growth unknown = valid;
  unknown.builds = {valid.plans.size()};
  EXPECT_FALSE(grows_by(unknown));
  Growth overlapping = valid;
  overlapping.layout.push_back(Placement{0, {1, 1}});
  EXPECT_FALSE(grows_by(overlapping));
  Growth outside = valid;
  outside.layout.push_back(Placement{0, {SETTLEMENT_SIDE, 0}});
  EXPECT_FALSE(grows_by(outside));
  Growth over_centre = valid;
  over_centre.layout.push_back(Placement{0, {2, 2}});
  EXPECT_FALSE(grows_by(over_centre));
  Growth too_many = valid;
  too_many.stances = {{}, {}, {}};
  EXPECT_FALSE(grows_by(too_many));
}

// A world of one village whose three residents start with `households`
// households.
World housed(std::uint32_t households) {
  constexpr Cell HOME{4, 4};
  constexpr std::size_t RESIDENTS = 3;
  return World{Seed{3},
               Map{open_map(OPEN_SIDE)},
               {army(HOME, HOME, RESIDENTS, 1, 0, {GUARD})},
               {},
               at_war(SEPARATION),
               {VillageSpec{{}, 0, {}, households}},
               {},
               one_discipline(0)};
}

TEST(World, AVillagesHouseholdsStartAsMarriedCouplesOfFamiliesOfTheirOwn) {
  const World world = housed(1);
  const Person& husband = world.person(0);
  const Person& wife = world.person(1);
  const Person& single = world.person(2);
  EXPECT_EQ(husband.sex, Sex::MALE);
  EXPECT_EQ(wife.sex, Sex::FEMALE);
  EXPECT_EQ(husband.spouses, std::vector<std::size_t>{1});
  EXPECT_EQ(wife.spouses, std::vector<std::size_t>{0});
  EXPECT_NE(husband.family, wife.family);
  // The resident outside a household is born single into the army's family,
  // which neither of the couple heads.
  EXPECT_TRUE(single.spouses.empty());
  EXPECT_NE(single.family, husband.family);
  EXPECT_NE(single.family, wife.family);
  // Control: with no household, no one starts married, and all share the
  // army's family.
  const World unhoused = housed(0);
  for (std::size_t person = 0; person < 3; ++person) {
    EXPECT_TRUE(unhoused.person(person).spouses.empty());
    EXPECT_EQ(unhoused.person(person).family, unhoused.person(0).family);
  }
}

TEST(World, RefusesMoreHouseholdsThanItsResidentsPairInto) {
  EXPECT_NO_THROW(static_cast<void>(housed(1)));
  EXPECT_THROW(static_cast<void>(housed(2)), std::invalid_argument);
}

// A world of sects, with days of one tick: a seat village of two people of
// `seat_faction` around (2, 3), whose one structure teaches `pupils`, a
// village of `villagers` of `villager_faction` around (12, 3), and an army of
// one of faction 1 around (8, 13). Faction 0's sect 0 is seated in the seat
// village; its founders are persons `founders`, at attainments `founding`.
struct Sectarian {
  std::uint32_t pupils = 4;
  std::uint32_t durability = INTACT;
  Admission admission{0, 0, 0, OPEN_SIDE, PER_MILLE, 10};
  std::optional<Election> election;
  std::size_t villagers = 3;
  std::size_t villager_faction = 0;
  std::array<std::uint32_t, 2> founding{20, 0};
  std::size_t seat_faction = 0;
  std::array<std::size_t, 2> founders{0, 1};
  Manuals manuals{{{0}}, 0, 0};
};

constexpr Cell SEAT_HOME{2, 3};
constexpr Cell VILLAGE_HOME{12, 3};
constexpr Cell OUTSIDERS_HOME{8, 13};
constexpr std::size_t SEAT_ARMY = 0;
constexpr std::size_t VILLAGE_ARMY = 1;
constexpr std::size_t OUTSIDERS = 2;
constexpr std::size_t SECT = 0;

World sect_world(const Sectarian& sectarian) {
  Rules peace{SEPARATION, {{false, false}, {false, false}}, STAGE_FLOORS};
  peace.ticks_per_day = 1;
  Life life = one_discipline(0);
  life.manuals = sectarian.manuals;
  const Leadership faction{std::nullopt, std::nullopt, sectarian.admission,
                           sectarian.election};
  return World{
      Seed{3},
      Map{open_map(OPEN_SIDE)},
      {army(SEAT_HOME, SEAT_HOME, 2, 1, sectarian.seat_faction, {IDLE}),
       army(VILLAGE_HOME, VILLAGE_HOME, sectarian.villagers, 1,
            sectarian.villager_faction, {IDLE}),
       army(OUTSIDERS_HOME, OUTSIDERS_HOME, 1, 1, 1, {IDLE})},
      {Structure{{SEAT_HOME.x, 0}, sectarian.durability, {1, 1}, true},
       Structure{{VILLAGE_HOME.x, 0}, INTACT, {1, 1}, true}},
      peace,
      {VillageSpec{{0}, SEAT_ARMY, {Holding{0, 0, 0, sectarian.pupils}}},
       VillageSpec{{1}, VILLAGE_ARMY, {Holding{}}}},
      {faction, faction},
      life,
      {SectSpec{
          "Hwasan",
          0,
          0,
          {Founder{sectarian.founders.at(0), sectarian.founding.at(0)},
           Founder{sectarian.founders.at(1), sectarian.founding.at(1)}}}}};
}

// Sectarian `sectarian`'s world after `days` days.
World sect_world_after(const Sectarian& sectarian, std::uint64_t days) {
  World world = sect_world(sectarian);
  run_ticks(world, days);
  return world;
}

constexpr std::uint64_t SECT_YEARS = 2 * DAYS_PER_YEAR;

TEST(World, ASectAdmitsItsFactionsPeopleWithinReachUpToItsCap) {
  const World world = sect_world_after(Sectarian{}, SECT_YEARS);
  // Its two founders and two of the three villagers, as many as the seat's
  // pupils.
  EXPECT_EQ(world.sect_members(SECT).size(), 4U);
  EXPECT_EQ(world.sect_cap(SECT), 4U);
  // Control: a seat teaching two holds only its founders.
  Sectarian narrow;
  narrow.pupils = 2;
  EXPECT_EQ(sect_world_after(narrow, SECT_YEARS).sect_members(SECT),
            (std::vector<std::size_t>{0, 1}));
}

TEST(World, ASectAdmitsNoOneBelowItsThresholdsBeyondReachOrOfAnotherFaction) {
  const std::vector<std::size_t> founders{0, 1};
  // Control: the same world with nothing barring them admits villagers.
  ASSERT_GT(sect_world_after(Sectarian{}, SECT_YEARS).sect_members(SECT).size(),
            founders.size());
  Sectarian untalented;
  untalented.admission.talent = MAX_TALENT;
  Sectarian unproven;
  unproven.admission.attainment = 1;
  Sectarian distant;
  distant.admission.reach = 4;
  Sectarian foreign;
  foreign.villager_faction = 1;
  for (const Sectarian& barred : {untalented, unproven, distant, foreign}) {
    EXPECT_EQ(sect_world_after(barred, SECT_YEARS).sect_members(SECT),
              founders);
  }
}

TEST(World, ALostSeatCapsItsSectAtNothingAndKeepsItsMembers) {
  Sectarian ruined;
  ruined.durability = 0;
  const World world = sect_world_after(ruined, SECT_YEARS);
  ASSERT_EQ(world.village_state(0), VillageState::RUINS);
  EXPECT_FALSE(world.seat_held(SECT));
  EXPECT_EQ(world.sect_cap(SECT), 0U);
  EXPECT_EQ(world.sect_members(SECT), (std::vector<std::size_t>{0, 1}));
  EXPECT_TRUE(world.sect_stands(SECT));
  // A seat standing intact but held by another faction is lost all the same.
  Sectarian taken;
  taken.seat_faction = 1;
  taken.founders = {2, 3};
  const World occupied = sect_world(taken);
  ASSERT_EQ(occupied.stage(0), Stage::INTACT);
  EXPECT_FALSE(occupied.seat_held(SECT));
  EXPECT_EQ(occupied.sect_cap(SECT), 0U);
  EXPECT_EQ(occupied.sect_members(SECT), (std::vector<std::size_t>{2, 3}));
  // Control: the seat standing, its pupils make the cap.
  const World held = sect_world(Sectarian{});
  EXPECT_TRUE(held.seat_held(SECT));
  EXPECT_EQ(held.sect_cap(SECT), 4U);
}

TEST(World, AMemberServingAnotherFactionLeavesItsSect) {
  World world = sect_world(Sectarian{});
  world.conscript(1, OUTSIDERS);
  run_ticks(world, 1);
  EXPECT_FALSE(world.sect_of(1).has_value());
  EXPECT_EQ(world.sect_members(SECT), std::vector<std::size_t>{0});
  // Control: serving another army of its own faction, it stays.
  World kept = sect_world(Sectarian{});
  kept.conscript(1, VILLAGE_ARMY);
  run_ticks(kept, 1);
  EXPECT_EQ(kept.sect_of(1), std::optional<std::size_t>{SECT});
}

TEST(World, EldersAreTheShareOfMembersOfHighestAttainment) {
  constexpr std::uint32_t STRONGER = 30;
  constexpr std::uint32_t WEAKER = 20;
  Sectarian both;
  both.founding = {STRONGER, WEAKER};
  both.admission.elder_share = PER_MILLE;
  World world = sect_world(both);
  EXPECT_EQ(world.sect_elders(SECT), (std::vector<std::size_t>{0, 1}));
  EXPECT_EQ(world.sect_master(SECT), std::optional<std::size_t>{0});
  // The master leaving, the next elder is master.
  world.conscript(0, OUTSIDERS);
  run_ticks(world, 1);
  EXPECT_EQ(world.sect_master(SECT), std::optional<std::size_t>{1});
  // A third of two members is none, and a standing sect keeps at least one:
  // the one of higher attainment.
  Sectarian third = both;
  third.admission.elder_share = PER_MILLE / 3;
  EXPECT_EQ(sect_world(third).sect_elders(SECT), std::vector<std::size_t>{0});
  // Control: with the second founder the stronger, it is the elder.
  third.founding = {WEAKER, STRONGER};
  EXPECT_EQ(sect_world(third).sect_elders(SECT), std::vector<std::size_t>{1});
}

// Sectarian `sectarian`'s world a day after both founders were conscripted
// into faction 1's army, which leaves the sect with no member.
World fallen(const Sectarian& sectarian) {
  World world = sect_world(sectarian);
  world.conscript(0, OUTSIDERS);
  world.conscript(1, OUTSIDERS);
  run_ticks(world, 1);
  return world;
}

TEST(World, AFallenSectScattersOrLosesItsManuals) {
  Sectarian scattering;
  scattering.manuals = Manuals{{{0}}, PER_MILLE, 3};
  const World world = fallen(scattering);
  ASSERT_FALSE(world.sect_stands(SECT));
  const Cell* cell = std::get_if<Cell>(&world.manuals().front().place);
  ASSERT_NE(cell, nullptr);
  EXPECT_LE(std::abs(cell->x - SEAT_HOME.x), 3);
  EXPECT_LE(std::abs(cell->y - SEAT_HOME.y), 3);
  // The same seed scatters it to the same cell.
  const World repeated = fallen(scattering);
  const Cell* again = std::get_if<Cell>(&repeated.manuals().front().place);
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->x, cell->x);
  EXPECT_EQ(again->y, cell->y);
  // With no chance to scatter, it is lost.
  EXPECT_TRUE(std::holds_alternative<Lost>(
      fallen(Sectarian{}).manuals().front().place));
  // A fallen sect admits no one.
  World emptied = fallen(Sectarian{});
  run_ticks(emptied, SECT_YEARS);
  EXPECT_TRUE(emptied.sect_members(SECT).empty());
  // Control: a standing sect keeps its manual.
  EXPECT_TRUE(std::holds_alternative<HeldBySect>(
      sect_world_after(scattering, 1).manuals().front().place));
}

TEST(World, AnElectionMakesTheMasterLeaderWhenNoOneLeads) {
  Sectarian electing;
  electing.election = Election{0};
  const World world = sect_world_after(electing, 1);
  EXPECT_EQ(world.leader(0), std::optional<std::size_t>{0});
  // A faction with an election and no duel still refuses every challenge.
  EXPECT_EQ(world.challenge_refusal(VILLAGE_ARMY, 0),
            ChallengeRefusal::NO_SUCCESSION);
  // Control: without an election no one comes to lead.
  EXPECT_FALSE(sect_world_after(Sectarian{}, 1).leader(0).has_value());
}

}  // namespace
}  // namespace murim
