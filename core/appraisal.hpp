#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

#include "flow_field.hpp"
#include "limits.hpp"
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

/// The might, per mille, and the weight of a person in no realm.
constexpr std::uint32_t UNRANKED_MIGHT = PER_MILLE;
constexpr std::uint32_t UNRANKED_WEIGHT = 1;

/// The ratio an appraisal reads when its own side has no strength and the
/// threat has some, and the most any ratio reads.
constexpr std::uint32_t RATIO_CAP = UINT32_MAX;

/// How many stances there are.
constexpr std::size_t STANCES = 5;

/// A stance other than close yields only once held this many ticks, and to a
/// ratio strictly below this share, per mille, of its entry ratio: ADR 0071's
/// 30 ticks and three quarters.
constexpr std::uint64_t STANCE_TICKS = 30;
constexpr std::uint32_t RELEASE_SHARE = 750;

/// What a person's appraisal read: its ratio, whether no one of its side but
/// itself stood in its sight, whether it stands on its home ground, and
/// whether it has a refuge to flee to.
struct Reading {
  std::uint32_t ratio = 0;
  bool alone = false;
  bool home_ground = false;
  bool refuge = false;
};

/// Returns the stance a person of `thresholds`, holding `held` for `age`
/// ticks, takes on `reading` (ADR 0071). Its choice is flee above the flee
/// ratio, then regroup when alone above half the back-off ratio, then hold
/// on its home ground or back off above the back-off ratio, then close; a
/// stance needs what it acts on, so fleeing and regrouping need a refuge and
/// holding home ground. From close, or to flee, or out of a stance it can no
/// longer hold, it takes its choice at once; otherwise only after
/// STANCE_TICKS and below RELEASE_SHARE of the stance's entry ratio.
[[nodiscard]] Stance next_stance(Stance held, std::uint64_t age,
                                 const Reading& reading,
                                 const Thresholds& thresholds);

/// Returns whether a person in `stance` engages foes and strikes them: it
/// closes or holds, and neither backs off, flees nor regroups.
[[nodiscard]] bool engages(Stance stance);

/// The strength-weighted mean of the offsets added to it. It halves its sums
/// and the weights still to come whenever an addition would overflow them,
/// so each weight keeps its share up to the halvings' rounding.
class Centroid {
 public:
  /// Adds offset (`x`, `y`), each within 2^32 of zero, of strength
  /// `strength`.
  void add(std::int64_t x, std::int64_t y, std::uint64_t strength);

  /// Returns the mean offset, or none when nothing of weight was added.
  [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>> mean()
      const;

 private:
  std::int64_t x_ = 0;
  std::int64_t y_ = 0;
  std::uint64_t weight_ = 0;
  std::uint32_t shift_ = 0;
};

/// The eight unit steps, in the order ties between them are broken.
constexpr std::array<Step, 8> STEPS{
    {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}}};

/// Returns, of the steps `open` allows, the one leading most directly along
/// (`x`, `y`), the earliest of STEPS among equals; none when no allowed step
/// leads along it at all.
template <typename Open>
[[nodiscard]] std::optional<Step> step_along(std::int64_t x, std::int64_t y,
                                             Open open) {
  // Halving both keeps the direction and the squares below within range.
  constexpr std::int64_t SPAN = std::int64_t{1} << 20;
  while (std::abs(x) > SPAN || std::abs(y) > SPAN) {
    x /= 2;
    y /= 2;
  }
  std::optional<Step> best;
  std::int64_t best_dot = 0;
  std::int64_t best_length = 1;
  for (const Step step : STEPS) {
    const std::int64_t dot = (step.x * x) + (step.y * y);
    const std::int64_t length = (step.x * step.x) + (step.y * step.y);
    // Of two steps along the line, the one at the smaller angle to it.
    if (dot > 0 && open(step) &&
        (!best || dot * dot * best_length > best_dot * best_dot * length)) {
      best = step;
      best_dot = dot;
      best_length = length;
    }
  }
  return best;
}

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
