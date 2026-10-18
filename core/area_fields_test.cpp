#include "area_fields.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "areas.hpp"
#include "coarse_graph.hpp"
#include "flow_field.hpp"
#include "map.hpp"

namespace murim {
namespace {

// A map of 3 by 2 areas whose first area a wall splits: west of it a strip
// whose only way out leads down into a sealed pocket, east of it open ground
// joined to the rest.
constexpr std::int32_t WIDTH = 96;
constexpr std::int32_t HEIGHT = 64;
constexpr std::int32_t WALL = 10;
constexpr std::int32_t POCKET_FLOOR = 41;
constexpr Cell OBJECTIVE{80, 16};
constexpr Cell CUT_OFF{5, 16};
constexpr Cell JOINED{20, 16};

Map split_map() {
  std::vector<std::string> rows(static_cast<std::size_t>(HEIGHT),
                                std::string(WIDTH, '.'));
  for (std::string& row : rows) {
    row.at(static_cast<std::size_t>(WALL)) = '#';
  }
  std::string& floor = rows.at(static_cast<std::size_t>(POCKET_FLOOR));
  std::fill_n(floor.begin(), WALL, '#');
  return Map{rows};
}

TEST(AreaFields, AComponentCutOffFromTheObjectiveGetsNoStep) {
  const Map map = split_map();
  const CoarseGraph graph{map};
  // The strip leaves its area only into the pocket, which reaches nothing.
  ASSERT_EQ(graph.area_of(CUT_OFF), graph.area_of(JOINED));
  ASSERT_FALSE(graph.joined(map, Walk{CUT_OFF, OBJECTIVE}));
  ASSERT_TRUE(graph.joined(map, Walk{JOINED, OBJECTIVE}));
  AreaFields fields;
  const FlowField& cut_off =
      fields.field(fields.field_for(map, graph, CUT_OFF, OBJECTIVE));
  EXPECT_EQ(cut_off.distance(CUT_OFF), FlowField::UNREACHABLE);
  EXPECT_EQ(cut_off.step(CUT_OFF).x, 0);
  EXPECT_EQ(cut_off.step(CUT_OFF).y, 0);
  // Control: in the same area's field, a cell of the joined component steps
  // toward the objective, east.
  const FlowField& joined =
      fields.field(fields.field_for(map, graph, JOINED, OBJECTIVE));
  EXPECT_NE(joined.distance(JOINED), FlowField::UNREACHABLE);
  EXPECT_EQ(joined.step(JOINED).x, 1);
}

TEST(AreaFields, AFieldAskedForEveryTickIsBuiltOnce) {
  const Map map = split_map();
  const CoarseGraph graph{map};
  AreaFields fields;
  const std::size_t first = fields.field_for(map, graph, JOINED, OBJECTIVE);
  ASSERT_EQ(fields.built(), 1U);
  fields.end_tick();
  EXPECT_EQ(fields.field_for(map, graph, JOINED, OBJECTIVE), first);
  EXPECT_EQ(fields.built(), 0U);
  EXPECT_EQ(fields.held(), 1U);
  // Control: a field no one asks for in a tick is dropped, and built again.
  fields.end_tick();
  fields.end_tick();
  EXPECT_EQ(fields.held(), 0U);
  static_cast<void>(fields.field_for(map, graph, JOINED, OBJECTIVE));
  EXPECT_EQ(fields.built(), 1U);
}

// Cells either side of the side between the first two areas, and one two
// areas east.
constexpr Cell WEST_OF_SIDE{31, 16};
constexpr Cell EAST_OF_SIDE{32, 16};
constexpr Cell TWO_AREAS_EAST{70, 16};

TEST(AreaFields, AFieldIsKeptWhileAPersonBoundForItStepsBackAcrossItsSide) {
  const Map map = split_map();
  const CoarseGraph graph{map};
  ASSERT_NE(graph.area_of(WEST_OF_SIDE), graph.area_of(EAST_OF_SIDE));
  ASSERT_EQ(graph.area_of(TWO_AREAS_EAST), graph.area_of(EAST_OF_SIDE) + 1);
  AreaFields fields;
  static_cast<void>(fields.field_for(map, graph, WEST_OF_SIDE, OBJECTIVE));
  fields.end_tick();
  static_cast<void>(fields.field_for(map, graph, EAST_OF_SIDE, OBJECTIVE));
  fields.end_tick();
  static_cast<void>(fields.field_for(map, graph, WEST_OF_SIDE, OBJECTIVE));
  EXPECT_EQ(fields.built(), 0U);
  fields.end_tick();
  // Control: a tick the person stands two areas away, the west field is
  // dropped, and built again on the way back.
  static_cast<void>(fields.field_for(map, graph, TWO_AREAS_EAST, OBJECTIVE));
  fields.end_tick();
  static_cast<void>(fields.field_for(map, graph, WEST_OF_SIDE, OBJECTIVE));
  EXPECT_EQ(fields.built(), 1U);
}

// How many cells of `cell`'s area and its margin step or cost differently
// in the two fields.
std::size_t differing(const Map& map, const FlowField& one,
                      const FlowField& other, Cell cell) {
  const std::int32_t left = ((cell.x / AREA_SIDE) * AREA_SIDE) - 1;
  const std::int32_t top = ((cell.y / AREA_SIDE) * AREA_SIDE) - 1;
  std::size_t differ = 0;
  for (std::int32_t y = std::max(0, top);
       y < std::min(map.height(), top + AREA_SIDE + 2); ++y) {
    for (std::int32_t x = std::max(0, left);
         x < std::min(map.width(), left + AREA_SIDE + 2); ++x) {
      const Cell at{x, y};
      const bool same = one.distance(at) == other.distance(at) &&
                        one.step(at).x == other.step(at).x &&
                        one.step(at).y == other.step(at).y;
      differ += same ? 0 : 1;
    }
  }
  return differ;
}

// A lay of `laid` on `before`, with a field held toward `objective` in every
// area beforehand: the field of `probe`'s area from before it, the cells
// that differ between that and a fresh build after it, those that differ
// between every field held through it and a fresh build, and how many
// fields were built again.
struct Relaid {
  std::size_t probe_changed = 0;
  std::size_t differ = 0;
  std::size_t built = 0;
  std::size_t fields = 0;
};

Relaid relaid(const Map& before, const std::vector<Cell>& laid, Cell objective,
              Cell probe) {
  CoarseGraph graph{before};
  std::vector<Cell> starts;
  for (std::int32_t y = AREA_SIDE / 2; y < before.height(); y += AREA_SIDE) {
    for (std::int32_t x = AREA_SIDE / 2; x < before.width(); x += AREA_SIDE) {
      starts.push_back(Cell{x, y});
    }
  }
  AreaFields fields;
  for (const Cell start : starts) {
    static_cast<void>(fields.field_for(before, graph, start, objective));
  }
  fields.end_tick();
  const FlowField probed =
      fields.field(fields.field_for(before, graph, probe, objective));
  Map after = before;
  for (const Cell cell : laid) {
    after.block(cell);
  }
  static_cast<void>(graph.rebuild(after, laid));
  fields.changed(after, graph, laid);
  AreaFields fresh;
  Relaid made;
  made.probe_changed = differing(
      after, fresh.field(fresh.field_for(after, graph, probe, objective)),
      probed, probe);
  for (const Cell start : starts) {
    made.differ += differing(
        after, fields.field(fields.field_for(after, graph, start, objective)),
        fresh.field(fresh.field_for(after, graph, start, objective)), start);
  }
  made.built = fields.built();
  made.fields = starts.size();
  return made;
}

// A map of 4 by 4 areas split by a wall down column WALLED_AT, with a gap
// near the top and one near the bottom, and the objective east of the wall
// nearer the top gap. The lay fills the top gap, so a walk from the west
// goes round by the bottom one.
constexpr std::int32_t LAID_SIDE = 128;
constexpr std::int32_t WALLED_AT = 64;
constexpr std::int32_t TOP_GAP = 10;
constexpr std::int32_t BOTTOM_GAP = 110;
constexpr std::int32_t GAP = 4;
constexpr Cell EAST_OF_THE_WALL{100, 30};
constexpr Cell FAR_WEST{16, 48};

TEST(AreaFields, EveryFieldKeptThroughALayIsTheOneAFreshBuildGives) {
  std::vector<std::string> rows(LAID_SIDE, std::string(LAID_SIDE, '.'));
  for (std::int32_t row = 0; row < LAID_SIDE; ++row) {
    const bool gap = (row >= TOP_GAP && row < TOP_GAP + GAP) ||
                     (row >= BOTTOM_GAP && row < BOTTOM_GAP + GAP);
    if (!gap) {
      rows.at(static_cast<std::size_t>(row)).at(WALLED_AT) = '#';
    }
  }
  std::vector<Cell> laid;
  for (std::int32_t row = TOP_GAP; row < TOP_GAP + GAP; ++row) {
    laid.push_back(Cell{WALLED_AT, row});
  }
  const Relaid made = relaid(Map{rows}, laid, EAST_OF_THE_WALL, FAR_WEST);
  // Setup: the lay changes, through the graph's costs, a field whose window
  // holds none of its cells; and some fields were kept.
  ASSERT_GT(made.probe_changed, 0U);
  ASSERT_GT(made.built, 0U);
  ASSERT_LT(made.built, made.fields);
  EXPECT_EQ(made.differ, 0U);
}

// A map of 3 areas in a row, the objective in the last behind a wall open
// only at its foot, so no line from the middle area reaches it. The lay
// draws a bar across the middle area below its entrances' middles, which
// lengthens the walk from the east entrance down to the first area's
// border without changing any node's cost.
constexpr std::int32_t ROW_WIDTH = 96;
constexpr std::int32_t SCREEN_AT = 72;
constexpr std::int32_t SCREEN_FOOT = 28;
constexpr std::int32_t BAR_AT = 24;
constexpr std::int32_t BAR_FROM = 40;
constexpr std::int32_t BAR_TO = 61;
constexpr Cell BEHIND_THE_SCREEN{88, 4};
constexpr Cell FIRST_AREA{16, 16};

TEST(AreaFields, AFieldSeededByAWalkALayLengthensIsBuiltAgain) {
  std::vector<std::string> rows(AREA_SIDE, std::string(ROW_WIDTH, '.'));
  for (std::int32_t row = 0; row < SCREEN_FOOT; ++row) {
    rows.at(static_cast<std::size_t>(row)).at(SCREEN_AT) = '#';
  }
  std::vector<Cell> laid;
  for (std::int32_t column = BAR_FROM; column < BAR_TO; ++column) {
    laid.push_back(Cell{column, BAR_AT});
  }
  const Relaid made = relaid(Map{rows}, laid, BEHIND_THE_SCREEN, FIRST_AREA);
  // Setup: the lay changes, through a walk within the next area alone, the
  // first area's field, whose window holds none of its cells.
  ASSERT_GT(made.probe_changed, 0U);
  EXPECT_EQ(made.differ, 0U);
}

// Open ground of 4 areas in a row, the objective in the last; the lay
// stands a bar across the third, on the lines from the second to it.
constexpr std::int32_t OPEN_WIDTH = 128;
constexpr std::int32_t STOP_AT = 80;
constexpr std::int32_t STOP_FROM = 12;
constexpr std::int32_t STOP_TO = 21;
constexpr Cell IN_THE_LAST{120, 16};

TEST(AreaFields, AFieldWhoseSeedsLineALayStopsIsBuiltAgain) {
  const std::vector<std::string> rows(AREA_SIDE, std::string(OPEN_WIDTH, '.'));
  std::vector<Cell> laid;
  for (std::int32_t row = STOP_FROM; row < STOP_TO; ++row) {
    laid.push_back(Cell{STOP_AT, row});
  }
  const Relaid made = relaid(Map{rows}, laid, IN_THE_LAST, FIRST_AREA);
  // Setup: the lay changes, through its seeds' lines alone, the first area's
  // field, whose window and whose neighbour hold none of its cells.
  ASSERT_GT(made.probe_changed, 0U);
  EXPECT_EQ(made.differ, 0U);
}

// A map of 2 by 2 areas crossed by a wall of cells touching only at their
// corners, along x + y = DIAGONAL, from column `gap` on: a gap below it lets
// a walk round, and a line along the wall passes through its corners.
constexpr std::int32_t DIAGONAL_SIDE = 64;
constexpr std::int32_t DIAGONAL = 63;
constexpr std::int32_t DIAGONAL_GAP = 4;
constexpr Cell BEYOND_THE_WALL{45, 33};
constexpr int MOST_STEPS = 4000;

Map diagonal_wall(std::int32_t gap) {
  std::vector<std::string> rows(static_cast<std::size_t>(DIAGONAL_SIDE),
                                std::string(DIAGONAL_SIDE, '.'));
  for (std::int32_t column = gap; column < DIAGONAL_SIDE; ++column) {
    rows.at(static_cast<std::size_t>(DIAGONAL - column))
        .at(static_cast<std::size_t>(column)) = '#';
  }
  return Map{rows};
}

// Of the cells a field over the whole map reaches `objective` from, how many
// a walk stepping by fields within areas, a tick a step, brings there.
struct Arrivals {
  int reached = 0;
  int arrived = 0;
};

Arrivals arrivals(const Map& map, Cell objective) {
  const CoarseGraph graph{map};
  const FlowField whole{map, objective};
  // A field is a function of the map, the graph and its key alone, so the
  // walks share one cache.
  AreaFields fields;
  Arrivals found;
  for (std::int32_t row = 0; row < map.height(); ++row) {
    for (std::int32_t column = 0; column < map.width(); ++column) {
      const Cell start{column, row};
      if (!map.passable(start) ||
          whole.distance(start) == FlowField::UNREACHABLE) {
        continue;
      }
      ++found.reached;
      Cell cell = start;
      for (int each = 0; each < MOST_STEPS; ++each) {
        if (cell.x == objective.x && cell.y == objective.y) {
          ++found.arrived;
          break;
        }
        const Step step =
            fields.field(fields.field_for(map, graph, cell, objective))
                .step(cell);
        fields.end_tick();
        cell = Cell{cell.x + step.x, cell.y + step.y};
      }
    }
  }
  return found;
}

TEST(AreaFields, EveryWalkRoundAWallOfTouchingCornersArrives) {
  const Map map = diagonal_wall(DIAGONAL_GAP);
  const Arrivals round = arrivals(map, BEYOND_THE_WALL);
  // Cells on the near side of the wall reach the objective only through the
  // gap.
  ASSERT_GT(round.reached, DIAGONAL_SIDE * DIAGONAL_SIDE / 2);
  EXPECT_EQ(round.arrived, round.reached);
  // Control: with no gap the wall seals the near side, which the field over
  // the whole map then reaches the objective from nowhere on.
  const Arrivals sealed = arrivals(diagonal_wall(0), BEYOND_THE_WALL);
  EXPECT_LT(sealed.reached, DIAGONAL_SIDE * DIAGONAL_SIDE / 2);
  EXPECT_EQ(sealed.arrived, sealed.reached);
}

// Where a walk from `from` toward `objective` first crosses onto column
// `border`, stepping by `step` from each cell: the row, or nothing when it
// stops first.
template <typename Step>
std::optional<std::int32_t> crossing(Cell from, std::int32_t border,
                                     Step step) {
  constexpr int STEPS = 400;
  Cell cell = from;
  for (int each = 0; each < STEPS && cell.x < border; ++each) {
    const murim::Step next = step(cell);
    if (next.x == 0 && next.y == 0) {
      return std::nullopt;
    }
    cell = Cell{cell.x + next.x, cell.y + next.y};
  }
  return cell.x >= border ? std::optional<std::int32_t>{cell.y} : std::nullopt;
}

TEST(AreaFields, AWalkCrossesAnAreaBorderWhereTheWholeMapFieldsWalkDoes) {
  // Three areas in a row. The border between the first two is open from row
  // 0 to row 27, an entrance whose middle is row 13; the objective lies in
  // the second area, behind a short wall that hides its line from the
  // entrance's top cells.
  constexpr std::int32_t WIDE = 96;
  constexpr std::int32_t HIGH = 32;
  constexpr std::int32_t BORDER = 32;
  constexpr std::int32_t ENTRANCE_END = 28;
  constexpr std::int32_t MIDDLE = 13;
  constexpr std::int32_t PILLAR = 48;
  constexpr std::int32_t PILLAR_END = 7;
  constexpr std::int32_t APART = 5;
  std::vector<std::string> rows(HIGH, std::string(WIDE, '.'));
  for (std::int32_t row = ENTRANCE_END; row < HIGH; ++row) {
    rows.at(static_cast<std::size_t>(row)).at(BORDER - 1) = '#';
  }
  for (std::int32_t row = 0; row < PILLAR_END; ++row) {
    rows.at(static_cast<std::size_t>(row)).at(PILLAR) = '#';
  }
  const Map map{rows};
  const CoarseGraph graph{map};
  const Cell from{5, 2};
  const Cell objective{60, 2};
  const FlowField whole{map, objective};
  const std::optional<std::int32_t> exact =
      crossing(from, BORDER, [&whole](Cell cell) { return whole.step(cell); });
  AreaFields fields;
  const std::optional<std::int32_t> stepped =
      crossing(from, BORDER, [&](Cell cell) {
        return fields.field(fields.field_for(map, graph, cell, objective))
            .step(cell);
      });
  // Setup: the whole-map walk crosses well away from the entrance's middle,
  // where a walk drawn to the middle node would cross.
  ASSERT_TRUE(exact.has_value());
  ASSERT_GE(std::abs(exact.value_or(MIDDLE) - MIDDLE), APART);
  ASSERT_TRUE(stepped.has_value());
  EXPECT_LE(std::abs(stepped.value_or(MIDDLE) - exact.value_or(0)), 1)
      << "fields within areas cross at row " << stepped.value_or(-1)
      << ", the whole-map field at row " << exact.value_or(-1);
}

}  // namespace
}  // namespace murim
