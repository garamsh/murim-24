#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {

/// Cells along each side of a generated region.
constexpr std::int32_t REGION_SIDE = 128;

/// A kind of ground: its name, what it bears, buildings and bodies, bodies
/// only or neither, and how often generation picks it relative to the other
/// kinds.
struct TerrainKind {
  std::string name;
  Footing footing;
  std::int32_t weight;
};

/// A faction: its name, the factions, by index, whose bodies it attacks,
/// the skills its armies use, which decide, among other things, whether they
/// rebuild what is broken, how its leadership passes, and its stance.
struct Faction {
  std::string name;
  std::vector<std::size_t> hostile_to;
  std::vector<Skill> skills;
  /// How its leadership passes; none keeps its leader.
  std::optional<Duel> succession;
  /// What its villages do as they press against their caps, in order of
  /// preference; none grows no village.
  std::vector<Expansion> stance;
  /// Who its sects admit; none admits no one, and its sects have no elders.
  std::optional<Admission> admission;
  /// How it elects its leader; none elects no one.
  std::optional<Election> election;
};

/// A sect: its name and its faction, by index. Its seat is laid out from the
/// seat template in its faction's territory.
struct SectData {
  std::string name;
  std::size_t faction;
};

/// A kind of structure, by `name`: the people it houses, the food it stores,
/// the food each cell of its footprint yields each harvest, which makes it a
/// field, whether it blocks movement, the pupils it teaches a sect seated in
/// its village, and, for a workshop, the people it arms and tends, the edge
/// per mille they strike with and the health a day they recover (Holding).
struct StructureKind {
  std::string name;
  std::uint32_t housing;
  std::uint32_t storage;
  std::uint32_t yield;
  bool blocks;
  std::uint32_t pupils;
  std::uint32_t arms = 0;
  std::uint32_t tends = 0;
  std::uint32_t edge = 0;
  std::uint32_t healing = 0;
};

/// A structure a village template places, its cell an offset from the area's
/// corner; the `kind` of building it is, which names its art and its data;
/// and how many `households` the village starts with in it, each a married
/// man and woman.
struct Building {
  Structure structure;
  std::string kind;
  std::uint32_t households;
};

/// A kind of village: the `area` it occupies, and its `buildings`. Its
/// resident army, two people for each household its buildings start with,
/// stands on the open cells nearest the area's centre.
struct VillageTemplate {
  std::string name;
  Footprint area{};
  std::vector<Building> buildings;
  /// The land growth keeps clear, each corner an offset from the area's
  /// corner; it may reach past the area.
  std::vector<Clearing> clearings;
};

/// The tables a region is generated from.
struct RegionData {
  std::vector<TerrainKind> terrain;
  std::vector<Faction> factions;
  std::vector<VillageTemplate> villages;
  /// How its people are born, age and are born again.
  Life life;
  /// The kinds of structure the templates build.
  std::vector<StructureKind> kinds;
  /// The template every sect's seat is laid out from.
  VillageTemplate seat;
  /// The sects, each seated in a village of its own. The life's manuals give
  /// the manuals each holds, in this order.
  std::vector<SectData> sects;
};

/// A village as generated: the `area` from `corner` it occupies, its owner,
/// its structures and its resident army, by their indices in the region's
/// lists.
struct Village {
  std::string name;
  /// The name of the template it was laid out from.
  std::string kind;
  std::size_t faction;
  Cell corner;
  Footprint area;
  std::vector<std::size_t> structures;
  std::size_t army;
  /// The households its residents start as.
  std::uint32_t households;
  /// The land growth keeps clear, from its template's clearings.
  std::vector<Clearing> clearings;
};

/// A generated region: the inputs a World is created from, and the terrain
/// kind and owning faction of each cell, row by row.
struct Region {
  Seed seed;
  std::vector<std::string> map_rows;
  std::vector<ArmySpec> armies;
  std::vector<Structure> structures;
  /// The kind of each structure, in the order of `structures`.
  std::vector<std::string> structure_kinds;
  Rules rules;
  std::vector<std::size_t> terrain;
  std::vector<std::size_t> territory;
  std::vector<Village> villages;
  /// Each faction's leadership: the first resident of its first village, if
  /// it has one, and its succession.
  std::vector<Leadership> leadership;
  /// The tables' rules of life, as the world is created with them.
  Life life;
  /// What each structure holds, by its kind, in the order of `structures`.
  std::vector<Holding> holdings;
  /// The sects, each seated in a village laid out after the others, its
  /// founders that village's residents, the first an elder.
  std::vector<SectSpec> sects;
};

/// Returns `rules` with the plans growth lays: a plan for each of `builds`,
/// from what its kind holds in `data`, its builds, then one for each of
/// `settlement`'s buildings, laid out for a new village of that template.
[[nodiscard]] Growth growth_for(const RegionData& data, Growth rules,
                                const std::vector<Building>& builds,
                                const VillageTemplate& settlement);

/// Returns whether `data` passes the checks generate_region makes of it.
[[nodiscard]] bool consistent(const RegionData& data);

/// Generates a region from `seed` and `data`: terrain and territories, six
/// to eight villages owned in turn by each faction, and a seat for each sect
/// in its faction's territory, each village on buildable cells in its owner's
/// territory and overlapping no other. The same seed and data give the same
/// region. Throws std::invalid_argument when a table is empty, the terrain
/// weights are not positive, a faction has no skills, a village template or
/// the seat template has no buildings, houses no household, has one outside
/// its area, two that overlap, one over the area's centre, or one of a kind
/// the kinds do not name, a faction is hostile to one that does not exist, or
/// a sect names a faction that does not exist; throws std::runtime_error when
/// a village or a seat finds no room.
[[nodiscard]] Region generate_region(Seed seed, const RegionData& data);

/// Returns the villages of `region` as a World is created with them: each
/// village's structures, resident army, what its structures hold and the land
/// growth keeps clear.
[[nodiscard]] std::vector<VillageSpec> village_specs(const Region& region);

/// Returns the cells of `village`'s area that none of its structures
/// covers, row by row.
[[nodiscard]] std::vector<Cell> open_cells(const Region& region,
                                           const Village& village);

/// Returns an army of one person born into village `village` of `region`:
/// of the village's faction, with its residents' skills and health, making
/// the village its home, standing on the first open cell of the village's area,
/// row by row, that no resident stands on, with that cell as its objective.
/// Throws std::out_of_range when there is no such village, and
/// std::runtime_error when the village has no such cell.
[[nodiscard]] ArmySpec born_in(const Region& region, std::size_t village);

}  // namespace murim
