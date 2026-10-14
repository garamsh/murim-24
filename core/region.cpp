#include "region.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "calendar.hpp"
#include "growth.hpp"
#include "life.hpp"
#include "map.hpp"
#include "random.hpp"
#include "sect.hpp"
#include "village.hpp"
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
// A structure's durability when intact, which the stages' floors divide, and
// the health a village's residents start with.
constexpr std::uint32_t DURABILITY = 10'000;
constexpr std::uint64_t HEALTH = 120;
// An event reaches everyone within 32 cells of it as a rumour, a quarter of
// the region's side.
constexpr std::int32_t RUMOUR_REACH = 32;
// One person may talk to another within 2 cells, a step past the cell apart
// people keep.
constexpr std::int32_t TALK_REACH = 2;
// A person closes on a foe within 6 cells while none is within its reach,
// as far as two bands meeting in the open start apart.
constexpr std::int32_t SIGHT = 6;
// The steered see the land for their map within 15 cells, half the 30 rows
// the PC's least view shows (ADR 0073).
constexpr std::int32_t MAP_SIGHT = 15;
// The attainment a sect's first founder starts with, so it is its master.
constexpr std::uint32_t FOUNDER_ATTAINMENT = 120;

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

}  // namespace

Growth growth_for(const RegionData& data, Growth rules,
                  const std::vector<Building>& builds,
                  const VillageTemplate& settlement) {
  Growth growth = std::move(rules);
  growth.plans.clear();
  growth.builds.clear();
  growth.layout.clear();
  growth.settlement = settlement.area;
  growth.settlement_kind = settlement.name;
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

namespace {

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

bool consistent(const RegionData& data) {
  try {
    validate(data);
  } catch (const std::invalid_argument&) {
    return false;
  }
  return true;
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
                      RUMOUR_REACH,
                      TALK_REACH,
                      SIGHT},
                generate_terrain(random, data.terrain),
                generate_territory(random, data.factions.size()),
                {},
                {},
                life_of(data),
                {},
                {}};
  region.rules.map_sight = MAP_SIGHT;
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
    throw std::runtime_error("the village has no free cell to be born on");
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
