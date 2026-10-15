#include "coarse_graph.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "random.hpp"

namespace murim {
namespace {

constexpr std::int32_t SMALL_SIDE = 96;
constexpr std::int32_t DENSE = 40;
constexpr int PAIRS = 40;
constexpr std::uint64_t MAPS = 6;
// The draws of the pairs of cells start this far past the map's seed.
constexpr std::uint64_t PAIR_DRAWS = 100;

// A map of SMALL_SIDE cells, each blocked with `blocked` chances in 100,
// drawn from `seed`.
struct Scatter {
  std::uint64_t seed;
  std::int32_t blocked;
};

std::vector<std::string> random_rows(Scatter scatter) {
  constexpr std::int32_t HUNDRED = 100;
  Random random{scatter.seed};
  std::vector<std::string> rows(static_cast<std::size_t>(SMALL_SIDE),
                                std::string(SMALL_SIDE, '.'));
  for (std::string& row : rows) {
    for (char& cell : row) {
      cell = random.below(HUNDRED) < scatter.blocked ? '#' : '.';
    }
  }
  return rows;
}

// A passable cell of `map` drawn from `random`.
Cell passable_cell(const Map& map, Random& random) {
  Cell cell{random.below(map.width()), random.below(map.height())};
  while (!map.passable(cell)) {
    cell = Cell{random.below(map.width()), random.below(map.height())};
  }
  return cell;
}

// Over PAIRS pairs of passable cells of `map` drawn from `draws`, how many a
// whole-map field joins, how many it parts, and how many the graph answers
// otherwise, by joined() or by whether path_cost() finds a walk.
struct Agreement {
  std::size_t reached = 0;
  std::size_t apart = 0;
  std::size_t disagreed = 0;
};

Agreement agreement(const Map& map, const CoarseGraph& graph, Random& draws) {
  Agreement found;
  for (int pair = 0; pair < PAIRS; ++pair) {
    const Cell from = passable_cell(map, draws);
    const Cell goal = passable_cell(map, draws);
    const bool full =
        FlowField{map, goal}.distance(from) != FlowField::UNREACHABLE;
    const Walk walk{from, goal};
    const bool agrees = graph.joined(map, walk) == full &&
                        graph.path_cost(map, walk, true).has_value() == full;
    ++(full ? found.reached : found.apart);
    found.disagreed += agrees ? 0 : 1;
  }
  return found;
}

TEST(CoarseGraph, TwoCellsAreJoinedExactlyWhereAFullFieldReachesOne) {
  Agreement total;
  for (std::uint64_t seed = 1; seed <= MAPS; ++seed) {
    const Map map{random_rows(Scatter{seed, DENSE})};
    const CoarseGraph graph{map};
    Random draws{seed + PAIR_DRAWS};
    const Agreement found = agreement(map, graph, draws);
    EXPECT_EQ(found.disagreed, 0U) << seed;
    total.reached += found.reached;
    total.apart += found.apart;
  }
  // The maps hold both joined and parted pairs, so both answers are tried.
  EXPECT_GT(total.reached, 0U);
  EXPECT_GT(total.apart, 0U);
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
  for (std::int32_t row = 0; row < SIDE; ++row) {
    if (row % GAP_EVERY >= GAP) {
      rows.at(static_cast<std::size_t>(row))
          .at(static_cast<std::size_t>(WALL)) = '#';
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
  for (const auto& [from, goal] : walks) {
    const std::optional<CoarseCost> regions =
        graph.path_cost(map, Walk{from, goal}, true);
    const std::optional<CoarseCost> areas =
        graph.path_cost(map, Walk{from, goal}, false);
    ASSERT_TRUE(regions.has_value());
    ASSERT_TRUE(areas.has_value());
    EXPECT_EQ(regions, areas) << from.x << "," << from.y;
    // Neither is shorter than the full field's exact shortest walk.
    const std::uint32_t exact = FlowField{map, goal}.distance(from);
    EXPECT_GE(regions.value_or(0) / 5, exact) << from.x << "," << from.y;
  }
}

// The map the rebuild test blocks a wall across.
constexpr std::uint64_t REBUILT_SEED = 7;

TEST(CoarseGraph, ARebuildAfterBlockingCellsMatchesAGraphBuiltAfresh) {
  Map map{random_rows(Scatter{REBUILT_SEED, DENSE / 2})};
  CoarseGraph graph{map};
  // A wall down the middle area's column, with no gap.
  std::vector<Cell> blocked;
  for (std::int32_t row = 0; row < SMALL_SIDE; ++row) {
    blocked.push_back(Cell{SMALL_SIDE / 2, row});
    map.block(blocked.back());
  }
  graph.rebuild(map, blocked);
  const CoarseGraph fresh{map};
  EXPECT_EQ(graph.node_count(), fresh.node_count());
  Random draws{REBUILT_SEED + PAIR_DRAWS};
  for (int pair = 0; pair < PAIRS; ++pair) {
    const Walk walk{passable_cell(map, draws), passable_cell(map, draws)};
    EXPECT_EQ(graph.joined(map, walk), fresh.joined(map, walk));
    EXPECT_EQ(graph.path_cost(map, walk, true),
              fresh.path_cost(map, walk, true));
  }
  // The wall parts its halves.
  EXPECT_FALSE(graph.joined(map, Walk{Cell{0, 0}, Cell{SMALL_SIDE - 1, 0}}) &&
               map.passable(Cell{0, 0}) &&
               map.passable(Cell{SMALL_SIDE - 1, 0}));
}

TEST(CoarseGraph, ARebuildRelinksTheNeighboursOfAnAreaWhoseNodesItRenumbers) {
  // A map of 3 by 2 areas. Blocking two cells of the lower middle area's top
  // row splits the entrance it shares with the upper middle area, so that
  // area is rebuilt with one node more along its bottom side, numbered before
  // its left and right sides' nodes. Its western neighbour, diagonal to the
  // blocked area, holds links to those renumbered nodes.
  constexpr std::int32_t WIDE = 96;
  constexpr std::int32_t HIGH = 64;
  constexpr std::int32_t ROW = 32;
  constexpr std::int32_t SPLIT = 48;
  Map map{std::vector<std::string>(HIGH, std::string(WIDE, '.'))};
  CoarseGraph graph{map};
  const CoarseGraph before = graph;
  map.block(Cell{SPLIT, ROW});
  map.block(Cell{SPLIT + 1, ROW});
  graph.rebuild(map, {Cell{SPLIT, ROW}, Cell{SPLIT + 1, ROW}});
  const CoarseGraph fresh{map};
  // The upper middle area's nodes are numbered afresh: one is not where it
  // was, so a link not relinked would name the wrong node.
  constexpr std::size_t MIDDLE = 1;
  bool moved = false;
  for (std::size_t node = 0; node < 3; ++node) {
    const Cell was = before.node(NodeRef{MIDDLE, node}).cell;
    const Cell now = fresh.node(NodeRef{MIDDLE, node}).cell;
    moved = moved || was.x != now.x || was.y != now.y;
  }
  ASSERT_TRUE(moved);
  // A walk from the western area through the middle one to the eastern costs
  // what it does over a graph built afresh.
  const Cell west{5, 5};
  const Cell east{90, 5};
  EXPECT_EQ(graph.path_cost(map, Walk{west, east}, true),
            fresh.path_cost(map, Walk{west, east}, true));
  EXPECT_EQ(graph.path_cost(map, Walk{west, east}, false),
            fresh.path_cost(map, Walk{west, east}, false));
  EXPECT_TRUE(graph.joined(map, Walk{west, Cell{SPLIT, ROW + 5}}));
}

}  // namespace
}  // namespace murim
