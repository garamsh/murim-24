#include "flow_field.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
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

}  // namespace

FlowField::FlowField(const Map& map, Cell objective) {
  rebuild(map, Cell{0, 0}, Footprint{map.width(), map.height()},
          {FieldSeed{objective, 0}}, objective);
}

FlowField::FlowField(const Map& map, Cell corner, Footprint size,
                     const std::vector<FieldSeed>& seeds, Cell aim,
                     Frontier frontier) {
  rebuild(map, corner, size, seeds, aim, frontier);
}

void FlowField::rebuild(const Map& map, Cell corner, Footprint size,
                        const std::vector<FieldSeed>& seeds, Cell aim,
                        Frontier frontier) {
  corner_ = corner;
  width_ = size.width;
  height_ = size.height;
  const std::size_t cells =
      static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
  steps_.assign(cells, Step{0, 0});
  costs_.assign(cells, UNREACHABLE);
  // Every cell a step from within the window reads, the two beside a
  // diagonal step included, lies within the window, so it is read once.
  passable_.assign(cells, false);
  for (std::size_t slot = 0; slot < cells; ++slot) {
    passable_.at(slot) = map.passable(cell_at(slot));
  }
  for (const FieldSeed& seed : seeds) {
    // A seed may be blocked, as an objective on a structure is: the walk
    // ends beside it.
    if (inside(seed.cell) && seed.cost < costs_.at(index(seed.cell))) {
      costs_.at(index(seed.cell)) = seed.cost;
    }
  }
  if (frontier == Frontier::BUCKETS) {
    settle_by_buckets(seeds);
  } else {
    settle_by_heap(seeds);
  }
  seeded_.assign(cells, false);
  for (const FieldSeed& seed : seeds) {
    if (inside(seed.cell)) {
      seeded_.at(index(seed.cell)) = costs_.at(index(seed.cell)) == seed.cost;
    }
  }
  for (std::size_t slot = 0; slot < steps_.size(); ++slot) {
    if (costs_.at(slot) != UNREACHABLE && !seeded_.at(slot)) {
      steps_.at(slot) = straightest(cell_at(slot), aim);
    }
  }
}

template <typename Reached>
void FlowField::relax(std::size_t slot, Reached reached) {
  const std::uint32_t cost = costs_.at(slot);
  const Cell cell = cell_at(slot);
  for (const Step direction : DIRECTIONS) {
    const Cell next{cell.x + direction.x, cell.y + direction.y};
    // Every step is open both ways, so a path from `next` may take it back.
    if (!inside(next) || !opens(cell, direction)) {
      continue;
    }
    const std::uint32_t through = cost + cost_of(direction);
    if (through < costs_.at(index(next))) {
      costs_.at(index(next)) = through;
      reached(through, index(next));
    }
  }
}

// Cells are settled in order of cost. A step raises a cost by at most
// DIAGONAL_COST, so every cell waiting has a cost within RING of the one
// being settled, and each lies in the bucket of its cost modulo RING. A seed
// joins the ring when its cost comes within reach, and the ring skips ahead
// to the next seed when it is empty.
void FlowField::settle_by_buckets(const std::vector<FieldSeed>& seeds) {
  sorted_seeds_.clear();
  for (const FieldSeed& seed : seeds) {
    if (inside(seed.cell) && costs_.at(index(seed.cell)) == seed.cost) {
      sorted_seeds_.emplace_back(seed.cost, index(seed.cell));
    }
  }
  std::sort(sorted_seeds_.begin(), sorted_seeds_.end());
  for (std::vector<std::size_t>& bucket : buckets_) {
    bucket.clear();
  }
  std::size_t waiting = 0;
  std::size_t next_seed = 0;
  const auto file = [this, &waiting](std::uint32_t cost, std::size_t slot) {
    buckets_.at(cost % RING).push_back(slot);
    ++waiting;
  };
  std::uint32_t cost = 0;
  while (waiting > 0 || next_seed < sorted_seeds_.size()) {
    if (waiting == 0) {
      cost = sorted_seeds_.at(next_seed).first;
    }
    while (next_seed < sorted_seeds_.size() &&
           sorted_seeds_.at(next_seed).first == cost) {
      file(cost, sorted_seeds_.at(next_seed).second);
      ++next_seed;
    }
    // A step costs at least STRAIGHT_COST, so settling this bucket files
    // nothing back into it.
    std::vector<std::size_t>& bucket = buckets_.at(cost % RING);
    waiting -= bucket.size();
    for (const std::size_t slot : bucket) {
      if (costs_.at(slot) == cost) {
        relax(slot, file);
      }
    }
    bucket.clear();
    ++cost;
  }
}

