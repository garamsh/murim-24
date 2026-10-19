#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "coarse_graph.hpp"
#include "compiled_map.hpp"

namespace murim {

/// The format version a shipped graph is written in. A change to what it
/// holds raises it.
constexpr std::uint32_t GRAPH_VERSION = 1;

/// Why a shipped graph is refused.
enum class GraphFault : std::uint8_t {
  /// The bytes do not begin as a shipped graph does.
  NOT_A_GRAPH,
  /// The graph was written in a format version other than this one.
  UNSUPPORTED_VERSION,
  /// The graph was built for a map other than the one it is read for.
  OTHER_MAP,
  /// The graph fails its checksum, runs short or past its length, or holds
  /// an index or a size no graph of its map can hold.
  CORRUPTED,
};

/// Encodes `graph`, the coarse graph of the bare compiled map whose hash is
/// `map`, as the build tool ships it with the map (ADR 0047).
[[nodiscard]] std::vector<std::uint8_t> ship_graph(const CoarseGraph& graph,
                                                   MapHash map);

/// Reads the graph `bytes` ship for the compiled map whose hash is `map`, of
/// `size` cells, or why it is refused. A graph of a map of another size is
/// refused as CORRUPTED, and its indices are checked against its own areas
/// and regions; that it is the graph of `map` rests on the hash.
[[nodiscard]] std::variant<CoarseGraph, GraphFault> read_shipped_graph(
    const std::vector<std::uint8_t>& bytes, MapHash map, Footprint size);

}  // namespace murim
