#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "map.hpp"
#include "village.hpp"

namespace murim {

/// What a cell offers a structure being placed: nothing, for ground bodies
/// cannot cross; nothing, for a cell a structure or a clearing covers or
/// someone stands on or is bound for; a terraced field, for rough ground; or
/// anything, for free buildable ground.
enum class Plot : std::uint8_t { BLOCKED, BUILT, TAKEN, ROUGH, FREE };

/// The land a village's structures are placed on: what each cell offers, and
/// whether bodies pass it, row by row over `size`.
struct Lot {
  Footprint size;
  std::vector<Plot> plots;
  std::vector<bool> passable;
};

/// Where a village grows (ADR 0076): its centre, the square its tier keeps
/// clear round it, and the cells from it within which it builds.
struct Townsite {
  Cell centre;
  Footprint square;
  std::int32_t vicinity;
};

/// Returns the corner of a square of `square` round `centre`.
[[nodiscard]] Cell square_corner(Cell centre, Footprint square);

/// Returns whether no centre of `centres` lies nearer than `apart` cells to
/// `site`: the one test generation and founding keep villages apart by.
[[nodiscard]] bool open_site(Cell site, const std::vector<Cell>& centres,
                             std::int32_t apart);

/// Returns the draw that breaks ties for the structure laid `index`-th in
/// village `village` of a world of `seed`: a counter-based hash, so it does not
/// depend on what else the world drew.
[[nodiscard]] std::uint64_t placement_draw(std::uint64_t seed,
                                           std::size_t village,
                                           std::size_t index);

/// Returns the corner where a structure of `footprint` and `role` is placed
/// for the village at `site` on `lot`, within its vicinity, by path distance
/// from its centre over cells bodies pass, a diagonal step costing 7 and a
/// straight one 5, `draw` choosing among the corners equally near:
/// - square: on free cells touching the square's edge, or, when none is
///   free, as near as any free cells
/// - home: on free cells, a cell clear of any built cell
/// - edge: on free or rough cells, at least 2 straight steps farther from the
///   centre than the farthest of `homes`
/// Nothing when no corner fits.
[[nodiscard]] std::optional<Cell> place(const Lot& lot, const Townsite& site,
                                        Footprint footprint, Role role,
                                        const std::vector<Structure>& homes,
                                        std::uint64_t draw);

/// Marks the cells of `footprint` from `corner` on `lot` built, and not passed
/// by bodies when the structure `blocks`.
void build_on(Lot& lot, Cell corner, Footprint footprint, bool blocks);

}  // namespace murim
