#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "map.hpp"

namespace murim {

/// The longest side a hand-made map may have, in cells (ADR 0070): its
/// 65,536 areas of 32 cells still fit an area's index in 2 bytes.
constexpr std::int32_t MAX_MAP_SIDE = 8'192;

/// The shortest side a hand-made map may have: one area of 32 cells.
constexpr std::int32_t MIN_MAP_SIDE = 32;

/// Whether a map of `side` cells painted per `block` cells is within the
/// core's bounds: a side a power of two from MIN_MAP_SIDE to MAX_MAP_SIDE,
/// and a block of 8 or 16 cells.
[[nodiscard]] bool map_side_holds(std::int32_t side);
[[nodiscard]] bool map_block_holds(std::int32_t block);

/// A key route between two authored places, by their indices, and its
/// designed length in cells.
struct Route {
  std::size_t from;
  std::size_t to;
  std::int32_t length;
};

/// A village an authored place settles: laid out from village template
/// `settlement` and owned by faction `owner`, by their indices.
struct VillagePlace {
  std::size_t owner;
  std::size_t settlement;
};

/// The seat of sect `sect`, by its index, that an authored place settles.
struct SeatPlace {
  std::size_t sect;
};

/// A place authored on a map: its cell, and what it settles there.
struct Place {
  Cell cell;
  std::variant<VillagePlace, SeatPlace> settles;
};

/// A hand-made map as its pack paints it (ADR 0070): `side` cells square,
/// painted per `block` cells, with each block's terrain kind, by its index,
/// footing, level and land row by row, and the places authored on it.
struct MapSource {
  std::int32_t side = 0;
  std::int32_t block = 0;
  std::vector<std::uint8_t> terrains;
  std::vector<Footing> footings;
  std::vector<std::uint8_t> levels;
  std::vector<std::uint8_t> lands;
  std::vector<Place> places;
};

/// Digest of a compiled map; equal maps give equal hashes.
enum class MapHash : std::uint64_t {};

/// A hand-made map compiled from its source: every cell's footing and level,
/// a byte a cell, each block's terrain kind and land, and the places
/// authored on it.
class CompiledMap {
 public:
  /// Compiles `source`, whose side and block hold the core's bounds and whose
  /// layers hold one value a block: every cell takes its block's values.
  explicit CompiledMap(const MapSource& source);

  [[nodiscard]] std::int32_t side() const;
  [[nodiscard]] std::int32_t block() const;
  [[nodiscard]] const std::vector<Place>& places() const;

  /// Returns whether `cell` lies on the map.
  [[nodiscard]] bool contains(Cell cell) const;

  /// Returns the footing, level, terrain kind and land of `cell`, which must
  /// lie on the map.
  [[nodiscard]] Footing footing(Cell cell) const;
  [[nodiscard]] std::uint8_t level(Cell cell) const;
  [[nodiscard]] std::uint8_t terrain(Cell cell) const;
  [[nodiscard]] std::uint8_t land(Cell cell) const;

  /// Returns the digest of every cell, terrain kind, land and place.
  [[nodiscard]] MapHash hash() const;

 private:
  [[nodiscard]] std::uint8_t byte(Cell cell) const;
  [[nodiscard]] std::size_t block_of(Cell cell) const;

  std::int32_t side_;
  std::int32_t block_;
  // A byte a cell, eight to a word, row by row: its footing in the low
  // nibble and its level in the high one.
  std::vector<std::uint64_t> words_;
  std::vector<std::uint8_t> terrains_;
  std::vector<std::uint8_t> lands_;
  std::vector<Place> places_;
};

/// Returns `map`'s footing as the rows a Map is parsed from, a cell each its
/// footing's tile_of: the map a world on it is created on before its
/// structures block their cells.
[[nodiscard]] std::vector<std::string> footing_rows(const CompiledMap& map);

/// Why a compiled map is refused.
enum class MapFault : std::uint8_t {
  /// A place stands on a blocked cell.
  BLOCKED_PLACE,
  /// A place stands closer than `apart` to an earlier one.
  CROWDED_PLACE,
  /// A key route's shortest path is shorter than its designed length.
  SHORT_ROUTE,
  /// A key route's shortest path is more than a fifth longer than its
  /// designed length, or there is none.
  LONG_ROUTE,
};

/// One fault found in a compiled map, at place or route `index`.
struct MapFinding {
  MapFault fault;
  std::size_t index;
};

/// Checks `map` as ADR 0047's validator does, as far as its layers reach:
/// every place on a passable cell and no nearer than `apart` cells to an
/// earlier one, by the rule founding keeps villages apart by (ADR 0077), and
/// each of `routes`' shortest walks no shorter than its designed length and
/// at most a fifth longer. A walk is 8-connected through passable cells, a
/// step only between levels at most one apart (ADR 0058), a diagonal step
/// costing 7/5 of a straight one and taken only where both cells beside it
/// could be stepped through, and its length in cells rounded down, as
/// FlowField counts it. Returns every fault, places first.
[[nodiscard]] std::vector<MapFinding> validate_map(
    const CompiledMap& map, const std::vector<Route>& routes,
    std::int32_t apart);

}  // namespace murim
