#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {

/// Ticks the simulation runs per second.
constexpr int TICKS_PER_SECOND = 30;

/// The M1 benchmark scenario, built from a fixed seed: 20,000 agents in
/// armies at every tier around `focus`, a near battle of two hostile lines of
/// armies with structures between them, and two caster armies whose area
/// strikes break those structures.
struct M1Scenario {
  Seed seed;
  std::vector<std::string> map_rows;
  std::vector<ArmySpec> armies;
  std::vector<Structure> structures;
  /// The kind of each structure, in the order of `structures`: barrels, a
  /// barricade between the lines.
  std::vector<std::string> structure_kinds;
  Rules rules;
  Cell focus;
  /// Armies [0, near_armies) are the near battle.
  std::size_t near_armies;
  /// Armies casters_first and casters_first + 1 cast the area strikes.
  std::size_t casters_first;
};

/// Returns the M1 benchmark scenario; every call returns the same one.
[[nodiscard]] M1Scenario m1_scenario();

}  // namespace murim
