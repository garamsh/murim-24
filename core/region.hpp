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

/// A faction: its name and the factions, by index, whose bodies it attacks.
struct Faction {
  std::string name;
  std::vector<std::size_t> hostile_to;
};

/// A kind of village: the `area` it occupies, and its `structures`, each cell
/// an offset from the area's corner. Its resident army stands at the area's
/// centre, with a fixed number of agents for each structure.
struct VillageTemplate {
  std::string name;
  Footprint area;
  std::vector<Structure> structures;
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
/// terrain weights are not positive, a village template has no structures,
/// one outside its area, two that overlap, or one over the area's centre, or a
/// faction is hostile to one that does not exist; throws std::runtime_error
/// when a village finds no room.
[[nodiscard]] Region generate_region(Seed seed, const RegionData& data);

/// Returns an army of one agent born into village `village` of `region`: of
/// the village's faction, with their skills and an agent's share of their
/// strength, standing on the first cell of the village's area, row by row,
/// that no structure covers and its residents do not crowd, with that cell
/// as its objective. Throws std::out_of_range when there
/// is no such village or no such cell.
[[nodiscard]] ArmySpec born_in(const Region& region, std::size_t village);

}  // namespace murim
