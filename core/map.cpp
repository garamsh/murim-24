#include "map.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace murim {
namespace {

// Rounds toward negative infinity, so a position left of the map falls in a
// cell outside it rather than in cell 0.
std::int32_t cells_in(std::int32_t units) {
  return (units / CELL_SIZE) - static_cast<std::int32_t>(units % CELL_SIZE < 0);
}

}  // namespace

std::vector<Cell> cells_of(Cell corner, Footprint footprint) {
  std::vector<Cell> cells;
  for (std::int32_t row = 0; row < footprint.height; ++row) {
    for (std::int32_t column = 0; column < footprint.width; ++column) {
      cells.push_back(Cell{corner.x + column, corner.y + row});
    }
  }
  return cells;
}

Cell cell_of(Position position) {
  return Cell{cells_in(position.x), cells_in(position.y)};
}

Position centre_of(Cell cell) {
  return Position{(cell.x * CELL_SIZE) + (CELL_SIZE / 2),
                  (cell.y * CELL_SIZE) + (CELL_SIZE / 2)};
}

Map::Map(const std::vector<std::string>& rows)
    : width_(rows.empty() ? 0 : static_cast<std::int32_t>(rows.front().size())),
      height_(static_cast<std::int32_t>(rows.size())) {
  const auto ragged = [this](const std::string& row) {
    return static_cast<std::int32_t>(row.size()) != width_;
  };
  if (width_ == 0 || std::any_of(rows.begin(), rows.end(), ragged)) {
    throw std::invalid_argument("map rows must be non-empty and equally long");
  }
  for (const std::string& row : rows) {
    for (const char tile : row) {
      blocked_.push_back(tile == '#');
    }
  }
}

std::int32_t Map::width() const { return width_; }

std::int32_t Map::height() const { return height_; }

bool Map::passable(Cell cell) const {
  return cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_ &&
         !blocked_.at(index(cell));
}

void Map::block(Cell cell) { blocked_.at(index(cell)) = true; }

std::size_t Map::index(Cell cell) const {
  return (static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_)) +
         static_cast<std::size_t>(cell.x);
}

}  // namespace murim
