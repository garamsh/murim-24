#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace murim {

/// Sub-cell units along one side of a cell: positions are fixed-point at this
/// scale.
constexpr std::int32_t CELL_SIZE = 256;

/// A tile of the map, in whole cells.
struct Cell {
  std::int32_t x;
  std::int32_t y;
};

/// A point on the map, in sub-cell units.
struct Position {
  std::int32_t x;
  std::int32_t y;
};

/// Returns the cell holding a position.
[[nodiscard]] Cell cell_of(Position position);

/// Returns the position at the centre of a cell.
[[nodiscard]] Position centre_of(Cell cell);

/// A rectangular grid of cells, each passable or blocked.
class Map {
 public:
  /// Parses rows of equal, non-zero length, where `#` is a blocked cell and
  /// any other character a passable one. Throws std::invalid_argument
  /// otherwise.
  explicit Map(const std::vector<std::string>& rows);

  [[nodiscard]] std::int32_t width() const;
  [[nodiscard]] std::int32_t height() const;

  /// Returns whether the cell lies inside the map and is not blocked.
  [[nodiscard]] bool passable(Cell cell) const;

 private:
  [[nodiscard]] std::size_t index(Cell cell) const;

  std::int32_t width_;
  std::int32_t height_;
  std::vector<bool> blocked_;
};

}  // namespace murim
