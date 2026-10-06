#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {

/// An index over positions, bucketed by the cell holding each, that answers a
/// query by visiting only the cells its radius overlaps among those between
/// the indexed positions, so no radius walks more cells than they span.
class SpatialHash {
 public:
  explicit SpatialHash(std::vector<Position> positions);

  /// Returns the indices of the positions within `radius` sub-cell units of
  /// `centre`, Euclidean distance inclusive, in ascending order.
  [[nodiscard]] std::vector<std::size_t> within(Position centre,
                                                std::int32_t radius) const;

  /// Returns the first and last cell, corner to corner, that a query of
  /// `radius` around `centre` visits; the first lies past the last when it
  /// visits none.
  [[nodiscard]] std::pair<Cell, Cell> walk(Position centre,
                                           std::int32_t radius) const;

  /// Returns the indexed position `index`.
  [[nodiscard]] Position position(std::size_t index) const;

 private:
  [[nodiscard]] std::size_t slot_of(Cell cell) const;

  std::vector<Position> positions_;
  Cell first_;
  Cell last_;
  std::size_t slot_mask_;
  std::vector<std::size_t> slot_starts_;
  std::vector<std::size_t> entries_;
};

}  // namespace murim
