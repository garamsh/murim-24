#include "live_scenario.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "attack.hpp"
#include "calendar.hpp"
#include "map.hpp"
#include "skill.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr std::uint64_t SCENARIO_SEED = 2026;
constexpr std::int32_t MAP_SIDE = 256;
// People stand a cell apart, as wide as a body is drawn.
constexpr std::int32_t SEPARATION = CELL_SIZE;
// People close on a foe within the region's sight.
constexpr std::int32_t SIGHT = 6;
constexpr std::uint64_t HEALTH = 3'000;
constexpr std::uint32_t DURABILITY = 3'000;

// A melee strike every half second, sweeping an arc at arm's length, as the
// M1 scenario's lines strike.
constexpr Skill JAB{3 * CELL_SIZE / 2,    0, 10,        0,
                    TICKS_PER_SECOND / 2, 0, Shape::ARC};

// Four factions: the north's west and east fight one battle, the south's
// west and east the other, and no faction is hostile to the other half's.
constexpr std::size_t FACTIONS = 4;
constexpr std::array<std::size_t, 2> NORTH{0, 1};
constexpr std::array<std::size_t, 2> SOUTH{2, 3};

// Each battle: two lines of ten armies, each a block of five columns by ten
// rows, a cell apart across the battle's middle column.
constexpr std::int32_t LINE_ARMIES = 10;
constexpr Footprint BATTLE_BLOCK{5, 10};
constexpr std::int32_t LINE_GAP = 1;

// Each faction's two villages, 64 rows apart on its half's outer column, of
// 250 people each standing about its centre, and five bands of 50 on the
// road between them, each bound for the village it walks away from at the
// start, so it is on the road throughout the timed ticks.
constexpr std::int32_t FIRST_VILLAGE_ROW = 16;
constexpr std::int32_t VILLAGE_INSET = 40;
constexpr std::int32_t VILLAGE_ROWS = 64;
constexpr Footprint VILLAGE_BLOCK{25, 10};
constexpr std::int32_t ROAD_OFFSET = 20;
constexpr std::int32_t BANDS = 5;
constexpr Footprint BAND_BLOCK{5, 10};

Rules rules() {
  std::vector<std::vector<bool>> hostile(FACTIONS,
                                         std::vector<bool>(FACTIONS, false));
  for (const std::array<std::size_t, 2>& pair : {NORTH, SOUTH}) {
    hostile.at(pair.at(0)).at(pair.at(1)) = true;
    hostile.at(pair.at(1)).at(pair.at(0)) = true;
  }
  Rules live{SEPARATION, hostile, {DURABILITY, DURABILITY / 2, 1}};
  live.sight = SIGHT;
  live.people_capacity = LIVE_CAPACITY;
  live.tokens = default_tokens();
  return live;
}

// A block of `footprint` people from `corner`.
std::vector<Recruit> block(Cell corner, Footprint footprint) {
  std::vector<Recruit> members;
  for (const Cell cell : cells_of(corner, footprint)) {
    members.push_back(Recruit{cell, HEALTH});
  }
  return members;
}

// The two lines of a battle about `centre` between `pair`'s factions.
void battle(std::vector<ArmySpec>& armies, Cell centre,
            const std::array<std::size_t, 2>& pair) {
  const std::int32_t first_row =
      centre.y - (LINE_ARMIES * BATTLE_BLOCK.height / 2);
  for (std::int32_t line = 0; line < LINE_ARMIES; ++line) {
    const std::int32_t top = first_row + (line * BATTLE_BLOCK.height);
    const std::int32_t middle = top + (BATTLE_BLOCK.height / 2);
    armies.push_back(ArmySpec{
        {centre.x - 1, middle},
        block({centre.x - LINE_GAP - BATTLE_BLOCK.width, top}, BATTLE_BLOCK),
        pair.at(0),
        {JAB}});
    armies.push_back(ArmySpec{{centre.x + 1, middle},
                              block({centre.x + LINE_GAP, top}, BATTLE_BLOCK),
                              pair.at(1),
                              {JAB}});
  }
}

// Faction `faction`'s two villages, the first centred on `first` and the
// second VILLAGE_ROWS south of it, and the bands on its road, ROAD_OFFSET
// columns nearer the map's middle.
void homeland(std::vector<ArmySpec>& armies, std::size_t faction, Cell first) {
  const std::int32_t north = first.y;
  const std::int32_t road =
      first.x + (first.x < MAP_SIDE / 2 ? ROAD_OFFSET : -ROAD_OFFSET);
  const std::array<Cell, 2> centres{first, Cell{first.x, north + VILLAGE_ROWS}};
  for (const Cell centre : centres) {
    armies.push_back(ArmySpec{centre,
                              block({centre.x - (VILLAGE_BLOCK.width / 2),
                                     centre.y - (VILLAGE_BLOCK.height / 2)},
                                    VILLAGE_BLOCK),
                              faction,
                              {JAB}});
  }
  const std::int32_t spacing = VILLAGE_ROWS / BANDS;
  for (std::int32_t band = 0; band < BANDS; ++band) {
    const std::int32_t top = north + (band * spacing);
    // Bands in the road's northern half walk south, the rest north.
    const bool southward =
        top + (BAND_BLOCK.height / 2) < north + (VILLAGE_ROWS / 2);
    const Cell bound = southward ? centres.at(1) : centres.at(0);
    armies.push_back(
        ArmySpec{bound,
                 block({road - (BAND_BLOCK.width / 2), top}, BAND_BLOCK),
                 faction,
                 {JAB}});
  }
}

}  // namespace

LiveScenario live_scenario() {
  LiveScenario scenario{
      Seed{SCENARIO_SEED},
      std::vector<std::string>(MAP_SIDE, std::string(MAP_SIDE, '.')),
      {},
      rules(),
      0};
  constexpr std::int32_t MIDDLE = MAP_SIDE / 2;
  constexpr std::int32_t QUARTER = MAP_SIDE / 4;
  battle(scenario.armies, Cell{MIDDLE, QUARTER}, NORTH);
  battle(scenario.armies, Cell{MIDDLE, MIDDLE + QUARTER}, SOUTH);
  scenario.battle_armies = scenario.armies.size();
  for (const std::array<std::size_t, 2>& pair : {NORTH, SOUTH}) {
    const std::int32_t north = FIRST_VILLAGE_ROW + (pair == NORTH ? 0 : MIDDLE);
    homeland(scenario.armies, pair.at(0), Cell{VILLAGE_INSET, north});
    homeland(scenario.armies, pair.at(1),
             Cell{MAP_SIDE - VILLAGE_INSET, north});
  }
  return scenario;
}

}  // namespace murim
