#include "world.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "areas.hpp"
#include "arts.hpp"
#include "attack.hpp"
#include "calendar.hpp"
#include "chart.hpp"
#include "coarse_graph.hpp"
#include "diplomacy.hpp"
#include "event.hpp"
#include "flow_field.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "leadership.hpp"
#include "life.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "schedule.hpp"
#include "sect.hpp"
#include "skill.hpp"
#include "spatial_hash.hpp"
#include "talent.hpp"
#include "talk.hpp"
#include "trade.hpp"
#include "village.hpp"

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
  // With ADR 0068's attack slots and tokens, so the pinned hash covers them
  // too. A token is held only by someone engaging a foe in sight, and at 10
  // cells every army meets a foe on its way.
  Rules rules = at_war(SEPARATION);
  rules.tokens = default_tokens();
  constexpr std::int32_t MEETING_SIGHT = 10;
  rules.sight = MEETING_SIGHT;
  World world{seed, Map{crossing_map()}, armies, crossing_structures(), rules};
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
            0xAB0A003912E54CCCULL);
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

// How one person ordered from `start` to `objective` on an open map walks
// there: the ticks it takes to stand on the objective's cell, or
// ARRIVAL_TICKS when it never does, and the furthest it strays from the
// line between the two cells' centres, in sub-cell units.
struct Walk {
  int ticks;
  std::int64_t strayed;
};

Walk walk_alone(Cell start, Cell objective, std::size_t side = OPEN_SIDE) {
  World world{Seed{1},
              Map{open_map(side)},
              {army(start, objective, 1, 1, 0, {IDLE})},
              {},
              peaceful(SEPARATION)};
  const Position from = centre_of(start);
  const Position end = centre_of(objective);
  const std::int64_t along_x = std::int64_t{end.x} - from.x;
  const std::int64_t along_y = std::int64_t{end.y} - from.y;
  const auto length = static_cast<std::int64_t>(std::sqrt(
      static_cast<double>((along_x * along_x) + (along_y * along_y))));
  Walk walk{ARRIVAL_TICKS, 0};
  for (int tick = 1; tick <= ARRIVAL_TICKS; ++tick) {
    world.tick();
    const Position here = lead(world, 0);
    const std::int64_t cross = (along_x * (here.y - from.y)) -
                               (along_y * (std::int64_t{here.x} - from.x));
    walk.strayed = std::max(walk.strayed, std::abs(cross) / length);
    if (in(objective, here)) {
      walk.ticks = tick;
      break;
    }
  }
  return walk;
}

TEST(World, APersonOrderedAlongADiagonalWalksItStraightAtItsSpeed) {
  // Ten cells each way: the objective's cell is entered half a cell short of
  // its centre, 9.5 cells each way from the start's.
  const Walk diagonal = walk_alone({2, 2}, {12, 12});
  const Walk straight = walk_alone({2, 2}, {12, 2});
  // Setup: both arrived.
  ASSERT_LT(diagonal.ticks, ARRIVAL_TICKS);
  ASSERT_LT(straight.ticks, ARRIVAL_TICKS);

  // On the line between the centres, where a staircase strays half a cell,
  // and in its row walked straight.
  EXPECT_EQ(diagonal.strayed, 0);
  EXPECT_EQ(straight.strayed, 0);
  // 9.5 cells, 2,432 units, at SPEED straight; at DIAGONAL_SPEED on each
  // axis diagonally, 11 times the square root of two, 2.8% short of SPEED
  // over the square root of two times as far.
  EXPECT_EQ(straight.ticks, 152);
  EXPECT_EQ(diagonal.ticks, 222);
}

// An open map of 4 by 4 areas.
constexpr std::size_t AREAS_SIDE = 128;
// The walk from (2, 10) to (100, 70) by one field over the whole map, on
// main 5477540 before fields within areas: its ticks, and how far it strays
// from the line, 7.4 cells, since each step is the one nearest the line from
// its own cell (ADR 0037), not from the start's.
constexpr int WHOLE_MAP_TICKS = 1'993;
constexpr std::int64_t WHOLE_MAP_STRAY = 1'906;

TEST(World, AWalkAcrossThreeAreasOnOpenGroundIsTheWholeMapFieldsWalk) {
  // From area (0, 0) to area (3, 2), off every diagonal, by the fields of
  // the areas it crosses (ADR 0043).
  const Cell start{2, 10};
  const Cell objective{100, 70};
  const Walk across = walk_alone(start, objective, AREAS_SIDE);
  // Setup: it arrived, across three areas or more.
  ASSERT_LT(across.ticks, ARRIVAL_TICKS);
  ASSERT_GE((objective.x / AREA_SIDE) - (start.x / AREA_SIDE), 3);
  EXPECT_EQ(across.ticks, WHOLE_MAP_TICKS);
  EXPECT_EQ(across.strayed, WHOLE_MAP_STRAY);
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
  // To within a sub-cell unit: two people a diagonal apart and a unit short
  // of it push each other less than a unit along either axis, which rounds
  // to none.
  const std::int64_t apart = std::int64_t{SEPARATION - 1} * (SEPARATION - 1);
  const std::vector<Position> separated = arrive(SEPARATION);
  // Setup: the army arrived, its lead within a cell of the objective.
  ASSERT_LE(distance_squared(separated.front(), centre_of({12, 12})),
            std::int64_t{CELL_SIZE} * CELL_SIZE);
  EXPECT_GE(closest_pair_squared(separated), apart);
  // Control: without separation the people pack closer.
  EXPECT_LT(closest_pair_squared(arrive(0)), apart);
}

// The closest any two members of a column of nine come while it marches
// across a 48-cell map toward a cell eight rows off its own, so its people
// close on one another, once it has spread out from its start and
// before its first members near the objective and stand: its lead stays more
// than ten cells short of it.
std::int64_t closest_while_marching(std::int32_t separation) {
  constexpr std::size_t SIDE = 48;
  constexpr int SPREAD_TICKS = 100;
  constexpr int MARCH_TICKS = 300;
  const std::vector<std::string> rows = open_map(SIDE);
  const ArmySpec column = army(rows, {2, 16}, {45, 24}, 9, 1, 0, {IDLE});
  World world{Seed{1}, Map{rows}, {column}, {}, peaceful(separation)};
  run_ticks(world, SPREAD_TICKS);
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  for (int tick = 0; tick < MARCH_TICKS; ++tick) {
    world.tick();
    closest = std::min(closest, closest_pair_squared(positions(world, 0)));
  }
  // Setup: the column marched, so the distances were measured on the move,
  // and its lead stayed short of where its first members stand.
  EXPECT_GT(cell_of(lead(world, 0)).x, 10);
  EXPECT_LT(cell_of(lead(world, 0)).x, 45 - 10);
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

// Two people crossing an open map on diagonals that meet at its centre, of
// factions at war or at peace, each with a strike of three cells that
// never kills in the time given.
constexpr Cell NORTH_WEST{2, 2};
constexpr Cell NORTH_EAST{12, 2};
constexpr Cell SOUTH_WEST{2, 12};
constexpr Cell SOUTH_EAST{12, 12};

World crossing_diagonals(bool war) {
  const Skill strike{3 * CELL_SIZE, 0, 1, 0, 10};
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army(NORTH_WEST, SOUTH_EAST, 1, TARGET_HEALTH, 0, {strike}),
                army(NORTH_EAST, SOUTH_WEST, 1, TARGET_HEALTH, 1, {strike})},
               {},
               Rules{SEPARATION, {{false, war}, {war, false}}, STAGE_FLOORS}};
}

// The closest the two people of `world` come in ARRIVAL_TICKS, squared.
std::int64_t closest_crossing(World& world) {
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  for (int tick = 0; tick < ARRIVAL_TICKS; ++tick) {
    world.tick();
    closest =
        std::min(closest, distance_squared(lead(world, 0), lead(world, 1)));
  }
  return closest;
}

TEST(World, FoesOnCrossingDiagonalsHoldWithinReachAndFight) {
  const std::int64_t reach = std::int64_t{3} * CELL_SIZE;
  const std::int64_t two_cells = std::int64_t{2} * CELL_SIZE;
  World war = crossing_diagonals(true);
  const std::int64_t held = closest_crossing(war);

  // Each halted with the other ahead within its reach, short of two cells
  // apart, held there, and both were struck.
  EXPECT_GT(held, two_cells * two_cells);
  EXPECT_LE(distance_squared(lead(war, 0), lead(war, 1)), reach * reach);
  EXPECT_LT(health_of(war, 0), TARGET_HEALTH);
  EXPECT_LT(health_of(war, 1), TARGET_HEALTH);
  // Ordered back the way it came, its foe behind it, one walks off.
  war.order(0, {NORTH_WEST, {0}});
  run_ticks(war, ARRIVAL_TICKS);
  EXPECT_TRUE(in(NORTH_WEST, lead(war, 0)));

  // Control: at peace the same two walk on within two cells of each other,
  // where foes hold.
  World peace = crossing_diagonals(false);
  EXPECT_LT(closest_crossing(peace), two_cells * two_cells);
  EXPECT_EQ(health_of(peace, 0), TARGET_HEALTH);
}

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

TEST(World, AWarDeclaredBetweenTicksIsStruckOnTheNextTick) {
  const Skill reach{2 * CELL_SIZE, 0, 1, 0, 1};
  // Two leaders of factions at peace by the rules, each in the other's
  // reach.
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army({2, 0}, {2, 0}, 1, TARGET_HEALTH, 0, {reach}),
               army({4, 0}, {4, 0}, 1, TARGET_HEALTH, 1, {reach})},
              {},
              Rules{SEPARATION, {{false, false}, {false, false}}, STAGE_FLOORS},
              {},
              {{0, std::nullopt, std::nullopt, std::nullopt},
               {1, std::nullopt, std::nullopt, std::nullopt}}};
  // The control: at peace, neither strikes.
  world.tick();
  ASSERT_EQ(health_of(world, 1), TARGET_HEALTH);
  world.declare(0, 1, Declaration::WAR);
  ASSERT_TRUE(world.hostile(0, 1));
  world.tick();

  EXPECT_LT(health_of(world, 1), TARGET_HEALTH);
  EXPECT_LT(health_of(world, 0), TARGET_HEALTH);
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
// the open one, is created under `rules`.
bool populated(std::size_t people, const Rules& rules = peaceful(0)) {
  const std::vector<std::string> rows = open_map(OPEN_SIDE * 2);
  try {
    const World world{Seed{1},
                      Map{rows},
                      {army(rows, {0, 0}, {0, 0}, people, 1, 0, {IDLE})},
                      {},
                      rules};
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

// peaceful(0) with a capacity of `capacity` people.
Rules with_capacity(std::size_t capacity) {
  Rules rules = peaceful(0);
  rules.people_capacity = capacity;
  return rules;
}

TEST(World, AWorldHoldsNoMoreThanTheCapacityItsRulesSet) {
  constexpr std::size_t CAPACITY = MAX_PEOPLE + 10;
  EXPECT_FALSE(populated(CAPACITY + 1, with_capacity(CAPACITY)));
  // Control: as many as the capacity are accepted, past MAX_PEOPLE.
  EXPECT_TRUE(populated(CAPACITY, with_capacity(CAPACITY)));
  // A capacity past the ceiling is refused, however few live.
  EXPECT_FALSE(populated(1, with_capacity(PEOPLE_CEILING + 1)));
  // Control: the ceiling itself is accepted.
  EXPECT_TRUE(populated(1, with_capacity(PEOPLE_CEILING)));
}

TEST(World, ACapacityOtherThanMaxPeopleChangesTheHash) {
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const auto hashed = [&rows](std::size_t capacity) {
    return World{Seed{1},
                 Map{rows},
                 {army(rows, {0, 0}, {0, 0}, 1, 1, 0, {IDLE})},
                 {},
                 with_capacity(capacity)}
        .hash();
  };
  EXPECT_NE(hashed(MAX_PEOPLE + 1), hashed(MAX_PEOPLE));
  // Control: the default capacity hashes as a world whose rules never set one.
  EXPECT_EQ(hashed(MAX_PEOPLE),
            (World{Seed{1},
                   Map{rows},
                   {army(rows, {0, 0}, {0, 0}, 1, 1, 0, {IDLE})},
                   {},
                   peaceful(0)}
                 .hash()));
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
// person at (12, 4); its steered see the land within `map_sight` cells.
constexpr std::size_t HOME_ARMY = 0;
constexpr std::size_t FIELD = 1;
constexpr std::size_t POST = 2;
constexpr Cell HOME{4, 4};
constexpr Cell POST_CELL{12, 4};
constexpr std::uint64_t POST_HEALTH = 200;
constexpr std::uint32_t HOME_DURABILITY = 100;

World conscription_world(std::int32_t map_sight = 0) {
  const Skill strike{2 * CELL_SIZE, 0, 10, 0, 15};
  const std::vector<ArmySpec> armies{
      army(HOME, HOME, 3, 1'000, 0, {strike}),
      ArmySpec{HOME, {}, 0, {strike}},
      army(POST_CELL, POST_CELL, 1, POST_HEALTH, 1, {strike}),
  };
  Rules rules = at_war(SEPARATION);
  rules.map_sight = map_sight;
  return World{Seed{1}, Map{open_map(OPEN_SIDE)},
               armies,  {{{2, 2}, HOME_DURABILITY}},
               rules,   {{{0}, HOME_ARMY, {}, 0, {}, {}}}};
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
  late.at(2).tick += 1;
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

VillageSpec small_village() {
  return VillageSpec{{0, 1}, RESIDENTS, {}, 0, {}, {}};
}

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

// village_world() with a neutral watcher of faction 2 at `watcher`, steered
// and carrying a soul, who sees the land within WATCH_SIGHT cells.
constexpr std::size_t WATCHER = 3;
constexpr std::int32_t WATCH_SIGHT = 4;
constexpr Cell BESIDE_VILLAGE{10, 12};
constexpr Cell WATCH_SEAT{10, 10};
constexpr Cell WATCH_CORNER{22, 1};

World watched_village_world() {
  const Skill guard{3 * CELL_SIZE, 0, 1, 0, 15};
  const Skill strike{3 * CELL_SIZE, 0, 50, 0, 10};
  const Skill torch{0, 3 * CELL_SIZE, 0, 10, 10};
  const Skill mend{0, 3 * CELL_SIZE, 0, -2, 10};
  const std::vector<ArmySpec> armies{
      ArmySpec{WATCH_SEAT,
               {Recruit{{9, 10}, RESIDENT_HEALTH},
                Recruit{{10, 10}, RESIDENT_HEALTH},
                Recruit{{11, 10}, RESIDENT_HEALTH},
                Recruit{{12, 10}, RESIDENT_HEALTH}},
               0,
               {guard}},
      army(open_map(ORDERS_SIDE), RAIDER_HOME, RAIDER_HOME, 10, 100'000, 1,
           {strike, torch}),
      army(open_map(ORDERS_SIDE), MENDER_HOME, MENDER_HOME, 10, 100'000, 2,
           {mend}),
      army(open_map(ORDERS_SIDE), BESIDE_VILLAGE, BESIDE_VILLAGE, 1, 100'000, 2,
           {IDLE}),
  };
  Rules rules{
      SEPARATION,
      {{false, true, false}, {true, false, false}, {false, false, false}},
      STAGE_FLOORS};
  rules.map_sight = WATCH_SIGHT;
  World world{VILLAGE_SEED,
              Map{open_map(ORDERS_SIDE)},
              armies,
              {{{WEST_HOUSE, HOUSE_TOP}, HOUSE_DURABILITY, SMALL_HOUSE},
               {{EAST_HOUSE, HOUSE_TOP}, HOUSE_DURABILITY, SMALL_HOUSE}},
              rules,
              {small_village()}};
  world.ensoul(world.roster(WATCHER).front());
  world.steer(WATCHER, true);
  return world;
}

// The village as the watcher's soul, the world's only one, last saw it.
std::optional<VillageMemory> remembered(const World& world) {
  const std::vector<std::optional<VillageMemory>>& villages =
      world.chart(0).villages();
  return villages.empty() ? std::nullopt : villages.front();
}

// Whether the watcher stands on `cell`.
bool watcher_on(const World& world, Cell cell) {
  const Cell here =
      cell_of(world.person(world.roster(WATCHER).front()).position);
  return here.x == cell.x && here.y == cell.y;
}

// Ticks until the watcher stands on `cell`, or PHASE_TICKS pass.
void walk_watcher(World& world, Cell cell) {
  world.order(WATCHER, {cell, {0}});
  for (std::uint64_t tick = 0; tick < PHASE_TICKS && !watcher_on(world, cell);
       ++tick) {
    world.tick();
  }
}

// Ticks until the village lies in ruins, or PHASE_TICKS pass.
void raze(World& world) {
  world.order(RAIDER, {IN_VILLAGE, {0, 1}});
  for (std::uint64_t tick = 0;
       tick < PHASE_TICKS && world.village_state(0) != VillageState::RUINS;
       ++tick) {
    world.tick();
  }
}

TEST(World, AVillageBurntOutOfSightIsRememberedAsItWasUntilSeenAgain) {
  World world = watched_village_world();
  world.tick();
  const VillageMemory standing{0, true, VillageStage::STANDING, WATCH_SEAT};
  ASSERT_EQ(remembered(world), standing);

  walk_watcher(world, WATCH_CORNER);
  ASSERT_TRUE(watcher_on(world, WATCH_CORNER));
  raze(world);
  ASSERT_EQ(world.village_state(0), VillageState::RUINS);
  // Burnt while the watcher was away: it is remembered standing and peopled.
  EXPECT_EQ(remembered(world), standing);

  walk_watcher(world, BESIDE_VILLAGE);
  ASSERT_TRUE(watcher_on(world, BESIDE_VILLAGE));
  EXPECT_EQ(remembered(world),
            (VillageMemory{0, false, VillageStage::RUINS, WATCH_SEAT}));

  // Control: the same burning in sight is remembered on the tick it ends.
  World watched = watched_village_world();
  watched.tick();
  ASSERT_EQ(remembered(watched), standing);
  raze(watched);
  ASSERT_EQ(watched.village_state(0), VillageState::RUINS);
  ASSERT_TRUE(remembered(watched).has_value());
  EXPECT_EQ(remembered(watched).value_or(standing).stage, VillageStage::RUINS);
}

// Rebuilds the razed village under faction 2 while the raiders leave it.
void rebuild_watched_village(World& world) {
  world.order(RAIDER, {RAIDER_HOME, {0, 1}});
  world.order(MENDER, {IN_VILLAGE, {0}});
  run_ticks(world, PHASE_TICKS);
}

TEST(World, AVillageRebuiltOutOfSightKeepsItsRememberedOwnerUntilSeenAgain) {
  World world = watched_village_world();
  world.tick();
  const VillageMemory standing{0, true, VillageStage::STANDING, WATCH_SEAT};
  ASSERT_EQ(remembered(world), standing);
  walk_watcher(world, WATCH_CORNER);
  ASSERT_TRUE(watcher_on(world, WATCH_CORNER));
  raze(world);
  ASSERT_EQ(world.village_state(0), VillageState::RUINS);
  rebuild_watched_village(world);
  ASSERT_EQ(world.owner(0), 2U);
  ASSERT_EQ(world.stage(0), Stage::INTACT);
  ASSERT_EQ(world.stage(1), Stage::INTACT);
  EXPECT_EQ(remembered(world), standing);
  walk_watcher(world, BESIDE_VILLAGE);
  ASSERT_TRUE(watcher_on(world, BESIDE_VILLAGE));
  const VillageMemory rebuilt{2, true, VillageStage::STANDING, IN_VILLAGE};
  EXPECT_EQ(remembered(world), rebuilt);

  // Control: the same rebuilding in sight updates the remembered owner.
  World watched = watched_village_world();
  watched.tick();
  raze(watched);
  ASSERT_EQ(watched.village_state(0), VillageState::RUINS);
  rebuild_watched_village(watched);
  ASSERT_EQ(watched.owner(0), 2U);
  ASSERT_EQ(watched.stage(0), Stage::INTACT);
  ASSERT_EQ(watched.stage(1), Stage::INTACT);
  EXPECT_EQ(remembered(watched), rebuilt);
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
  EXPECT_FALSE(refused({{{0, 1}, 0, {}, 0, {}, {}}}));

  // A structure the world lacks, one two villages name, an army it lacks.
  EXPECT_TRUE(refused({{{0, 2}, 0, {}, 0, {}, {}}}));
  EXPECT_TRUE(refused({{{0}, 0, {}, 0, {}, {}},
                       {{1}, 0, {}, 0, {}, {}},
                       {{0}, 0, {}, 0, {}, {}}}));
  EXPECT_TRUE(refused({{{0, 1}, 1, {}, 0, {}, {}}}));
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
              at_war(0), {{{0}, 0, {}, 0, {}, {}}}};
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
                 std::optional<std::size_t> cult_leader = 0,
                 const Life& life = {},
                 std::optional<std::size_t> ally_leader = std::nullopt,
                 std::uint64_t ticks_per_day = TICKS_PER_DAY) {
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
  Rules rules{
      SEPARATION,
      {{false, true, false}, {true, false, false}, {false, false, false}},
      STAGE_FLOORS};
  rules.ticks_per_day = ticks_per_day;
  return World{CULT_SEED,
               Map{rows},
               armies,
               {},
               rules,
               {},
               {{},
                {cult_leader, Duel{true, 0, true}, std::nullopt, std::nullopt},
                {ally_leader, std::nullopt, std::nullopt, std::nullopt}},
               life};
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

// Whether declare() refuses person `person`'s declaration of `what` toward
// faction `faction` exactly when declaration_refusal() names a reason, and
// that reason is `expected`.
bool declaration_refused_for(World& world, std::size_t person,
                             std::size_t faction, Declaration what,
                             DeclarationRefusal expected) {
  if (world.declaration_refusal(person, faction, what) != expected) {
    return false;
  }
  bool thrown = false;
  try {
    world.declare(person, faction, what);
  } catch (const std::invalid_argument&) {
    thrown = true;
  }
  return thrown == (expected != DeclarationRefusal::NONE);
}

// Diplomacy that holds every declaration `hold_days` days and, with
// `chance` per mille a year for each, declares war on a faction no more
// numerous and makes peace.
Life diplomatic(std::uint32_t hold_days, std::uint32_t chance = 0) {
  Life life;
  life.diplomacy = Diplomacy{PER_MILLE, chance, chance, hold_days};
  return life;
}

TEST(World, OnlyAFactionsLeaderDeclaresWarOrMakesPeace) {
  World world = cult_world(TARGET_HEALTH);
  EXPECT_TRUE(declaration_refused_for(world, PLAYER, ALLY, Declaration::WAR,
                                      DeclarationRefusal::NOT_THE_LEADER));
  EXPECT_TRUE(declaration_refused_for(world, OUTSIDER, CULT, Declaration::WAR,
                                      DeclarationRefusal::NOT_THE_LEADER));
  EXPECT_EQ(world.relation(CULT, ALLY), Relation::ALLY);
  // Control: the Cult's leader, person 0, may, and the war is both
  // factions'.
  EXPECT_TRUE(declaration_refused_for(world, 0, ALLY, Declaration::WAR,
                                      DeclarationRefusal::NONE));
  EXPECT_EQ(world.relation(CULT, ALLY), Relation::AT_WAR);
  EXPECT_EQ(world.relation(ALLY, CULT), Relation::AT_WAR);
  EXPECT_TRUE(world.hostile(ALLY, CULT));
  EXPECT_TRUE(world.hostile(CULT, ALLY));
  const std::optional<Declared> made = world.declaration(ALLY, CULT);
  ASSERT_TRUE(made.has_value());
  EXPECT_EQ(made.value_or(Declared{}).by, CULT);
  EXPECT_EQ(made.value_or(Declared{}).tick, world.tick_count());
}

TEST(World, ADeclarationIsRefusedForEachReasonDeclareRefusesIt) {
  World world = cult_world(TARGET_HEALTH, 0, diplomatic(1));
  EXPECT_TRUE(declaration_refused_for(world, 0, CULT, Declaration::WAR,
                                      DeclarationRefusal::SAME_FACTION));
  EXPECT_TRUE(declaration_refused_for(world, 0, ALLY, Declaration::PEACE,
                                      DeclarationRefusal::ALREADY_AT_PEACE));
  // Control: peace with a faction hostile by the rules is made, and then
  // neither attacks the other.
  ASSERT_EQ(world.relation(CULT, ORTHODOX), Relation::HOSTILE);
  EXPECT_TRUE(declaration_refused_for(world, 0, ORTHODOX, Declaration::PEACE,
                                      DeclarationRefusal::NONE));
  EXPECT_EQ(world.relation(ORTHODOX, CULT), Relation::NEUTRAL);
  EXPECT_FALSE(world.hostile(ORTHODOX, CULT));
  EXPECT_TRUE(declaration_refused_for(world, 0, ORTHODOX, Declaration::PEACE,
                                      DeclarationRefusal::ALREADY_AT_PEACE));
  // A day of one tick must pass before the peace may be broken.
  EXPECT_TRUE(declaration_refused_for(world, 0, ORTHODOX, Declaration::WAR,
                                      DeclarationRefusal::TOO_SOON));
  run_ticks(world, TICKS_PER_DAY);
  EXPECT_TRUE(declaration_refused_for(world, 0, ORTHODOX, Declaration::WAR,
                                      DeclarationRefusal::NONE));
  EXPECT_TRUE(declaration_refused_for(world, 0, ORTHODOX, Declaration::WAR,
                                      DeclarationRefusal::ALREADY_AT_WAR));

  // A challenger who lost the duel died.
  World lost = cult_world(1);
  ASSERT_EQ(after_challenge(lost, PLAYER), std::optional<std::size_t>{0});
  EXPECT_TRUE(declaration_refused_for(lost, PLAYER, ALLY, Declaration::WAR,
                                      DeclarationRefusal::DEAD));
  EXPECT_THROW(
      static_cast<void>(lost.declaration_refusal(99, ALLY, Declaration::WAR)),
      std::invalid_argument);
  EXPECT_THROW(
      static_cast<void>(lost.declaration_refusal(0, 3, Declaration::WAR)),
      std::invalid_argument);
}

TEST(World, ALeaderNoDriverSteersDeclaresByTheDiplomacy) {
  // The Cult, three living, outnumbers its ally of one, so at a chance of a
  // thousand per mille a year its leader declares war within ten years of
  // one-tick days.
  constexpr std::uint64_t TEN_YEARS = 10 * DAYS_PER_YEAR;
  World own =
      cult_world(TARGET_HEALTH, 0, diplomatic(0, PER_MILLE), std::nullopt, 1);
  ASSERT_FALSE(own.steered(0));
  run_ticks(own, TEN_YEARS);
  EXPECT_EQ(own.relation(CULT, ALLY), Relation::AT_WAR);
  // It leaves its foe by the rules as it is.
  EXPECT_EQ(own.relation(CULT, ORTHODOX), Relation::HOSTILE);
  // Control: the same leader, steered, declares nothing.
  World steered =
      cult_world(TARGET_HEALTH, 0, diplomatic(0, PER_MILLE), std::nullopt, 1);
  steered.steer(0, true);
  ASSERT_TRUE(steered.steered(0));
  run_ticks(steered, TEN_YEARS);
  EXPECT_EQ(steered.relation(CULT, ALLY), Relation::ALLY);
  EXPECT_FALSE(steered.declaration(CULT, ALLY).has_value());
}

TEST(World, ALeaderOutnumberedInADeclaredWarMakesPeace) {
  // The Cult's leader, steered, declares war on its ally of one, led by
  // person 4; outnumbered three to one, the ally's leader makes peace.
  constexpr std::uint64_t TEN_YEARS = 10 * DAYS_PER_YEAR;
  World world = cult_world(TARGET_HEALTH, 0, diplomatic(0, PER_MILLE), 4, 1);
  world.steer(0, true);
  world.declare(0, ALLY, Declaration::WAR);
  run_ticks(world, TEN_YEARS);
  EXPECT_EQ(world.relation(CULT, ALLY), Relation::NEUTRAL);
  const std::optional<Declared> made = world.declaration(CULT, ALLY);
  EXPECT_EQ(made.value_or(Declared{}).by, ALLY);
  // Control: with the ally's leader steered too, the war stands.
  World both = cult_world(TARGET_HEALTH, 0, diplomatic(0, PER_MILLE), 4, 1);
  both.steer(0, true);
  both.steer(4, true);
  both.declare(0, ALLY, Declaration::WAR);
  run_ticks(both, TEN_YEARS);
  EXPECT_EQ(both.relation(CULT, ALLY), Relation::AT_WAR);
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

// One discipline whose limit is drawn over [500 - spread, 500 + spread],
// and one art of it with no skills, which a manual may name.
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
              std::nullopt,
              std::nullopt,
              {},
              {Art{{"검법", "Sword art"}, 0, Nature{}, {}}},
              Learning{},
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

TEST(World, ASoulServesInTheArmyOfItsLatestBody) {
  World world = soul_world();
  EXPECT_EQ(world.soul_army(0), std::optional<std::size_t>{POST});
  run_ticks(world, ARRIVAL_TICKS);
  // Setup: the post died and its soul was born again into the post's army.
  ASSERT_EQ(world.person(world.population() - 1).soul,
            std::optional<std::size_t>{0});
  EXPECT_EQ(world.soul_army(0), std::optional<std::size_t>{POST});
  // Control: a soul no one carries serves in no army.
  EXPECT_FALSE(world.soul_army(1).has_value());
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

TEST(World, ASoulBornAgainHasRoomForAnAttackSlot) {
  World world = soul_world();
  const std::size_t before = world.population();
  ASSERT_EQ(world.attacks().people(), before);
  run_ticks(world, ARRIVAL_TICKS);

  // Setup: the soul was born again as the world's last person.
  ASSERT_EQ(world.population(), before + 1);
  ASSERT_TRUE(world.person(before).soul.has_value());
  EXPECT_EQ(world.attacks().people(), world.population());
}

// The post's soul, steered, in conscription_world() with a map sight of
// CHART_SIGHT; returns the soul.
constexpr std::int32_t CHART_SIGHT = 3;

std::size_t steered_post(World& world) {
  const std::size_t soul = world.ensoul(world.roster(POST).front());
  world.steer(POST, true);
  return soul;
}

TEST(World, ASteeredSoulChartsTheCellsWithinItsMapSight) {
  World world = conscription_world(CHART_SIGHT);
  const std::size_t soul = steered_post(world);
  ASSERT_TRUE(world.chart(soul).empty());
  world.tick();

  const Chart& chart = world.chart(soul);
  EXPECT_TRUE(chart.seen(POST_CELL));
  EXPECT_TRUE(chart.seen(Cell{POST_CELL.x - CHART_SIGHT, POST_CELL.y}));
  EXPECT_FALSE(chart.seen(Cell{POST_CELL.x - CHART_SIGHT - 1, POST_CELL.y}));
  // The house at (2, 2) lies 10 cells off, out of view.
  EXPECT_TRUE(chart.villages().empty());
  // Controls: the same soul unsteered, or steered with no map sight, charts
  // nothing.
  World unsteered = conscription_world(CHART_SIGHT);
  const std::size_t loose = unsteered.ensoul(unsteered.roster(POST).front());
  unsteered.tick();
  EXPECT_TRUE(unsteered.chart(loose).empty());
  World blind = conscription_world(0);
  const std::size_t unseeing = steered_post(blind);
  blind.tick();
  EXPECT_TRUE(blind.chart(unseeing).empty());
}

TEST(World, ASoulKeepsItsChartWhenItIsBornAgain) {
  World world = conscription_world(CHART_SIGHT);
  const std::size_t soul = steered_post(world);
  const std::size_t post = world.roster(POST).front();
  const std::size_t before = world.population();
  world.order(POST, {HOME, {0}});
  run_ticks(world, ARRIVAL_TICKS);

  // Setup: the post died at the village and its soul was born again there.
  ASSERT_EQ(world.person(post).health, 0U);
  ASSERT_EQ(world.population(), before + 1);
  ASSERT_EQ(world.person(before).soul, std::optional<std::size_t>{soul});
  const Cell reborn_at = cell_of(world.person(before).position);
  ASSERT_GT(std::abs(reborn_at.x - POST_CELL.x), CHART_SIGHT);
  // Where the first body started, out of the new body's sight, is still
  // charted, and so is the village it reached.
  EXPECT_TRUE(world.chart(soul).seen(POST_CELL));
  ASSERT_EQ(world.chart(soul).villages().size(), 1U);
  EXPECT_TRUE(world.chart(soul).villages().front().has_value());
  // Control: a cell no body of the soul came within sight of is not.
  EXPECT_FALSE(
      world.chart(soul).seen(Cell{static_cast<std::int32_t>(OPEN_SIDE) - 1,
                                  static_cast<std::int32_t>(OPEN_SIDE) - 1}));
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
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army(SQUARE, SQUARE, FOUNDERS, 1, 0, {IDLE}),
                army(second_square, second_square, others, 1, 0, {IDLE})},
               structures,
               daily,
               {VillageSpec{first_structures,
                            0,
                            {first.house, first.granary, first.field},
                            0,
                            {},
                            {}},
                VillageSpec{second_structures,
                            1,
                            {second.house, second.granary, second.field},
                            0,
                            {},
                            {}}},
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

TEST(World, AVillagesHarvestIsWhatItsIntactFieldsYieldPastItsStorage) {
  constexpr std::uint32_t YIELD = 3 * DAYS_PER_YEAR;
  const Holdings other = holding(1, DAYS_PER_YEAR, DAYS_PER_YEAR);
  // Storage for one year's food bounds the stock, not the harvest.
  const World world =
      economy_world(holding(FOR_TWENTY, DAYS_PER_YEAR, YIELD), other, 1);
  EXPECT_EQ(world.harvest(0), YIELD);
  // Control: the same field burnt yields nothing.
  Holdings burnt = holding(FOR_TWENTY, DAYS_PER_YEAR, YIELD);
  burnt.field_durability = BURNT;
  EXPECT_EQ(economy_world(burnt, other, 1).harvest(0), 0U);
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
  World raided{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(SQUARE, SQUARE, 1, 1, 0, {IDLE}),
       army({4, 1}, {4, 1}, 1, 1, 1, {torch})},
      {Structure{{3, 1}, INTACT, {1, 1}, false}},
      at_war(0),
      {VillageSpec{{0}, 0, {Holding{0, 0, DAYS_PER_YEAR}}, 0, {}, {}}}};
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

// A village of one workshop at (1, 1), holding `workshop` at `durability`,
// and `residents` residents of faction 0 around SQUARE of `health` each, who
// strike once with a blow of WORKSHOP_BLOW within five cells; and one
// stranger of faction 1, at war with them, of STRANGER_HEALTH, two cells
// south of SQUARE. A newborn has NEWBORN_HEALTH, and days last a tick.
constexpr std::uint64_t WORKSHOP_BLOW = 100;
constexpr std::uint64_t STRANGER_HEALTH = 100'000;
constexpr std::uint64_t NEWBORN_HEALTH = 100;
constexpr std::size_t STRANGER = 1;
// A smithy's edge, half again as hard, and an apothecary's daily healing.
constexpr std::uint32_t HALF_AGAIN = 500;
constexpr std::uint32_t DAILY_HEALING = 25;
// Durability in the damaged and the destroyed stage.
constexpr std::uint32_t DAMAGED = 50;
constexpr std::uint32_t DESTROYED = 10;

struct Workshop {
  Holding holding;
  std::uint32_t durability = INTACT;
  std::size_t residents = 2;
  std::uint64_t health = NEWBORN_HEALTH;
};

World workshop_world(const Workshop& workshop) {
  constexpr std::int32_t FIVE_CELLS = 5 * CELL_SIZE;
  const Skill blow{FIVE_CELLS, 0, WORKSHOP_BLOW, 0, NEVER_AGAIN};
  const Cell stranger{SQUARE.x, SQUARE.y + 2};
  Rules daily = at_war(0);
  daily.ticks_per_day = 1;
  Life life;
  life.newborn_health = NEWBORN_HEALTH;
  return World{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(SQUARE, SQUARE, workshop.residents, workshop.health, 0, {blow}),
       army(stranger, stranger, 1, STRANGER_HEALTH, 1, {IDLE})},
      {Structure{{1, 1}, workshop.durability, {1, 1}, true}},
      daily,
      {VillageSpec{{0}, 0, {workshop.holding}, 0, {}, {}}},
      {},
      life};
}

// The health the stranger lost to `workshop`'s residents' one strike each.
std::uint64_t struck(const Workshop& workshop) {
  World world = workshop_world(workshop);
  run_ticks(world, 1);
  return STRANGER_HEALTH - health_of(world, STRANGER);
}

TEST(World, ResidentsStrikeHarderWhileASmithyArmingThemStands) {
  Holding smithy;
  smithy.arms = 2;
  smithy.edge = HALF_AGAIN;
  // Two residents' blows land half again as hard, intact or damaged.
  constexpr std::uint64_t PLAIN = 2 * WORKSHOP_BLOW;
  constexpr std::uint64_t SHARPENED = PLAIN + (PLAIN / 2);
  EXPECT_EQ(struck(Workshop{smithy}), SHARPENED);
  EXPECT_EQ(struck(Workshop{smithy, DAMAGED}), SHARPENED);
  // Ruined, it sharpens nothing.
  EXPECT_EQ(struck(Workshop{smithy, DESTROYED}), PLAIN);
  // Control: arming one of two residents, it sharpens nothing.
  Holding small = smithy;
  small.arms = 1;
  EXPECT_EQ(struck(Workshop{small}), PLAIN);
}

// The health of `workshop`'s first resident after `days` days.
std::uint64_t tended(const Workshop& workshop, std::uint64_t days) {
  World world = workshop_world(workshop);
  run_ticks(world, days);
  return world.person(world.roster(0).front()).health;
}

TEST(World, TheWoundedHealWhileAnApothecaryTendingThemStands) {
  constexpr std::uint64_t WOUNDED = 40;
  Holding apothecary;
  apothecary.tends = 2;
  apothecary.healing = DAILY_HEALING;
  // Each day it restores its healing, intact or damaged, up to a newborn's
  // health and no further.
  EXPECT_EQ(tended(Workshop{apothecary, INTACT, 2, WOUNDED}, 1),
            WOUNDED + DAILY_HEALING);
  EXPECT_EQ(tended(Workshop{apothecary, DAMAGED, 2, WOUNDED}, 2),
            WOUNDED + (std::uint64_t{2} * DAILY_HEALING));
  EXPECT_EQ(tended(Workshop{apothecary, INTACT, 2, WOUNDED}, 3),
            NEWBORN_HEALTH);
  // No one's health above a newborn's is lowered.
  EXPECT_EQ(tended(Workshop{apothecary, INTACT, 2, 2 * NEWBORN_HEALTH}, 1),
            2 * NEWBORN_HEALTH);
  // Ruined, it heals no one.
  EXPECT_EQ(tended(Workshop{apothecary, DESTROYED, 2, WOUNDED}, 1), WOUNDED);
  // Control: tending one of two residents, it heals no one.
  Holding small = apothecary;
  small.tends = 1;
  EXPECT_EQ(tended(Workshop{small, INTACT, 2, WOUNDED}, 1), WOUNDED);
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
// Each village's three structures, a house, a granary and a field; the side
// of a village's square, and the cells from its centre it builds within.
constexpr std::size_t STRUCTURES_AT_START = 6;
constexpr std::int32_t SQUARE_SIDE = 3;
constexpr std::int32_t VICINITY = 6;

// The growth tests' temple and inn plans.
constexpr std::size_t TEMPLE_PLAN = 5;
constexpr std::size_t INN_PLAN = 6;

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
  // Village B's residents.
  std::size_t neighbours = FOUNDERS;
  std::uint32_t self_reliance = std::numeric_limits<std::uint32_t>::max();
  // Whether village A's first resident, person 0, leads faction 0.
  bool led = false;
  // The least distance between villages' centres.
  std::int32_t apart = 0;
  // Residents reaching a second tier, with a square two cells wider; none
  // at 0.
  std::uint32_t second_tier = 0;
  // Residents reaching a third tier, after the second, with a square two
  // cells wider again and no square kinds of its own; none at 0.
  std::uint32_t third_tier = 0;
  // Whether the second tier names the temple and then the inn as its square
  // kinds.
  bool named = false;
  // Whether village A's first structure, its house, is of the temple's kind,
  // and its durability.
  bool temple_first = false;
  std::uint32_t first_durability = INTACT;
};

// Growth of seven plans, a granary and a house for four, homes, a field, an
// edge, a smithy and an apothecary each serving four, and a temple and an
// inn holding nothing, of the square; one
// tier with a 3 x 3 square, building within six cells; a core of one of the
// first three each; two settlers and two raiders.
Growth growth_of(const Growing& growing) {
  Growth growth;
  growth.interval = 1;
  growth.pressure = growing.pressure;
  growth.reach = growing.reach;
  growth.spacing = 1;
  growth.claim = 4;
  growth.apart = growing.apart;
  growth.tiers = {Tier{0, {SQUARE_SIDE, SQUARE_SIDE}, VICINITY, {}}};
  if (growing.second_tier > 0) {
    growth.tiers.push_back(
        Tier{growing.second_tier,
             {SQUARE_SIDE + 2, SQUARE_SIDE + 2},
             VICINITY,
             growing.named ? std::vector<std::size_t>{TEMPLE_PLAN, INN_PLAN}
                           : std::vector<std::size_t>{}});
  }
  if (growing.third_tier > 0) {
    growth.tiers.push_back(Tier{
        growing.third_tier, {SQUARE_SIDE + 4, SQUARE_SIDE + 4}, VICINITY, {}});
  }
  growth.plans = {
      Plan{"granary", {1, 1}, true, Holding{0, TEN_YEARS_FOOD, 0}, Role::HOME},
      Plan{"house", {2, 2}, true, Holding{HOUSE_FOR_FOUR, 0, 0}, Role::HOME},
      Plan{"field", {2, 2}, false, Holding{0, 0, TEN_YEARS_FOOD}, Role::EDGE}};
  Holding smithy;
  smithy.arms = HOUSE_FOR_FOUR;
  smithy.edge = HALF_AGAIN;
  Holding apothecary;
  apothecary.tends = HOUSE_FOR_FOUR;
  apothecary.healing = DAILY_HEALING;
  growth.plans.push_back(Plan{"smithy", {1, 1}, true, smithy, Role::SQUARE});
  growth.plans.push_back(
      Plan{"apothecary", {1, 1}, true, apothecary, Role::SQUARE});
  growth.plans.push_back(Plan{"temple", {1, 1}, true, {}, Role::SQUARE});
  growth.plans.push_back(Plan{"inn", {1, 1}, true, {}, Role::SQUARE});
  growth.self_reliance = growing.self_reliance;
  growth.builds = growing.builds;
  growth.settlement_kind = "hamlet";
  growth.core = {1, 0, 2};
  growth.settlers = 2;
  growth.raiders = 2;
  growth.stances = {growing.stance, {}};
  return growth;
}

// Village A of four residents housed for four, with ten years' food stored
// and harvested, so housing is its scarcest holding, keeping `clearings`
// clear, and village B of four, on open ground but for `rough` cells, which
// bodies cross and nothing is built on.
World growth_world(const Growing& growing, const std::vector<Cell>& rough = {},
                   const std::vector<Clearing>& clearings = {}) {
  const auto row = [](Cell from) {
    return std::vector<Structure>{
        Structure{from, INTACT, {1, 1}, true},
        Structure{{from.x + 1, from.y}, INTACT, {1, 1}, true},
        Structure{{from.x + 2, from.y}, INTACT, {1, 1}, false}};
  };
  constexpr Cell NEIGHBOUR_ROW{20, 20};
  std::vector<Structure> structures = row({1, 1});
  structures.front().durability = growing.first_durability;
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
  return World{
      Seed{3},
      Map{rows},
      {army(rows, GROWING_SEAT, GROWING_SEAT, growing.founders, 1, 0,
            {MEND, GUARD}),
       army(rows, NEIGHBOUR_SEAT, NEIGHBOUR_SEAT, growing.neighbours,
            growing.neighbour_health, 1, {IDLE})},
      structures,
      daily,
      {VillageSpec{{0, 1, 2},
                   0,
                   holdings,
                   0,
                   clearings,
                   growing.temple_first
                       ? std::vector<std::string>{"temple", "", ""}
                       : std::vector<std::string>{}},
       VillageSpec{{3, 4, STRUCTURES_AT_START - 1}, 1, holdings, 0, {}, {}}},
      growing.led ? std::vector<Leadership>{Leadership{
                        0, std::nullopt, std::nullopt, std::nullopt}}
                  : std::vector<Leadership>{},
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

TEST(World, AStructureLaidByGrowthTakesItsPlansKind) {
  World world = growth_world(Growing{{Expansion::BUILD}});
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"house"});
  // Control: a structure the world was created with was laid from no plan.
  EXPECT_FALSE(world.laid_kind(0).has_value());
  EXPECT_THROW(static_cast<void>(world.laid_kind(STRUCTURES_AT_START + 1)),
               std::out_of_range);
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

// Whether no cell of `area` from `corner` lies within a cell of `clearing`.
bool a_cell_clear_of(Cell corner, Footprint area, const Clearing& clearing) {
  return corner.x > clearing.corner.x + clearing.area.width ||
         clearing.corner.x > corner.x + area.width ||
         corner.y > clearing.corner.y + clearing.area.height ||
         clearing.corner.y > corner.y + area.height;
}

TEST(World, APressedVillageBuildsACellClearOfItsClearings) {
  // Control: with no clearing, the house is laid on the cells nearest the
  // centre.
  World open = growth_world(Growing{{Expansion::BUILD}});
  run_ticks(open, 1);
  ASSERT_EQ(open.structure_count(), STRUCTURES_AT_START + 1);
  const Structure nearest = open.structure(STRUCTURES_AT_START);
  // With those cells a clearing, as a seat's court is, it is laid elsewhere,
  // still beside the centre, a cell clear of the clearing as of a structure.
  const Clearing court{nearest.cell, nearest.footprint};
  World world = growth_world(Growing{{Expansion::BUILD}}, {}, {court});
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  const Structure built = world.structure(STRUCTURES_AT_START);
  EXPECT_TRUE(within_four(built.cell, GROWING_SEAT));
  EXPECT_TRUE(a_cell_clear_of(built.cell, built.footprint, court))
      << "(" << built.cell.x << ", " << built.cell.y << ")";
}

TEST(World, AVillagePastItsSelfRelianceBuildsASmithyThenAnApothecary) {
  constexpr std::size_t SMITHY = 3;
  constexpr std::size_t APOTHECARY = 4;
  constexpr std::size_t HOUSE = 1;
  constexpr std::uint64_t RAISING_TICKS = 50;
  Growing growing{{Expansion::BUILD}};
  growing.builds = {0, HOUSE, SMITHY, APOTHECARY};
  // Housing for four is its scarcest holding of the three that cap it, and
  // it arms and tends only three with no workshop.
  growing.self_reliance = FOUNDERS - 1;
  World world = growth_world(growing);
  ASSERT_EQ(world.cap(VILLAGE_A), FOUNDERS);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.plan_of(STRUCTURES_AT_START),
            std::optional<std::size_t>{SMITHY});
  EXPECT_EQ(world.stage(STRUCTURES_AT_START), Stage::RUBBLE);
  // Raised by repair, the smithy arms seven; tends, at three, is scarcest,
  // and the apothecary is laid next.
  run_ticks(world, RAISING_TICKS);
  EXPECT_EQ(world.stage(STRUCTURES_AT_START), Stage::INTACT);
  ASSERT_GT(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.plan_of(STRUCTURES_AT_START + 1),
            std::optional<std::size_t>{APOTHECARY});
  // Control: arming and tending four, as many as its housing holds, it lays
  // the house, housing coming first among equals.
  growing.self_reliance = FOUNDERS;
  World reliant = growth_world(growing);
  run_ticks(reliant, 1);
  ASSERT_EQ(reliant.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(reliant.plan_of(STRUCTURES_AT_START),
            std::optional<std::size_t>{HOUSE});
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
  // The land is the first tier's square round the new village's centre.
  EXPECT_EQ(laid_on.area.width, SQUARE_SIDE);
  EXPECT_EQ(laid_on.area.height, SQUARE_SIDE);
  const std::size_t settlers = world.army_count() - 1;
  const Cell centre = world.objective(settlers);
  EXPECT_EQ(laid_on.corner.x, centre.x - (SQUARE_SIDE / 2));
  EXPECT_EQ(laid_on.corner.y, centre.y - (SQUARE_SIDE / 2));
  // Each plan of the core is laid once, within the vicinity of the centre and
  // off its square.
  const Growth growth = growth_of(Growing{{Expansion::FOUND}});
  for (const std::size_t plan : growth.core) {
    std::size_t laid = 0;
    for (std::size_t structure = STRUCTURES_AT_START;
         structure < world.structure_count(); ++structure) {
      const Structure built = world.structure(structure);
      const bool on_square =
          built.cell.x < laid_on.corner.x + SQUARE_SIDE &&
          laid_on.corner.x < built.cell.x + built.footprint.width &&
          built.cell.y < laid_on.corner.y + SQUARE_SIDE &&
          laid_on.corner.y < built.cell.y + built.footprint.height;
      const bool near = std::abs(built.cell.x - centre.x) <= VICINITY &&
                        std::abs(built.cell.y - centre.y) <= VICINITY;
      if (world.plan_of(structure) == plan && !on_square && near) {
        ++laid;
      }
    }
    EXPECT_EQ(laid, 1U) << "plan " << plan;
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

// The squared distance between two cells.
std::int64_t apart_squared(Cell one, Cell other) {
  const std::int64_t across = std::int64_t{one.x} - other.x;
  const std::int64_t down = std::int64_t{one.y} - other.y;
  return (across * across) + (down * down);
}

TEST(World, NoVillageIsFoundedNearerThanApartToAnother) {
  // Sixteen cells apart, the village is founded at least sixteen cells from
  // both.
  constexpr std::int32_t APART = 16;
  Growing spaced{{Expansion::FOUND}};
  spaced.apart = APART;
  World world = growth_world(spaced);
  run_ticks(world, 1);
  ASSERT_EQ(world.village_count(), 3U);
  const Cell centre = world.objective(world.army_count() - 1);
  EXPECT_GE(apart_squared(centre, GROWING_SEAT), APART * APART);
  EXPECT_GE(apart_squared(centre, NEIGHBOUR_SEAT), APART * APART);
  // Control: at an apart of 0 the same village founds nearer than sixteen
  // cells to its own.
  World close = growth_world(Growing{{Expansion::FOUND}});
  run_ticks(close, 1);
  ASSERT_EQ(close.village_count(), 3U);
  EXPECT_LT(
      apart_squared(close.objective(close.army_count() - 1), GROWING_SEAT),
      APART * APART);
  // Forty-eight cells apart, beyond every site within its reach of 24 that
  // the 32-cell map holds, it founds nothing.
  Growing far{{Expansion::FOUND}};
  far.apart = 3 * APART;
  World nowhere = growth_world(far);
  run_ticks(nowhere, 1);
  EXPECT_EQ(nowhere.village_count(), 2U);
}

TEST(World, AVillageKeepsTheTierItReachedAsItsPeopleLeave) {
  // A second tier at four residents, which village A starts with.
  Growing growing{{Expansion::FOUND}};
  growing.second_tier = FOUNDERS;
  World world = growth_world(growing);
  EXPECT_EQ(world.tier(VILLAGE_A), 1U);
  // Two settle the village A founds, leaving it two, and it keeps its tier.
  run_ticks(world, 1 + SETTLING_TICKS);
  ASSERT_EQ(world.village_count(), 3U);
  ASSERT_EQ(world.residents_of(VILLAGE_A), FOUNDERS - 2);
  EXPECT_EQ(world.tier(VILLAGE_A), 1U);
  // Control: the founded village, as many as A now, has not reached it.
  ASSERT_EQ(world.residents_of(FOUNDED), FOUNDERS - 2);
  EXPECT_EQ(world.tier(FOUNDED), 0U);
}

// A world whose village A has reached a second tier naming the temple and
// the inn as its square kinds, the stance `stance`, and village B, of two,
// has not.
Growing square_kinds_of(std::vector<Expansion> stance) {
  Growing growing{std::move(stance)};
  growing.neighbours = FOUNDERS - 2;
  growing.second_tier = FOUNDERS;
  growing.named = true;
  return growing;
}

// Ticks `world` until structure `structure` stands intact, for at most 200
// ticks.
void raise(World& world, std::size_t structure) {
  constexpr std::uint64_t RAISING_LIMIT = 200;
  for (std::uint64_t tick = 0;
       tick < RAISING_LIMIT && world.stage(structure) != Stage::INTACT;
       ++tick) {
    world.tick();
  }
}

TEST(World, AVillageReachingATierLaysItsSquareKindsAheadOfItsBuilds) {
  // Pressed, with housing scarcest, it lays the temple first, as rubble, and
  // nothing else that check.
  World world = growth_world(square_kinds_of({Expansion::BUILD}));
  ASSERT_EQ(world.tier(VILLAGE_A), 1U);
  ASSERT_EQ(world.tier(VILLAGE_B), 0U);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"temple"});
  EXPECT_EQ(world.stage(STRUCTURES_AT_START), Stage::RUBBLE);
  // Raised by repair, it is followed by the inn, then by the house.
  raise(world, STRUCTURES_AT_START);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 2);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START + 1),
            std::optional<std::string>{"inn"});
  raise(world, STRUCTURES_AT_START + 1);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 3);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START + 2),
            std::optional<std::string>{"house"});
  // Control: a tier naming no square kinds lays the house first.
  Growing plain = square_kinds_of({Expansion::BUILD});
  plain.named = false;
  World unnamed = growth_world(plain);
  ASSERT_EQ(unnamed.tier(VILLAGE_A), 1U);
  run_ticks(unnamed, 1);
  ASSERT_EQ(unnamed.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(unnamed.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"house"});
}

TEST(World, ATownLaysTheSquareKindsOfTheVillageTierBelowIt) {
  // Village A, of four, reaches a third tier naming no kinds of its own; the
  // second, at three, names the temple and the inn. Village B, of two,
  // reaches neither.
  Growing town = square_kinds_of({});
  town.second_tier = FOUNDERS - 1;
  town.third_tier = FOUNDERS;
  World world = growth_world(town);
  ASSERT_EQ(world.tier(VILLAGE_A), 2U);
  ASSERT_EQ(world.tier(VILLAGE_B), 0U);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"temple"});
  raise(world, STRUCTURES_AT_START);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 2);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START + 1),
            std::optional<std::string>{"inn"});
  // Control: with the lower tier naming none, the town lays nothing.
  town.named = false;
  World unnamed = growth_world(town);
  ASSERT_EQ(unnamed.tier(VILLAGE_A), 2U);
  run_ticks(unnamed, 1);
  EXPECT_EQ(unnamed.structure_count(), STRUCTURES_AT_START);
}

