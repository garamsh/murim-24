#include "area_fields.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "areas.hpp"
#include "coarse_graph.hpp"
#include "flow_field.hpp"
#include "map.hpp"

namespace murim {
namespace {

// -1, 0 or 1, as `value` is below, at or above 0.
std::int32_t sign(std::int32_t value) {
  if (value == 0) {
    return 0;
  }
  return value > 0 ? 1 : -1;
}

// Whether a person can walk from `from` to `goal` at the octile cost by the
// walker's own step rule: each step is one that keeps the walk at the octile
// cost, the one whose cell lies nearest the line from `from` to `goal`, the
// diagonal first among equals, and it opens only as a field's step does, a
// diagonal one only where both cells beside it are passable. So the octile
// cost it allows is the cost of a walk.
bool walks_its_line(const Map& map, Cell from, Cell goal) {
  const std::int64_t line_x = std::int64_t{goal.x} - from.x;
  const std::int64_t line_y = std::int64_t{goal.y} - from.y;
  const auto off_line = [&](Cell cell) {
    return std::abs((line_x * (std::int64_t{cell.y} - from.y)) -
                    (line_y * (std::int64_t{cell.x} - from.x)));
  };
  Cell cell = from;
  while (cell.x != goal.x || cell.y != goal.y) {
    const std::int32_t along_x = goal.x - cell.x;
    const std::int32_t along_y = goal.y - cell.y;
    const std::int32_t sign_x = sign(along_x);
    const std::int32_t sign_y = sign(along_y);
    Cell next{cell.x + sign_x, cell.y + sign_y};
    // A straight step along the longer side keeps the octile cost too, where
    // the sides differ.
    if (std::abs(along_x) != std::abs(along_y)) {
      const Cell straight = std::abs(along_x) > std::abs(along_y)
                                ? Cell{cell.x + sign_x, cell.y}
                                : Cell{cell.x, cell.y + sign_y};
      if (sign_x == 0 || sign_y == 0 || off_line(straight) < off_line(next)) {
        next = straight;
      }
    }
    const bool diagonal = next.x != cell.x && next.y != cell.y;
    if (!map.passable(next) ||
        (diagonal && (!map.passable(Cell{next.x, cell.y}) ||
                      !map.passable(Cell{cell.x, next.y})))) {
      return false;
    }
    cell = next;
  }
  return true;
}

}  // namespace

std::size_t AreaFields::field_for(const Map& map, const CoarseGraph& graph,
                                  Cell cell, Cell objective) {
  const std::size_t area = graph.area_of(cell);
  const Key key{area, objective.x, objective.y};
  standing_.push_back(key);
  columns_ = static_cast<std::size_t>(graph.columns());
  rows_ = static_cast<std::size_t>((map.height() + AREA_SIDE - 1) / AREA_SIDE);
  const auto found = held_.find(key);
  if (found != held_.end()) {
    return found->second;
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
  held_.emplace(key, slot);
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
    // Where a walk at the octile cost reaches the objective, that is its cost,
    // so a walk across areas keeps to its line (ADR 0043).
    scratch_.push_back(
        FieldSeed{seed, walks_its_line(map, seed, objective)
                            ? std::min(best, octile(seed, objective))
                            : best});
  }
}

const FlowField& AreaFields::field(std::size_t index) const {
  return pool_.at(index);
}

void AreaFields::end_tick() {
  // A field is held while a person bound for its objective stands in its area
  // or in one of the eight a step can cross into it from, so a walk stepping
  // back and forth across an area's side or corner steps by fields already
  // built.
  std::sort(standing_.begin(), standing_.end());
  standing_.erase(std::unique(standing_.begin(), standing_.end()),
                  standing_.end());
  for (auto each = held_.begin(); each != held_.end();) {
    if (stood_near(each->first)) {
      ++each;
    } else {
      free_.push_back(each->second);
      each = held_.erase(each);
    }
  }
  standing_.clear();
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

bool AreaFields::stood_near(const Key& key) const {
  const std::size_t area = std::get<0>(key);
  const std::size_t column = area % columns_;
  const std::size_t row = area / columns_;
  for (std::size_t near_row = row == 0 ? 0 : row - 1;
       near_row <= std::min(row + 1, rows_ - 1); ++near_row) {
    for (std::size_t near_column = column == 0 ? 0 : column - 1;
         near_column <= std::min(column + 1, columns_ - 1); ++near_column) {
      if (std::binary_search(standing_.begin(), standing_.end(),
                             Key{(near_row * columns_) + near_column,
                                 std::get<1>(key), std::get<2>(key)})) {
        return true;
      }
    }
  }
  return false;
}

void AreaFields::clear() {
  for (const auto& [key, slot] : held_) {
    free_.push_back(slot);
  }
  held_.clear();
  standing_.clear();
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
