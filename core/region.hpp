#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {

/// Cells along each side of a generated region.
constexpr std::int32_t REGION_SIDE = 128;

/// A kind of ground: its name, whether bodies may stand on it, and how often
/// generation picks it relative to the other kinds.
struct TerrainKind {
  std::string name;
  bool passable;
  std::int32_t weight;
};

/// A faction: its name, the factions, by index, whose bodies it attacks, and
/// the skills its armies use, which decide, among other things, whether they
/// rebuild what is broken.
struct Faction {
  std::string name;
  std::vector<std::size_t> hostile_to;
  std::vector<Skill> skills;
};

/// A structure a village template places, its cell an offset from the area's
/// corner; the `kind` of building it is, which names its art; and how many
/// `residents` it houses.
struct Building {
  Structure structure;
  std::string kind;
  std::uint32_t residents;
};

/// A kind of village: the `area` it occupies, and its `buildings`. Its
/// resident army stands at the area's centre, as many agents as its buildings
/// house.
struct VillageTemplate {
  std::string name;
  Footprint area;
  std::vector<Building> buildings;
};

/// The tables a region is generated from.
struct RegionData {
  std::vector<TerrainKind> terrain;
  std::vector<Faction> factions;
  std::vector<VillageTemplate> villages;
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
};

/// Returns the game's terrain kinds, factions and village templates.
[[nodiscard]] RegionData region_data();

/// Generates a region from `seed` and `data`: terrain and territories, and six
/// to eight villages owned in turn by each faction, each on passable cells in
/// its owner's territory and overlapping no other. The same seed and data give
/// the same region. Throws std::invalid_argument when a table is empty, the
/// terrain weights are not positive, a faction has no skills, a village
/// template has no buildings, houses no residents, has one outside its area,
/// two that overlap, or one over the area's centre, or a faction is hostile to
/// one that does not exist; throws std::runtime_error when a village finds no
/// room.
[[nodiscard]] Region generate_region(Seed seed, const RegionData& data);

/// Returns the villages of `region` as a World is created with them: each
/// village's structures and resident army.
[[nodiscard]] std::vector<VillageSpec> village_specs(const Region& region);

/// Returns an army of one agent born into village `village` of `region`: of
/// the village's faction, with their skills and an agent's share of their
/// strength, standing on the first cell of the village's area, row by row,
/// that no structure covers and its residents do not crowd, with that cell
/// as its objective. Throws std::out_of_range when there
/// is no such village or no such cell.
[[nodiscard]] ArmySpec born_in(const Region& region, std::size_t village);

}  // namespace murim