TEST(World, AnUnpressedVillageWhoseStanceOnlyRaidsLaysItsSquareKinds) {
  // Three people under a cap of four, with no hostile neighbour to raid.
  Growing raiding = square_kinds_of({Expansion::RAID});
  raiding.hostile = false;
  raiding.founders = FOUNDERS - 1;
  raiding.second_tier = FOUNDERS - 1;
  World world = growth_world(raiding);
  ASSERT_EQ(world.tier(VILLAGE_A), 1U);
  ASSERT_EQ(world.tier(VILLAGE_B), 0U);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"temple"});
  raise(world, STRUCTURES_AT_START);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 2);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START + 1),
            std::optional<std::string>{"inn"});
  // Both stand, and it lays nothing more.
  raise(world, STRUCTURES_AT_START + 1);
  run_ticks(world, 1);
  EXPECT_EQ(world.structure_count(), STRUCTURES_AT_START + 2);
  // Control: a tier naming no square kinds lays nothing.
  raiding.named = false;
  World unnamed = growth_world(raiding);
  run_ticks(unnamed, 1);
  EXPECT_EQ(unnamed.structure_count(), STRUCTURES_AT_START);
}

TEST(World, ATempleInRubbleIsRaisedByRepairAndNotLaidAgain) {
  // Village A's first structure is a temple in rubble.
  Growing ruined = square_kinds_of({});
  ruined.temple_first = true;
  ruined.first_durability = 0;
  World world = growth_world(ruined);
  ASSERT_EQ(world.stage(0), Stage::RUBBLE);
  run_ticks(world, 1);
  EXPECT_EQ(world.structure_count(), STRUCTURES_AT_START);
  // Raised by repair, it is held, and the inn is laid.
  raise(world, 0);
  ASSERT_EQ(world.stage(0), Stage::INTACT);
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"inn"});
  // Control: the same structure of no kind leaves the temple to be laid.
  ruined.temple_first = false;
  World unheld = growth_world(ruined);
  raise(unheld, 0);
  ASSERT_EQ(unheld.stage(0), Stage::INTACT);
  run_ticks(unheld, 1);
  ASSERT_EQ(unheld.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(unheld.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"temple"});
}

// The column and row of `cell`.
std::pair<std::int32_t, std::int32_t> xy(Cell cell) { return {cell.x, cell.y}; }

// Clearings covering every cell of the growth tests' map but `free`.
std::vector<Clearing> all_but(Cell free) {
  constexpr std::int32_t SIDE = GROWTH_SIDE;
  return {Clearing{{0, 0}, {free.x, SIDE}},
          Clearing{{free.x + 1, 0}, {SIDE - free.x - 1, SIDE}},
          Clearing{{free.x, 0}, {1, free.y}},
          Clearing{{free.x, free.y + 1}, {1, SIDE - free.y - 1}}};
}

TEST(World, ASquareKindWithNoFreeCornerOnTheSquaresEdgeTakesTheNearest) {
  // The second tier's 5 x 5 square spans x 4 to 8 round the centre, and the
  // one free cell lies two cells off its edge, within the vicinity.
  constexpr Cell FREE{GROWING_SEAT.x + 5, GROWING_SEAT.y};
  World world = growth_world(square_kinds_of({}), {}, all_but(FREE));
  run_ticks(world, 1);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"temple"});
  EXPECT_EQ(xy(world.structure(STRUCTURES_AT_START).cell), xy(FREE));
  // Control: with the edge open, it touches the square.
  World open = growth_world(square_kinds_of({}));
  run_ticks(open, 1);
  ASSERT_EQ(open.structure_count(), STRUCTURES_AT_START + 1);
  const Cell laid = open.structure(STRUCTURES_AT_START).cell;
  constexpr std::int32_t REACH = 3;
  EXPECT_EQ(std::max(std::abs(laid.x - GROWING_SEAT.x),
                     std::abs(laid.y - GROWING_SEAT.y)),
            REACH);
}

// Ticks `world` until growth lays a structure, for at most 100 ticks.
void tick_until_laid(World& world) {
  constexpr std::uint64_t LAYING_LIMIT = 100;
  const std::size_t before = world.structure_count();
  for (std::uint64_t tick = 0;
       tick < LAYING_LIMIT && world.structure_count() == before; ++tick) {
    world.tick();
  }
}

