#include "flow_field.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

#include "map.hpp"

namespace murim {
namespace {

constexpr std::array<Step, 4> DIRECTIONS{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

}  // namespace

FlowField::FlowField(const Map& map, Cell objective)
    : width_(map.width()),
      height_(map.height()),
      steps_(
          static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_),
          Step{0, 0}),
      distances_(steps_.size(), UNREACHABLE) {
  distances_.at(index(objective)) = 0;
  std::deque<Cell> frontier{objective};
  while (!frontier.empty()) {
    const Cell cell = frontier.front();
    frontier.pop_front();
    for (const Step direction : DIRECTIONS) {
      const Cell next{cell.x + direction.x, cell.y + direction.y};
      if (distance(next) == UNREACHABLE && map.passable(next)) {
        distances_.at(index(next)) = distances_.at(index(cell)) + 1;
        steps_.at(index(next)) = Step{-direction.x, -direction.y};
        frontier.push_back(next);
      }
    }
  }
}

Step FlowField::step(Cell cell) const { return steps_.at(index(cell)); }

std::uint32_t FlowField::distance(Cell cell) const {
  const bool inside =
      cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_;
  return inside ? distances_.at(index(cell)) : UNREACHABLE;
}

std::size_t FlowField::index(Cell cell) const {
  return (static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_)) +
         static_cast<std::size_t>(cell.x);
}

}  // namespace murim
