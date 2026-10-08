#include "flow_field.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr std::size_t SIDE = 16;

std::vector<std::string> open_rows() { return {SIDE, std::string(SIDE, '.')}; }

// The cells a walk along `field`'s steps visits from `from` to where the
// steps end, `from` included; at most SIDE * SIDE of them.
std::vector<Cell> walk(const FlowField& field, Cell from) {
  std::vector<Cell> cells{from};
  for (std::size_t i = 0; i < SIDE * SIDE; ++i) {
    const Step step = field.step(cells.back());
    if (step.x == 0 && step.y == 0) {
      break;
    }
    cells.push_back(Cell{cells.back().x + step.x, cells.back().y + step.y});
  }
  return cells;
}

bool same(Cell one, Cell other) { return one.x == other.x && one.y == other.y; }

// Twice the area between cell `cell` and the line from `from` to `end`,
// over the line's length: the cell's distance from the line, times that
// length.
std::int64_t off_line(Cell from, Cell end, Cell cell) {
  return std::abs(((std::int64_t{end.x} - from.x) * (cell.y - from.y)) -
                  ((std::int64_t{end.y} - from.y) * (cell.x - from.x)));
}

// Two cells touching at the corner a diagonal step from CORNER_FROM to
// CORNER_TO would cut.
constexpr Cell CORNER_FROM{4, 4};
constexpr Cell CORNER_TO{5, 5};
constexpr Cell EAST_OF_CORNER{5, 4};
constexpr Cell SOUTH_OF_CORNER{4, 5};

void block(std::vector<std::string>& rows, Cell cell) {
  rows.at(static_cast<std::size_t>(cell.y))
      .at(static_cast<std::size_t>(cell.x)) = '#';
}

TEST(FlowField, ADiagonalObjectiveIsWalkedAlongTheDiagonal) {
  const Map map{open_rows()};
  const FlowField field{map, Cell{12, 12}};
  const std::vector<Cell> cells = walk(field, Cell{2, 2});

  // Ten diagonal steps, every one of them on the diagonal.
  ASSERT_EQ(cells.size(), 11U);
  EXPECT_TRUE(same(cells.back(), Cell{12, 12}));
  for (const Cell cell : cells) {
    EXPECT_EQ(cell.x, cell.y);
  }
  // Ten diagonal steps count 7/5 of a cell each.
  EXPECT_EQ(field.distance(Cell{2, 2}), 14U);
  EXPECT_EQ(field.distance(Cell{11, 11}), 1U);
}

TEST(FlowField, AnObjectiveOffEveryDiagonalIsWalkedNearItsLine) {
  const Map map{open_rows()};
  constexpr Cell FROM{1, 3};
  constexpr Cell END{13, 8};
  const FlowField field{map, END};
  const std::vector<Cell> cells = walk(field, FROM);

  // Five diagonal and seven straight steps, the fewest that reach it.
  ASSERT_EQ(cells.size(), 13U);
  EXPECT_TRUE(same(cells.back(), END));
  // Every cell walked lies within a cell of the line, 13 cells long, where
  // an L of every step along x before every step along y strays 4.6.
  for (const Cell cell : cells) {
    EXPECT_LE(off_line(FROM, END, cell), 13);
  }
}

TEST(FlowField, NoStepCutsACornerBetweenTwoBlockedCells) {
  std::vector<std::string> rows = open_rows();
  block(rows, EAST_OF_CORNER);
  block(rows, SOUTH_OF_CORNER);
  const FlowField field{Map{rows}, Cell{7, 7}};
  const std::vector<Cell> cells = walk(field, Cell{2, 2});

  EXPECT_TRUE(same(cells.back(), Cell{7, 7}));
  for (std::size_t i = 1; i < cells.size(); ++i) {
    const Cell from = cells.at(i - 1);
    const Cell next = cells.at(i);
    EXPECT_TRUE(Map{rows}.passable(Cell{next.x, from.y}) &&
                Map{rows}.passable(Cell{from.x, next.y}))
        << "step from (" << from.x << ", " << from.y << ") to (" << next.x
        << ", " << next.y << ") cuts a blocked corner";
  }
  // Control: on open ground the same walk takes that diagonal step.
  const std::vector<Cell> open =
      walk(FlowField{Map{open_rows()}, {7, 7}}, {2, 2});
  bool cut = false;
  for (std::size_t i = 1; i < open.size(); ++i) {
    cut = cut ||
          (same(open.at(i - 1), CORNER_FROM) && same(open.at(i), CORNER_TO));
  }
  EXPECT_TRUE(cut);
}

TEST(FlowField, NoStepPassesBesideOneBlockedCell) {
  std::vector<std::string> rows = open_rows();
  block(rows, EAST_OF_CORNER);
  const FlowField field{Map{rows}, CORNER_TO};

  // The diagonal from CORNER_FROM would pass beside the blocked cell, so it
  // steps down first, two straight steps.
  const Step step = field.step(CORNER_FROM);
  EXPECT_FALSE(step.x == 1 && step.y == 1);
  EXPECT_EQ(field.distance(CORNER_FROM), 2U);
  // Control: with that cell open the step is the diagonal.
  const Step open = FlowField{Map{open_rows()}, CORNER_TO}.step(CORNER_FROM);
  EXPECT_TRUE(open.x == 1 && open.y == 1);
}

}  // namespace
}  // namespace murim
