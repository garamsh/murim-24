#include "coarse_graph.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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

TEST(CoarseGraph, AnExitLinksToTheFarNodeWhoseExitLeadsBack) {
  // A map of 2 by 2 areas. The south-eastern area's corner cell (32, 32) is
  // the only cell open across its top border and across its left one, so it
  // is the middle of an entrance on each side: two nodes at one cell.
  constexpr std::int32_t SIDE = 64;
  constexpr std::int32_t CORNER = 32;
  std::vector<std::string> rows(SIDE, std::string(SIDE, '.'));
  for (std::int32_t along = CORNER + 1; along < SIDE; ++along) {
    rows.at(CORNER - 1).at(static_cast<std::size_t>(along)) = '#';
    rows.at(static_cast<std::size_t>(along)).at(CORNER - 1) = '#';
  }
  const Map map{rows};
  const CoarseGraph graph{map};
  const std::size_t corner_area = graph.area_of(Cell{CORNER, CORNER});
  const std::size_t west = graph.area_of(Cell{0, CORNER});
  const std::size_t north = graph.area_of(Cell{CORNER, 0});
  // Setup: the corner area holds two nodes at the corner cell, one leading
  // north and one west.
  std::vector<std::size_t> at_corner;
  for (const CoarseExit& exit : graph.exits(corner_area)) {
    const Cell cell = graph.node(NodeRef{corner_area, exit.node}).cell;
    if (cell.x == CORNER && cell.y == CORNER) {
      at_corner.push_back(exit.node);
    }
  }
  ASSERT_EQ(at_corner.size(), 2U);
  ASSERT_NE(at_corner.front(), at_corner.back());
  // Each neighbour's entrance names the node whose exit leads back to it.
  for (const std::size_t neighbour : {west, north}) {
    for (const CoarseExit& exit : graph.exits(neighbour)) {
      if (exit.area != corner_area) {
        continue;
      }
      bool back = false;
      for (const CoarseExit& other : graph.exits(corner_area)) {
        back = back || (other.node == exit.far_node && other.area == neighbour);
      }
      EXPECT_TRUE(back) << neighbour;
    }
  }
}

// A map of 2 by 2 regions, each cell blocked with DENSE / 4 chances in 100,
// drawn from `seed`.
constexpr std::int32_t REGIONS_SIDE = 512;
constexpr std::uint64_t REGIONS_SEED = 11;
constexpr int OBJECTIVES = 3;
// The exits drawn for each objective.
constexpr int EXITS = 40;

std::vector<std::string> regions_rows(std::uint64_t seed) {
  constexpr std::int32_t HUNDRED = 100;
  Random random{seed};
  std::vector<std::string> rows(static_cast<std::size_t>(REGIONS_SIDE),
                                std::string(REGIONS_SIDE, '.'));
  for (std::string& row : rows) {
    for (char& cell : row) {
      cell = random.below(HUNDRED) < DENSE / 4 ? '#' : '.';
    }
  }
  return rows;
}

TEST(CoarseGraph, ASearchByRegionsCostsExitsAsTheAreasAloneDo) {
  constexpr std::int32_t REGION_SIDE = REGION_AREAS * 32;
  for (std::uint64_t seed = REGIONS_SEED; seed < REGIONS_SEED + 2; ++seed) {
    const Map map{regions_rows(seed)};
    const CoarseGraph graph{map};
    Random draws{seed + PAIR_DRAWS};
    for (int each = 0; each < OBJECTIVES; ++each) {
      const Cell objective = passable_cell(map, draws);
      GoalSearch search = graph.toward(map, objective);
      std::size_t reached = 0;
      std::size_t elsewhere = 0;
      const auto areas =
          static_cast<std::int32_t>((REGIONS_SIDE / 32) * (REGIONS_SIDE / 32));
      for (int drawn = 0; drawn < EXITS; ++drawn) {
        const auto area = static_cast<std::size_t>(draws.below(areas));
        const std::vector<CoarseExit>& exits = graph.exits(area);
        if (exits.empty()) {
          continue;
        }
        const CoarseExit& exit = exits.at(static_cast<std::size_t>(
            draws.below(static_cast<std::int32_t>(exits.size()))));
        const NodeRef far{exit.area, exit.far_node};
        const std::optional<CoarseCost> by_areas =
            graph.cost_over_areas(map, objective, far);
        EXPECT_EQ(graph.cost_from(search, far), by_areas)
            << seed << " " << area << " " << exit.far_node;
        const Cell cell = graph.node(far).cell;
        reached += by_areas ? 1 : 0;
        elsewhere +=
            by_areas && (cell.x / REGION_SIDE != objective.x / REGION_SIDE ||
                         cell.y / REGION_SIDE != objective.y / REGION_SIDE)
                ? 1
                : 0;
      }
      // Setup: exits in another region than the objective's reach it.
      ASSERT_GT(reached, 0U);
      ASSERT_GT(elsewhere, 0U);
    }
  }
}

