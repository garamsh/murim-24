#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <set>
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

/// Returns the least a walk between two cells can cost: a diagonal step for
/// each cell both axes differ by, a straight one for the rest.
[[nodiscard]] CoarseCost octile(Cell from, Cell goal);

/// A walk's two ends.
struct Walk {
  Cell from;
  Cell to;
};

/// A search outward from an objective over the coarse graph's level of areas,
/// kept between queries: what it has settled, and what it would settle next.
struct GoalSearch {
  std::map<std::pair<std::size_t, std::size_t>, CoarseCost> cost;
  std::set<std::pair<std::size_t, std::size_t>> settled;
  std::priority_queue<
      std::pair<CoarseCost, std::pair<std::size_t, std::size_t>>,
      std::vector<std::pair<CoarseCost, std::pair<std::size_t, std::size_t>>>,
      std::greater<>>
      frontier;
};

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

  /// Returns whether a walk joins `walk`'s ends; a blocked cell is joined to
  /// whatever the cells it could step onto are.
  [[nodiscard]] bool joined(const Map& map, Walk walk) const;

  /// Starts a search outward from `objective`, from its area's nodes at
  /// their cost to it.
  [[nodiscard]] GoalSearch toward(const Map& map, Cell objective) const;

  /// Returns the cost of the shortest walk over the areas from node `from` to
  /// the objective `search` was started toward, settling only as far as it
  /// must; nothing when none joins them. The answer depends on the graph and
  /// the objective alone, however far `search` had gone.
  [[nodiscard]] std::optional<CoarseCost> cost_from(GoalSearch& search,
                                                    NodeRef from) const;

  /// Returns the cost of the shortest walk the graph finds between `walk`'s
  /// passable ends, over the regions when `by_regions` and over the areas
  /// alone otherwise; nothing when no walk joins them.
  [[nodiscard]] std::optional<CoarseCost> path_cost(const Map& map, Walk walk,
                                                    bool by_regions) const;

  /// Returns the ways out of area `area`, in node order.
  [[nodiscard]] const std::vector<CoarseExit>& exits(std::size_t area) const;

  [[nodiscard]] std::int32_t columns() const;
  [[nodiscard]] std::size_t area_of(Cell cell) const;
  [[nodiscard]] std::size_t node_count() const;
  [[nodiscard]] const CoarseNode& node(NodeRef ref) const;

 private:
  struct Edge {
    std::size_t to;
    CoarseCost cost;
  };
  // An area's components, each cell's component when it has more than one,
  // nodes, the edges among its nodes, its exits, and each node's index among
  // its region's border nodes, if it is one.
  struct AreaGraph {
    std::uint16_t components = 0;
    std::vector<std::int16_t> labels;
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
  [[nodiscard]] std::vector<std::size_t> sets_of(const Map& map,
                                                 Cell cell) const;
  [[nodiscard]] std::vector<std::int16_t> labels(const Map& map,
                                                 std::size_t area) const;
  [[nodiscard]] std::vector<CoarseCost> area_costs(const Map& map,
                                                   std::size_t area,
                                                   Cell from) const;
  [[nodiscard]] std::size_t slot_in(std::size_t area, Cell cell) const;
  [[nodiscard]] std::vector<std::optional<CoarseCost>> node_costs(
      const Map& map, std::size_t area, Cell from) const;
  // Nodes at a cost: a search's starts or goals, or what it settled.
  using Costs = std::vector<std::pair<NodeRef, CoarseCost>>;
  // A search's starts and goals.
  struct Terminals {
    Costs starts;
    Costs goals;
  };
  using Relax = std::function<void(NodeRef, CoarseCost)>;
  using Expand = std::function<void(NodeRef, const Relax&)>;

  [[nodiscard]] Costs inserted(const Map& map, Cell cell) const;
  void expand_areas(NodeRef ref, const Relax& relax) const;
  void expand_regions(NodeRef ref, const Relax& relax) const;
  [[nodiscard]] std::optional<CoarseCost> a_star(const Terminals& ends,
                                                 const Expand& expand) const;
  [[nodiscard]] Costs settle_region(const Costs& starts,
                                    std::size_t region) const;
  [[nodiscard]] Costs on_border(const Costs& settled) const;
  [[nodiscard]] std::optional<CoarseCost> between(const Terminals& ends,
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
