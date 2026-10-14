#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "map.hpp"
#include "village.hpp"

namespace murim {

/// Cells along each side of an area, the square the chart keeps seen cells
/// in.
constexpr std::int32_t AREA_SIDE = 32;

/// Bytes of one area's seen cells, a bit a cell, row by row, the lowest bit
/// of each byte first.
constexpr std::size_t AREA_BYTES =
    static_cast<std::size_t>(AREA_SIDE * AREA_SIDE) / 8;

/// How a village's structures stood when it was last seen: every one intact,
/// some not, or every one rubble.
enum class VillageStage : std::uint8_t { STANDING, DAMAGED, RUINS };

/// A village as it was last seen.
struct VillageMemory {
  std::size_t owner = 0;
  bool peopled = false;
  VillageStage stage = VillageStage::STANDING;
  /// The village's claim centre when it was last seen.
  Cell centre{};

  friend bool operator==(const VillageMemory& left,
                         const VillageMemory& right) {
    return left.owner == right.owner && left.peopled == right.peopled &&
           left.stage == right.stage && left.centre.x == right.centre.x &&
           left.centre.y == right.centre.y;
  }
  friend bool operator!=(const VillageMemory& left,
                         const VillageMemory& right) {
    return !(left == right);
  }
};

/// One area's seen cells: the area's index, row by row over the map's areas,
/// and its bits.
struct ChartBlock {
  std::uint32_t area;
  std::array<std::uint8_t, AREA_BYTES> bits;
};

/// What a soul has seen of the land: each cell seen, kept by area once one of
/// the area's cells is, and each village as it was last seen.
class Chart {
 public:
  /// A chart of nothing seen, over a map of `size` cells. Throws
  /// std::invalid_argument when either side is not positive.
  explicit Chart(Footprint size);

  /// Restores a chart over a map of `size` cells from `blocks` and
  /// `villages`, as `blocks` and `villages` gave them, or nothing when they
  /// are no chart's: an area off the map, areas out of order or repeated, an
  /// area with no seen cell or with a cell off the map seen, or a village
  /// owned by a faction at or past `factions`, or a remembered claim centre
  /// off the map.
  [[nodiscard]] static std::optional<Chart> restore(
      Footprint size, const std::vector<ChartBlock>& blocks,
      const std::vector<std::optional<VillageMemory>>& villages,
      std::size_t factions);

  /// Marks every cell of the map within `reach` cells of `centre`, its
  /// centre's distance to `centre` at most `reach`, as seen. Returns whether
  /// any was not seen before.
  bool see(Cell centre, std::int32_t reach);

  /// Returns whether `cell` has been seen; a cell off the map has not.
  [[nodiscard]] bool seen(Cell cell) const;

  /// Remembers village `village` as `memory`. Returns whether that changed
  /// what was remembered of it.
  bool remember(std::size_t village, VillageMemory memory);

  /// Each village by index as it was last seen, or nothing for one never
  /// seen; villages past the last one seen are left off.
  [[nodiscard]] const std::vector<std::optional<VillageMemory>>& villages()
      const;

  /// The seen areas' cells, in order of area.
  [[nodiscard]] std::vector<ChartBlock> blocks() const;

  /// Returns whether nothing has been seen: no cell and no village.
  [[nodiscard]] bool empty() const;

  /// A count that rises whenever a cell is first seen or a village's memory
  /// changes, for a view to redraw on; it is no part of what was seen.
  [[nodiscard]] std::uint64_t revision() const;

  /// Returns the faction each cell's territory lay in as last seen, row by
  /// row: as claimed() derives it within `claim` cells of the villages
  /// remembered with living residents, nothing anywhere without a claim, and
  /// nothing for a cell not seen.
  [[nodiscard]] std::vector<std::optional<std::size_t>> territory(
      std::optional<std::int32_t> claim) const;

 private:
  [[nodiscard]] std::size_t area_of(Cell cell) const;

  Footprint size_;
  std::int32_t areas_across_;
  // The block of each area by index, or NO_BLOCK while none of its cells is
  // seen.
  std::vector<std::uint32_t> block_of_;
  std::vector<ChartBlock> blocks_;
  std::vector<std::optional<VillageMemory>> villages_;
  std::uint64_t revision_ = 0;
};

/// Returns the holder of each cell of a `size` map, row by row: the owner of
/// the nearest of `centres`, each a village's centre and owner, within
/// `claim` cells, the earliest among equals, or nothing beyond every one.
[[nodiscard]] std::vector<std::optional<std::size_t>> claimed(
    Footprint size, std::int32_t claim,
    const std::vector<std::pair<Cell, std::size_t>>& centres);

/// Returns whether a cell of one of `built`, a village's structures by index
/// among `structures`, lies within `reach` cells of `from`: the nearest cell
/// of its footprint at most `reach` away.
[[nodiscard]] bool in_view(const std::vector<Structure>& structures,
                           const std::vector<std::size_t>& built, Cell from,
                           std::int32_t reach);

}  // namespace murim
