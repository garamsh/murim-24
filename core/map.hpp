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

/// A rectangle of cells, `width` columns by `height` rows.
struct Footprint {
  std::int32_t width;
  std::int32_t height;
};

/// Returns the cells of `footprint` from `corner`, its top-left cell, row by
/// row.
[[nodiscard]] std::vector<Cell> cells_of(Cell corner, Footprint footprint);

/// Returns the cell holding a position.
[[nodiscard]] Cell cell_of(Position position);

/// Returns the position at the centre of a cell.
[[nodiscard]] Position centre_of(Cell cell);

/// What a cell bears: buildings and bodies, bodies only, or neither.
enum class Footing : std::uint8_t { BUILDABLE, ROUGH, BLOCKED };

/// Returns the character a map row marks a cell of `footing` with: `.`, `^`
/// or `#`.
[[nodiscard]] char tile_of(Footing footing);

/// A rectangular grid of cells, each buildable, rough or blocked.
class Map {
 public:
  /// Parses rows of equal, non-zero length, where `#` is a blocked cell, `^`
  /// a rough one and any other character a buildable one. Throws
  /// std::invalid_argument otherwise.
  explicit Map(const std::vector<std::string>& rows);

  [[nodiscard]] std::int32_t width() const;
  [[nodiscard]] std::int32_t height() const;

  /// Returns whether the cell lies inside the map and is not blocked.
  [[nodiscard]] bool passable(Cell cell) const;

  /// Returns whether the cell is passable and not rough, so something may be
  /// built on it.
  [[nodiscard]] bool buildable(Cell cell) const;

  /// Blocks a cell that lies inside the map.
  void block(Cell cell);

 private:
  [[nodiscard]] std::size_t index(Cell cell) const;

  std::int32_t width_;
  std::int32_t height_;
  std::vector<bool> blocked_;
  std::vector<bool> rough_;
};

}  // namespace murim
