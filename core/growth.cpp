#include "growth.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "calendar.hpp"
#include "flow_field.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "village.hpp"

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

// How many marked cells of a grid lie under a rectangle, read from a
// summed-area table in four lookups.
class Tally {
 public:
  Tally(const std::vector<bool>& marked, std::int32_t width,
        std::int32_t height)
      : width_(width),
        height_(height),
        sums_((static_cast<std::size_t>(width) + 1) *
                  (static_cast<std::size_t>(height) + 1),
              0) {
    for (Cell cell{0, 0}; cell.y < height; ++cell.y) {
      for (cell.x = 0; cell.x < width; ++cell.x) {
        const std::size_t index = (static_cast<std::size_t>(cell.y) *
                                   static_cast<std::size_t>(width)) +
                                  static_cast<std::size_t>(cell.x);
        sums_.at(slot(cell.x + 1, cell.y + 1)) =
            static_cast<std::int32_t>(marked.at(index)) +
            sums_.at(slot(cell.x, cell.y + 1)) +
            sums_.at(slot(cell.x + 1, cell.y)) - sums_.at(slot(cell.x, cell.y));
      }
    }
  }

  // Whether `area` from `corner` lies on the grid with no cell marked.
  [[nodiscard]] bool clear(Cell corner, Footprint area) const {
    return corner.x >= 0 && corner.y >= 0 && corner.x + area.width <= width_ &&
           corner.y + area.height <= height_ &&
           count(corner.x, corner.y, corner.x + area.width,
                 corner.y + area.height) == 0;
  }

  // Whether no cell within `margin` of `area` from `corner`, on the grid, is
  // marked.
  [[nodiscard]] bool clear_around(Cell corner, Footprint area,
                                  std::int32_t margin) const {
    return count(std::max(corner.x - margin, 0), std::max(corner.y - margin, 0),
                 std::min(corner.x + area.width + margin, width_),
                 std::min(corner.y + area.height + margin, height_)) == 0;
  }

 private:
  [[nodiscard]] std::size_t slot(std::int32_t column, std::int32_t row) const {
    return (static_cast<std::size_t>(row) *
            (static_cast<std::size_t>(width_) + 1)) +
           static_cast<std::size_t>(column);
  }

  // The marked cells in columns [left, right) of rows [top, bottom).
  [[nodiscard]] std::int32_t count(std::int32_t left, std::int32_t top,
                                   std::int32_t right,
                                   std::int32_t bottom) const {
    return sums_.at(slot(right, bottom)) - sums_.at(slot(left, bottom)) -
           sums_.at(slot(right, top)) + sums_.at(slot(left, top));
  }

  std::int32_t width_;
  std::int32_t height_;
  std::vector<std::int32_t> sums_;
};

