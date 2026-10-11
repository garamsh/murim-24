#include "growth.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "limits.hpp"
#include "map.hpp"

namespace murim {
namespace {

// Whether `layout` places only known plans, inside `area`, over no other
// placement and off the area's centre.
bool lays_out(const Growth& growth) {
  const Footprint area = growth.settlement;
  std::vector<bool> covered(static_cast<std::size_t>(area.width) *
                                static_cast<std::size_t>(area.height),
                            false);
  const auto cover = [&area, &covered](Cell cell) {
    const bool inside = cell.x >= 0 && cell.y >= 0 && cell.x < area.width &&
                        cell.y < area.height;
    if (!inside) {
      return false;
    }
    const std::size_t slot = (static_cast<std::size_t>(cell.y) *
                              static_cast<std::size_t>(area.width)) +
                             static_cast<std::size_t>(cell.x);
    const bool free = !covered.at(slot);
    covered.at(slot) = true;
    return free;
  };
  const auto placed = [&growth, &cover](const Placement& placement) {
    if (placement.plan >= growth.plans.size()) {
      return false;
    }
    const std::vector<Cell> cells =
        cells_of(placement.offset, growth.plans.at(placement.plan).footprint);
    return std::all_of(cells.begin(), cells.end(), cover);
  };
  return std::all_of(growth.layout.begin(), growth.layout.end(), placed) &&
         cover(Cell{area.width / 2, area.height / 2});
}

}  // namespace

void check_growth(const Growth& growth) {
  const auto unknown = [&growth](std::size_t plan) {
    return plan >= growth.plans.size();
  };
  const bool reaches_out =
      !reachable(growth.reach) || !reachable(growth.vicinity) ||
      !reachable(growth.spacing) || !reachable(growth.claim);
  const bool misshapen =
      !spans(growth.settlement) ||
      std::any_of(growth.plans.begin(), growth.plans.end(),
                  [](const Plan& plan) { return !spans(plan.footprint); });
  if (growth.interval == 0 || growth.pressure > PER_MILLE || reaches_out ||
      misshapen ||
      std::any_of(growth.builds.begin(), growth.builds.end(), unknown) ||
      !lays_out(growth)) {
    throw std::invalid_argument(
        "growth must check at an interval, press within a thousand per "
        "mille, reach within MAX_REACH, and lay footprints of 1 to "
        "MAX_FOOTPRINT cells from known plans inside its settlement, apart "
        "and off its centre");
  }
}

}  // namespace murim
