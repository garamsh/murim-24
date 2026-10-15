#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {

/// The side of a region, in areas: the level of the coarse graph above the
/// areas (ADR 0070).
constexpr std::int32_t REGION_AREAS = 8;

/// A node of the coarse graph (ADR 0043): the middle cell, on one side, of an
/// entrance between two areas, and the component of its area that cell lies
/// in.
struct CoarseNode {
  Cell cell;
  std::uint16_t component;
};

/// One way out of an area through an entrance: the node on this side, the
/// node on the far side, in the neighbouring area, and the entrance's cells
/// on the far side, a run of `length` cells from `first` along the border.
struct CoarseExit {
  std::size_t node;
  std::size_t area;
  std::size_t far_node;
  Cell first;
  Cell along;
  std::int32_t length;
};

/// A node of the coarse graph named by its area and its index among the
/// area's nodes.
struct NodeRef {
  std::size_t area;
  std::size_t node;
};

/// A shortest walk's cost over the coarse graph, a straight step 5 and a
/// diagonal 7 as FlowField counts it.
using CoarseCost = std::uint32_t;

/// The coarse graph of a map (ADR 0043, ADR 0070): each area's passable cells
/// labelled into components, a node on either side of each entrance along
/// the areas' shared borders, edges joining the nodes of one component at
/// the cost of the shortest walk between them within the area and the two
/// sides of an entrance at a straight step's cost, and above them a level of
/// regions of REGION_AREAS by REGION_AREAS areas, joining the nodes on a
/// region's border at the cost of the shortest level-one walk between them
/// within the region. Nodes are numbered in area order, then within an area
/// by side, top, bottom, left and right, then by cell along the side; every
/// search breaks ties by that order, so the same map gives the same graph and
/// the same paths on every host.
class CoarseGraph {
 public:
  explicit CoarseGraph(const Map& map);

  /// Rebuilds the areas holding `cells`, and their neighbours' entrances
  /// toward them, after the map changed there, and the regions holding them.
  void rebuild(const Map& map, const std::vector<Cell>& cells);

  /// Returns the component of `cell`'s area that the passable cell `cell`
  /// lies in.
  [[nodiscard]] std::uint16_t component_of(const Map& map, Cell cell) const;

  /// Returns whether a walk joins the passable cells `from` and `to`.
  [[nodiscard]] bool joined(const Map& map, Cell from, Cell to) const;

  /// Returns the cost of the shortest walk the graph finds from node `from`
  /// to the passable cell `to`, searched over the regions and then the areas
  /// they cross; nothing when no walk joins them.
  [[nodiscard]] std::optional<CoarseCost> cost_to(const Map& map, NodeRef from,
                                                  Cell to) const;

  /// Returns the cost of the shortest walk the graph finds from the passable
  /// cell `from` to the passable cell `to`, over the regions when
  /// `by_regions` and over the areas alone otherwise; nothing when no walk
  /// joins them.
  [[nodiscard]] std::optional<CoarseCost> path_cost(const Map& map, Cell from,
                                                    Cell to,
                                                    bool by_regions) const;

  /// Returns the ways out of component `component` of area `area`, in node
  /// order.
  [[nodiscard]] std::vector<CoarseExit> exits(std::size_t area,
                                              std::uint16_t component) const;

  [[nodiscard]] std::int32_t columns() const;
  [[nodiscard]] std::size_t area_of(Cell cell) const;
  [[nodiscard]] std::size_t node_count() const;
  [[nodiscard]] const CoarseNode& node(NodeRef ref) const;

 private:
  struct Edge {
    std::size_t to;
    CoarseCost cost;
  };
  // An area's components, nodes, the edges among its nodes, its exits, and
  // each node's index among its region's border nodes, if it is one.
  struct AreaGraph {
    std::uint16_t components = 0;
    std::vector<CoarseNode> nodes;
    std::vector<std::vector<Edge>> within;
    std::vector<CoarseExit> exits;
    std::vector<std::int32_t> border;
  };
  // A region's border nodes and the shortest walks among them within it.
  struct RegionGraph {
    std::vector<NodeRef> border;
    std::vector<std::vector<Edge>> within;
  };

  void build_area(const Map& map, std::size_t area);
  void link_exits(std::size_t area);
  void build_region(std::size_t region);
  void join_components();
  [[nodiscard]] std::size_t region_of_area(std::size_t area) const;
  [[nodiscard]] std::vector<std::int16_t> labels(const Map& map,
                                                 std::size_t area) const;
  [[nodiscard]] std::vector<CoarseCost> area_costs(const Map& map,
                                                   std::size_t area,
                                                   Cell from) const;
  [[nodiscard]] std::size_t slot_in(std::size_t area, Cell cell) const;
  [[nodiscard]] std::vector<std::optional<CoarseCost>> node_costs(
      const Map& map, std::size_t area, Cell from) const;
  [[nodiscard]] std::vector<std::pair<NodeRef, CoarseCost>> inserted(
      const Map& map, Cell cell) const;
  [[nodiscard]] std::optional<CoarseCost> over_areas(
      const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
      const std::vector<std::pair<NodeRef, CoarseCost>>& goals) const;
  [[nodiscard]] std::vector<std::pair<NodeRef, CoarseCost>> settle_region(
      const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
      std::size_t region) const;
  [[nodiscard]] std::optional<CoarseCost> over_regions(
      const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
      const std::vector<std::pair<NodeRef, CoarseCost>>& goals) const;
  [[nodiscard]] std::optional<CoarseCost> between(
      const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
      const std::vector<std::pair<NodeRef, CoarseCost>>& goals,
      bool by_regions) const;
  [[nodiscard]] Cell area_corner(std::size_t area) const;
  [[nodiscard]] Footprint area_size(std::size_t area) const;

  std::int32_t width_;
  std::int32_t height_;
  std::int32_t columns_;
  std::int32_t rows_;
  std::vector<AreaGraph> areas_;
  std::vector<RegionGraph> regions_;
  // Each area's components' first index among every component, and every
  // component's set, joined through the entrances.
  std::vector<std::size_t> first_component_;
  std::vector<std::size_t> joined_;
};

}  // namespace murim
