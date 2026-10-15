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
#include <variant>
#include <vector>

#include "attack.hpp"
#include "calendar.hpp"
#include "compiled_map.hpp"
#include "growth.hpp"
#include "layout.hpp"
#include "life.hpp"
#include "limits.hpp"
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
// The open sites a starting village's site is chosen among.
constexpr std::int32_t SITE_CANDIDATES = 16;

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

// The buildings of `kind` in the order they are placed: those of the square
// first, then homes, then edges, so its fields lie beyond its houses.
std::vector<Building> by_role(const RegionData& data,
                              std::vector<Building> buildings) {
  const auto role = [&data](const Building& building) {
    return kind_named(data, building.kind).value_or(StructureKind{}).role;
  };
  std::stable_sort(buildings.begin(), buildings.end(),
                   [&role](const Building& one, const Building& other) {
                     return role(one) < role(other);
                   });
  return buildings;
}

// The plan of `building`, from what its kind holds.
Plan plan_of(const RegionData& data, const Building& building) {
  const StructureKind kind =
      kind_named(data, building.kind).value_or(StructureKind{});
  const Footprint footprint = building.structure.footprint;
  return Plan{building.kind, footprint, kind.blocks,
              holding_of(kind, footprint), kind.role};
}

}  // namespace

Growth growth_for(const RegionData& data, Growth rules,
                  const std::vector<Building>& builds,
                  const VillageTemplate& settlement) {
  Growth growth = std::move(rules);
  growth.plans.clear();
  growth.builds.clear();
  growth.core.clear();
  growth.settlement_kind = settlement.name;
  for (const Building& building : builds) {
    growth.builds.push_back(growth.plans.size());
    growth.plans.push_back(plan_of(data, building));
  }
  for (const Building& building : settlement.buildings) {
    growth.core.push_back(growth.plans.size());
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

// Adds `building` to `village`, laid with its corner at `corner`: the
// structure, its kind and what it holds.
void add_building(Region& region, const RegionData& data, Village& village,
                  const Building& building, Cell corner) {
  const Structure& structure = building.structure;
  village.structures.push_back(region.structures.size());
  const StructureKind made =
      kind_named(data, building.kind).value_or(StructureKind{});
  region.structures.push_back(Structure{corner, structure.durability,
                                        structure.footprint, made.blocks,
                                        made.role});
  region.structure_kinds.push_back(building.kind);
  region.holdings.push_back(holding_of(made, structure.footprint));
  village.households += building.households;
}

// Gives `village`, its structures laid, its resident army, two people for
// each household, standing on the cells of `open` nearest `centre`, bound
// for the centre, and adds it to `region`.
void settle(Region& region, const RegionData& data, Village village,
            std::vector<Cell> open, Cell centre) {
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
  region.armies.push_back(ArmySpec{centre, std::move(members), village.faction,
                                   data.factions.at(village.faction).skills});
  region.villages.push_back(std::move(village));
}

// Lays seat `name` of the authored template `kind`, owned by `owner`, with its
// area's corner at `corner`: each building at its offset, and its clearings.
void lay_seat(Region& region, const RegionData& data,
              const VillageTemplate& kind, std::size_t owner, Cell corner,
              std::string name) {
  Village village{std::move(name),      kind.name, owner, corner, kind.area, {},
                  region.armies.size(), 0,         {}};
  for (const Building& building : kind.buildings) {
    add_building(region, data, village, building,
                 Cell{corner.x + building.structure.cell.x,
                      corner.y + building.structure.cell.y});
  }
  for (const Clearing& clearing : kind.clearings) {
    village.clearings.push_back(Clearing{
        Cell{corner.x + clearing.corner.x, corner.y + clearing.corner.y},
        clearing.area});
  }
  const Cell middle = middle_of(kind.area);
  std::vector<Cell> open = open_cells(region, village);
  settle(region, data, std::move(village), std::move(open),
         Cell{corner.x + middle.x, corner.y + middle.y});
}

// The land as `owner` may build on it: `land` with every free or rough cell
// outside its territory taken.
Lot owned_by(const Lot& land, const Region& region, std::size_t owner) {
  Lot owned = land;
  for (std::size_t cell = 0; cell < owned.plots.size(); ++cell) {
    Plot& plot = owned.plots.at(cell);
    if ((plot == Plot::FREE || plot == Plot::ROUGH) &&
        region.territory.at(cell) != owner) {
      plot = Plot::TAKEN;
    }
  }
  return owned;
}

// The index on `lot` of `cell`, which lies on it.
std::size_t index_on(const Lot& lot, Cell cell) {
  return (static_cast<std::size_t>(cell.y) *
          static_cast<std::size_t>(lot.size.width)) +
         static_cast<std::size_t>(cell.x);
}

// A village to lay: its template, its owner, the centre it is laid round and
// its name.
struct Laying {
  const VillageTemplate* kind;
  std::size_t owner;
  Cell centre;
  std::string name;
};

// Lays village `laying` on `land`, its first tier's square kept clear and
// each building placed by its role (ADR 0076) on `owned`, `land` as its
// owner may build on it, and marks what it built on `land`. Returns whether
// every building found a place; when one does not, nothing is laid.
bool lay_village(Region& region, const RegionData& data, const Growth& growth,
                 Lot& land, Lot owned, Laying laying) {
  const VillageTemplate& kind = *laying.kind;
  const std::size_t owner = laying.owner;
  const Cell centre = laying.centre;
  // Its start list is placed nearest first within the largest tier's
  // vicinity, so a list larger than the first tier's still lays.
  const Tier& first = growth.tiers.front();
  const Townsite site{centre, first.square, growth.tiers.back().vicinity};
  const Cell corner = square_corner(centre, first.square);
  build_on(owned, corner, first.square, false);
  std::vector<std::pair<Building, Cell>> placed;
  std::vector<Structure> homes;
  for (const Building& building : by_role(data, kind.buildings)) {
    const StructureKind made =
        kind_named(data, building.kind).value_or(StructureKind{});
    const Footprint footprint = building.structure.footprint;
    const std::optional<Cell> corner_at =
        place(owned, site, footprint, made.role, homes,
              placement_draw(static_cast<std::uint64_t>(region.seed),
                             region.villages.size(), placed.size()));
    if (!corner_at) {
      return false;
    }
    build_on(owned, *corner_at, footprint, made.blocks);
    if (made.role == Role::HOME) {
      homes.push_back(
          Structure{*corner_at, 0, footprint, made.blocks, made.role});
    }
    placed.emplace_back(building, *corner_at);
  }
  Village village{
      std::move(laying.name), kind.name, owner, corner, first.square, {},
      region.armies.size(),   0,         {}};
  build_on(land, corner, first.square, false);
  const auto inside_square = [corner, &first](Cell cell) {
    return cell.x >= corner.x && cell.y >= corner.y &&
           cell.x < corner.x + first.square.width &&
           cell.y < corner.y + first.square.height;
  };
  for (const auto& [building, laid_at] : placed) {
    add_building(region, data, village, building, laid_at);
    build_on(land, laid_at, building.structure.footprint,
             kind_named(data, building.kind).value_or(StructureKind{}).blocks);
  }
  // The residents stand on the free or rough cells within its vicinity, or
  // its square's, nearest the centre, and take them from later villages.
  std::vector<Cell> open;
  for (Cell cell{0, std::max(centre.y - site.vicinity, 0)};
       cell.y <= std::min(centre.y + site.vicinity, land.size.height - 1);
       ++cell.y) {
    for (cell.x = std::max(centre.x - site.vicinity, 0);
         cell.x <= std::min(centre.x + site.vicinity, land.size.width - 1);
         ++cell.x) {
      const Plot plot = land.plots.at(index_on(land, cell));
      if (plot == Plot::FREE || plot == Plot::ROUGH || inside_square(cell)) {
        open.push_back(cell);
      }
    }
  }
  settle(region, data, std::move(village), std::move(open), centre);
  for (const Recruit& resident : region.armies.back().members) {
    Plot& plot = land.plots.at(index_on(land, resident.cell));
    plot = plot == Plot::BUILT ? plot : Plot::TAKEN;
  }
  return true;
}

// The centre of each village of `region`, where its residents are bound.
std::vector<Cell> centres_of(const Region& region) {
  std::vector<Cell> centres;
  centres.reserve(region.villages.size());
  for (const Village& village : region.villages) {
    centres.push_back(region.armies.at(village.army).objective);
  }
  return centres;
}

// Draws each sect's seat a corner among those where the seat template lies
// on cells `room` gives its faction, its centre no nearer than the growth's
// `apart` to another seat's; those cells then hold `nobody`. Returns each
// seat's corner, in the sects' order. Throws std::runtime_error when a seat
// finds no room.
std::vector<Cell> site_seats(const RegionData& data, const Growth& growth,
                             std::vector<std::size_t>& room, Random& random,
                             std::size_t nobody) {
  std::vector<Cell> seats;
  std::vector<Cell> centres;
  const Cell middle = middle_of(data.seat.area);
  for (const SectData& sect : data.sects) {
    std::vector<Cell> corners = corners_for(room, sect.faction, data.seat.area);
    corners.erase(std::remove_if(corners.begin(), corners.end(),
                                 [&centres, &growth, middle](Cell corner) {
                                   return !open_site(Cell{corner.x + middle.x,
                                                          corner.y + middle.y},
                                                     centres, growth.apart);
                                 }),
                  corners.end());
    if (corners.empty()) {
      throw std::runtime_error("no room for a seat");
    }
    const Cell corner = corners.at(static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(corners.size()))));
    for (const Cell cell : cells_of(corner, data.seat.area)) {
      room.at(index_of(cell)) = nobody;
    }
    seats.push_back(corner);
    centres.push_back(Cell{corner.x + middle.x, corner.y + middle.y});
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
  // A village's start list is placed by role: it must house someone, and
  // every building must cover a cell.
  const auto unplaceable = [](const VillageTemplate& village) {
    const auto housed = [](const Building& building) {
      return building.households > 0;
    };
    return village.buildings.empty() ||
           std::none_of(village.buildings.begin(), village.buildings.end(),
                        housed) ||
           std::any_of(village.buildings.begin(), village.buildings.end(),
                       [](const Building& building) {
                         return !spans(building.structure.footprint);
                       });
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
      !data.life.growth ||
      std::any_of(data.villages.begin(), data.villages.end(), unplaceable) ||
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

// The land as generation builds on `map`: free where buildable, rough
// where bodies only pass, blocked elsewhere.
Lot land_of(const Map& map) {
  const Footprint size{map.width(), map.height()};
  Lot land{size, {}, {}};
  const std::size_t cells = static_cast<std::size_t>(size.width) *
                            static_cast<std::size_t>(size.height);
  land.plots.reserve(cells);
  land.passable.reserve(cells);
  for (const Cell cell : cells_of(Cell{0, 0}, size)) {
    Plot plot = Plot::BLOCKED;
    if (map.buildable(cell)) {
      plot = Plot::FREE;
    } else if (map.passable(cell)) {
      plot = Plot::ROUGH;
    }
    land.plots.push_back(plot);
    land.passable.push_back(map.passable(cell));
  }
  return land;
}

// What siting the villages reads and keeps: the tables and their growth,
// the land as built so far, the owner of each free buildable cell, the owner
// no faction is, and the seats' centres.
struct Siting {
  const RegionData* data;
  Growth growth;
  Lot land;
  std::vector<std::size_t> room;
  std::size_t nobody;
  std::vector<Cell> seat_centres;
};

// The centres village number `number` may take for `owner`: where the first
// tier's square lies on free buildable cells of its, its centre no nearer
// than `apart` to `centres`.
std::vector<Cell> village_sites(const Siting& siting, std::size_t owner,
                                const std::vector<Cell>& centres) {
  const Footprint square = siting.growth.tiers.front().square;
  std::vector<Cell> sites = corners_for(siting.room, owner, square);
  for (Cell& site : sites) {
    site = Cell{site.x + (square.width / 2), site.y + (square.height / 2)};
  }
  sites.erase(std::remove_if(sites.begin(), sites.end(),
                             [&centres, &siting](Cell site) {
                               return !open_site(site, centres,
                                                 siting.growth.apart);
                             }),
              sites.end());
  return sites;
}

// The index among `sites` of the drawn candidate farthest from `centres`,
// as a capital is drawn, so the villages leave one another room.
std::size_t farthest_drawn(const std::vector<Cell>& sites, Random& random,
                           const std::vector<Cell>& centres) {
  const auto clearance = [&centres](Cell site) {
    std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
    for (const Cell centre : centres) {
      nearest = std::min(nearest, distance_squared(site, centre));
    }
    return nearest;
  };
  std::size_t drawn = 0;
  for (std::int32_t candidate = 0;
       candidate < SITE_CANDIDATES && !sites.empty(); ++candidate) {
    const auto site = static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(sites.size())));
    if (candidate == 0 ||
        clearance(sites.at(site)) > clearance(sites.at(drawn))) {
      drawn = site;
    }
  }
  return drawn;
}

// A village to site: its number, and the template drawn for it.
struct Drawn {
  std::int32_t number;
  std::size_t kind;
};

// Sites and lays village `drawn_village`, owned by the factions in turn by
// its number, of the template drawn for it, trying the open sites from the
// drawn one until its buildings fit, and takes what it built from `room`.
// Throws std::runtime_error when none fits.
void site_village(Region& region, Siting& siting, Random& random,
                  Drawn drawn_village) {
  const RegionData& data = *siting.data;
  const std::int32_t number = drawn_village.number;
  const std::size_t owner =
      static_cast<std::size_t>(number) % data.factions.size();
  const VillageTemplate& kind = data.villages.at(drawn_village.kind);
  std::vector<Cell> centres = centres_of(region);
  centres.insert(centres.end(), siting.seat_centres.begin(),
                 siting.seat_centres.end());
  const std::vector<Cell> sites = village_sites(siting, owner, centres);
  const std::string name =
      village_name(kind.name, static_cast<std::size_t>(number));
  const std::size_t drawn = farthest_drawn(sites, random, centres);
  bool laid = false;
  for (std::size_t tried = 0; tried < sites.size() && !laid; ++tried) {
    laid = lay_village(
        region, data, siting.growth, siting.land,
        owned_by(siting.land, region, owner),
        Laying{&kind, owner, sites.at((drawn + tried) % sites.size()), name});
  }
  if (!laid) {
    throw std::runtime_error(
        sites.empty() ? "no room for a village: no open site"
                      : "no room for a village: no site fits its buildings");
  }
  for (std::size_t cell = 0; cell < CELLS; ++cell) {
    if (siting.land.plots.at(cell) == Plot::BUILT) {
      siting.room.at(cell) = siting.nobody;
    }
  }
}

// The rules a world on `data` keeps, on a region or a map.
Rules rules_of(const RegionData& data) {
  Rules rules{SEPARATION,
              hostility(data.factions),
              {DURABILITY, DURABILITY / 2, 1},
              TICKS_PER_DAY,
              RUMOUR_REACH,
              TALK_REACH,
              SIGHT};
  rules.map_sight = MAP_SIGHT;
  rules.tokens = default_tokens();
  return rules;
}

// Gives each faction of `region` its succession, admission and election,
// and its first village's first resident as its leader.
void lead(Region& region, const RegionData& data) {
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
}

// Each sect's seat corner on `map`, where its seat place puts the seat
// template's middle, its area then taken from `land` before the villages are
// laid, as the region's seats are sited first; none for a sect no place
// seats. Throws std::invalid_argument when a place seats a sect `data`
// lacks, and std::runtime_error when a seat's area is not free buildable
// land or a sect has two seat places.
std::vector<std::optional<Cell>> seat_corners_on(const CompiledMap& map,
                                                 const RegionData& data,
                                                 Lot& land) {
  const Footprint size = land.size;
  const Cell middle = middle_of(data.seat.area);
  std::vector<std::optional<Cell>> seat_corners(data.sects.size());
  for (const Place& place : map.places()) {
    const auto* seat = std::get_if<SeatPlace>(&place.settles);
    if (seat == nullptr) {
      continue;
    }
    if (seat->sect >= data.sects.size()) {
      throw std::invalid_argument("a place seats a sect the data lacks");
    }
    const Cell corner{place.cell.x - middle.x, place.cell.y - middle.y};
    const std::vector<Cell> area = cells_of(corner, data.seat.area);
    const bool fits = std::all_of(area.begin(), area.end(), [&](Cell cell) {
      return cell.x >= 0 && cell.y >= 0 && cell.x < size.width &&
             cell.y < size.height &&
             land.plots.at(index_on(land, cell)) == Plot::FREE;
    });
    if (!fits || seat_corners.at(seat->sect)) {
      throw std::runtime_error(fits ? "a sect has more than one seat place"
                                    : "no room for a seat at its place");
    }
    for (const Cell cell : area) {
      land.plots.at(index_on(land, cell)) = Plot::TAKEN;
    }
    seat_corners.at(seat->sect) = corner;
  }
  return seat_corners;
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
                rules_of(data),
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

  // How many villages there are and each one's template are drawn first,
  // as before villages were kept apart, with the stream stepped once a
  // village where its corner was drawn, so a seed gives each faction the
  // same templates.
  const std::int32_t villages = MIN_VILLAGES + random.below(VILLAGE_COUNTS);
  std::vector<std::size_t> kinds;
  for (std::int32_t number = 0; number < villages; ++number) {
    kinds.push_back(static_cast<std::size_t>(
        random.below(static_cast<std::int32_t>(data.villages.size()))));
    static_cast<void>(random.below(1));
  }
  // Seats are sited next, as the largest fixed layouts, and laid after the
  // villages, so the first village's residents still lead their factions.
  const Growth growth = data.life.growth.value_or(Growth{});
  const std::vector<Cell> seat_corners =
      site_seats(data, growth, room, random, nobody);
  Siting siting{&data, growth, land_of(map), std::move(room), nobody, {}};
  for (const Cell seat : seat_corners) {
    const Cell middle = middle_of(data.seat.area);
    siting.seat_centres.push_back(Cell{seat.x + middle.x, seat.y + middle.y});
  }
  for (std::int32_t number = 0; number < villages; ++number) {
    site_village(region, siting, random,
                 Drawn{number, kinds.at(static_cast<std::size_t>(number))});
  }
  std::vector<std::size_t> seats;
  for (std::size_t sect = 0; sect < data.sects.size(); ++sect) {
    seats.push_back(region.villages.size());
    lay_seat(region, data, data.seat, data.sects.at(sect).faction,
             seat_corners.at(sect),
             data.seat.name + " of " + data.sects.at(sect).name);
  }
  lead(region, data);
  region.sects = sects_of(region, data, seats);
  return region;
}

Region settle_map(Seed seed, const RegionData& data, const CompiledMap& map) {
  validate(data);
  const auto side = static_cast<std::size_t>(map.side());
  Region region{seed,
                std::vector<std::string>(side, std::string(side, '.')),
                {},
                {},
                {},
                rules_of(data),
                std::vector<std::size_t>(side * side),
                std::vector<std::size_t>(side * side, data.factions.size()),
                {},
                {},
                life_of(data),
                {},
                {}};
  const Footprint size{map.side(), map.side()};
  for (const Cell cell : cells_of(Cell{0, 0}, size)) {
    const std::size_t slot = (static_cast<std::size_t>(cell.y) * side) +
                             static_cast<std::size_t>(cell.x);
    region.terrain.at(slot) = map.terrain(cell);
    region.map_rows.at(static_cast<std::size_t>(cell.y))
        .at(static_cast<std::size_t>(cell.x)) = tile_of(map.footing(cell));
  }
  Lot land = land_of(Map{region.map_rows});
  const Growth growth = data.life.growth.value_or(Growth{});
  const std::vector<std::optional<Cell>> seat_corners =
      seat_corners_on(map, data, land);
  for (const Place& place : map.places()) {
    const auto* village = std::get_if<VillagePlace>(&place.settles);
    if (village == nullptr) {
      continue;
    }
    if (village->owner >= data.factions.size() ||
        village->settlement >= data.villages.size()) {
      throw std::invalid_argument(
          "a place names a faction or template the data lacks");
    }
    const VillageTemplate& kind = data.villages.at(village->settlement);
    if (!lay_village(region, data, growth, land, land,
                     Laying{&kind, village->owner, place.cell,
                            village_name(kind.name, region.villages.size())})) {
      throw std::runtime_error("no room for a village at its place");
    }
  }
  std::vector<std::size_t> seats;
  for (std::size_t sect = 0; sect < data.sects.size(); ++sect) {
    if (!seat_corners.at(sect)) {
      throw std::runtime_error("a sect has no seat place");
    }
    seats.push_back(region.villages.size());
    lay_seat(region, data, data.seat, data.sects.at(sect).faction,
             seat_corners.at(sect).value_or(Cell{0, 0}),
             data.seat.name + " of " + data.sects.at(sect).name);
  }
  lead(region, data);
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

std::string village_name(const std::string& kind, std::size_t village) {
  return kind + " " + std::to_string(village + 1);
}

std::optional<std::size_t> gate_of(const Village& village,
                                   const std::vector<std::string>& kinds) {
  const auto gate = [&kinds](std::size_t structure) {
    return structure < kinds.size() && kinds.at(structure) == SEAT_GATE;
  };
  const auto found =
      std::find_if(village.structures.begin(), village.structures.end(), gate);
  if (found == village.structures.end()) {
    return std::nullopt;
  }
  return *found;
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
    std::vector<std::string> kinds;
    holdings.reserve(village.structures.size());
    kinds.reserve(village.structures.size());
    for (const std::size_t structure : village.structures) {
      holdings.push_back(region.holdings.at(structure));
      kinds.push_back(region.structure_kinds.at(structure));
    }
    specs.push_back(VillageSpec{village.structures, village.army,
                                std::move(holdings), village.households,
                                village.clearings, std::move(kinds)});
  }
  return specs;
}

}  // namespace murim
