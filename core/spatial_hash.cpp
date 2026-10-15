#include "spatial_hash.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

// The cells along each side of a block.
constexpr std::int32_t BLOCK_SIDE = 8;

// The block of cells holding `cell`.
Cell block_of(Cell cell) {
  const auto floor_div = [](std::int32_t value) {
    return (value >= 0 ? value : value - (BLOCK_SIDE - 1)) / BLOCK_SIDE;
  };
  return Cell{floor_div(cell.x), floor_div(cell.y)};
}

// Cell slots per entry: with few, most empty cells share a slot with an
// occupied one, and a search walking them reads that slot's kinds and entries.
constexpr std::size_t SLOTS_PER_ENTRY = 8;

// Block slots per entry: no more blocks hold an entry than there are entries,
// so two slots an entry, a quarter of the cells' table, leave at least half the
// block slots empty.
constexpr std::size_t BLOCK_SLOTS_PER_ENTRY = 2;

// `per_entry` slots per entry at least, rounded up to a power of two so a mask
// picks the slot.
std::size_t slot_count_for(std::size_t entries, std::size_t per_entry) {
  std::size_t slots = 1;
  while (slots < entries * per_entry) {
    slots *= 2;
  }
  return slots;
}

// The slot of a table of `mask` + 1 slots that `cell` hashes to.
std::size_t hashed(Cell cell, std::size_t mask) {
  const std::uint64_t key = (static_cast<std::uint64_t>(cell.x) * PRIME_X) ^
                            (static_cast<std::uint64_t>(cell.y) * PRIME_Y);
  return static_cast<std::size_t>(key) & mask;
}

// `positions`, each of every kind.
std::vector<SpatialHash::Placed> of_every_kind(
    const std::vector<Position>& positions) {
  std::vector<SpatialHash::Placed> placed;
  placed.reserve(positions.size());
  for (const Position position : positions) {
    placed.push_back(SpatialHash::Placed{
        position, std::numeric_limits<std::uint64_t>::max()});
  }
  return placed;
}

}  // namespace

SpatialHash::SpatialHash(const std::vector<Position>& positions)
    : SpatialHash(of_every_kind(positions)) {}

SpatialHash::SpatialHash(std::vector<Placed> placed)
    : placed_(std::move(placed)),
      first_{std::numeric_limits<std::int32_t>::max(),
             std::numeric_limits<std::int32_t>::max()},
      last_{std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::min()},
      slot_mask_(slot_count_for(placed_.size(), SLOTS_PER_ENTRY) - 1),
      block_mask_(slot_count_for(placed_.size(), BLOCK_SLOTS_PER_ENTRY) - 1),
      slot_starts_(slot_mask_ + 2, 0),
      entries_(placed_.size()),
      slot_kinds_(slot_mask_ + 1, 0),
      block_kinds_(block_mask_ + 1, 0) {
  for (const Placed& each : placed_) {
    const Cell cell = cell_of(each.position);
    first_ = Cell{std::min(first_.x, cell.x), std::min(first_.y, cell.y)};
    last_ = Cell{std::max(last_.x, cell.x), std::max(last_.y, cell.y)};
    ++slot_starts_.at(slot_of(cell) + 1);
    slot_kinds_.at(slot_of(cell)) |= each.kinds;
    block_kinds_.at(hashed(block_of(cell), block_mask_)) |= each.kinds;
  }
  std::partial_sum(slot_starts_.begin(), slot_starts_.end(),
                   slot_starts_.begin());
  std::vector<std::size_t> cursors(slot_starts_.begin(),
                                   slot_starts_.end() - 1);
  for (std::size_t index = 0; index < placed_.size(); ++index) {
    const Cell cell = cell_of(placed_.at(index).position);
    entries_.at(cursors.at(slot_of(cell))++) = index;
  }
}

std::pair<Cell, Cell> SpatialHash::walk(Position centre,
                                        std::int32_t radius) const {
  const Cell low = cell_of(Position{centre.x - radius, centre.y - radius});
  const Cell high = cell_of(Position{centre.x + radius, centre.y + radius});
  return {Cell{std::max(low.x, first_.x), std::max(low.y, first_.y)},
          Cell{std::min(high.x, last_.x), std::min(high.y, last_.y)}};
}

Position SpatialHash::position(std::size_t index) const {
  return placed_.at(index).position;
}

bool SpatialHash::holds(std::pair<Cell, Cell> span, std::uint64_t kinds) const {
  const Cell first = block_of(span.first);
  const Cell last = block_of(span.second);
  for (Cell block{first.x, first.y}; block.y <= last.y; ++block.y) {
    for (block.x = first.x; block.x <= last.x; ++block.x) {
      if ((block_kinds_.at(hashed(block, block_mask_)) & kinds) != 0) {
        return true;
      }
    }
  }
  return false;
}

std::size_t SpatialHash::slot_of(Cell cell) const {
  return hashed(cell, slot_mask_);
}

}  // namespace murim