TEST(World, ASquareKindWithNoSiteIsTriedAgainOnceOneFrees) {
  // The one free cell is the residents' objective, so no corner is free.
  constexpr Cell FREE{GROWING_SEAT.x + 5, GROWING_SEAT.y};
  World world = growth_world(square_kinds_of({}), {}, all_but(FREE));
  const std::size_t residents_a = 0;
  world.order(residents_a, Order{FREE, {0, 1}});
  constexpr std::uint64_t WAITING_TICKS = 5;
  run_ticks(world, WAITING_TICKS);
  EXPECT_EQ(world.structure_count(), STRUCTURES_AT_START);
  // Sent back to the centre, they free it, and the temple is laid there.
  world.order(residents_a, Order{GROWING_SEAT, {0, 1}});
  tick_until_laid(world);
  ASSERT_EQ(world.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(world.laid_kind(STRUCTURES_AT_START),
            std::optional<std::string>{"temple"});
  EXPECT_EQ(xy(world.structure(STRUCTURES_AT_START).cell), xy(FREE));
  // Control: with the cell free from the start, the temple is laid on the
  // first check.
  World free = growth_world(square_kinds_of({}), {}, all_but(FREE));
  run_ticks(free, 1);
  ASSERT_EQ(free.structure_count(), STRUCTURES_AT_START + 1);
  EXPECT_EQ(xy(free.structure(STRUCTURES_AT_START).cell), xy(FREE));
}

// A world of one village whose residents mend every tick, holding a rubble
// field of its own over a 2 x 2 patch of the map's `ground`, after one tick.
World mended_field(char ground) {
  std::vector<std::string> rows = open_map(GROWTH_SIDE);
  for (std::size_t row = 2; row < 4; ++row) {
    rows.at(row).replace(2, 2, 2, ground);
  }
  Life life = one_discipline(0);
  Growth growth = growth_of(Growing{});
  growth.terrace = 2 * PER_MILLE;
  growth.stances = {{}};
  life.growth = growth;
  Rules rules{SEPARATION, {{false}}, STAGE_FLOORS};
  rules.ticks_per_day = 1;
  World world{Seed{3},
              Map{rows},
              {army(rows, GROWING_SEAT, GROWING_SEAT, FOUNDERS, 1, 0, {MEND})},
              {Structure{{2, 2}, 0, {2, 2}, false, Role::EDGE}},
              rules,
              {VillageSpec{{0}, 0, {Holding{0, 0, TEN_YEARS_FOOD}}, 0, {}, {}}},
              {},
              life};
  run_ticks(world, 1);
  return world;
}

TEST(World, ATerracedFieldIsRaisedAtTheTerracesShareOfTheLabour) {
  // On hills, at a terrace of 2,000 per mille, the field takes half the
  // repair a field on flat ground takes.
  const std::uint32_t flat =
      mended_field(tile_of(Footing::BUILDABLE)).structure(0).durability;
  const std::uint32_t terraced =
      mended_field(tile_of(Footing::ROUGH)).structure(0).durability;
  ASSERT_GT(flat, 0U);
  EXPECT_EQ(terraced * 2, flat);
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

// Whether every person of `world` serves in the army whose roster holds it,
// of that army's faction, and a person on no roster in none.
bool serves_as_rostered(const World& world) {
  std::vector<std::optional<std::size_t>> rostered(world.population());
  for (std::size_t army = 0; army < world.army_count(); ++army) {
    for (const std::size_t member : world.roster(army)) {
      rostered.at(member) = army;
    }
  }
  for (std::size_t person = 0; person < rostered.size(); ++person) {
    const std::optional<std::size_t> army = rostered.at(person);
    const std::optional<std::size_t> faction =
        army ? std::optional<std::size_t>{world.faction(*army)} : std::nullopt;
    if (world.army_of(person) != army || world.faction_of(person) != faction) {
      return false;
    }
  }
  return true;
}

TEST(World,
     EveryPersonServesInTheArmyWhoseRosterHoldsItAsAPartyGoesAndComesHome) {
  // A party formed from village A's residents raids village B and is recalled
  // to them.
  Growing raiding{{Expansion::RAID}};
  raiding.neighbour_health = GUARD.body_damage;
  World world = growth_world(raiding);
  bool agreed = serves_as_rostered(world);
  constexpr std::size_t PARTY = 2;
  constexpr std::uint64_t MARCH_LIMIT = 2'000;
  for (std::uint64_t tick = 0;
       tick < MARCH_LIMIT &&
       (world.army_count() <= PARTY || !world.roster(VILLAGE_B).empty());
       ++tick) {
    world.tick();
    agreed = agreed && serves_as_rostered(world);
  }
  // Setup: the party was formed, the raid ended, and its members went home.
  ASSERT_EQ(world.army_count(), PARTY + 1);
  ASSERT_TRUE(world.roster(VILLAGE_B).empty());
  ASSERT_TRUE(world.roster(PARTY).empty());
  ASSERT_EQ(world.roster(VILLAGE_A).size(), FOUNDERS);
  EXPECT_TRUE(agreed);
}

TEST(World, EveryPersonServesInTheArmyWhoseRosterHoldsItAsTheHungryMove) {
  // Village 0's people, their field burnt, move to village 1 as they go
  // hungry.
  constexpr std::uint32_t STORED = 600;
  constexpr std::uint64_t DAYS = 200;
  Holdings starving = holding(FOR_TEN, STORED, STORED);
  starving.field_durability = BURNT;
  World world =
      economy_world(starving, holding(FOR_TEN, 2 * STORED, STORED), 1);
  bool agreed = serves_as_rostered(world);
  for (std::uint64_t tick = 0; tick < DAYS; ++tick) {
    world.tick();
    agreed = agreed && serves_as_rostered(world);
  }
  // Setup: all of village 0's people moved.
  ASSERT_EQ(world.residents_of(0), 0U);
  ASSERT_EQ(world.residents_of(1), FOUNDERS + 1);
  EXPECT_TRUE(agreed);
}

// Where the beset village's seat and its two structures stand, and how far
// its claim reaches. The second, NEARER, lies nearer the band than the first.
constexpr Cell BESET_SEAT{6, 16};
constexpr Cell OUTLYING{26, 16};
constexpr Cell NEARER{8, 26};
constexpr std::size_t NEARER_INDEX = 1;
constexpr std::int32_t BESET_CLAIM = 20;
constexpr std::int32_t BESET_SIGHT = 6;
constexpr std::size_t BESET_RESIDENTS = 0;
constexpr std::size_t BAND = 1;
// A torch that burns structures within two cells of its user.
constexpr Skill TORCH{0, 2 * CELL_SIZE, 0, 10, 1, 0, Shape::SELF};

// Faction 1's village, its structures 20 cells east and 10 cells south of its
// seat, where its four residents of `health` stand, idle; faction 0's band of
// three beside them, ordered on the seat with a strike and a torch, and at war
// with it. The village's claim reaches 20 cells, people see 6, and growth
// expands nothing.
World beset_far(std::uint64_t health) {
  const std::vector<std::string> rows = open_map(GROWTH_SIDE);
  Life life = one_discipline(0);
  Growth growth = growth_of(Growing{});
  growth.claim = BESET_CLAIM;
  life.growth = growth;
  Rules rules = at_war(SEPARATION);
  rules.ticks_per_day = 1;
  rules.sight = BESET_SIGHT;
  return World{
      Seed{3},
      Map{rows},
      {army(rows, BESET_SEAT, BESET_SEAT, FOUNDERS, health, 1, {IDLE}),
       army(rows, {BESET_SEAT.x + 2, BESET_SEAT.y}, BESET_SEAT, 3, INTACT, 0,
            {GUARD, TORCH})},
      {Structure{OUTLYING, INTACT, {1, 1}, true},
       Structure{NEARER, INTACT, {1, 1}, true}},
      rules,
      {VillageSpec{{0, NEARER_INDEX}, BESET_RESIDENTS, {{}, {}}, 0, {}, {}}},
      {},
      life};
}

TEST(World, ABandFightsAFoeInSightThenGoesForTheNearestStandingStructure) {
  constexpr std::uint64_t TICKS = 300;
  constexpr std::uint64_t STURDY = 1'000'000;
  World sturdy = beset_far(STURDY);
  run_ticks(sturdy, TICKS);
  // Setup: the residents still stand in the band's sight, and both
  // structures stand beyond its torch.
  ASSERT_EQ(sturdy.roster(BESET_RESIDENTS).size(), FOUNDERS);
  ASSERT_FALSE(sturdy.foes_in_sight(sturdy.roster(BAND).front()).empty());
  ASSERT_EQ(sturdy.stage(0), Stage::INTACT);
  ASSERT_EQ(sturdy.stage(NEARER_INDEX), Stage::INTACT);
  // The band fights on, its objective still the seat.
  EXPECT_TRUE(in(BESET_SEAT, centre_of(sturdy.objective(BAND))));
  // Control: residents who fall leave the band no foe in sight, and it goes
  // first for the standing structure nearest it, though another comes first
  // by index; once that falls, it goes on to the other.
  World frail = beset_far(1);
  std::uint64_t ticks = 0;
  for (; ticks < TICKS && in(BESET_SEAT, centre_of(frail.objective(BAND)));
       ++ticks) {
    frail.tick();
  }
  ASSERT_TRUE(frail.roster(BESET_RESIDENTS).empty());
  EXPECT_TRUE(within_four(frail.objective(BAND), NEARER));
  run_ticks(frail, TICKS);
  ASSERT_EQ(frail.stage(NEARER_INDEX), Stage::RUBBLE);
  EXPECT_TRUE(within_four(frail.objective(BAND), OUTLYING));
}

// The edge village's claim, and its two structures: one on its claim's east
// edge, whose cells beside it within the claim are blocked, and one 11 cells
// north of its seat.
constexpr std::int32_t EDGE_CLAIM = 10;
constexpr Cell ON_EDGE{BESET_SEAT.x + EDGE_CLAIM, BESET_SEAT.y};
constexpr Cell NORTH{BESET_SEAT.x, BESET_SEAT.y - EDGE_CLAIM - 1};

// beset_far's village and band with a claim of 10 cells and frail residents,
// its structures ON_EDGE and NORTH, every cell beside ON_EDGE within the claim
// blocked, so the cells beside it the band can stand on lie outside it.
World edge_beset() {
  std::vector<std::string> rows = open_map(GROWTH_SIDE);
  for (const Cell cell :
       {Cell{ON_EDGE.x - 1, ON_EDGE.y - 1}, Cell{ON_EDGE.x - 1, ON_EDGE.y},
        Cell{ON_EDGE.x - 1, ON_EDGE.y + 1}, Cell{ON_EDGE.x, ON_EDGE.y - 1},
        Cell{ON_EDGE.x, ON_EDGE.y + 1}}) {
    rows.at(static_cast<std::size_t>(cell.y))
        .at(static_cast<std::size_t>(cell.x)) = tile_of(Footing::BLOCKED);
  }
  Life life = one_discipline(0);
  Growth growth = growth_of(Growing{});
  growth.claim = EDGE_CLAIM;
  life.growth = growth;
  Rules rules = at_war(SEPARATION);
  rules.ticks_per_day = 1;
  rules.sight = BESET_SIGHT;
  return World{Seed{3},
               Map{rows},
               {army(rows, BESET_SEAT, BESET_SEAT, FOUNDERS, 1, 1, {IDLE}),
                army(rows, {BESET_SEAT.x + 2, BESET_SEAT.y}, BESET_SEAT, 3,
                     INTACT, 0, {GUARD, TORCH})},
               {Structure{ON_EDGE, INTACT, {1, 1}, true},
                Structure{NORTH, INTACT, {1, 1}, true}},
               rules,
               {VillageSpec{{0, 1}, BESET_RESIDENTS, {{}, {}}, 0, {}, {}}},
               {},
               life};
}

TEST(World, ABandRuinsAVillageWhoseStructureStandsOnItsClaimsEdge) {
  constexpr std::uint64_t TICKS = 600;
  World world = edge_beset();
  // Setup: the structure on the edge is the nearer the band, and the only
  // open cells beside it lie outside the claim.
  ASSERT_LT(std::abs(ON_EDGE.x - (BESET_SEAT.x + 2)),
            std::abs(NORTH.y - BESET_SEAT.y));
  std::uint64_t ticks = 0;
  for (; ticks < TICKS && world.village_state(0) != VillageState::RUINS;
       ++ticks) {
    world.tick();
  }
  // The band burns the edge structure from inside the claim and goes on to
  // the other, so the village falls.
  EXPECT_EQ(world.stage(0), Stage::RUBBLE);
  EXPECT_EQ(world.stage(1), Stage::RUBBLE);
  EXPECT_EQ(world.village_state(0), VillageState::RUINS);
}

// The straggled village's last structure, five cells east of its seat, and
// what it takes to bring down: as much as one torch burns in 2,000 ticks.
constexpr Cell LAST{BESET_SEAT.x + 5, BESET_SEAT.y};
constexpr std::uint32_t LAST_DURABILITY = 20'000;
constexpr std::size_t STRAGGLING_BAND = 6;
// A torch that burns structures within four cells of its user, so the band
// standing round the seat reaches LAST only from its east side.
constexpr std::int32_t WIDE_TORCH_RADIUS = 4 * CELL_SIZE;
constexpr Skill WIDE_TORCH{0, WIDE_TORCH_RADIUS, 0, 10, 1, 0, Shape::SELF};

// beset_far's village of frail residents with LAST alone standing, and a band
// of six round its seat, ordered on it with a strike and a wide torch.
World straggling() {
  const std::vector<std::string> rows = open_map(GROWTH_SIDE);
  Life life = one_discipline(0);
  Growth growth = growth_of(Growing{});
  growth.claim = BESET_CLAIM;
  life.growth = growth;
  Rules rules = at_war(SEPARATION);
  rules.ticks_per_day = 1;
  rules.sight = BESET_SIGHT;
  return World{Seed{3},
               Map{rows},
               {army(rows, BESET_SEAT, BESET_SEAT, FOUNDERS, 1, 1, {IDLE}),
                army(rows, BESET_SEAT, BESET_SEAT, STRAGGLING_BAND, INTACT, 0,
                     {GUARD, WIDE_TORCH})},
               {Structure{LAST, LAST_DURABILITY, {1, 1}, true}},
               rules,
               {VillageSpec{{0}, BESET_RESIDENTS, {{}}, 0, {}, {}}},
               {},
               life};
}

// Whether the wide torch from `from` reaches LAST.
bool torch_reaches_last(Position from) {
  const auto clamp = [](std::int32_t along, std::int32_t first) {
    return std::clamp(along, first * CELL_SIZE, ((first + 1) * CELL_SIZE) - 1);
  };
  const Position near{clamp(from.x, LAST.x), clamp(from.y, LAST.y)};
  return distance_squared(from, near) <=
         std::int64_t{WIDE_TORCH_RADIUS} * WIDE_TORCH_RADIUS;
}

TEST(World, ABandMovesOnThoughOneMemberReachesTheLastStructure) {
  // One torch alone takes 2,000 ticks; the band brought up takes far fewer.
  constexpr std::uint64_t LIMIT = 1'000;
  World world = straggling();
  bool straggled = false;
  std::uint64_t ticks = 0;
  for (; ticks < LIMIT && world.stage(0) != Stage::RUBBLE; ++ticks) {
    world.tick();
    // The state the band once held in: its foes gone, its objective the seat
    // and out of reach of LAST, and a member of it in reach.
    if (world.roster(BESET_RESIDENTS).empty() &&
        in(BESET_SEAT, centre_of(world.objective(BAND))) &&
        !torch_reaches_last(centre_of(world.objective(BAND)))) {
      const std::vector<Position> standing = positions(world, BAND);
      straggled = straggled || std::any_of(standing.begin(), standing.end(),
                                           torch_reaches_last);
    }
  }
  // Setup: a lone member reached LAST while the band's objective did not.
  ASSERT_TRUE(straggled);
  // The band is moved on, and LAST falls within the limit.
  EXPECT_EQ(world.stage(0), Stage::RUBBLE);
  EXPECT_LT(ticks, LIMIT);
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

TEST(World, ADeclaredPeaceStopsARaidAndADeclaredWarSendsOne) {
  Growing hostile{{Expansion::RAID}};
  hostile.led = true;
  World at_peace = growth_world(hostile);
  at_peace.declare(0, 1, Declaration::PEACE);
  run_ticks(at_peace, 1);
  EXPECT_EQ(at_peace.army_count(), 2U);
  // Control: the same village with no declaration raids (also above).
  EXPECT_EQ(grown_armies(hostile), 3U);

  Growing friendly{{Expansion::RAID}};
  friendly.led = true;
  friendly.hostile = false;
  EXPECT_EQ(grown_armies(friendly), 2U);
  World at_war = growth_world(friendly);
  at_war.declare(0, 1, Declaration::WAR);
  run_ticks(at_war, 1);
  EXPECT_EQ(at_war.army_count(), 3U);
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
  Growth unknown_core = valid;
  unknown_core.core = {valid.plans.size()};
  EXPECT_FALSE(grows_by(unknown_core));
  Growth untiered = valid;
  untiered.tiers.clear();
  EXPECT_FALSE(grows_by(untiered));
  Growth late_first = valid;
  late_first.tiers.front().residents = 1;
  EXPECT_FALSE(grows_by(late_first));
  Growth unordered = valid;
  unordered.tiers.push_back(Tier{0, {1, 1}, 1, {}});
  EXPECT_FALSE(grows_by(unordered));
  Growth squareless = valid;
  squareless.tiers.front().square = {0, 1};
  EXPECT_FALSE(grows_by(squareless));
  Growth effortless = valid;
  effortless.terrace = 0;
  EXPECT_FALSE(grows_by(effortless));
  // Control: a second tier past the first is accepted.
  Growth two_tiers = valid;
  two_tiers.tiers.push_back(Tier{2, {1, 1}, 1, {}});
  EXPECT_TRUE(grows_by(two_tiers));
  Growth too_many = valid;
  too_many.stances = {{}, {}, {}};
  EXPECT_FALSE(grows_by(too_many));
}

TEST(World, GrowthKeepingMoreThanAVillagesWindowHoldsIsRefused) {
  // A window holds 128 cells a side with 4 round what a village keeps: a
  // vicinity of 59 keeps 119 cells across its centre, and a square of 120
  // keeps 120.
  constexpr std::int32_t WIDEST_VICINITY = 59;
  constexpr std::int32_t WIDEST_SQUARE = 120;
  const Growth valid = growth_of(Growing{});
  // Controls: the widest a window holds are accepted.
  Growth widest_vicinity = valid;
  widest_vicinity.tiers.back().vicinity = WIDEST_VICINITY;
  EXPECT_TRUE(grows_by(widest_vicinity));
  Growth widest_square = valid;
  widest_square.tiers.back().square = {1, WIDEST_SQUARE};
  EXPECT_TRUE(grows_by(widest_square));
  Growth wider_vicinity = valid;
  wider_vicinity.tiers.back().vicinity = WIDEST_VICINITY + 1;
  EXPECT_FALSE(grows_by(wider_vicinity));
  Growth wider_square = valid;
  wider_square.tiers.back().square = {1, WIDEST_SQUARE + 1};
  EXPECT_FALSE(grows_by(wider_square));
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
               {VillageSpec{{}, 0, {}, households, {}, {}}},
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
  std::optional<Spread> lifespan;
  std::int32_t rumour_reach = 0;
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
  peace.rumour_reach = sectarian.rumour_reach;
  Life life = one_discipline(0);
  life.manuals = sectarian.manuals;
  life.lifespan = sectarian.lifespan;
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
      {VillageSpec{
           {0}, SEAT_ARMY, {Holding{0, 0, 0, sectarian.pupils}}, 0, {}, {}},
       VillageSpec{{1}, VILLAGE_ARMY, {Holding{}}, 0, {}, {}}},
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

TEST(World, AFactionReadsAsElectingOnlyWithAnElection) {
  Sectarian electing;
  electing.election = Election{0};
  EXPECT_TRUE(sect_world(electing).elects(0));
  // Control: the same faction without an election does not elect.
  EXPECT_FALSE(sect_world(Sectarian{}).elects(0));
}

TEST(World, OnlyAFactionWithADuelPassesItsLeadByDuel) {
  const World world = cult_world(TARGET_HEALTH);
  EXPECT_TRUE(world.passes_by_duel(CULT));
  // Control: its ally, led without a duel, and the Orthodox, led by no one.
  EXPECT_FALSE(world.passes_by_duel(ALLY));
  EXPECT_FALSE(world.passes_by_duel(ORTHODOX));
  EXPECT_THROW(static_cast<void>(world.passes_by_duel(99)), std::out_of_range);
}

constexpr std::size_t VILLAGER = 2;
constexpr std::size_t HOMELESS = 5;

TEST(World, AnApplicantJoinsAtOnceByTheAdmissionsThresholdsWithoutItsChance) {
  Sectarian unlikely;
  unlikely.admission.chance = 0;
  World world = sect_world(unlikely);
  ASSERT_EQ(world.application_refusal(VILLAGER, SECT),
            ApplicationRefusal::NONE);
  world.apply(VILLAGER, SECT);
  EXPECT_EQ(world.sect_of(VILLAGER), std::optional<std::size_t>{SECT});
  EXPECT_EQ(world.sect_members(SECT), (std::vector<std::size_t>{0, 1, 2}));
  // Control: with no chance, the daily admission takes no one in two years.
  EXPECT_EQ(sect_world_after(unlikely, SECT_YEARS).sect_members(SECT),
            (std::vector<std::size_t>{0, 1}));
}

// The world `sectarian` makes, after `days` days, in which person `person`
// applies to the sect.
struct Applying {
  Sectarian sectarian;
  std::size_t person = VILLAGER;
  std::uint64_t days = 0;
  bool fallen = false;
};

World application_world(const Applying& applying) {
  World world = applying.fallen ? fallen(applying.sectarian)
                                : sect_world(applying.sectarian);
  run_ticks(world, applying.days);
  return world;
}

// What person `applying.person` is told on applying in `applying`'s world:
// the refusal, whether apply() refused it, and the sect it belongs to after.
using Answer = std::tuple<ApplicationRefusal, bool, std::optional<std::size_t>>;

Answer answer(const Applying& applying) {
  World world = application_world(applying);
  const ApplicationRefusal refusal =
      world.application_refusal(applying.person, SECT);
  bool refused = false;
  try {
    world.apply(applying.person, SECT);
  } catch (const std::invalid_argument&) {
    refused = true;
  }
  return Answer{refusal, refused, world.sect_of(applying.person)};
}

// Whether an application to a sect the world lacks is refused.
bool applying_to_no_sect_is_refused() {
  const World world = application_world(Applying{});
  try {
    static_cast<void>(world.application_refusal(VILLAGER, SECT + 1));
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
}

TEST(World, AnApplicationIsRefusedForEachReasonApplyRefusesIt) {
  Sectarian mortal;
  mortal.lifespan = Spread{1, 0};
  Sectarian foreign;
  foreign.villager_faction = 1;
  Sectarian older;
  older.admission.age = 1;
  Sectarian untalented;
  untalented.admission.talent = MAX_TALENT;
  Sectarian unproven;
  unproven.admission.attainment = 1;
  Sectarian distant;
  distant.admission.reach = 4;
  Sectarian ruined;
  ruined.durability = 0;
  Sectarian narrow;
  narrow.pupils = 2;
  const std::vector<std::pair<Applying, ApplicationRefusal>> cases{
      {Applying{mortal, VILLAGER, 2}, ApplicationRefusal::DEAD},
      {Applying{Sectarian{}, 0}, ApplicationRefusal::IN_A_SECT},
      {Applying{Sectarian{}, HOMELESS}, ApplicationRefusal::NO_HOME},
      {Applying{foreign}, ApplicationRefusal::NOT_OF_FACTION},
      {Applying{older}, ApplicationRefusal::TOO_YOUNG},
      {Applying{untalented}, ApplicationRefusal::UNPROVEN},
      {Applying{unproven}, ApplicationRefusal::UNPROVEN},
      {Applying{distant}, ApplicationRefusal::TOO_FAR},
      {Applying{ruined}, ApplicationRefusal::SEAT_LOST},
      {Applying{Sectarian{}, VILLAGER, 0, true}, ApplicationRefusal::NO_ELDER},
      {Applying{narrow}, ApplicationRefusal::FULL},
  };
  std::vector<Answer> answers;
  std::vector<Answer> refusals;
  for (const auto& [applying, reason] : cases) {
    answers.push_back(answer(applying));
    const bool member = reason == ApplicationRefusal::IN_A_SECT;
    refusals.emplace_back(
        reason, true, member ? std::optional<std::size_t>{SECT} : std::nullopt);
  }
  EXPECT_EQ(answers, refusals);
  // Control: the same villager, with nothing barring it, is admitted.
  EXPECT_EQ(answer(Applying{}), Answer(ApplicationRefusal::NONE, false, SECT));
  EXPECT_TRUE(applying_to_no_sect_is_refused());
}

// The happenings of kind `Kind` among the events world `world` keeps, oldest
// first.
template <typename Kind>
std::vector<Kind> recorded(const World& world) {
  std::vector<Kind> found;
  for (const Event& event : world.events()) {
    if (const auto* what = std::get_if<Kind>(&event.what)) {
      found.push_back(*what);
    }
  }
  return found;
}

TEST(World, EveryDeathAndRebirthIsRecordedWhereAndWhenItHappened) {
  World world = soul_world();
  ASSERT_TRUE(world.events().empty());
  const std::size_t post = world.roster(POST).front();
  const std::size_t reborn = world.population();
  run_ticks(world, ARRIVAL_TICKS);
  ASSERT_EQ(world.person(post).health, 0U);
  std::size_t dead = 0;
  for (std::size_t person = 0; person < world.population(); ++person) {
    dead += world.person(person).health == 0 ? 1U : 0U;
  }
  EXPECT_EQ(recorded<Died>(world).size(), dead);
  const auto died = std::find_if(
      world.events().begin(), world.events().end(), [post](const Event& event) {
        const auto* what = std::get_if<Died>(&event.what);
        return what != nullptr && what->person == post;
      });
  ASSERT_NE(died, world.events().end());
  EXPECT_TRUE(in(died->cell, world.person(post).position));
  // The soul is born again on the tick its body died, after the death.
  const Event& born = world.events().back();
  ASSERT_TRUE(std::holds_alternative<Born>(born.what));
  EXPECT_EQ(std::get<Born>(born.what).person, reborn);
  EXPECT_EQ(born.tick, died->tick);
  EXPECT_GT(born.sequence, died->sequence);
  // Control: no one was born but the soul.
  EXPECT_EQ(recorded<Born>(world).size(), 1U);
}

TEST(World, ASectsFallIsRecordedOnceWhenItsLastMemberLeaves) {
  World world = fallen(Sectarian{});
  run_ticks(world, SECT_YEARS);
  const std::vector<SectFell> fell = recorded<SectFell>(world);
  ASSERT_EQ(fell.size(), 1U);
  EXPECT_EQ(fell.front().sect, SECT);
  EXPECT_EQ(fell.front().faction, 0U);
  // Control: with one founder left, the sect stands and nothing is recorded.
  World standing = sect_world(Sectarian{});
  standing.conscript(0, OUTSIDERS);
  run_ticks(standing, 1);
  ASSERT_TRUE(standing.sect_stands(SECT));
  EXPECT_TRUE(recorded<SectFell>(standing).empty());
}

TEST(World, ALeadChangingHandsIsRecordedAndARenewedOneIsNot) {
  // Elected each day, the same master is recorded once.
  Sectarian electing;
  electing.election = Election{1};
  const std::vector<LeaderChanged> elected =
      recorded<LeaderChanged>(sect_world_after(electing, 3));
  ASSERT_EQ(elected.size(), 1U);
  EXPECT_EQ(elected.front().leader, 0U);
  EXPECT_EQ(elected.front().by, ChosenBy::ELECTION);
  // A challenger who wins is recorded as winning the lead by duel.
  World won = cult_world(TARGET_HEALTH);
  ASSERT_EQ(after_challenge(won, PLAYER), std::optional<std::size_t>{1});
  const std::vector<LeaderChanged> duelled = recorded<LeaderChanged>(won);
  ASSERT_EQ(duelled.size(), 1U);
  EXPECT_EQ(duelled.front().faction, CULT);
  EXPECT_EQ(duelled.front().by, ChosenBy::DUEL);
  // Control: a lost duel changes no hands.
  World lost = cult_world(1);
  ASSERT_EQ(after_challenge(lost, PLAYER), std::optional<std::size_t>{0});
  EXPECT_TRUE(recorded<LeaderChanged>(lost).empty());
}

TEST(World, VillagesFoundedRaidedAndRuinedAreRecorded) {
  const std::vector<Founded> founding = recorded<Founded>(founded());
  ASSERT_EQ(founding.size(), 1U);
  EXPECT_EQ(founding.front().village, FOUNDED);
  EXPECT_EQ(founding.front().faction, 0U);
  const Growing raiding{{Expansion::RAID}};
  World raided = growth_world(raiding);
  run_ticks(raided, 1);
  const std::vector<Raided> raids = recorded<Raided>(raided);
  ASSERT_EQ(raids.size(), 1U);
  EXPECT_EQ(raids.front().village, VILLAGE_B);
  EXPECT_EQ(raids.front().raider, 0U);
  EXPECT_EQ(raids.front().owner, raided.owner(VILLAGE_B));
  World ruined = village_world(RESIDENT_HEALTH);
  ruined.order(RAIDER, {IN_VILLAGE, {0, 1}});
  run_ticks(ruined, PHASE_TICKS);
  ASSERT_EQ(ruined.village_state(0), VillageState::RUINS);
  const std::vector<Ruined> ruins = recorded<Ruined>(ruined);
  ASSERT_EQ(ruins.size(), 1U);
  EXPECT_EQ(ruins.front().destroyer.army, RAIDER);
  // Control: a village at peace raids no one, and none is founded or ruined.
  Growing peace{{Expansion::RAID}};
  peace.hostile = false;
  World quiet = growth_world(peace);
  run_ticks(quiet, 1);
  EXPECT_TRUE(recorded<Raided>(quiet).empty());
  EXPECT_TRUE(recorded<Founded>(quiet).empty());
  EXPECT_TRUE(recorded<Ruined>(quiet).empty());
}

constexpr Cell MIDDLE{8, 8};

// A world of days of one tick in which `people` people, who live a day, die.
World dying(std::size_t people) {
  Life life = one_discipline(0);
  life.lifespan = Spread{1, 0};
  Rules daily = peaceful(0);
  daily.ticks_per_day = 1;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army(MIDDLE, MIDDLE, people, 1, 0, {IDLE})},
              {},
              daily,
              {},
              {},
              life};
  run_ticks(world, 2);
  return world;
}

TEST(World, TheWorldKeepsItsLatestEventsUpToItsCapacity) {
  constexpr std::size_t PAST_CAPACITY = EVENT_CAPACITY + 6;
  const World world = dying(PAST_CAPACITY);
  ASSERT_EQ(world.living(), 0U);
  ASSERT_EQ(world.events().size(), EVENT_CAPACITY);
  EXPECT_EQ(world.events().front().sequence, 6U);
  EXPECT_EQ(world.events().back().sequence, PAST_CAPACITY - 1);
  // Control: fewer than its capacity are all kept, from the first.
  const World few = dying(EVENT_CAPACITY - 1);
  ASSERT_EQ(few.events().size(), EVENT_CAPACITY - 1);
  EXPECT_EQ(few.events().front().sequence, 0U);
}

// Whether a world whose rumours reach `reach` cells is created.
bool hears_within(std::int32_t reach) {
  Rules rules = peaceful(0);
  rules.rumour_reach = reach;
  try {
    const World world{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                      {},
                      rules};
    return world.living() == 1;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

TEST(World, ARumourReachOutsideItsLimitsIsRefused) {
  EXPECT_FALSE(hears_within(-1));
  EXPECT_FALSE(hears_within(MAX_REACH + 1));
  // Control: the limits themselves are accepted.
  EXPECT_TRUE(hears_within(0));
  EXPECT_TRUE(hears_within(MAX_REACH));
}

constexpr Cell FAR_CORNER{14, 14};

TEST(World, ARumourReachesThoseNearItItsFactionAndItsKinAndNoOneElse) {
  // A sect of faction 0 fell at its seat: its faction's villager hears of it
  // from afar, and faction 1's outsider, beyond the reach, does not.
  const World world = fallen(Sectarian{});
  ASSERT_EQ(recorded<SectFell>(world).size(), 1U);
  EXPECT_EQ(world.rumours(VILLAGER).size(), 1U);
  EXPECT_TRUE(world.rumours(HOMELESS).empty());
  // Control: within the reach, the outsider hears it too.
  Sectarian near;
  near.rumour_reach = OPEN_SIDE;
  EXPECT_EQ(fallen(near).rumours(HOMELESS).size(), 1U);

  // A death reaches the dead one's family from afar and no stranger of its
  // faction.
  const Skill once{2 * CELL_SIZE, 0, 1, 0, NEVER_AGAIN};
  World struck{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army({1, 1}, {1, 1}, 2, 1, 0, {IDLE}),
                army({1, 2}, {1, 2}, 1, 1, 1, {once}),
                army(FAR_CORNER, FAR_CORNER, 1, 1, 0, {IDLE})},
               {},
               at_war(0)};
  run_ticks(struck, 1);
  ASSERT_EQ(recorded<Died>(struck).size(), 1U);
  const std::size_t dead = recorded<Died>(struck).front().person;
  const std::size_t kin = dead == 0 ? 1U : 0U;
  ASSERT_GT(struck.person(kin).health, 0U);
  EXPECT_EQ(struck.rumours(kin).size(), 1U);
  EXPECT_TRUE(struck.rumours(3).empty());
}

// The home of the member of an army making village `home` its home, beside
// a village of one structure housing another army, or nothing when the
// world refuses it.
std::optional<std::optional<std::size_t>> homed(
    std::optional<std::size_t> home) {
  ArmySpec born = army(VILLAGE_HOME, VILLAGE_HOME, 1, 1, 0, {IDLE});
  born.home = home;
  try {
    const World world{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army(SEAT_HOME, SEAT_HOME, 1, 1, 0, {IDLE}), born},
                      {Structure{{SEAT_HOME.x, 0}, INTACT, {1, 1}, true}},
                      peaceful(0),
                      {VillageSpec{{0}, 0, {Holding{}}, 0, {}, {}}}};
    return world.person(1).home;
  } catch (const std::invalid_argument&) {
    return std::nullopt;
  }
}

TEST(World, AnArmyMakesTheVillageItNamesItsMembersHome) {
  EXPECT_EQ(homed(0), std::optional<std::optional<std::size_t>>{0});
  // A home that is no village is refused.
  EXPECT_FALSE(homed(1).has_value());
  // Control: an army naming no home leaves its members homeless.
  EXPECT_EQ(homed(std::nullopt),
            std::make_optional(std::optional<std::size_t>{}));
}

constexpr std::size_t CASTER = 0;
constexpr std::size_t NEAR_FOE = 1;
constexpr std::size_t FAR_FOE = 2;
constexpr std::size_t BEHIND_FOE = 3;
constexpr Cell CASTER_CELL{6, 8};
constexpr Cell NEAR_FOE_CELL{8, 8};
constexpr Cell FAR_FOE_CELL{10, 8};
constexpr Cell BEHIND_FOE_CELL{5, 8};
constexpr std::uint64_t CAST_COOLDOWN = 30;
constexpr std::uint64_t BOLT_DAMAGE = 10;
constexpr std::uint64_t NOVA_DAMAGE = 5;
// The caster's skills: IDLE, which its order strikes with, then these.
constexpr std::size_t BOLT = 1;
constexpr std::size_t SHORT_BOLT = 2;
constexpr std::size_t NOVA = 3;
constexpr std::size_t SKILLS = 4;
// A cell on the map's east edge, on the caster's row.
constexpr Cell EAST_EDGE{15, CASTER_CELL.y};

// A caster at CASTER_CELL whose order strikes with IDLE alone, so it uses
// nothing unless it casts, and three hostile people, each an army of its own
// with IDLE: two cells east, four cells east and one cell west.
World cast_world() {
  const Skill bolt{4 * CELL_SIZE, 0, BOLT_DAMAGE, 0, CAST_COOLDOWN};
  const Skill short_bolt{3 * CELL_SIZE, 0, BOLT_DAMAGE, 0, CAST_COOLDOWN};
  const Skill nova{0, (3 * CELL_SIZE) / 2, NOVA_DAMAGE, 0, CAST_COOLDOWN};
  World world{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(CASTER_CELL, CASTER_CELL, 1, TARGET_HEALTH, 0,
            {IDLE, bolt, short_bolt, nova}),
       army(NEAR_FOE_CELL, NEAR_FOE_CELL, 1, TARGET_HEALTH, 1, {IDLE}),
       army(FAR_FOE_CELL, FAR_FOE_CELL, 1, TARGET_HEALTH, 1, {IDLE}),
       army(BEHIND_FOE_CELL, BEHIND_FOE_CELL, 1, TARGET_HEALTH, 1, {IDLE})},
      {},
      at_war(0)};
  world.order(CASTER, {CASTER_CELL, {0}});
  return world;
}

// The health person `person` has lost from TARGET_HEALTH.
std::uint64_t lost(const World& world, std::size_t person) {
  return TARGET_HEALTH - world.person(person).health;
}

TEST(World, ACastAtAPersonLandsOnThatPersonInPlaceOfTheNearest) {
  World world = cast_world();
  world.cast(CASTER, BOLT, AtPerson{FAR_FOE});
  world.tick();

  EXPECT_EQ(lost(world, FAR_FOE), BOLT_DAMAGE);
  // Control: the nearer people, whom an automatic strike would aim at first,
  // are untouched.
  EXPECT_EQ(lost(world, NEAR_FOE), 0U);
  EXPECT_EQ(lost(world, BEHIND_FOE), 0U);
}

TEST(World, ACastAtAPointBeyondRangeLandsAtTheRangeTowardIt) {
  // Bolt reaches four cells: a point at the map's east edge is brought in to
  // FAR_FOE_CELL's centre, four cells east of the caster.
  World far = cast_world();
  far.cast(CASTER, BOLT, Point{centre_of(EAST_EDGE)});
  far.tick();
  EXPECT_EQ(lost(far, FAR_FOE), BOLT_DAMAGE);
  EXPECT_EQ(lost(far, NEAR_FOE), 0U);

  // Control: a bolt of three cells, aimed the same way, lands a cell short
  // and hits no one.
  World short_of = cast_world();
  short_of.cast(CASTER, SHORT_BOLT, Point{centre_of(EAST_EDGE)});
  short_of.tick();
  EXPECT_EQ(lost(short_of, FAR_FOE), 0U);
  EXPECT_EQ(lost(short_of, NEAR_FOE), 0U);
  // A cast that hits no one still starts its cooldown.
  EXPECT_EQ(short_of.person(CASTER).ready_at, 1 + CAST_COOLDOWN);
}

TEST(World, TickCountIsTheTickACooldownIsCountedAgainst) {
  World world = cast_world();
  EXPECT_EQ(world.tick_count(), 0U);
  world.cast(CASTER, BOLT, AtPerson{FAR_FOE});
  world.tick();

  EXPECT_EQ(world.tick_count(), 1U);
  EXPECT_EQ(world.person(CASTER).ready_at, world.tick_count() + CAST_COOLDOWN);
  // Control: a tick in which no one strikes still counts.
  world.tick();
  EXPECT_EQ(world.tick_count(), 2U);
  EXPECT_EQ(world.person(CASTER).ready_at, 1 + CAST_COOLDOWN);
}

TEST(World, ACastAtItselfCentresItsDiscOnTheCaster) {
  World world = cast_world();
  world.cast(CASTER, NOVA, Itself{});
  world.tick();

  // The disc of a cell and a half takes the person a cell behind, and spares
  // the caster, whom no one in it is hostile to.
  EXPECT_EQ(lost(world, BEHIND_FOE), NOVA_DAMAGE);
  EXPECT_EQ(lost(world, CASTER), 0U);
  // Control: the person two cells away lies outside it.
  EXPECT_EQ(lost(world, NEAR_FOE), 0U);
}

// A user at USER_CELL and five hostile people around it, each an army of
// its own with IDLE, in this order: a cell east, a cell north-east, a cell
// west, three cells east and five cells east. The user's order strikes with
// `skill`, so it uses it on its own once ready, and never again after.
constexpr Cell USER_CELL{6, 8};
constexpr std::array<Cell, 5> AROUND{{{7, 8}, {7, 7}, {5, 8}, {9, 8}, {11, 8}}};
constexpr std::size_t BY_EAST = 1;
constexpr std::size_t BY_NORTH_EAST = 2;
constexpr std::size_t BY_WEST = 3;
constexpr std::size_t BY_THREE_EAST = 4;
constexpr std::size_t BY_FIVE_EAST = 5;
constexpr std::uint64_t SHAPE_DAMAGE = 3;
constexpr std::int32_t ARM = 3 * CELL_SIZE / 2;

World shape_world(Skill skill) {
  skill.body_damage = SHAPE_DAMAGE;
  skill.cooldown = NEVER_AGAIN;
  std::vector<ArmySpec> armies{
      army(USER_CELL, USER_CELL, 1, TARGET_HEALTH, 0, {skill})};
  for (const Cell cell : AROUND) {
    armies.push_back(army(cell, cell, 1, TARGET_HEALTH, 1, {IDLE}));
  }
  return World{Seed{1}, Map{open_map(OPEN_SIDE)}, armies, {}, at_war(0)};
}

// The people of shape_world() hit, by index, after its first tick.
std::vector<std::size_t> hit_by(World world) {
  world.tick();
  std::vector<std::size_t> hit;
  for (std::size_t person = 1; person <= AROUND.size(); ++person) {
    if (lost(world, person) == SHAPE_DAMAGE) {
      hit.push_back(person);
    }
  }
  return hit;
}

using Hit = std::vector<std::size_t>;

TEST(World, AnArcSweepsEveryFoeAheadWithinArmsLengthAndNoneBehind) {
  // It faces the nearest foe, east, the first in roster order of the two a
  // cell away, and sweeps the one north-east too, a diagonal cell away.
  EXPECT_EQ(hit_by(shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC})),
            (Hit{BY_EAST, BY_NORTH_EAST}));
  // Control: an area of no radius at the same range hits only the one it
  // aims at.
  EXPECT_EQ(hit_by(shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::AREA})),
            (Hit{BY_EAST}));
  // Cast toward the west, it sweeps the one there alone.
  World west = shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC});
  west.cast(0, 0, Point{centre_of({0, USER_CELL.y})});
  EXPECT_EQ(hit_by(west), (Hit{BY_WEST}));
  // Cast at itself, it sweeps all around.
  World around = shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC});
  around.cast(0, 0, Itself{});
  EXPECT_EQ(hit_by(around), (Hit{BY_EAST, BY_NORTH_EAST, BY_WEST}));
}

TEST(World, AnArcSweepsAsFarAsItsHalfWidthEitherSideAndNoFurther) {
  // Facing the foe east, the one straight north lies at the half-width and is
  // swept; the one north-west lies half as far again past it and is spared.
  constexpr std::int32_t QUARTER_TURN_DEGREES = 90;
  static_assert(ARC_HALF_WIDTH_DEGREES == QUARTER_TURN_DEGREES,
                "the foes stand a quarter turn and three eighths of a turn "
                "from the facing");
  const Cell north{USER_CELL.x, USER_CELL.y - 1};
  const Cell north_west{USER_CELL.x - 1, USER_CELL.y - 1};
  World world{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(USER_CELL, USER_CELL, 1, TARGET_HEALTH, 0,
            {Skill{ARM, 0, SHAPE_DAMAGE, 0, NEVER_AGAIN, 0, Shape::ARC}}),
       army(AROUND.front(), AROUND.front(), 1, TARGET_HEALTH, 1, {IDLE}),
       army(north, north, 1, TARGET_HEALTH, 1, {IDLE}),
       army(north_west, north_west, 1, TARGET_HEALTH, 1, {IDLE})},
      {},
      at_war(0)};
  world.tick();
  EXPECT_EQ(lost(world, 1), SHAPE_DAMAGE);
  EXPECT_EQ(lost(world, 2), SHAPE_DAMAGE);
  EXPECT_EQ(lost(world, 3), 0U);
}

