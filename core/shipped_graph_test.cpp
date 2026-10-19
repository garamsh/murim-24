#include "shipped_graph.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "areas.hpp"
#include "coarse_graph.hpp"
#include "compiled_map.hpp"
#include "encoding.hpp"
#include "map.hpp"

namespace murim {
namespace {

// Wider and taller than a region, and not a whole number of areas, so the
// map holds four regions and areas cut short at its right and bottom edges.
constexpr std::int32_t WIDE = 300;
constexpr std::int32_t TALL = 280;
constexpr MapHash BUILT_FOR{0x5EED};
constexpr MapHash ANOTHER{0xBEEF};

// Open ground with a wall splitting the first area into two components, a
// blocked cell in each area of the second row of areas, and a wall across the
// region border with one gap.
Map varied_map() {
  std::vector<std::string> rows(static_cast<std::size_t>(TALL),
                                std::string(WIDE, '.'));
  for (std::int32_t row = 0; row < AREA_SIDE; ++row) {
    rows.at(static_cast<std::size_t>(row)).at(AREA_SIDE / 2) = '#';
  }
  for (std::int32_t column = AREA_SIDE / 3; column < WIDE;
       column += AREA_SIDE) {
    rows.at(AREA_SIDE + (AREA_SIDE / 2)).at(static_cast<std::size_t>(column)) =
        '#';
  }
  const std::int32_t border = REGION_AREAS * AREA_SIDE;
  for (std::int32_t row = 0; row < TALL; ++row) {
    if (row != TALL / 2) {
      rows.at(static_cast<std::size_t>(row))
          .at(static_cast<std::size_t>(border) + 3) = '#';
    }
  }
  return Map{rows};
}

// `payload` framed as ship_graph frames a graph for `map`.
std::vector<std::uint8_t> framed(const std::vector<std::uint8_t>& payload,
                                 MapHash map) {
  Writer out;
  for (const char letter : std::string{"MURIMGRF"}) {
    out.u8(static_cast<std::uint8_t>(letter));
  }
  out.u32(GRAPH_VERSION);
  out.u64(static_cast<std::uint64_t>(map));
  out.size(payload.size());
  std::vector<std::uint8_t>& bytes = out.bytes();
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  out.u64(checksum(bytes, 0, bytes.size()));
  return bytes;
}

// The payload of the graph of one open area of 32 cells, holding one node
// `across` cells along its top row: no map gives it, since it has no
// entrance, but every index in it is in range when the node lies inside.
std::vector<std::uint8_t> one_node_at(std::uint8_t across) {
  Writer out;
  out.u8(0);
  out.i32(AREA_SIDE);
  out.i32(AREA_SIDE);
  out.u16(1);
  out.flag(true);
  out.flag(false);
  out.u8(1);
  out.u8(across);
  out.u8(0);
  out.u16(0);
  out.u8(0);
  out.u16(0);
  out.u8(0);
  out.u16(0);
  return out.bytes();
}

TEST(ShippedGraph, AShippedGraphReadsBackAsTheGraphBuiltFromItsMap) {
  const Map map = varied_map();
  const CoarseGraph built{map};
  // The map holds what each part of the graph is written for: an open area,
  // an obstructed one, an area of two components and region border nodes.
  ASSERT_TRUE(built.costed_in_closed_form(built.area_of(Cell{WIDE - 1, 0})));
  ASSERT_FALSE(built.costed_in_closed_form(
      built.area_of(Cell{AREA_SIDE / 3, AREA_SIDE + (AREA_SIDE / 2)})));
  ASSERT_EQ(built.component_of(Cell{0, 0}), 0);
  ASSERT_EQ(built.component_of(Cell{AREA_SIDE - 1, 0}), 1);
  ASSERT_GT(built.columns(), REGION_AREAS);
  ASSERT_GT(built.node_count(), 0U);

  const auto read = read_shipped_graph(ship_graph(built, BUILT_FOR), BUILT_FOR);

  ASSERT_TRUE(std::holds_alternative<CoarseGraph>(read));
  EXPECT_TRUE(std::get<CoarseGraph>(read) == built);
}

TEST(ShippedGraph, AGraphShippedForAnotherMapIsRefused) {
  const std::vector<std::uint8_t> bytes =
      ship_graph(CoarseGraph{varied_map()}, BUILT_FOR);

  const auto other = read_shipped_graph(bytes, ANOTHER);
  const auto own = read_shipped_graph(bytes, BUILT_FOR);

  ASSERT_TRUE(std::holds_alternative<GraphFault>(other));
  EXPECT_EQ(std::get<GraphFault>(other), GraphFault::OTHER_MAP);
  EXPECT_TRUE(std::holds_alternative<CoarseGraph>(own));
}

TEST(ShippedGraph, ADamagedOrForeignGraphIsRefusedForWhatIsWrongWithIt) {
  const std::vector<std::uint8_t> bytes =
      ship_graph(CoarseGraph{varied_map()}, BUILT_FOR);
  constexpr std::size_t VERSION_AT = 8;
  const auto fault = [](const std::vector<std::uint8_t>& read) {
    const auto result = read_shipped_graph(read, BUILT_FOR);
    return std::holds_alternative<GraphFault>(result)
               ? std::optional<GraphFault>{std::get<GraphFault>(result)}
               : std::nullopt;
  };

  std::vector<std::uint8_t> damaged = bytes;
  damaged.at(bytes.size() / 2) ^= 1U;
  std::vector<std::uint8_t> versioned = bytes;
  ++versioned.at(VERSION_AT);
  std::vector<std::uint8_t> foreign = bytes;
  foreign.at(0) = 'X';
  const std::vector<std::uint8_t> cut(bytes.begin(), bytes.end() - 1);

  EXPECT_EQ(fault(damaged), GraphFault::CORRUPTED);
  EXPECT_EQ(fault(versioned), GraphFault::UNSUPPORTED_VERSION);
  EXPECT_EQ(fault(foreign), GraphFault::NOT_A_GRAPH);
  EXPECT_EQ(fault(cut), GraphFault::CORRUPTED);
  EXPECT_TRUE(std::holds_alternative<CoarseGraph>(
      read_shipped_graph(bytes, BUILT_FOR)));
}

TEST(ShippedGraph, ANodeOutsideItsAreaIsRefusedThoughItsChecksumHolds) {
  const auto outside =
      read_shipped_graph(framed(one_node_at(AREA_SIDE), BUILT_FOR), BUILT_FOR);
  const auto inside = read_shipped_graph(
      framed(one_node_at(AREA_SIDE - 1), BUILT_FOR), BUILT_FOR);

  ASSERT_TRUE(std::holds_alternative<GraphFault>(outside));
  EXPECT_EQ(std::get<GraphFault>(outside), GraphFault::CORRUPTED);
  ASSERT_TRUE(std::holds_alternative<CoarseGraph>(inside));
  EXPECT_EQ(std::get<CoarseGraph>(inside).node_count(), 1U);
}

}  // namespace
}  // namespace murim
