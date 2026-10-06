#include "region.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
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

constexpr std::int32_t SEPARATION = 72;
constexpr std::uint32_t DURABILITY = 3'000;
constexpr std::uint64_t STRENGTH_PER_AGENT = 100'000;

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
      return building.residents > 0;
    };
    return village.buildings.empty() ||
           std::none_of(village.buildings.begin(), village.buildings.end(),
                        housed) ||
           !std::all_of(village.buildings.begin(), village.buildings.end(),
                        placed) ||
           !cover(middle_of(village.area));
  };
  if (data.terrain.empty() || data.factions.empty() || data.villages.empty() ||
      std::any_of(data.terrain.begin(), data.terrain.end(), weightless) ||
      std::any_of(data.factions.begin(), data.factions.end(), unknown_enemy) ||
      std::any_of(
          data.factions.begin(), data.factions.end(),
          [](const Faction& faction) { return faction.skills.empty(); }) ||
      std::any_of(data.villages.begin(), data.villages.end(), malformed)) {
    throw std::invalid_argument("region data is incomplete or inconsistent");
  }
}

}  // namespace

RegionData region_data() {
  // A guard's strike; a mend that restores the structures within ten cells
  // of where it is used, a village's from its centre; and a torch that burns
  // them, where they belong to a faction its user is hostile to.
  constexpr std::uint64_t GUARD_COOLDOWN = 15;
  constexpr Skill GUARD{3 * CELL_SIZE, 0, 1, 0, GUARD_COOLDOWN};
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
  constexpr std::uint32_t HOUSE_RESIDENTS = 4;
  constexpr std::uint32_t HALL_RESIDENTS = 6;
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
  const auto building = [](Cell cell, Footprint footprint, const char* kind,
                           std::uint32_t residents) {
    return Building{Structure{cell, DURABILITY, footprint}, kind, residents};
  };
  return RegionData{
      {
          {"plain", true, 4},
          {"forest", true, 3},
          {"hills", true, 2},
          {"water", false, 1},
          {"mountain", false, 1},
      },
      {
          // The Orthodox and Unorthodox mend villages; the Demonic Cult burns
          // them and rebuilds nothing.
          {"Orthodox", {1}, {GUARD, MEND}},
          {"Demonic", {0, 2}, {GUARD, TORCH}},
          {"Unorthodox", {1}, {GUARD, MEND}},
      },
      // The approved target mockup's village: houses of 3 by 3 cells and a
      // hall of 5 by 3 north of the road through its centre, more houses
      // south of it, a well in the square before the hall, barrels at the
      // road's ends and a fence along the field between the southern houses.
      // The hamlet keeps its west end. Wells, barrels and fences block and
      // break like any structure, and house no one.
      {
          {"hamlet",
           {HAMLET_WIDTH, VILLAGE_HEIGHT},
           {building({0, 0}, HOUSE, "house", HOUSE_RESIDENTS),
            building({4, 0}, HOUSE, "house", HOUSE_RESIDENTS),
            building({2, SOUTH_ROW}, HOUSE, "house", HOUSE_RESIDENTS),
            building({HAMLET_WIDTH - 1, ROAD_ROW}, BARRELS, "barrels", 0)}},
          {"village",
           {VILLAGE_WIDTH, VILLAGE_HEIGHT},
           {building({0, 0}, HOUSE, "house", HOUSE_RESIDENTS),
            building({4, 0}, HALL, "hall", HALL_RESIDENTS),
            building({HALL_EAST, 0}, HOUSE, "house", HOUSE_RESIDENTS),
            building({FAR_EAST, 0}, HOUSE, "house", HOUSE_RESIDENTS),
            building({1, SOUTH_ROW}, HOUSE, "house", HOUSE_RESIDENTS),
            building({SOUTH_EAST, SOUTH_ROW}, HOUSE, "house", HOUSE_RESIDENTS),
            building({WELL_EAST, ROAD_ROW}, WELL, "well", 0),
            building({0, ROAD_ROW}, BARRELS, "barrels", 0),
            building({VILLAGE_WIDTH - 1, ROAD_ROW}, BARRELS, "barrels", 0),
            building({FENCE_EAST, FENCE_ROW}, FENCE, "fence", 0)}},
      },
  };
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
                      {DURABILITY, DURABILITY / 2, 1}},
                generate_terrain(random, data.terrain),
                generate_territory(random, data.factions.size()),
                {}};
  // The owner of each free passable cell; a blocked or built-on cell is
  // owned by no faction, so no village fits on it.
  const std::size_t nobody = data.factions.size();
  std::vector<std::size_t> room(CELLS);
  for (const Cell cell : cells_of(Cell{0, 0}, REGION)) {
    const std::size_t place = index_of(cell);
    const bool passable = data.terrain.at(region.terrain.at(place)).passable;
    region.map_rows.at(static_cast<std::size_t>(cell.y))
        .at(static_cast<std::size_t>(cell.x)) = passable ? '.' : '#';
    room.at(place) = passable ? region.territory.at(place) : nobody;
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

    Village village{kind.name + " " + std::to_string(number + 1),
                    kind.name,
                    owner,
                    corner,
                    kind.area,
                    {},
                    region.armies.size()};
    std::uint32_t residents = 0;
    for (const Building& building : kind.buildings) {
      const Structure& structure = building.structure;
      village.structures.push_back(region.structures.size());
      region.structures.push_back(Structure{
          Cell{corner.x + structure.cell.x, corner.y + structure.cell.y},
          structure.durability, structure.footprint});
      region.structure_kinds.push_back(building.kind);
      residents += building.residents;
    }
    const Cell middle = middle_of(kind.area);
    const Cell centre{corner.x + middle.x, corner.y + middle.y};
    region.armies.push_back(ArmySpec{centre, centre, residents,
                                     residents * STRENGTH_PER_AGENT, owner,
                                     data.factions.at(owner).skills});
    region.villages.push_back(village);
  }
  return region;
}

ArmySpec born_in(const Region& region, std::size_t village) {
  const Village& home = region.villages.at(village);
  const ArmySpec& residents = region.armies.at(home.army);
  const auto taken = [&region, &home, &residents](Cell cell) {
    const auto covers = [&region, cell](std::size_t index) {
      const Structure& structure = region.structures.at(index);
      const std::vector<Cell> cells =
          cells_of(structure.cell, structure.footprint);
      return std::any_of(cells.begin(), cells.end(), [cell](Cell covered) {
        return covered.x == cell.x && covered.y == cell.y;
      });
    };
    return (cell.x == residents.start.x && cell.y == residents.start.y) ||
           std::any_of(home.structures.begin(), home.structures.end(), covers);
  };
  const std::vector<Cell> cells = cells_of(home.corner, home.area);
  const auto free = std::find_if_not(cells.begin(), cells.end(), taken);
  if (free == cells.end()) {
    throw std::out_of_range("the village has no free cell to be born on");
  }
  const Cell birthplace = *free;
  return ArmySpec{birthplace,
                  birthplace,
                  1,
                  residents.strength / residents.count,
                  residents.faction,
                  residents.skills};
}

std::vector<VillageSpec> village_specs(const Region& region) {
  std::vector<VillageSpec> specs;
  specs.reserve(region.villages.size());
  for (const Village& village : region.villages) {
    specs.push_back(VillageSpec{village.structures, village.army});
  }
  return specs;
}

}  // namespace murim
