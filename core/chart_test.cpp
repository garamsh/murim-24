#include "chart.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr Footprint MAP{128, 128};
constexpr std::int32_t REACH = 15;
constexpr std::size_t FACTIONS = 3;
constexpr std::int32_t BITS_PER_BYTE = 8;
constexpr Cell AREA_MIDDLE{AREA_SIDE / 2, AREA_SIDE / 2};
constexpr Cell SOUTH_WEST{10, 100};
constexpr Cell NORTH_EAST{90, 20};

std::size_t seen_count(const Chart& chart, Footprint size) {
  std::size_t count = 0;
  for (const Cell cell : cells_of(Cell{0, 0}, size)) {
    count += chart.seen(cell) ? 1U : 0U;
  }
  return count;
}

// Whether a 40 x 40 map's chart restores from `blocks`, with no village.
bool restores(const std::vector<ChartBlock>& blocks) {
  constexpr Footprint SMALL{40, 40};
  return Chart::restore(SMALL, blocks, {}, FACTIONS).has_value();
}

ChartBlock block_with(std::uint32_t area, Cell within) {
  ChartBlock block{area, {}};
  const std::int32_t bit = (within.y * AREA_SIDE) + within.x;
  block.bits.at(static_cast<std::size_t>(bit / BITS_PER_BYTE)) =
      static_cast<std::uint8_t>(1U << (bit % BITS_PER_BYTE));
  return block;
}

TEST(Chart, SeeMarksEveryCellWithinReachAndNoOther) {
  Chart chart{MAP};
  ASSERT_TRUE(chart.empty());
  EXPECT_TRUE(chart.see(Cell{64, 64}, REACH));
  // The cells whose centres lie within 15 cells of a centre: 709.
  EXPECT_EQ(seen_count(chart, MAP), 709U);
  EXPECT_TRUE(chart.seen(Cell{64 + REACH, 64}));
  EXPECT_TRUE(chart.seen(Cell{64 + 9, 64 + 12}));
  EXPECT_FALSE(chart.seen(Cell{64 + REACH + 1, 64}));
  EXPECT_FALSE(chart.seen(Cell{64 + 11, 64 + 11}));
  EXPECT_FALSE(chart.empty());
}

TEST(Chart, SeeReportsOnlyCellsNotSeenBefore) {
  Chart chart{MAP};
  ASSERT_TRUE(chart.see(Cell{40, 40}, REACH));
  const std::uint64_t revision = chart.revision();
  EXPECT_FALSE(chart.see(Cell{40, 40}, REACH));
  EXPECT_EQ(chart.revision(), revision);
  // Control: a step aside sees a new edge.
  EXPECT_TRUE(chart.see(Cell{41, 40}, REACH));
  EXPECT_GT(chart.revision(), revision);
}

TEST(Chart, SeeStopsAtTheMapsEdge) {
  constexpr Footprint SMALL{40, 20};
  Chart chart{SMALL};
  chart.see(Cell{0, 0}, REACH);
  EXPECT_TRUE(chart.seen(Cell{0, 0}));
  EXPECT_TRUE(chart.seen(Cell{REACH, 0}));
  EXPECT_FALSE(chart.seen(Cell{-1, 0}));
  EXPECT_FALSE(chart.seen(Cell{0, SMALL.height}));
  chart.see(Cell{SMALL.width - 1, SMALL.height - 1}, REACH);
  EXPECT_TRUE(chart.seen(Cell{SMALL.width - 1, SMALL.height - 1}));
  EXPECT_FALSE(chart.seen(Cell{SMALL.width, SMALL.height - 1}));
}

TEST(Chart, KeepsABlockOnlyForAreasWithASeenCell) {
  Chart chart{MAP};
  chart.see(AREA_MIDDLE, REACH);
  EXPECT_EQ(chart.blocks().size(), 1U);
  // Control: a sight across an area's corner touches the four areas there.
  chart.see(Cell{AREA_SIDE * 2, AREA_SIDE * 2}, REACH);
  const std::vector<ChartBlock> blocks = chart.blocks();
  ASSERT_EQ(blocks.size(), 5U);
  for (std::size_t index = 1; index < blocks.size(); ++index) {
    EXPECT_LT(blocks.at(index - 1).area, blocks.at(index).area);
  }
}

