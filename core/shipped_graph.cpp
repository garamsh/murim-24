#include "shipped_graph.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "areas.hpp"
#include "coarse_graph.hpp"
#include "compiled_map.hpp"
#include "encoding.hpp"
#include "map.hpp"

namespace murim {
namespace {

// "MURIMGRF": the bytes every shipped graph begins with.
constexpr std::array<std::uint8_t, 8> MAGIC{0x4D, 0x55, 0x52, 0x49,
                                            0x4D, 0x47, 0x52, 0x46};
// The magic, the version, the map's hash and the payload's length.
constexpr std::size_t HEADER_BYTES = MAGIC.size() + sizeof(std::uint32_t) +
                                     sizeof(std::uint64_t) +
                                     sizeof(std::uint64_t);
constexpr std::size_t CHECKSUM_BYTES = sizeof(std::uint64_t);
// AreaCosts' members.
constexpr std::uint8_t COSTS = 2;
// A node's border index as written when it is no region's border node; any
// other is the index plus one.
constexpr std::uint16_t NO_BORDER = 0;

// `value` in the narrower width a shipped graph writes it in. No graph of a
// map within the core's bounds holds a value past it.
template <typename Narrow, typename Wide>
Narrow narrow(Wide value) {
  if (value > std::numeric_limits<Narrow>::max()) {
    throw std::length_error("a graph holds a value past its shipped width");
  }
  return static_cast<Narrow>(value);
}

// Refuses what is read unless `holds`.
void require(bool holds) {
  if (!holds) {
    throw Malformed{};
  }
}

}  // namespace

class GraphCodec {
 public:
  // Writes every area, then every region; the numbering and the joined
  // components are derived from them and not written.
  static void write(Writer& out, const CoarseGraph& graph) {
    out.u8(static_cast<std::uint8_t>(graph.costs_));
    out.i32(graph.width_);
    out.i32(graph.height_);
    for (std::size_t area = 0; area < graph.areas_.size(); ++area) {
      write_area(out, graph.areas_.at(area), graph.area_corner(area));
    }
    for (const CoarseGraph::RegionGraph& region : graph.regions_) {
      out.u16(narrow<std::uint16_t>(region.border.size()));
      for (const NodeRef& ref : region.border) {
        out.u32(narrow<std::uint32_t>(ref.area));
        out.u8(narrow<std::uint8_t>(ref.node));
      }
      for (const std::vector<CoarseGraph::Edge>& edges : region.within) {
        out.u16(narrow<std::uint16_t>(edges.size()));
        for (const CoarseGraph::Edge& edge : edges) {
          out.u16(narrow<std::uint16_t>(edge.to));
          out.u32(edge.cost);
        }
      }
    }
  }

  // Reads a graph as write() wrote it, refusing any index or size out of
  // range of its areas, nodes and regions.
  static CoarseGraph read(Reader& input) {
    const auto costs = static_cast<AreaCosts>(input.tag(COSTS));
    const std::int32_t width = input.i32();
    const std::int32_t height = input.i32();
    require(width >= 1 && height >= 1 && width <= MAX_MAP_SIDE &&
            height <= MAX_MAP_SIDE);
    CoarseGraph graph{costs, Footprint{width, height}};
    for (std::size_t area = 0; area < graph.areas_.size(); ++area) {
      graph.areas_.at(area) = read_area(input, graph, area);
    }
    for (const CoarseGraph::AreaGraph& held : graph.areas_) {
      for (const CoarseExit& exit : held.exits) {
        require(exit.far_node < graph.areas_.at(exit.area).nodes.size());
      }
    }
    for (std::size_t region = 0; region < graph.regions_.size(); ++region) {
      graph.regions_.at(region) = read_region(input, graph, region);
    }
    // Each border node names its place in its region's border, and each
    // place names it back.
    for (std::size_t area = 0; area < graph.areas_.size(); ++area) {
      const CoarseGraph::RegionGraph& region =
          graph.regions_.at(graph.region_of_area(area));
      const std::vector<std::int32_t>& border = graph.areas_.at(area).border;
      for (std::size_t node = 0; node < border.size(); ++node) {
        if (border.at(node) >= 0) {
          const auto place = static_cast<std::size_t>(border.at(node));
          require(place < region.border.size() &&
                  region.border.at(place).area == area &&
                  region.border.at(place).node == node);
        }
      }
    }
    for (const CoarseGraph::RegionGraph& region : graph.regions_) {
      for (std::size_t place = 0; place < region.border.size(); ++place) {
        const NodeRef ref = region.border.at(place);
        require(graph.areas_.at(ref.area).border.at(ref.node) ==
                static_cast<std::int32_t>(place));
      }
    }
    graph.number();
    graph.join_components();
    return graph;
  }

