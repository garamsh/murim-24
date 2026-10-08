#include "flow_field.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

// Diagonals first, so a step as near the line as a straight one is taken
// diagonally, which keeps a walk off every diagonal nearer its line.
constexpr std::array<Step, 8> DIRECTIONS{
    {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

// The cost of a step along an axis and across a corner: 7/5 is within 1% of
// the square root of two.
constexpr std::uint32_t STRAIGHT_COST = 5;
constexpr std::uint32_t DIAGONAL_COST = 7;

std::uint32_t cost_of(Step direction) {
  return direction.x != 0 && direction.y != 0 ? DIAGONAL_COST : STRAIGHT_COST;
}

// Whether a step from `cell` along `direction` stays on passable cells: a
// diagonal step passes beside two cells, and both must be passable.
bool open(const Map& map, Cell cell, Step direction) {
  const Cell next{cell.x + direction.x, cell.y + direction.y};
  return map.passable(next) && (direction.x == 0 || direction.y == 0 ||
                                (map.passable(Cell{next.x, cell.y}) &&
                                 map.passable(Cell{cell.x, next.y})));
}

}  // namespace

FlowField::FlowField(const Map& map, Cell objective)
    : width_(map.width()),
      height_(map.height()),
      steps_(
          static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_),
          Step{0, 0}),
      costs_(steps_.size(), UNREACHABLE) {
  using Entry = std::pair<std::uint32_t, std::size_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  costs_.at(index(objective)) = 0;
  frontier.emplace(0, index(objective));
  while (!frontier.empty()) {
    const auto [cost, slot] = frontier.top();
    frontier.pop();
    if (cost != costs_.at(slot)) {
      continue;
    }
    const Cell cell = cell_at(slot);
    for (const Step direction : DIRECTIONS) {
      const Cell next{cell.x + direction.x, cell.y + direction.y};
      // Every step is open both ways, so a path from `next` may take it back.
      if (!open(map, cell, direction)) {
        continue;
      }
      const std::uint32_t through = cost + cost_of(direction);
      if (through < costs_.at(index(next))) {
        costs_.at(index(next)) = through;
        frontier.emplace(through, index(next));
      }
    }
  }
  for (std::size_t slot = 0; slot < steps_.size(); ++slot) {
    if (costs_.at(slot) != UNREACHABLE && costs_.at(slot) != 0) {
      steps_.at(slot) = straightest(map, cell_at(slot), objective);
    }
  }
}

Step FlowField::step(Cell cell) const { return steps_.at(index(cell)); }

std::uint32_t FlowField::distance(Cell cell) const {
  const bool inside =
      cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_;
  if (!inside || costs_.at(index(cell)) == UNREACHABLE) {
    return UNREACHABLE;
  }
  return costs_.at(index(cell)) / STRAIGHT_COST;
}

// Of the steps from `cell` that begin a shortest path, the one whose cell
// lies nearest the line from `cell` to `objective`, the first in DIRECTIONS
// among equals.
Step FlowField::straightest(const Map& map, Cell cell, Cell objective) const {
  const std::int64_t toward_x = std::int64_t{objective.x} - cell.x;
  const std::int64_t toward_y = std::int64_t{objective.y} - cell.y;
  Step best{0, 0};
  std::int64_t nearest = -1;
  for (const Step direction : DIRECTIONS) {
    if (!open(map, cell, direction)) {
      continue;
    }
    const Cell next{cell.x + direction.x, cell.y + direction.y};
    if (costs_.at(index(next)) + cost_of(direction) != costs_.at(index(cell))) {
      continue;
    }
    const std::int64_t off =
        std::abs((direction.x * toward_y) - (direction.y * toward_x));
    if (nearest < 0 || off < nearest) {
      best = direction;
      nearest = off;
    }
  }
  return best;
}

std::size_t FlowField::index(Cell cell) const {
  return (static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_)) +
         static_cast<std::size_t>(cell.x);
}

Cell FlowField::cell_at(std::size_t slot) const {
  const auto width = static_cast<std::size_t>(width_);
  return Cell{static_cast<std::int32_t>(slot % width),
              static_cast<std::int32_t>(slot / width)};
}

}  // namespace murim
