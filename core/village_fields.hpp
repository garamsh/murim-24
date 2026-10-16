#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"

namespace murim {

/// The cells a village keeps on each side of its centre's cell, west, north,
/// east and south: its square, its structures and its centre, as whatever
/// lays them bounds them.
struct Kept {
  std::int32_t west = 0;
  std::int32_t north = 0;
  std::int32_t east = 0;
  std::int32_t south = 0;
};

/// One village's path fields (ADR 0081): over its window, the cells from
/// `corner` of `size` as passable on the map it was made over, one field for
/// each of its places. Each cell of a field holds its step in one byte and
/// its path cost to the place in two, a straight step costing 5 and a
/// diagonal one 7, taken only where both cells beside it are passable
/// (ADR 0037); a cell whose cost would pass the largest two bytes hold is not
/// reached.
class VillageFields {
 public:
  /// No window and no fields.
  VillageFields() = default;

  /// The window from `corner` of `size` over `map`, for `places` places,
  /// holding no field yet. Throws std::invalid_argument when `size` is wider
  /// or taller than MAX_SIDE or not positive.
  VillageFields(const Map& map, Cell corner, Footprint size,
                std::size_t places);

  /// Returns whether place `place`'s field is built. Throws
  /// std::out_of_range when there is no such place.
  [[nodiscard]] bool holds(std::size_t place) const;

  /// Builds place `place`'s field toward `seeds`, the cells of the window a
  /// walk to it ends on; of the steps beginning a shortest path, a cell's is
  /// the one nearest the line to `aim`, the first among equals.
  void build(std::size_t place, const std::vector<Cell>& seeds, Cell aim);

  /// Returns whether `cell` reaches place `place` by its field: inside the
  /// window, passable, and joined to a seed.
  [[nodiscard]] bool reaches(std::size_t place, Cell cell) const;

  /// Returns the step from `cell` toward place `place`: none at a seed, off
  /// the window or where it is not reached.
  [[nodiscard]] Step step(std::size_t place, Cell cell) const;

  /// Returns how many fields are built.
  [[nodiscard]] std::size_t built() const;

  /// Returns the bytes the built fields' cells take.
  [[nodiscard]] std::size_t bytes() const;

  /// Returns whether the window shares a cell with the cells from `corner`
  /// of `size`.
  [[nodiscard]] bool overlaps(Cell corner, Footprint size) const;

  /// The widest and tallest window.
  static constexpr std::int32_t MAX_SIDE = 128;
  /// The cells a village's window keeps round its square, structures and
  /// centre.
  static constexpr std::int32_t MARGIN = 4;

  /// Returns whether a window over `kept`, with MARGIN cells round it, is no
  /// wider or taller than MAX_SIDE: the one bound on what a village keeps,
  /// whatever lays it.
  [[nodiscard]] static bool holds(Kept kept);

 private:
  void search(std::size_t base, const std::vector<Cell>& seeds);
  void choose_steps(std::size_t base, Cell aim);
  [[nodiscard]] bool open(std::size_t slot, std::size_t way) const;
  [[nodiscard]] std::size_t beside(std::size_t slot, Step direction) const;
  [[nodiscard]] std::size_t cells() const;
  [[nodiscard]] Cell cell_at(std::size_t slot) const;
  [[nodiscard]] bool inside(Cell cell) const;
  [[nodiscard]] std::size_t index(Cell cell) const;
  [[nodiscard]] std::size_t slot_of(std::size_t place) const;

  Cell corner_{0, 0};
  Footprint size_{0, 0};
  // Each place's field slot, or NONE while it is not built.
  std::vector<std::size_t> slots_;
  // Each window cell's passability on the map, and the directions a step
  // from it may take, a bit for each in the order of the directions.
  std::vector<bool> passable_;
  std::vector<std::uint8_t> ways_;
  // Each slot's cells in turn: a step, 0 for none or one plus its index in
  // the directions, and a path cost.
  std::vector<std::uint8_t> steps_;
  std::vector<std::uint16_t> costs_;
  std::size_t built_ = 0;
};

}  // namespace murim
