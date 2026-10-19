#include "appraisal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include "limits.hpp"
#include "person.hpp"

namespace murim {
namespace {

constexpr std::uint64_t MOST = std::numeric_limits<std::uint64_t>::max();

// Returns `left` times `right`, at most MOST.
std::uint64_t times(std::uint64_t left, std::uint64_t right) {
  return right != 0 && left > MOST / right ? MOST : left * right;
}

// The stance chosen afresh on `reading` under `thresholds`.
Stance chosen(const Reading& reading, const Thresholds& thresholds) {
  const std::uint64_t ratio = reading.ratio;
  if (reading.refuge && ratio > thresholds.flee) {
    return Stance::FLEE;
  }
  if (reading.refuge && reading.alone && 2 * ratio > thresholds.back_off) {
    return Stance::REGROUP;
  }
  if (ratio > thresholds.back_off) {
    return reading.home_ground ? Stance::HOLD : Stance::BACK_OFF;
  }
  return Stance::CLOSE;
}

// Whether `stance` still has what it acts on under `reading`.
bool holdable(Stance stance, const Reading& reading) {
  switch (stance) {
    case Stance::FLEE:
    case Stance::REGROUP:
      return reading.refuge;
    case Stance::HOLD:
      return reading.home_ground;
    case Stance::CLOSE:
    case Stance::BACK_OFF:
      return true;
  }
  return true;
}

// The ratio, per mille, above which `stance` is entered; regroup enters
// above half of it, so its ratio is doubled against it.
std::uint64_t entry(Stance stance, const Thresholds& thresholds) {
  return stance == Stance::FLEE ? thresholds.flee : thresholds.back_off;
}

}  // namespace

Stance next_stance(Stance held, std::uint64_t age, const Reading& reading,
                   const Thresholds& thresholds) {
  const Stance fresh = chosen(reading, thresholds);
  if (held == Stance::CLOSE || fresh == Stance::FLEE ||
      !holdable(held, reading)) {
    return fresh;
  }
  // Regroup's entry is half the back-off ratio, so its ratio counts double.
  const std::uint64_t ratio = std::uint64_t{reading.ratio} *
                              (held == Stance::REGROUP ? 2 : 1) * PER_MILLE;
  const bool released =
      age >= STANCE_TICKS && ratio < RELEASE_SHARE * entry(held, thresholds);
  return released ? fresh : held;
}

bool engages(Stance stance) {
  return stance == Stance::CLOSE || stance == Stance::HOLD;
}

void Centroid::add(std::int64_t x, std::int64_t y, std::uint64_t strength) {
  constexpr std::int64_t MOST_SIGNED = std::numeric_limits<std::int64_t>::max();
  const auto fits = [&](std::uint64_t weight) {
    const auto room = [weight](std::int64_t sum, std::int64_t offset) {
      return offset == 0 ||
             weight <= static_cast<std::uint64_t>(MOST_SIGNED - std::abs(sum)) /
                           static_cast<std::uint64_t>(std::abs(offset));
    };
    return weight <= MOST - weight_ && room(x_, x) && room(y_, y);
  };
  std::uint64_t weight = strength >> shift_;
  while (weight != 0 && !fits(weight)) {
    ++shift_;
    x_ /= 2;
    y_ /= 2;
    weight_ /= 2;
    weight = strength >> shift_;
  }
  x_ += static_cast<std::int64_t>(weight) * x;
  y_ += static_cast<std::int64_t>(weight) * y;
  weight_ += weight;
}

std::optional<std::pair<std::int64_t, std::int64_t>> Centroid::mean() const {
  if (weight_ == 0) {
    return std::nullopt;
  }
  // A weight past the signed range outweighs any sum, so the mean is nil.
  if (weight_ >
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return std::pair<std::int64_t, std::int64_t>{0, 0};
  }
  const auto weight = static_cast<std::int64_t>(weight_);
  return std::pair<std::int64_t, std::int64_t>{x_ / weight, y_ / weight};
}

void check_temperaments(const Temperaments& temperaments) {
  std::uint64_t shares = 0;
  bool inverted = false;
  for (const Disposition& disposition : temperaments) {
    shares += disposition.share;
    inverted = inverted ||
               disposition.thresholds.flee <= disposition.thresholds.back_off;
  }
  if (shares != PER_MILLE || inverted) {
    throw std::invalid_argument(
        "temperaments' shares must sum to a thousand per mille, and each "
        "must flee above the ratio it backs off at");
  }
}

Temperament temperament_of(std::uint64_t draw,
                           const Temperaments& temperaments) {
  const std::uint64_t point = draw % PER_MILLE;
  std::uint64_t cumulative = 0;
  for (std::size_t temperament = 0; temperament + 1 < TEMPERAMENTS;
       ++temperament) {
    cumulative += temperaments.at(temperament).share;
    if (point < cumulative) {
      return static_cast<Temperament>(temperament);
    }
  }
  return static_cast<Temperament>(TEMPERAMENTS - 1);
}

std::uint64_t strength(std::uint64_t health, std::uint32_t might,
                       std::uint32_t weight) {
  return times(times(health, might) / PER_MILLE, weight);
}

std::uint64_t add_strength(std::uint64_t total, std::uint64_t strength) {
  return total > MOST - strength ? MOST : total + strength;
}

std::uint32_t ratio(std::uint64_t threat, std::uint64_t own) {
  if (threat == 0) {
    return 0;
  }
  // Halving both keeps their ratio while the threat is too great to scale by
  // a thousand.
  while (threat > MOST / PER_MILLE) {
    threat >>= 1U;
    own >>= 1U;
  }
  if (own == 0) {
    return RATIO_CAP;
  }
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(threat * PER_MILLE / own, RATIO_CAP));
}

}  // namespace murim
