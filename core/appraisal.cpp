#include "appraisal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "limits.hpp"
#include "person.hpp"

namespace murim {
namespace {

constexpr std::uint64_t MOST = std::numeric_limits<std::uint64_t>::max();

// Returns `left` times `right`, at most MOST.
std::uint64_t times(std::uint64_t left, std::uint64_t right) {
  return right != 0 && left > MOST / right ? MOST : left * right;
}

}  // namespace

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
