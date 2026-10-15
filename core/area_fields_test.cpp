#include "area_fields.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

}  // namespace
}  // namespace murim