// Twice the squared distance between the centre of `area` from `corner` and
// the centre of cell `seat`, in half cells, so it stays an integer.
std::int64_t remoteness(Cell corner, Footprint area, Cell seat) {
  const std::int64_t offset_x = (2 * std::int64_t{corner.x}) + area.width -
                                (2 * std::int64_t{seat.x}) - 1;
  const std::int64_t offset_y = (2 * std::int64_t{corner.y}) + area.height -
                                (2 * std::int64_t{seat.y}) - 1;
  return (offset_x * offset_x) + (offset_y * offset_y);
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

Plans::Plans(std::size_t structures) : laid_from_(structures) {}

Plans::Plans(std::vector<std::optional<std::size_t>> laid_from)
    : laid_from_(std::move(laid_from)) {}

const std::vector<std::optional<std::size_t>>& Plans::laid_from() const {
  return laid_from_;
}

const std::optional<std::size_t>& Plans::plan_of(std::size_t structure) const {
  return laid_from_.at(structure);
}

void Plans::add(std::optional<std::size_t> plan) { laid_from_.push_back(plan); }

Holding Plans::holding_of(std::size_t structure, const Growth& growth) const {
  return growth.plans.at(laid_from_.at(structure).value_or(0)).holding;
}

bool Plans::kept(std::size_t structures,
                 const std::optional<Growth>& growth) const {
  const std::size_t plans = growth ? growth->plans.size() : 0;
  const auto unplanned = [plans](std::optional<std::size_t> plan) {
    return plan && *plan >= plans;
  };
  return laid_from_.size() == structures &&
         std::none_of(laid_from_.begin(), laid_from_.end(), unplanned);
}

std::optional<std::size_t> build_plan(
    const Holding& held, const Growth& growth,
    const std::optional<Husbandry>& husbandry) {
  const std::uint64_t yearly = std::max<std::uint64_t>(
      std::uint64_t{husbandry ? husbandry->consumption : 1} * DAYS_PER_YEAR, 1);
  // Housing, food, storage, arms and tends, each in the people it holds,
  // the last two beyond the `reliance` a village holds with no workshop.
  constexpr std::size_t HOLDINGS = 5;
  const auto in_people = [yearly](const Holding& holding,
                                  std::uint64_t reliance) {
    return std::array<std::uint64_t, HOLDINGS>{
        holding.housing, holding.harvest / yearly, holding.storage / yearly,
        reliance + holding.arms, reliance + holding.tends};
  };
  const std::array<std::uint64_t, HOLDINGS> holds =
      in_people(held, growth.self_reliance);
  const auto scarcest = static_cast<std::size_t>(
      std::min_element(holds.begin(), holds.end()) - holds.begin());
  std::optional<std::size_t> chosen;
  std::uint64_t most = 0;
  for (const std::size_t plan : growth.builds) {
    const std::uint64_t gain =
        in_people(growth.plans.at(plan).holding, 0).at(scarcest);
    if (gain > most) {
      most = gain;
      chosen = plan;
    }
  }
  return chosen;
}

std::optional<Cell> build_site(Footprint footprint, Cell centre,
                               const Growth& growth, const Ground& ground,
                               Footprint size) {
  std::vector<bool> open = ground.covered;
  for (std::size_t cell = 0; cell < open.size(); ++cell) {
    open.at(cell) = open.at(cell) || ground.rough.at(cell);
  }
  const Tally blocked{open, size.width, size.height};
  const Tally taken{ground.taken, size.width, size.height};
  const std::int32_t vicinity = growth.vicinity;
  std::optional<Cell> best;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (Cell corner{0, centre.y - vicinity};
       corner.y + footprint.height - 1 <= centre.y + vicinity; ++corner.y) {
    for (corner.x = centre.x - vicinity;
         corner.x + footprint.width - 1 <= centre.x + vicinity; ++corner.x) {
      const std::int64_t distance = remoteness(corner, footprint, centre);
      if (distance < nearest && blocked.clear(corner, footprint) &&
          blocked.clear_around(corner, footprint, 1) &&
          taken.clear(corner, footprint)) {
        nearest = distance;
        best = corner;
      }
    }
  }
  return best;
}

std::vector<Cell> settlement_sites(
    Cell home, const Growth& growth, const Ground& ground, Footprint size,
    const FlowField& paths,
    const std::vector<std::optional<std::size_t>>& territory,
    std::size_t faction) {
  const Footprint area = growth.settlement;
  const Cell middle{area.width / 2, area.height / 2};
  std::vector<bool> open = ground.covered;
  for (std::size_t cell = 0; cell < open.size(); ++cell) {
    open.at(cell) =
        open.at(cell) || ground.rough.at(cell) || ground.taken.at(cell);
  }
  const Tally blocked{open, size.width, size.height};
  const Tally covered{ground.covered, size.width, size.height};
  const auto claimed = [&territory, faction, size](Cell cell) {
    const std::optional<std::size_t> holder =
        territory.at((static_cast<std::size_t>(cell.y) *
                      static_cast<std::size_t>(size.width)) +
                     static_cast<std::size_t>(cell.x));
    return holder && *holder != faction;
  };
  std::vector<Cell> free;
  for (Cell corner{0, home.y - growth.reach - middle.y};
       corner.y <= home.y + growth.reach - middle.y; ++corner.y) {
    for (corner.x = home.x - growth.reach - middle.x;
         corner.x <= home.x + growth.reach - middle.x; ++corner.x) {
      const Cell centre{corner.x + middle.x, corner.y + middle.y};
      if (!blocked.clear(corner, area) ||
          !covered.clear_around(corner, area, growth.spacing) ||
          paths.distance(centre) == FlowField::UNREACHABLE) {
        continue;
      }
      const std::vector<Cell> cells = cells_of(corner, area);
      if (std::none_of(cells.begin(), cells.end(), claimed)) {
        free.push_back(corner);
      }
    }
  }
  return free;
}

std::vector<Laid> layout_at(Cell corner, const Growth& growth) {
  std::vector<Laid> layout;
  layout.reserve(growth.layout.size());
  for (const Placement& placement : growth.layout) {
    layout.push_back(Laid{placement.plan, Cell{corner.x + placement.offset.x,
                                               corner.y + placement.offset.y}});
  }
  return layout;
}

}  // namespace murim