TEST(World, ALineHitsEveryFoeAlongItWithinItsLength) {
  constexpr std::int32_t HALF_A_BODY = CELL_SIZE / 2;
  EXPECT_EQ(hit_by(shape_world(
                Skill{6 * CELL_SIZE, HALF_A_BODY, 0, 0, 0, 0, Shape::LINE})),
            (Hit{BY_EAST, BY_THREE_EAST, BY_FIVE_EAST}));
  // Control: four cells long, it stops short of the one five cells east.
  EXPECT_EQ(hit_by(shape_world(
                Skill{4 * CELL_SIZE, HALF_A_BODY, 0, 0, 0, 0, Shape::LINE})),
            (Hit{BY_EAST, BY_THREE_EAST}));
  // Its far end is rounded: a foe a cell aside, five cells along, lies
  // within a cell of a line 4.3 cells long only were it longer, and is
  // spared; a line five cells long takes it in.
  const auto aside = [](std::int32_t range) {
    const Cell beyond{USER_CELL.x + 5, USER_CELL.y + 1};
    World world{
        Seed{1},
        Map{open_map(OPEN_SIDE)},
        {army(USER_CELL, USER_CELL, 1, TARGET_HEALTH, 0,
              {Skill{range, CELL_SIZE, SHAPE_DAMAGE, 0, NEVER_AGAIN, 0,
                     Shape::LINE}}),
         army(AROUND.front(), AROUND.front(), 1, TARGET_HEALTH, 1, {IDLE}),
         army(beyond, beyond, 1, TARGET_HEALTH, 1, {IDLE})},
        {},
        at_war(0)};
    world.tick();
    return lost(world, 2);
  };
  constexpr std::int32_t SHORT_OF_IT = (43 * CELL_SIZE) / 10;
  EXPECT_EQ(aside(SHORT_OF_IT), 0U);
  EXPECT_EQ(aside(5 * CELL_SIZE), SHAPE_DAMAGE);
}

// The durability each of `structures` keeps after a user at USER_CELL uses
// `skill` once toward a lone foe a cell north-east of it.
std::vector<std::uint32_t> standing_after(Skill skill,
                                          const std::vector<Cell>& structures) {
  constexpr std::uint32_t DURABLE = 100;
  skill.structure_damage = 1;
  skill.cooldown = NEVER_AGAIN;
  std::vector<Structure> built;
  built.reserve(structures.size());
  for (const Cell cell : structures) {
    built.push_back(Structure{cell, DURABLE});
  }
  const Cell foe = AROUND.at(BY_NORTH_EAST - 1);
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army(USER_CELL, USER_CELL, 1, TARGET_HEALTH, 0, {skill}),
               army(foe, foe, 1, TARGET_HEALTH, 1, {IDLE})},
              built,
              at_war(0)};
  world.tick();
  std::vector<std::uint32_t> left;
  left.reserve(built.size());
  for (std::size_t structure = 0; structure < built.size(); ++structure) {
    left.push_back(world.structure(structure).durability);
  }
  return left;
}

TEST(World, AnArcAndALineReachTheStructuresTheyCover) {
  using Left = std::vector<std::uint32_t>;
  // The arc reaches the barrel east, ahead of the foe's way, and not the one
  // west, behind it.
  EXPECT_EQ(
      standing_after(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC}, {{7, 8}, {5, 8}}),
      (Left{99, 100}));
  // The line north-east reaches the barrel on its way four cells out, and
  // not one as far east off it.
  EXPECT_EQ(standing_after(
                Skill{5 * CELL_SIZE, CELL_SIZE / 2, 0, 0, 0, 0, Shape::LINE},
                {{9, 5}, {10, 8}}),
            (Left{99, 100}));
}

TEST(World, ASelfSkillLandsAroundItsUserWhereverItIsCast) {
  const Skill self{3 * CELL_SIZE, ARM, 0, 0, 0, 0, Shape::SELF};
  World cast = shape_world(self);
  cast.cast(0, 0, Point{centre_of(AROUND.at(BY_THREE_EAST - 1))});
  EXPECT_EQ(hit_by(cast), (Hit{BY_EAST, BY_NORTH_EAST, BY_WEST}));
  // Control: an area cast at the same point lands there.
  Skill area = self;
  area.shape = Shape::AREA;
  World there = shape_world(area);
  there.cast(0, 0, Point{centre_of(AROUND.at(BY_THREE_EAST - 1))});
  EXPECT_EQ(hit_by(there), (Hit{BY_THREE_EAST}));
}

TEST(World, ATargetedSkillHitsThePersonItLocksOnAlone) {
  const Skill locked{3 * CELL_SIZE, ARM, 0, 0, 0, 0, Shape::TARGETED};
  World cast = shape_world(locked);
  cast.cast(0, 0, AtPerson{BY_EAST});
  EXPECT_EQ(hit_by(cast), (Hit{BY_EAST}));
  EXPECT_EQ(cast.cast_refusal(0, 0, Point{centre_of(AROUND.front())}),
            CastRefusal::NO_TARGET);
  EXPECT_EQ(cast.cast_refusal(0, 0, Itself{}), CastRefusal::NO_TARGET);
  // Control: an area of the same radius cast at the same person also hits
  // the one beside it, and may be cast at a point.
  Skill area = locked;
  area.shape = Shape::AREA;
  World disc = shape_world(area);
  EXPECT_EQ(disc.cast_refusal(0, 0, Point{centre_of(AROUND.front())}),
            CastRefusal::NONE);
  disc.cast(0, 0, AtPerson{BY_EAST});
  EXPECT_EQ(hit_by(disc), (Hit{BY_EAST, BY_NORTH_EAST}));
}

// Whether the people at `first` and `second`, a user at USER_CELL casting
// `skill` at `target`, lose health; `ally` makes the first of its faction.
std::pair<bool, bool> struck(Skill skill, const Target& target, Cell first,
                             Cell second, bool ally) {
  skill.body_damage = SHAPE_DAMAGE;
  skill.cooldown = NEVER_AGAIN;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army(USER_CELL, USER_CELL, 1, TARGET_HEALTH, 0, {IDLE, skill}),
               army(first, first, 1, TARGET_HEALTH, ally ? 0 : 1, {IDLE}),
               army(second, second, 1, TARGET_HEALTH, 1, {IDLE})},
              {},
              at_war(0)};
  world.cast(0, 1, target);
  world.tick();
  return {lost(world, 1) > 0, lost(world, 2) > 0};
}

// A use of `skill` at `target` that reaches the people at `first` and
// `second`.
struct Reaching {
  Skill skill;
  Target target;
  Cell first;
  Cell second;
};

// Checks that `use` spares the first person as an ally, while it strikes the
// same person as a foe and, for a shape reaching past the one it aims at, the
// foe beside it either way.
void expect_spares_the_ally(const Reaching& use) {
  const int shape = static_cast<int>(use.skill.shape);
  const auto [ally_hit, foe_hit] =
      struck(use.skill, use.target, use.first, use.second, true);
  EXPECT_FALSE(ally_hit) << "shape " << shape;
  // Control: with both hostile, the same use strikes the first, so the ally
  // stood where the shape reaches.
  const auto [first_hit, second_hit] =
      struck(use.skill, use.target, use.first, use.second, false);
  EXPECT_TRUE(first_hit) << "shape " << shape;
  if (use.skill.shape != Shape::TARGETED) {
    EXPECT_TRUE(foe_hit) << "shape " << shape;
    EXPECT_TRUE(second_hit) << "shape " << shape;
  }
}

TEST(World, NoShapeStrikesAnAlly) {
  const Cell east = AROUND.at(0);
  const Cell north_east = AROUND.at(1);
  const Cell three_east = AROUND.at(3);
  const Point at_east{centre_of(east)};
  const std::int32_t wide = 3 * CELL_SIZE / 2;
  const std::int32_t far_cells = 6;
  const std::int32_t far = far_cells * CELL_SIZE;
  expect_spares_the_ally(
      {Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC}, at_east, east, north_east});
  expect_spares_the_ally(
      {Skill{far, CELL_SIZE / 2, 0, 0, 0, 0, Shape::LINE},
       Point{centre_of({USER_CELL.x + far_cells, USER_CELL.y})}, east,
       three_east});
  expect_spares_the_ally(
      {Skill{ARM, wide, 0, 0, 0, 0, Shape::AREA}, at_east, east, north_east});
  expect_spares_the_ally(
      {Skill{0, wide, 0, 0, 0, 0, Shape::SELF}, Itself{}, east, north_east});
  expect_spares_the_ally({Skill{ARM, 0, 0, 0, 0, 0, Shape::TARGETED},
                          AtPerson{1}, east, north_east});
}

TEST(World, EachUseIsRecordedWithItsShapeReachOriginDirectionAndStruck) {
  World world = shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC});
  EXPECT_TRUE(world.strikes().empty());
  world.tick();
  // Every foe uses IDLE on itself each tick; the user's sweep comes first.
  ASSERT_EQ(world.strikes().size(), 1 + AROUND.size());
  const Strike& strike = world.strikes().front();
  EXPECT_EQ(strike.person, 0U);
  EXPECT_EQ(strike.skill, 0U);
  EXPECT_EQ(strike.shape, Shape::ARC);
  EXPECT_EQ(strike.range, ARM);
  EXPECT_EQ(strike.radius, 0);
  EXPECT_EQ(strike.body_damage, SHAPE_DAMAGE);
  EXPECT_EQ(strike.origin.x, centre_of(USER_CELL).x);
  EXPECT_EQ(strike.origin.y, centre_of(USER_CELL).y);
  EXPECT_EQ(strike.toward.x, centre_of(AROUND.front()).x);
  EXPECT_EQ(strike.toward.y, centre_of(AROUND.front()).y);
  EXPECT_EQ(strike.end.x, strike.toward.x);
  EXPECT_EQ(strike.end.y, strike.toward.y);
  Hit struck = strike.struck;
  std::sort(struck.begin(), struck.end());
  EXPECT_EQ(struck, (Hit{BY_EAST, BY_NORTH_EAST}));
  EXPECT_EQ(world.strikes().back().shape, Shape::AREA);
  EXPECT_EQ(world.strikes().back().body_damage, 0U);
  EXPECT_TRUE(world.strikes().back().struck.empty());
  // A line's record ends its range along the way it was used, past the foe
  // it faced a cell away.
  constexpr std::int32_t LENGTH = 6 * CELL_SIZE;
  World line = shape_world(Skill{LENGTH, 0, 0, 0, 0, 0, Shape::LINE});
  line.tick();
  EXPECT_EQ(line.strikes().front().end.x, centre_of(USER_CELL).x + LENGTH);
  EXPECT_EQ(line.strikes().front().end.y, centre_of(USER_CELL).y);
  // Control: on the next tick, cooling down, it strikes nothing, and the
  // record holds the foes' uses alone.
  world.tick();
  ASSERT_EQ(world.strikes().size(), AROUND.size());
  EXPECT_NE(world.strikes().front().person, 0U);
}

TEST(World, ASkillsShapeChangesTheHash) {
  const World arc = shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC});
  EXPECT_NE(arc.hash(),
            shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::LINE}).hash());
  // Control: the same shape hashes the same.
  EXPECT_EQ(arc.hash(),
            shape_world(Skill{ARM, 0, 0, 0, 0, 0, Shape::ARC}).hash());
}

// Two hostile people on row 8, each standing on its own objective, `apart`
// cells apart, each sweeping an arc at arm's length, in a world whose people
// close on a foe within SIGHT cells.
constexpr std::int32_t SIGHT = 6;
constexpr Cell STAND{3, 8};

World closing_world(std::int32_t apart) {
  const Skill arm{ARM, 0, 1, 0, 1, 0, Shape::ARC};
  const Cell other{STAND.x + apart, STAND.y};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army(STAND, STAND, 1, TARGET_HEALTH, 0, {arm}),
                army(other, other, 1, TARGET_HEALTH, 1, {arm})},
               {},
               rules};
}

constexpr std::uint64_t CLOSING_TICKS = 90;

// The ticks, up to the last of CLOSING_TICKS, the two people of `world` have
// stood within arm's length of each other without a break.
std::uint64_t ticks_within_reach(World& world) {
  std::uint64_t within = 0;
  for (std::uint64_t tick = 0; tick < CLOSING_TICKS; ++tick) {
    world.tick();
    const bool reached =
        distance_squared(world.person(0).position, world.person(1).position) <=
        std::int64_t{ARM} * ARM;
    within = reached ? within + 1 : 0;
  }
  return within;
}

TEST(World, APersonClosesOnAFoeInSightAndStandsToFightIt) {
  World world = closing_world(SIGHT - 1);
  // Once within reach, the two stay there rather than walk back to their
  // objectives.
  EXPECT_GT(ticks_within_reach(world), CLOSING_TICKS / 2);
  EXPECT_GT(lost(world, 0), 0U);
  EXPECT_GT(lost(world, 1), 0U);
  // Control: a foe a cell beyond sight is left alone, each keeping its
  // objective, unhurt.
  World apart = closing_world(SIGHT + 1);
  run_ticks(apart, CLOSING_TICKS);
  EXPECT_TRUE(in(STAND, apart.person(0).position));
  EXPECT_TRUE(in(Cell{STAND.x + SIGHT + 1, STAND.y}, apart.person(1).position));
  EXPECT_EQ(lost(apart, 0), 0U);
  EXPECT_EQ(lost(apart, 1), 0U);
}

// Eight attackers of faction 0 around RING_START, and one foe of faction 1
// standing at RING_FOE that strikes no one, on `rows`; the attackers close on
// it within SIGHT cells and strike it at arm's length for nothing. The rules
// set `tokens` when given, and the foe's army is listed first when
// `foe_first`.
constexpr Cell RING_FOE{8, 8};
constexpr Cell RING_START{6, 9};
constexpr std::size_t RING_ATTACKERS = 8;
constexpr std::uint64_t RING_TICKS = 300;

World ringed(const std::vector<std::string>& rows,
             const std::optional<Tokens>& tokens, bool foe_first) {
  const Skill jab{ARM, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  rules.tokens = tokens;
  const ArmySpec attackers = army(rows, RING_START, RING_START, RING_ATTACKERS,
                                  TARGET_HEALTH, 0, {jab});
  const ArmySpec foe =
      army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {IDLE});
  std::vector<ArmySpec> armies{attackers, foe};
  if (foe_first) {
    std::swap(armies.front(), armies.back());
  }
  return World{Seed{1}, Map{rows}, armies, {}, rules};
}

// ADR 0068's default table, but granting every one of RING_ATTACKERS a
// token, so all of them strike and stand on attack slots.
Tokens all_strike() {
  Tokens tokens = default_tokens();
  tokens.by_rank = {static_cast<std::uint32_t>(RING_ATTACKERS)};
  return tokens;
}

// The attacking army of a world ringed() made.
std::size_t attackers_of(bool foe_first) { return foe_first ? 1 : 0; }

// Where each of `world`'s attackers' slots lies, in roster order, after
// RING_TICKS ticks; nothing for one holding no slot.
std::vector<std::optional<Position>> seats(World& world, bool foe_first) {
  run_ticks(world, RING_TICKS);
  const std::size_t foe = world.roster(attackers_of(!foe_first)).front();
  const Position centre = world.person(foe).position;
  const SlotRing ring = slot_ring(ARM - RING_INSET, SEPARATION);
  std::vector<std::optional<Position>> seated;
  for (const std::size_t member : world.roster(attackers_of(foe_first))) {
    const std::optional<Engagement> held = world.attacks().engagement(member);
    if (!held || held->foe != foe || held->ring != Ring::NEAR || !held->slot) {
      seated.emplace_back();
      continue;
    }
    const Position offset = slot_offset(ring, *held->slot);
    seated.emplace_back(Position{centre.x + offset.x, centre.y + offset.y});
  }
  return seated;
}

TEST(World, AttackersStandOnDistinctSlotsAroundTheirFoe) {
  World world = ringed(open_map(OPEN_SIDE), all_strike(), false);
  const std::vector<std::optional<Position>> seated = seats(world, false);
  // Setup: the foe stood its ground, and its ring has a slot for each.
  ASSERT_TRUE(in(RING_FOE, world.person(world.roster(1).front()).position));
  ASSERT_EQ(slot_ring(ARM - RING_INSET, SEPARATION).count, RING_ATTACKERS);
  std::vector<Position> taken;
  for (std::size_t rank = 0; rank < seated.size(); ++rank) {
    ASSERT_TRUE(seated.at(rank).has_value()) << "attacker " << rank;
    const Position seat = seated.at(rank).value_or(Position{});
    EXPECT_LE(
        distance_squared(world.person(world.roster(0).at(rank)).position, seat),
        std::int64_t{ON_SLOT} * ON_SLOT)
        << "attacker " << rank;
    taken.push_back(seat);
  }
  // Eight slots, each a separation or more from the next.
  EXPECT_GE(closest_pair_squared(taken), std::int64_t{SEPARATION} * SEPARATION);
  // Control: with no attack tokens in the rules nobody takes a slot, and they
  // close straight on, inside the separation of one another.
  World straight = ringed(open_map(OPEN_SIDE), std::nullopt, false);
  const std::vector<std::optional<Position>> unseated = seats(straight, false);
  EXPECT_TRUE(std::none_of(
      unseated.begin(), unseated.end(),
      [](const std::optional<Position>& seat) { return seat.has_value(); }));
  EXPECT_LT(closest_pair_squared(positions(straight, 0)),
            std::int64_t{SEPARATION} * SEPARATION);
}

// Two people closing on attack slots round one foe pass through each other
// (ADR 0080, amending ADR 0020's give-way between them), but once both stand
// on their slots no two overlap, within half the separation, for longer than
// half a second. Standing within a quarter cell of slots the separation
// apart, two may rest a little inside the separation itself.
constexpr std::uint64_t OVERLAP_TICKS = TICKS_PER_SECOND / 2;
constexpr std::uint64_t RINGING_TICKS = 1'000;

// Whether `member` of `world` stands within a quarter cell of its slot of
// `ring` round `foe`.
bool on_its_slot(const World& world, std::size_t member, std::size_t foe,
                 const SlotRing& ring) {
  const std::optional<Engagement> held = world.attacks().engagement(member);
  if (!held || !held->slot) {
    return false;
  }
  const Position centre = world.person(foe).position;
  const Position offset = slot_offset(ring, *held->slot);
  const Position seat{centre.x + offset.x, centre.y + offset.y};
  return distance_squared(world.person(member).position, seat) <=
         std::int64_t{ON_SLOT} * ON_SLOT;
}

// The longest runs of ticks two of `people` people overlapped: at any time,
// and once both had stood on their slots.
class Overlaps {
 public:
  explicit Overlaps(std::size_t people)
      : passing_(people * people, 0), settled_(people * people, 0) {}

  // Counts one tick of pair `pair`, overlapping or not, both having arrived
  // on their slots or not.
  void count(std::size_t pair, bool inside, bool arrived) {
    std::uint64_t& run = passing_.at(pair);
    run = inside ? run + 1 : 0;
    longest_passing_ = std::max(longest_passing_, run);
    std::uint64_t& since = settled_.at(pair);
    since = inside && arrived ? since + 1 : 0;
    longest_settled_ = std::max(longest_settled_, since);
  }

  [[nodiscard]] std::uint64_t longest_passing() const {
    return longest_passing_;
  }
  [[nodiscard]] std::uint64_t longest_settled() const {
    return longest_settled_;
  }

 private:
  std::vector<std::uint64_t> passing_;
  std::vector<std::uint64_t> settled_;
  std::uint64_t longest_passing_ = 0;
  std::uint64_t longest_settled_ = 0;
};

TEST(World, AttackersOnTheirSlotsStayOverlappedNoLongerThanHalfASecond) {
  World world = ringed(open_map(OPEN_SIDE), all_strike(), false);
  const std::vector<std::size_t> roster = world.roster(0);
  const std::size_t foe = world.roster(1).front();
  const SlotRing ring = slot_ring(ARM - RING_INSET, SEPARATION);
  const std::int64_t apart = std::int64_t{SEPARATION / 2} * (SEPARATION / 2);
  const std::size_t people = roster.size();
  std::vector<bool> arrived(people, false);
  Overlaps overlaps{people};
  for (std::uint64_t tick = 0; tick < RINGING_TICKS; ++tick) {
    world.tick();
    for (std::size_t rank = 0; rank < people; ++rank) {
      arrived.at(rank) =
          arrived.at(rank) || on_its_slot(world, roster.at(rank), foe, ring);
    }
    for (std::size_t pair = 0; pair < people * people; ++pair) {
      const std::size_t one = pair / people;
      const std::size_t other = pair % people;
      const bool inside =
          one < other &&
          distance_squared(world.person(roster.at(one)).position,
                           world.person(roster.at(other)).position) < apart;
      overlaps.count(pair, inside, arrived.at(one) && arrived.at(other));
    }
  }
  // Setup: every attacker reached its slot.
  ASSERT_EQ(std::count(arrived.begin(), arrived.end(), false), 0);
  EXPECT_LE(overlaps.longest_settled(), OVERLAP_TICKS);
  // Control: on the way to their slots, people do pass through each other,
  // so the measure sees an overlap where there is one.
  EXPECT_GT(overlaps.longest_passing(), OVERLAP_TICKS);
}

// Two people of faction 0 on row 8, one at cell 10 with an attack intent on
// a foe at cell 3 and one at cell 5 on a foe at cell 12, so each walks along
// the row toward the other, seeking a slot round a different foe. They reach
// as far as LONG_REACH, so both are within their foes' waiting reach where
// their paths cross.
constexpr std::int32_t CROSSING_ROW = 8;
constexpr std::int32_t CROSSING_SIGHT = 10;

World seekers_crossing() {
  const Skill jab{LONG_REACH, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = CROSSING_SIGHT;
  rules.tokens = default_tokens();
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const Cell westward{10, CROSSING_ROW};
  const Cell eastward{5, CROSSING_ROW};
  const Cell west_foe{3, CROSSING_ROW};
  const Cell east_foe{12, CROSSING_ROW};
  World world{Seed{1},
              Map{rows},
              {army(rows, westward, westward, 1, TARGET_HEALTH, 0, {jab}),
               army(rows, eastward, eastward, 1, TARGET_HEALTH, 0, {jab}),
               army(rows, west_foe, west_foe, 1, TARGET_HEALTH, 1, {IDLE}),
               army(rows, east_foe, east_foe, 1, TARGET_HEALTH, 1, {IDLE})},
              {},
              rules};
  world.order(0, Order{westward, {0}, world.roster(2).front()});
  world.order(1, Order{eastward, {0}, world.roster(3).front()});
  return world;
}

TEST(World, PeopleSeekingSlotsRoundDifferentFoesKeepTheSeparation) {
  World world = seekers_crossing();
  const std::size_t westward = world.roster(0).front();
  const std::size_t eastward = world.roster(1).front();
  std::int64_t closest = std::numeric_limits<std::int64_t>::max();
  bool both_seeking = false;
  for (std::uint64_t tick = 0; tick < RING_TICKS; ++tick) {
    world.tick();
    const std::optional<Engagement> one = world.attacks().engagement(westward);
    const std::optional<Engagement> other =
        world.attacks().engagement(eastward);
    both_seeking = both_seeking || (one && other && one->foe != other->foe);
    closest =
        std::min(closest, distance_squared(world.person(westward).position,
                                           world.person(eastward).position));
  }
  // Setup: both held slots round their own foes on the way.
  ASSERT_TRUE(both_seeking);
  EXPECT_GE(closest, std::int64_t{SEPARATION / 2} * (SEPARATION / 2));
}

TEST(World, AttackersTakeTheSameSlotsWhicheverArmyIsListedFirst) {
  World attackers_first = ringed(open_map(OPEN_SIDE), all_strike(), false);
  World foe_first = ringed(open_map(OPEN_SIDE), all_strike(), true);
  const std::vector<std::optional<Position>> first =
      seats(attackers_first, false);
  // Setup: every attacker holds a slot.
  ASSERT_EQ(std::count(first.begin(), first.end(), std::nullopt), 0);
  const std::vector<std::optional<Position>> second = seats(foe_first, true);
  ASSERT_EQ(first.size(), second.size());
  for (std::size_t rank = 0; rank < first.size(); ++rank) {
    const Position one = first.at(rank).value_or(Position{});
    const Position other = second.at(rank).value_or(Position{-1, -1});
    EXPECT_EQ(one.x, other.x) << "attacker " << rank;
    EXPECT_EQ(one.y, other.y) << "attacker " << rank;
  }
}

// One attacker of faction 0 at RING_START closing on one foe of faction 1 at
// RING_FOE, striking it at arm's length for 1; with `guarded`, eight people
// of faction 2, hostile to no one, stand in the cells round the foe and crowd
// every slot of its ring.
constexpr std::size_t NEUTRAL = 2;

World guarded_foe(bool guarded) {
  const Skill jab{ARM, 0, 1, 0, 1, 0, Shape::TARGETED};
  Rules rules{
      SEPARATION,
      {{false, true, false}, {true, false, false}, {false, false, false}},
      STAGE_FLOORS};
  rules.sight = SIGHT;
  rules.tokens = default_tokens();
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  std::vector<ArmySpec> armies{
      army(rows, RING_START, RING_START, 1, TARGET_HEALTH, 0, {jab}),
      army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {IDLE})};
  if (guarded) {
    std::vector<Recruit> guards;
    for (const Cell cell :
         cells_of(Cell{RING_FOE.x - 1, RING_FOE.y - 1}, Footprint{3, 3})) {
      if (cell.x != RING_FOE.x || cell.y != RING_FOE.y) {
        guards.push_back(Recruit{cell, TARGET_HEALTH});
      }
    }
    armies.push_back(ArmySpec{RING_FOE, guards, NEUTRAL, {IDLE}});
  }
  return World{Seed{1}, Map{rows}, armies, {}, rules};
}

TEST(World, APersonOfferedNoSlotClosesOnTheFoeAndFightsItInReach) {
  World world = guarded_foe(true);
  const std::size_t attacker = world.roster(0).front();
  const std::size_t foe = world.roster(1).front();
  run_ticks(world, RING_TICKS);
  // Setup: the guards still stand round the foe.
  ASSERT_EQ(world.roster(NEUTRAL).size(), 8U);
  const Engagement held =
      world.attacks().engagement(attacker).value_or(Engagement{});
  EXPECT_TRUE(held.token && !held.slot);
  EXPECT_LE(distance_squared(world.person(attacker).position,
                             world.person(foe).position),
            std::int64_t{ARM} * ARM);
  EXPECT_LT(world.person(foe).health, TARGET_HEALTH);
  // Control: with nobody round the foe, the attacker takes a slot.
  World open = guarded_foe(false);
  run_ticks(open, RING_TICKS);
  EXPECT_TRUE(open.attacks()
                  .engagement(open.roster(0).front())
                  .value_or(Engagement{})
                  .slot.has_value());
}

// The most of `world`'s army `army` that strike on any one tick over
// RING_TICKS ticks.
std::size_t most_striking(World& world, std::size_t army) {
  std::size_t most = 0;
  for (std::uint64_t tick = 0; tick < RING_TICKS; ++tick) {
    const std::vector<std::size_t> roster = world.roster(army);
    world.tick();
    const std::vector<Strike>& strikes = world.strikes();
    most = std::max(
        most, static_cast<std::size_t>(std::count_if(
                  strikes.begin(), strikes.end(), [&](const Strike& strike) {
                    return std::find(roster.begin(), roster.end(),
                                     strike.person) != roster.end();
                  })));
  }
  return most;
}

TEST(World, AtMostTheFoesTokensStrikeItOnAnyTick) {
  // The foe is of no realm, so it grants the default table's first row.
  constexpr std::size_t GRANTED = 4;
  ASSERT_EQ(default_tokens().by_rank.front(), GRANTED);
  World world = ringed(open_map(OPEN_SIDE), default_tokens(), false);
  EXPECT_EQ(most_striking(world, 0), GRANTED);
  // Control: granting all eight a token, all eight strike on one tick.
  World all = ringed(open_map(OPEN_SIDE), all_strike(), false);
  EXPECT_EQ(most_striking(all, 0), RING_ATTACKERS);
}

// Four attackers of faction 0 crowding RING_FOE from the cell south of it,
// and a foe of faction 1 there, of no realm, granting `granted` tokens and
// striking no one.
World pressed(std::uint32_t granted) {
  const Skill jab{ARM, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  Tokens tokens = default_tokens();
  tokens.by_rank = {granted};
  rules.tokens = tokens;
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const Cell south{RING_FOE.x, RING_FOE.y + 1};
  return World{Seed{1},
               Map{rows},
               {army(rows, south, south, 4, TARGET_HEALTH, 0, {jab}),
                army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {IDLE})},
               {},
               rules};
}

TEST(World, OneHoldingNoTokenStrikesNotThoughTheFoeIsInItsReach) {
  constexpr std::uint32_t ONE = 1;
  World world = pressed(ONE);
  // Setup: more than one attacker stands within reach of the foe at the
  // start.
  const Position foe = world.person(world.roster(1).front()).position;
  const std::vector<Position> attackers = positions(world, 0);
  ASSERT_GT(std::count_if(attackers.begin(), attackers.end(),
                          [&](Position place) {
                            return distance_squared(place, foe) <=
                                   std::int64_t{ARM} * ARM;
                          }),
            1);
  EXPECT_EQ(most_striking(world, 0), ONE);
  // Control: granting each a token, more than one strikes on one tick.
  constexpr std::uint32_t EACH = 4;
  World each = pressed(EACH);
  EXPECT_GT(most_striking(each, 0), ONE);
}

TEST(World, ThePlayerAsAFoeIsCappedByItsOwnTokens) {
  constexpr std::size_t GRANTED = 4;
  World world = ringed(open_map(OPEN_SIDE), default_tokens(), false);
  world.steer(1, true);
  EXPECT_EQ(most_striking(world, 0), GRANTED);
  // Control: granting all eight a token, all eight strike the player.
  World all = ringed(open_map(OPEN_SIDE), all_strike(), false);
  all.steer(1, true);
  EXPECT_EQ(most_striking(all, 0), RING_ATTACKERS);
}

// As many attackers of faction 0 beside RING_FOE as the default table's
// first row grants, a person of faction 0 four cells further off with an
// attack intent on the foe, steered when `steered`, and the foe, of faction
// 1, of no realm, striking no one.
World latecomer(bool steered) {
  const Skill jab{ARM, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  rules.tokens = default_tokens();
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const Cell late{RING_FOE.x - 6, RING_FOE.y};
  World world{Seed{1},
              Map{rows},
              {army(rows, RING_START, RING_START,
                    default_tokens().by_rank.front(), TARGET_HEALTH, 0, {jab}),
               army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {IDLE}),
               army(rows, late, late, 1, TARGET_HEALTH, 0, {jab})},
              {},
              rules};
  world.order(2, Order{late, {0}, world.roster(1).front()});
  world.steer(2, steered);
  return world;
}

// What the latecomer of `world` engaging holds on the tick it first engages,
// and what each NPC of army 0 held the tick before.
struct Arrival {
  std::optional<Engagement> late;
  std::vector<std::optional<Engagement>> holding;
};

Arrival arrive(World& world) {
  const std::size_t late = world.roster(2).front();
  const std::vector<std::size_t> npcs = world.roster(0);
  Arrival arrival{std::nullopt,
                  std::vector<std::optional<Engagement>>(npcs.size())};
  for (std::uint64_t tick = 0; tick < RING_TICKS && !arrival.late; ++tick) {
    for (std::size_t rank = 0; rank < npcs.size(); ++rank) {
      arrival.holding.at(rank) = world.attacks().engagement(npcs.at(rank));
    }
    world.tick();
    arrival.late = world.attacks().engagement(late);
  }
  return arrival;
}

// The rank among `npcs` of the one that took its token last by `holding`, the
// higher person among equals.
std::size_t last_to_take(
    const std::vector<std::size_t>& npcs,
    const std::vector<std::optional<Engagement>>& holding) {
  const auto taken = [&](std::size_t rank) {
    return std::tuple{holding.at(rank).value_or(Engagement{}).token_since,
                      npcs.at(rank)};
  };
  std::size_t last = 0;
  for (std::size_t rank = 1; rank < npcs.size(); ++rank) {
    if (taken(rank) > taken(last)) {
      last = rank;
    }
  }
  return last;
}

TEST(World, TheSteeredTakesATokenTheNPCsHoldFromTheLastToTakeOne) {
  World world = latecomer(true);
  const Arrival steered = arrive(world);
  // Setup: every NPC held one of the foe's tokens as the steered person
  // arrived.
  ASSERT_TRUE(std::all_of(steered.holding.begin(), steered.holding.end(),
                          [](const std::optional<Engagement>& held) {
                            return held && held->token;
                          }));
  ASSERT_TRUE(steered.late.has_value());
  EXPECT_TRUE(steered.late.value_or(Engagement{}).token);
  // The NPC that took its token last, the higher among equals, gave it up,
  // and the others kept theirs.
  const std::vector<std::size_t> npcs = world.roster(0);
  const std::size_t last = last_to_take(npcs, steered.holding);
  std::vector<bool> kept;
  std::vector<bool> expected;
  for (std::size_t rank = 0; rank < npcs.size(); ++rank) {
    kept.push_back(
        world.attacks().engagement(npcs.at(rank)).value_or(Engagement{}).token);
    expected.push_back(rank != last);
  }
  EXPECT_EQ(kept, expected);
  // And the steered person strikes the foe.
  EXPECT_GT(most_striking(world, 2), 0U);
  // Control: an NPC arriving instead waits.
  World npc = latecomer(false);
  const Engagement waiting = arrive(npc).late.value_or(Engagement{});
  EXPECT_TRUE(!waiting.token && waiting.ring == Ring::WAITING);
}

// One attacker of faction 0 at RING_START with an attack intent on the first
// of two foes of faction 1 beside RING_FOE, steered, so it engages only the
// target of its intent.
World intent_on_foe() {
  const Skill jab{ARM, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  rules.tokens = default_tokens();
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const Cell other{RING_FOE.x, RING_FOE.y + 3};
  World world{Seed{1},
              Map{rows},
              {army(rows, RING_START, RING_START, 1, TARGET_HEALTH, 0, {jab}),
               army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {IDLE}),
               army(rows, other, other, 1, TARGET_HEALTH, 1, {IDLE})},
              {},
              rules};
  world.steer(0, true);
  world.order(0, Order{RING_START, {0}, world.roster(1).front()});
  run_ticks(world, RING_TICKS);
  return world;
}

TEST(World, ARetargetOrAnEndedIntentFreesTheToken) {
  World retargeted = intent_on_foe();
  const std::size_t attacker = retargeted.roster(0).front();
  const std::size_t first = retargeted.roster(1).front();
  const std::size_t second = retargeted.roster(2).front();
  // Setup: the attacker holds the first foe's token.
  ASSERT_TRUE(
      retargeted.attacks().engagement(attacker).value_or(Engagement{}).token);
  ASSERT_EQ(retargeted.attacks().tokens_held(first), 1U);
  retargeted.order(0, Order{RING_START, {0}, second});
  retargeted.tick();
  EXPECT_EQ(retargeted.attacks().tokens_held(first), 0U);
  EXPECT_EQ(
      retargeted.attacks().engagement(attacker).value_or(Engagement{}).foe,
      second);
  World ended = intent_on_foe();
  ended.order(0, Order{RING_START, {0}});
  ended.tick();
  EXPECT_EQ(ended.attacks().tokens_held(first), 0U);
  EXPECT_FALSE(ended.attacks().engagement(attacker).has_value());
  // Control: the same order kept, the token stays held.
  World kept = intent_on_foe();
  kept.tick();
  EXPECT_EQ(kept.attacks().tokens_held(first), 1U);
}

