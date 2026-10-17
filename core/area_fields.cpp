#include "area_fields.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "areas.hpp"
#include "coarse_graph.hpp"
#include "flow_field.hpp"
#include "map.hpp"

namespace murim {
namespace {

// Whether the segment between the centres of `from` and `goal` crosses
// only passable cells, read at two points a cell along it.
bool clear_line(const Map& map, Cell from, Cell goal) {
  const Position start = centre_of(from);
  const Position end = centre_of(goal);
  const std::int64_t points =
      2 * std::max<std::int64_t>(std::abs(goal.x - from.x),
                                 std::abs(goal.y - from.y));
  const std::int64_t parts = std::max<std::int64_t>(points, 1);
  for (std::int64_t point = 0; point <= points; ++point) {
    const Position along{
        static_cast<std::int32_t>(
            start.x + ((std::int64_t{end.x} - start.x) * point / parts)),
        static_cast<std::int32_t>(
            start.y + ((std::int64_t{end.y} - start.y) * point / parts))};
    if (!map.passable(cell_of(along))) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::size_t AreaFields::field_for(const Map& map, const CoarseGraph& graph,
                                  Cell cell, Cell objective) {
  const std::size_t area = graph.area_of(cell);
  const Key key{area, objective.x, objective.y};
  const auto found = held_.find(key);
  if (found != held_.end()) {
    found->second.asked = true;
    return found->second.slot;
  }
  // The area and a cell's margin round it, on the map.
  const Cell area_corner{(cell.x / AREA_SIDE) * AREA_SIDE,
                         (cell.y / AREA_SIDE) * AREA_SIDE};
  const Cell corner{std::max(0, area_corner.x - 1),
                    std::max(0, area_corner.y - 1)};
  const Cell far{std::min(map.width(), area_corner.x + AREA_SIDE + 1),
                 std::min(map.height(), area_corner.y + AREA_SIDE + 1)};
  const Footprint size{far.x - corner.x, far.y - corner.y};
  const std::vector<FieldSeed>& sown = seeds(map, graph, area, objective);
  std::size_t slot = 0;
  if (free_.empty()) {
    slot = pool_.size();
    pool_.emplace_back(map, corner, size, sown, objective);
  } else {
    slot = free_.back();
    free_.pop_back();
    pool_.at(slot).rebuild(map, corner, size, sown, objective);
  }
  held_.emplace(key, Held{slot, true});
  ++built_;
  ++built_in_all_;
  return slot;
}

const std::vector<FieldSeed>& AreaFields::seeds(const Map& map,
                                                const CoarseGraph& graph,
                                                std::size_t area,
                                                Cell objective) {
  scratch_.clear();
  if (graph.area_of(objective) == area) {
    scratch_.push_back(FieldSeed{objective, 0});
  }
  const Objective aim{objective.x, objective.y};
  auto found = searches_.find(aim);
  if (found == searches_.end()) {
    found =
        searches_.emplace(aim, Search{graph.toward(map, objective), false, {}})
            .first;
  }
  Search& held = found->second;
  held.read = true;
  columns_ = static_cast<std::size_t>(graph.columns());
  for (const CoarseExit& exit : graph.exits(area)) {
    sow(map, graph, exit, objective, held);
  }
  return scratch_;
}

void AreaFields::sow(const Map& map, const CoarseGraph& graph,
                     const CoarseExit& exit, Cell objective, Search& held) {
  // Each cell on the far side is seeded at its walk within the far area to
  // the nearest of that area's nodes by the walk and their cost to the
  // objective, or to the objective when it lies there, so a walk crosses
  // where the far area's interior leads rather than at the entrance's
  // middle. A cell no such walk reaches is not seeded, so a component it
  // alone leads out of gets no step.
  std::vector<std::pair<const std::vector<CoarseCost>*, CoarseCost>> onward;
  for (std::size_t node = 0; node < graph.nodes_in(exit.area); ++node) {
    if (const std::optional<CoarseCost> cost =
            graph.cost_from(held.search, NodeRef{exit.area, node})) {
      onward.emplace_back(&walks(map, graph, Node{exit.area, node}), *cost);
    }
  }
  const bool near = graph.area_of(objective) == exit.area;
  if (near && held.near.empty()) {
    held.near = graph.walks_within(map, exit.area, objective);
  }
  for (std::int32_t along = 0; along < exit.length; ++along) {
    const Cell seed{exit.first.x + (exit.along.x * along),
                    exit.first.y + (exit.along.y * along)};
    const std::size_t slot = graph.slot_in(exit.area, seed);
    CoarseCost best = near ? held.near.at(slot) : CoarseGraph::NO_WALK;
    for (const auto& [walked, cost] : onward) {
      if (walked->at(slot) != CoarseGraph::NO_WALK) {
        best = std::min(best, cost + walked->at(slot));
      }
    }
    if (best == CoarseGraph::NO_WALK) {
      continue;
    }
    // Over open ground the cost to the objective is the octile cost, so a
    // walk across areas keeps to its line (ADR 0043).
    scratch_.push_back(
        FieldSeed{seed, clear_line(map, seed, objective)
                            ? std::min(best, octile(seed, objective))
                            : best});
  }
}

const FlowField& AreaFields::field(std::size_t index) const {
  return pool_.at(index);
}

void AreaFields::end_tick() {
  for (auto each = held_.begin(); each != held_.end();) {
    if (each->second.asked) {
      each->second.asked = false;
      ++each;
    } else {
      free_.push_back(each->second.slot);
      each = held_.erase(each);
    }
  }
  // A search is a function of the map, the graph and its objective alone, so
  // keeping it while a field toward its objective is held, for the next build
  // toward it, changes no cost.
  for (const auto& [key, held] : held_) {
    const auto search =
        searches_.find(Objective{std::get<1>(key), std::get<2>(key)});
    if (search != searches_.end()) {
      search->second.read = true;
    }
  }
  for (auto each = searches_.begin(); each != searches_.end();) {
    if (each->second.read) {
      each->second.read = false;
      ++each;
    } else {
      each = searches_.erase(each);
    }
  }
  // The walks within an area are kept while a held field's area is that area
  // or one beside it, whose fields they seed.
  std::set<std::size_t> beside;
  for (const auto& [key, held] : held_) {
    const std::size_t area = std::get<0>(key);
    beside.insert(area);
    beside.insert(area + 1);
    beside.insert(area + columns_);
    if (area >= 1) {
      beside.insert(area - 1);
    }
    if (area >= columns_) {
      beside.insert(area - columns_);
    }
  }
  for (auto each = walks_.begin(); each != walks_.end();) {
    each = beside.count(each->first.first) == 0 ? walks_.erase(each)
                                                : std::next(each);
  }
  built_ = 0;
}

void AreaFields::clear() {
  for (const auto& [key, held] : held_) {
    free_.push_back(held.slot);
  }
  held_.clear();
  searches_.clear();
  walks_.clear();
}

const std::vector<CoarseCost>& AreaFields::walks(const Map& map,
                                                 const CoarseGraph& graph,
                                                 Node node) {
  auto found = walks_.find(node);
  if (found == walks_.end()) {
    found = walks_
                .emplace(node,
                         graph.walks_within(
                             map, node.first,
                             graph.node(NodeRef{node.first, node.second}).cell))
                .first;
  }
  return found->second;
}

std::size_t AreaFields::walk_bytes() const {
  std::size_t bytes = 0;
  for (const auto& [node, walked] : walks_) {
    bytes += walked.size() * sizeof(CoarseCost);
  }
  for (const auto& [objective, held] : searches_) {
    bytes += held.near.size() * sizeof(CoarseCost);
  }
  return bytes;
}

std::size_t AreaFields::held() const { return held_.size(); }

std::size_t AreaFields::built() const { return built_; }

std::size_t AreaFields::built_in_all() const { return built_in_all_; }

std::vector<const GoalSearch*> AreaFields::searches() const {
  std::vector<const GoalSearch*> held;
  held.reserve(searches_.size());
  for (const auto& [objective, search] : searches_) {
    held.push_back(&search.search);
  }
  return held;
}

}  // namespace murim
