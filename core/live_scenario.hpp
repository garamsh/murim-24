#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {

/// The live benchmark scenario, built from a fixed seed: LIVE_PEOPLE people
/// acting on every tick, as issue #216 asks the live tier to hold, spread
/// over villages, the roads between them and two battles, on a map of four
/// factions where only the two pairs facing each other in battle are
/// hostile.
struct LiveScenario {
  Seed seed;
  std::vector<std::string> map_rows;
  std::vector<ArmySpec> armies;
  Rules rules;
  /// Armies 0 to battle_armies - 1 fight the two battles; the villages' and
  /// the roads' follow.
  std::size_t battle_armies;
};

/// The people the live scenario holds.
constexpr std::size_t LIVE_PEOPLE = 5'000;

/// The capacity the live scenario's rules set: LIVE_PEOPLE with a quarter
/// more for headroom, so the bench can tell a world that fills it from one
/// that fits.
constexpr std::size_t LIVE_CAPACITY = 6'250;

/// Returns the live benchmark scenario; every call returns the same one.
[[nodiscard]] LiveScenario live_scenario();

}  // namespace murim