// A heap ordered as std::priority_queue orders one with std::greater.
void FlowField::settle_by_heap(const std::vector<FieldSeed>& seeds) {
  frontier_.clear();
  const auto push = [this](std::uint32_t cost, std::size_t slot) {
    frontier_.emplace_back(cost, slot);
    std::push_heap(frontier_.begin(), frontier_.end(), std::greater<>{});
  };
  for (const FieldSeed& seed : seeds) {
    if (inside(seed.cell) && costs_.at(index(seed.cell)) == seed.cost) {
      push(seed.cost, index(seed.cell));
    }
  }
  while (!frontier_.empty()) {
    std::pop_heap(frontier_.begin(), frontier_.end(), std::greater<>{});
    const auto [cost, slot] = frontier_.back();
    frontier_.pop_back();
    if (cost == costs_.at(slot)) {
      relax(slot, push);
    }
  }
}

// Whether a step from `cell`, within the window, along `direction` onto a
// cell within it stays on passable cells: a diagonal step passes beside two
// cells, and both must be passable.
bool FlowField::opens(Cell cell, Step direction) const {
  const Cell next{cell.x + direction.x, cell.y + direction.y};
  return passable_.at(index(next)) &&
         (direction.x == 0 || direction.y == 0 ||
          (passable_.at(index(Cell{next.x, cell.y})) &&
           passable_.at(index(Cell{cell.x, next.y}))));
}

Step FlowField::step(Cell cell) const {
  return inside(cell) ? steps_.at(index(cell)) : Step{0, 0};
}

std::uint32_t FlowField::distance(Cell cell) const {
  if (!inside(cell) || costs_.at(index(cell)) == UNREACHABLE) {
    return UNREACHABLE;
  }
  return costs_.at(index(cell)) / STRAIGHT_COST;
}

// Of the steps from `cell` that begin a shortest path, the one whose cell
// lies nearest the line from `cell` to `objective`, the first in DIRECTIONS
// among equals.
Step FlowField::straightest(Cell cell, Cell objective) const {
  const std::int64_t toward_x = std::int64_t{objective.x} - cell.x;
  const std::int64_t toward_y = std::int64_t{objective.y} - cell.y;
  Step best{0, 0};
  std::int64_t nearest = -1;
  for (const Step direction : DIRECTIONS) {
    const Cell next{cell.x + direction.x, cell.y + direction.y};
    if (!inside(next) || !opens(cell, direction) ||
        costs_.at(index(next)) == UNREACHABLE) {
      continue;
    }
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

bool FlowField::inside(Cell cell) const {
  return cell.x >= corner_.x && cell.y >= corner_.y &&
         cell.x < corner_.x + width_ && cell.y < corner_.y + height_;
}

std::size_t FlowField::index(Cell cell) const {
  return (static_cast<std::size_t>(cell.y - corner_.y) *
          static_cast<std::size_t>(width_)) +
         static_cast<std::size_t>(cell.x - corner_.x);
}

Cell FlowField::cell_at(std::size_t slot) const {
  const auto width = static_cast<std::size_t>(width_);
  return Cell{corner_.x + static_cast<std::int32_t>(slot % width),
              corner_.y + static_cast<std::int32_t>(slot / width)};
}

}  // namespace murim