// Four attackers of faction 0 at RING_START, of `health` each, around a foe
// of faction 1 that strikes them with `skill`, every person granting one
// token.
World struck_back(const Skill& skill, std::uint64_t health = TARGET_HEALTH) {
  const Skill jab{ARM, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  Tokens one = default_tokens();
  one.by_rank = {1};
  rules.tokens = one;
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  return World{Seed{1},
               Map{rows},
               {army(rows, RING_START, RING_START, 4, health, 0, {jab}),
                army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {skill})},
               {},
               rules};
}

// Whether over RING_TICKS ticks anyone of `world`'s army 0 lost health on a
// tick it began waiting on the waiting ring of army 1's foe, other than the
// one that foe engages.
bool a_waiter_was_hit(World& world) {
  const std::size_t foe = world.roster(1).front();
  bool hit = false;
  for (std::uint64_t tick = 0; tick < RING_TICKS; ++tick) {
    std::vector<std::pair<std::size_t, std::uint64_t>> waiting;
    const std::optional<Engagement> target = world.attacks().engagement(foe);
    for (const std::size_t member : world.roster(0)) {
      if (world.attacks().engagement(member).value_or(Engagement{}).ring ==
              Ring::WAITING &&
          (!target || target->foe != member)) {
        waiting.emplace_back(member, world.person(member).health);
      }
    }
    world.tick();
    for (const auto& [member, health] : waiting) {
      hit = hit || world.person(member).health < health;
    }
  }
  return hit;
}

TEST(World, AnAreaUseStillHitsAWaiterInItsShape) {
  constexpr std::int32_t WIDE = 6 * CELL_SIZE;
  const Skill blast{ARM, WIDE, 1, 0, TICKS_PER_SECOND, 0, Shape::AREA};
  World world = struck_back(blast);
  EXPECT_TRUE(a_waiter_was_hit(world));
  // Control: a strike aimed at its token's holder alone hits no waiter.
  const Skill jab{ARM, 0, 1, 0, TICKS_PER_SECOND, 0, Shape::TARGETED};
  World targeted = struck_back(jab);
  EXPECT_FALSE(a_waiter_was_hit(targeted));
}

// The last of `world`'s army 0 holding a token of `foe`, if any does.
std::optional<std::size_t> token_holder(const World& world, std::size_t foe) {
  std::optional<std::size_t> holder;
  for (const std::size_t member : world.roster(0)) {
    const std::optional<Engagement> held = world.attacks().engagement(member);
    if (held && held->foe == foe && held->token) {
      holder = member;
    }
  }
  return holder;
}

// Whether `person` strikes within RING_TICKS ticks of `world`.
bool strikes_soon(World& world, std::size_t person) {
  bool struck = false;
  for (std::uint64_t tick = 0; tick < RING_TICKS && !struck; ++tick) {
    world.tick();
    for (const Strike& strike : world.strikes()) {
      struck = struck || strike.person == person;
    }
  }
  return struck;
}

TEST(World, AHoldersDeathGivesTheHeadOfTheQueueItsTokenWithinATick) {
  // The foe kills whoever it strikes, every second.
  constexpr std::uint64_t FRAIL = 10;
  const Skill kill{ARM, 0, FRAIL, 0, TICKS_PER_SECOND, 0, Shape::TARGETED};
  World world = struck_back(kill, FRAIL);
  const std::size_t foe = world.roster(1).front();
  std::optional<std::size_t> taker;
  bool refilled = false;
  for (std::uint64_t tick = 0; tick < RING_TICKS && !taker; ++tick) {
    const std::optional<std::size_t> holder = token_holder(world, foe);
    world.tick();
    if (holder && world.person(*holder).health == 0) {
      refilled = world.attacks().tokens_held(foe) == 1;
      taker = token_holder(world, foe);
    }
  }
  // Setup: a holder died while others waited.
  ASSERT_TRUE(taker.has_value());
  EXPECT_TRUE(refilled);
  // The new holder strikes the foe once in reach.
  EXPECT_TRUE(strikes_soon(world, taker.value_or(0)));
}

// `holders` people of faction 0 at RING_START beside a foe of faction 1, of
// no realm, at RING_FOE, a second foe four cells east of it, and a newcomer of
// faction 0 to the north standing where both foes are in sight, the first
// nearer.
World two_foes(std::size_t holders) {
  const Skill jab{ARM, 0, 0, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  rules.tokens = default_tokens();
  const std::vector<std::string> rows = open_map(OPEN_SIDE);
  const Cell second{RING_FOE.x + 4, RING_FOE.y};
  const Cell north{RING_FOE.x + 1, RING_FOE.y - 5};
  return World{
      Seed{1},
      Map{rows},
      {army(rows, RING_START, RING_START, holders, TARGET_HEALTH, 0, {jab}),
       army(rows, RING_FOE, RING_FOE, 1, TARGET_HEALTH, 1, {IDLE}),
       army(rows, second, second, 1, TARGET_HEALTH, 1, {IDLE}),
       army(rows, north, north, 1, TARGET_HEALTH, 0, {jab})},
      {},
      rules};
}

TEST(World, ANewcomerTurnsToAFoeWithAFreeTokenBeforeANearerOneWithout) {
  const std::uint32_t granted = default_tokens().by_rank.front();
  World full = two_foes(granted);
  const std::size_t near = full.roster(1).front();
  const std::size_t far = full.roster(2).front();
  const std::size_t newcomer = full.roster(3).front();
  // Setup: the first foe is nearer the newcomer.
  ASSERT_LT(distance_squared(full.person(newcomer).position,
                             full.person(near).position),
            distance_squared(full.person(newcomer).position,
                             full.person(far).position));
  run_ticks(full, RING_TICKS);
  // Setup: the holders hold all the first foe's tokens.
  ASSERT_EQ(full.attacks().tokens_held(near), granted);
  EXPECT_EQ(full.attacks().engagement(newcomer).value_or(Engagement{}).foe,
            far);
  // Control: with a token of the nearer foe free, it engages that one.
  World free = two_foes(granted - 1);
  run_ticks(free, RING_TICKS);
  EXPECT_EQ(free.attacks()
                .engagement(free.roster(3).front())
                .value_or(Engagement{})
                .foe,
            free.roster(1).front());
}

TEST(World, NoSlotLiesOnABlockedCell) {
  // A wall down the column east of the foe.
  std::vector<std::string> walled = open_map(OPEN_SIDE);
  for (std::string& row : walled) {
    row.at(static_cast<std::size_t>(RING_FOE.x) + 1) = '#';
  }
  const Map wall{walled};
  World world = ringed(walled, all_strike(), false);
  const std::vector<std::optional<Position>> seated = seats(world, false);
  // Setup: some attacker holds a slot.
  ASSERT_TRUE(std::any_of(
      seated.begin(), seated.end(),
      [](const std::optional<Position>& seat) { return seat.has_value(); }));
  for (const std::optional<Position>& seat : seated) {
    if (seat) {
      EXPECT_TRUE(wall.passable(cell_of(*seat)));
    }
  }
  // Control: on open ground a slot is taken on a cell the wall blocks.
  World open = ringed(open_map(OPEN_SIDE), all_strike(), false);
  const std::vector<std::optional<Position>> unwalled = seats(open, false);
  EXPECT_TRUE(std::any_of(unwalled.begin(), unwalled.end(),
                          [&wall](const std::optional<Position>& seat) {
                            return seat && !wall.passable(cell_of(*seat));
                          }));
}

// A steered chaser at STAND striking one foe at arm's length, a foe of
// INTENT_FOE_HEALTH standing `apart` cells east of it, bound for `foe_bound`,
// an ally two cells east and two north, and a foe beyond sight to the east;
// people close on a foe within SIGHT cells. The map is `rows`.
constexpr std::size_t CHASER = 0;
constexpr std::size_t INTENT_FOE = 1;
constexpr std::size_t INTENT_ALLY = 2;
constexpr std::size_t UNSEEN_FOE = 3;
constexpr std::uint64_t INTENT_FOE_HEALTH = 30;
constexpr std::uint64_t INTENT_DAMAGE = 10;

World intent_world(std::int32_t apart, Cell foe_bound,
                   const std::vector<std::string>& rows) {
  const Skill strike{ARM, 0, INTENT_DAMAGE, 0, 1, 0, Shape::TARGETED};
  const Cell foe{STAND.x + apart, STAND.y};
  const Cell ally{STAND.x + 2, STAND.y - 2};
  const Cell far{STAND.x + SIGHT + 4, STAND.y + 4};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  World world{Seed{1},
              Map{rows},
              {army(rows, STAND, STAND, 1, TARGET_HEALTH, 0, {strike}),
               army(rows, foe, foe_bound, 1, INTENT_FOE_HEALTH, 1, {IDLE}),
               army(rows, ally, ally, 1, TARGET_HEALTH, 0, {IDLE}),
               army(rows, far, far, 1, TARGET_HEALTH, 1, {IDLE})},
              {},
              rules};
  world.steer(CHASER, true);
  return world;
}

World intent_world(std::int32_t apart) {
  return intent_world(apart, Cell{STAND.x + apart, STAND.y},
                      open_map(OPEN_SIDE));
}

TEST(World, AnAttackIntentClosesOnItsTargetAndStrikesItUntilItDies) {
  constexpr std::int32_t APART = 4;
  constexpr std::uint64_t CHASE_TICKS = 300;
  // Control: steered and holding no intent, the chaser stays on its cell and
  // the foe beyond its reach is unhurt.
  World idle = intent_world(APART);
  run_ticks(idle, CHASE_TICKS);
  EXPECT_TRUE(in(STAND, idle.person(CHASER).position));
  EXPECT_EQ(idle.person(INTENT_FOE).health, INTENT_FOE_HEALTH);

  World world = intent_world(APART);
  world.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  ASSERT_EQ(world.target(CHASER), std::optional<std::size_t>{INTENT_FOE});
  run_ticks(world, CHASE_TICKS);
  // The foe stood beyond arm's length, so its death shows the chaser closed.
  EXPECT_EQ(world.person(INTENT_FOE).health, 0U);
  // Its target dead, the intent ends.
  EXPECT_EQ(world.target(CHASER), std::nullopt);
}

TEST(World, AMoveOrderGivenDuringAnIntentReachesItsCell) {
  constexpr std::int32_t APART = 4;
  // Too few ticks to reach a foe four cells away, then enough to walk on.
  constexpr std::uint64_t APPROACH_TICKS = 10;
  constexpr std::uint64_t AFTER_TICKS = 300;
  const Cell elsewhere{STAND.x, STAND.y + 4};
  World world = intent_world(APART);
  world.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(world, APPROACH_TICKS);
  ASSERT_FALSE(in(STAND, world.person(CHASER).position));
  world.order(CHASER, Order{elsewhere, {0}});
  run_ticks(world, AFTER_TICKS);
  // The order replaced the intent and the chaser walked to its cell.
  EXPECT_EQ(world.target(CHASER), std::nullopt);
  EXPECT_TRUE(in(elsewhere, world.person(CHASER).position));
  EXPECT_EQ(world.person(INTENT_FOE).health, INTENT_FOE_HEALTH);

  // Control: an intent the core ends, its target dead, leaves the chaser
  // holding where it struck, not walking back to STAND.
  World ended = intent_world(APART);
  ended.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(ended, AFTER_TICKS);
  ASSERT_EQ(ended.person(INTENT_FOE).health, 0U);
  const Cell held = cell_of(ended.person(CHASER).position);
  EXPECT_EQ(ended.objective(CHASER).x, held.x);
  EXPECT_EQ(ended.objective(CHASER).y, held.y);
  run_ticks(ended, AFTER_TICKS);
  EXPECT_TRUE(in(held, ended.person(CHASER).position));
  EXPECT_FALSE(in(STAND, ended.person(CHASER).position));
}

TEST(World, EndingAnIntentHoldsTheArmyWhereItsLeadStands) {
  constexpr std::int32_t APART = 4;
  constexpr std::uint64_t APPROACH_TICKS = 10;
  constexpr std::uint64_t AFTER_TICKS = 300;
  World world = intent_world(APART);
  world.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(world, APPROACH_TICKS);
  ASSERT_FALSE(in(STAND, world.person(CHASER).position));
  const Cell left = cell_of(world.person(CHASER).position);
  world.end_intent(CHASER);
  EXPECT_EQ(world.target(CHASER), std::nullopt);
  EXPECT_EQ(world.objective(CHASER).x, left.x);
  EXPECT_EQ(world.objective(CHASER).y, left.y);
  run_ticks(world, AFTER_TICKS);
  // It holds where it was, neither back at STAND nor on to the foe.
  EXPECT_TRUE(in(left, world.person(CHASER).position));
  EXPECT_EQ(world.person(INTENT_FOE).health, INTENT_FOE_HEALTH);

  // Control: an order back to STAND, given at the same point, walks it back.
  World ordered = intent_world(APART);
  ordered.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(ordered, APPROACH_TICKS);
  ASSERT_FALSE(in(STAND, ordered.person(CHASER).position));
  ordered.order(CHASER, Order{STAND, {0}});
  run_ticks(ordered, AFTER_TICKS);
  EXPECT_TRUE(in(STAND, ordered.person(CHASER).position));
}

TEST(World, AnAttackIntentStepsRoundACornerShuttingItsDiagonal) {
  constexpr std::uint64_t CHASE_TICKS = 300;
  // The foe stands two cells west and four north, so the step nearest the
  // line to it is north-west, which a wall west of the chaser shuts.
  const Cell foe{STAND.x - 2, STAND.y - 4};
  std::vector<std::string> rows = open_map(OPEN_SIDE);
  rows.at(STAND.y).at(STAND.x - 1) = '#';
  const Skill strike{ARM, 0, INTENT_DAMAGE, 0, 1, 0, Shape::TARGETED};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  World world{Seed{1},
              Map{rows},
              {army(rows, STAND, STAND, 1, TARGET_HEALTH, 0, {strike}),
               army(rows, foe, foe, 1, INTENT_FOE_HEALTH, 1, {IDLE})},
              {},
              rules};
  // Both steered, so neither closes on the other but by the intent.
  world.steer(0, true);
  world.steer(1, true);
  world.order(0, Order{STAND, {0}, 1});
  ASSERT_EQ(world.target(0), std::optional<std::size_t>{1});
  run_ticks(world, CHASE_TICKS);

  // It went north round the wall, reached the foe and struck it dead.
  EXPECT_EQ(world.person(1).health, 0U);
}

TEST(World, AnOrderWithoutATargetEndsTheAttackIntent) {
  constexpr std::int32_t APART = 4;
  // Too few ticks to come within arm's length of a foe four cells away.
  constexpr std::uint64_t APPROACH_TICKS = 10;
  constexpr std::uint64_t AFTER_TICKS = 200;
  World world = intent_world(APART);
  world.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(world, APPROACH_TICKS);
  ASSERT_FALSE(in(STAND, world.person(CHASER).position));
  world.order(CHASER, Order{STAND, {0}});
  EXPECT_EQ(world.target(CHASER), std::nullopt);
  run_ticks(world, AFTER_TICKS);
  // It walks back to its objective and never reaches the foe.
  EXPECT_TRUE(in(STAND, world.person(CHASER).position));
  EXPECT_EQ(world.person(INTENT_FOE).health, INTENT_FOE_HEALTH);
}

TEST(World, AnAttackIntentEndsWhenItsTargetLeavesSight) {
  // The chaser stands walled in, so the foe walking east leaves its sight.
  std::vector<std::string> rows = open_map(OPEN_SIDE);
  for (std::int32_t row = STAND.y - 1; row <= STAND.y + 1; ++row) {
    for (std::int32_t column = STAND.x - 1; column <= STAND.x + 1; ++column) {
      const bool own = column == STAND.x && row == STAND.y;
      rows.at(static_cast<std::size_t>(row))
          .at(static_cast<std::size_t>(column)) = own ? '.' : '#';
    }
  }
  constexpr std::int32_t APART = 3;
  constexpr std::int32_t AWAY = 12;
  constexpr std::uint64_t WALK_TICKS = 200;
  World leaving = intent_world(APART, Cell{STAND.x + AWAY, STAND.y}, rows);
  leaving.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(leaving, WALK_TICKS);
  ASSERT_GT(distance_squared(leaving.person(CHASER).position,
                             leaving.person(INTENT_FOE).position),
            std::int64_t{SIGHT} * CELL_SIZE * SIGHT * CELL_SIZE);
  EXPECT_EQ(leaving.target(CHASER), std::nullopt);
  // Control: a foe that stays in sight keeps the intent.
  World staying = intent_world(APART, Cell{STAND.x + APART, STAND.y}, rows);
  staying.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  run_ticks(staying, WALK_TICKS);
  EXPECT_EQ(staying.target(CHASER), std::optional<std::size_t>{INTENT_FOE});
}

TEST(World, AnArmyMayTargetOnlyALivingFoeInSight) {
  World world = intent_world(4);
  EXPECT_EQ(world.attack_refusal(CHASER, INTENT_FOE), AttackRefusal::NONE);
  EXPECT_EQ(world.attack_refusal(CHASER, INTENT_ALLY),
            AttackRefusal::NOT_A_FOE);
  EXPECT_EQ(world.attack_refusal(CHASER, CHASER), AttackRefusal::NOT_A_FOE);
  EXPECT_EQ(world.attack_refusal(CHASER, UNSEEN_FOE),
            AttackRefusal::OUT_OF_SIGHT);
  EXPECT_EQ(world.attack_refusal(CHASER, world.population()),
            AttackRefusal::NO_TARGET);
  EXPECT_THROW(world.order(CHASER, Order{STAND, {0}, INTENT_ALLY}),
               std::invalid_argument);
  EXPECT_EQ(world.target(CHASER), std::nullopt);
  // Control: the foe in sight is taken.
  world.order(CHASER, Order{STAND, {0}, INTENT_FOE});
  EXPECT_EQ(world.target(CHASER), std::optional<std::size_t>{INTENT_FOE});
}

TEST(World, FoesInSightAreListedNearestFirst) {
  const World world = intent_world(4);
  // The foe four cells east; the ally and the foe beyond sight are left out.
  EXPECT_EQ(world.foes_in_sight(CHASER),
            (std::vector<std::size_t>{INTENT_FOE}));
  // From the foe, the ally under three cells away comes before the chaser
  // four, though the chaser serves in an earlier army.
  EXPECT_EQ(world.foes_in_sight(INTENT_FOE),
            (std::vector<std::size_t>{INTENT_ALLY, CHASER}));
}

// A village whose centre is STAND, claiming 3 cells about it, with one
// resident there who strikes no one; a foe who strikes no one `apart` cells
// east of the centre; and, a cell south of the centre, a person sweeping at
// arm's length in an army of its own bound for `objective`, whose home is the
// village if `homed`. Its people close on a foe within SIGHT cells.
constexpr std::size_t DEFENDER = 2;
constexpr Cell BESIDE{STAND.x, STAND.y + 1};

World defending(std::int32_t apart, Cell objective, bool homed) {
  constexpr std::int32_t CLAIM = 3;
  const Skill arm{ARM, 0, 1, 0, 1, 0, Shape::ARC};
  const Cell foe{STAND.x + apart, STAND.y};
  Rules rules = at_war(SEPARATION);
  rules.sight = SIGHT;
  Life life = one_discipline(0);
  Growth growth;
  growth.interval = 1;
  growth.claim = CLAIM;
  growth.stances = {{}, {}};
  life.growth = growth;
  ArmySpec defender = army(BESIDE, objective, 1, TARGET_HEALTH, 0, {arm});
  defender.home = homed ? std::optional<std::size_t>{0} : std::nullopt;
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army(STAND, STAND, 1, TARGET_HEALTH, 0, {IDLE}),
                army(foe, foe, 1, TARGET_HEALTH, 1, {IDLE}), defender},
               {},
               rules,
               {VillageSpec{{}, 0, {}, 0, {}, {}}},
               {},
               life};
}

constexpr Cell CHART_HOME{8, 9};
constexpr Cell CHART_HOUSE{8, 8};
constexpr std::int32_t CHART_REACH = 5;
constexpr Cell CHART_WATCH{8, 11};
constexpr Cell CHART_AWAY{28, 28};
constexpr Cell CHART_MOVED{18, 9};
constexpr std::size_t CHART_WATCHER = 1;

// A peopled village with one house and a steered soul beside it, on open
// ground.
World territory_chart_world() {
  Rules rules = peaceful(SEPARATION);
  rules.map_sight = CHART_REACH;
  Life life = one_discipline(0);
  Growth growth;
  growth.interval = 1;
  growth.claim = 4;
  growth.stances.resize(rules.hostile.size());
  life.growth = growth;
  World world{Seed{1},
              Map{open_map(GROWTH_SIDE)},
              {army(CHART_HOME, CHART_HOME, 1, TARGET_HEALTH, 0, {IDLE}),
               army(CHART_WATCH, CHART_WATCH, 1, TARGET_HEALTH, 0, {IDLE})},
              {Structure{CHART_HOUSE, INTACT}},
              rules,
              {VillageSpec{{0}, 0, {}, 0, {}, {}}},
              {},
              life};
  world.ensoul(world.roster(CHART_WATCHER).front());
  world.steer(CHART_WATCHER, true);
  world.tick();
  return world;
}

void walk_chart_watcher(World& world, Cell destination) {
  world.order(CHART_WATCHER, {destination, {0}});
  for (std::uint64_t tick = 0; tick < PHASE_TICKS; ++tick) {
    const Cell here =
        cell_of(world.person(world.roster(CHART_WATCHER).front()).position);
    if (here.x == destination.x && here.y == destination.y) {
      return;
    }
    world.tick();
  }
}

TEST(World, RememberedBordersKeepTheLastSeenClaimCentreUntilSeenAgain) {
  World world = territory_chart_world();
  walk_chart_watcher(world, CHART_AWAY);
  const Cell away =
      cell_of(world.person(world.roster(CHART_WATCHER).front()).position);
  ASSERT_EQ(away.x, CHART_AWAY.x);
  ASSERT_EQ(away.y, CHART_AWAY.y);
  const auto remembered_before = world.remembered_territory(0);
  ASSERT_TRUE(std::any_of(remembered_before.begin(), remembered_before.end(),
                          [](const auto& owner) { return owner.has_value(); }));
  const auto live_before = world.territory();
  const auto revision = world.chart(0).revision();
  world.order(0, {CHART_MOVED, {0}});
  world.tick();
  ASSERT_NE(world.territory(), live_before);
  EXPECT_EQ(world.remembered_territory(0), remembered_before);
  EXPECT_EQ(world.chart(0).revision(), revision);
  walk_chart_watcher(world, CHART_WATCH);
  const Cell returned =
      cell_of(world.person(world.roster(CHART_WATCHER).front()).position);
  ASSERT_EQ(returned.x, CHART_WATCH.x);
  ASSERT_EQ(returned.y, CHART_WATCH.y);
  constexpr Cell NEW_CLAIM_CELL{15, 11};
  const auto slot = [](Cell cell) {
    return (static_cast<std::size_t>(cell.y) * GROWTH_SIDE) +
           static_cast<std::size_t>(cell.x);
  };
  ASSERT_TRUE(world.chart(0).seen(CHART_HOME));
  ASSERT_TRUE(world.chart(0).seen(NEW_CLAIM_CELL));
  ASSERT_EQ(world.territory().at(slot(NEW_CLAIM_CELL)),
            std::optional<std::size_t>{0});
  const auto refreshed = world.remembered_territory(0);
  EXPECT_EQ(refreshed.at(slot(CHART_HOME)), std::nullopt);
  EXPECT_EQ(refreshed.at(slot(NEW_CLAIM_CELL)), std::optional<std::size_t>{0});

  // Control: the same change in sight changes the revision without seeing a new
  // cell.
  World watched = territory_chart_world();
  const auto before = watched.chart(0).revision();
  const auto blocks = watched.chart(0).blocks();
  ASSERT_EQ(blocks.size(), 1U);
  watched.order(0, {CHART_MOVED, {0}});
  watched.tick();
  ASSERT_EQ(watched.chart(0).blocks().size(), 1U);
  EXPECT_EQ(watched.chart(0).blocks().front().bits, blocks.front().bits);
  EXPECT_GT(watched.chart(0).revision(), before);
}

TEST(World, AtHomeAPersonDefendsItsClaimAndOnTheMarchFightsWhatItMeets) {
  constexpr std::uint64_t TICKS = 30;
  // Cells east of the centre, beyond its claim of 3 and within sight.
  constexpr std::int32_t BEYOND = 5;
  // A foe inside the claim is closed on.
  World inside = defending(3, BESIDE, true);
  ASSERT_EQ(inside.person(DEFENDER).home, std::optional<std::size_t>{0});
  run_ticks(inside, TICKS);
  EXPECT_FALSE(in(BESIDE, inside.person(DEFENDER).position));
  // A foe in sight beyond the claim is left alone at home.
  World beyond = defending(BEYOND, BESIDE, true);
  run_ticks(beyond, TICKS);
  EXPECT_TRUE(in(BESIDE, beyond.person(DEFENDER).position));
  // Control: with no home the same person closes on it.
  World homeless = defending(BEYOND, BESIDE, false);
  ASSERT_FALSE(homeless.person(DEFENDER).home.has_value());
  run_ticks(homeless, TICKS);
  EXPECT_GT(homeless.person(DEFENDER).position.x, centre_of(BESIDE).x);
  // Marching south past its claim, it turns on the foe east of it.
  World marching = defending(BEYOND, {BESIDE.x, BESIDE.y + BEYOND}, true);
  run_ticks(marching, TICKS);
  EXPECT_GT(marching.person(DEFENDER).position.x, centre_of(BESIDE).x);
}

TEST(World, ASteeredBodyDoesNotCloseOnAFoe) {
  World steered = closing_world(SIGHT - 1);
  steered.steer(0, true);
  run_ticks(steered, CLOSING_TICKS);
  // The foe closes on it, and its automatic strike still swings back.
  EXPECT_TRUE(in(STAND, steered.person(0).position));
  EXPECT_GT(lost(steered, 1), 0U);
  // Control: unsteered, it leaves its cell to close on the foe.
  World free = closing_world(SIGHT - 1);
  run_ticks(free, CLOSING_TICKS);
  EXPECT_FALSE(in(STAND, free.person(0).position));
}

TEST(World, ACastIsRefusedWithItsReason) {
  World world = cast_world();
  // Bolt reaches four cells; the far foe stands four cells east.
  EXPECT_EQ(world.cast_refusal(CASTER, BOLT, AtPerson{FAR_FOE}),
            CastRefusal::NONE);
  EXPECT_EQ(world.cast_refusal(CASTER, SHORT_BOLT, AtPerson{FAR_FOE}),
            CastRefusal::OUT_OF_RANGE);
  EXPECT_THROW(world.cast(CASTER, SHORT_BOLT, AtPerson{FAR_FOE}),
               std::invalid_argument);
  EXPECT_EQ(world.cast_refusal(CASTER, SKILLS, Itself{}),
            CastRefusal::NO_SKILL);
  EXPECT_EQ(world.cast_refusal(CASTER, BOLT, AtPerson{world.population()}),
            CastRefusal::NO_TARGET);
  EXPECT_THROW(
      static_cast<void>(world.cast_refusal(world.population(), BOLT, Itself{})),
      std::invalid_argument);

  world.cast(CASTER, BOLT, AtPerson{NEAR_FOE});
  world.tick();
  ASSERT_EQ(lost(world, NEAR_FOE), BOLT_DAMAGE);
  EXPECT_EQ(world.cast_refusal(CASTER, BOLT, AtPerson{NEAR_FOE}),
            CastRefusal::ON_COOLDOWN);
  EXPECT_THROW(world.cast(CASTER, BOLT, AtPerson{NEAR_FOE}),
               std::invalid_argument);
  // Control: once the cooldown has passed, the same cast may be made.
  run_ticks(world, CAST_COOLDOWN - 1);
  EXPECT_EQ(world.cast_refusal(CASTER, BOLT, AtPerson{NEAR_FOE}),
            CastRefusal::NONE);
}

TEST(World, ACastByOrAtTheDeadIsRefused) {
  // A hostile of the caster's bolt's damage dies to one.
  const Skill bolt{4 * CELL_SIZE, 0, BOLT_DAMAGE, 0, 1};
  World world{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(CASTER_CELL, CASTER_CELL, 1, TARGET_HEALTH, 0, {IDLE, bolt}),
       army(NEAR_FOE_CELL, NEAR_FOE_CELL, 1, BOLT_DAMAGE, 1, {IDLE, bolt})},
      {},
      at_war(0)};
  world.order(CASTER, {CASTER_CELL, {0}});
  world.order(NEAR_FOE, {NEAR_FOE_CELL, {0}});
  // Control: both may cast before the bolt lands.
  EXPECT_EQ(world.cast_refusal(NEAR_FOE, 1, Itself{}), CastRefusal::NONE);
  EXPECT_EQ(world.cast_refusal(CASTER, 1, AtPerson{NEAR_FOE}),
            CastRefusal::NONE);
  world.cast(CASTER, 1, AtPerson{NEAR_FOE});
  world.tick();
  ASSERT_EQ(world.person(NEAR_FOE).health, 0U);

  EXPECT_EQ(world.cast_refusal(NEAR_FOE, 1, Itself{}), CastRefusal::DEAD);
  EXPECT_EQ(world.cast_refusal(CASTER, 1, AtPerson{NEAR_FOE}),
            CastRefusal::NO_TARGET);
}

TEST(World, AnNpcCastsThroughTheSameFunction) {
  // The foe, an NPC of faction 1, casts a bolt at a person of faction 0.
  const Skill bolt{4 * CELL_SIZE, 0, BOLT_DAMAGE, 0, CAST_COOLDOWN};
  World npc{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {army(CASTER_CELL, CASTER_CELL, 1, TARGET_HEALTH, 0, {IDLE}),
       army(NEAR_FOE_CELL, NEAR_FOE_CELL, 1, TARGET_HEALTH, 1, {IDLE, bolt})},
      {},
      at_war(0)};
  npc.order(NEAR_FOE, {NEAR_FOE_CELL, {0}});
  npc.cast(NEAR_FOE, 1, AtPerson{CASTER});
  npc.tick();
  EXPECT_EQ(lost(npc, CASTER), BOLT_DAMAGE);
  EXPECT_EQ(npc.person(NEAR_FOE).ready_at, 1 + CAST_COOLDOWN);
  // Control: with no cast, its order's IDLE hits no one.
  npc.tick();
  run_ticks(npc, CAST_COOLDOWN);
  EXPECT_EQ(lost(npc, CASTER), BOLT_DAMAGE);
}

TEST(World, ALaterCastBeforeTheTickReplacesTheFirst) {
  World world = cast_world();
  world.cast(CASTER, BOLT, AtPerson{NEAR_FOE});
  world.cast(CASTER, BOLT, AtPerson{FAR_FOE});
  world.tick();

  EXPECT_EQ(lost(world, FAR_FOE), BOLT_DAMAGE);
  EXPECT_EQ(lost(world, NEAR_FOE), 0U);
  // The cast is spent: the next tick makes no use of it.
  EXPECT_FALSE(world.person(CASTER).casting.has_value());
}

TEST(World, ACastNoLongerAllowedOnItsTickLapses) {
  World world = cast_world();
  world.cast(CASTER, BOLT, AtPerson{NEAR_FOE});
  // An army of IDLE alone has no skill of the bolt's index.
  world.conscript(CASTER, NEAR_FOE);
  ASSERT_EQ(world.cast_refusal(CASTER, BOLT, AtPerson{NEAR_FOE}),
            CastRefusal::NO_SKILL);
  world.tick();
  EXPECT_EQ(lost(world, NEAR_FOE), 0U);
  EXPECT_FALSE(world.person(CASTER).casting.has_value());
}

TEST(World, CastsReplayToTheSameHash) {
  const auto played = [](std::uint64_t first) {
    World world = cast_world();
    for (std::uint64_t tick = 0; tick < 2 * CAST_COOLDOWN; ++tick) {
      if (tick == first) {
        world.cast(CASTER, BOLT, AtPerson{FAR_FOE});
      }
      if (tick == first + CAST_COOLDOWN) {
        world.cast(CASTER, NOVA, Itself{});
      }
      world.tick();
    }
    return world.hash();
  };
  EXPECT_EQ(played(1), played(1));
  // Control: the same casts a tick later are another run.
  EXPECT_NE(played(1), played(2));
}

TEST(World, APendingCastIsPartOfTheHash) {
  World cast = cast_world();
  cast.cast(CASTER, BOLT, AtPerson{FAR_FOE});
  World other = cast_world();
  other.cast(CASTER, BOLT, AtPerson{NEAR_FOE});
  EXPECT_NE(cast.hash(), other.hash());
  // Control: the same cast hashes the same.
  World same = cast_world();
  same.cast(CASTER, BOLT, AtPerson{FAR_FOE});
  EXPECT_EQ(cast.hash(), same.hash());
}

TEST(World, AimTakesTheNearestFoeAheadOfTheFacing) {
  const World world = cast_world();
  const Step east{1, 0};
  const Step west{-1, 0};
  const Step south{0, 1};
  EXPECT_EQ(world.aim(CASTER, BOLT, east),
            std::optional<std::size_t>{NEAR_FOE});
  EXPECT_EQ(world.aim(CASTER, BOLT, west),
            std::optional<std::size_t>{BEHIND_FOE});
  // Controls: with no facing, or one with no one ahead but those beside it,
  // the nearest is taken, as the automatic strike takes it.
  EXPECT_EQ(world.aim(CASTER, BOLT, Step{0, 0}),
            std::optional<std::size_t>{BEHIND_FOE});
  EXPECT_EQ(world.aim(CASTER, BOLT, south),
            std::optional<std::size_t>{BEHIND_FOE});
  // A skill whose range reaches no one aims at no one.
  EXPECT_EQ(world.aim(CASTER, NOVA, east), std::nullopt);
  EXPECT_THROW(
      static_cast<void>(world.aim(CASTER, BOLT, Step{MAX_REACH + 1, 0})),
      std::invalid_argument);
}

// The qi tests' world, days of one tick: a reader of faction 0 at
// READER_CELL; a sect of faction 0 seated at SEAT_HOME, whose two founders
// are its elders and hold `held` manuals; an outsider of faction 1, at
// peace with everyone; and a foe of faction 2, hostile to faction 0, at
// QI_FOE_CELL, beyond any skill's reach of the seat's cell. Everyone's army
// strikes with IDLE alone. People cultivate
// `growth` qi a day up to `limit`, recovering `recovery` per mille of it
// every `recovery_ticks` ticks, over realms at 100, 300 and 800 qi, and
// learn the arts ORTHODOX_ART and DEMONIC_ART by `learning`.
constexpr std::uint32_t QI_GROWTH = 10;
constexpr std::uint32_t QI_LIMIT = 1'000;
constexpr std::uint32_t THIRD_RATE = 100;
constexpr std::uint32_t SECOND_RATE = 300;
constexpr std::uint32_t FIRST_RATE = 800;
constexpr std::uint32_t HALF = PER_MILLE / 2;
constexpr Spread FOUNDED_DAYS{100, 0};
constexpr Seed QI_SEED{5};