 private:
  static void write_area(Writer& out, const CoarseGraph::AreaGraph& held,
                         Cell corner) {
    out.u16(held.components);
    out.flag(held.open);
    out.flag(!held.labels.empty());
    for (const std::int16_t label : held.labels) {
      out.u16(static_cast<std::uint16_t>(label + 1));
    }
    out.u8(narrow<std::uint8_t>(held.nodes.size()));
    for (const CoarseNode& node : held.nodes) {
      out.u8(static_cast<std::uint8_t>(node.cell.x - corner.x));
      out.u8(static_cast<std::uint8_t>(node.cell.y - corner.y));
      out.u16(node.component);
    }
    for (std::size_t node = 0; node < held.nodes.size(); ++node) {
      const std::vector<CoarseGraph::Edge>& edges = held.within.at(node);
      out.u8(narrow<std::uint8_t>(edges.size()));
      for (const CoarseGraph::Edge& edge : edges) {
        out.u8(narrow<std::uint8_t>(edge.to));
        out.u16(narrow<std::uint16_t>(edge.cost));
      }
      const std::int32_t border = held.border.at(node);
      out.u16(border < 0 ? NO_BORDER
                         : narrow<std::uint16_t>(
                               static_cast<std::size_t>(border) + 1));
    }
    out.u8(narrow<std::uint8_t>(held.exits.size()));
    for (const CoarseExit& exit : held.exits) {
      out.u8(narrow<std::uint8_t>(exit.node));
      out.u32(narrow<std::uint32_t>(exit.area));
      out.u8(narrow<std::uint8_t>(exit.far_node));
      out.u16(static_cast<std::uint16_t>(exit.first.x));
      out.u16(static_cast<std::uint16_t>(exit.first.y));
      out.flag(exit.along.y == 1);
      out.u8(static_cast<std::uint8_t>(exit.length));
    }
  }

  static CoarseGraph::AreaGraph read_area(Reader& input,
                                          const CoarseGraph& graph,
                                          std::size_t area) {
    const Cell corner = graph.area_corner(area);
    const Footprint size = graph.area_size(area);
    const std::size_t cells = static_cast<std::size_t>(size.width) *
                              static_cast<std::size_t>(size.height);
    CoarseGraph::AreaGraph made;
    made.components = input.u16();
    made.open = input.flag();
    const bool labelled = input.flag();
    // As built: an area of more than one component keeps its labels, and
    // one costed in closed form is a single component.
    require(made.components <= cells && labelled == (made.components > 1) &&
            (!made.open || made.components == 1));
    if (labelled) {
      made.labels.reserve(cells);
      for (std::size_t cell = 0; cell < cells; ++cell) {
        const std::uint16_t label = input.u16();
        require(label <= made.components);
        made.labels.push_back(static_cast<std::int16_t>(label - 1));
      }
    }
    const std::size_t nodes = input.u8();
    for (std::size_t node = 0; node < nodes; ++node) {
      const std::int32_t across = input.u8();
      const std::int32_t down = input.u8();
      const std::uint16_t component = input.u16();
      require(across < size.width && down < size.height &&
              component < made.components);
      made.nodes.push_back(
          CoarseNode{Cell{corner.x + across, corner.y + down}, component});
    }
    made.within.resize(nodes);
    for (std::size_t node = 0; node < nodes; ++node) {
      const std::size_t edges = input.u8();
      for (std::size_t edge = 0; edge < edges; ++edge) {
        const std::size_t target = input.u8();
        require(target < nodes && target != node);
        made.within.at(node).push_back(CoarseGraph::Edge{target, input.u16()});
      }
      const std::uint16_t border = input.u16();
      made.border.push_back(
          border == NO_BORDER ? -1 : static_cast<std::int32_t>(border) - 1);
    }
    const std::size_t exits = input.u8();
    for (std::size_t exit = 0; exit < exits; ++exit) {
      const std::size_t node = input.u8();
      const std::size_t far_area = input.u32();
      const std::size_t far_node = input.u8();
      const Cell first{input.u16(), input.u16()};
      const Cell along = input.flag() ? Cell{0, 1} : Cell{1, 0};
      const std::int32_t length = input.u8();
      const Cell last{first.x + (along.x * (length - 1)),
                      first.y + (along.y * (length - 1))};
      require(node < nodes && far_area < graph.areas_.size() && length >= 1 &&
              length <= AREA_SIDE && last.x < graph.width_ &&
              last.y < graph.height_);
      made.exits.push_back(
          CoarseExit{node, far_area, far_node, first, along, length});
    }
    return made;
  }

