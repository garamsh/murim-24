#include "region.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "map.hpp"
#include "random.hpp"
#include "world.hpp"

namespace murim {
namespace {

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
constexpr std::size_t RESIDENTS_PER_STRUCTURE = 4;
constexpr std::uint64_t STRENGTH_PER_AGENT = 100'000;
constexpr std::uint64_t GUARD_COOLDOWN = 15;
constexpr Skill GUARD{3 * CELL_SIZE, 0, 1, 0, GUARD_COOLDOWN};

// A point a partition is made around, and the value its cells take.
struct Site {
  Cell cell;
  std::size_t value;
};

std::size_t index_of(Cell cell) {
  return (static_cast<std::size_t>(cell.y) *
          static_cast<std::size_t>(REGION_SIDE)) +
         static_cast<std::size_t>(cell.x);
}

Cell random_cell(Random& random) {
  return Cell{random.below(REGION_SIDE), random.below(REGION_SIDE)};
}

// The cells of the square of `side` cells from `corner`, row by row.
std::vector<Cell> square(Cell corner, std::int32_t side) {
  std::vector<Cell> cells;
  for (std::int32_t row = corner.y; row < corner.y + side; ++row) {
    for (std::int32_t column = corner.x; column < corner.x + side; ++column) {
      cells.push_back(Cell{column, row});
    }
  }
  return cells;
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
  for (const Cell cell : square(Cell{0, 0}, REGION_SIDE)) {
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

void validate(const RegionData& data) {
  const auto weightless = [](const TerrainKind& kind) {
    return kind.weight <= 0;
  };
  const auto unknown_enemy = [&data](const Faction& faction) {
    return std::any_of(
        faction.hostile_to.begin(), faction.hostile_to.end(),
        [&data](std::size_t enemy) { return enemy >= data.factions.size(); });
  };
  const auto malformed = [](const VillageTemplate& village) {
    const auto outside = [&village](Cell offset) {
      return offset.x < 0 || offset.y < 0 || offset.x >= village.side ||
             offset.y >= village.side;
    };
    return village.structures.empty() ||
           std::any_of(village.structures.begin(), village.structures.end(),
                       outside);
  };
  if (data.terrain.empty() || data.factions.empty() || data.villages.empty() ||
      std::any_of(data.terrain.begin(), data.terrain.end(), weightless) ||
      std::any_of(data.factions.begin(), data.factions.end(), unknown_enemy) ||
      std::any_of(data.villages.begin(), data.villages.end(), malformed)) {
    throw std::invalid_argument("region data is incomplete or inconsistent");
  }
}

}  // namespace

RegionData region_data() {
  constexpr std::int32_t TOWN_SIDE = 5;
  return RegionData{
      {
          {"plain", true, 4},
          {"forest", true, 3},
          {"hills", true, 2},
          {"water", false, 1},
          {"mountain", false, 1},
      },
      {
          {"Orthodox", {1}},
          {"Demonic", {0, 2}},
          {"Unorthodox", {1}},
      },
      {
          {"hamlet", 3, {{0, 0}, {2, 0}, {0, 2}, {2, 2}}},
          {"village", 4, {{0, 0}, {3, 0}, {0, 3}, {3, 3}, {1, 0}, {0, 2}}},
          {"town",
           TOWN_SIDE,
           {{0, 0}, {2, 0}, {4, 0}, {0, 2}, {4, 2}, {0, 4}, {2, 4}, {4, 4}}},
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
  for (const Cell cell : square(Cell{0, 0}, REGION_SIDE)) {
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
    const VillageTemplate& kind = data.villages.at(static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(data.villages.size()))));
    const auto fits = [&room, &kind, owner](Cell corner) {
      const std::vector<Cell> cells = square(corner, kind.side);
      return std::all_of(cells.begin(), cells.end(), [&room, owner](Cell cell) {
        return room.at(index_of(cell)) == owner;
      });
    };
    std::vector<Cell> corners = square(Cell{0, 0}, REGION_SIDE - kind.side + 1);
    corners.erase(
        std::remove_if(corners.begin(), corners.end(),
                       [&fits](Cell corner) { return !fits(corner); }),
        corners.end());
    if (corners.empty()) {
      throw std::runtime_error("no room for a village");
    }
    const Cell corner = corners.at(static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(corners.size()))));
    for (const Cell cell : square(corner, kind.side)) {
      room.at(index_of(cell)) = nobody;
    }

    Village village{kind.name + " " + std::to_string(number + 1),
                    owner,
                    corner,
                    kind.side,
                    {},
                    region.armies.size()};
    for (const Cell offset : kind.structures) {
      village.structures.push_back(region.structures.size());
      region.structures.push_back(Structure{
          Cell{corner.x + offset.x, corner.y + offset.y}, DURABILITY});
    }
    const Cell centre{corner.x + (kind.side / 2), corner.y + (kind.side / 2)};
    const auto residents = static_cast<std::uint32_t>(kind.structures.size() *
                                                      RESIDENTS_PER_STRUCTURE);
    region.armies.push_back(ArmySpec{centre,
                                     centre,
                                     residents,
                                     residents * STRENGTH_PER_AGENT,
                                     owner,
                                     {GUARD}});
    region.villages.push_back(village);
  }
  return region;
}

}  // namespace murim
