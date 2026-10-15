#include "coarse_graph.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "random.hpp"

namespace murim {
namespace {

// Rows of `side` cells, each blocked with `blocked` chances in 100, drawn
// from `seed`.
std::vector<std::string> random_rows(std::uint64_t seed, std::int32_t side,
                                     std::int32_t blocked) {
  constexpr std::int32_t HUNDRED = 100;
  Random random{seed};
  std::vector<std::string> rows(
      static_cast<std::size_t>(side),
      std::string(static_cast<std::size_t>(side), '.'));
  for (std::string& row : rows) {
    for (char& cell : row) {
      cell = random.below(HUNDRED) < blocked ? '#' : '.';
    }
  }
  return rows;
}

// A passable cell of `map` drawn from `random`.
Cell passable_cell(const Map& map, Random& random) {
  Cell cell{0, 0};
  do {
    cell = Cell{random.below(map.width()), random.below(map.height())};
  } while (!map.passable(cell));
  return cell;
}

constexpr std::int32_t SMALL_SIDE = 96;
constexpr std::int32_t DENSE = 40;
constexpr int PAIRS = 40;

TEST(CoarseGraph, TwoCellsAreJoinedExactlyWhereAFullFieldReachesOne) {
  std::size_t reached = 0;
  std::size_t apart = 0;
  for (std::uint64_t seed = 1; seed <= 6; ++seed) {
    const Map map{random_rows(seed, SMALL_SIDE, DENSE)};
    const CoarseGraph graph{map};
    Random draws{seed + 100};
    for (int pair = 0; pair < PAIRS; ++pair) {
      const Cell from = passable_cell(map, draws);
      const Cell to = passable_cell(map, draws);
      const bool full =
          FlowField{map, to}.distance(from) != FlowField::UNREACHABLE;
      EXPECT_EQ(graph.joined(map, from, to), full) << seed << " " << pair;
      EXPECT_EQ(graph.path_cost(map, from, to, true).has_value(), full)
          << seed << " " << pair;
      ++(full ? reached : apart);
    }
  }
  // The maps hold both joined and parted pairs, so both answers are tried.
  EXPECT_GT(reached, 0U);
  EXPECT_GT(apart, 0U);
}

// An open map of four regions, 512 cells square, crossed by a wall with a
// gap every region, so paths must find the gaps.
std::vector<std::string> walled_rows() {
  constexpr std::int32_t SIDE = 512;
  constexpr std::int32_t WALL = 300;
  constexpr std::int32_t GAP_EVERY = 200;
  constexpr std::int32_t GAP = 6;
  std::vector<std::string> rows(static_cast<std::size_t>(SIDE),
                                std::string(SIDE, '.'));
  for (std::int32_t y = 0; y < SIDE; ++y) {
    if (y % GAP_EVERY >= GAP) {
      rows.at(static_cast<std::size_t>(y)).at(static_cast<std::size_t>(WALL)) =
          '#';
    }
  }
  return rows;
}

TEST(CoarseGraph, APathOverTheRegionsCostsWhatOneOverTheAreasDoes) {
  const Map map{walled_rows()};
  const CoarseGraph graph{map};
  const std::vector<std::pair<Cell, Cell>> walks{
      {Cell{10, 10}, Cell{500, 500}},
      {Cell{20, 400}, Cell{480, 30}},
      {Cell{100, 250}, Cell{310, 260}}};
  for (const auto& [from, to] : walks) {
    const std::optional<CoarseCost> regions =
        graph.path_cost(map, from, to, true);
    const std::optional<CoarseCost> areas =
        graph.path_cost(map, from, to, false);
    ASSERT_TRUE(regions.has_value());
    ASSERT_TRUE(areas.has_value());
    EXPECT_EQ(regions, areas) << from.x << "," << from.y;
    // Neither is shorter than the full field's exact shortest walk.
    const std::uint32_t exact = FlowField{map, to}.distance(from);
    EXPECT_GE(regions.value_or(0) / 5, exact) << from.x << "," << from.y;
  }
}

TEST(CoarseGraph, ARebuildAfterBlockingCellsMatchesAGraphBuiltAfresh) {
  Map map{random_rows(7, SMALL_SIDE, DENSE / 2)};
  CoarseGraph graph{map};
  // A wall down the middle area's column, with no gap.
  std::vector<Cell> blocked;
  for (std::int32_t y = 0; y < SMALL_SIDE; ++y) {
    blocked.push_back(Cell{SMALL_SIDE / 2, y});
    map.block(blocked.back());
  }
  graph.rebuild(map, blocked);
  const CoarseGraph fresh{map};
  EXPECT_EQ(graph.node_count(), fresh.node_count());
  Random draws{77};
  for (int pair = 0; pair < PAIRS; ++pair) {
    const Cell from = passable_cell(map, draws);
    const Cell to = passable_cell(map, draws);
    EXPECT_EQ(graph.joined(map, from, to), fresh.joined(map, from, to));
    EXPECT_EQ(graph.path_cost(map, from, to, true),
              fresh.path_cost(map, from, to, true));
  }
  // The wall parts its halves.
  EXPECT_FALSE(graph.joined(map, Cell{0, 0}, Cell{SMALL_SIDE - 1, 0}) &&
               map.passable(Cell{0, 0}) &&
               map.passable(Cell{SMALL_SIDE - 1, 0}));
}

}  // namespace
}  // namespace murim