struct Cultivating {
  Spread growth{QI_GROWTH, 0};
  Spread limit{QI_LIMIT, 0};
  std::uint32_t recovery = PER_MILLE;
  std::uint64_t recovery_ticks = 1;
  std::optional<Spread> founding_age;
  std::uint32_t might = PER_MILLE;
  std::uint32_t strike_cost = 50;
  bool form_comprehensible = true;
  Learning learning{0,         0,         50,        CELL_SIZE,
                    PER_MILLE, PER_MILLE, PER_MILLE, PER_MILLE};
  std::vector<std::size_t> held{0, 1};
  std::uint32_t admission_chance = 0;
  std::int32_t rumour_reach = 0;
};

constexpr std::size_t READER = 0;
constexpr std::size_t READER_ARMY = 0;
constexpr std::size_t QI_SEAT_ARMY = 1;
constexpr std::size_t QI_OUTSIDERS = 2;
constexpr std::size_t QI_FOE = 4;
constexpr std::size_t QI_FOE_ARMY = 3;
constexpr Cell READER_CELL{2, 9};
constexpr Cell QI_FOE_CELL{12, 12};
// Where the foe comes to fight: four cells east of the seat's cell.
constexpr Cell QI_FIELD{6, 3};
constexpr Cell QI_OUTSIDERS_CELL{13, 1};
constexpr std::uint64_t QI_FOE_HEALTH = 10'000;
constexpr std::uint64_t ART_DAMAGE = 10;
constexpr std::size_t ORTHODOX_ART = 0;
constexpr std::size_t DEMONIC_ART = 1;
// The reader's skills: IDLE, then those it learns, in the order learned:
// the strike and the sweep, the cut, and the form once first-rate.
constexpr std::size_t STRIKE = 1;
constexpr std::size_t CUT = 3;

World qi_world(const Cultivating& cultivating) {
  const std::int32_t reach = 5 * CELL_SIZE;
  const Skill strike{reach, 0, ART_DAMAGE, 0, 30, cultivating.strike_cost};
  const Skill sweep{reach, CELL_SIZE, ART_DAMAGE, 0, 60, 100};
  const Skill form{reach, CELL_SIZE, ART_DAMAGE, 0, 90, 100};
  Life life = one_discipline(0);
  life.founding_age = cultivating.founding_age;
  life.cultivation =
      Cultivation{cultivating.growth, cultivating.limit, cultivating.recovery,
                  cultivating.recovery_ticks};
  life.realms = {Realm{{"삼류", "Third-rate"}, THIRD_RATE, PER_MILLE},
                 Realm{{"이류", "Second-rate"}, SECOND_RATE, cultivating.might},
                 Realm{{"일류", "First-rate"}, FIRST_RATE, cultivating.might}};
  life.arts = {
      Art{{"매화검법", "Plum Blossom Sword"},
          0,
          Nature{Orientation::ORTHODOX, Polarity::YANG},
          {ArtSkill{{"일검", "Strike"}, strike, 1, false},
           ArtSkill{{"이검", "Sweep"}, sweep, 2, true},
           ArtSkill{
               {"삼검", "Form"}, form, 3, cultivating.form_comprehensible}}},
      Art{{"천마도법", "Heavenly Demon Saber"},
          0,
          Nature{Orientation::DEMONIC, Polarity::YIN},
          {ArtSkill{{"일도", "Cut"}, strike, 1, false}}}};
  life.learning = cultivating.learning;
  // Every manual the sect holds scatters onto its seat's own cell when it
  // falls.
  life.manuals = Manuals{{cultivating.held}, PER_MILLE, 0};
  Rules rules{
      SEPARATION,
      {{false, false, true}, {false, false, false}, {true, false, false}},
      STAGE_FLOORS};
  rules.ticks_per_day = 1;
  rules.rumour_reach = cultivating.rumour_reach;
  const Leadership members{
      std::nullopt, std::nullopt,
      Admission{0, 0, 0, OPEN_SIDE, cultivating.admission_chance, PER_MILLE},
      std::nullopt};
  constexpr std::uint32_t FOUNDING_ATTAINMENT = 20;
  return World{
      QI_SEED,
      Map{open_map(OPEN_SIDE)},
      {army(READER_CELL, READER_CELL, 1, 1, 0, {IDLE}),
       army(SEAT_HOME, SEAT_HOME, 2, 1, 0, {IDLE}),
       army(QI_OUTSIDERS_CELL, QI_OUTSIDERS_CELL, 1, 1, 1, {IDLE}),
       army(QI_FOE_CELL, QI_FOE_CELL, 1, QI_FOE_HEALTH, 2, {IDLE})},
      {Structure{{SEAT_HOME.x, 0}, INTACT, {1, 1}, true}},
      rules,
      {VillageSpec{{0}, QI_SEAT_ARMY, {Holding{0, 0, 0, 4}}, 0, {}, {}}},
      {members, Leadership{}, Leadership{}},
      life,
      {SectSpec{
          "Hwasan", 0, 0, {Founder{1, FOUNDING_ATTAINMENT}, Founder{2, 0}}}}};
}

// Cultivating `cultivating`'s world after its sect has fallen, its manuals
// lying on the seat's cell, and the reader has stood there `ticks` ticks
// after walking to it.
World read(const Cultivating& cultivating, std::uint64_t ticks = 10) {
  constexpr int LONGEST_WALK = 1'000;
  World world = qi_world(cultivating);
  world.conscript(1, QI_OUTSIDERS);
  world.conscript(2, QI_OUTSIDERS);
  world.order(READER_ARMY, {SEAT_HOME, {0}});
  for (int walked = 0;
       walked < LONGEST_WALK && !in(SEAT_HOME, world.person(READER).position);
       ++walked) {
    world.tick();
  }
  EXPECT_TRUE(in(SEAT_HOME, world.person(READER).position));
  run_ticks(world, ticks);
  return world;
}

// The art skills person `person` knows, as (art, skill) pairs in the order
// learned.
std::vector<std::pair<std::size_t, std::size_t>> known_by(const World& world,
                                                          std::size_t person) {
  std::vector<std::pair<std::size_t, std::size_t>> known;
  for (const Known& each : world.person(person).known) {
    known.emplace_back(each.art, each.skill);
  }
  return known;
}

TEST(World, QiCapacityGrowsADayUpToItsLimitAndQiRecovers) {
  constexpr std::uint32_t LIMIT = 25;
  constexpr std::uint32_t FOUR_TENTHS = 400;
  constexpr std::uint64_t FILLING = 10;
  Cultivating slow;
  slow.limit = {LIMIT, 0};
  slow.recovery = FOUR_TENTHS;
  World world = qi_world(slow);
  run_ticks(world, 2);
  // Two days' growth, and the second tick recovered four tenths of the
  // first day's ten, rounded up.
  EXPECT_EQ(world.person(READER).qi_capacity, 20U);
  EXPECT_EQ(world.person(READER).qi, 4U);
  run_ticks(world, FILLING);
  EXPECT_EQ(world.person(READER).qi_capacity, LIMIT);
  EXPECT_EQ(world.person(READER).qi, LIMIT);
  // Control: a world with no cultivation holds no qi.
  World uncultivated{QI_SEED,
                     Map{open_map(OPEN_SIDE)},
                     {army(READER_CELL, READER_CELL, 1, 1, 0, {IDLE})},
                     {},
                     peaceful(0)};
  run_ticks(uncultivated, 2 + FILLING);
  EXPECT_EQ(uncultivated.person(READER).qi_capacity, 0U);
}

TEST(World, ThePeopleAWorldStartsWithCultivatedForTheirFoundingAge) {
  Cultivating founded;
  founded.growth = {3, 0};
  founded.founding_age = FOUNDED_DAYS;
  const World world = qi_world(founded);
  EXPECT_EQ(world.person(READER).qi_capacity, 300U);
  EXPECT_EQ(world.person(READER).qi, 300U);
  // Control: born as the world begins, they hold none.
  EXPECT_EQ(qi_world(Cultivating{}).person(READER).qi_capacity, 0U);
}

TEST(World, ARealmIsHowManyRealmsTheQiCapacityReaches) {
  World world = qi_world(Cultivating{});
  EXPECT_EQ(world.rank(READER), 0U);
  run_ticks(world, THIRD_RATE / QI_GROWTH);
  ASSERT_EQ(world.person(READER).qi_capacity, THIRD_RATE);
  EXPECT_EQ(world.rank(READER), 1U);
  run_ticks(world, (FIRST_RATE - THIRD_RATE) / QI_GROWTH);
  ASSERT_EQ(world.person(READER).qi_capacity, FIRST_RATE);
  EXPECT_EQ(world.rank(READER), 3U);
  // Control: a tick short of a threshold is the realm below.
  World short_of = qi_world(Cultivating{});
  run_ticks(short_of, (THIRD_RATE / QI_GROWTH) - 1);
  EXPECT_EQ(short_of.rank(READER), 0U);
}

bool cultivates_by(const Life& life) {
  try {
    const World world{
        Seed{1}, Map{open_map(OPEN_SIDE)}, {}, {}, peaceful(0), {}, {}, life};
    return true;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

TEST(World, QiDataPastItsLimitsIsRefused) {
  constexpr std::uint32_t LOWER = 10;
  constexpr std::uint32_t HIGHER = 20;
  Life life = one_discipline(0);
  life.cultivation = Cultivation{{1, 0}, {HIGHER, 0}, LOWER, 1};
  life.realms = {Realm{{"a", "a"}, LOWER, PER_MILLE},
                 Realm{{"b", "b"}, HIGHER, PER_MILLE}};
  ASSERT_TRUE(cultivates_by(life));
  Life descending = life;
  std::swap(descending.realms.at(0), descending.realms.at(1));
  Life mighty = life;
  mighty.realms.at(1).might = MAX_MIGHT + 1;
  Life stray = life;
  stray.arts.at(0).discipline = 1;
  Life beyond = life;
  beyond.arts.at(0).skills.push_back(ArtSkill{{"c", "c"}, IDLE, 3, true});
  Life costly = life;
  costly.arts.at(0).skills.push_back(
      ArtSkill{{"c", "c"}, Skill{0, 0, 0, 0, 0, MAX_TALENT + 1}, 0, true});
  Life never = life;
  never.cultivation->recovery_ticks = 0;
  Life easy = life;
  easy.learning.orientation_ease = PER_MILLE + 1;
  for (const Life& refused :
       {descending, mighty, stray, beyond, costly, never, easy}) {
    EXPECT_FALSE(cultivates_by(refused));
  }
}

TEST(World, AManualOnACellTeachesWhatItsReadersRealmAllowsAndStays) {
  const World world = read(Cultivating{});
  ASSERT_TRUE(std::holds_alternative<Cell>(world.manuals().front().place));
  ASSERT_EQ(world.rank(READER), 3U);
  // Both manuals as second-rate, from a cell away, the orthodox art's
  // skills first; then the form, read again from the manual still lying
  // there once first-rate.
  EXPECT_EQ(
      known_by(world, READER),
      (std::vector<std::pair<std::size_t, std::size_t>>{{ORTHODOX_ART, 0},
                                                        {ORTHODOX_ART, 1},
                                                        {DEMONIC_ART, 0},
                                                        {ORTHODOX_ART, 2}}));
  EXPECT_EQ(world.skill_count(READER), 5U);
  // The manuals stay where they lie.
  for (const Manual& manual : world.manuals()) {
    EXPECT_TRUE(std::holds_alternative<Cell>(manual.place));
  }
  // Control: a reader holding less qi than the reading costs learns nothing.
  Cultivating costly;
  costly.learning.reading_cost = QI_LIMIT + 1;
  EXPECT_TRUE(known_by(read(costly), READER).empty());
}

TEST(World, ReadingSpendsQiAndTeachesOnlyUpToTheReadersRealm) {
  // Founded with 400 qi, which never recovers, and growing no more: second
  // rank, and qi for one reading of 150 but not two.
  Cultivating held;
  held.growth = {4, 0};
  constexpr std::uint32_t STORE = 400;
  constexpr std::uint32_t ONCE = 300;
  constexpr std::uint32_t TWICE = 200;
  held.limit = {STORE, 0};
  held.founding_age = FOUNDED_DAYS;
  held.recovery_ticks = NEVER_AGAIN;
  held.learning.reading_cost = ONCE;
  const World world = read(held);
  ASSERT_EQ(world.rank(READER), 2U);
  EXPECT_EQ(known_by(world, READER),
            (std::vector<std::pair<std::size_t, std::size_t>>{
                {ORTHODOX_ART, 0}, {ORTHODOX_ART, 1}}));
  EXPECT_EQ(world.person(READER).qi, STORE - ONCE);
  // Control: with qi for both readings, the second manual is read too.
  held.learning.reading_cost = TWICE;
  EXPECT_EQ(known_by(read(held), READER).size(), 3U);
}

TEST(World, AnArtOfAnotherNatureCostsMoreToRead) {
  // Half the ease along each axis: the demonic art, both axes away from the
  // orthodox heart, costs four times the 50 qi.
  Cultivating uneasy;
  uneasy.growth = {4, 0};
  constexpr std::uint32_t STORE = 400;
  constexpr std::uint32_t COST = 50;
  uneasy.limit = {STORE, 0};
  uneasy.founding_age = FOUNDED_DAYS;
  uneasy.recovery_ticks = NEVER_AGAIN;
  uneasy.learning.orientation_ease = HALF;
  uneasy.learning.polarity_ease = HALF;
  const World world = read(uneasy);
  ASSERT_EQ(known_by(world, READER).size(), 3U);
  EXPECT_EQ(world.person(READER).qi, STORE - COST - (4 * COST));
  EXPECT_EQ(world.nature(READER)
                .value_or(Nature{Orientation::DEMONIC, Polarity::YIN})
                .orientation,
            Orientation::ORTHODOX);
  // Control: at full ease it costs 50.
  Cultivating easy = uneasy;
  easy.learning.orientation_ease = PER_MILLE;
  easy.learning.polarity_ease = PER_MILLE;
  EXPECT_EQ(read(easy).person(READER).qi, STORE - COST - COST);
}

TEST(World, AnArtsAgreementAndRealmAreReadAsTheReaderLearnsBy) {
  // Founded with 400 qi and growing no more: second rank.
  constexpr std::uint32_t STORE = 400;
  Cultivating uneasy;
  uneasy.growth = {4, 0};
  uneasy.limit = {STORE, 0};
  uneasy.founding_age = FOUNDED_DAYS;
  uneasy.recovery_ticks = NEVER_AGAIN;
  uneasy.learning.orientation_ease = HALF;
  uneasy.learning.polarity_ease = HALF;
  const World world = read(uneasy);
  ASSERT_EQ(world.rank(READER), 2U);
  EXPECT_EQ(world.heart_art(READER), std::optional<std::size_t>{ORTHODOX_ART});
  EXPECT_EQ(world.art_agreement(READER, ORTHODOX_ART), PER_MILLE);
  EXPECT_EQ(world.art_agreement(READER, DEMONIC_ART),
            std::uint64_t{HALF} * HALF / PER_MILLE);
  // The sweep is of rank 2, the form of rank 3.
  EXPECT_TRUE(world.within_realm(READER, ORTHODOX_ART, 1));
  EXPECT_FALSE(world.within_realm(READER, ORTHODOX_ART, 2));
  // Control: the foe knows no art, so has no heart and every art agrees.
  EXPECT_FALSE(world.heart_art(QI_FOE).has_value());
  EXPECT_EQ(world.art_agreement(QI_FOE, DEMONIC_ART), PER_MILLE);
  EXPECT_THROW(static_cast<void>(world.within_realm(READER, ORTHODOX_ART, 3)),
               std::out_of_range);
}

// Cultivating `cultivating`'s world after the reader has read its manuals
// and the foe has come within reach of it, once the reader may cast skill
// `skill` at it.
World facing(const Cultivating& cultivating, std::size_t skill) {
  constexpr int LONGEST_WAIT = 1'000;
  World world = read(cultivating);
  world.order(QI_FOE_ARMY, {QI_FIELD, {0}});
  for (int wait = 0;
       wait < LONGEST_WAIT &&
       world.cast_refusal(READER, skill, AtPerson{QI_FOE}) != CastRefusal::NONE;
       ++wait) {
    world.tick();
  }
  return world;
}

// The health QI_FOE loses on the tick the reader casts skill `skill` at it,
// once it may, after reading `cultivating`'s manuals.
std::uint64_t cast_damage(const Cultivating& cultivating, std::size_t skill) {
  World world = facing(cultivating, skill);
  const std::uint64_t before = world.person(QI_FOE).health;
  world.cast(READER, skill, AtPerson{QI_FOE});
  world.tick();
  return before - world.person(QI_FOE).health;
}

TEST(World, ARealmsMightAndAnArtsNatureScaleItsDamage) {
  EXPECT_EQ(cast_damage(Cultivating{}, STRIKE), ART_DAMAGE);
  Cultivating mighty;
  mighty.might = 2 * PER_MILLE;
  EXPECT_EQ(cast_damage(mighty, STRIKE), 2 * ART_DAMAGE);
  // The demonic art strikes at half for each axis it differs in from the
  // orthodox heart; the heart's own art at full.
  Cultivating weak;
  weak.learning.orientation_potency = HALF;
  weak.learning.polarity_potency = HALF;
  EXPECT_EQ(cast_damage(weak, CUT), ART_DAMAGE / 4);
  EXPECT_EQ(cast_damage(weak, STRIKE), ART_DAMAGE);
}

TEST(World, EachArtSkillKeepsItsOwnCooldown) {
  constexpr int LONGEST_WAIT = 1'000;
  World world = facing(Cultivating{}, STRIKE);
  for (int wait = 0;
       wait < LONGEST_WAIT &&
       world.cast_refusal(READER, CUT, AtPerson{QI_FOE}) != CastRefusal::NONE;
       ++wait) {
    world.tick();
  }
  ASSERT_EQ(world.cast_refusal(READER, STRIKE, AtPerson{QI_FOE}),
            CastRefusal::NONE);
  world.cast(READER, STRIKE, AtPerson{QI_FOE});
  world.tick();
  EXPECT_EQ(world.cast_refusal(READER, STRIKE, AtPerson{QI_FOE}),
            CastRefusal::ON_COOLDOWN);
  EXPECT_GT(world.ready_at(READER, STRIKE), world.ready_at(READER, CUT));
  // Control: its other art skill, and its army's, are ready.
  EXPECT_EQ(world.cast_refusal(READER, CUT, AtPerson{QI_FOE}),
            CastRefusal::NONE);
  EXPECT_EQ(world.cast_refusal(READER, 0, Itself{}), CastRefusal::NONE);
}

TEST(World, ACastIsRefusedForLackOfQi) {
  Cultivating dear;
  dear.strike_cost = QI_LIMIT + 1;
  World world = read(dear);
  ASSERT_LT(world.person(READER).qi, dear.strike_cost);
  EXPECT_EQ(world.cast_refusal(READER, STRIKE, Itself{}), CastRefusal::NO_QI);
  EXPECT_THROW(world.cast(READER, STRIKE, Itself{}), std::invalid_argument);
  // Control: a strike within its qi may be cast.
  dear.strike_cost = THIRD_RATE;
  const World affordable = read(dear);
  ASSERT_GE(affordable.person(READER).qi, dear.strike_cost);
  EXPECT_EQ(affordable.cast_refusal(READER, STRIKE, Itself{}),
            CastRefusal::NONE);
}

TEST(World, AUseSpendsItsSkillsQi) {
  Cultivating held;
  held.recovery_ticks = NEVER_AGAIN;
  held.founding_age = FOUNDED_DAYS;
  World world = facing(held, STRIKE);
  const std::uint32_t before = world.person(READER).qi;
  world.cast(READER, STRIKE, AtPerson{QI_FOE});
  world.tick();
  EXPECT_EQ(world.person(READER).qi, before - 50U);
  // Control: with the foe gone beyond reach, a tick spends nothing.
  World apart = read(held);
  const std::uint32_t kept = apart.person(READER).qi;
  apart.tick();
  EXPECT_EQ(apart.person(READER).qi, kept);
}

// Cultivating `cultivating`'s world after the reader has read and the foe
// has stood on the field, within the reader's skills' reach, for 100 ticks.
World fought(const Cultivating& cultivating) {
  constexpr int LONGEST_WALK = 1'000;
  constexpr std::uint64_t FIGHT = 100;
  World world = read(cultivating);
  world.order(QI_FOE_ARMY, {QI_FIELD, {0}});
  for (int walked = 0;
       walked < LONGEST_WALK && !in(QI_FIELD, world.person(QI_FOE).position);
       ++walked) {
    world.tick();
  }
  EXPECT_TRUE(in(QI_FIELD, world.person(QI_FOE).position));
  run_ticks(world, FIGHT);
  return world;
}

TEST(World, ThoseWhoKnowAnArtStrikeWithItUnbidden) {
  EXPECT_LT(fought(Cultivating{}).person(QI_FOE).health, QI_FOE_HEALTH);
  // Control: a reader who could not afford to read, under the same order of
  // IDLE alone, hits no one.
  Cultivating costly;
  costly.learning.reading_cost = QI_LIMIT + 1;
  EXPECT_EQ(fought(costly).person(QI_FOE).health, QI_FOE_HEALTH);
}

// Cultivating `cultivating`'s world after the reader has read as
// second-rate and walked back to READER_CELL, beyond the manual's reach,
// and lived 200 more days there.
World walked_away(const Cultivating& cultivating) {
  constexpr std::uint64_t DAYS = 200;
  World world = read(cultivating);
  EXPECT_EQ(world.rank(READER), 2U);
  EXPECT_EQ(known_by(world, READER).size(), 2U);
  world.order(READER_ARMY, {READER_CELL, {0}});
  run_ticks(world, DAYS);
  EXPECT_EQ(world.rank(READER), 3U);
  return world;
}

TEST(World, ARealmGainedComprehendsASkillOfAnArtItKnows) {
  // Reading as second-rate, 300 qi of 800: the strike and the sweep. The
  // form waits for first-rate.
  Cultivating slow;
  slow.growth = {4, 0};
  slow.learning.comprehension = PER_MILLE;
  slow.held = {0};
  EXPECT_EQ(known_by(walked_away(slow), READER).back(),
            (std::pair<std::size_t, std::size_t>{ORTHODOX_ART, 2}));
  // Controls: with no chance, or a form comprehension cannot unlock, it is
  // not learned.
  Cultivating unmoved = slow;
  unmoved.learning.comprehension = 0;
  EXPECT_EQ(known_by(walked_away(unmoved), READER).size(), 2U);
  Cultivating secret = slow;
  secret.form_comprehensible = false;
  EXPECT_EQ(known_by(walked_away(secret), READER).size(), 2U);
}

TEST(World, ASectTeachesItsArtsWithinItsMembersAndEldersRealms) {
  // Teaching at a thousand per mille a year for each rank of the best elder.
  Cultivating taught;
  taught.learning.teaching = PER_MILLE;
  taught.held = {0};
  World world = qi_world(taught);
  run_ticks(world, DAYS_PER_YEAR);
  ASSERT_GE(world.rank(1), 3U);
  EXPECT_FALSE(world.person(1).known.empty());
  EXPECT_FALSE(world.person(2).known.empty());
  // Its arts only, never the demonic art it holds no manual of.
  for (const std::size_t member : {std::size_t{1}, std::size_t{2}}) {
    for (const Known& known : world.person(member).known) {
      EXPECT_EQ(known.art, ORTHODOX_ART);
    }
  }
  // Control: a sect that teaches at no chance teaches no one, and the reader,
  // of no sect, learns nothing.
  EXPECT_TRUE(world.person(READER).known.empty());
  World idle = qi_world(Cultivating{});
  run_ticks(idle, DAYS_PER_YEAR);
  EXPECT_TRUE(idle.person(1).known.empty());
}

// The skills person `person` was noted learning `how`, as (art, skill)
// pairs oldest first, among the progress `world` keeps.
std::vector<std::pair<std::size_t, std::size_t>> recorded_learning(
    const World& world, std::size_t person, LearnedBy how) {
  std::vector<std::pair<std::size_t, std::size_t>> learned;
  for (const Progress& step : world.progress()) {
    const auto* lesson = std::get_if<Learned>(&step.what);
    if (lesson != nullptr && lesson->person == person && lesson->by == how) {
      learned.emplace_back(lesson->art, lesson->skill);
    }
  }
  return learned;
}

// The realm ranks person `person` was noted reaching, oldest first.
std::vector<std::size_t> recorded_realms(const World& world,
                                         std::size_t person) {
  std::vector<std::size_t> ranks;
  for (const Progress& step : world.progress()) {
    const auto* gained = std::get_if<RealmGained>(&step.what);
    if (gained != nullptr && gained->person == person) {
      ranks.push_back(gained->rank);
    }
  }
  return ranks;
}

TEST(World, EveryRealmGainedAndSkillReadFromAManualIsRecorded) {
  const World world = read(Cultivating{});
  ASSERT_EQ(world.rank(READER), 3U);
  EXPECT_EQ(recorded_realms(world, READER),
            (std::vector<std::size_t>{1, 2, 3}));
  EXPECT_EQ(recorded_learning(world, READER, LearnedBy::MANUAL),
            known_by(world, READER));
  // Control: a reader short of the reading's qi gains the same realms and
  // is recorded learning nothing.
  Cultivating costly;
  costly.learning.reading_cost = QI_LIMIT + 1;
  const World unread = read(costly);
  ASSERT_TRUE(known_by(unread, READER).empty());
  EXPECT_EQ(recorded_realms(unread, READER),
            (std::vector<std::size_t>{1, 2, 3}));
  EXPECT_TRUE(recorded_learning(unread, READER, LearnedBy::MANUAL).empty());
}

TEST(World, ASkillComprehendedIsRecordedAsComprehended) {
  Cultivating slow;
  slow.growth = {4, 0};
  slow.learning.comprehension = PER_MILLE;
  slow.held = {0};
  const World comprehending = walked_away(slow);
  ASSERT_EQ(known_by(comprehending, READER).back(),
            (std::pair<std::size_t, std::size_t>{ORTHODOX_ART, 2}));
  EXPECT_EQ(
      recorded_learning(comprehending, READER, LearnedBy::COMPREHENSION),
      (std::vector<std::pair<std::size_t, std::size_t>>{{ORTHODOX_ART, 2}}));
  // Control: with no chance of comprehension, none is recorded.
  Cultivating unmoved = slow;
  unmoved.learning.comprehension = 0;
  EXPECT_TRUE(
      recorded_learning(walked_away(unmoved), READER, LearnedBy::COMPREHENSION)
          .empty());
}

TEST(World, ASkillTaughtIsRecordedAsTaught) {
  Cultivating taught;
  taught.learning.teaching = PER_MILLE;
  taught.held = {0};
  World teaching = qi_world(taught);
  run_ticks(teaching, DAYS_PER_YEAR);
  for (const std::size_t member : {std::size_t{1}, std::size_t{2}}) {
    ASSERT_FALSE(teaching.person(member).known.empty());
    EXPECT_EQ(recorded_learning(teaching, member, LearnedBy::TEACHING),
              known_by(teaching, member));
  }
  // Control: with no chance of teaching, none is recorded.
  World idle = qi_world(Cultivating{});
  run_ticks(idle, DAYS_PER_YEAR);
  EXPECT_TRUE(recorded_learning(idle, 1, LearnedBy::TEACHING).empty());
}

// A person growing a qi a day through `count` realms, one a qi apart, on days
// of one tick, lived as many ticks.
World climbing(std::uint32_t count) {
  Life life = one_discipline(0);
  life.cultivation = Cultivation{{1, 0}, {count, 0}, 0, 1};
  for (std::uint32_t rank = 1; rank <= count; ++rank) {
    life.realms.push_back(Realm{{"경", "Realm"}, rank, PER_MILLE});
  }
  Rules rules = peaceful(0);
  rules.ticks_per_day = 1;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {army(READER_CELL, READER_CELL, 1, 1, 0, {IDLE})},
              {},
              rules,
              {},
              {},
              life};
  run_ticks(world, count);
  return world;
}

// The realm ranks from `first` to `last`, ascending.
std::vector<std::size_t> ranks_from(std::size_t first, std::size_t last) {
  std::vector<std::size_t> ranks;
  for (std::size_t rank = first; rank <= last; ++rank) {
    ranks.push_back(rank);
  }
  return ranks;
}

TEST(World, ProgressKeepsTheLatestStepsApartFromTheEvents) {
  constexpr std::uint32_t CLIMBED = PROGRESS_CAPACITY + 8;
  const World world = climbing(CLIMBED);
  ASSERT_EQ(world.rank(READER), CLIMBED);
  // The latest PROGRESS_CAPACITY realms, numbered on from the first, and no
  // event recorded for any of them.
  EXPECT_EQ(recorded_realms(world, READER),
            ranks_from(CLIMBED - PROGRESS_CAPACITY + 1, CLIMBED));
  EXPECT_EQ(world.progress().front().sequence, CLIMBED - PROGRESS_CAPACITY);
  EXPECT_TRUE(world.events().empty());
}

TEST(World, ProgressShortOfItsCapacityKeepsEveryStep) {
  // Control for the latest kept: one realm short of the capacity keeps every
  // one.
  constexpr std::uint32_t FEWER = PROGRESS_CAPACITY - 1;
  const World world = climbing(FEWER);
  ASSERT_EQ(world.rank(READER), FEWER);
  EXPECT_EQ(recorded_realms(world, READER), ranks_from(1, FEWER));
}

TEST(World, KnownSkillsAndQiArePartOfTheHash) {
  const World one = read(Cultivating{});
  Cultivating costly;
  costly.learning.reading_cost = QI_LIMIT + 1;
  const World other = read(costly);
  ASSERT_NE(one.person(READER).known.size(), other.person(READER).known.size());
  EXPECT_NE(one.hash(), other.hash());
  // Control: the same reading hashes the same.
  EXPECT_EQ(one.hash(), read(Cultivating{}).hash());
}

constexpr Cell SPEAKER_AT{2, 2};
constexpr Cell NEIGHBOUR_AT{4, 2};
constexpr Cell PASSER_BY_AT{5, 2};
constexpr Cell FOE_AT{2, 4};

// Faction 0's speaker at (2, 2) beside faction 1's neighbour 2 cells east,
// faction 1's passer-by 3 cells east and faction 2's foe 2 cells south, faction
// 2 hostile to faction 0 unless `foe_hostile` is false; people talk within 2
// cells, and the speaker leads its faction.
World talking(bool foe_hostile = true) {
  Rules rules{0,
              {{false, false, foe_hostile},
               {false, false, false},
               {false, false, false}},
              STAGE_FLOORS};
  rules.talk_reach = 2;
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {ArmySpec{SPEAKER_AT, {Recruit{SPEAKER_AT, 1}}, 0, {IDLE}},
                ArmySpec{NEIGHBOUR_AT, {Recruit{NEIGHBOUR_AT, 1}}, 1, {IDLE}},
                ArmySpec{PASSER_BY_AT, {Recruit{PASSER_BY_AT, 1}}, 1, {IDLE}},
                ArmySpec{FOE_AT, {Recruit{FOE_AT, 1}}, 2, {IDLE}}},
               {},
               rules,
               {},
               {{0, std::nullopt, std::nullopt, std::nullopt}, {}, {}}};
}

constexpr std::size_t SPEAKER = 0;
constexpr std::size_t NEIGHBOUR = 1;
constexpr std::size_t PASSER_BY = 2;
constexpr std::size_t FOE = 3;

TEST(World, APersonTalksToAnyoneWithinTheTalkReachWhoIsNotHostile) {
  const World world = talking();
  EXPECT_EQ(world.talk_refusal(SPEAKER, NEIGHBOUR), TalkRefusal::NONE);
  // Control: one cell farther is beyond the reach.
  EXPECT_EQ(world.talk_refusal(SPEAKER, PASSER_BY), TalkRefusal::OUT_OF_REACH);
  EXPECT_EQ(world.talk_refusal(SPEAKER, SPEAKER), TalkRefusal::SELF);
  EXPECT_THROW(static_cast<void>(world.talk_refusal(SPEAKER, 99)),
               std::invalid_argument);
}

TEST(World, AHostileFactionsPersonRefusesToTalk) {
  // Faction 2 is hostile to faction 0 by stance, so its person refuses both
  // ways.
  const World world = talking();
  ASSERT_EQ(world.relation(0, 2), Relation::HOSTILE);
  EXPECT_EQ(world.talk_refusal(SPEAKER, FOE), TalkRefusal::HOSTILE);
  EXPECT_EQ(world.talk_refusal(FOE, SPEAKER), TalkRefusal::HOSTILE);
  // Control: the same person at the same place, its faction allied, talks.
  const World allied = talking(false);
  ASSERT_EQ(allied.relation(0, 2), Relation::ALLY);
  EXPECT_EQ(allied.talk_refusal(SPEAKER, FOE), TalkRefusal::NONE);
}

TEST(World, APersonOfAFactionAtWarRefusesToTalk) {
  World world = talking();
  ASSERT_EQ(world.talk_refusal(SPEAKER, NEIGHBOUR), TalkRefusal::NONE);
  world.declare(SPEAKER, 1, Declaration::WAR);
  ASSERT_EQ(world.relation(0, 1), Relation::AT_WAR);
  EXPECT_EQ(world.talk_refusal(SPEAKER, NEIGHBOUR), TalkRefusal::HOSTILE);
  // Control: peace made with the foe's faction lets its person talk.
  World peace = talking();
  ASSERT_EQ(peace.talk_refusal(SPEAKER, FOE), TalkRefusal::HOSTILE);
  peace.declare(SPEAKER, 2, Declaration::PEACE);
  ASSERT_EQ(peace.relation(0, 2), Relation::NEUTRAL);
  EXPECT_EQ(peace.talk_refusal(SPEAKER, FOE), TalkRefusal::NONE);
}

TEST(World, APersonTalksToTheNearestItMayTalkToOrMeetAsAFoe) {
  const World world = talking();
  // The foe, hostile, stands 2 cells from the speaker and the neighbour
  // farther, so the foe is whom it would talk to.
  EXPECT_EQ(world.talk_target(FOE), std::optional<std::size_t>{SPEAKER});
  // The neighbour and the foe stand 2 cells from the speaker: the first in
  // army order.
  EXPECT_EQ(world.talk_target(SPEAKER), std::optional<std::size_t>{NEIGHBOUR});
  // Control: the speaker beyond the passer-by's reach is never its target.
  EXPECT_EQ(world.talk_target(PASSER_BY),
            std::optional<std::size_t>{NEIGHBOUR});
  EXPECT_THROW(static_cast<void>(world.talk_target(99)), std::out_of_range);
}

