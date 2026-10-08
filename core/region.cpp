#include "region.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"
#include "random.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr Footprint REGION{REGION_SIDE, REGION_SIDE};
constexpr std::size_t CELLS = static_cast<std::size_t>(REGION_SIDE) *
                              static_cast<std::size_t>(REGION_SIDE);

// Points the terrain is partitioned around, and the random cells each
// faction's capital is chosen among.
constexpr std::int32_t TERRAIN_SITES = 40;
constexpr std::int32_t CAPITAL_CANDIDATES = 16;

constexpr std::int32_t MIN_VILLAGES = 6;
constexpr std::int32_t VILLAGE_COUNTS = 3;

// People stand a cell apart, as wide as a body is drawn.
constexpr std::int32_t SEPARATION = CELL_SIZE;
// Combat is paced by this data at 30 ticks a second: a duel to the death in
// 10 to 30 seconds, twenty a side in 10 seconds to 2 minutes, and a band of
// twenty raiders leaving a hamlet in ruins in 1 to 5 minutes. Health is eight
// to twelve guard strikes; durability sets the raid.
constexpr std::uint32_t DURABILITY = 10'000;
constexpr std::uint64_t HEALTH = 120;
// An event reaches everyone within 32 cells of it as a rumour, a quarter of
// the region's side.
constexpr std::int32_t RUMOUR_REACH = 32;
// The rules of life in region_data().
constexpr std::uint32_t GROWTH = 10;
constexpr std::uint32_t GROWTH_SPREAD = 5;
constexpr std::uint32_t LIMIT = 500;
constexpr std::uint32_t LIMIT_SPREAD = 200;
constexpr std::uint32_t LIFESPAN_DAYS = 60 * 120;
constexpr std::uint32_t LIFESPAN_SPREAD_DAYS = 15 * 120;
constexpr std::uint32_t DISCIPLINE_WIDENING = 200;
constexpr std::uint32_t UPPER_WIDENING = 100;
// Kinship in region_data(), ages in days.
constexpr std::uint32_t YEAR = 120;
constexpr std::uint32_t FOUNDING_AGE = 22 * YEAR;
constexpr std::uint32_t FOUNDING_SPREAD = 6 * YEAR;
constexpr std::uint32_t MARRYING_FROM = 16 * YEAR;
constexpr std::uint32_t MARRYING_TO = 40 * YEAR;
constexpr std::int32_t NEIGHBOURHOOD = 80;
constexpr std::uint32_t MARRIAGE_CHANCE = 500;
constexpr std::uint32_t BEARING_FROM = 18 * YEAR;
constexpr std::uint32_t BEARING_TO = 40 * YEAR;
constexpr std::uint32_t BIRTH_CHANCE = 400;
constexpr std::uint32_t BIRTH_SPACING = 2 * YEAR;
// Husbandry and the structure kinds in region_data().
constexpr std::uint32_t HUNGER_DAYS = 10;
constexpr std::int32_t MIGRATION_REACH = 48;
constexpr std::uint32_t HOUSE_HOUSING = 8;
constexpr std::uint32_t HALL_HOUSING = 12;
constexpr std::uint32_t GRANARY_STORAGE = 2'400;
constexpr std::uint32_t STOREHOUSE_STORAGE = 3'600;
constexpr std::uint32_t FIELD_YIELD = 600;
constexpr std::uint32_t WORKSHOP_SERVES = 40;
constexpr std::uint32_t SMITHY_EDGE = 250;
constexpr std::uint32_t APOTHECARY_HEALING = 12;
// Growth in region_data(), distances in cells.
constexpr std::uint32_t GROWTH_INTERVAL_DAYS = 30;
constexpr std::uint32_t GROWTH_PRESSURE = 200;
constexpr std::int32_t GROWTH_REACH = 40;
constexpr std::int32_t BUILD_VICINITY = 8;
constexpr std::int32_t SETTLEMENT_SPACING = 3;
constexpr std::int32_t TERRITORY_CLAIM = 16;
constexpr std::uint32_t SETTLERS = 6;
constexpr std::uint32_t RAIDERS = 10;
constexpr std::uint32_t SELF_RELIANCE = 20;
// Sects in region_data(), ages in days and chances per mille a year.
constexpr std::uint32_t ADMISSION_AGE = 12 * YEAR;
constexpr std::uint32_t ASSESSED_TALENT = 550;
constexpr std::uint32_t PROVEN_ATTAINMENT = 20;
constexpr std::int32_t ADMISSION_REACH = 48;
constexpr std::uint32_t ADMISSION_CHANCE = 300;
constexpr std::uint32_t ELDER_SHARE = 333;
constexpr std::uint32_t FOUNDER_ATTAINMENT = 120;
constexpr std::uint32_t HALL_PUPILS = 4;
constexpr std::uint32_t MANUAL_HALL_PUPILS = 4;
constexpr std::uint32_t TRAINING_GROUND_PUPILS = 12;
constexpr std::uint32_t SCATTER_CHANCE = 500;
constexpr std::int32_t SCATTER_REACH = 16;
constexpr std::uint32_t QI_GROWTH = 1;
constexpr std::uint32_t QI_GROWTH_SPREAD = 1;
constexpr std::uint32_t QI_LIMIT = 8'000;
constexpr std::uint32_t QI_LIMIT_SPREAD = 7'000;
constexpr std::uint32_t QI_RECOVERY = 10;
constexpr std::uint64_t QI_RECOVERY_TICKS = 30;
constexpr std::uint32_t COMPREHENSION = 300;
constexpr std::uint32_t TEACHING = 200;
constexpr std::uint32_t READING_COST = 500;
constexpr std::uint32_t HALF = 500;
constexpr std::uint32_t SEVEN_TENTHS = 700;
constexpr std::uint32_t NINE_TENTHS = 900;

// Diplomacy in region_data(): war on a faction at peace, outnumbering it by
// half again, at 1 in 10 a year; peace while outnumbered in a declared war at
// 1 in 5 a year; a declaration holding 5 years.
constexpr std::uint32_t WAR_MARGIN = 1'500;
constexpr std::uint32_t WAR_CHANCE = 100;
constexpr std::uint32_t PEACE_CHANCE = 200;
constexpr std::uint32_t HOLD_DAYS = 5 * 120;

