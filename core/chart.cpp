#include "chart.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "areas.hpp"
#include "map.hpp"
#include "village.hpp"

namespace murim {
namespace {

constexpr std::uint32_t NO_BLOCK = std::numeric_limits<std::uint32_t>::max();
constexpr std::int32_t BITS_PER_BYTE = 8;

std::int32_t areas_along(std::int32_t cells) {
  return (cells + AREA_SIDE - 1) / AREA_SIDE;
}

// The byte and the bit within it of `cell` in its area's block.
struct BitAt {
  std::size_t byte;
  std::uint8_t mask;
};

BitAt bit_at(Cell cell) {
  const std::int32_t within =
      ((cell.y % AREA_SIDE) * AREA_SIDE) + (cell.x % AREA_SIDE);
  return BitAt{static_cast<std::size_t>(within / BITS_PER_BYTE),
               static_cast<std::uint8_t>(1U << (within % BITS_PER_BYTE))};
}

}  // namespace

Chart::Chart(Footprint size)
    : size_(size), areas_across_(areas_along(size.width)) {
  if (size.width <= 0 || size.height <= 0) {
    throw std::invalid_argument("a chart's map must have cells");
  }
  block_of_.assign(static_cast<std::size_t>(areas_across_) *
                       static_cast<std::size_t>(areas_along(size.height)),
                   NO_BLOCK);
}

std::optional<Chart> Chart::restore(
    Footprint size, const std::vector<ChartBlock>& blocks,
    const std::vector<std::optional<VillageMemory>>& villages,
    std::size_t factions) {
  Chart chart{size};
  std::optional<std::uint32_t> last;
  for (const ChartBlock& block : blocks) {
    if (block.area >= chart.block_of_.size() || (last && block.area <= *last)) {
      return std::nullopt;
    }
    last = block.area;
    const Cell corner{
        static_cast<std::int32_t>(
            block.area % static_cast<std::uint32_t>(chart.areas_across_)) *
            AREA_SIDE,
        static_cast<std::int32_t>(
            block.area / static_cast<std::uint32_t>(chart.areas_across_)) *
            AREA_SIDE};
    bool any = false;
    for (const Cell cell : cells_of(corner, Footprint{AREA_SIDE, AREA_SIDE})) {
      const BitAt bit = bit_at(cell);
      if ((block.bits.at(bit.byte) & bit.mask) == 0) {
        continue;
      }
      if (cell.x >= size.width || cell.y >= size.height) {
        return std::nullopt;
      }
      any = true;
    }
    if (!any) {
      return std::nullopt;
    }
    chart.block_of_.at(block.area) =
        static_cast<std::uint32_t>(chart.blocks_.size());
    chart.blocks_.push_back(block);
  }
  for (const std::optional<VillageMemory>& memory : villages) {
    if (memory && (memory->owner >= factions || memory->centre.x < 0 ||
                   memory->centre.y < 0 || memory->centre.x >= size.width ||
                   memory->centre.y >= size.height)) {
      return std::nullopt;
    }
  }
  chart.villages_ = villages;
  while (!chart.villages_.empty() && !chart.villages_.back()) {
    chart.villages_.pop_back();
  }
  return chart;
}

bool Chart::see(Cell centre, std::int32_t reach) {
  bool fresh = false;
  const std::int64_t reach_squared = std::int64_t{reach} * reach;
  for (std::int32_t row = std::max(centre.y - reach, 0);
       row <= std::min(centre.y + reach, size_.height - 1); ++row) {
    for (std::int32_t column = std::max(centre.x - reach, 0);
         column <= std::min(centre.x + reach, size_.width - 1); ++column) {
      const std::int64_t across = column - centre.x;
      const std::int64_t down = row - centre.y;
      if ((across * across) + (down * down) > reach_squared) {
        continue;
      }
      const Cell cell{column, row};
      std::uint32_t& block = block_of_.at(area_of(cell));
      if (block == NO_BLOCK) {
        block = static_cast<std::uint32_t>(blocks_.size());
        blocks_.push_back(
            ChartBlock{static_cast<std::uint32_t>(area_of(cell)), {}});
      }
      const BitAt bit = bit_at(cell);
      std::uint8_t& byte = blocks_.at(block).bits.at(bit.byte);
      if ((byte & bit.mask) == 0) {
        byte = static_cast<std::uint8_t>(byte | bit.mask);
        fresh = true;
      }
    }
  }
  if (fresh) {
    ++revision_;
  }
  return fresh;
}

bool Chart::seen(Cell cell) const {
  if (cell.x < 0 || cell.y < 0 || cell.x >= size_.width ||
      cell.y >= size_.height) {
    return false;
  }
  const std::uint32_t block = block_of_.at(area_of(cell));
  if (block == NO_BLOCK) {
    return false;
  }
  const BitAt bit = bit_at(cell);
  return (blocks_.at(block).bits.at(bit.byte) & bit.mask) != 0;
}

bool Chart::remember(std::size_t village, VillageMemory memory) {
  if (village >= villages_.size()) {
    villages_.resize(village + 1);
  }
  std::optional<VillageMemory>& kept = villages_.at(village);
  if (kept == memory) {
    return false;
  }
  kept = memory;
  ++revision_;
  return true;
}

const std::vector<std::optional<VillageMemory>>& Chart::villages() const {
  return villages_;
}

std::vector<ChartBlock> Chart::blocks() const {
  std::vector<ChartBlock> ordered = blocks_;
  std::sort(ordered.begin(), ordered.end(),
            [](const ChartBlock& left, const ChartBlock& right) {
              return left.area < right.area;
            });
  return ordered;
}

bool Chart::empty() const { return blocks_.empty() && villages_.empty(); }

std::uint64_t Chart::revision() const { return revision_; }

std::size_t Chart::area_of(Cell cell) const {
  return (static_cast<std::size_t>(cell.y / AREA_SIDE) *
          static_cast<std::size_t>(areas_across_)) +
         static_cast<std::size_t>(cell.x / AREA_SIDE);
}

std::vector<std::optional<std::size_t>> Chart::territory(
    std::optional<std::int32_t> claim) const {
  std::vector<std::pair<Cell, std::size_t>> centres;
  for (const std::optional<VillageMemory>& memory : villages_) {
    if (memory && memory->peopled) {
      centres.emplace_back(memory->centre, memory->owner);
    }
  }
  if (!claim) {
    centres.clear();
  }
  std::vector<std::optional<std::size_t>> held =
      claimed(size_, claim.value_or(0), centres);
  for (const Cell cell : cells_of(Cell{0, 0}, size_)) {
    if (!seen(cell)) {
      held.at((static_cast<std::size_t>(cell.y) *
               static_cast<std::size_t>(size_.width)) +
              static_cast<std::size_t>(cell.x)) = std::nullopt;
    }
  }
  return held;
}

bool in_view(const std::vector<Structure>& structures,
             const std::vector<std::size_t>& built, Cell from,
             std::int32_t reach) {
  const std::int64_t range = reach;
  return std::any_of(
      built.begin(), built.end(),
      [&structures, from, range](std::size_t index) {
        const Structure& structure = structures.at(index);
        const Cell corner = structure.cell;
        const Cell far{corner.x + structure.footprint.width - 1,
                       corner.y + structure.footprint.height - 1};
        const std::int64_t across =
            std::max({corner.x - from.x, 0, from.x - far.x});
        const std::int64_t down =
            std::max({corner.y - from.y, 0, from.y - far.y});
        return (across * across) + (down * down) <= range * range;
      });
}

}  // namespace murim