TEST(Chart, RememberReportsOnlyAChangedMemory) {
  Chart chart{MAP};
  const VillageMemory standing{1, true, VillageStage::STANDING};
  EXPECT_TRUE(chart.remember(2, standing));
  ASSERT_EQ(chart.villages().size(), 3U);
  EXPECT_FALSE(chart.villages().at(0));
  EXPECT_EQ(chart.villages().at(2), standing);
  const std::uint64_t revision = chart.revision();
  EXPECT_FALSE(chart.remember(2, standing));
  EXPECT_EQ(chart.revision(), revision);
  // Control: the same village burnt is a change.
  const VillageMemory burnt{1, false, VillageStage::RUINS};
  EXPECT_TRUE(chart.remember(2, burnt));
  EXPECT_EQ(chart.villages().at(2), burnt);
  EXPECT_GT(chart.revision(), revision);
}

TEST(Chart, RestoresWhatItGave) {
  Chart chart{MAP};
  chart.see(SOUTH_WEST, REACH);
  chart.see(NORTH_EAST, REACH);
  chart.remember(4, VillageMemory{2, true, VillageStage::DAMAGED});
  const std::optional<Chart> restored =
      Chart::restore(MAP, chart.blocks(), chart.villages(), FACTIONS);
  ASSERT_TRUE(restored.has_value());
  const Chart back = restored.value_or(Chart{MAP});
  EXPECT_EQ(seen_count(back, MAP), seen_count(chart, MAP));
  for (const Cell cell : cells_of(Cell{0, 0}, MAP)) {
    ASSERT_EQ(back.seen(cell), chart.seen(cell));
  }
  EXPECT_EQ(back.villages(), chart.villages());
}

TEST(Chart, RefusesBlocksNoChartGives) {
  const ChartBlock first = block_with(0, Cell{3, 3});
  const ChartBlock second = block_with(1, Cell{3, 3});
  // Control: two areas in order, each with a seen cell on the map.
  EXPECT_TRUE(restores({first, second}));
  EXPECT_FALSE(restores({second, first}));
  EXPECT_FALSE(restores({first, first}));
  EXPECT_FALSE(restores({block_with(4, Cell{3, 3})}));
  EXPECT_FALSE(restores({ChartBlock{0, {}}}));
  // Area 1 spans cells 32 to 63 across, and the map ends at 39.
  EXPECT_FALSE(restores({block_with(1, Cell{8, 3})}));
  EXPECT_TRUE(restores({block_with(1, Cell{7, 3})}));
}

TEST(Chart, RefusesAVillageOwnedByNoFaction) {
  const std::vector<std::optional<VillageMemory>> owned{
      VillageMemory{FACTIONS - 1, true, VillageStage::STANDING}};
  EXPECT_TRUE(Chart::restore(MAP, {}, owned, FACTIONS));
  const std::vector<std::optional<VillageMemory>> unowned{
      VillageMemory{FACTIONS, true, VillageStage::STANDING}};
  EXPECT_FALSE(Chart::restore(MAP, {}, unowned, FACTIONS));
}

TEST(Chart, AMovedClaimCentreChangesTheMemoryAndRevision) {
  Chart chart{MAP};
  const VillageMemory first{1, true, VillageStage::STANDING, {8, 9}};
  ASSERT_TRUE(chart.remember(0, first));
  const auto revision = chart.revision();
  EXPECT_FALSE(chart.remember(0, first));
  EXPECT_EQ(chart.revision(), revision);
  const VillageMemory moved{1, true, VillageStage::STANDING, {18, 9}};
  EXPECT_TRUE(chart.remember(0, moved));
  EXPECT_EQ(chart.villages().front(), moved);
  EXPECT_GT(chart.revision(), revision);
}

TEST(Chart, RefusesAVillageClaimCentreOffTheMap) {
  const auto accepts = [](Cell centre) {
    return Chart::restore(
               MAP, {},
               {VillageMemory{0, true, VillageStage::STANDING, centre}},
               FACTIONS)
        .has_value();
  };
  EXPECT_TRUE(accepts({0, 0}));
  EXPECT_TRUE(accepts({MAP.width - 1, MAP.height - 1}));
  EXPECT_FALSE(accepts({-1, 0}));
  EXPECT_FALSE(accepts({0, -1}));
  EXPECT_FALSE(accepts({MAP.width, 0}));
  EXPECT_FALSE(accepts({0, MAP.height}));
}

}  // namespace
}  // namespace murim
