#include "live_scenario.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "attack.hpp"
#include "calendar.hpp"
#include "growth.hpp"
#include "life.hpp"
#include "map.hpp"
#include "occupation.hpp"
#include "skill.hpp"
#include "talent.hpp"
#include "village.hpp"
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

// Each village's layout round its centre, as ADR 0081's prototype laid the
// scenario out: four rows of eleven houses of 8, a smithy, an apothecary and
// a hall on the square's edge, and two rows of six fields beyond the houses;
// a structure off the map, or on a cell a person or structure takes, is not
// laid.
constexpr std::array<std::int32_t, 4> HOUSE_ROWS{-11, -8, 6, 9};
constexpr std::int32_t HOUSE_FROM = -16;
constexpr std::int32_t HOUSE_TO = 14;
constexpr std::int32_t HOUSE_STEP = 3;
constexpr std::uint32_t HOUSE_HOUSING = 8;
constexpr std::uint32_t HALL_HOUSING = 12;
constexpr std::uint32_t WORKSHOP_SERVES = 40;
constexpr std::array<std::int32_t, 2> FIELD_ROWS{-17, 13};
constexpr std::int32_t FIELD_STEP = 6;
constexpr std::uint32_t FIELD_HARVEST = 600;
// The workshops' and the hall's corners from the centre: the smithy and the
// hall to the west, the apothecary to the east.
constexpr std::int32_t WEST_SIDE = -17;
constexpr std::int32_t EAST_SIDE = 15;
constexpr std::int32_t WORKSHOP_ROW = -3;
constexpr std::int32_t HALL_ROW = 1;
constexpr Footprint HOUSE{2, 2};
constexpr Footprint WORKSHOP{3, 2};
constexpr Footprint HALL{3, 3};
constexpr Footprint FIELD{4, 4};
// The square each village keeps clear round its centre, and the growth's
// interval, longer than any run of the bench, so no village grows.
constexpr Footprint SQUARE{13, 9};
constexpr std::int32_t VICINITY = 24;
constexpr std::uint32_t NEVER = 1'000'000;
constexpr std::uint64_t ADULT_YEARS = 20;

// Lays the village of army `residents`, centred on `centre`, onto `scenario`,
// clear of the cells in `taken`, which it takes.
void lay(LiveScenario& scenario, std::size_t residents, Cell centre,
         std::set<std::pair<std::int32_t, std::int32_t>>& taken) {
  VillageSpec spec;
  spec.residents = residents;
  const auto place = [&](Cell corner, Footprint footprint, bool blocks,
                         Role role, Holding holding, const std::string& kind) {
    const std::vector<Cell> cells = cells_of(corner, footprint);
    const bool clear = std::all_of(cells.begin(), cells.end(), [&](Cell cell) {
      return cell.x >= 0 && cell.y >= 0 && cell.x < MAP_SIDE &&
             cell.y < MAP_SIDE && taken.count({cell.x, cell.y}) == 0;
    });
    if (!clear) {
      return;
    }
    for (const Cell cell : cells) {
      taken.insert({cell.x, cell.y});
    }
    spec.structures.push_back(scenario.structures.size());
    spec.holdings.push_back(holding);
    spec.kinds.push_back(kind);
    scenario.structures.push_back(
        Structure{corner, DURABILITY, footprint, blocks, role});
  };
  Holding house;
  house.housing = HOUSE_HOUSING;
  for (const std::int32_t row : HOUSE_ROWS) {
    for (std::int32_t column = HOUSE_FROM; column <= HOUSE_TO;
         column += HOUSE_STEP) {
      place({centre.x + column, centre.y + row}, HOUSE, true, Role::HOME, house,
            "house");
    }
  }
  Holding smithy;
  smithy.arms = WORKSHOP_SERVES;
  place({centre.x + WEST_SIDE, centre.y + WORKSHOP_ROW}, WORKSHOP, true,
        Role::SQUARE, smithy, "smithy");
  Holding apothecary;
  apothecary.tends = WORKSHOP_SERVES;
  place({centre.x + EAST_SIDE, centre.y + WORKSHOP_ROW}, WORKSHOP, true,
        Role::SQUARE, apothecary, "apothecary");
  Holding hall;
  hall.housing = HALL_HOUSING;
  place({centre.x + WEST_SIDE, centre.y + HALL_ROW}, HALL, true, Role::SQUARE,
        hall, "hall");
  Holding field;
  field.harvest = FIELD_HARVEST;
  for (const std::int32_t row : FIELD_ROWS) {
    for (std::int32_t column = HOUSE_FROM; column <= HOUSE_TO;
         column += FIELD_STEP) {
      place({centre.x + column, centre.y + row}, FIELD, false, Role::EDGE,
            field, "field");
    }
  }
  scenario.villages.push_back(spec);
}

