#include "areas.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

// The areas along a side of `cells` cells, the last one partly off the map.
std::int32_t areas_along(std::int32_t cells) {
  return (cells + AREA_SIDE - 1) / AREA_SIDE;
}

}  // namespace

Areas::Areas(Footprint size)
    : columns_(areas_along(size.width)),
      rows_(areas_along(size.height)),
      tiers_(
          static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_),
          AreaTier::LIVE) {}

Areas::Areas(std::int32_t columns, std::vector<AreaTier> tiers)
    : columns_(columns),
      rows_(columns > 0 ? static_cast<std::int32_t>(
                              tiers.size() / static_cast<std::size_t>(columns))
                        : 0),
      tiers_(std::move(tiers)) {}

std::int32_t Areas::columns() const { return columns_; }

std::size_t Areas::count() const { return tiers_.size(); }

const std::vector<AreaTier>& Areas::tiers() const { return tiers_; }

std::size_t Areas::area_of(Cell cell) const {
  return (static_cast<std::size_t>(cell.y / AREA_SIDE) *
          static_cast<std::size_t>(columns_)) +
         static_cast<std::size_t>(cell.x / AREA_SIDE);
}

AreaTier Areas::tier(std::size_t area) const { return tiers_.at(area); }

void Areas::make_live() {
  std::fill(tiers_.begin(), tiers_.end(), AreaTier::LIVE);
}

void Areas::recompute(const Census& census, const Profile& profile) {
  const std::vector<std::size_t>& focus = census.focus;
  const std::vector<std::size_t>& people = census.people;
  if (focus.empty()) {
    make_live();
    return;
  }
  const auto column_of = [this](std::size_t area) {
    return static_cast<std::int32_t>(area % static_cast<std::size_t>(columns_));
  };
  const auto row_of = [this](std::size_t area) {
    return static_cast<std::int32_t>(area / static_cast<std::size_t>(columns_));
  };
  std::vector<std::int32_t> distance(tiers_.size(),
                                     std::numeric_limits<std::int32_t>::max());
  for (std::size_t area = 0; area < tiers_.size(); ++area) {
    for (const std::size_t held : focus) {
      distance.at(area) =
          std::min(distance.at(area),
                   std::max(std::abs(column_of(area) - column_of(held)),
                            std::abs(row_of(area) - row_of(held))));
    }
  }
  // The live radius the cap allows: each ring nearest first while the
  // people within it stay within the cap, the focus ring always.
  std::int32_t live = 0;
  for (std::int32_t ring = 1; ring <= profile.live_radius; ++ring) {
    std::size_t within = 0;
    for (std::size_t area = 0; area < tiers_.size(); ++area) {
      within += distance.at(area) <= ring ? people.at(area) : 0;
    }
    if (within > profile.live_cap) {
      break;
    }
    live = ring;
  }
  const auto radius_of = [live, &profile](AreaTier tier) {
    return tier == AreaTier::LIVE ? live : profile.stepped_radius;
  };
  for (std::size_t area = 0; area < tiers_.size(); ++area) {
    const std::int32_t away = distance.at(area);
    AreaTier made = AreaTier::FAR;
    if (away <= live) {
      made = AreaTier::LIVE;
    } else if (away <= profile.stepped_radius) {
      made = AreaTier::STEPPED;
    }
    const AreaTier held = tiers_.at(area);
    const bool kept =
        made > held && held != AreaTier::FAR && away <= radius_of(held) + 1;
    tiers_.at(area) = kept ? held : made;
  }
}

}  // namespace murim
