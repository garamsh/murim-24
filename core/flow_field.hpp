#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {

/// A unit step along each axis: -1, 0 or 1.
struct Step {
  std::int32_t x;
  std::int32_t y;
};

/// A cell a field is seeded on, and its cost to the objective, a straight
/// step 5 and a diagonal one 7.
struct FieldSeed {
  Cell cell;
  std::uint32_t cost;
};

/// How a field's search settles its cells: by a ring of buckets over the
/// costs, which a step raises by at most DIAGONAL_COST, or by a binary heap, a
/// seam for a test to compare the two. Both settle every cell at the same cost.
enum class Frontier : std::uint8_t { BUCKETS, HEAP };

/// For every cell of a window of a map, the step and the path length toward
/// an objective along a shortest 8-connected path through passable cells of
/// the window, a diagonal step costing 7/5 of a straight one and taken only
/// where both cells beside it are passable; the path ends on one of the
/// field's seeds, at that seed's cost. Of the steps that begin a shortest
/// path, a cell's step is the one nearest the straight line to the aim, the
/// objective itself.
class FlowField {
 public:
  /// Builds the field over the whole map toward `objective`, which must be
  /// passable.
  FlowField(const Map& map, Cell objective);

  /// Builds the field over the cells from `corner` of `size`, seeded on
  /// `seeds`, aimed at `aim`.
  FlowField(const Map& map, Cell corner, Footprint size,
            const std::vector<FieldSeed>& seeds, Cell aim,
            Frontier frontier = Frontier::BUCKETS);

  /// Builds this field again as the windowed constructor does, reusing what
  /// it holds.
  void rebuild(const Map& map, Cell corner, Footprint size,
               const std::vector<FieldSeed>& seeds, Cell aim,
               Frontier frontier = Frontier::BUCKETS);

  /// Returns the step from `cell` toward the objective: none at a seed, where
  /// it cannot be reached, or off the window.
  [[nodiscard]] Step step(Cell cell) const;

  /// Returns the path length from `cell` to the objective, in cells, a
  /// diagonal step counting 7/5 of one and the total rounded down: 0 at the
  /// objective, UNREACHABLE where it cannot be reached or the cell is not
  /// passable or off the window.
  [[nodiscard]] std::uint32_t distance(Cell cell) const;

  static constexpr std::uint32_t UNREACHABLE = UINT32_MAX;

 private:
  // Settles every cell's cost from the seeds already costed in costs_.
  void settle_by_buckets(const std::vector<FieldSeed>& seeds);
  void settle_by_heap(const std::vector<FieldSeed>& seeds);
  // Lowers the cost of each cell a step from `slot`, at its cost, opens onto,
  // handing each lowered cell's cost and slot to `reached`.
  template <typename Reached>
  void relax(std::size_t slot, Reached reached);
  [[nodiscard]] Step straightest(Cell cell, Cell objective) const;
  [[nodiscard]] bool opens(Cell cell, Step direction) const;
  [[nodiscard]] bool inside(Cell cell) const;
  [[nodiscard]] std::size_t index(Cell cell) const;
  [[nodiscard]] Cell cell_at(std::size_t slot) const;

  Cell corner_{0, 0};
  std::int32_t width_ = 0;
  std::int32_t height_ = 0;
  std::vector<Step> steps_;
  // Path costs, a straight step 5 and a diagonal one 7.
  std::vector<std::uint32_t> costs_;
  // Scratch a rebuild reuses, so a field rebuilt allocates nothing once its
  // buffers have grown: whether each cell is passable, the frontier, and
  // whether each cell is a seed that kept its cost.
  std::vector<bool> passable_;
  std::vector<std::pair<std::uint32_t, std::size_t>> frontier_;
  // The ring of buckets, a bucket for each cost modulo its size, and the
  // seeds by cost, for the bucket search.
  static constexpr std::size_t RING = 8;  // more than the dearest step, 7
  std::array<std::vector<std::size_t>, RING> buckets_;
  std::vector<std::pair<std::uint32_t, std::size_t>> sorted_seeds_;
  std::vector<bool> seeded_;
};

}  // namespace murim
