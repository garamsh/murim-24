#include "spatial_hash.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

// Teschner et al., "Optimized Spatial Hashing for Collision Detection of
// Deformable Objects" (2003): a cell's coordinates times large primes,
// combined with XOR.
constexpr std::uint64_t PRIME_X = 73856093ULL;
constexpr std::uint64_t PRIME_Y = 19349663ULL;

// One slot per entry at least, rounded up to a power of two so a mask picks
// the slot.
std::size_t slot_count_for(std::size_t entries) {
  std::size_t slots = 1;
  while (slots < entries) {
    slots *= 2;
  }
  return slots;
}

}  // namespace

SpatialHash::SpatialHash(std::vector<Position> positions)
    : positions_(std::move(positions)),
      slot_mask_(slot_count_for(positions_.size()) - 1),
      slot_starts_(slot_mask_ + 2, 0),
      entries_(positions_.size()) {
  for (const Position position : positions_) {
    ++slot_starts_.at(slot_of(cell_of(position)) + 1);
  }
  std::partial_sum(slot_starts_.begin(), slot_starts_.end(),
                   slot_starts_.begin());
  std::vector<std::size_t> cursors(slot_starts_.begin(),
                                   slot_starts_.end() - 1);
  for (std::size_t index = 0; index < positions_.size(); ++index) {
    entries_.at(cursors.at(slot_of(cell_of(positions_.at(index))))++) = index;
  }
}

std::vector<std::size_t> SpatialHash::within(Position centre,
                                             std::int32_t radius) const {
  const Cell low = cell_of(Position{centre.x - radius, centre.y - radius});
  const Cell high = cell_of(Position{centre.x + radius, centre.y + radius});
  const std::int64_t radius_squared = std::int64_t{radius} * radius;
  std::vector<std::size_t> found;
  for (Cell cell{low.x, low.y}; cell.y <= high.y; ++cell.y) {
    for (cell.x = low.x; cell.x <= high.x; ++cell.x) {
      const std::size_t slot = slot_of(cell);
      for (std::size_t entry = slot_starts_.at(slot);
           entry < slot_starts_.at(slot + 1); ++entry) {
        const std::size_t other = entries_.at(entry);
        const Position position = positions_.at(other);
        const Cell own = cell_of(position);
        // Cells share slots, so an entry counts only in its own cell; that
        // also bounds the offsets squared below.
        const std::int64_t offset_x = std::int64_t{position.x} - centre.x;
        const std::int64_t offset_y = std::int64_t{position.y} - centre.y;
        if (own.x == cell.x && own.y == cell.y &&
            (offset_x * offset_x) + (offset_y * offset_y) <= radius_squared) {
          found.push_back(other);
        }
      }
    }
  }
  std::sort(found.begin(), found.end());
  return found;
}

Position SpatialHash::position(std::size_t index) const {
  return positions_.at(index);
}

std::size_t SpatialHash::slot_of(Cell cell) const {
  const std::uint64_t key = (static_cast<std::uint64_t>(cell.x) * PRIME_X) ^
                            (static_cast<std::uint64_t>(cell.y) * PRIME_Y);
  return static_cast<std::size_t>(key) & slot_mask_;
}

}  // namespace murim