// The base pack's seven occupations as they stood before #379 gave monks a
// temple and the innkeeper an inn, which the scenario's villages do not have
// (packs/wuxia/data/occupations.json and life.json): their schedules, the
// merchant's residents a place, which occupation is the fallback, and the
// working age and waking hours.
constexpr std::array<Band, 3> MERCHANT_DAY{
    {{0, Where::HOME}, {7, Where::WORK}, {20, Where::HOME}}};
constexpr std::array<Band, 4> WORKSHOP_DAY{{{0, Where::HOME},
                                            {7, Where::WORK},
                                            {19, Where::SQUARE},
                                            {21, Where::HOME}}};
constexpr std::array<Band, 3> INNKEEPER_DAY{
    {{0, Where::HOME}, {8, Where::WORK}, {22, Where::HOME}}};
constexpr std::array<Band, 4> FARMER_DAY{{{0, Where::HOME},
                                          {6, Where::WORK},
                                          {18, Where::SQUARE},
                                          {21, Where::HOME}}};
constexpr std::array<Band, 3> MONK_DAY{
    {{0, Where::HOME}, {5, Where::WORK}, {21, Where::HOME}}};
constexpr std::array<Band, 3> BEGGAR_DAY{
    {{0, Where::HOME}, {7, Where::WORK}, {22, Where::HOME}}};
constexpr std::array<Band, 3> IDLE_DAY{
    {{0, Where::HOME}, {8, Where::SQUARE}, {18, Where::HOME}}};
constexpr std::uint32_t RESIDENTS_A_MERCHANT = 30;
constexpr std::size_t BEGGAR = 6;
constexpr std::uint32_t WORKING_AGE = 16;
constexpr Waking AWAKE{6, 22};

template <std::size_t N>
std::vector<Band> bands(const std::array<Band, N>& day) {
  return {day.begin(), day.end()};
}

// Those seven occupations, selling nothing, as the scenario has no trade.
Occupations occupations() {
  return Occupations{
      {Occupation{
           "merchant", OnSquare{RESIDENTS_A_MERCHANT}, bands(MERCHANT_DAY), {}},
       Occupation{
           "physician", AtWorkplace{"apothecary", 2}, bands(WORKSHOP_DAY), {}},
       Occupation{"smith", AtWorkplace{"smithy", 2}, bands(WORKSHOP_DAY), {}},
       Occupation{
           "innkeeper", AtWorkplace{"hall", 1}, bands(INNKEEPER_DAY), {}},
       Occupation{"farmer", AtWorkplace{"field", 2}, bands(FARMER_DAY), {}},
       Occupation{"monk", Nowhere{}, bands(MONK_DAY), {}},
       Occupation{"beggar", Nowhere{}, bands(BEGGAR_DAY), {}}},
      WORKING_AGE,
      BEGGAR,
      bands(IDLE_DAY),
      AWAKE};
}

}  // namespace

LiveScenario live_scenario() {
  LiveScenario scenario{
      Seed{SCENARIO_SEED},
      std::vector<std::string>(MAP_SIDE, std::string(MAP_SIDE, '.')),
      {},
      rules(),
      0,
      {},
      {},
      {}};
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
  std::set<std::pair<std::int32_t, std::int32_t>> taken;
  for (const ArmySpec& army : scenario.armies) {
    for (const Recruit& recruit : army.members) {
      taken.insert({recruit.cell.x, recruit.cell.y});
    }
  }
  // Each homeland lists its two villages, then its bands.
  for (std::size_t army = scenario.battle_armies; army < scenario.armies.size();
       army += static_cast<std::size_t>(2 + BANDS)) {
    for (const std::size_t village : {army, army + 1}) {
      lay(scenario, village, scenario.armies.at(village).objective, taken);
    }
  }
  scenario.life.founding_age = Spread{ADULT_YEARS * DAYS_PER_YEAR, 0};
  scenario.life.occupations = occupations();
  Growth growth;
  growth.interval = NEVER;
  growth.tiers = {Tier{0, SQUARE, VICINITY, {}}};
  scenario.life.growth = growth;
  return scenario;
}

}  // namespace murim
