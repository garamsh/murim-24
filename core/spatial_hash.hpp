#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "map.hpp"

namespace murim {

/// An index over positions, bucketed by the cell holding each, that finds a
/// position's neighbours by visiting only the cells its radius overlaps.
class SpatialHash {
 public:
  /// Indexes `positions` for queries within `radius` sub-cell units,
  /// Euclidean distance inclusive.
  SpatialHash(std::vector<Position> positions, std::int32_t radius);

  /// Returns the indices of the other positions within the radius of
  /// position `index`, in ascending order.
  [[nodiscard]] std::vector<std::size_t> neighbours(std::size_t index) const;

  /// Returns the indexed position `index`.
  [[nodiscard]] Position position(std::size_t index) const;

 private:
  [[nodiscard]] std::size_t slot_of(Cell cell) const;

  std::vector<Position> positions_;
  std::int32_t radius_;
  std::size_t slot_mask_;
  std::vector<std::size_t> slot_starts_;
  std::vector<std::size_t> entries_;
};

}  // namespace murim
