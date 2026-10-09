#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {

/// An index over positions, bucketed by the cell holding each, that answers a
/// query by visiting only the cells its radius overlaps among those between
/// the indexed positions, so no radius walks more cells than they span. Each
/// position is of kinds, bits of a mask, and the index notes the kinds each
/// cell and each block of cells holds, so a search for some kinds passes
/// over the places holding none.
class SpatialHash {
 public:
  /// Indexes `positions`, every one of every kind.
  explicit SpatialHash(const std::vector<Position>& positions);

  /// Indexes `positions`, each of the kinds of the same index in `kinds`.
  SpatialHash(std::vector<Position> positions,
              std::vector<std::uint64_t> kinds);

  /// Calls `visit` with the index of every position within `radius` sub-cell
  /// units of `centre`, Euclidean distance inclusive, once each and in no
  /// set order; it allocates nothing.
  template <typename Visit>
  void visit_within(Position centre, std::int32_t radius, Visit visit) const {
    const auto [low, high] = walk(centre, radius);
    const std::int64_t radius_squared = std::int64_t{radius} * radius;
    for (Cell cell{low.x, low.y}; cell.y <= high.y; ++cell.y) {
      for (cell.x = low.x; cell.x <= high.x; ++cell.x) {
        visit_cell(cell, [&](std::size_t other, Position position) {
          if (distance_squared(centre, position) <= radius_squared) {
            visit(other);
          }
        });
      }
    }
  }

  /// Returns the index of the position of any of `kinds` nearest `centre`
  /// within `radius` sub-cell units, Euclidean distance inclusive, among
  /// those `accept` accepts, the lowest index among equals. It walks
  /// the cells in rings outward from the centre's, passing over those that
  /// hold none of `kinds`, stops at the first ring no nearer position can lie
  /// in, and asks `accept` only of a position nearer than the best so far;
  /// it allocates nothing.
  template <typename Accept>
  [[nodiscard]] std::optional<std::size_t> nearest(std::uint64_t kinds,
                                                   Position centre,
                                                   std::int32_t radius,
                                                   Accept accept) const {
    const std::pair<Cell, Cell> span = walk(centre, radius);
    const auto [low, high] = span;
    if (!holds(span, kinds)) {
      return std::nullopt;
    }
    const Cell middle = cell_of(centre);
    const std::int32_t rings = std::max({middle.x - low.x, high.x - middle.x,
                                         middle.y - low.y, high.y - middle.y});
    std::optional<std::size_t> best;
    std::int64_t best_distance = std::int64_t{radius} * radius;
    const auto offer = [&](std::size_t other, Position position) {
      const std::int64_t distance = distance_squared(centre, position);
      if ((kinds_.at(other) & kinds) != 0 &&
          (distance < best_distance ||
           (distance == best_distance && (!best || other < *best))) &&
          accept(other)) {
        best = other;
        best_distance = distance;
      }
    };
    for (std::int32_t ring = 0; ring <= rings; ++ring) {
      // A cell `ring` cells out lies more than `ring - 1` cells away.
      const std::int64_t gap = std::int64_t{ring - 1} * CELL_SIZE;
      if (best && ring > 0 && gap * gap > best_distance) {
        break;
      }
      for (std::int32_t row = std::max(low.y, middle.y - ring);
           row <= std::min(high.y, middle.y + ring); ++row) {
        // The ring's top and bottom rows whole, and its sides' two cells.
        const bool edge = row == middle.y - ring || row == middle.y + ring;
        const std::int32_t step = edge ? 1 : std::max(1, 2 * ring);
        for (std::int32_t column = middle.x - ring; column <= middle.x + ring;
             column += step) {
          const Cell cell{column, row};
          if (column >= low.x && column <= high.x &&
              (slot_kinds_.at(slot_of(cell)) & kinds) != 0) {
            visit_cell(cell, offer);
          }
        }
      }
    }
    return best;
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

  // Whether any block of cells overlapping `span`, its first and last cell
  // corner to corner, may hold a position of any of `kinds`.
  [[nodiscard]] bool holds(std::pair<Cell, Cell> span,
                           std::uint64_t kinds) const;

  [[nodiscard]] static std::int64_t distance_squared(Position origin,
                                                     Position point) {
    const std::int64_t offset_x = std::int64_t{point.x} - origin.x;
    const std::int64_t offset_y = std::int64_t{point.y} - origin.y;
    return (offset_x * offset_x) + (offset_y * offset_y);
  }

  // Calls `visit` with the index and position of every position in `cell`;
  // cells share slots, so an entry counts only in its own cell.
  template <typename Visit>
  void visit_cell(Cell cell, Visit visit) const {
    const std::size_t slot = slot_of(cell);
    for (std::size_t entry = slot_starts_.at(slot);
         entry < slot_starts_.at(slot + 1); ++entry) {
      const std::size_t other = entries_.at(entry);
      const Position position = positions_.at(other);
      const Cell own = cell_of(position);
      if (own.x == cell.x && own.y == cell.y) {
        visit(other, position);
      }
    }
  }

  std::vector<Position> positions_;
  std::vector<std::uint64_t> kinds_;
  Cell first_;
  Cell last_;
  std::size_t slot_mask_;
  std::vector<std::size_t> slot_starts_;
  std::vector<std::size_t> entries_;
  // The kinds held in each slot of cells and each slot of blocks.
  std::vector<std::uint64_t> slot_kinds_;
  std::vector<std::uint64_t> block_kinds_;
};

}  // namespace murim
