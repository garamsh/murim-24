#include "compiled_map.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "layout.hpp"
#include "map.hpp"

namespace murim {
namespace {

// SplitMix64's constants (Steele, Lea and Flood, 2014).
constexpr std::uint64_t GOLDEN = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t MIX_A = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t MIX_B = 0x94D049BB133111EBULL;
constexpr int SHIFT_A = 30;
constexpr int SHIFT_B = 27;
constexpr int SHIFT_C = 31;

std::uint64_t mix(std::uint64_t value) {
  value += GOLDEN;
  value = (value ^ (value >> SHIFT_A)) * MIX_A;
  value = (value ^ (value >> SHIFT_B)) * MIX_B;
  return value ^ (value >> SHIFT_C);
}

constexpr std::int32_t SMALL_BLOCK = 8;
constexpr std::int32_t LARGE_BLOCK = 16;

constexpr unsigned LEVEL_SHIFT = 4;
constexpr std::uint8_t FOOTING_MASK = 0x0F;

// A word holds eight cells, the first in its lowest byte.
constexpr std::size_t CELLS_A_WORD = 8;
constexpr std::size_t BYTE_BITS = 8;
// A byte repeated in every byte of a word, multiplied by this.
constexpr std::uint64_t EVERY_BYTE = 0x0101010101010101ULL;

// The cost of a step along an axis and across a corner, as FlowField counts
// them.
constexpr std::uint32_t STRAIGHT_COST = 5;
constexpr std::uint32_t DIAGONAL_COST = 7;

// A route may be at most a fifth longer than designed: 6/5 of its length.
constexpr std::int64_t SLACK_OVER = 6;
constexpr std::int64_t SLACK_UNDER = 5;

constexpr std::array<std::array<std::int32_t, 2>, 8> DIRECTIONS{
    {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

constexpr std::uint32_t UNREACHED = UINT32_MAX;

// Whether a walker on `from` may step onto `onto`: it is passable and at
// most one level away.
bool steps(const CompiledMap& map, Cell from, Cell onto) {
  return map.contains(onto) && map.footing(onto) != Footing::BLOCKED &&
         std::abs(int{map.level(from)} - int{map.level(onto)}) <= 1;
}

// The least a walk from `from` to `goal` can cost, every step open: a
// diagonal step for each cell both axes still differ by, a straight one for
// the rest. It never overestimates and never falls by more than a step's
// cost over a step, so a search ordered by it is A* with a consistent
// heuristic.
std::uint32_t least_cost(Cell from, Cell goal) {
  const auto across = static_cast<std::uint32_t>(std::abs(goal.x - from.x));
  const auto down = static_cast<std::uint32_t>(std::abs(goal.y - from.y));
  const std::uint32_t diagonal = std::min(across, down);
  return (diagonal * DIAGONAL_COST) +
         ((std::max(across, down) - diagonal) * STRAIGHT_COST);
}

// The shortest walk from `from` to `goal` within `limit` cells, by the rule
// validate_map states, searched over the cells within `limit` of `from`
// along each axis, since no shorter walk leaves them.
std::optional<std::int32_t> walk_length(const CompiledMap& map, Cell from,
                                        Cell goal, std::int32_t limit) {
  const std::int32_t left = std::max(0, from.x - limit);
  const std::int32_t top = std::max(0, from.y - limit);
  const std::int32_t right = std::min(map.side(), from.x + limit + 1);
  const std::int32_t bottom = std::min(map.side(), from.y + limit + 1);
  const auto width = static_cast<std::size_t>(right - left);
  const auto slot = [&](Cell cell) {
    return (static_cast<std::size_t>(cell.y - top) * width) +
           static_cast<std::size_t>(cell.x - left);
  };
  const auto inside = [&](Cell cell) {
    return cell.x >= left && cell.x < right && cell.y >= top && cell.y < bottom;
  };
  // A walk of `limit` cells costs less than this.
  const std::uint32_t ceiling =
      (static_cast<std::uint32_t>(limit) + 1) * STRAIGHT_COST;
  std::vector<std::uint32_t> costs(
      width * static_cast<std::size_t>(bottom - top), UNREACHED);
  // Each entry is a cell's cost so far plus its least cost to `goal`.
  using Entry = std::pair<std::uint32_t, Cell>;
  const auto later = [](const Entry& one, const Entry& other) {
    return one.first > other.first;
  };
  std::priority_queue<Entry, std::vector<Entry>, decltype(later)> frontier(
      later);
  costs.at(slot(from)) = 0;
  frontier.emplace(least_cost(from, goal), from);
  while (!frontier.empty()) {
    const auto [estimate, cell] = frontier.top();
    frontier.pop();
    const std::uint32_t cost = costs.at(slot(cell));
    if (cell.x == goal.x && cell.y == goal.y) {
      return static_cast<std::int32_t>(cost / STRAIGHT_COST);
    }
    if (estimate != cost + least_cost(cell, goal)) {
      continue;
    }
    for (const auto& [column, row] : DIRECTIONS) {
      const Cell next{cell.x + column, cell.y + row};
      const bool diagonal = column != 0 && row != 0;
      const Cell beside_x{next.x, cell.y};
      const Cell beside_y{cell.x, next.y};
      const bool open =
          inside(next) && steps(map, cell, next) &&
          (!diagonal ||
           (steps(map, cell, beside_x) && steps(map, beside_x, next) &&
            steps(map, cell, beside_y) && steps(map, beside_y, next)));
      if (!open) {
        continue;
      }
      const std::uint32_t through =
          cost + (diagonal ? DIAGONAL_COST : STRAIGHT_COST);
      const std::uint32_t bound = through + least_cost(next, goal);
      if (bound < ceiling && through < costs.at(slot(next))) {
        costs.at(slot(next)) = through;
        frontier.emplace(bound, next);
      }
    }
  }
  return std::nullopt;
}

}  // namespace

bool map_side_holds(std::int32_t side) {
  const bool power_of_two = side > 0 && (side & (side - 1)) == 0;
  return power_of_two && side >= MIN_MAP_SIDE && side <= MAX_MAP_SIDE;
}

bool map_block_holds(std::int32_t block) {
  return block == SMALL_BLOCK || block == LARGE_BLOCK;
}

CompiledMap::CompiledMap(const MapSource& source)
    : side_(source.side),
      block_(source.block),
      words_(
          (static_cast<std::size_t>(side_) * static_cast<std::size_t>(side_)) /
          CELLS_A_WORD),
      lands_(source.lands),
      places_(source.places) {
  // A block spans whole words, so each row of blocks paints one row of words,
  // copied to each of its rows of cells.
  const auto block = static_cast<std::size_t>(block_);
  const std::size_t blocks = static_cast<std::size_t>(side_) / block;
  std::vector<std::uint64_t> row(static_cast<std::size_t>(side_) /
                                 CELLS_A_WORD);
  auto painted = words_.begin();
  for (std::size_t first = 0; first < blocks * blocks; first += blocks) {
    auto word = row.begin();
    for (std::size_t each = first; each < first + blocks; ++each) {
      const auto footing = static_cast<std::uint64_t>(source.footings.at(each));
      const std::uint64_t cell =
          footing | (std::uint64_t{source.levels.at(each)} << LEVEL_SHIFT);
      word = std::fill_n(word, block / CELLS_A_WORD, cell * EVERY_BYTE);
    }
    for (std::size_t copy = 0; copy < block; ++copy) {
      painted = std::copy(row.begin(), row.end(), painted);
    }
  }
}

std::int32_t CompiledMap::side() const { return side_; }

std::int32_t CompiledMap::block() const { return block_; }

const std::vector<Cell>& CompiledMap::places() const { return places_; }

bool CompiledMap::contains(Cell cell) const {
  return cell.x >= 0 && cell.y >= 0 && cell.x < side_ && cell.y < side_;
}

Footing CompiledMap::footing(Cell cell) const {
  return static_cast<Footing>(byte(cell) & FOOTING_MASK);
}

std::uint8_t CompiledMap::level(Cell cell) const {
  return static_cast<std::uint8_t>(byte(cell) >> LEVEL_SHIFT);
}

std::uint8_t CompiledMap::land(Cell cell) const {
  const auto blocks = static_cast<std::size_t>(side_ / block_);
  return lands_.at((static_cast<std::size_t>(cell.y / block_) * blocks) +
                   static_cast<std::size_t>(cell.x / block_));
}

MapHash CompiledMap::hash() const {
  std::uint64_t digest = mix(mix(static_cast<std::uint64_t>(side_)) ^
                             static_cast<std::uint64_t>(block_));
  const auto fold = [&digest](std::uint64_t value) {
    digest = mix(digest ^ value);
  };
  for (const std::uint64_t word : words_) {
    fold(word);
  }
  for (const std::uint8_t land : lands_) {
    fold(land);
  }
  for (const Cell place : places_) {
    fold(static_cast<std::uint64_t>(place.x));
    fold(static_cast<std::uint64_t>(place.y));
  }
  return MapHash{digest};
}

std::uint8_t CompiledMap::byte(Cell cell) const {
  const std::size_t cell_index =
      (static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(side_)) +
      static_cast<std::size_t>(cell.x);
  const auto shift =
      static_cast<unsigned>((cell_index % CELLS_A_WORD) * BYTE_BITS);
  return static_cast<std::uint8_t>(words_.at(cell_index / CELLS_A_WORD) >>
                                   shift);
}

std::vector<MapFinding> validate_map(const CompiledMap& map,
                                     const std::vector<Route>& routes,
                                     std::int32_t apart) {
  std::vector<MapFinding> found;
  std::vector<Cell> earlier;
  for (std::size_t place = 0; place < map.places().size(); ++place) {
    const Cell cell = map.places().at(place);
    if (map.footing(cell) == Footing::BLOCKED) {
      found.push_back(MapFinding{MapFault::BLOCKED_PLACE, place});
    }
    if (!open_site(cell, earlier, apart)) {
      found.push_back(MapFinding{MapFault::CROWDED_PLACE, place});
    }
    earlier.push_back(cell);
  }
  for (std::size_t route = 0; route < routes.size(); ++route) {
    const Route& key = routes.at(route);
    const std::int64_t length = key.length;
    const auto limit =
        static_cast<std::int32_t>(length * SLACK_OVER / SLACK_UNDER);
    const std::optional<std::int32_t> walked = walk_length(
        map, map.places().at(key.from), map.places().at(key.to), limit);
    if (!walked) {
      found.push_back(MapFinding{MapFault::LONG_ROUTE, route});
    } else if (*walked < key.length) {
      found.push_back(MapFinding{MapFault::SHORT_ROUTE, route});
    }
  }
  return found;
}

}  // namespace murim
