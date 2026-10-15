#include "area_fields.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
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
  return slot;
}

std::vector<FieldSeed> AreaFields::seeds(const Map& map,
                                         const CoarseGraph& graph,
                                         std::size_t area, Cell objective) {
  scratch_.clear();
  if (graph.area_of(objective) == area) {
    scratch_.push_back(FieldSeed{objective, 0});
  }
  const Objective aim{objective.x, objective.y};
  auto found = searches_.find(aim);
  if (found == searches_.end()) {
    found = searches_.emplace(aim, Search{graph.toward(map, objective), false})
                .first;
  }
  found->second.read = true;
  for (const CoarseExit& exit : graph.exits(area)) {
    const NodeRef far{exit.area, exit.far_node};
    // An exit whose far side reaches no walk to the objective is not seeded,
    // so a component it alone leads out of gets no step.
    const std::optional<CoarseCost> cost =
        graph.cost_from(found->second.search, far);
    if (!cost) {
      continue;
    }
    const Cell node = graph.node(far).cell;
    for (std::int32_t along = 0; along < exit.length; ++along) {
      const Cell seed{exit.first.x + (exit.along.x * along),
                      exit.first.y + (exit.along.y * along)};
      // Over open ground the cost to the objective is the octile cost, so a
      // walk across areas keeps to its line; elsewhere, the cost by the
      // exit's node (ADR 0043).
      const CoarseCost by_node = cost.value_or(0) + octile(seed, node);
      const CoarseCost straight = octile(seed, objective);
      scratch_.push_back(FieldSeed{seed, clear_line(map, seed, objective)
                                             ? std::min(by_node, straight)
                                             : by_node});
    }
  }
  return scratch_;
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
  for (auto each = searches_.begin(); each != searches_.end();) {
    if (each->second.read) {
      each->second.read = false;
      ++each;
    } else {
      each = searches_.erase(each);
    }
  }
  built_ = 0;
}

void AreaFields::clear() {
  for (const auto& [key, held] : held_) {
    free_.push_back(held.slot);
  }
  held_.clear();
  searches_.clear();
}

std::size_t AreaFields::held() const { return held_.size(); }

std::size_t AreaFields::built() const { return built_; }

}  // namespace murim