  static CoarseGraph::RegionGraph read_region(Reader& input,
                                              const CoarseGraph& graph,
                                              std::size_t region) {
    CoarseGraph::RegionGraph made;
    const std::size_t border = input.u16();
    for (std::size_t place = 0; place < border; ++place) {
      const std::size_t area = input.u32();
      const std::size_t node = input.u8();
      require(area < graph.areas_.size() &&
              graph.region_of_area(area) == region &&
              node < graph.areas_.at(area).nodes.size());
      made.border.push_back(NodeRef{area, node});
    }
    made.within.resize(border);
    for (std::size_t from = 0; from < border; ++from) {
      const std::size_t edges = input.u16();
      for (std::size_t edge = 0; edge < edges; ++edge) {
        const std::size_t target = input.u16();
        require(target < border && target != from);
        made.within.at(from).push_back(CoarseGraph::Edge{target, input.u32()});
      }
    }
    return made;
  }
};

std::vector<std::uint8_t> ship_graph(const CoarseGraph& graph, MapHash map) {
  Writer payload;
  GraphCodec::write(payload, graph);
  Writer out;
  for (const std::uint8_t byte : MAGIC) {
    out.u8(byte);
  }
  out.u32(GRAPH_VERSION);
  out.u64(static_cast<std::uint64_t>(map));
  out.size(payload.bytes().size());
  std::vector<std::uint8_t>& bytes = out.bytes();
  bytes.insert(bytes.end(), payload.bytes().begin(), payload.bytes().end());
  out.u64(checksum(bytes, 0, bytes.size()));
  return std::move(bytes);
}

std::variant<CoarseGraph, GraphFault> read_shipped_graph(
    const std::vector<std::uint8_t>& bytes, MapHash map) {
  // The one handler for a refused graph: every refusal while it is read ends
  // here as CORRUPTED.
  try {
    for (std::size_t index = 0; index < MAGIC.size(); ++index) {
      if (index >= bytes.size() || bytes.at(index) != MAGIC.at(index)) {
        return GraphFault::NOT_A_GRAPH;
      }
    }
    if (bytes.size() < HEADER_BYTES + CHECKSUM_BYTES) {
      return GraphFault::CORRUPTED;
    }
    Reader header{bytes, MAGIC.size(), HEADER_BYTES};
    if (header.u32() != GRAPH_VERSION) {
      return GraphFault::UNSUPPORTED_VERSION;
    }
    const std::uint64_t built_for = header.u64();
    const std::uint64_t length = header.u64();
    const std::size_t after = bytes.size() - HEADER_BYTES - CHECKSUM_BYTES;
    if (length != after) {
      return GraphFault::CORRUPTED;
    }
    const std::size_t end = HEADER_BYTES + after;
    if (Reader{bytes, end, end + CHECKSUM_BYTES}.u64() !=
        checksum(bytes, 0, end)) {
      return GraphFault::CORRUPTED;
    }
    // Checked after the checksum, so a graph built for another map is told
    // from a damaged one.
    if (built_for != static_cast<std::uint64_t>(map)) {
      return GraphFault::OTHER_MAP;
    }
    Reader input{bytes, HEADER_BYTES, end};
    CoarseGraph graph = GraphCodec::read(input);
    if (!input.done()) {
      return GraphFault::CORRUPTED;
    }
    return graph;
  } catch (const Malformed&) {
    return GraphFault::CORRUPTED;
  }
}

}  // namespace murim
