#include "area_fields.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

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
