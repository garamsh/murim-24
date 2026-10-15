#include "coarse_graph.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "areas.hpp"
#include "map.hpp"

namespace murim {
namespace {

// The cost of a straight and of a diagonal step, as FlowField counts them.
constexpr CoarseCost STRAIGHT = 5;
constexpr CoarseCost DIAGONAL = 7;

constexpr std::array<std::array<std::int32_t, 2>, 8> DIRECTIONS{
    {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

constexpr std::int16_t UNLABELLED = -1;
constexpr CoarseCost UNREACHED = std::numeric_limits<CoarseCost>::max();

// A node's place in the search's order: its area, then its index in the area.
using Key = std::pair<std::size_t, std::size_t>;

Key key_of(NodeRef ref) { return Key{ref.area, ref.node}; }

// The least a walk between two cells can cost: a diagonal step for each cell
// both axes differ by, a straight one for the rest.
CoarseCost octile(Cell from, Cell to) {
  const auto across = static_cast<CoarseCost>(std::abs(to.x - from.x));
  const auto down = static_cast<CoarseCost>(std::abs(to.y - from.y));
  const CoarseCost diagonal = std::min(across, down);
  return (diagonal * DIAGONAL) +
         ((std::max(across, down) - diagonal) * STRAIGHT);
}

// Whether a walker within the box from `corner` of `size` may step from
// `cell` along (`column`, `row`) onto a passable cell, a diagonal step only
// where both cells beside it are passable too.
bool opens(const Map& map, Cell corner, Footprint size, Cell cell,
           std::int32_t column, std::int32_t row) {
  const auto inside = [&](Cell at) {
    return at.x >= corner.x && at.y >= corner.y &&
           at.x < corner.x + size.width && at.y < corner.y + size.height &&
           map.passable(at);
  };
  const Cell next{cell.x + column, cell.y + row};
  return inside(next) &&
         (column == 0 || row == 0 ||
          (inside(Cell{next.x, cell.y}) && inside(Cell{cell.x, next.y})));
}

}  // namespace

CoarseGraph::CoarseGraph(const Map& map)
    : width_(map.width()),
      height_(map.height()),
      columns_((width_ + AREA_SIDE - 1) / AREA_SIDE),
      rows_((height_ + AREA_SIDE - 1) / AREA_SIDE),
      areas_(static_cast<std::size_t>(columns_) *
             static_cast<std::size_t>(rows_)),
      regions_(
          static_cast<std::size_t>((columns_ + REGION_AREAS - 1) /
                                   REGION_AREAS) *
          static_cast<std::size_t>((rows_ + REGION_AREAS - 1) / REGION_AREAS)) {
  for (std::size_t area = 0; area < areas_.size(); ++area) {
    build_area(map, area);
  }
  for (std::size_t area = 0; area < areas_.size(); ++area) {
    link_exits(area);
  }
  for (std::size_t region = 0; region < regions_.size(); ++region) {
    build_region(region);
  }
  join_components();
}

void CoarseGraph::rebuild(const Map& map, const std::vector<Cell>& cells) {
  // The areas changed and their neighbours, whose entrances toward them
  // change; then every area whose exits name one of those areas' nodes.
  std::set<std::size_t> changed;
  for (const Cell cell : cells) {
    const std::size_t area = area_of(cell);
    const auto column =
        static_cast<std::int32_t>(area % static_cast<std::size_t>(columns_));
    const auto row =
        static_cast<std::int32_t>(area / static_cast<std::size_t>(columns_));
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
      for (std::int32_t dx = -1; dx <= 1; ++dx) {
        const bool beside = (dx == 0) != (dy == 0) || (dx == 0 && dy == 0);
        const std::int32_t x = column + dx;
        const std::int32_t y = row + dy;
        if (beside && x >= 0 && y >= 0 && x < columns_ && y < rows_) {
          changed.insert((static_cast<std::size_t>(y) *
                          static_cast<std::size_t>(columns_)) +
                         static_cast<std::size_t>(x));
        }
      }
    }
  }
  std::set<std::size_t> relinked = changed;
  for (const std::size_t area : changed) {
    build_area(map, area);
    for (const CoarseExit& exit : areas_.at(area).exits) {
      relinked.insert(exit.area);
    }
  }
  std::set<std::size_t> regions;
  for (const std::size_t area : relinked) {
    link_exits(area);
    regions.insert(region_of_area(area));
  }
  for (const std::size_t region : regions) {
    build_region(region);
  }
  join_components();
}

Cell CoarseGraph::area_corner(std::size_t area) const {
  const auto columns = static_cast<std::size_t>(columns_);
  return Cell{static_cast<std::int32_t>(area % columns) * AREA_SIDE,
              static_cast<std::int32_t>(area / columns) * AREA_SIDE};
}

Footprint CoarseGraph::area_size(std::size_t area) const {
  const Cell corner = area_corner(area);
  return Footprint{std::min(AREA_SIDE, width_ - corner.x),
                   std::min(AREA_SIDE, height_ - corner.y)};
}

std::size_t CoarseGraph::area_of(Cell cell) const {
  return (static_cast<std::size_t>(cell.y / AREA_SIDE) *
          static_cast<std::size_t>(columns_)) +
         static_cast<std::size_t>(cell.x / AREA_SIDE);
}

std::int32_t CoarseGraph::columns() const { return columns_; }

std::size_t CoarseGraph::node_count() const {
  std::size_t count = 0;
  for (const AreaGraph& area : areas_) {
    count += area.nodes.size();
  }
  return count;
}

const CoarseNode& CoarseGraph::node(NodeRef ref) const {
  return areas_.at(ref.area).nodes.at(ref.node);
}

std::size_t CoarseGraph::region_of_area(std::size_t area) const {
  const auto columns = static_cast<std::size_t>(columns_);
  const std::size_t region_columns =
      (columns + REGION_AREAS - 1) / static_cast<std::size_t>(REGION_AREAS);
  return ((area / columns / REGION_AREAS) * region_columns) +
         (area % columns / REGION_AREAS);
}

std::vector<std::int16_t> CoarseGraph::labels(const Map& map,
                                              std::size_t area) const {
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  const auto width = static_cast<std::size_t>(size.width);
  std::vector<std::int16_t> label(width * static_cast<std::size_t>(size.height),
                                  UNLABELLED);
  const auto slot = [&](Cell cell) {
    return (static_cast<std::size_t>(cell.y - corner.y) * width) +
           static_cast<std::size_t>(cell.x - corner.x);
  };
  std::int16_t next = 0;
  std::vector<Cell> waiting;
  for (Cell cell{0, corner.y}; cell.y < corner.y + size.height; ++cell.y) {
    for (cell.x = corner.x; cell.x < corner.x + size.width; ++cell.x) {
      if (!map.passable(cell) || label.at(slot(cell)) != UNLABELLED) {
        continue;
      }
      label.at(slot(cell)) = next;
      waiting.push_back(cell);
      while (!waiting.empty()) {
        const Cell at = waiting.back();
        waiting.pop_back();
        for (const auto& [column, row] : DIRECTIONS) {
          const Cell beside{at.x + column, at.y + row};
          if (opens(map, corner, size, at, column, row) &&
              label.at(slot(beside)) == UNLABELLED) {
            label.at(slot(beside)) = next;
            waiting.push_back(beside);
          }
        }
      }
      ++next;
    }
  }
  return label;
}

void CoarseGraph::build_area(const Map& map, std::size_t area) {
  AreaGraph made;
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  const std::vector<std::int16_t> label = labels(map, area);
  const auto slot = [&](Cell cell) {
    return (static_cast<std::size_t>(cell.y - corner.y) *
            static_cast<std::size_t>(size.width)) +
           static_cast<std::size_t>(cell.x - corner.x);
  };
  for (const std::int16_t each : label) {
    made.components = std::max<std::uint16_t>(
        made.components, static_cast<std::uint16_t>(each + 1));
  }
  // Each side's entrances: the runs of cells passable on this side and the
  // far one, from the side's first cell along it.
  struct Side {
    Cell first;
    Cell along;
    Cell outward;
    std::int32_t length;
  };
  const std::array<Side, 4> sides{
      {{corner, Cell{1, 0}, Cell{0, -1}, size.width},
       {Cell{corner.x, corner.y + size.height - 1}, Cell{1, 0}, Cell{0, 1},
        size.width},
       {corner, Cell{0, 1}, Cell{-1, 0}, size.height},
       {Cell{corner.x + size.width - 1, corner.y}, Cell{0, 1}, Cell{1, 0},
        size.height}}};
  for (const Side& side : sides) {
    const Cell beyond{side.first.x + side.outward.x,
                      side.first.y + side.outward.y};
    if (beyond.x < 0 || beyond.y < 0 || beyond.x >= width_ ||
        beyond.y >= height_) {
      continue;
    }
    std::int32_t run = 0;
    for (std::int32_t step = 0; step <= side.length; ++step) {
      const Cell here{side.first.x + (side.along.x * step),
                      side.first.y + (side.along.y * step)};
      const Cell there{here.x + side.outward.x, here.y + side.outward.y};
      const bool open =
          step < side.length && map.passable(here) && map.passable(there);
      if (open) {
        ++run;
        continue;
      }
      if (run > 0) {
        const std::int32_t start = step - run;
        const std::int32_t middle = start + ((run - 1) / 2);
        const Cell cell{side.first.x + (side.along.x * middle),
                        side.first.y + (side.along.y * middle)};
        made.nodes.push_back(
            CoarseNode{cell, static_cast<std::uint16_t>(label.at(slot(cell)))});
        const Cell far_first{
            side.first.x + (side.along.x * start) + side.outward.x,
            side.first.y + (side.along.y * start) + side.outward.y};
        made.exits.push_back(CoarseExit{
            made.nodes.size() - 1,
            area_of(Cell{cell.x + side.outward.x, cell.y + side.outward.y}), 0,
            far_first, side.along, run});
      }
      run = 0;
    }
  }
  made.within.resize(made.nodes.size());
  // node_costs reads the area's nodes, so the new ones are put in place first.
  areas_.at(area) = std::move(made);
  AreaGraph& built = areas_.at(area);
  for (std::size_t from = 0; from < built.nodes.size(); ++from) {
    const std::vector<std::optional<CoarseCost>> costs =
        node_costs(map, area, built.nodes.at(from).cell);
    for (std::size_t to = 0; to < built.nodes.size(); ++to) {
      if (to != from && costs.at(to)) {
        built.within.at(from).push_back(Edge{to, costs.at(to).value_or(0)});
      }
    }
  }
  built.border.assign(built.nodes.size(), -1);
}

void CoarseGraph::link_exits(std::size_t area) {
  for (CoarseExit& exit : areas_.at(area).exits) {
    const Cell middle{exit.first.x + (exit.along.x * ((exit.length - 1) / 2)),
                      exit.first.y + (exit.along.y * ((exit.length - 1) / 2))};
    const std::vector<CoarseNode>& far = areas_.at(exit.area).nodes;
    const auto found =
        std::find_if(far.begin(), far.end(), [middle](const CoarseNode& each) {
          return each.cell.x == middle.x && each.cell.y == middle.y;
        });
    exit.far_node = static_cast<std::size_t>(found - far.begin());
  }
}

std::vector<CoarseCost> CoarseGraph::area_costs(const Map& map,
                                                std::size_t area,
                                                Cell from) const {
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  const auto width = static_cast<std::size_t>(size.width);
  const auto slot = [&](Cell cell) {
    return (static_cast<std::size_t>(cell.y - corner.y) * width) +
           static_cast<std::size_t>(cell.x - corner.x);
  };
  std::vector<CoarseCost> cost(width * static_cast<std::size_t>(size.height),
                               UNREACHED);
  using Entry = std::pair<CoarseCost, std::size_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  cost.at(slot(from)) = 0;
  frontier.emplace(0, slot(from));
  while (!frontier.empty()) {
    const auto [held, at] = frontier.top();
    frontier.pop();
    if (held != cost.at(at)) {
      continue;
    }
    const Cell cell{corner.x + static_cast<std::int32_t>(at % width),
                    corner.y + static_cast<std::int32_t>(at / width)};
    for (const auto& [column, row] : DIRECTIONS) {
      if (!opens(map, corner, size, cell, column, row)) {
        continue;
      }
      const Cell next{cell.x + column, cell.y + row};
      const CoarseCost through =
          held + (column != 0 && row != 0 ? DIAGONAL : STRAIGHT);
      if (through < cost.at(slot(next))) {
        cost.at(slot(next)) = through;
        frontier.emplace(through, slot(next));
      }
    }
  }
  return cost;
}

std::size_t CoarseGraph::slot_in(std::size_t area, Cell cell) const {
  const Cell corner = area_corner(area);
  return (static_cast<std::size_t>(cell.y - corner.y) *
          static_cast<std::size_t>(area_size(area).width)) +
         static_cast<std::size_t>(cell.x - corner.x);
}

std::vector<std::optional<CoarseCost>> CoarseGraph::node_costs(
    const Map& map, std::size_t area, Cell from) const {
  const std::vector<CoarseCost> cost = area_costs(map, area, from);
  std::vector<std::optional<CoarseCost>> found;
  for (const CoarseNode& node : areas_.at(area).nodes) {
    const CoarseCost reached = cost.at(slot_in(area, node.cell));
    found.push_back(reached == UNREACHED ? std::nullopt
                                         : std::optional<CoarseCost>{reached});
  }
  return found;
}

void CoarseGraph::build_region(std::size_t region) {
  RegionGraph made;
  for (std::size_t area = 0; area < areas_.size(); ++area) {
    if (region_of_area(area) != region) {
      continue;
    }
    AreaGraph& held = areas_.at(area);
    held.border.assign(held.nodes.size(), -1);
    for (const CoarseExit& exit : held.exits) {
      if (region_of_area(exit.area) != region &&
          held.border.at(exit.node) < 0) {
        held.border.at(exit.node) =
            static_cast<std::int32_t>(made.border.size());
        made.border.push_back(NodeRef{area, exit.node});
      }
    }
  }
  // Border nodes are in area order, then by node, as the numbering has it.
  made.within.resize(made.border.size());
  regions_.at(region) = std::move(made);
  RegionGraph& built = regions_.at(region);
  for (std::size_t from = 0; from < built.border.size(); ++from) {
    for (const auto& [ref, cost] :
         settle_region({{built.border.at(from), 0}}, region)) {
      const std::int32_t index = areas_.at(ref.area).border.at(ref.node);
      if (index >= 0 && static_cast<std::size_t>(index) != from) {
        built.within.at(from).push_back(
            Edge{static_cast<std::size_t>(index), cost});
      }
    }
  }
}

void CoarseGraph::join_components() {
  first_component_.assign(areas_.size() + 1, 0);
  for (std::size_t area = 0; area < areas_.size(); ++area) {
    first_component_.at(area + 1) =
        first_component_.at(area) + areas_.at(area).components;
  }
  joined_.resize(first_component_.back());
  for (std::size_t each = 0; each < joined_.size(); ++each) {
    joined_.at(each) = each;
  }
  const auto root = [this](std::size_t each) {
    while (joined_.at(each) != each) {
      joined_.at(each) = joined_.at(joined_.at(each));
      each = joined_.at(each);
    }
    return each;
  };
  for (std::size_t area = 0; area < areas_.size(); ++area) {
    for (const CoarseExit& exit : areas_.at(area).exits) {
      const std::size_t here = first_component_.at(area) +
                               areas_.at(area).nodes.at(exit.node).component;
      const std::size_t there =
          first_component_.at(exit.area) +
          areas_.at(exit.area).nodes.at(exit.far_node).component;
      const std::size_t one = root(here);
      const std::size_t other = root(there);
      joined_.at(std::max(one, other)) = std::min(one, other);
    }
  }
  for (std::size_t each = 0; each < joined_.size(); ++each) {
    joined_.at(each) = root(each);
  }
}

std::uint16_t CoarseGraph::component_of(const Map& map, Cell cell) const {
  const std::size_t area = area_of(cell);
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  return static_cast<std::uint16_t>(
      labels(map, area).at((static_cast<std::size_t>(cell.y - corner.y) *
                            static_cast<std::size_t>(size.width)) +
                           static_cast<std::size_t>(cell.x - corner.x)));
}

bool CoarseGraph::joined(const Map& map, Cell from, Cell to) const {
  const std::size_t one =
      first_component_.at(area_of(from)) + component_of(map, from);
  const std::size_t other =
      first_component_.at(area_of(to)) + component_of(map, to);
  return joined_.at(one) == joined_.at(other);
}

std::vector<CoarseExit> CoarseGraph::exits(std::size_t area,
                                           std::uint16_t component) const {
  std::vector<CoarseExit> found;
  const AreaGraph& held = areas_.at(area);
  for (const CoarseExit& exit : held.exits) {
    if (held.nodes.at(exit.node).component == component) {
      found.push_back(exit);
    }
  }
  return found;
}

std::vector<std::pair<NodeRef, CoarseCost>> CoarseGraph::inserted(
    const Map& map, Cell cell) const {
  const std::size_t area = area_of(cell);
  const std::vector<std::optional<CoarseCost>> costs =
      node_costs(map, area, cell);
  std::vector<std::pair<NodeRef, CoarseCost>> found;
  for (std::size_t node = 0; node < costs.size(); ++node) {
    if (costs.at(node)) {
      found.emplace_back(NodeRef{area, node}, costs.at(node).value_or(0));
    }
  }
  return found;
}

std::optional<CoarseCost> CoarseGraph::over_areas(
    const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
    const std::vector<std::pair<NodeRef, CoarseCost>>& goals) const {
  // A* over the nodes, ordered by estimate, then by the nodes' order; its
  // estimate the least octile cost to a goal node, which never overestimates.
  std::map<Key, CoarseCost> goal_cost;
  for (const auto& [ref, cost] : goals) {
    goal_cost.emplace(key_of(ref), cost);
  }
  const auto estimate = [&](NodeRef ref) {
    CoarseCost least = std::numeric_limits<CoarseCost>::max();
    for (const auto& [goal, cost] : goals) {
      least = std::min(least, octile(node(ref).cell, node(goal).cell));
    }
    return least;
  };
  using Entry = std::pair<CoarseCost, Key>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  std::map<Key, CoarseCost> cost;
  for (const auto& [ref, held] : starts) {
    const auto [place, fresh] = cost.emplace(key_of(ref), held);
    if (fresh || held < place->second) {
      place->second = held;
      frontier.emplace(held + estimate(ref), key_of(ref));
    }
  }
  std::optional<CoarseCost> best;
  while (!frontier.empty()) {
    const auto [bound, at] = frontier.top();
    frontier.pop();
    if (best && bound >= *best) {
      break;
    }
    const NodeRef ref{at.first, at.second};
    const CoarseCost held = cost.at(at);
    if (bound != held + estimate(ref)) {
      continue;
    }
    const auto goal = goal_cost.find(at);
    if (goal != goal_cost.end()) {
      best = std::min(best.value_or(std::numeric_limits<CoarseCost>::max()),
                      held + goal->second);
    }
    const AreaGraph& area = areas_.at(ref.area);
    const auto relax = [&](NodeRef next, CoarseCost step) {
      const CoarseCost through = held + step;
      const auto [place, fresh] = cost.emplace(key_of(next), through);
      if (fresh || through < place->second) {
        place->second = through;
        frontier.emplace(through + estimate(next), key_of(next));
      }
    };
    for (const Edge& edge : area.within.at(ref.node)) {
      relax(NodeRef{ref.area, edge.to}, edge.cost);
    }
    for (const CoarseExit& exit : area.exits) {
      if (exit.node == ref.node) {
        relax(NodeRef{exit.area, exit.far_node}, STRAIGHT);
      }
    }
  }
  return best;
}

std::vector<std::pair<NodeRef, CoarseCost>> CoarseGraph::settle_region(
    const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
    std::size_t region) const {
  using Entry = std::pair<CoarseCost, Key>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  std::map<Key, CoarseCost> cost;
  for (const auto& [ref, held] : starts) {
    const auto [place, fresh] = cost.emplace(key_of(ref), held);
    if (fresh || held < place->second) {
      place->second = held;
      frontier.emplace(held, key_of(ref));
    }
  }
  while (!frontier.empty()) {
    const CoarseCost held = frontier.top().first;
    const Key at = frontier.top().second;
    frontier.pop();
    if (held != cost.at(at)) {
      continue;
    }
    const AreaGraph& area = areas_.at(at.first);
    const auto relax = [&](NodeRef next, CoarseCost step) {
      if (region_of_area(next.area) != region) {
        return;
      }
      const CoarseCost through = held + step;
      const auto [place, fresh] = cost.emplace(key_of(next), through);
      if (fresh || through < place->second) {
        place->second = through;
        frontier.emplace(through, key_of(next));
      }
    };
    for (const Edge& edge : area.within.at(at.second)) {
      relax(NodeRef{at.first, edge.to}, edge.cost);
    }
    for (const CoarseExit& exit : area.exits) {
      if (exit.node == at.second) {
        relax(NodeRef{exit.area, exit.far_node}, STRAIGHT);
      }
    }
  }
  std::vector<std::pair<NodeRef, CoarseCost>> settled;
  for (const auto& [at, held] : cost) {
    settled.emplace_back(NodeRef{at.first, at.second}, held);
  }
  return settled;
}

std::optional<CoarseCost> CoarseGraph::over_regions(
    const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
    const std::vector<std::pair<NodeRef, CoarseCost>>& goals) const {
  // The start's and the goal's region border nodes, at their cost from the
  // start and to the goal within their regions; then A* over the regions'
  // border nodes, through the shortest walks within each region and the
  // entrances across their borders, ordered by estimate and then by the
  // nodes' order.
  const auto bordering = [this](const auto& settled) {
    std::vector<std::pair<NodeRef, CoarseCost>> border;
    for (const auto& [ref, cost] : settled) {
      if (areas_.at(ref.area).border.at(ref.node) >= 0) {
        border.emplace_back(ref, cost);
      }
    }
    return border;
  };
  const auto from = bordering(
      settle_region(starts, region_of_area(starts.front().first.area)));
  const auto to =
      bordering(settle_region(goals, region_of_area(goals.front().first.area)));
  std::map<Key, CoarseCost> goal_cost;
  for (const auto& [ref, cost] : to) {
    goal_cost.emplace(key_of(ref), cost);
  }
  const auto estimate = [&](NodeRef ref) {
    CoarseCost least = std::numeric_limits<CoarseCost>::max();
    for (const auto& [goal, held] : to) {
      least = std::min(least, octile(node(ref).cell, node(goal).cell));
    }
    return least;
  };
  using Entry = std::pair<CoarseCost, Key>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  std::map<Key, CoarseCost> cost;
  for (const auto& [ref, held] : from) {
    cost[key_of(ref)] = held;
    frontier.emplace(held + estimate(ref), key_of(ref));
  }
  std::optional<CoarseCost> best;
  while (!frontier.empty()) {
    const auto [bound, at] = frontier.top();
    frontier.pop();
    if (best && bound >= *best) {
      break;
    }
    const NodeRef ref{at.first, at.second};
    const CoarseCost held = cost.at(at);
    if (bound != held + estimate(ref)) {
      continue;
    }
    const auto goal = goal_cost.find(at);
    if (goal != goal_cost.end()) {
      best = std::min(best.value_or(std::numeric_limits<CoarseCost>::max()),
                      held + goal->second);
    }
    const auto relax = [&](NodeRef next, CoarseCost step) {
      const CoarseCost through = held + step;
      const auto [place, fresh] = cost.emplace(key_of(next), through);
      if (fresh || through < place->second) {
        place->second = through;
        frontier.emplace(through + estimate(next), key_of(next));
      }
    };
    const std::size_t region = region_of_area(ref.area);
    const RegionGraph& graph = regions_.at(region);
    const auto index =
        static_cast<std::size_t>(areas_.at(ref.area).border.at(ref.node));
    for (const Edge& edge : graph.within.at(index)) {
      relax(graph.border.at(edge.to), edge.cost);
    }
    for (const CoarseExit& exit : areas_.at(ref.area).exits) {
      if (exit.node == ref.node && region_of_area(exit.area) != region) {
        relax(NodeRef{exit.area, exit.far_node}, STRAIGHT);
      }
    }
  }
  return best;
}

std::optional<CoarseCost> CoarseGraph::between(
    const std::vector<std::pair<NodeRef, CoarseCost>>& starts,
    const std::vector<std::pair<NodeRef, CoarseCost>>& goals,
    bool by_regions) const {
  if (starts.empty() || goals.empty()) {
    return std::nullopt;
  }
  const bool apart = region_of_area(starts.front().first.area) !=
                     region_of_area(goals.front().first.area);
  return by_regions && apart ? over_regions(starts, goals)
                             : over_areas(starts, goals);
}

std::optional<CoarseCost> CoarseGraph::cost_to(const Map& map, NodeRef from,
                                               Cell to) const {
  return between({{from, 0}}, inserted(map, to), true);
}

std::optional<CoarseCost> CoarseGraph::path_cost(const Map& map, Cell from,
                                                 Cell to,
                                                 bool by_regions) const {
  if (!joined(map, from, to)) {
    return std::nullopt;
  }
  if (area_of(from) == area_of(to) &&
      component_of(map, from) == component_of(map, to)) {
    // Within one component of one area, the walk within it, as a field
    // restricted to the area finds it.
    return area_costs(map, area_of(from), from).at(slot_in(area_of(from), to));
  }
  return between(inserted(map, from), inserted(map, to), by_regions);
}

}  // namespace murim
