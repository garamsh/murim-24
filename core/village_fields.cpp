#include "village_fields.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"

namespace murim {
namespace {

// Diagonals first, as the whole-map field takes them.
constexpr std::array<Step, 8> DIRECTIONS{
    {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

constexpr std::uint32_t STRAIGHT_COST = 5;
constexpr std::uint32_t DIAGONAL_COST = 7;
constexpr std::uint16_t UNREACHED = std::numeric_limits<std::uint16_t>::max();
constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();

std::uint32_t cost_of(Step direction) {
  return direction.x != 0 && direction.y != 0 ? DIAGONAL_COST : STRAIGHT_COST;
}

}  // namespace

VillageFields::VillageFields(const Map& map, Cell corner, Footprint size,
                             std::size_t places)
    : corner_(corner), size_(size), slots_(places, NONE) {
  if (size.width <= 0 || size.height <= 0 || size.width > MAX_SIDE ||
      size.height > MAX_SIDE) {
    throw std::invalid_argument(
        "a village's window must be at least a cell and at most 128 a side");
  }
  const auto walkable = [&](Cell each) {
    return inside(each) && map.passable(each);
  };
  passable_.resize(cells());
  ways_.resize(cells(), 0);
  for (std::size_t slot = 0; slot < cells(); ++slot) {
    const Cell cell = cell_at(slot);
    passable_.at(slot) = map.passable(cell);
    for (std::size_t way = 0; way < DIRECTIONS.size(); ++way) {
      const Step direction = DIRECTIONS.at(way);
      const Cell next{cell.x + direction.x, cell.y + direction.y};
      // A diagonal step passes beside two cells, and both must be passable.
      const bool open =
          walkable(next) &&
          (direction.x == 0 || direction.y == 0 ||
           (walkable(Cell{next.x, cell.y}) && walkable(Cell{cell.x, next.y})));
      if (open) {
        ways_.at(slot) =
            static_cast<std::uint8_t>(ways_.at(slot) | (1U << way));
      }
    }
  }
}

bool VillageFields::holds(std::size_t place) const {
  return slots_.at(place) != NONE;
}

void VillageFields::build(std::size_t place, const std::vector<Cell>& seeds,
                          Cell aim) {
  const std::size_t base = built_ * cells();
  slots_.at(place) = built_;
  ++built_;
  steps_.resize(base + cells(), 0);
  costs_.resize(base + cells(), UNREACHED);
  search(base, seeds);
  choose_steps(base, aim);
}

// Fills the costs of the field at `base` outward from `seeds` by Dijkstra's
// search over the window's passable cells, its frontier kept in a bucket for
// each cost a step can add, since no step costs more than a diagonal.
void VillageFields::search(std::size_t base, const std::vector<Cell>& seeds) {
  constexpr std::size_t BUCKETS = DIAGONAL_COST + 1;
  std::array<std::vector<std::size_t>, BUCKETS> buckets;
  std::size_t pending = 0;
  for (const Cell seed : seeds) {
    if (inside(seed) && passable_.at(index(seed))) {
      costs_.at(base + index(seed)) = 0;
      buckets.at(0).push_back(index(seed));
      ++pending;
    }
  }
  for (std::uint32_t cost = 0; pending > 0; ++cost) {
    // No step adds a multiple of BUCKETS, so nothing joins this bucket while
    // it is read.
    std::vector<std::size_t>& bucket = buckets.at(cost % BUCKETS);
    for (const std::size_t slot : bucket) {
      --pending;
      if (cost != costs_.at(base + slot)) {
        continue;
      }
      for (std::size_t way = 0; way < DIRECTIONS.size(); ++way) {
        if (!open(slot, way)) {
          continue;
        }
        const std::size_t next = beside(slot, DIRECTIONS.at(way));
        const std::uint32_t through = cost + cost_of(DIRECTIONS.at(way));
        if (through < UNREACHED && through < costs_.at(base + next)) {
          costs_.at(base + next) = static_cast<std::uint16_t>(through);
          buckets.at(through % BUCKETS).push_back(next);
          ++pending;
        }
      }
    }
    bucket.clear();
  }
}

// Sets each reached cell's step of the field at `base`: of the steps that
// begin a shortest path, the one nearest the line to `aim`.
void VillageFields::choose_steps(std::size_t base, Cell aim) {
  for (std::size_t slot = 0; slot < cells(); ++slot) {
    const std::uint32_t cost = costs_.at(base + slot);
    if (cost == UNREACHED || cost == 0) {
      continue;
    }
    const Cell cell = cell_at(slot);
    const std::int64_t toward_x = std::int64_t{aim.x} - cell.x;
    const std::int64_t toward_y = std::int64_t{aim.y} - cell.y;
    std::int64_t nearest = -1;
    for (std::size_t way = 0; way < DIRECTIONS.size(); ++way) {
      const Step direction = DIRECTIONS.at(way);
      if (!open(slot, way) ||
          costs_.at(base + beside(slot, direction)) + cost_of(direction) !=
              cost) {
        continue;
      }
      const std::int64_t off =
          std::abs((direction.x * toward_y) - (direction.y * toward_x));
      if (nearest < 0 || off < nearest) {
        steps_.at(base + slot) = static_cast<std::uint8_t>(way + 1);
        nearest = off;
      }
    }
  }
}

// Whether a step from the cell at `slot` along direction `way` stays on
// passable cells of the window.
bool VillageFields::open(std::size_t slot, std::size_t way) const {
  return ((ways_.at(slot) >> way) & 1U) != 0;
}

// The slot of the cell a step of `direction` from the cell at `slot` reaches,
// which open() has found inside the window.
std::size_t VillageFields::beside(std::size_t slot, Step direction) const {
  const std::ptrdiff_t offset =
      (static_cast<std::ptrdiff_t>(direction.y) * size_.width) + direction.x;
  return static_cast<std::size_t>(static_cast<std::ptrdiff_t>(slot) + offset);
}

std::size_t VillageFields::cells() const {
  return static_cast<std::size_t>(size_.width) *
         static_cast<std::size_t>(size_.height);
}

Cell VillageFields::cell_at(std::size_t slot) const {
  const auto width = static_cast<std::size_t>(size_.width);
  return Cell{corner_.x + static_cast<std::int32_t>(slot % width),
              corner_.y + static_cast<std::int32_t>(slot / width)};
}

bool VillageFields::reaches(std::size_t place, Cell cell) const {
  const std::size_t slot = slot_of(place);
  return inside(cell) && costs_.at((slot * cells()) + index(cell)) != UNREACHED;
}

Step VillageFields::step(std::size_t place, Cell cell) const {
  const std::size_t slot = slot_of(place);
  if (!inside(cell)) {
    return Step{0, 0};
  }
  const std::uint8_t way = steps_.at((slot * cells()) + index(cell));
  return way == 0 ? Step{0, 0} : DIRECTIONS.at(way - 1U);
}

std::size_t VillageFields::built() const { return built_; }

bool VillageFields::overlaps(Cell corner, Footprint size) const {
  return corner.x < corner_.x + size_.width &&
         corner_.x < corner.x + size.width &&
         corner.y < corner_.y + size_.height &&
         corner_.y < corner.y + size.height;
}

std::size_t VillageFields::bytes() const {
  return (steps_.size() * sizeof(std::uint8_t)) +
         (costs_.size() * sizeof(std::uint16_t));
}

bool VillageFields::inside(Cell cell) const {
  return cell.x >= corner_.x && cell.y >= corner_.y &&
         cell.x < corner_.x + size_.width && cell.y < corner_.y + size_.height;
}

std::size_t VillageFields::index(Cell cell) const {
  return (static_cast<std::size_t>(cell.y - corner_.y) *
          static_cast<std::size_t>(size_.width)) +
         static_cast<std::size_t>(cell.x - corner_.x);
}

// Throws std::out_of_range when place `place` is not built or not a place.
std::size_t VillageFields::slot_of(std::size_t place) const {
  const std::size_t slot = slots_.at(place);
  if (slot == NONE) {
    throw std::out_of_range("a place's field must be built before it is read");
  }
  return slot;
}

}  // namespace murim
