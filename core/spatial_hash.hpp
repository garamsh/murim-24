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

  /// Calls `visit` with the index of every position within `radius` sub-cell
  /// units of `centre`, Euclidean distance inclusive, once each and in no
  /// set order; it allocates nothing.
  template <typename Visit>
  void visit_within(Position centre, std::int32_t radius, Visit visit) const {
    const auto [low, high] = walk(centre, radius);
    const std::int64_t radius_squared = std::int64_t{radius} * radius;
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
            visit(other);
          }
        }
      }
    }
  }

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
