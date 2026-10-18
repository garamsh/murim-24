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

// Whether a blocked `cell` can be one walks_its_line() steps onto or past
// from `from` to `goal`: every cell it steps onto lies within half a cell of
// the line, by the larger side, and each cell beside a diagonal step within
// one more.
bool may_cross(Cell from, Cell goal, Cell cell) {
  const std::int64_t line_x = std::int64_t{goal.x} - from.x;
  const std::int64_t line_y = std::int64_t{goal.y} - from.y;
  const std::int64_t off_line =
      std::abs((line_x * (std::int64_t{cell.y} - from.y)) -
               (line_y * (std::int64_t{cell.x} - from.x)));
  return cell.x >= std::min(from.x, goal.x) &&
         cell.x <= std::max(from.x, goal.x) &&
         cell.y >= std::min(from.y, goal.y) &&
         cell.y <= std::max(from.y, goal.y) &&
         off_line <= 2 * std::max(std::abs(line_x), std::abs(line_y));
}

// The cell of the rectangle from `corner` of `size` nearest `cell` along
// each axis.
Cell nearest_in(Cell corner, Footprint size, Cell cell) {
  return Cell{std::clamp(cell.x, corner.x, corner.x + size.width - 1),
              std::clamp(cell.y, corner.y, corner.y + size.height - 1)};
}

}  // namespace

AreaFields::Window AreaFields::window_of(const Map& map, Cell area_corner) {
  const Cell corner{std::max(0, area_corner.x - 1),
                    std::max(0, area_corner.y - 1)};
  const Cell far{std::min(map.width(), area_corner.x + AREA_SIDE + 1),
                 std::min(map.height(), area_corner.y + AREA_SIDE + 1)};
  return Window{corner, Footprint{far.x - corner.x, far.y - corner.y}};
}

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
  const Window window = window_of(map, Cell{(cell.x / AREA_SIDE) * AREA_SIDE,
                                            (cell.y / AREA_SIDE) * AREA_SIDE});
  const std::vector<FieldSeed>& sown = seeds(map, graph, area, objective);
  std::size_t slot = 0;
  if (free_.empty()) {
    slot = pool_.size();
    pool_.emplace_back(map, window.corner, window.size, sown, objective);
    sown_.emplace_back();
  } else {
    slot = free_.back();
    free_.pop_back();
    pool_.at(slot).rebuild(map, window.corner, window.size, sown, objective);
  }
  sown_.at(slot) = Sown{sown, sowing_};
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
  sowing_.clear();
  if (graph.area_of(objective) == area) {
    scratch_.push_back(FieldSeed{objective, 0});
    sowing_.push_back(Sowing{0, true});
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
      onward.emplace_back(&walks(map, graph, NodeRef{exit.area, node}), *cost);
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
    const bool line = walks_its_line(map, seed, objective);
    scratch_.push_back(
        FieldSeed{seed, line ? std::min(best, octile(seed, objective)) : best});
    sowing_.push_back(Sowing{best, line});
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
    each = beside.count(std::get<0>(each->first)) == 0 ? walks_.erase(each)
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

void AreaFields::changed(const Map& map, const CoarseGraph& graph,
                         const std::vector<Cell>& cells) {
  // A search reads the whole graph, and a walk within an area its cells.
  searches_.clear();
  std::set<std::size_t> holding;
  for (const Cell cell : cells) {
    holding.insert(graph.area_of(cell));
  }
  for (auto each = walks_.begin(); each != walks_.end();) {
    each = holding.count(std::get<0>(each->first)) == 0 ? std::next(each)
                                                        : walks_.erase(each);
  }
  // The graph changed only within the areas holding a cell and a cell round
  // them, where a neighbour's entrance toward them lies: each such area's
  // window.
  const auto columns = static_cast<std::size_t>(graph.columns());
  const auto area_window = [&map, columns](std::size_t area) {
    return window_of(
        map, Cell{static_cast<std::int32_t>(area % columns) * AREA_SIDE,
                  static_cast<std::int32_t>(area / columns) * AREA_SIDE});
  };
  std::vector<Window> rebuilt;
  rebuilt.reserve(holding.size());
  for (const std::size_t area : holding) {
    rebuilt.push_back(area_window(area));
  }
  const auto same = [](const FieldSeed& one, const FieldSeed& other) {
    return one.cell.x == other.cell.x && one.cell.y == other.cell.y &&
           one.cost == other.cost;
  };
  for (auto each = held_.begin(); each != held_.end();) {
    const auto& [area, objective_x, objective_y] = each->first;
    const Cell objective{objective_x, objective_y};
    const Window window = area_window(area);
    const bool covered =
        std::any_of(cells.begin(), cells.end(), [&window](Cell cell) {
          return nearest_in(window.corner, window.size, cell).x == cell.x &&
                 nearest_in(window.corner, window.size, cell).y == cell.y;
        });
    Sown& was = sown_.at(each->second);
    bool kept = !covered && unchanged(map, was, objective, cells, rebuilt);
    if (!covered && !kept) {
      const std::vector<FieldSeed>& now = seeds(map, graph, area, objective);
      kept = std::equal(now.begin(), now.end(), was.seeds.begin(),
                        was.seeds.end(), same);
      if (kept) {
        was.sowing = sowing_;
      }
    }
    if (kept) {
      ++each;
    } else {
      free_.push_back(each->second);
      each = held_.erase(each);
    }
  }
}

bool AreaFields::unchanged(const Map& map, const Sown& sown, Cell objective,
                           const std::vector<Cell>& cells,
                           const std::vector<Window>& rebuilt) {
  for (std::size_t index = 0; index < sown.seeds.size(); ++index) {
    const Cell seed = sown.seeds.at(index).cell;
    const Sowing& sowing = sown.sowing.at(index);
    if (sowing.line) {
      // A seed whose line is walked costs the octile cost, whatever its walk
      // over the graph, while no blocked cell stops that line.
      const bool stopped =
          std::any_of(
              cells.begin(), cells.end(),
              [&](Cell cell) { return may_cross(seed, objective, cell); }) &&
          !walks_its_line(map, seed, objective);
      if (stopped) {
        return false;
      }
      continue;
    }
    // A walk over the graph that changed passes a rebuilt window, so it
    // costs at least the octile cost to the window and from it to the
    // objective; a seed's walk cheaper than every such walk is unchanged.
    const bool cheaper =
        std::all_of(rebuilt.begin(), rebuilt.end(), [&](const Window& window) {
          return sowing.through <
                 octile(seed, nearest_in(window.corner, window.size, seed)) +
                     octile(nearest_in(window.corner, window.size, objective),
                            objective);
        });
    if (!cheaper) {
      return false;
    }
  }
  return true;
}

const std::vector<CoarseCost>& AreaFields::walks(const Map& map,
                                                 const CoarseGraph& graph,
                                                 NodeRef node) {
  const Cell from = graph.node(node).cell;
  const Start start{node.area, from.x, from.y};
  auto found = walks_.find(start);
  if (found == walks_.end()) {
    found =
        walks_.emplace(start, graph.walks_within(map, node.area, from)).first;
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
