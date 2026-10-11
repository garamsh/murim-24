#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace murim {

/// Talent for one discipline: how much each practice adds to attainment,
/// and the most attainment it can reach. Hidden from the player.
struct Aptitude {
  std::uint32_t growth = 0;
  std::uint32_t limit = 0;
};

/// A uniform draw over [mean - spread, mean + spread].
struct Spread {
  std::uint32_t mean = 0;
  std::uint32_t spread = 0;
};

/// How one discipline's talent is drawn at birth.
struct DisciplineDraw {
  std::string name;
  Spread growth;
  Spread limit;
};

/// The lowest and highest value a draw can give, or a span of ages.
struct Range {
  std::uint32_t low = 0;
  std::uint32_t high = 0;
};

/// What a soul carries between bodies: the lives it has lived, and whether
/// each discipline was practised in one of them.
struct Soul {
  std::uint32_t lives = 0;
  std::vector<bool> practised;
};

/// The range a spread draws over.
[[nodiscard]] Range range_of(Spread values);

}  // namespace murim