// Two people of faction 0 side by side, one of whom faction 1's person beside
// them strikes dead on the first tick: {world, the dead, the living}.
struct Struck {
  World world;
  std::size_t dead = 0;
  std::size_t alive = 0;
};

Struck struck_beside() {
  const Skill once{2 * CELL_SIZE, 0, 1, 0, NEVER_AGAIN};
  Rules rules = at_war(0);
  rules.talk_reach = 2;
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              {ArmySpec{{2, 2}, {Recruit{{2, 2}, 1}}, 0, {IDLE}},
               ArmySpec{{2, 3}, {Recruit{{2, 3}, 1}}, 0, {IDLE}},
               ArmySpec{{3, 2}, {Recruit{{3, 2}, 1}}, 1, {once}}},
              {},
              rules};
  run_ticks(world, 1);
  const std::size_t dead = world.person(0).health == 0 ? 0U : 1U;
  return Struck{world, dead, 1U - dead};
}

TEST(World, TheDeadDoNotTalk) {
  const Struck struck = struck_beside();
  ASSERT_EQ(struck.world.person(struck.dead).health, 0U);
  EXPECT_EQ(struck.world.talk_refusal(struck.alive, struck.dead),
            TalkRefusal::DEAD);
  EXPECT_EQ(struck.world.talk_refusal(struck.dead, struck.alive),
            TalkRefusal::DEAD);
}

TEST(World, TheDeadOfferNoNews) {
  const Struck struck = struck_beside();
  ASSERT_EQ(struck.world.person(struck.dead).health, 0U);
  EXPECT_TRUE(struck.world.offers(struck.dead).empty());
  // Control: the living one beside it still offers its news.
  EXPECT_EQ(struck.world.offers(struck.alive), std::vector<Offer>{Offer::NEWS});
  EXPECT_THROW(static_cast<void>(struck.world.offers(99)), std::out_of_range);
}

TEST(World, NoOneIsTalkedToWhenNoneMayBe) {
  const Struck struck = struck_beside();
  ASSERT_EQ(struck.world.person(struck.dead).health, 0U);
  // The dead talk to no one, and the living one's foe beside it, of a
  // faction at war, counts; control: the living one has someone.
  EXPECT_FALSE(struck.world.talk_target(struck.dead).has_value());
  EXPECT_TRUE(struck.world.talk_target(struck.alive).has_value());
}

