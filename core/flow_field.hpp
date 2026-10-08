#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "map.hpp"

namespace murim {

/// A unit step along each axis: -1, 0 or 1.
struct Step {
  std::int32_t x;
  std::int32_t y;
};

/// For every cell of a map, the step and the path length toward one
/// objective along a shortest 8-connected path through passable cells, a
/// diagonal step costing 7/5 of a straight one and taken only where both cells
/// beside it are passable. Of the steps that begin a shortest path, a cell's
/// step is the one nearest the straight line to the objective.
class FlowField {
 public:
  /// Builds the field toward `objective`, which must be passable.
  FlowField(const Map& map, Cell objective);

  /// Returns the step from `cell` toward the objective: none at the objective
  /// or where it cannot be reached. `cell` must be passable.
  [[nodiscard]] Step step(Cell cell) const;

  /// Returns the path length from `cell` to the objective, in cells, a
  /// diagonal step counting 7/5 of one and the total rounded down: 0 at the
  /// objective, UNREACHABLE where it cannot be reached or the cell is not
  /// passable.
  [[nodiscard]] std::uint32_t distance(Cell cell) const;

  static constexpr std::uint32_t UNREACHABLE = UINT32_MAX;

 private:
  [[nodiscard]] Step straightest(const Map& map, Cell cell,
                                 Cell objective) const;
  [[nodiscard]] std::size_t index(Cell cell) const;
  [[nodiscard]] Cell cell_at(std::size_t at) const;

  std::int32_t width_;
  std::int32_t height_;
  std::vector<Step> steps_;
  // Path costs, a straight step 5 and a diagonal one 7.
  std::vector<std::uint32_t> costs_;
};

}  // namespace murim