TEST(CoarseGraph, ASearchByRegionsLeavesTheObjectivesRegionWhereThatIsTheWay) {
  // Two regions side by side; a wall across the western one parts its upper
  // rows from its lower, so a node below the wall reaches an objective above
  // it only through the eastern region, which is open.
  constexpr std::int32_t WIDE = 512;
  constexpr std::int32_t HIGH = 256;
  constexpr std::int32_t REGION_SIDE = 256;
  constexpr std::int32_t WALL = 40;
  std::vector<std::string> rows(HIGH, std::string(WIDE, '.'));
  for (std::int32_t col = 0; col < REGION_SIDE; ++col) {
    rows.at(WALL).at(static_cast<std::size_t>(col)) = '#';
  }
  const Map map{rows};
  const CoarseGraph graph{map};
  const Cell objective{240, 10};
  // A node below the wall in the western region: the far side of an exit
  // from the area holding (240, 100) into its western neighbour.
  const std::size_t area = graph.area_of(Cell{240, 100});
  std::optional<NodeRef> below;
  for (const CoarseExit& exit : graph.exits(area)) {
    if (exit.area + 1 == area) {
      below = NodeRef{exit.area, exit.far_node};
    }
  }
  ASSERT_TRUE(below.has_value());
  const NodeRef from = below.value_or(NodeRef{0, 0});
  GoalSearch search = graph.toward(map, objective);
  const std::optional<CoarseCost> by_areas =
      graph.cost_over_areas(map, objective, from);
  ASSERT_TRUE(by_areas.has_value());
  EXPECT_EQ(graph.cost_from(search, from), by_areas);
  // Control: within the western region alone, no walk joins them.
  std::vector<std::string> western;
  for (const std::string& row : rows) {
    western.push_back(row.substr(0, REGION_SIDE));
  }
  const Map alone{western};
  const CoarseGraph alone_graph{alone};
  EXPECT_FALSE(
      alone_graph.joined(alone, Walk{graph.node(from).cell, objective}));
}

TEST(CoarseGraph, ARevertedRebuildLeavesTheGraphAsItWas) {
  // Open ground of two regions side by side.
  constexpr std::int32_t WIDE = 512;
  constexpr std::int32_t HIGH = 256;
  const Map map{std::vector<std::string>(HIGH, std::string(WIDE, '.'))};
  const CoarseGraph original{map};
  // A house in the middle of the western region's easternmost area, which
  // lies across the walks between its region's border nodes and parts
  // nothing, and a wall down the western region, which parts it.
  std::vector<Cell> house;
  for (std::int32_t row = 110; row < 113; ++row) {
    for (std::int32_t col = 238; col < 241; ++col) {
      house.push_back(Cell{col, row});
    }
  }
  std::vector<Cell> wall;
  for (std::int32_t row = 0; row < HIGH; ++row) {
    wall.push_back(Cell{WIDE / 4, row});
  }
  for (const std::vector<Cell>& blocked : {house, wall}) {
    Map built = map;
    for (const Cell cell : blocked) {
      built.block(cell);
    }
    CoarseGraph graph = original;
    CoarseGraph::Undo undo = graph.rebuild(built, blocked);
    // The rebuild changed the graph to what a fresh build gives.
    ASSERT_FALSE(graph == original);
    EXPECT_TRUE(graph == CoarseGraph{built});
    graph.revert(std::move(undo));
    EXPECT_TRUE(graph == original);
  }
  // Setup: the house kept every area's nodes, and the wall did not.
  Map housed = map;
  for (const Cell cell : house) {
    housed.block(cell);
  }
  Map walled = map;
  for (const Cell cell : wall) {
    walled.block(cell);
  }
  EXPECT_EQ(CoarseGraph{housed}.node_count(), original.node_count());
  EXPECT_FALSE(
      CoarseGraph{walled}.joined(walled, Walk{Cell{0, 0}, Cell{WIDE - 1, 0}}));
}

TEST(CoarseGraph, OpenAreasCostedInClosedFormMatchTheirSearch) {
  // Open ground east of the middle column, scattered blocks west of it, and
  // in one open area a wall with a gap, which lengthens walks across it.
  std::vector<std::string> rows = random_rows(Scatter{REBUILT_SEED, DENSE / 2});
  for (std::string& row : rows) {
    std::fill(row.begin() + SMALL_SIDE / 2, row.end(), '.');
  }
  constexpr std::int32_t WALLED = 80;
  for (std::int32_t row = 64; row < 92; ++row) {
    rows.at(static_cast<std::size_t>(row)).at(WALLED) = '#';
  }
  const Map map{rows};
  const CoarseGraph closed{map};
  const CoarseGraph searched{map, AreaCosts::SEARCHED};
  EXPECT_TRUE(closed == searched);
  // Control: across the wall a walk costs more than the octile cost, which
  // the closed form would have given that area.
  const Cell west{WALLED - 4, 70};
  const Cell east{WALLED + 4, 70};
  const std::optional<CoarseCost> across =
      closed.path_cost(map, Walk{west, east}, false);
  ASSERT_TRUE(across.has_value());
  EXPECT_GT(across.value_or(0), octile(west, east));
  // Setup: the graph does cost open areas, east of the middle column, and
  // walks within one cost the octile cost.
  const Cell open_from{70, 5};
  const Cell open_to{90, 25};
  EXPECT_EQ(closed.path_cost(map, Walk{open_from, open_to}, false),
            octile(open_from, open_to));
}

}  // namespace
}  // namespace murim