// Whether a world whose people talk within `reach` cells is created.
bool talks_within(std::int32_t reach) {
  Rules rules = peaceful(0);
  rules.talk_reach = reach;
  try {
    const World world{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                      {},
                      rules};
    return world.living() == 1;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

TEST(World, ATalkReachOutsideItsLimitsIsRefused) {
  EXPECT_FALSE(talks_within(-1));
  EXPECT_FALSE(talks_within(MAX_REACH + 1));
  // Control: the limits themselves are accepted.
  EXPECT_TRUE(talks_within(0));
  EXPECT_TRUE(talks_within(MAX_REACH));
}

constexpr std::uint64_t WHOLE = 100;
constexpr std::uint64_t HURT = 50;
constexpr std::uint64_t PURSE = 30;
constexpr std::uint64_t HERB_PRICE = 10;
constexpr std::uint64_t HERB_HEALTH = 30;
constexpr std::uint64_t PILL_PRICE = 60;
constexpr std::uint32_t PILL_QI = 500;
constexpr std::uint32_t STORE = 1'000;
constexpr std::uint32_t DRAIN = 800;
constexpr std::size_t HERB = 0;
constexpr std::size_t PILL = 1;
// Sold by the salve seller's occupation, which opens no place.
constexpr std::size_t SALVE = 2;
constexpr std::size_t BUYER = 0;
constexpr std::size_t SELLER = 1;
constexpr std::size_t AWAY = 2;
constexpr std::size_t DRAINING = 1;
constexpr Cell BUYER_AT{2, 2};
constexpr Cell SELLER_AT{4, 2};
constexpr Cell AWAY_AT{8, 2};
// Beside the seller, a cell east of it.
constexpr Cell BESIDE_SELLER{5, 2};

// How the market below is set up.
struct Stall {
  std::uint64_t health = WHOLE;
  std::uint64_t purse = PURSE;
  bool steered = true;
  bool homed = false;
  bool foe = false;
};

// Faction 0's buyer at (2, 2), whose second skill drains 800 qi, a seller 2
// cells east and a person away 6 cells east, each with a store of 1,000 qi,
// full. The trade sells a herb restoring 30 health and a pill restoring 500
// qi, and a salve; people use them at half health or a quarter of their
// store. The seller's army are the first village's residents, where the
// herbalist, selling the herb and the pill, opens a place for each of them;
// the salve seller opens none. With `homed` the buyer's army are the second
// village's residents; with `foe`, faction 1's person beside the seller
// strikes it dead on the first tick. Days last a tick, and only the buyer may
// go unsteered.
World market(const Stall& stall) {
  Life life;
  life.disciplines = {{"sword", {0, 0}, {WHOLE, 0}},
                      {"medicine", {0, 0}, {2 * WHOLE, 0}}};
  life.newborn_health = WHOLE;
  life.founding_age = Spread{DAYS_PER_YEAR, 0};
  life.cultivation = Cultivation{{STORE, 0}, {STORE, 0}, 0, 1};
  life.trade = Trade{{Ware{"herb", HERB_PRICE, HERB_HEALTH, 0},
                      Ware{"pill", PILL_PRICE, 0, PILL_QI},
                      Ware{"salve", HERB_PRICE, HERB_HEALTH, 0}},
                     stall.purse,
                     1,
                     PER_MILLE / 2,
                     PER_MILLE / 4};
  const std::vector<Band> day{{0, Where::HOME}};
  life.occupations =
      Occupations{{Occupation{"herbalist", OnSquare{1}, day, {HERB, PILL}},
                   Occupation{"salve_seller", Nowhere{}, day, {SALVE}}},
                  0,
                  1,
                  day,
                  Waking{}};
  Rules rules = stall.foe ? at_war(0) : peaceful(0);
  rules.talk_reach = 2;
  rules.ticks_per_day = 1;
  const Skill drain{0, 0, 0, 0, NEVER_AGAIN, DRAIN};
  std::vector<ArmySpec> armies{
      ArmySpec{BUYER_AT, {Recruit{BUYER_AT, stall.health}}, 0, {IDLE, drain}},
      ArmySpec{SELLER_AT, {Recruit{SELLER_AT, WHOLE}}, 0, {IDLE}},
      ArmySpec{AWAY_AT, {Recruit{AWAY_AT, WHOLE}}, 0, {IDLE}}};
  if (stall.foe) {
    const Skill once{2 * CELL_SIZE, 0, WHOLE, 0, NEVER_AGAIN};
    armies.push_back(
        ArmySpec{BESIDE_SELLER, {Recruit{BESIDE_SELLER, WHOLE}}, 1, {once}});
  }
  std::vector<VillageSpec> villages{VillageSpec{{}, 1, {}, 0, {}, {}}};
  if (stall.homed) {
    villages.push_back(VillageSpec{{}, 0, {}, 0, {}, {}});
  }
  World world{Seed{1},
              Map{open_map(OPEN_SIDE)},
              std::move(armies),
              {},
              rules,
              std::move(villages),
              {},
              life};
  world.order(0, {BUYER_AT, {0}});
  for (std::size_t army = 0; army < world.army_count(); ++army) {
    world.steer(army, army != 0 || stall.steered);
  }
  return world;
}

// The stall's buyer wounded below half its health.
Stall hurt_stall() {
  Stall stall;
  stall.health = HURT;
  return stall;
}

// The stall's wounded buyer steered by no driver.
Stall unsteered_stall() {
  Stall stall = hurt_stall();
  stall.steered = false;
  return stall;
}

// The stall with a purse that pays for the pill.
Stall richer_stall() {
  Stall stall;
  stall.purse = PILL_PRICE;
  return stall;
}

// The stall with the buyer's army at home.
Stall homed_stall() {
  Stall stall;
  stall.homed = true;
  return stall;
}

// The stall with a foe beside the seller.
Stall beset_stall() {
  Stall stall;
  stall.foe = true;
  return stall;
}

TEST(World, APersonSellsTheWaresOfItsOccupation) {
  const World world = market({});
  ASSERT_EQ(world.wares().size(), 3U);
  ASSERT_EQ(world.occupation_of(SELLER), std::optional<std::size_t>{0});
  EXPECT_TRUE(world.sells(SELLER, HERB));
  EXPECT_TRUE(world.sells(SELLER, PILL));
  // Control: the salve is the salve seller's, which is not the seller's.
  EXPECT_FALSE(world.sells(SELLER, SALVE));
  // Control: the person away, with no home, holds no occupation and sells
  // nothing.
  EXPECT_FALSE(world.occupation_of(AWAY).has_value());
  EXPECT_FALSE(world.sells(AWAY, HERB));
  EXPECT_EQ(world.offers(SELLER),
            (std::vector<Offer>{Offer::NEWS, Offer::TRADE}));
  EXPECT_THROW(static_cast<void>(world.sells(SELLER, 3)), std::out_of_range);
}

// A village of three founders at home, a household and one more, with a
// smithy of `durability`; smiths take two places at each standing smithy,
// beggars are the fallback. Days last a tick.
World smithing(std::uint32_t durability) {
  const std::vector<Band> day{{0, Where::HOME}};
  Life life;
  life.newborn_health = WHOLE;
  life.founding_age = Spread{2 * DAYS_PER_YEAR, 0};
  life.occupations =
      Occupations{{Occupation{"smith", AtWorkplace{"smithy", 2}, day, {}},
                   Occupation{"beggar", Nowhere{}, day, {}}},
                  1,
                  1,
                  day,
                  Waking{}};
  Rules rules = peaceful(0);
  rules.ticks_per_day = 1;
  return World{Seed{1},
               Map{open_map(OPEN_SIDE)},
               {army({1, 1}, {1, 1}, 3, WHOLE, 0, {IDLE})},
               {Structure{{1, 4}, durability, {2, 2}, true, Role::SQUARE}},
               rules,
               {VillageSpec{{0}, 0, {}, 1, {}, {"smithy"}}},
               {},
               life};
}

// The occupation each of smithing()'s three founders holds.
std::vector<std::optional<std::size_t>> founders_held(const World& world) {
  return {world.occupation_of(0), world.occupation_of(1),
          world.occupation_of(2)};
}

TEST(World, AVillagesPeopleTakeOccupationsFromItsStandingBuildings) {
  using Held = std::vector<std::optional<std::size_t>>;
  constexpr std::size_t SMITH = 0;
  constexpr std::size_t BEGGAR = 1;
  World world = smithing(STAGE_FLOORS.at(0));
  // The first two take the smithy's places and the third the fallback, from
  // the world's creation and on the next day.
  EXPECT_EQ(founders_held(world), (Held{SMITH, SMITH, BEGGAR}));
  world.tick();
  EXPECT_EQ(founders_held(world), (Held{SMITH, SMITH, BEGGAR}));
  // Control: with the smithy in rubble, no place opens and all three beg.
  const World ruined = smithing(0);
  ASSERT_EQ(ruined.stage(0), Stage::RUBBLE);
  EXPECT_EQ(founders_held(ruined), (Held{BEGGAR, BEGGAR, BEGGAR}));
}

TEST(World, NoOneSellsWithoutATrade) {
  const World world = talking();
  EXPECT_TRUE(world.wares().empty());
  EXPECT_EQ(world.offers(NEIGHBOUR), std::vector<Offer>{Offer::NEWS});
}

TEST(World, ABuyerPaysTheSellerForTheWareItHolds) {
  World world = market({});
  ASSERT_EQ(world.person(BUYER).silver, PURSE);
  ASSERT_EQ(world.buy_refusal(BUYER, SELLER, HERB), BuyRefusal::NONE);
  world.buy(BUYER, SELLER, HERB);
  EXPECT_EQ(world.person(BUYER).silver, PURSE - HERB_PRICE);
  EXPECT_EQ(world.person(SELLER).silver, PURSE + HERB_PRICE);
  EXPECT_EQ(world.person(BUYER).held, (std::vector<std::uint64_t>{1, 0, 0}));
}

TEST(World, APurchaseIsRefusedForItsFirstReason) {
  World world = market({});
  // Control: the herb from the seller within reach is sold.
  ASSERT_EQ(world.buy_refusal(BUYER, SELLER, HERB), BuyRefusal::NONE);
  EXPECT_EQ(world.buy_refusal(BUYER, AWAY, HERB), BuyRefusal::CANNOT_TALK);
  EXPECT_EQ(world.buy_refusal(BUYER, BUYER, HERB), BuyRefusal::CANNOT_TALK);
  EXPECT_EQ(world.buy_refusal(BUYER, SELLER, SALVE), BuyRefusal::NOT_SOLD);
  EXPECT_EQ(world.buy_refusal(BUYER, SELLER, PILL), BuyRefusal::NO_SILVER);
  EXPECT_THROW(world.buy(BUYER, SELLER, PILL), std::invalid_argument);
  EXPECT_EQ(world.person(BUYER).silver, PURSE);
  EXPECT_THROW(static_cast<void>(world.buy_refusal(BUYER, SELLER, 3)),
               std::invalid_argument);
  // The same pill sells to a buyer with its price.
  const World richer = market(richer_stall());
  EXPECT_EQ(richer.buy_refusal(BUYER, SELLER, PILL), BuyRefusal::NONE);
}

TEST(World, TheDeadNeitherSellNorBuy) {
  World world = market(beset_stall());
  run_ticks(world, 1);
  ASSERT_EQ(world.person(SELLER).health, 0U);
  ASSERT_GT(world.person(BUYER).health, 0U);
  EXPECT_FALSE(world.sells(SELLER, HERB));
  EXPECT_EQ(world.buy_refusal(BUYER, SELLER, HERB), BuyRefusal::DEAD);
  EXPECT_EQ(world.consume_refusal(SELLER, HERB), ConsumeRefusal::DEAD);
  EXPECT_TRUE(world.offers(SELLER).empty());
}

TEST(World, AHerbRestoresHealthUpToANewbornsAndNoFurther) {
  World world = market(hurt_stall());
  EXPECT_EQ(world.consume_refusal(BUYER, HERB), ConsumeRefusal::NONE_HELD);
  world.buy(BUYER, SELLER, HERB);
  world.buy(BUYER, SELLER, HERB);
  ASSERT_EQ(world.consume_refusal(BUYER, HERB), ConsumeRefusal::NONE);
  world.consume(BUYER, HERB);
  EXPECT_EQ(world.person(BUYER).health, HURT + HERB_HEALTH);
  // The second herb would pass a newborn's health, and stops there.
  ASSERT_GT(HURT + (2 * HERB_HEALTH), WHOLE);
  world.consume(BUYER, HERB);
  EXPECT_EQ(world.person(BUYER).health, WHOLE);
  EXPECT_EQ(world.person(BUYER).held.at(HERB), 0U);
}

TEST(World, AHerbIsRefusedAtFullHealth) {
  World world = market({});
  world.buy(BUYER, SELLER, HERB);
  EXPECT_EQ(world.consume_refusal(BUYER, HERB), ConsumeRefusal::FULL);
  EXPECT_THROW(world.consume(BUYER, HERB), std::invalid_argument);
  EXPECT_EQ(world.person(BUYER).held.at(HERB), 1U);
  // Control: the same herb held while wounded is consumed.
  World hurt = market(hurt_stall());
  hurt.buy(BUYER, SELLER, HERB);
  EXPECT_EQ(hurt.consume_refusal(BUYER, HERB), ConsumeRefusal::NONE);
}

TEST(World, APillRestoresQiUpToTheStoreAndIsRefusedWhenFull) {
  World world = market(richer_stall());
  world.buy(BUYER, SELLER, PILL);
  ASSERT_EQ(world.person(BUYER).qi, STORE);
  EXPECT_EQ(world.consume_refusal(BUYER, PILL), ConsumeRefusal::FULL);
  world.cast(BUYER, DRAINING, Itself{});
  run_ticks(world, 1);
  ASSERT_EQ(world.person(BUYER).qi, STORE - DRAIN);
  ASSERT_EQ(world.consume_refusal(BUYER, PILL), ConsumeRefusal::NONE);
  world.consume(BUYER, PILL);
  EXPECT_EQ(world.person(BUYER).qi, STORE - DRAIN + PILL_QI);
}

TEST(World, ThoseNoDriverSteersBuyADayAndConsumeWhenWounded) {
  World world = market(unsteered_stall());
  // The first day's shopping buys a herb; the next tick the buyer, below half
  // its health, consumes it.
  run_ticks(world, 1);
  EXPECT_EQ(world.person(BUYER).held, (std::vector<std::uint64_t>{1, 0, 0}));
  EXPECT_EQ(world.person(BUYER).silver, PURSE - HERB_PRICE);
  run_ticks(world, 1);
  EXPECT_EQ(world.person(BUYER).health, HURT + HERB_HEALTH);
  // Above half its health it keeps the next herb it buys.
  EXPECT_EQ(world.person(BUYER).held.at(HERB), 1U);
  // Control: steered, the same buyer buys nothing, and a herb bought for it
  // stays unconsumed.
  World steered = market(hurt_stall());
  run_ticks(steered, 1);
  EXPECT_EQ(steered.person(BUYER).held, (std::vector<std::uint64_t>{0, 0, 0}));
  steered.buy(BUYER, SELLER, HERB);
  run_ticks(steered, 1);
  EXPECT_EQ(steered.person(BUYER).held, (std::vector<std::uint64_t>{1, 0, 0}));
  EXPECT_EQ(steered.person(BUYER).health, HURT);
}

TEST(World, ThoseWithAHomeEarnAWageADay) {
  World world = market(homed_stall());
  ASSERT_TRUE(world.person(BUYER).home.has_value());
  run_ticks(world, 2);
  EXPECT_EQ(world.person(BUYER).silver, PURSE + 2);
  // Control: the person away, with no home, earns nothing.
  ASSERT_FALSE(world.person(AWAY).home.has_value());
  EXPECT_EQ(world.person(AWAY).silver, PURSE);
}

constexpr Cell SHOPPER_AT{4, 4};
// A cell north of the buyer, where a seller lies nearest it.
constexpr Cell NORTH_OF_SHOPPER{4, 3};
// Two cells south and east of the buyer, the edge of its talk reach.
constexpr Cell SOUTH_EAST_OF_SHOPPER{6, 6};
constexpr std::size_t FIRST_SELLER = 1;
constexpr std::size_t SECOND_SELLER = 2;

// Faction 0's buyer at (4, 4), steered by no driver, with a purse for one
// herb, and two steered sellers of it within its talk reach of 2 cells, the
// first at `first` and the second at `second`, each its own village's one
// resident and herbalist. No one earns a wage, and days last a tick.
World two_sellers(Cell first, Cell second) {
  Life life;
  life.newborn_health = WHOLE;
  life.founding_age = Spread{DAYS_PER_YEAR, 0};
  life.trade = Trade{{Ware{"herb", HERB_PRICE, HERB_HEALTH, 0}},
                     HERB_PRICE,
                     0,
                     PER_MILLE / 2,
                     PER_MILLE / 4};
  const std::vector<Band> day{{0, Where::HOME}};
  life.occupations =
      Occupations{{Occupation{"herbalist", OnSquare{1}, day, {HERB}},
                   Occupation{"beggar", Nowhere{}, day, {}}},
                  0,
                  1,
                  day,
                  Waking{}};
  Rules rules = peaceful(0);
  rules.talk_reach = 2;
  rules.ticks_per_day = 1;
  World world{
      Seed{1},
      Map{open_map(OPEN_SIDE)},
      {ArmySpec{SHOPPER_AT, {Recruit{SHOPPER_AT, WHOLE}}, 0, {IDLE}},
       ArmySpec{first, {Recruit{first, WHOLE}}, 0, {IDLE}},
       ArmySpec{second, {Recruit{second, WHOLE}}, 0, {IDLE}}},
      {},
      rules,
      {VillageSpec{{}, 1, {}, 0, {}, {}}, VillageSpec{{}, 2, {}, 0, {}, {}}},
      {},
      life};
  for (std::size_t army = 1; army < world.army_count(); ++army) {
    world.steer(army, true);
  }
  return world;
}

// Whether person `person` of `world` stands in cell `cell`.
bool stands_in(const World& world, std::size_t person, Cell cell) {
  const Cell here = cell_of(world.person(person).position);
  return here.x == cell.x && here.y == cell.y;
}

TEST(World, ABuyerBuysFromTheFirstSellerByIndexInReachNotTheNearest) {
  World world = two_sellers(SOUTH_EAST_OF_SHOPPER, NORTH_OF_SHOPPER);
  ASSERT_EQ(world.buy_refusal(BUYER, FIRST_SELLER, HERB), BuyRefusal::NONE);
  ASSERT_EQ(world.buy_refusal(BUYER, SECOND_SELLER, HERB), BuyRefusal::NONE);
  run_ticks(world, 1);
  ASSERT_TRUE(stands_in(world, BUYER, SHOPPER_AT));
  ASSERT_TRUE(stands_in(world, SECOND_SELLER, NORTH_OF_SHOPPER));
  EXPECT_EQ(world.person(BUYER).held, std::vector<std::uint64_t>{1});
  EXPECT_EQ(world.person(FIRST_SELLER).silver, 2 * HERB_PRICE);
  EXPECT_EQ(world.person(SECOND_SELLER).silver, HERB_PRICE);
  // Control: with the places swapped, the seller nearest the buyer is the
  // first, and it is the one that sells.
  World swapped = two_sellers(NORTH_OF_SHOPPER, SOUTH_EAST_OF_SHOPPER);
  run_ticks(swapped, 1);
  ASSERT_TRUE(stands_in(swapped, FIRST_SELLER, NORTH_OF_SHOPPER));
  EXPECT_EQ(swapped.person(BUYER).held, std::vector<std::uint64_t>{1});
  EXPECT_EQ(swapped.person(FIRST_SELLER).silver, 2 * HERB_PRICE);
  EXPECT_EQ(swapped.person(SECOND_SELLER).silver, HERB_PRICE);
}

TEST(World, SilverAndWaresHeldArePartOfTheHash) {
  const World one = market({});
  World other = market({});
  ASSERT_EQ(one.hash(), other.hash());
  other.buy(BUYER, SELLER, HERB);
  EXPECT_NE(one.hash(), other.hash());
}

// Whether a world given `trade` is created.
bool trades(const Trade& trade) {
  Life life;
  life.disciplines = {{"medicine", {0, 0}, {WHOLE, 0}}};
  life.trade = trade;
  try {
    const World world{Seed{1},
                      Map{open_map(OPEN_SIDE)},
                      {army({1, 1}, {1, 1}, 1, 1, 0, {IDLE})},
                      {},
                      peaceful(0),
                      {},
                      {},
                      life};
    return world.person(0).silver == trade.purse;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

TEST(World, ATradeOutsideItsLimitsIsRefused) {
  const Ware herb{"herb", HERB_PRICE, HERB_HEALTH, 0};
  const Trade sound{{herb}, PURSE, 1, PER_MILLE, PER_MILLE};
  // Control: the limits themselves are accepted.
  EXPECT_TRUE(trades(sound));
  EXPECT_TRUE(trades(Trade{{Ware{"herb", MAX_TALENT, MAX_HEALTH, MAX_TALENT}},
                           MAX_TALENT,
                           MAX_TALENT,
                           PER_MILLE,
                           PER_MILLE}));
  Trade free = sound;
  free.wares.at(0).price = 0;
  EXPECT_FALSE(trades(free));
  Trade lavish = sound;
  lavish.wage = MAX_TALENT + 1;
  EXPECT_FALSE(trades(lavish));
  Trade eager = sound;
  eager.wounded = PER_MILLE + 1;
  EXPECT_FALSE(trades(eager));
}

// The schedule tests' day, 2,400 ticks, so an hour is 100 ticks and a walk
// across their village fits in a band of hours.
constexpr std::uint64_t SCHEDULED_DAY = 2'400;
constexpr std::uint64_t AN_HOUR = SCHEDULED_DAY / HOURS_PER_DAY;
// The hours they read their village at: asleep at home; awake at home before
// work; the smiths' work beginning; late morning; and the afternoon, when
// all are at work.
constexpr std::uint64_t ASLEEP = 5;
constexpr std::uint64_t BEFORE_WORK = 7;
constexpr std::uint64_t WORK_BEGINS = 8;
constexpr std::uint64_t LATE_MORNING = 12;
constexpr std::uint64_t AFTERNOON = 14;
constexpr Cell SCHEDULED_CENTRE{20, 12};
constexpr std::size_t SCHEDULED_WIDTH = 40;
constexpr std::size_t SCHEDULED_HEIGHT = 24;
// The schedule tests' village's structures: a house of two, a house of
// three, a smithy and a field.
constexpr std::array<Structure, 4> SCHEDULED_STRUCTURES{
    {Structure{{10, 4}, 100, {2, 2}, true, Role::HOME},
     Structure{{6, 10}, 100, {2, 2}, true, Role::HOME},
     Structure{{28, 4}, 100, {3, 2}, true, Role::SQUARE},
     Structure{{30, 16}, 100, {4, 4}, false, Role::EDGE}}};
constexpr std::size_t FIRST_HOUSE = 0;
constexpr std::size_t SECOND_HOUSE = 1;
constexpr std::size_t THE_SMITHY = 2;
constexpr std::size_t THE_FIELD = 3;
// Its residents: a married couple, both smiths; a farmer; and a beggar; and
// the person of another army, if any.
constexpr std::size_t FIRST_SMITH = 0;
constexpr std::size_t SECOND_SMITH = 1;
constexpr std::size_t THE_FARMER = 2;
constexpr std::size_t THE_BEGGAR = 3;
constexpr std::size_t INTRUDER = 4;
constexpr std::array<Cell, 4> RESIDENTS_AT{
    {{19, 12}, {21, 12}, {20, 11}, {20, 13}}};
// The occupations' days, and the village's rules: its claim, its square, the
// vicinity it builds in, the sight people engage within, the waking hours,
// and a growth interval no test reaches.
constexpr std::array<Band, 4> SMITH_DAY{{{0, Where::HOME},
                                         {8, Where::WORK},
                                         {16, Where::SQUARE},
                                         {20, Where::HOME}}};
constexpr std::array<Band, 3> FARMER_DAY{
    {{0, Where::HOME}, {6, Where::WORK}, {18, Where::HOME}}};
constexpr std::array<Band, 3> BEGGAR_DAY{
    {{0, Where::HOME}, {8, Where::SQUARE}, {20, Where::HOME}}};
constexpr std::int32_t SCHEDULED_CLAIM = 20;
constexpr Footprint SCHEDULED_SQUARE{5, 5};
constexpr std::int32_t SCHEDULED_VICINITY = 12;
constexpr std::int32_t SCHEDULED_SIGHT = 10;
constexpr Waking SCHEDULED_WAKING{6, 22};
constexpr std::uint32_t NEVER_GROWS = 1'000'000;
constexpr std::uint32_t AMPLE_HARVEST = 1'000;
// The corners away from the village where another army may stand: the
// north-east, out of sight, and the south-east.
constexpr Cell UNSEEN_CORNER{39, 0};
constexpr Cell ALLY_CORNER{38, 22};

std::vector<std::string> scheduled_rows() {
  return {SCHEDULED_HEIGHT, std::string(SCHEDULED_WIDTH, '.')};
}

// How a schedule test's world differs from its village alone: its map; a
// steered person of the hostile faction standing at `foe`, with `foe_skills`;
// a person of the residents' faction standing at `ally`, in an army of its
// own after the foe's; how old the residents are made, in days; and whether
// the village lays a field, its scarcest holding, at each day's rules.
struct Daily {
  std::vector<std::string> rows = scheduled_rows();
  std::optional<Cell> foe;
  std::vector<Skill> foe_skills{IDLE};
  std::optional<Cell> ally;
  std::uint64_t founding_days = 2 * DAYS_PER_YEAR;
  bool builds = false;
  std::optional<Kinship> kinship;
  std::uint64_t ticks_per_day = SCHEDULED_DAY;
};

template <std::size_t N>
std::vector<Band> day_of(const std::array<Band, N>& bands) {
  return {bands.begin(), bands.end()};
}

// The schedule tests' life: their occupations, waking hours and growth.
Life daily_life(const Daily& daily) {
  Life life;
  life.newborn_health = WHOLE;
  life.founding_age =
      Spread{static_cast<std::uint32_t>(daily.founding_days), 0};
  life.kinship = daily.kinship;
  life.occupations = Occupations{
      {Occupation{"smith", AtWorkplace{"smithy", 2}, day_of(SMITH_DAY), {}},
       Occupation{"farmer", AtWorkplace{"field", 1}, day_of(FARMER_DAY), {}},
       Occupation{"beggar", Nowhere{}, day_of(BEGGAR_DAY), {}}},
      1,
      2,
      {{0, Where::HOME}},
      SCHEDULED_WAKING};
  Growth growth;
  growth.interval = NEVER_GROWS;
  growth.claim = SCHEDULED_CLAIM;
  growth.tiers = {Tier{0, SCHEDULED_SQUARE, SCHEDULED_VICINITY, {}}};
  if (daily.builds) {
    Holding harvest;
    harvest.harvest = AMPLE_HARVEST;
    growth.interval = 1;
    growth.pressure = 0;
    growth.plans = {Plan{"field", {2, 2}, false, harvest, Role::EDGE}};
    growth.builds = {0};
    growth.stances = {{Expansion::BUILD}, {}};
  }
  life.growth = growth;
  return life;
}

// A village round SCHEDULED_CENTRE of SCHEDULED_STRUCTURES and four
// residents at its centre, the first two married. Smiths take the smithy's
// two places and work 8 to 16, the farmer the field's one from 6 to 18, and
// the beggar, the fallback, is on the square 8 to 20; each is home
// otherwise, awake 6 to 22. Its claim reaches 20 cells and its square is 5
// by 5. People overlap, and residents reach 2 cells with a harmless touch.
// Residents take an occupation from a year old.
World daily_village(const Daily& daily = {}) {
  const Life life = daily_life(daily);
  Rules rules = at_war(0);
  rules.ticks_per_day = daily.ticks_per_day;
  rules.sight = SCHEDULED_SIGHT;
  const Skill touch{2 * CELL_SIZE, 0, 0, 0, NEVER_AGAIN};
  std::vector<Recruit> residents;
  residents.reserve(RESIDENTS_AT.size());
  for (const Cell cell : RESIDENTS_AT) {
    residents.push_back(Recruit{cell, WHOLE});
  }
  std::vector<ArmySpec> armies{
      ArmySpec{SCHEDULED_CENTRE, residents, 0, {touch}}};
  if (daily.foe) {
    armies.push_back(ArmySpec{
        *daily.foe, {Recruit{*daily.foe, WHOLE}}, 1, daily.foe_skills});
  }
  if (daily.ally) {
    armies.push_back(
        ArmySpec{*daily.ally, {Recruit{*daily.ally, WHOLE}}, 0, {IDLE}});
  }
  Holding house;
  house.housing = 2;
  Holding roomy;
  roomy.housing = 3;
  World world{Seed{1},
              Map{daily.rows},
              armies,
              {SCHEDULED_STRUCTURES.begin(), SCHEDULED_STRUCTURES.end()},
              rules,
              {VillageSpec{{0, 1, 2, 3},
                           RESIDENTS,
                           {house, roomy, {}, {}},
                           1,
                           {},
                           {"house", "house", "smithy", "field"}}},
              {},
              life};
  if (daily.foe) {
    world.steer(1, true);
  }
  return world;
}

// Runs `world` until hour `hour` of the day it is in, or of the next.
void run_to(World& world, std::uint64_t hour) {
  const std::uint64_t ticks = world.ticks_per_day();
  const std::uint64_t day = world.tick_count() / ticks;
  std::uint64_t until = (day * ticks) + (hour * ticks / HOURS_PER_DAY);
  if (until < world.tick_count()) {
    until += ticks;
  }
  run_ticks(world, until - world.tick_count());
}

Cell cell_at(const World& world, std::size_t person) {
  return cell_of(world.person(person).position);
}

// Cells between two cells along the farther axis.
std::int32_t cells_apart(Cell one, Cell other) {
  return std::max(std::abs(one.x - other.x), std::abs(one.y - other.y));
}

// The door of the schedule tests' structure `structure`.
Cell door_of(std::size_t structure) {
  return door(Map{scheduled_rows()}, SCHEDULED_STRUCTURES.at(structure),
              SCHEDULED_CENTRE)
      .value_or(Cell{-1, -1});
}

// How many cells person `person` of `world` stands on over the `hours`
// hours from now, one a tick.
std::size_t cells_stood_on(std::size_t person, World& world,
                           std::uint64_t hours) {
  std::vector<std::pair<std::int32_t, std::int32_t>> stood;
  for (std::uint64_t tick = 0; tick < hours * AN_HOUR; ++tick) {
    world.tick();
    stood.emplace_back(cell_at(world, person).x, cell_at(world, person).y);
  }
  std::sort(stood.begin(), stood.end());
  return static_cast<std::size_t>(std::unique(stood.begin(), stood.end()) -
                                  stood.begin());
}

// The lowest row person `person` of `world` reaches over the `hours` hours
// from now.
std::int32_t lowest_row(std::size_t person, World& world, std::uint64_t hours) {
  std::int32_t lowest = 0;
  for (std::uint64_t tick = 0; tick < hours * AN_HOUR; ++tick) {
    world.tick();
    lowest = std::max(lowest, cell_at(world, person).y);
  }
  return lowest;
}

// Runs `world` while person `person` is at home, for at most one wander
// period.
void run_while_at_home(World& world, std::size_t person) {
  for (std::uint64_t tick = 0; tick < wander_ticks(world.ticks_per_day()) &&
                               world.activity(person) == Activity::AT_HOME;
       ++tick) {
    run_ticks(world, 1);
  }
}

TEST(World, AResidentWalksToItsWorkplaceInItsWorkBandAndWorksThere) {
  World world = daily_village();
  ASSERT_EQ(world.occupation_of(FIRST_SMITH), std::optional<std::size_t>{0});
  ASSERT_EQ(world.occupation_of(THE_BEGGAR), std::optional<std::size_t>{2});
  run_to(world, BEFORE_WORK);
  // Before its work band, awake at home.
  EXPECT_EQ(world.activity(FIRST_SMITH), Activity::AT_HOME);
  EXPECT_LE(cells_apart(cell_at(world, FIRST_SMITH), door_of(FIRST_HOUSE)), 2);
  // It leaves at the start of its first period in the work band, within one
  // period of the band's hour.
  run_to(world, WORK_BEGINS);
  run_while_at_home(world, FIRST_SMITH);
  EXPECT_EQ(world.activity(FIRST_SMITH), Activity::WALKING);
  run_to(world, AFTERNOON);
  EXPECT_EQ(world.activity(FIRST_SMITH), Activity::WORKING);
  EXPECT_LE(cells_apart(cell_at(world, FIRST_SMITH), door_of(THE_SMITHY)), 2);
  // Control: the beggar, in its square band, is idle on the square.
  EXPECT_EQ(world.activity(THE_BEGGAR), Activity::IDLE);
  EXPECT_LE(cells_apart(cell_at(world, THE_BEGGAR), SCHEDULED_CENTRE), 3);
}

// Whether person `person` of `world` is at home within a cell of the door of
// structure `house`.
bool at_home_by(const World& world, std::size_t person, std::size_t house) {
  return world.activity(person) == Activity::AT_HOME &&
         cells_apart(cell_at(world, person), door_of(house)) <= 1;
}

TEST(World, AtNightEachHouseholdIsAtHomeInTheFirstHouseWithRoomForIt) {
  World world = daily_village();
  run_to(world, ASLEEP);
  // The couple fill the first house; the farmer and the beggar, each alone,
  // take the second.
  EXPECT_TRUE(at_home_by(world, FIRST_SMITH, FIRST_HOUSE));
  EXPECT_TRUE(at_home_by(world, SECOND_SMITH, FIRST_HOUSE));
  EXPECT_TRUE(at_home_by(world, THE_FARMER, SECOND_HOUSE));
  EXPECT_TRUE(at_home_by(world, THE_BEGGAR, SECOND_HOUSE));
}

TEST(World, AFarmerWorksOnItsFieldsOwnCells) {
  // How far a smith at work stands from the field.
  constexpr std::int32_t FAR_FROM_FIELD = 5;
  World world = daily_village();
  run_to(world, AFTERNOON);
  const Structure& field = SCHEDULED_STRUCTURES.at(THE_FIELD);
  const Cell farmer = cell_at(world, THE_FARMER);
  EXPECT_EQ(world.activity(THE_FARMER), Activity::WORKING);
  // On the field, or a cell off it as it wanders.
  EXPECT_GE(farmer.x, field.cell.x - 1);
  EXPECT_GE(farmer.y, field.cell.y - 1);
  EXPECT_LE(farmer.x, field.cell.x + field.footprint.width);
  EXPECT_LE(farmer.y, field.cell.y + field.footprint.height);
  // Control: a smith working at the same hour stands by its smithy.
  EXPECT_GT(cells_apart(cell_at(world, FIRST_SMITH), field.cell),
            FAR_FROM_FIELD);
}

TEST(World, AnIdlePersonWandersEveryHourOnlyWhileAwake) {
  // Home by 3, it sleeps until 6; on the square from 8, it is awake there
  // from 10 to 19.
  constexpr std::uint64_t HOME_BY = 3;
  constexpr std::uint64_t SETTLED = 10;
  constexpr std::uint64_t SETTLED_HOURS = 9;
  World world = daily_village();
  run_to(world, HOME_BY);
  EXPECT_EQ(cells_stood_on(THE_BEGGAR, world, ASLEEP - HOME_BY), 1U);
  run_to(world, SETTLED);
  EXPECT_GT(cells_stood_on(THE_BEGGAR, world, SETTLED_HOURS), 1U);
}

TEST(World, NoneButAnUnsteeredVillagesResidentsBoundForItsCentreFollow) {
  // A cell beyond the centre, south of it.
  constexpr Cell SOUTH{20, 20};
  // Control: the residents follow, the smith at its smithy by 14.
  World following = daily_village();
  run_to(following, AFTERNOON);
  ASSERT_EQ(following.activity(FIRST_SMITH), Activity::WORKING);
  // Steered, they follow no schedule.
  World steered = daily_village();
  steered.steer(RESIDENTS, true);
  run_to(steered, AFTERNOON);
  EXPECT_EQ(steered.activity(FIRST_SMITH), Activity::IDLE);
  EXPECT_GT(cells_apart(cell_at(steered, FIRST_SMITH), door_of(THE_SMITHY)), 3);
  // Bound beyond the centre, they follow their order.
  World elsewhere = daily_village();
  elsewhere.order(RESIDENTS, Order{SOUTH, {0}});
  run_to(elsewhere, AFTERNOON);
  EXPECT_EQ(elsewhere.activity(FIRST_SMITH), Activity::IDLE);
  EXPECT_GT(cells_apart(cell_at(elsewhere, FIRST_SMITH), door_of(THE_SMITHY)),
            3);
}

TEST(World, ResidentsChasingAnAttackIntentFollowNoSchedule) {
  // A foe stands east of the centre, within the rules' sight.
  constexpr Cell EAST{29, 12};
  Daily daily;
  daily.foe = EAST;
  World chasing = daily_village(daily);
  ASSERT_EQ(chasing.attack_refusal(RESIDENTS, INTRUDER), AttackRefusal::NONE);
  chasing.order(RESIDENTS, Order{SCHEDULED_CENTRE, {0}, INTRUDER});
  run_ticks(chasing, AN_HOUR);
  EXPECT_EQ(chasing.activity(FIRST_SMITH), Activity::IDLE);
  // A person of no village follows no schedule either.
  EXPECT_EQ(chasing.activity(INTRUDER), Activity::IDLE);
  // Control: with no intent, the same residents follow theirs, closing on
  // the foe in their claim as the schedule's pull gives way, and so are not
  // idle by it.
  World resting = daily_village(daily);
  run_ticks(resting, AN_HOUR);
  EXPECT_NE(resting.activity(FIRST_SMITH), Activity::IDLE);
}

TEST(World, AWalkToWorkGoesRoundAWallByItsPlacesField) {
  // A wall down column 24 from the top to row 10 stands between the first
  // house and the smithy; the houses' rows end at 7.
  constexpr std::size_t WALL_COLUMN = 24;
  constexpr std::size_t WALL_FOOT = 10;
  constexpr std::int32_t HOUSE_ROWS_END = 7;
  constexpr std::uint64_t WALK_HOURS = AFTERNOON - WORK_BEGINS;
  std::vector<std::string> walled = scheduled_rows();
  for (std::size_t row = 0; row <= WALL_FOOT; ++row) {
    walled.at(row).at(WALL_COLUMN) = '#';
  }
  Daily daily;
  daily.rows = walled;
  World world = daily_village(daily);
  run_to(world, WORK_BEGINS);
  EXPECT_GT(lowest_row(FIRST_SMITH, world, WALK_HOURS),
            static_cast<std::int32_t>(WALL_FOOT));
  EXPECT_EQ(world.activity(FIRST_SMITH), Activity::WORKING);
  // Control: on open ground the walk keeps to the houses' rows.
  World open = daily_village();
  run_to(open, WORK_BEGINS);
  EXPECT_LE(lowest_row(FIRST_SMITH, open, WALK_HOURS), HOUSE_ROWS_END);
}

TEST(World, ClosingOnAFoeOverridesTheSchedule) {
  // A foe stands four cells from the first house's door, inside the claim.
  const Cell beside{door_of(FIRST_HOUSE).x + 4, door_of(FIRST_HOUSE).y};
  Daily daily;
  daily.foe = beside;
  World engaging = daily_village(daily);
  run_to(engaging, ASLEEP);
  EXPECT_LE(cells_apart(cell_at(engaging, FIRST_SMITH), beside), 2);
  // Control: with no foe, the smith sleeps at its door.
  World asleep = daily_village();
  run_to(asleep, ASLEEP);
  EXPECT_LE(cells_apart(cell_at(asleep, FIRST_SMITH), door_of(FIRST_HOUSE)), 1);
}

TEST(World, EveryPlacesFieldIsBuiltAsTheWorldIsCreatedAtThreeBytesACell) {
  // The square, the first house and the second, the smithy and the field:
  // each field over the window from (2, 0) to (38, 24), 4 cells round
  // columns 6 to 33 and rows 4 to 19, the map's edge cutting it at row 24.
  constexpr std::size_t PLACES = 5;
  constexpr std::size_t WINDOW_CELLS = std::size_t{36} * 24;
  constexpr std::size_t BYTES_A_CELL = 3;
  World world = daily_village();
  const World::FieldUse made = world.schedule_fields();
  EXPECT_EQ(made.fields, PLACES);
  EXPECT_EQ(made.builds, PLACES);
  EXPECT_EQ(made.bytes, PLACES * WINDOW_CELLS * BYTES_A_CELL);
  // Control: a day of errands to every place builds no field again.
  run_ticks(world, SCHEDULED_DAY);
  EXPECT_EQ(world.schedule_fields().fields, PLACES);
  EXPECT_EQ(world.schedule_fields().builds, PLACES);
}

TEST(World, ResidentsComingOfAgeAtTheDaysRulesGoToWorkThatDay) {
  // A day short of the working age as the world is created, they hold no
  // occupation and stay home by the idle schedule.
  Daily daily;
  daily.founding_days = DAYS_PER_YEAR - 1;
  World world = daily_village(daily);
  ASSERT_FALSE(world.occupation_of(FIRST_SMITH).has_value());
  run_to(world, AFTERNOON);
  EXPECT_EQ(world.activity(FIRST_SMITH), Activity::AT_HOME);
  // The next day's rules give them their occupations, and by 14 the smith
  // works at the smithy.
  run_ticks(world, SCHEDULED_DAY);
  ASSERT_EQ(world.occupation_of(FIRST_SMITH), std::optional<std::size_t>{0});
  EXPECT_EQ(world.activity(FIRST_SMITH), Activity::WORKING);
  EXPECT_LE(cells_apart(cell_at(world, FIRST_SMITH), door_of(THE_SMITHY)), 2);
}

TEST(World, SmithsWhoseSmithyFallsMidDayGoToTheSquare) {
  // A foe out of sight in the north-east corner, striking no one by itself,
  // breaks the smithy with one cast of its second skill at 12.
  constexpr std::int32_t WRECKING_REACH = 12;
  constexpr std::int32_t WRECKING_DAMAGE = 1'000;
  constexpr Cell IN_THE_SMITHY{29, 4};
  const Skill wrecking{WRECKING_REACH * CELL_SIZE, 2 * CELL_SIZE, 0,
                       WRECKING_DAMAGE, NEVER_AGAIN};
  Daily daily;
  daily.foe = UNSEEN_CORNER;
  daily.foe_skills = {IDLE, wrecking};
  World wrecked = daily_village(daily);
  run_to(wrecked, LATE_MORNING);
  ASSERT_EQ(wrecked.activity(FIRST_SMITH), Activity::WORKING);
  ASSERT_EQ(wrecked.stage(THE_SMITHY), Stage::INTACT);
  wrecked.cast(INTRUDER, 1, Point{centre_of(IN_THE_SMITHY)});
  run_to(wrecked, AFTERNOON);
  ASSERT_EQ(wrecked.stage(THE_SMITHY), Stage::RUBBLE);
  EXPECT_EQ(wrecked.activity(FIRST_SMITH), Activity::IDLE);
  EXPECT_LE(cells_apart(cell_at(wrecked, FIRST_SMITH), SCHEDULED_CENTRE), 3);
  // Control: with no cast, the same smith works on at its smithy.
  World standing = daily_village(daily);
  run_to(standing, AFTERNOON);
  EXPECT_EQ(standing.activity(FIRST_SMITH), Activity::WORKING);
}

TEST(World, AResidentConscriptedAwayLeavesItsHouseToBeFilledAgain) {
  // An army of the residents' faction stands in the south-east corner.
  constexpr std::size_t ALLY_ARMY = 1;
  Daily daily;
  daily.ally = ALLY_CORNER;
  World conscripted = daily_village(daily);
  run_ticks(conscripted, AN_HOUR);
  conscripted.conscript(SECOND_SMITH, ALLY_ARMY);
  run_to(conscripted, ASLEEP);
  // With the wife gone, the husband lives alone, and the farmer joins him in
  // the first house.
  EXPECT_EQ(conscripted.activity(THE_FARMER), Activity::AT_HOME);
  EXPECT_LE(cells_apart(cell_at(conscripted, THE_FARMER), door_of(FIRST_HOUSE)),
            1);
  // Control: with no one conscripted, the farmer sleeps in the second house.
  World whole = daily_village(daily);
  run_to(whole, ASLEEP);
  EXPECT_LE(cells_apart(cell_at(whole, THE_FARMER), door_of(SECOND_HOUSE)), 1);
}

// The hours either side of midnight the schedule tests read lodging and
// errands at, everyone asleep at home.
constexpr std::uint64_t LATE = 23;
constexpr std::uint64_t SMALL_HOURS = 2;

TEST(World, AnUnchangedNightFindsNoLodgingOrErrandAgain) {
  // Asleep from 22, everyone stays home through midnight and the day's rules,
  // which change no occupation and marry no one.
  World night = daily_village();
  run_to(night, LATE);
  const World::FieldUse before = night.schedule_fields();
  run_to(night, SMALL_HOURS);
  EXPECT_EQ(night.schedule_fields().lodgings, before.lodgings);
  EXPECT_EQ(night.schedule_fields().errands, before.errands);
  // Control: waking at 6, each finds its errand again.
  run_to(night, BEFORE_WORK);
  EXPECT_GT(night.schedule_fields().errands, before.errands);
}

// The most errands village `village` finds again in one tick of the wander
// period from the start of hour `hour`, and all it finds in that period.
std::pair<std::uint64_t, std::uint64_t> found_from(World& village,
                                                   std::uint64_t hour) {
  run_to(village, hour);
  const std::uint64_t before = village.schedule_fields().errands;
  std::uint64_t most = 0;
  for (std::uint64_t tick = 0; tick < wander_ticks(village.ticks_per_day());
       ++tick) {
    const std::uint64_t was = village.schedule_fields().errands;
    run_ticks(village, 1);
    most = std::max(most, village.schedule_fields().errands - was);
  }
  return {most, village.schedule_fields().errands - before};
}

// An hour at which some of a village's residents change, and how many.
struct Change {
  std::uint64_t hour;
  std::uint64_t residents;
};

// Expects the residents of `village` who change at `change` to find their
// errands again over the wander period from its hour, fewer than all of
// them in any one tick.
void expect_spread(World& village, Change change) {
  const auto [most, all] = found_from(village, change.hour);
  EXPECT_LT(most, change.residents);
  EXPECT_GE(all, change.residents);
}

// Expects the residents of `village` to change band, wake and fall asleep
// each at its own phase, each within an hour of the band's: at 6 the farmer
// goes to work and the other three wake at home; at 8 the smiths go to work
// and the beggar to the square; at 22 all four fall asleep. Without the
// phase, every one that changes would find its errand again in the same
// tick; with a period other than an hour of the day, some would not within
// the hour (the mutations in #387).
void expect_own_phases(World& village) {
  constexpr std::uint64_t RESIDENTS = 4;
  constexpr std::uint64_t TO_WORK = 8;
  expect_spread(village, Change{SCHEDULED_WAKING.from, RESIDENTS});
  expect_spread(village, Change{TO_WORK, RESIDENTS - 1});
  expect_spread(village, Change{SCHEDULED_WAKING.to, RESIDENTS});
  // Control: asleep, none of them finds an errand again through the next
  // period, as each awake did once a period.
  const std::uint64_t asleep = village.schedule_fields().errands;
  run_ticks(village, wander_ticks(village.ticks_per_day()));
  EXPECT_EQ(village.schedule_fields().errands, asleep);
}

TEST(World, ResidentsChangeBandWakeAndFallAsleepAtTheirOwnPhase) {
  World village = daily_village();
  expect_own_phases(village);
}

TEST(World, ResidentsKeepTheirOwnPhaseWithinAnHourOfAShortDay) {
  // A day of 240 ticks: a wander period of 10.
  constexpr std::uint64_t SHORT_DAY = 240;
  Daily daily;
  daily.ticks_per_day = SHORT_DAY;
  World village = daily_village(daily);
  expect_own_phases(village);
}

// Two villages of the same faction 20 cells apart on the schedule tests'
// map, each of one resident, a single of a family of its own, and a house;
// anyone may marry, at the most chance there is.
World two_singles() {
  constexpr std::array<Cell, 2> CENTRES{{{10, 12}, {30, 12}}};
  constexpr std::int32_t HOUSE_OFF = 2;
  Daily daily;
  daily.kinship =
      Kinship{Range{0, NEVER_GROWS}, SCHEDULED_CLAIM, PER_MILLE, {}, 0, 0};
  Rules rules = peaceful(0);
  rules.ticks_per_day = SCHEDULED_DAY;
  rules.sight = SCHEDULED_SIGHT;
  Holding house;
  house.housing = 2;
  std::vector<ArmySpec> armies;
  std::vector<Structure> structures;
  std::vector<VillageSpec> villages;
  for (const Cell centre : CENTRES) {
    villages.push_back(VillageSpec{
        {structures.size()}, armies.size(), {house}, 0, {}, {"house"}});
    armies.push_back(ArmySpec{centre, {Recruit{centre, WHOLE}}, 0, {IDLE}});
    structures.push_back(Structure{{centre.x + HOUSE_OFF, centre.y + HOUSE_OFF},
                                   HOME_DURABILITY,
                                   {2, 2},
                                   true,
                                   Role::HOME});
  }
  return World{
      Seed{1}, Map{scheduled_rows()}, armies, structures, rules, villages,
      {},      daily_life(daily)};
}

// Lets nights pass in `world`, at most 1,000, until person `person` marries,
// expecting a night with no marriage to find no lodging and the night of the
// marriage to find two, one for each spouse's army, as two_singles() makes
// them; returns the nights with none.
std::uint64_t nights_until_wed(World& world, std::size_t person) {
  constexpr std::uint64_t MOST_NIGHTS = 1'000;
  constexpr std::uint64_t BOTH = 2;
  std::uint64_t quiet = 0;
  for (std::uint64_t night = 0;
       night < MOST_NIGHTS && world.person(person).spouses.empty(); ++night) {
    run_to(world, LATE);
    const std::uint64_t before = world.schedule_fields().lodgings;
    run_to(world, SMALL_HOURS);
    const bool wed = !world.person(person).spouses.empty();
    EXPECT_EQ(world.schedule_fields().lodgings, before + (wed ? BOTH : 0));
    quiet += wed ? 0 : 1;
  }
  return quiet;
}

TEST(World, AMarriageFindsItsCouplesLodgingsAgain) {
  // Nights pass until the two singles marry. The night they do finds each
  // village's lodging, one for each spouse's army.
  constexpr std::size_t FIRST = 0;
  constexpr std::size_t SECOND = 1;
  World wed = two_singles();
  ASSERT_NE(wed.person(FIRST).sex, wed.person(SECOND).sex);
  const std::uint64_t quiet = nights_until_wed(wed, FIRST);
  ASSERT_EQ(wed.person(FIRST).spouses, std::vector<std::size_t>{SECOND});
  // Control: the nights before it, with no marriage, found none.
  EXPECT_GT(quiet, 0U);
}

TEST(World, ADeathAwayFindsItsSpousesLodgingAgain) {
  // The second smith serves in an ally's army in the south-east corner, where
  // a foe beside it, out of the village's sight, strikes down whoever comes
  // within 5 cells.
  constexpr std::size_t ALLY_ARMY = 2;
  constexpr Cell BESIDE_THE_ALLY{39, 23};
  constexpr std::int32_t KILLING_REACH = 5;
  const Skill killing{KILLING_REACH * CELL_SIZE, 0, WHOLE, 0, 0};
  const auto away = [&](const Skill& foe_skill) {
    Daily daily;
    daily.foe = BESIDE_THE_ALLY;
    daily.foe_skills = {foe_skill};
    daily.ally = ALLY_CORNER;
    World world = daily_village(daily);
    world.conscript(SECOND_SMITH, ALLY_ARMY);
    return world;
  };
  World widowed = away(killing);
  std::uint64_t before = 0;
  for (std::uint64_t tick = 0;
       tick < SCHEDULED_DAY && widowed.person(SECOND_SMITH).health > 0;
       ++tick) {
    before = widowed.schedule_fields().lodgings;
    widowed.tick();
  }
  ASSERT_EQ(widowed.person(SECOND_SMITH).health, 0U);
  ASSERT_GT(widowed.person(FIRST_SMITH).health, 0U);
  // Her husband, left unwed in the village, may join his parents'
  // household, so its lodging is found again in the tick she dies.
  EXPECT_EQ(widowed.schedule_fields().lodgings, before + 1);
  // Control: with a foe that strikes no one, she lives, and the same ticks
  // find no lodging after the first.
  World spared = away(IDLE);
  spared.tick();
  const std::uint64_t first = spared.schedule_fields().lodgings;
  run_ticks(spared, widowed.tick_count() - spared.tick_count());
  ASSERT_GT(spared.person(SECOND_SMITH).health, 0U);
  EXPECT_EQ(spared.schedule_fields().lodgings, first);
}

TEST(World, AFollowerOutsideItsVillagesWindowWalksInByItsArmysField) {
  // On a map 80 cells wide, a wall down column 50 leaves a gap in its bottom
  // two rows only. A person of the residents' faction east of it is
  // conscripted to the residents, whose village and its window lie west of
  // the wall.
  constexpr std::size_t WIDE = 80;
  constexpr std::size_t WALL = 50;
  constexpr std::size_t GAP_FROM = 22;
  constexpr Cell EAST_OF_THE_WALL{70, 12};
  constexpr std::size_t THE_RESIDENTS = 0;
  constexpr std::uint64_t HALF_A_DAY = SCHEDULED_DAY / 2;
  Daily daily;
  daily.rows =
      std::vector<std::string>(SCHEDULED_HEIGHT, std::string(WIDE, '.'));
  for (std::size_t row = 0; row < GAP_FROM; ++row) {
    daily.rows.at(row).at(WALL) = '#';
  }
  daily.ally = EAST_OF_THE_WALL;
  World joined = daily_village(daily);
  joined.conscript(INTRUDER, THE_RESIDENTS);
  run_ticks(joined, HALF_A_DAY);
  // Its army's field takes it round the wall through the gap, where a straight
  // walk toward its spot would stop at the wall.
  EXPECT_LT(cell_at(joined, INTRUDER).x, static_cast<std::int32_t>(WALL));
  // Control: not conscripted, it holds east of the wall with its own army.
  World apart = daily_village(daily);
  run_ticks(apart, HALF_A_DAY);
  EXPECT_GT(cell_at(apart, INTRUDER).x, static_cast<std::int32_t>(WALL));
}

// Ticks `world` until it holds more structures than `before`, for at most
// `most` ticks.
void run_until_laid(World& world, std::size_t before, std::uint64_t most) {
  for (std::uint64_t tick = 0; tick < most && world.structure_count() == before;
       ++tick) {
    world.tick();
  }
}

TEST(World, AStructureGivenToAVillageRebuildsItsFieldsInTheTickItIsLaid) {
  // Its places as created: the square and four structures; and with the
  // field the day's rules lay.
  constexpr std::size_t PLACES = 5;
  constexpr std::size_t WITH_THE_FIELD = 6;
  Daily daily;
  daily.builds = true;
  World built = daily_village(daily);
  run_ticks(built, SCHEDULED_DAY - 1);
  ASSERT_EQ(built.schedule_fields().builds, PLACES);
  const std::size_t before = built.structure_count();
  run_until_laid(built, before, AN_HOUR);
  ASSERT_EQ(built.structure_count(), before + 1);
  // Built in the tick that laid it, which a blocking lay already makes long,
  // rather than in a tick of their own.
  EXPECT_EQ(built.schedule_fields().fields, WITH_THE_FIELD);
  EXPECT_EQ(built.schedule_fields().builds, PLACES + WITH_THE_FIELD);
  // Control: a village that lays nothing builds none again.
  World kept = daily_village();
  run_ticks(kept, SCHEDULED_DAY - 1 + AN_HOUR);
  ASSERT_EQ(kept.structure_count(), before);
  EXPECT_EQ(kept.schedule_fields().builds, PLACES);
}

// The schedule tests' village, at peace, beside a second village of the
// other faction round `builder_centre` on a map 80 cells wide: one resident
// and a house of its own, 3 cells south-east of its centre. Only the second
// village's faction builds, laying a field 6 cells west and 3 south of its
// centre at each day's rules. Each village's square is `square`.
World beside_a_builder(Cell builder_centre,
                       Footprint square = SCHEDULED_SQUARE) {
  constexpr std::size_t WIDE = 80;
  constexpr std::int32_t HOUSE_OFF = 3;
  Life life = daily_life(Daily{});
  Growth growth = life.growth.value_or(Growth{});
  Holding harvest;
  harvest.harvest = AMPLE_HARVEST;
  growth.interval = 1;
  growth.pressure = 0;
  growth.plans = {Plan{"field", {2, 2}, false, harvest, Role::EDGE}};
  growth.builds = {0};
  growth.stances = {{}, {Expansion::BUILD}};
  growth.tiers.at(0).square = square;
  life.growth = growth;
  Rules rules = peaceful(0);
  rules.hostile = {{false, false}, {false, false}};
  rules.ticks_per_day = SCHEDULED_DAY;
  rules.sight = SCHEDULED_SIGHT;
  const Skill touch{2 * CELL_SIZE, 0, 0, 0, NEVER_AGAIN};
  std::vector<Recruit> residents;
  residents.reserve(RESIDENTS_AT.size());
  for (const Cell cell : RESIDENTS_AT) {
    residents.push_back(Recruit{cell, WHOLE});
  }
  const std::vector<ArmySpec> armies{
      ArmySpec{SCHEDULED_CENTRE, residents, 0, {touch}},
      ArmySpec{builder_centre, {Recruit{builder_centre, WHOLE}}, 1, {touch}}};
  Holding house;
  house.housing = 2;
  Holding roomy;
  roomy.housing = 3;
  std::vector<Structure> structures{SCHEDULED_STRUCTURES.begin(),
                                    SCHEDULED_STRUCTURES.end()};
  structures.push_back(
      Structure{{builder_centre.x + HOUSE_OFF, builder_centre.y + HOUSE_OFF},
                HOME_DURABILITY,
                {2, 2},
                true,
                Role::HOME});
  return World{
      Seed{1},
      Map{std::vector<std::string>(SCHEDULED_HEIGHT, std::string(WIDE, '.'))},
      armies,
      structures,
      rules,
      {VillageSpec{{0, 1, 2, 3},
                   0,
                   {house, roomy, {}, {}},
                   1,
                   {},
                   {"house", "house", "smithy", "field"}},
       VillageSpec{{4}, 1, {house}, 0, {}, {"house"}}},
      {},
      life};
}

TEST(World, AStructureLaidOnAnotherVillagesWindowRebuildsItsFields) {
  // The first village's window ends at column 37. A builder round (43, 12)
  // lays its field at (37, 15), on that window; one round (44, 12) at
  // (38, 15), just past it.
  constexpr std::size_t LAID = 5;
  constexpr std::int32_t ON_THE_WINDOW = 37;
  constexpr std::uint64_t FIRST_PLACES = 5;
  constexpr std::uint64_t BUILDER_PLACES = 2;
  constexpr Cell NEAR_CENTRE{43, 12};
  constexpr Cell FAR_CENTRE{44, 12};
  World near = beside_a_builder(NEAR_CENTRE);
  const std::uint64_t made = near.schedule_fields().builds;
  ASSERT_EQ(made, FIRST_PLACES + BUILDER_PLACES);
  run_ticks(near, SCHEDULED_DAY + 1);
  ASSERT_EQ(near.structure(LAID).cell.x, ON_THE_WINDOW);
  // Both build again: the builder for its new place, the first village for
  // the field on its window.
  EXPECT_EQ(near.schedule_fields().builds,
            made + FIRST_PLACES + BUILDER_PLACES + 1);
  // Control: laid just past the window, only the builder builds again.
  World far = beside_a_builder(FAR_CENTRE);
  run_ticks(far, SCHEDULED_DAY + 1);
  ASSERT_EQ(far.structure(LAID).cell.x, ON_THE_WINDOW + 1);
  EXPECT_EQ(far.schedule_fields().builds, made + BUILDER_PLACES + 1);
}

TEST(World, AStructureGivenToAVillageOutsideItsWindowRebuildsItsFields) {
  // With squares of one cell, a builder round (60, 12) has a window from
  // column 56, and lays its field 6 cells west, outside it and far from the
  // first village's window: no layout changes, but its places do.
  constexpr std::size_t LAID = 5;
  constexpr std::int32_t WINDOW_FROM = 56;
  constexpr std::uint64_t BUILDER_PLACES = 2;
  constexpr Cell BUILDER_CENTRE{60, 12};
  World builder = beside_a_builder(BUILDER_CENTRE, {1, 1});
  const std::uint64_t made = builder.schedule_fields().builds;
  run_ticks(builder, SCHEDULED_DAY - 1);
  // Control: before the lay, nothing builds again.
  ASSERT_EQ(builder.schedule_fields().builds, made);
  run_ticks(builder, 2);
  ASSERT_LE(builder.structure(LAID).cell.x + 2, WINDOW_FROM);
  EXPECT_EQ(builder.schedule_fields().builds, made + BUILDER_PLACES + 1);
}

TEST(World, APersonConscriptedIntoTheResidentsMidDayFollowsTheirSchedule) {
  // A person of the residents' faction, in an army of its own, stands in the
  // south-east corner; the walk home takes it until 9.
  constexpr std::size_t ALLY_ARMY = 1;
  constexpr std::size_t NEWCOMER = 4;
  constexpr std::uint64_t HOME_BY = 9;
  Daily daily;
  daily.ally = ALLY_CORNER;
  World world = daily_village(daily);
  run_ticks(world, AN_HOUR);
  EXPECT_EQ(world.activity(NEWCOMER), Activity::IDLE);
  world.conscript(NEWCOMER, RESIDENTS);
  ASSERT_NE(world.roster(ALLY_ARMY).size(), 1U);
  run_to(world, HOME_BY);
  // With no occupation until the day's rules, it keeps the idle schedule at
  // home, as a household of its own in the second house, which has room for
  // a third.
  EXPECT_EQ(world.activity(NEWCOMER), Activity::AT_HOME);
  EXPECT_LE(cells_apart(cell_at(world, NEWCOMER), door_of(SECOND_HOUSE)), 1);
}

TEST(World, AWorldGivenItsMapsBareGraphRebuildsItWhereItsStructuresBlock) {
  constexpr std::size_t SIDE = 96;
  const std::vector<std::string> rows = open_map(SIDE);
  // Two blocking structures astride area borders, cutting entrances, and one
  // that blocks nothing.
  const std::vector<Structure> structures{
      Structure{{AREA_SIDE - 1, 40}, INTACT, {2, 2}, true},
      Structure{{(2 * AREA_SIDE) - 1, 10}, INTACT, {2, 2}, true},
      Structure{{10, 70}, INTACT, {2, 2}, false}};
  const ArmySpec walkers =
      army(rows, Cell{5, 5}, Cell{90, 90}, 1, 1, 0, {IDLE});
  const CoarseGraph bare{Map{rows}};

  const World given{Seed{1}, Map{rows}, {walkers}, structures, peaceful(0),
                    {},      {},        {},        {},         bare};
  const World built{Seed{1}, Map{rows}, {walkers}, structures, peaceful(0)};

  // The structures change the graph, so the bare one matches only once it is
  // rebuilt under them.
  ASSERT_FALSE(bare == built.graph());
  EXPECT_TRUE(given.graph() == built.graph());
}

// A map of two rows of ten areas, open but for `structures`, with army 0
// steered at its east end on the low profile, and armies bound east and south
// from its first area, which its tiers then make far.
World far_from_the_first_area(std::vector<Structure> structures) {
  constexpr std::int32_t WIDE = 10 * AREA_SIDE;
  constexpr std::int32_t TALL = 2 * AREA_SIDE;
  constexpr std::int32_t INSET = 5;
  const std::vector<std::string> rows(static_cast<std::size_t>(TALL),
                                      std::string(WIDE, '.'));
  World world{Seed{1},
              Map{rows},
              {army(rows, Cell{WIDE - INSET, INSET}, Cell{WIDE - INSET, INSET},
                    1, 1, 0, {IDLE}),
               army(rows, Cell{INSET, INSET},
                    Cell{(2 * AREA_SIDE) + INSET, INSET}, 1, 1, 0, {IDLE}),
               army(rows, Cell{INSET, 2 * INSET},
                    Cell{INSET, AREA_SIDE + INSET}, 1, 1, 0, {IDLE})},
              std::move(structures),
              peaceful(0)};
  world.set_profile(LOW_PROFILE);
  world.steer(0, true);
  world.retier();
  return world;
}

TEST(World, AFarArmysNodeIsTheExitItsAnchorLeadsOutByTowardItsObjective) {
  const World world = far_from_the_first_area({});
  ASSERT_FALSE(world.far(0).has_value());
  ASSERT_TRUE(world.far(1).has_value());
  ASSERT_TRUE(world.far(2).has_value());

  const std::optional<NodeRef> east = world.far_node(1);
  const std::optional<NodeRef> south = world.far_node(2);

  ASSERT_TRUE(east.has_value());
  ASSERT_TRUE(south.has_value());
  EXPECT_EQ(east.value_or(NodeRef{}).area, 0U);
  EXPECT_EQ(world.graph().node(east.value_or(NodeRef{})).cell.x, AREA_SIDE - 1);
  // Control: bound south from the same area, it leads out by the bottom.
  EXPECT_EQ(world.graph().node(south.value_or(NodeRef{})).cell.y,
            AREA_SIDE - 1);
  EXPECT_FALSE(world.far_node(0).has_value());
}

TEST(World, AFarArmysNodeFollowsItsExitWhenItsAreasNodesAreNumberedAfresh) {
  // A house on the first area's bottom border splits its bottom entrance in
  // two, so the east exit's node, numbered after the bottom's, moves on.
  const World plain = far_from_the_first_area({});
  const World split = far_from_the_first_area(
      {Structure{{AREA_SIDE / 2, AREA_SIDE - 1}, INTACT, {1, 1}, true}});
  const std::optional<NodeRef> before = plain.far_node(1);
  const std::optional<NodeRef> after = split.far_node(1);
  ASSERT_TRUE(before.has_value());
  ASSERT_TRUE(after.has_value());
  ASSERT_NE(after.value_or(NodeRef{}).node, before.value_or(NodeRef{}).node);

  const Cell was = plain.graph().node(before.value_or(NodeRef{})).cell;
  const Cell now = split.graph().node(after.value_or(NodeRef{})).cell;

  EXPECT_EQ(now.x, was.x);
  EXPECT_EQ(now.y, was.y);
}

}  // namespace
}  // namespace murim
