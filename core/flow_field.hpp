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
/// objective along a shortest 4-connected path through passable cells.
class FlowField {
 public:
  /// Builds the field toward `objective`, which must be passable.
  FlowField(const Map& map, Cell objective);

  /// Returns the step from `cell` toward the objective: none at the objective
  /// or where it cannot be reached. `cell` must be passable.
  [[nodiscard]] Step step(Cell cell) const;

  /// Returns the path length from `cell` to the objective, in cells: 0 at the
  /// objective, UNREACHABLE where it cannot be reached or the cell is not
  /// passable.
  [[nodiscard]] std::uint32_t distance(Cell cell) const;

  static constexpr std::uint32_t UNREACHABLE = UINT32_MAX;

 private:
  [[nodiscard]] std::size_t index(Cell cell) const;

  std::int32_t width_;
  std::int32_t height_;
  std::vector<Step> steps_;
  std::vector<std::uint32_t> distances_;
};

}  // namespace murim
