#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {

/// Cells along each side of a generated region.
constexpr std::int32_t REGION_SIDE = 96;

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

/// A kind of village: a square of `side` cells with a structure at each of
/// `structures`, offsets from its corner. Its resident army stands at its
/// centre, with a fixed number of agents for each structure.
struct VillageTemplate {
  std::string name;
  std::int32_t side;
  std::vector<Cell> structures;
};

/// The tables a region is generated from.
struct RegionData {
  std::vector<TerrainKind> terrain;
  std::vector<Faction> factions;
  std::vector<VillageTemplate> villages;
};

/// A village as generated: the square of `side` cells from `corner` it
/// occupies, its owner, its structures and its resident army, by their
/// indices in the region's lists.
struct Village {
  std::string name;
  std::size_t faction;
  Cell corner;
  std::int32_t side;
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
/// terrain weights are not positive, a village template has no structures or
/// one outside its square, or a faction is hostile to one that does not exist;
/// throws std::runtime_error when a village finds no room.
[[nodiscard]] Region generate_region(Seed seed, const RegionData& data);

/// Returns an army of one agent born into village `village` of `region`: of
/// the village's faction, standing at its corner with that cell as its
/// objective, clear of the residents crowding its centre, with their skills
/// and an agent's share of their strength. Throws std::out_of_range when there
/// is no such village.
[[nodiscard]] ArmySpec born_in(const Region& region, std::size_t village);

}  // namespace murim
