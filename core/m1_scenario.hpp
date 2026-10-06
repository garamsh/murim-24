#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {

/// Ticks the simulation runs per second.
constexpr int TICKS_PER_SECOND = 30;

/// The M1 benchmark scenario, built from a fixed seed: the worst case for
/// the tick budget, as many people as a world may hold, MAX_PEOPLE, every
/// one of them in one battle of two hostile lines of armies with a barricade
/// between them, and two caster armies whose area strikes break it.
struct M1Scenario {
  Seed seed;
  std::vector<std::string> map_rows;
  std::vector<ArmySpec> armies;
  std::vector<Structure> structures;
  /// The kind of each structure, in the order of `structures`: barrels, a
  /// barricade between the lines.
  std::vector<std::string> structure_kinds;
  Rules rules;
  /// The cell the battle is fought around.
  Cell centre;
  /// Armies casters_first and casters_first + 1 cast the area strikes.
  std::size_t casters_first;
};

/// Returns the M1 benchmark scenario; every call returns the same one.
[[nodiscard]] M1Scenario m1_scenario();

}  // namespace murim
