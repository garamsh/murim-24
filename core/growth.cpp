#include "growth.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "limits.hpp"

namespace murim {

void check_growth(const Growth& growth) {
  const auto unknown = [&growth](std::size_t plan) {
    return plan >= growth.plans.size();
  };
  const bool reaches_out = !reachable(growth.reach) ||
                           !reachable(growth.spacing) ||
                           !reachable(growth.claim) || !reachable(growth.apart);
  const bool misshapen =
      std::any_of(growth.plans.begin(), growth.plans.end(),
                  [](const Plan& plan) { return !spans(plan.footprint); });
  const auto off_square = [&growth, &unknown](std::size_t plan) {
    return unknown(plan) || growth.plans.at(plan).role != Role::SQUARE;
  };
  bool tiered = !growth.tiers.empty() && growth.tiers.front().residents == 0;
  for (std::size_t tier = 0; tier < growth.tiers.size(); ++tier) {
    const Tier& each = growth.tiers.at(tier);
    tiered =
        tiered && spans(each.square) && reachable(each.vicinity) &&
        (tier == 0 || each.residents > growth.tiers.at(tier - 1).residents) &&
        std::none_of(each.square_kinds.begin(), each.square_kinds.end(),
                     off_square);
  }
  if (growth.interval == 0 || growth.pressure > PER_MILLE || reaches_out ||
      misshapen || growth.terrace == 0 || !tiered ||
      std::any_of(growth.builds.begin(), growth.builds.end(), unknown) ||
      std::any_of(growth.core.begin(), growth.core.end(), unknown)) {
    throw std::invalid_argument(
        "growth must check at an interval, press within a thousand per "
        "mille, reach within MAX_REACH, raise terraces at some labour, start "
        "its tiers at no one and raise them in order with squares of 1 to "
        "MAX_FOOTPRINT cells, and name only plans it has, of the square role "
        "where a tier names its square kinds");
  }
}

std::size_t tier_for(const Growth& growth, std::uint64_t residents) {
  std::size_t reached = 0;
  for (std::size_t tier = 0; tier < growth.tiers.size(); ++tier) {
    if (residents >= growth.tiers.at(tier).residents) {
      reached = tier;
    }
  }
  return reached;
}

}  // namespace murim