// The realms of cultivation, from third-rate to life-and-death, each
// reached at a store of qi and striking a tenth harder than the last.
std::vector<Realm> realms() {
  constexpr std::uint32_t STEP = 100;
  const std::vector<std::pair<NameEntry, std::uint32_t>> ladder{
      {{"삼류", "Third-rate"}, 1'000},
      {{"이류", "Second-rate"}, 3'000},
      {{"일류", "First-rate"}, 6'000},
      {{"절정", "Peak"}, 10'000},
      {{"초절정", "Beyond peak"}, 15'000},
      {{"화경", "Transformation"}, 21'000},
      {{"현경", "Mystery"}, 28'000},
      {{"생사경", "Life and death"}, 36'000},
  };
  std::vector<Realm> realms;
  realms.reserve(ladder.size());
  for (const auto& [name, threshold] : ladder) {
    realms.push_back(Realm{
        name, threshold,
        PER_MILLE + (STEP * static_cast<std::uint32_t>(realms.size() + 1))});
  }
  return realms;
}

// The sects' arts, one for each manual: Hwasan's plum-blossom sword,
// orthodox and yang; the Heavenly Demon's saber, demonic and yang; and the
// Black Fang's hidden weapons, unorthodox and yin. Each has a strike at
// the third-rate realm, taught or read only, a wider one at first-rate and
// the widest at beyond peak, both of which comprehension unlocks too.
std::vector<Art> arts() {
  constexpr std::uint32_t FIRST = 1;
  constexpr std::uint32_t THIRD = 3;
  constexpr std::uint32_t FIFTH = 5;
  constexpr std::uint64_t STRIKE_DAMAGE = 20;
  constexpr std::uint64_t SWEEP_DAMAGE = 15;
  constexpr std::uint64_t FORM_DAMAGE = 30;
  constexpr std::uint64_t STRIKE_COOLDOWN = 90;
  constexpr std::uint64_t SWEEP_COOLDOWN = 300;
  constexpr std::uint64_t FORM_COOLDOWN = 600;
  constexpr std::uint32_t STRIKE_QI = 100;
  constexpr std::uint32_t SWEEP_QI = 400;
  constexpr std::uint32_t FORM_QI = 1'200;
  constexpr std::int32_t NEAR = 3 * CELL_SIZE;
  constexpr std::int32_t FAR = 8 * CELL_SIZE;
  constexpr std::int32_t WIDE = 2 * CELL_SIZE;
  constexpr std::int32_t WIDEST = 3 * CELL_SIZE;
  const auto art = [&](NameEntry name, std::size_t discipline, Nature nature,
                       std::int32_t range, std::array<NameEntry, 3> skills) {
    return Art{
        std::move(name),
        discipline,
        nature,
        {ArtSkill{std::move(skills.at(0)),
                  Skill{range, 0, STRIKE_DAMAGE, 0, STRIKE_COOLDOWN, STRIKE_QI},
                  FIRST, false},
         ArtSkill{std::move(skills.at(1)),
                  Skill{range, WIDE, SWEEP_DAMAGE, 0, SWEEP_COOLDOWN, SWEEP_QI},
                  THIRD, true},
         ArtSkill{std::move(skills.at(2)),
                  Skill{range, WIDEST, FORM_DAMAGE, 0, FORM_COOLDOWN, FORM_QI},
                  FIFTH, true}}};
  };
  return {
      art({"매화검법", "Plum Blossom Sword"}, 0,
          Nature{Orientation::ORTHODOX, Polarity::YANG}, NEAR,
          {NameEntry{"매화일검", "Plum Blossom Strike"},
           NameEntry{"이십사수매화검법", "Twenty-Four Plum Blossoms"},
           NameEntry{"매화낙영", "Falling Plum Blossoms"}}),
      art({"천마도법", "Heavenly Demon Saber"}, 1,
          Nature{Orientation::DEMONIC, Polarity::YANG}, NEAR,
          {NameEntry{"천마일도", "Heavenly Demon Cut"},
           NameEntry{"천마혈풍", "Heavenly Demon Blood Wind"},
           NameEntry{"천마군림", "Heavenly Demon Descends"}}),
      art({"흑아비도술", "Black Fang Darts"}, 3,
          Nature{Orientation::UNORTHODOX, Polarity::YIN}, FAR,
          {NameEntry{"흑아표", "Black Fang Dart"},
           NameEntry{"만천화우", "Rain of Flowers"},
           NameEntry{"흑아폭우", "Black Fang Storm"}}),
  };
}

// Surnames of the murim's clans, common and of the old houses, and given
// names for women and men, in Korean with their romanised forms.
Names names() {
  return Names{
      {{"김", "Kim"},       {"이", "Lee"},     {"박", "Park"},
       {"최", "Choi"},      {"정", "Jeong"},   {"강", "Kang"},
       {"조", "Jo"},        {"윤", "Yun"},     {"장", "Jang"},
       {"임", "Im"},        {"한", "Han"},     {"오", "O"},
       {"서", "Seo"},       {"신", "Sin"},     {"권", "Gwon"},
       {"황", "Hwang"},     {"송", "Song"},    {"백", "Baek"},
       {"남궁", "Namgung"}, {"제갈", "Jegal"}, {"독고", "Dokgo"},
       {"모용", "Moyong"},  {"사마", "Sama"},  {"황보", "Hwangbo"}},
      {{"설화", "Seolhwa"},
       {"연화", "Yeonhwa"},
       {"소연", "Soyeon"},
       {"하린", "Harin"},
       {"은비", "Eunbi"},
       {"매화", "Maehwa"},
       {"수련", "Suryeon"},
       {"서희", "Seohui"},
       {"지아", "Jia"},
       {"채원", "Chaewon"},
       {"월향", "Wolhyang"},
       {"예린", "Yerin"}},
      {{"무진", "Mujin"},
       {"태산", "Taesan"},
       {"청운", "Cheongun"},
       {"현", "Hyeon"},
       {"도윤", "Doyun"},
       {"진혁", "Jinhyeok"},
       {"소운", "Soun"},
       {"천명", "Cheonmyeong"},
       {"강호", "Gangho"},
       {"백랑", "Baengnang"},
       {"일검", "Ilgeom"},
       {"우진", "Ujin"}},
  };
}

// A point a partition is made around, and the value its cells take.
struct Site {
  Cell cell;
  std::size_t value;
};

// The middle cell of an area, as offsets from its corner.
Cell middle_of(Footprint area) { return Cell{area.width / 2, area.height / 2}; }

std::size_t index_of(Cell cell) {
  return (static_cast<std::size_t>(cell.y) *
          static_cast<std::size_t>(REGION_SIDE)) +
         static_cast<std::size_t>(cell.x);
}

Cell random_cell(Random& random) {
  return Cell{random.below(REGION_SIDE), random.below(REGION_SIDE)};
}

std::int64_t distance_squared(Cell from, Cell other) {
  const std::int64_t offset_x = std::int64_t{other.x} - from.x;
  const std::int64_t offset_y = std::int64_t{other.y} - from.y;
  return (offset_x * offset_x) + (offset_y * offset_y);
}

// Each cell takes the value of its nearest site, the lowest index among
// equals.
std::vector<std::size_t> partition(const std::vector<Site>& sites) {
  std::vector<std::size_t> values(CELLS);
  for (const Cell cell : cells_of(Cell{0, 0}, REGION)) {
    const auto nearer = [cell](const Site& left, const Site& right) {
      return distance_squared(cell, left.cell) <
             distance_squared(cell, right.cell);
    };
    values.at(index_of(cell)) =
        std::min_element(sites.begin(), sites.end(), nearer)->value;
  }
  return values;
}

std::size_t weighted_kind(Random& random,
                          const std::vector<TerrainKind>& terrain) {
  std::int32_t draw = random.below(
      std::accumulate(terrain.begin(), terrain.end(), std::int32_t{0},
                      [](std::int32_t sum, const TerrainKind& kind) {
                        return sum + kind.weight;
                      }));
  std::size_t kind = 0;
  while (draw >= terrain.at(kind).weight) {
    draw -= terrain.at(kind).weight;
    ++kind;
  }
  return kind;
}

std::vector<std::size_t> generate_terrain(
    Random& random, const std::vector<TerrainKind>& terrain) {
  std::vector<Site> sites;
  for (std::int32_t site = 0; site < TERRAIN_SITES; ++site) {
    const Cell cell = random_cell(random);
    sites.push_back(Site{cell, weighted_kind(random, terrain)});
  }
  return partition(sites);
}

// Each capital is the candidate farthest from those already placed, so the
// factions' territories come out of comparable size.
std::vector<std::size_t> generate_territory(Random& random,
                                            std::size_t factions) {
  std::vector<Site> capitals;
  for (std::size_t faction = 0; faction < factions; ++faction) {
    const auto clearance = [&capitals](Cell cell) {
      std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
      for (const Site& capital : capitals) {
        nearest = std::min(nearest, distance_squared(cell, capital.cell));
      }
      return nearest;
    };
    Cell best = random_cell(random);
    for (std::int32_t candidate = 1; candidate < CAPITAL_CANDIDATES;
         ++candidate) {
      const Cell cell = random_cell(random);
      best = clearance(cell) > clearance(best) ? cell : best;
    }
    capitals.push_back(Site{best, faction});
  }
  return partition(capitals);
}

std::vector<std::vector<bool>> hostility(const std::vector<Faction>& factions) {
  std::vector<std::vector<bool>> hostile(
      factions.size(), std::vector<bool>(factions.size(), false));
  for (std::size_t faction = 0; faction < factions.size(); ++faction) {
    for (const std::size_t enemy : factions.at(faction).hostile_to) {
      hostile.at(faction).at(enemy) = true;
    }
  }
  return hostile;
}

// The corners from which `area` lies on cells `room` gives to `owner`. A
// summed-area table of those cells counts the ones under any rectangle in
// four reads.
std::vector<Cell> corners_for(const std::vector<std::size_t>& room,
                              std::size_t owner, Footprint area) {
  constexpr auto STRIDE = static_cast<std::size_t>(REGION_SIDE) + 1;
  std::vector<std::int32_t> sums(STRIDE * STRIDE, 0);
  const auto slot = [](std::int32_t column, std::int32_t row) {
    return (static_cast<std::size_t>(row) * STRIDE) +
           static_cast<std::size_t>(column);
  };
  for (const Cell cell : cells_of(Cell{0, 0}, REGION)) {
    sums.at(slot(cell.x + 1, cell.y + 1)) =
        static_cast<std::int32_t>(room.at(index_of(cell)) == owner) +
        sums.at(slot(cell.x, cell.y + 1)) + sums.at(slot(cell.x + 1, cell.y)) -
        sums.at(slot(cell.x, cell.y));
  }
  const auto fits = [&sums, &slot, area](Cell corner) {
    const std::int32_t right = corner.x + area.width;
    const std::int32_t bottom = corner.y + area.height;
    return sums.at(slot(right, bottom)) - sums.at(slot(corner.x, bottom)) -
               sums.at(slot(right, corner.y)) +
               sums.at(slot(corner.x, corner.y)) ==
           area.width * area.height;
  };
  std::vector<Cell> corners = cells_of(
      Cell{0, 0},
      Footprint{REGION_SIDE - area.width + 1, REGION_SIDE - area.height + 1});
  corners.erase(std::remove_if(corners.begin(), corners.end(),
                               [&fits](Cell corner) { return !fits(corner); }),
                corners.end());
  return corners;
}

// The kind named `name` among the data's kinds, if any.
std::optional<StructureKind> kind_named(const RegionData& data,
                                        const std::string& name) {
  const auto found = std::find_if(
      data.kinds.begin(), data.kinds.end(),
      [&name](const StructureKind& kind) { return kind.name == name; });
  return found == data.kinds.end() ? std::nullopt
                                   : std::optional<StructureKind>{*found};
}

// What a structure of `kind` over `footprint` holds: the yield of a field is
// per cell of its footprint.
Holding holding_of(const StructureKind& kind, Footprint footprint) {
  const auto cells =
      static_cast<std::uint32_t>(footprint.width * footprint.height);
  return Holding{kind.housing, kind.storage, kind.yield * cells, kind.pupils,
                 kind.arms,    kind.tends,   kind.edge,          kind.healing};
}

// The plan of `building`, from what its kind holds.
Plan plan_of(const RegionData& data, const Building& building) {
  const StructureKind kind =
      kind_named(data, building.kind).value_or(StructureKind{});
  const Footprint footprint = building.structure.footprint;
  return Plan{building.kind, footprint, kind.blocks,
              holding_of(kind, footprint)};
}

// Growth that builds `builds` and lays out `settlement`'s buildings for a
// new village, each a plan from what its kind holds: the builds' plans
// first, then the settlement's, in order.
Growth growth_of(const RegionData& data, const std::vector<Building>& builds,
                 const VillageTemplate& settlement) {
  Growth growth{GROWTH_INTERVAL_DAYS,
                GROWTH_PRESSURE,
                GROWTH_REACH,
                BUILD_VICINITY,
                SETTLEMENT_SPACING,
                TERRITORY_CLAIM,
                {},
                {},
                settlement.area,
                settlement.name,
                {},
                SETTLERS,
                RAIDERS,
                {},
                SELF_RELIANCE};
  for (const Building& building : builds) {
    growth.builds.push_back(growth.plans.size());
    growth.plans.push_back(plan_of(data, building));
  }
  for (const Building& building : settlement.buildings) {
    growth.layout.push_back(
        Placement{growth.plans.size(), building.structure.cell});
    growth.plans.push_back(plan_of(data, building));
  }
  return growth;
}

// The data's rules of life, its growth taking each faction's stance.
Life life_of(const RegionData& data) {
  Life life = data.life;
  if (life.growth) {
    for (const Faction& faction : data.factions) {
      life.growth->stances.push_back(faction.stance);
    }
  }
  return life;
}

// Lays village `name` of template `kind` owned by `owner` with its area's
// corner at `corner`: its structures, with what each holds, and its resident
// army, two people for each household, standing on the open cells nearest
// its centre, bound for the centre.
void lay_village(Region& region, const RegionData& data,
                 const VillageTemplate& kind, std::size_t owner, Cell corner,
                 std::string name) {
  Village village{std::move(name),      kind.name, owner, corner, kind.area, {},
                  region.armies.size(), 0,         {}};
  for (const Building& building : kind.buildings) {
    const Structure& structure = building.structure;
    village.structures.push_back(region.structures.size());
    const StructureKind made =
        kind_named(data, building.kind).value_or(StructureKind{});
    region.structures.push_back(Structure{
        Cell{corner.x + structure.cell.x, corner.y + structure.cell.y},
        structure.durability, structure.footprint, made.blocks});
    region.structure_kinds.push_back(building.kind);
    region.holdings.push_back(holding_of(made, structure.footprint));
    village.households += building.households;
  }
  for (const Clearing& clearing : kind.clearings) {
    village.clearings.push_back(Clearing{
        Cell{corner.x + clearing.corner.x, corner.y + clearing.corner.y},
        clearing.area});
  }
  const Cell middle = middle_of(kind.area);
  const Cell centre{corner.x + middle.x, corner.y + middle.y};
  // The residents stand on the open cells nearest the centre.
  std::vector<Cell> open = open_cells(region, village);
  std::stable_sort(open.begin(), open.end(), [centre](Cell left, Cell right) {
    return distance_squared(left, centre) < distance_squared(right, centre);
  });
  std::vector<Recruit> members;
  for (std::size_t person = 0;
       person <
       std::min<std::size_t>(std::size_t{2} * village.households, open.size());
       ++person) {
    members.push_back(Recruit{open.at(person), HEALTH});
  }
  region.armies.push_back(ArmySpec{centre, std::move(members), owner,
                                   data.factions.at(owner).skills});
  region.villages.push_back(village);
}

// Lays each sect's seat from the seat template at a corner drawn among those
// where it lies on cells `room` gives its faction, which then hold `nobody`.
// Returns each seat's village, in the sects' order. Throws
// std::runtime_error when a seat finds no room.
std::vector<std::size_t> seat_sects(Region& region, const RegionData& data,
                                    std::vector<std::size_t>& room,
                                    Random& random, std::size_t nobody) {
  std::vector<std::size_t> seats;
  for (const SectData& sect : data.sects) {
    const std::vector<Cell> corners =
        corners_for(room, sect.faction, data.seat.area);
    if (corners.empty()) {
      throw std::runtime_error("no room for a seat");
    }
    const Cell corner = corners.at(static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(corners.size()))));
    for (const Cell cell : cells_of(corner, data.seat.area)) {
      room.at(index_of(cell)) = nobody;
    }
    seats.push_back(region.villages.size());
    lay_village(region, data, data.seat, sect.faction, corner,
                data.seat.name + " of " + sect.name);
  }
  return seats;
}

// The sects of `region` as a World is created with them, each seated in its
// village of `seats`, its founders that village's residents, the first of
// them at FOUNDER_ATTAINMENT, so it is its sect's master.
std::vector<SectSpec> sects_of(const Region& region, const RegionData& data,
                               const std::vector<std::size_t>& seats) {
  std::vector<std::size_t> first_of(region.armies.size());
  std::size_t counted = 0;
  for (std::size_t army = 0; army < region.armies.size(); ++army) {
    first_of.at(army) = counted;
    counted += region.armies.at(army).members.size();
  }
  std::vector<SectSpec> sects;
  for (std::size_t sect = 0; sect < data.sects.size(); ++sect) {
    const SectData& made = data.sects.at(sect);
    const std::size_t army = region.villages.at(seats.at(sect)).army;
    SectSpec spec{made.name, made.faction, seats.at(sect), {}};
    for (std::size_t member = 0; member < region.armies.at(army).members.size();
         ++member) {
      spec.founders.push_back(Founder{first_of.at(army) + member,
                                      member == 0 ? FOUNDER_ATTAINMENT : 0});
    }
    sects.push_back(std::move(spec));
  }
  return sects;
}

void validate(const RegionData& data) {
  const auto weightless = [](const TerrainKind& kind) {
    return kind.weight <= 0;
  };
  const auto unknown_enemy = [&data](const Faction& faction) {
    return std::any_of(
        faction.hostile_to.begin(), faction.hostile_to.end(),
        [&data](std::size_t enemy) { return enemy >= data.factions.size(); });
  };
  // Lays the buildings out on the area, failing on a template that houses no
  // one, or a building that covers no cell, or a cell outside the area or
  // already covered; the centre is covered last, so a building over it fails
  // too.
  const auto malformed = [](const VillageTemplate& village) {
    std::vector<bool> covered(static_cast<std::size_t>(village.area.width) *
                                  static_cast<std::size_t>(village.area.height),
                              false);
    const auto cover = [&village, &covered](Cell cell) {
      const bool inside = cell.x >= 0 && cell.y >= 0 &&
                          cell.x < village.area.width &&
                          cell.y < village.area.height;
      const std::size_t slot =
          inside ? (static_cast<std::size_t>(cell.y) *
                    static_cast<std::size_t>(village.area.width)) +
                       static_cast<std::size_t>(cell.x)
                 : 0;
      const bool free = inside && !covered.at(slot);
      if (free) {
        covered.at(slot) = true;
      }
      return free;
    };
    const auto placed = [&cover](const Building& building) {
      const std::vector<Cell> cells =
          cells_of(building.structure.cell, building.structure.footprint);
      return !cells.empty() && std::all_of(cells.begin(), cells.end(), cover);
    };
    const auto housed = [](const Building& building) {
      return building.households > 0;
    };
    return village.buildings.empty() ||
           std::none_of(village.buildings.begin(), village.buildings.end(),
                        housed) ||
           !std::all_of(village.buildings.begin(), village.buildings.end(),
                        placed) ||
           !cover(middle_of(village.area));
  };
  const auto unknown_kind = [&data](const VillageTemplate& village) {
    return std::any_of(village.buildings.begin(), village.buildings.end(),
                       [&data](const Building& building) {
                         return !kind_named(data, building.kind);
                       });
  };
  if (data.terrain.empty() || data.factions.empty() || data.villages.empty() ||
      std::any_of(data.terrain.begin(), data.terrain.end(), weightless) ||
      std::any_of(data.factions.begin(), data.factions.end(), unknown_enemy) ||
      std::any_of(
          data.factions.begin(), data.factions.end(),
          [](const Faction& faction) { return faction.skills.empty(); }) ||
      std::any_of(data.villages.begin(), data.villages.end(), malformed) ||
      (!data.sects.empty() && malformed(data.seat)) ||
      std::any_of(data.sects.begin(), data.sects.end(),
                  [&data](const SectData& sect) {
                    return sect.faction >= data.factions.size();
                  }) ||
      std::any_of(data.villages.begin(), data.villages.end(), unknown_kind) ||
      (!data.sects.empty() && unknown_kind(data.seat))) {
    throw std::invalid_argument("region data is incomplete or inconsistent");
  }
}

}  // namespace

RegionData region_data() {
  // A guard's strike; a mend that restores the structures within ten cells
  // of where it is used, a village's from its centre; and a torch that burns
  // them, where they belong to a faction its user is hostile to.
  constexpr std::uint64_t GUARD_COOLDOWN = 15;
  constexpr std::uint64_t GUARD_DAMAGE = 10;
  constexpr Skill GUARD{3 * CELL_SIZE, 0, GUARD_DAMAGE, 0, GUARD_COOLDOWN};
  constexpr std::int32_t MEND_REACH = 10;
  constexpr std::int32_t MEND_REPAIR = -5;
  constexpr std::uint64_t MEND_COOLDOWN = 30;
  constexpr Skill MEND{0, MEND_REACH * CELL_SIZE, 0, MEND_REPAIR,
                       MEND_COOLDOWN};
  constexpr std::int32_t TORCH_DAMAGE = 10;
  constexpr Skill TORCH{0, MEND_REACH * CELL_SIZE, 0, TORCH_DAMAGE,
                        MEND_COOLDOWN};
  constexpr Footprint HOUSE{3, 3};
  constexpr Footprint HALL{5, 3};
  constexpr Footprint WELL{2, 2};
  constexpr Footprint BARRELS{1, 1};
  constexpr Footprint FENCE{5, 1};
  constexpr Footprint MILLET{2, 2};
  constexpr Footprint RICE{3, 2};
  constexpr Footprint GRANARY{2, 2};
  constexpr Footprint STOREHOUSE{3, 2};
  constexpr Footprint SMITHY{3, 2};
  constexpr Footprint APOTHECARY{4, 2};
  constexpr std::int32_t MILLET_EAST = 5;
  constexpr std::int32_t RICE_EAST = 7;
  constexpr std::int32_t GRANARY_EAST = 12;
  constexpr std::int32_t GRANARY_ROW = 4;
  constexpr std::uint32_t FOUNDING_HOUSEHOLDS = 1;
  constexpr std::int32_t HAMLET_WIDTH = 7;
  constexpr std::int32_t VILLAGE_WIDTH = 17;
  constexpr std::int32_t VILLAGE_HEIGHT = 9;
  constexpr std::int32_t ROAD_ROW = 3;
  constexpr std::int32_t SOUTH_ROW = 6;
  constexpr std::int32_t FENCE_ROW = 8;
  constexpr std::int32_t HALL_EAST = 10;
  constexpr std::int32_t FAR_EAST = 14;
  constexpr std::int32_t SOUTH_EAST = 11;
  constexpr std::int32_t WELL_EAST = 6;
  constexpr std::int32_t FENCE_EAST = 5;
  constexpr std::int32_t PLAIN_WEIGHT = 10;
  constexpr Footprint MANUAL_HALL{4, 3};
  constexpr Footprint TRAINING_GROUND{6, 4};
  constexpr Footprint SEAT_GATE{3, 1};
  constexpr std::int32_t SEAT_WIDTH = 12;
  constexpr std::int32_t SEAT_HEIGHT = 9;
  constexpr std::int32_t SEAT_HALL_EAST = 7;
  constexpr std::int32_t SEAT_YARD_ROW = 5;
  constexpr std::int32_t SEAT_GRANARY_EAST = 6;
  constexpr std::int32_t SEAT_FIELD_EAST = 9;
  constexpr std::int32_t SEAT_GATE_EAST = 7;
  constexpr std::int32_t SEAT_COURT_ROW = 3;
  constexpr std::int32_t SEAT_PATH_EAST = SEAT_GATE_EAST + 1;
  const auto building = [](Cell cell, Footprint footprint, const char* kind,
                           std::uint32_t households) {
    return Building{Structure{cell, DURABILITY, footprint}, kind, households};
  };
  RegionData data{
      {
          // Villages build on the plain and in forest; bodies cross hills,
          // where landforms stand, but nothing is built there.
          {"plain", Footing::BUILDABLE, PLAIN_WEIGHT},
          {"forest", Footing::BUILDABLE, 3},
          {"hills", Footing::ROUGH, 2},
          {"water", Footing::BLOCKED, 1},
          {"mountain", Footing::BLOCKED, 1},
      },
      {
          // The Orthodox and Unorthodox mend villages; the Demonic Cult burns
          // them and rebuilds nothing.
          // The Demonic Cult changes leader by duel, to the death, and the
          // old leader's allies turn on the winner.
          // As their villages press against their caps, the Orthodox build
          // first, then found and raid; the Unorthodox found first; the
          // Demonic Cult, which raises nothing, raids.
          // Sects admit their faction's people of 12 within 48 cells of the
          // seat at 3 in 10 a year: the Orthodox and the Cult assess a
          // talent limit of 550, the Unorthodox proof of usefulness, an
          // attainment of 20. The third of a sect's members of the highest
          // attainment are its elders, at least one. The Orthodox elect their
          // leader among their sects' masters.
          {"Orthodox",
           {1},
           {GUARD, MEND},
           std::nullopt,
           {Expansion::BUILD, Expansion::FOUND, Expansion::RAID},
           Admission{ADMISSION_AGE, ASSESSED_TALENT, 0, ADMISSION_REACH,
                     ADMISSION_CHANCE, ELDER_SHARE},
           Election{0}},
          {"Demonic",
           {0, 2},
           {GUARD, TORCH},
           Duel{true, 0, true},
           {Expansion::RAID},
           Admission{ADMISSION_AGE, ASSESSED_TALENT, 0, ADMISSION_REACH,
                     ADMISSION_CHANCE, ELDER_SHARE},
           std::nullopt},
          {"Unorthodox",
           {1},
           {GUARD, MEND},
           std::nullopt,
           {Expansion::FOUND, Expansion::RAID, Expansion::BUILD},
           Admission{ADMISSION_AGE, 0, PROVEN_ATTAINMENT, ADMISSION_REACH,
                     ADMISSION_CHANCE, ELDER_SHARE},
           std::nullopt},
      },
      // The approved target mockup's village: houses of 3 by 3 cells and a
      // hall of 5 by 3 north of the road through its centre, more houses
      // south of it, a well in the square before the hall, barrels at the
      // road's ends and a fence along the field between the southern houses.
      // The hamlet keeps its west end. Wells, barrels and fences block and
      // break like any structure, and house no one. Fields of millet and rice
      // lie where the mockup sows them, a granary stands east of the square
      // and a storehouse in the south-east corner; the hamlet keeps its
      // millet and a granary in its south-west corner. Each starts with one
      // household, a married man and woman: the village in its hall, the
      // hamlet in its first house.
      {
          {"hamlet",
           {HAMLET_WIDTH, VILLAGE_HEIGHT},
           {building({0, 0}, HOUSE, "house", FOUNDING_HOUSEHOLDS),
            building({4, 0}, HOUSE, "house", 0),
            building({2, SOUTH_ROW}, HOUSE, "house", 0),
            building({HAMLET_WIDTH - 1, ROAD_ROW}, BARRELS, "barrels", 0),
            building({MILLET_EAST, SOUTH_ROW}, MILLET, "field", 0),
            building({0, SOUTH_ROW}, GRANARY, "granary", 0)},
           {}},
          {"village",
           {VILLAGE_WIDTH, VILLAGE_HEIGHT},
           {building({0, 0}, HOUSE, "house", 0),
            building({4, 0}, HALL, "hall", FOUNDING_HOUSEHOLDS),
            building({HALL_EAST, 0}, HOUSE, "house", 0),
            building({FAR_EAST, 0}, HOUSE, "house", 0),
            building({1, SOUTH_ROW}, HOUSE, "house", 0),
            building({SOUTH_EAST, SOUTH_ROW}, HOUSE, "house", 0),
            building({WELL_EAST, ROAD_ROW}, WELL, "well", 0),
            building({0, ROAD_ROW}, BARRELS, "barrels", 0),
            building({VILLAGE_WIDTH - 1, ROAD_ROW}, BARRELS, "barrels", 0),
            building({FENCE_EAST, FENCE_ROW}, FENCE, "fence", 0),
            building({MILLET_EAST, SOUTH_ROW}, MILLET, "field", 0),
            building({RICE_EAST, SOUTH_ROW}, RICE, "field", 0),
            building({GRANARY_EAST, GRANARY_ROW}, GRANARY, "granary", 0),
            building({FAR_EAST, SOUTH_ROW}, STOREHOUSE, "storehouse", 0)},
           {}},
      },
      // Five disciplines, each drawn the same way: growth of 5 to 15 a use,
      // a limit of 300 to 700. People live 45 to 75 years of 120 days, a
      // day longer for each point of attainment; a soul's past lives widen a
      // practised discipline's limit by 200, and every limit's top by 100,
      // each over the lives lived.
      Life{{{"sword", {GROWTH, GROWTH_SPREAD}, {LIMIT, LIMIT_SPREAD}},
            {"saber", {GROWTH, GROWTH_SPREAD}, {LIMIT, LIMIT_SPREAD}},
            {"fist", {GROWTH, GROWTH_SPREAD}, {LIMIT, LIMIT_SPREAD}},
            {"hidden weapons", {GROWTH, GROWTH_SPREAD}, {LIMIT, LIMIT_SPREAD}},
            {"medicine", {GROWTH, GROWTH_SPREAD}, {LIMIT, LIMIT_SPREAD}}},
           HEALTH,
           Spread{LIFESPAN_DAYS, LIFESPAN_SPREAD_DAYS},
           1,
           DISCIPLINE_WIDENING,
           UPPER_WIDENING,
           // The people the region starts with are 16 to 28 years old. A
           // person of 16 to 40 marries, at even odds a year, someone of
           // another family from a village whose centre lies within 80
           // cells; a wife of 18 to 40 bears a child at 2 in 5 a year, two
           // years apart at least, into her husband's family, of the sex
           // fewer among her village's unmarried.
           Spread{FOUNDING_AGE, FOUNDING_SPREAD},
           Kinship{{MARRYING_FROM, MARRYING_TO},
                   NEIGHBOURHOOD,
                   MARRIAGE_CHANCE,
                   {BEARING_FROM, BEARING_TO},
                   BIRTH_CHANCE,
                   BIRTH_SPACING,
                   true,
                   true},
           names(),
           // Each person eats a measure of food a day. Unfed for ten days,
           // a person moves to a village within 48 cells with room and food,
           // or dies.
           Husbandry{1, HUNGER_DAYS, MIGRATION_REACH},
           std::nullopt,
           // Each sect holds the manual of its faction's art (arts()):
           // Hwasan the sword, the Heavenly Demon the saber, the Black Fang
           // hidden weapons. A fallen sect's manuals scatter at even odds
           // within 16 cells of its seat, and are otherwise lost.
           Manuals{{{0}, {1}, {2}}, SCATTER_CHANCE, SCATTER_REACH},
           // People cultivate 0 to 2 qi a day, up to a limit of 1,000 to
           // 15,000, and recover a hundredth of their store a second.
           Cultivation{{QI_GROWTH, QI_GROWTH_SPREAD},
                       {QI_LIMIT, QI_LIMIT_SPREAD},
                       QI_RECOVERY,
                       QI_RECOVERY_TICKS},
           realms(),
           arts(),
           // A realm gained comprehends a skill at 3 in 10 at a
           // discipline's mean talent; a sect teaches at 1 in 5 a year for
           // each rank of its best elder; a manual costs 500 qi to read from
           // within a cell. An art of another orientation is learned at half
           // the chance and strikes at seven tenths; of another polarity, at
           // seven tenths and with nine tenths.
           Learning{COMPREHENSION, TEACHING, READING_COST, CELL_SIZE, HALF,
                    SEVEN_TENTHS, SEVEN_TENTHS, NINE_TENTHS},
           // A leader declares war on a faction at peace with its own,
           // outnumbering it by half again, at 1 in 10 a year, and one
           // outnumbered in a declared war makes peace at 1 in 5 a year; a
           // declaration holds 5 years.
           Diplomacy{WAR_MARGIN, WAR_CHANCE, PEACE_CHANCE, HOLD_DAYS}},
      // What each kind of structure holds: a house shelters 8 and a hall 12,
      // a granary stores 2,400 measures and a storehouse 3,600, and each
      // cell of a field yields 600 a harvest. A field never blocks. A village
      // is capped at 50 people, a hamlet at 20. A seat's hall and manual
      // hall each teach 4 pupils and its training ground 12, on which people
      // stand, so it blocks nothing. A smithy arms 40 people, who strike a
      // quarter harder, and an apothecary tends 40, who recover 12 health a
      // day, a tenth of theirs.
      {{"house", HOUSE_HOUSING, 0, 0, true, 0},
       {"hall", HALL_HOUSING, 0, 0, true, HALL_PUPILS},
       {"well", 0, 0, 0, true, 0},
       {"barrels", 0, 0, 0, true, 0},
       {"fence", 0, 0, 0, true, 0},
       {"granary", 0, GRANARY_STORAGE, 0, true, 0},
       {"storehouse", 0, STOREHOUSE_STORAGE, 0, true, 0},
       {"field", 0, 0, FIELD_YIELD, false, 0},
       {"training_ground", 0, 0, 0, false, TRAINING_GROUND_PUPILS},
       {"manual_hall", 0, 0, 0, true, MANUAL_HALL_PUPILS},
       {"seat_gate", 0, 0, 0, true, 0},
       {"smithy", 0, 0, 0, true, 0, WORKSHOP_SERVES, 0, SMITHY_EDGE, 0},
       {"apothecary", 0, 0, 0, true, 0, 0, WORKSHOP_SERVES, 0,
        APOTHECARY_HEALING}},
      // A sect's seat of 12 by 9 cells, which every one of 200 seeds has room
      // for: a house for its founding household, a manual hall and a
      // hall along its north side, a training ground in its south-west
      // corner, a granary and a field of rice east of it, and the seat gate
      // on its south edge below them. Growth keeps clear its court, the two
      // rows before the halls, the path down from it between the granary and
      // the field to the gate, and two rows before the gate outside.
      {"seat",
       {SEAT_WIDTH, SEAT_HEIGHT},
       {building({0, 0}, HOUSE, "house", FOUNDING_HOUSEHOLDS),
        building({3, 0}, MANUAL_HALL, "manual_hall", 0),
        building({SEAT_HALL_EAST, 0}, HALL, "hall", 0),
        building({0, SEAT_YARD_ROW}, TRAINING_GROUND, "training_ground", 0),
        building({SEAT_GRANARY_EAST, SEAT_YARD_ROW}, GRANARY, "granary", 0),
        building({SEAT_FIELD_EAST, SEAT_YARD_ROW}, RICE, "field", 0),
        building({SEAT_GATE_EAST, SEAT_HEIGHT - 1}, SEAT_GATE, "seat_gate", 0)},
       {Clearing{{0, SEAT_COURT_ROW}, {SEAT_WIDTH, 2}},
        Clearing{{SEAT_PATH_EAST, SEAT_YARD_ROW}, {1, 3}},
        Clearing{{SEAT_GATE_EAST, SEAT_HEIGHT}, {SEAT_GATE.width, 2}}}},
      {{"Hwasan", 0}, {"Heavenly Demon", 1}, {"Black Fang", 2}},
  };
  // Villages pressing against a fifth of their caps grow, checked every 30
  // days. They build a house, a field of rice, a granary, a smithy or an
  // apothecary within 8 cells of their centre, or send 6 settlers to lay out
  // a hamlet within 40 cells, 3 cells clear of any structure, or 10 raiders
  // against a hostile village within 40 cells. A village holds the land
  // within 16 cells. It arms and tends 20 people with no workshop, so a
  // hamlet, capped at 20, builds neither, and a village, at 50, builds both.
  data.life.growth = growth_of(
      data,
      {building({0, 0}, HOUSE, "house", 0), building({0, 0}, RICE, "field", 0),
       building({0, 0}, GRANARY, "granary", 0),
       building({0, 0}, SMITHY, "smithy", 0),
       building({0, 0}, APOTHECARY, "apothecary", 0)},
      data.villages.front());
  return data;
}

Region generate_region(Seed seed, const RegionData& data) {
  validate(data);
  Random random{static_cast<std::uint64_t>(seed)};
  Region region{seed,
                std::vector<std::string>(
                    static_cast<std::size_t>(REGION_SIDE),
                    std::string(static_cast<std::size_t>(REGION_SIDE), '.')),
                {},
                {},
                {},
                Rules{SEPARATION,
                      hostility(data.factions),
                      {DURABILITY, DURABILITY / 2, 1},
                      TICKS_PER_DAY,
                      RUMOUR_REACH},
                generate_terrain(random, data.terrain),
                generate_territory(random, data.factions.size()),
                {},
                {},
                life_of(data),
                {},
                {}};
  for (const Cell cell : cells_of(Cell{0, 0}, REGION)) {
    region.map_rows.at(static_cast<std::size_t>(cell.y))
        .at(static_cast<std::size_t>(cell.x)) =
        tile_of(data.terrain.at(region.terrain.at(index_of(cell))).footing);
  }
  // The owner of each free buildable cell; any other cell, or a built-on
  // one, is owned by no faction, so no village fits on it.
  const Map map{region.map_rows};
  const std::size_t nobody = data.factions.size();
  std::vector<std::size_t> room(CELLS);
  for (const Cell cell : cells_of(Cell{0, 0}, REGION)) {
    room.at(index_of(cell)) =
        map.buildable(cell) ? region.territory.at(index_of(cell)) : nobody;
  }

  const std::int32_t villages = MIN_VILLAGES + random.below(VILLAGE_COUNTS);
  for (std::int32_t number = 0; number < villages; ++number) {
    const std::size_t owner =
        static_cast<std::size_t>(number) % data.factions.size();
    // The templates with room in the owner's territory, each with the
    // corners where it fits.
    std::vector<std::size_t> roomy;
    std::vector<std::vector<Cell>> fitting;
    for (std::size_t kind = 0; kind < data.villages.size(); ++kind) {
      std::vector<Cell> corners =
          corners_for(room, owner, data.villages.at(kind).area);
      if (!corners.empty()) {
        roomy.push_back(kind);
        fitting.push_back(std::move(corners));
      }
    }
    if (roomy.empty()) {
      throw std::runtime_error("no room for a village");
    }
    const auto drawn = static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(roomy.size())));
    const VillageTemplate& kind = data.villages.at(roomy.at(drawn));
    const std::vector<Cell>& corners = fitting.at(drawn);
    const Cell corner = corners.at(static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(corners.size()))));
    for (const Cell cell : cells_of(corner, kind.area)) {
      room.at(index_of(cell)) = nobody;
    }

    lay_village(region, data, kind, owner, corner,
                kind.name + " " + std::to_string(number + 1));
  }
  const std::vector<std::size_t> seats =
      seat_sects(region, data, room, random, nobody);
  // A faction is led by the first resident of its first village; people are
  // numbered in army order, so that resident's index counts every member of
  // the armies before it.
  region.leadership.resize(data.factions.size());
  for (std::size_t faction = 0; faction < data.factions.size(); ++faction) {
    region.leadership.at(faction).succession =
        data.factions.at(faction).succession;
    region.leadership.at(faction).admission =
        data.factions.at(faction).admission;
    region.leadership.at(faction).election = data.factions.at(faction).election;
  }
  std::size_t counted = 0;
  for (std::size_t army = 0; army < region.armies.size(); ++army) {
    const auto housed = [army](const Village& village) {
      return village.army == army;
    };
    const auto home =
        std::find_if(region.villages.begin(), region.villages.end(), housed);
    std::optional<std::size_t>& leader =
        region.leadership.at(region.armies.at(army).faction).leader;
    if (home != region.villages.end() && !leader &&
        !region.armies.at(army).members.empty()) {
      leader = counted;
    }
    counted += region.armies.at(army).members.size();
  }
  region.sects = sects_of(region, data, seats);
  return region;
}

std::vector<Cell> open_cells(const Region& region, const Village& village) {
  std::vector<Cell> open;
  for (const Cell cell : cells_of(village.corner, village.area)) {
    const auto covers = [&region, cell](std::size_t index) {
      const Structure& structure = region.structures.at(index);
      return cell.x >= structure.cell.x && cell.y >= structure.cell.y &&
             cell.x < structure.cell.x + structure.footprint.width &&
             cell.y < structure.cell.y + structure.footprint.height;
    };
    if (std::none_of(village.structures.begin(), village.structures.end(),
                     covers)) {
      open.push_back(cell);
    }
  }
  return open;
}

ArmySpec born_in(const Region& region, std::size_t village) {
  const Village& home = region.villages.at(village);
  const ArmySpec& residents = region.armies.at(home.army);
  const auto taken = [&residents](Cell cell) {
    return std::any_of(residents.members.begin(), residents.members.end(),
                       [cell](const Recruit& resident) {
                         return resident.cell.x == cell.x &&
                                resident.cell.y == cell.y;
                       });
  };
  const std::vector<Cell> open = open_cells(region, home);
  const auto free = std::find_if_not(open.begin(), open.end(), taken);
  if (free == open.end()) {
    throw std::out_of_range("the village has no free cell to be born on");
  }
  return ArmySpec{*free,
                  {Recruit{*free, HEALTH}},
                  residents.faction,
                  residents.skills,
                  village};
}

std::vector<VillageSpec> village_specs(const Region& region) {
  std::vector<VillageSpec> specs;
  specs.reserve(region.villages.size());
  for (const Village& village : region.villages) {
    std::vector<Holding> holdings;
    holdings.reserve(village.structures.size());
    for (const std::size_t structure : village.structures) {
      holdings.push_back(region.holdings.at(structure));
    }
    specs.push_back(VillageSpec{village.structures, village.army,
                                std::move(holdings), village.households,
                                village.clearings});
  }
  return specs;
}

}  // namespace murim
