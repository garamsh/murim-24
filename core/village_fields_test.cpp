#include "village_fields.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"

namespace murim {
namespace {

// The cells a walk takes from `from` by place `place`'s steps, until a step
// is none or `limit` steps are taken.
std::vector<Cell> walk(const VillageFields& fields, std::size_t place,
                       Cell from, std::size_t limit) {
  std::vector<Cell> cells{from};
  for (std::size_t taken = 0; taken < limit; ++taken) {
    const Step step = fields.step(place, cells.back());
    if (step.x == 0 && step.y == 0) {
      break;
    }
    cells.push_back(Cell{cells.back().x + step.x, cells.back().y + step.y});
  }
  return cells;
}

// What place `place`'s field reads at each cell of a window of `size` from
// (0, 0), row by row: its step's x and y, and whether it reaches the cell.
std::vector<std::int32_t> readings(const VillageFields& fields,
                                   std::size_t place, Footprint size) {
  std::vector<std::int32_t> read;
  for (Cell cell{0, 0}; cell.y < size.height; ++cell.y) {
    for (cell.x = 0; cell.x < size.width; ++cell.x) {
      const Step step = fields.step(place, cell);
      read.push_back(step.x);
      read.push_back(step.y);
      read.push_back(fields.reaches(place, cell) ? 1 : 0);
    }
  }
  return read;
}

// The wall tests' window, 7 by 5; their seed, east of the wall; where their
// walks start, west of it; and the most steps a walk takes.
constexpr Footprint WALLED{7, 5};
constexpr Cell EAST_SEED{5, 1};
constexpr Cell WEST_START{1, 1};
constexpr std::size_t MOST_STEPS = 20;

TEST(VillageFields, AFieldLeadsRoundAWallToItsSeed) {
  // A wall down column 3 from the top to row 3 lies between the start and
  // the seed; its foot is row 4.
  constexpr std::int32_t BELOW_WALL = 4;
  const Map walled{{"...#...", "...#...", "...#...", "...#...", "......."}};
  VillageFields fields{walled, {0, 0}, WALLED, 1};
  fields.build(0, {EAST_SEED}, EAST_SEED);
  const std::vector<Cell> path = walk(fields, 0, WEST_START, MOST_STEPS);
  EXPECT_EQ(path.back().x, EAST_SEED.x);
  EXPECT_EQ(path.back().y, EAST_SEED.y);
  EXPECT_TRUE(std::all_of(path.begin(), path.end(), [&walled](Cell cell) {
    return walled.passable(cell);
  }));
  EXPECT_TRUE(std::any_of(path.begin(), path.end(),
                          [](Cell cell) { return cell.y == BELOW_WALL; }));
  // Control: on open ground the walk keeps to the start's row, straight to
  // the seed.
  const Map open{std::vector<std::string>(
      static_cast<std::size_t>(WALLED.height),
      std::string(static_cast<std::size_t>(WALLED.width), '.'))};
  VillageFields straight{open, {0, 0}, WALLED, 1};
  straight.build(0, {EAST_SEED}, EAST_SEED);
  const std::vector<Cell> line = walk(straight, 0, WEST_START, MOST_STEPS);
  EXPECT_EQ(line.size(),
            static_cast<std::size_t>(EAST_SEED.x - WEST_START.x + 1));
  EXPECT_TRUE(std::all_of(line.begin(), line.end(),
                          [](Cell cell) { return cell.y == WEST_START.y; }));
}

TEST(VillageFields, ACellCutOffFromTheSeedsIsNotReached) {
  // Column 3 is a wall from top to bottom; a cell east of it is on the
  // seed's side.
  constexpr Cell EAST_OF_WALL{4, 1};
  const Map split{{"...#...", "...#...", "...#..."}};
  VillageFields fields{split, {0, 0}, {WALLED.width, 3}, 1};
  fields.build(0, {EAST_SEED}, EAST_SEED);
  EXPECT_FALSE(fields.reaches(0, WEST_START));
  EXPECT_EQ(fields.step(0, WEST_START).x, 0);
  EXPECT_EQ(fields.step(0, WEST_START).y, 0);
  // Control: a cell on the seed's side is reached, and steps toward it.
  EXPECT_TRUE(fields.reaches(0, EAST_OF_WALL));
  EXPECT_EQ(fields.step(0, EAST_OF_WALL).x, 1);
}

TEST(VillageFields, NoStepCutsBetweenTwoBlockedCorners) {
  // (0, 1) meets the seed at (1, 0) only diagonally, between the blocked
  // (0, 0) and (1, 1).
  const Map pinched{{"#.", ".#"}};
  VillageFields fields{pinched, {0, 0}, {2, 2}, 1};
  fields.build(0, {Cell{1, 0}}, Cell{1, 0});
  EXPECT_FALSE(fields.reaches(0, {0, 1}));
  // Control: with (0, 0) open, the walk goes round through it.
  const Map open_corner{{"..", ".#"}};
  VillageFields round{open_corner, {0, 0}, {2, 2}, 1};
  round.build(0, {Cell{1, 0}}, Cell{1, 0});
  EXPECT_TRUE(round.reaches(0, {0, 1}));
  EXPECT_EQ(round.step(0, {0, 1}).x, 0);
  EXPECT_EQ(round.step(0, {0, 1}).y, -1);
}

TEST(VillageFields, AFieldSeededOnManyCellsEndsAtTheNearest) {
  // A row of 9 with a seed at each end.
  constexpr Footprint ROW{9, 5};
  constexpr Cell WEST_END{0, 2};
  constexpr Cell EAST_END{8, 2};
  constexpr Cell MIDDLE{4, 2};
  constexpr Cell NEAR_WEST{2, 2};
  constexpr Cell NEAR_EAST{6, 2};
  const Map open{std::vector<std::string>(
      static_cast<std::size_t>(ROW.height),
      std::string(static_cast<std::size_t>(ROW.width), '.'))};
  VillageFields fields{open, {0, 0}, ROW, 1};
  fields.build(0, {WEST_END, EAST_END}, MIDDLE);
  EXPECT_EQ(walk(fields, 0, NEAR_WEST, MOST_STEPS).back().x, WEST_END.x);
  EXPECT_EQ(walk(fields, 0, NEAR_EAST, MOST_STEPS).back().x, EAST_END.x);
}

TEST(VillageFields, AFieldStepsAlikeWhenEverItIsBuilt) {
  // The wall map, with places 0 and 1 seeded east and west of the wall.
  const Map walled{{"...#...", "...#...", "...#...", "...#...", "......."}};
  constexpr Cell WEST_SEED{1, 3};
  VillageFields first{walled, {0, 0}, WALLED, 2};
  first.build(0, {EAST_SEED}, EAST_SEED);
  first.build(1, {WEST_SEED}, WEST_SEED);
  VillageFields last{walled, {0, 0}, WALLED, 2};
  last.build(1, {WEST_SEED}, WEST_SEED);
  last.build(0, {EAST_SEED}, EAST_SEED);
  for (std::size_t place = 0; place < 2; ++place) {
    EXPECT_EQ(readings(first, place, WALLED), readings(last, place, WALLED));
  }
  // Control: the fields compared differ from place to place, so they are not
  // none everywhere.
  EXPECT_NE(readings(first, 0, WALLED), readings(first, 1, WALLED));
}

TEST(VillageFields, EachCellOfAFieldTakesThreeBytes) {
  constexpr Footprint WINDOW{10, 8};
  constexpr Cell FAR_CORNER{8, 6};
  const Map open{std::vector<std::string>(
      static_cast<std::size_t>(WINDOW.height),
      std::string(static_cast<std::size_t>(WINDOW.width), '.'))};
  VillageFields fields{open, {0, 0}, WINDOW, 3};
  EXPECT_EQ(fields.built(), 0U);
  EXPECT_EQ(fields.bytes(), 0U);
  fields.build(2, {Cell{1, 1}}, Cell{1, 1});
  fields.build(0, {FAR_CORNER}, FAR_CORNER);
  EXPECT_TRUE(fields.holds(0));
  EXPECT_FALSE(fields.holds(1));
  EXPECT_TRUE(fields.holds(2));
  EXPECT_EQ(fields.built(), 2U);
  EXPECT_EQ(fields.bytes(), 2U * 10U * 8U * 3U);
}

TEST(VillageFields, AFieldNotBuiltCannotBeRead) {
  const Map open{std::vector<std::string>(3, std::string(3, '.'))};
  VillageFields fields{open, {0, 0}, {3, 3}, 2};
  EXPECT_THROW(static_cast<void>(fields.step(1, {1, 1})), std::out_of_range);
  // Control: once built, the same read answers.
  fields.build(1, {Cell{0, 0}}, Cell{0, 0});
  EXPECT_EQ(fields.step(1, {1, 1}).x, -1);
}

TEST(VillageFields, AWindowWiderThanTheLargestIsRefused) {
  constexpr std::int32_t SIDE = VillageFields::MAX_SIDE;
  constexpr std::size_t WIDER = static_cast<std::size_t>(SIDE) + 1;
  const Map open{std::vector<std::string>(WIDER, std::string(WIDER, '.'))};
  EXPECT_THROW((VillageFields{open, {0, 0}, {SIDE + 1, 1}, 1}),
               std::invalid_argument);
  EXPECT_THROW((VillageFields{open, {0, 0}, {1, 0}, 1}), std::invalid_argument);
  // Control: the largest window is taken.
  EXPECT_NO_THROW((VillageFields{open, {0, 0}, {SIDE, SIDE}, 1}));
}

TEST(VillageFields, AWindowOverlapsWhatSharesACellWithIt) {
  // A window of 4 by 3 from (2, 2), so its last cell is (5, 4).
  constexpr Cell CORNER{2, 2};
  constexpr Footprint WINDOW{4, 3};
  constexpr std::size_t SIDE = 8;
  const Map open{std::vector<std::string>(SIDE, std::string(SIDE, '.'))};
  const VillageFields fields{open, CORNER, WINDOW, 1};
  EXPECT_TRUE(fields.overlaps({5, 4}, {2, 2}));
  EXPECT_TRUE(fields.overlaps({0, 0}, {3, 3}));
  // Controls: a footprint just past its last column, or ending just before
  // its first row, does not.
  EXPECT_FALSE(fields.overlaps({6, 2}, {2, 2}));
  EXPECT_FALSE(fields.overlaps({2, 0}, {2, 2}));
}

}  // namespace
}  // namespace murim
