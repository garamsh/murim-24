#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "person.hpp"

namespace murim {

/// The ratios of threat to own strength, per mille, above which a person
/// backs off and flees (ADR 0071).
struct Thresholds {
  std::uint32_t back_off = 0;
  std::uint32_t flee = 0;
};

/// A temperament's share of births, per mille, and its thresholds.
struct Disposition {
  std::uint32_t share = 0;
  Thresholds thresholds;
};

/// How many temperaments there are.
constexpr std::size_t TEMPERAMENTS = 3;

/// Each temperament's disposition, by `Temperament`, as data.
using Temperaments = std::array<Disposition, TEMPERAMENTS>;

/// A ratio of threat to own strength, per mille, and the tick it was taken.
struct Appraisal {
  std::uint32_t ratio = 0;
  std::uint64_t tick = 0;
};

/// Ticks between a person's or an army's appraisals, each at its own phase.
constexpr std::uint64_t APPRAISAL_TICKS = 15;

/// The ratio an appraisal reads when its own side has no strength and the
/// threat has some, and the most any ratio reads.
constexpr std::uint32_t RATIO_CAP = UINT32_MAX;

/// Throws std::invalid_argument unless `temperaments`' shares sum to a
/// thousand per mille and each flee threshold lies above its back-off one.
void check_temperaments(const Temperaments& temperaments);

/// Returns the temperament draw `draw` picks under `temperaments`: the first
/// whose cumulative share lies above the draw modulo a thousand.
[[nodiscard]] Temperament temperament_of(std::uint64_t draw,
                                         const Temperaments& temperaments);

/// Returns the strength of a person of health `health` in a realm of might
/// `might` per mille and weight `weight`: its health times its might over a
/// thousand, times its weight, at most the greatest the type holds.
[[nodiscard]] std::uint64_t strength(std::uint64_t health, std::uint32_t might,
                                     std::uint32_t weight);

/// Returns `total` with `strength` added, at most the greatest the type holds.
[[nodiscard]] std::uint64_t add_strength(std::uint64_t total,
                                         std::uint64_t strength);

/// Returns the ratio of `threat` to `own`, per mille, at most RATIO_CAP: 0
/// for no threat, and RATIO_CAP for a threat against no own strength.
[[nodiscard]] std::uint32_t ratio(std::uint64_t threat, std::uint64_t own);

}  // namespace murim
