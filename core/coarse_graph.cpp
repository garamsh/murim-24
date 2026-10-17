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

// Whether a walker within the box from `corner` of `size` may step from
// `cell` along (`column`, `row`) onto a passable cell, a diagonal step only
// where both cells beside it are passable too.
bool opens(const Map& map, Cell corner, Footprint size, Cell cell,
           std::int32_t column, std::int32_t row) {
  const auto inside = [&](Cell place) {
    return place.x >= corner.x && place.y >= corner.y &&
           place.x < corner.x + size.width &&
           place.y < corner.y + size.height && map.passable(place);
  };
  const Cell next{cell.x + column, cell.y + row};
  return inside(next) &&
         (column == 0 || row == 0 ||
          (inside(Cell{next.x, cell.y}) && inside(Cell{cell.x, next.y})));
}

}  // namespace

CoarseCost octile(Cell from, Cell goal) {
  const auto across = static_cast<CoarseCost>(std::abs(goal.x - from.x));
  const auto down = static_cast<CoarseCost>(std::abs(goal.y - from.y));
  const CoarseCost diagonal = std::min(across, down);
  return (diagonal * DIAGONAL) +
         ((std::max(across, down) - diagonal) * STRAIGHT);
}

CoarseGraph::CoarseGraph(const Map& map, AreaCosts costs)
    : costs_(costs),
      width_(map.width()),
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
  number();
  join_components();
}

namespace {

bool same(Cell one, Cell other) { return one.x == other.x && one.y == other.y; }

}  // namespace

CoarseGraph::Undo CoarseGraph::rebuild(const Map& map,
                                       const std::vector<Cell>& cells) {
  // The areas holding a changed cell are rebuilt, and their neighbours too,
  // whose entrances toward them change; every neighbour of a rebuilt area is
  // relinked, since a rebuilt area's nodes are numbered afresh.
  const auto around = [this](std::size_t area, std::set<std::size_t>& into) {
    const auto column =
        static_cast<std::int32_t>(area % static_cast<std::size_t>(columns_));
    const auto row =
        static_cast<std::int32_t>(area / static_cast<std::size_t>(columns_));
    const std::array<std::array<std::int32_t, 2>, 5> beside{
        {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
    for (const auto& [across, down] : beside) {
      const std::int32_t next_column = column + across;
      const std::int32_t next_row = row + down;
      if (next_column >= 0 && next_row >= 0 && next_column < columns_ &&
          next_row < rows_) {
        into.insert((static_cast<std::size_t>(next_row) *
                     static_cast<std::size_t>(columns_)) +
                    static_cast<std::size_t>(next_column));
      }
    }
  };
  std::set<std::size_t> changed;
  for (const Cell cell : cells) {
    around(area_of(cell), changed);
  }
  std::set<std::size_t> relinked;
  for (const std::size_t area : changed) {
    around(area, relinked);
  }
  Undo undo;
  for (const std::size_t area : relinked) {
    undo.areas_.emplace_back(area, areas_.at(area));
  }
  // An area that keeps its components, its nodes with theirs, and its exits
  // joins the same components as before.
  const auto kept = [](const AreaGraph& before, const AreaGraph& after) {
    const auto node = [](const CoarseNode& one, const CoarseNode& other) {
      return same(one.cell, other.cell) && one.component == other.component;
    };
    const auto exit = [](const CoarseExit& one, const CoarseExit& other) {
      return one.node == other.node && one.area == other.area &&
             same(one.first, other.first) && same(one.along, other.along) &&
             one.length == other.length;
    };
    return before.components == after.components &&
           std::equal(before.nodes.begin(), before.nodes.end(),
                      after.nodes.begin(), after.nodes.end(), node) &&
           std::equal(before.exits.begin(), before.exits.end(),
                      after.exits.begin(), after.exits.end(), exit);
  };
  bool joins_kept = true;
  for (const std::size_t area : changed) {
    build_area(map, area);
    const auto before =
        std::find_if(undo.areas_.begin(), undo.areas_.end(),
                     [area](const auto& saved) { return saved.first == area; });
    joins_kept = joins_kept && kept(before->second, areas_.at(area));
  }
  for (const std::size_t area : relinked) {
    link_exits(area);
  }
  // Where every area kept its nodes, an area merely relinked is as it was,
  // so only the regions holding a rebuilt area change.
  std::set<std::size_t> regions;
  for (const std::size_t area : joins_kept ? changed : relinked) {
    regions.insert(region_of_area(area));
  }
  for (const std::size_t region : regions) {
    undo.regions_.emplace_back(region, regions_.at(region));
    build_region(region);
  }
  number();
  if (!joins_kept) {
    undo.components_.emplace(first_component_, joined_);
    join_components();
  }
  return undo;
}

void CoarseGraph::revert(Undo undo) {
  for (auto& [area, graph] : undo.areas_) {
    areas_.at(area) = std::move(graph);
  }
  for (auto& [region, graph] : undo.regions_) {
    regions_.at(region) = std::move(graph);
  }
  if (undo.components_) {
    first_component_ = std::move(undo.components_->first);
    joined_ = std::move(undo.components_->second);
  }
  number();
}

bool CoarseGraph::operator==(const CoarseGraph& other) const {
  const auto edges = [](const std::vector<std::vector<Edge>>& one,
                        const std::vector<std::vector<Edge>>& two) {
    return std::equal(
        one.begin(), one.end(), two.begin(), two.end(),
        [](const std::vector<Edge>& left, const std::vector<Edge>& right) {
          return std::equal(
              left.begin(), left.end(), right.begin(), right.end(),
              [](const Edge& first_edge, const Edge& second_edge) {
                return first_edge.to == second_edge.to &&
                       first_edge.cost == second_edge.cost;
              });
        });
  };
  const auto area = [&edges](const AreaGraph& one, const AreaGraph& two) {
    const auto node = [](const CoarseNode& left, const CoarseNode& right) {
      return same(left.cell, right.cell) && left.component == right.component;
    };
    const auto exit = [](const CoarseExit& left, const CoarseExit& right) {
      return left.node == right.node && left.area == right.area &&
             left.far_node == right.far_node && same(left.first, right.first) &&
             same(left.along, right.along) && left.length == right.length;
    };
    return one.components == two.components && one.labels == two.labels &&
           std::equal(one.nodes.begin(), one.nodes.end(), two.nodes.begin(),
                      two.nodes.end(), node) &&
           edges(one.within, two.within) &&
           std::equal(one.exits.begin(), one.exits.end(), two.exits.begin(),
                      two.exits.end(), exit) &&
           one.border == two.border;
  };
  const auto region = [&edges](const RegionGraph& one, const RegionGraph& two) {
    return std::equal(one.border.begin(), one.border.end(), two.border.begin(),
                      two.border.end(),
                      [](const NodeRef& left, const NodeRef& right) {
                        return left.area == right.area &&
                               left.node == right.node;
                      }) &&
           edges(one.within, two.within);
  };
  return width_ == other.width_ && height_ == other.height_ &&
         std::equal(areas_.begin(), areas_.end(), other.areas_.begin(),
                    other.areas_.end(), area) &&
         std::equal(regions_.begin(), regions_.end(), other.regions_.begin(),
                    other.regions_.end(), region) &&
         first_component_ == other.first_component_ &&
         joined_ == other.joined_ && first_node_ == other.first_node_ &&
         first_border_ == other.first_border_;
}

void CoarseGraph::number() {
  first_node_.assign(areas_.size() + 1, 0);
  for (std::size_t area = 0; area < areas_.size(); ++area) {
    first_node_.at(area + 1) =
        first_node_.at(area) + areas_.at(area).nodes.size();
  }
  first_border_.assign(regions_.size() + 1, 0);
  for (std::size_t region = 0; region < regions_.size(); ++region) {
    first_border_.at(region + 1) =
        first_border_.at(region) + regions_.at(region).border.size();
  }
}

std::size_t CoarseGraph::id_of(NodeRef ref) const {
  return first_node_.at(ref.area) + ref.node;
}

NodeRef CoarseGraph::ref_of(std::size_t ordinal) const {
  // The last area whose first node is at or before `id`: an area with no
  // nodes shares its first with the next.
  const auto after =
      std::upper_bound(first_node_.begin(), first_node_.end(), ordinal);
  const auto area = static_cast<std::size_t>(after - first_node_.begin()) - 1;
  return NodeRef{area, ordinal - first_node_.at(area)};
}

std::size_t CoarseGraph::border_id(NodeRef ref) const {
  return first_border_.at(region_of_area(ref.area)) +
         static_cast<std::size_t>(areas_.at(ref.area).border.at(ref.node));
}

NodeRef CoarseGraph::border_ref(std::size_t ordinal) const {
  const auto after =
      std::upper_bound(first_border_.begin(), first_border_.end(), ordinal);
  const auto region =
      static_cast<std::size_t>(after - first_border_.begin()) - 1;
  return regions_.at(region).border.at(ordinal - first_border_.at(region));
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
        const Cell reached = waiting.back();
        waiting.pop_back();
        for (const auto& [column, row] : DIRECTIONS) {
          const Cell beside{reached.x + column, reached.y + row};
          if (opens(map, corner, size, reached, column, row) &&
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

bool CoarseGraph::open_area(const Map& map, std::size_t area) const {
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  for (Cell cell{0, corner.y}; cell.y < corner.y + size.height; ++cell.y) {
    for (cell.x = corner.x; cell.x < corner.x + size.width; ++cell.x) {
      if (!map.passable(cell)) {
        return false;
      }
    }
  }
  return true;
}

void CoarseGraph::build_area(const Map& map, std::size_t area) {
  AreaGraph made;
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  // An area with no blocked cell is one component, and the shortest walk
  // between two of its cells is the octile cost: every step within it opens.
  made.open =
      costs_ == AreaCosts::CLOSED_FORM_WHEN_OPEN && open_area(map, area);
  const std::vector<std::int16_t> label =
      made.open
          ? std::vector<std::int16_t>(static_cast<std::size_t>(size.width) *
                                          static_cast<std::size_t>(size.height),
                                      0)
          : labels(map, area);
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
  // An area of one component needs no labels: every passable cell is in it.
  if (made.components > 1) {
    made.labels = label;
  }
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
    // A corner cell may be the middle of an entrance on two of the far area's
    // sides, so the far node is the one at that cell whose exit leads back.
    const AreaGraph& far = areas_.at(exit.area);
    const auto found =
        std::find_if(far.exits.begin(), far.exits.end(),
                     [&far, area, middle](const CoarseExit& back) {
                       return back.area == area &&
                              same(far.nodes.at(back.node).cell, middle);
                     });
    exit.far_node = found->node;
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
    const auto [held, place] = frontier.top();
    frontier.pop();
    if (held != cost.at(place)) {
      continue;
    }
    const Cell cell{corner.x + static_cast<std::int32_t>(place % width),
                    corner.y + static_cast<std::int32_t>(place / width)};
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
  std::vector<std::optional<CoarseCost>> found;
  if (areas_.at(area).open) {
    for (const CoarseNode& node : areas_.at(area).nodes) {
      found.emplace_back(octile(from, node.cell));
    }
    return found;
  }
  const std::vector<CoarseCost> cost = area_costs(map, area, from);
  for (const CoarseNode& node : areas_.at(area).nodes) {
    const CoarseCost reached = cost.at(slot_in(area, node.cell));
    found.push_back(reached == UNREACHED ? std::nullopt
                                         : std::optional<CoarseCost>{reached});
  }
  return found;
}

std::vector<std::size_t> CoarseGraph::region_areas(std::size_t region) const {
  const auto columns = static_cast<std::size_t>(columns_);
  const auto side = static_cast<std::size_t>(REGION_AREAS);
  const std::size_t region_columns = (columns + side - 1) / side;
  const std::size_t left = region % region_columns * side;
  const std::size_t top = region / region_columns * side;
  const std::size_t right = std::min(left + side, columns);
  const std::size_t bottom =
      std::min(top + side, static_cast<std::size_t>(rows_));
  std::vector<std::size_t> found;
  for (std::size_t row = top; row < bottom; ++row) {
    for (std::size_t column = left; column < right; ++column) {
      found.push_back((row * columns) + column);
    }
  }
  return found;
}

void CoarseGraph::build_region(std::size_t region) {
  RegionGraph made;
  for (const std::size_t area : region_areas(region)) {
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

std::uint16_t CoarseGraph::component_of(Cell cell) const {
  const std::size_t area = area_of(cell);
  const std::vector<std::int16_t>& held = areas_.at(area).labels;
  return held.empty()
             ? 0
             : static_cast<std::uint16_t>(held.at(slot_in(area, cell)));
}

std::vector<std::size_t> CoarseGraph::sets_of(const Map& map, Cell cell) const {
  // A blocked cell reaches the cells it could step onto, as a field seeded
  // on it does.
  std::vector<Cell> ends;
  if (map.passable(cell)) {
    ends.push_back(cell);
  } else {
    const Footprint whole{width_, height_};
    for (const auto& [column, row] : DIRECTIONS) {
      if (opens(map, Cell{0, 0}, whole, cell, column, row)) {
        ends.push_back(Cell{cell.x + column, cell.y + row});
      }
    }
  }
  std::vector<std::size_t> sets;
  sets.reserve(ends.size());
  for (const Cell end : ends) {
    sets.push_back(
        joined_.at(first_component_.at(area_of(end)) + component_of(end)));
  }
  return sets;
}

bool CoarseGraph::joined(const Map& map, Walk walk) const {
  const std::vector<std::size_t> one = sets_of(map, walk.from);
  const std::vector<std::size_t> other = sets_of(map, walk.to);
  return std::any_of(one.begin(), one.end(), [&other](std::size_t set) {
    return std::find(other.begin(), other.end(), set) != other.end();
  });
}

const std::vector<CoarseExit>& CoarseGraph::exits(std::size_t area) const {
  return areas_.at(area).exits;
}

std::vector<CoarseCost> CoarseGraph::walks_within(const Map& map,
                                                  std::size_t area,
                                                  Cell from) const {
  if (!areas_.at(area).open) {
    return area_costs(map, area, from);
  }
  const Cell corner = area_corner(area);
  const Footprint size = area_size(area);
  std::vector<CoarseCost> walks;
  walks.reserve(static_cast<std::size_t>(size.width) *
                static_cast<std::size_t>(size.height));
  for (Cell cell{0, corner.y}; cell.y < corner.y + size.height; ++cell.y) {
    for (cell.x = corner.x; cell.x < corner.x + size.width; ++cell.x) {
      walks.push_back(octile(from, cell));
    }
  }
  return walks;
}

std::size_t CoarseGraph::nodes_in(std::size_t area) const {
  return areas_.at(area).nodes.size();
}

CoarseGraph::Costs CoarseGraph::inserted(const Map& map, Cell cell) const {
  const std::size_t area = area_of(cell);
  const std::vector<std::optional<CoarseCost>> costs =
      node_costs(map, area, cell);
  Costs found;
  found.reserve(costs.size());
  for (std::size_t node = 0; node < costs.size(); ++node) {
    if (costs.at(node)) {
      found.emplace_back(NodeRef{area, node}, costs.at(node).value_or(0));
    }
  }
  return found;
}

template <typename Relax>
void CoarseGraph::expand_areas(NodeRef ref, Relax relax) const {
  const AreaGraph& area = areas_.at(ref.area);
  for (const Edge& edge : area.within.at(ref.node)) {
    relax(NodeRef{ref.area, edge.to}, edge.cost);
  }
  for (const CoarseExit& exit : area.exits) {
    if (exit.node == ref.node) {
      relax(NodeRef{exit.area, exit.far_node}, STRAIGHT);
    }
  }
}

template <typename Relax>
void CoarseGraph::expand_regions(std::size_t ordinal, Relax relax) const {
  const NodeRef ref = border_ref(ordinal);
  const std::size_t region = region_of_area(ref.area);
  const RegionGraph& graph = regions_.at(region);
  const auto index =
      static_cast<std::size_t>(areas_.at(ref.area).border.at(ref.node));
  for (const Edge& edge : graph.within.at(index)) {
    relax(first_border_.at(region) + edge.to, edge.cost);
  }
  // A far node across a region's border lies on its own region's border.
  for (const CoarseExit& exit : areas_.at(ref.area).exits) {
    if (exit.node == ref.node && region_of_area(exit.area) != region) {
      relax(border_id(NodeRef{exit.area, exit.far_node}), STRAIGHT);
    }
  }
}

namespace {

// Numbered nodes at a cost.
using Ends = std::vector<std::pair<std::size_t, CoarseCost>>;

// A search's numbered starts and goals.
struct Numbered {
  Ends starts;
  Ends goals;
};

// The cost of the cheapest walk from one of `starts` to one of `goals`, each
// at its cost, over `count` numbered nodes, each placed by `cell_of` and its
// neighbours given by `expand`; nothing when none joins them. The estimate is
// the octile cost to the box round the goal nodes' cells, which never exceeds
// a walk's cost to a goal and falls by no more than a step's cost along one,
// so the first goal reached at the least bound is the cheapest.
template <typename CellOf, typename Expand>
std::optional<CoarseCost> a_star(std::size_t count, const Numbered& ends,
                                 CellOf cell_of, Expand expand) {
  std::vector<CoarseCost> goal(count, UNREACHED);
  Cell low{std::numeric_limits<std::int32_t>::max(),
           std::numeric_limits<std::int32_t>::max()};
  Cell high{std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::min()};
  for (const auto& [ordinal, cost] : ends.goals) {
    goal.at(ordinal) = std::min(goal.at(ordinal), cost);
    const Cell cell = cell_of(ordinal);
    low = Cell{std::min(low.x, cell.x), std::min(low.y, cell.y)};
    high = Cell{std::max(high.x, cell.x), std::max(high.y, cell.y)};
  }
  const auto estimate = [&](std::size_t ordinal) {
    const Cell cell = cell_of(ordinal);
    return octile(Cell{0, 0},
                  Cell{std::max({0, low.x - cell.x, cell.x - high.x}),
                       std::max({0, low.y - cell.y, cell.y - high.y})});
  };
  std::vector<CoarseCost> cost(count, UNREACHED);
  using Entry = std::pair<CoarseCost, std::size_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  const auto offer = [&](std::size_t next, CoarseCost through) {
    if (through < cost.at(next)) {
      cost.at(next) = through;
      frontier.emplace(through + estimate(next), next);
    }
  };
  for (const auto& [ordinal, held] : ends.starts) {
    offer(ordinal, held);
  }
  std::optional<CoarseCost> best;
  while (!frontier.empty()) {
    const CoarseCost bound = frontier.top().first;
    const std::size_t ordinal = frontier.top().second;
    frontier.pop();
    if (best && bound >= *best) {
      break;
    }
    const CoarseCost held = cost.at(ordinal);
    if (bound != held + estimate(ordinal)) {
      continue;
    }
    if (goal.at(ordinal) != UNREACHED) {
      best = std::min(best.value_or(UNREACHED), held + goal.at(ordinal));
    }
    expand(ordinal, [&offer, held](std::size_t next, CoarseCost step) {
      offer(next, held + step);
    });
  }
  return best;
}

}  // namespace

CoarseGraph::Costs CoarseGraph::settle_region(const Costs& starts,
                                              std::size_t region) const {
  // The region's areas, in area order, and the number of each one's first
  // node among the region's.
  const std::vector<std::size_t> within = region_areas(region);
  std::vector<std::size_t> first(within.size() + 1, 0);
  for (std::size_t place = 0; place < within.size(); ++place) {
    first.at(place + 1) =
        first.at(place) + areas_.at(within.at(place)).nodes.size();
  }
  const auto local = [&](NodeRef ref) -> std::optional<std::size_t> {
    const auto place = std::lower_bound(within.begin(), within.end(), ref.area);
    if (place == within.end() || *place != ref.area) {
      return std::nullopt;
    }
    return first.at(static_cast<std::size_t>(place - within.begin())) +
           ref.node;
  };
  std::vector<CoarseCost> cost(first.back(), UNREACHED);
  std::vector<NodeRef> refs(first.back(), NodeRef{0, 0});
  using Entry = std::pair<CoarseCost, std::size_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  const auto offer = [&](NodeRef next, CoarseCost through) {
    const std::optional<std::size_t> here = local(next);
    if (here && through < cost.at(*here)) {
      cost.at(*here) = through;
      refs.at(*here) = next;
      frontier.emplace(through, *here);
    }
  };
  for (const auto& [ref, held] : starts) {
    offer(ref, held);
  }
  while (!frontier.empty()) {
    const CoarseCost held = frontier.top().first;
    const std::size_t here = frontier.top().second;
    frontier.pop();
    if (held != cost.at(here)) {
      continue;
    }
    expand_areas(refs.at(here), [&offer, held](NodeRef next, CoarseCost step) {
      offer(next, held + step);
    });
  }
  // In area order, then by node, as the numbering has it.
  Costs settled;
  for (std::size_t here = 0; here < cost.size(); ++here) {
    if (cost.at(here) != UNREACHED) {
      settled.emplace_back(refs.at(here), cost.at(here));
    }
  }
  return settled;
}

CoarseGraph::Costs CoarseGraph::on_border(const Costs& settled) const {
  Costs border;
  for (const auto& [ref, cost] : settled) {
    if (areas_.at(ref.area).border.at(ref.node) >= 0) {
      border.emplace_back(ref, cost);
    }
  }
  return border;
}

std::optional<CoarseCost> CoarseGraph::between(const Terminals& ends,
                                               bool by_regions) const {
  if (ends.starts.empty() || ends.goals.empty()) {
    return std::nullopt;
  }
  const auto numbered = [](const Costs& costs, const auto& number) {
    Ends found;
    found.reserve(costs.size());
    for (const auto& [ref, cost] : costs) {
      found.emplace_back(number(ref), cost);
    }
    return found;
  };
  const std::size_t start_region =
      region_of_area(ends.starts.front().first.area);
  const std::size_t goal_region = region_of_area(ends.goals.front().first.area);
  if (!by_regions || start_region == goal_region) {
    const auto ordinal = [this](NodeRef ref) { return id_of(ref); };
    return a_star(
        first_node_.back(),
        Numbered{numbered(ends.starts, ordinal), numbered(ends.goals, ordinal)},
        [this](std::size_t here) { return node(ref_of(here)).cell; },
        [this](std::size_t here, const auto& relax) {
          expand_areas(ref_of(here),
                       [this, &relax](NodeRef next, CoarseCost step) {
                         relax(id_of(next), step);
                       });
        });
  }
  // The start's and the goal's region border nodes, at their cost from the
  // start and to the goal within their regions; then over the regions'
  // border nodes, through the walks within each region and the entrances
  // across their borders.
  const auto ordinal = [this](NodeRef ref) { return border_id(ref); };
  return a_star(
      first_border_.back(),
      Numbered{
          numbered(on_border(settle_region(ends.starts, start_region)),
                   ordinal),
          numbered(on_border(settle_region(ends.goals, goal_region)), ordinal)},
      [this](std::size_t here) { return node(border_ref(here)).cell; },
      [this](std::size_t here, const auto& relax) {
        expand_regions(here, relax);
      });
}

GoalSearch CoarseGraph::toward(const Map& map, Cell objective) const {
  GoalSearch search;
  search.region = region_of_area(area_of(objective));
  search.border.assign(first_border_.back(), UNREACHED);
  search.settled.assign(first_border_.back(), false);
  for (const auto& [ref, held] :
       settle_region(inserted(map, objective), search.region)) {
    search.near.emplace(key_of(ref), held);
    if (areas_.at(ref.area).border.at(ref.node) >= 0) {
      search.border.at(border_id(ref)) = held;
      search.frontier.emplace(held, border_id(ref));
    }
  }
  return search;
}

void CoarseGraph::settle_borders(GoalSearch& search, std::size_t region) const {
  // Dijkstra outward over the regions' border nodes, paused once every
  // border node of `region` is settled. Ties go to the nodes' order.
  const std::size_t first = first_border_.at(region);
  const std::size_t last = first_border_.at(region + 1);
  std::size_t pending = 0;
  for (std::size_t here = first; here < last; ++here) {
    if (!search.settled.at(here)) {
      ++pending;
    }
  }
  while (pending > 0 && !search.frontier.empty()) {
    const CoarseCost held = search.frontier.top().first;
    const std::size_t here = search.frontier.top().second;
    search.frontier.pop();
    if (search.settled.at(here) || held != search.border.at(here)) {
      continue;
    }
    search.settled.at(here) = true;
    if (here >= first && here < last) {
      --pending;
    }
    expand_regions(here, [&search, held](std::size_t next, CoarseCost step) {
      const CoarseCost through = held + step;
      if (through < search.border.at(next)) {
        search.border.at(next) = through;
        search.frontier.emplace(through, next);
      }
    });
  }
}

std::optional<CoarseCost> CoarseGraph::cost_from(GoalSearch& search,
                                                 NodeRef from) const {
  // A walk from `from` to the objective either keeps to the objective's
  // region, whose nodes `near` costs, or leaves `from`'s region through one
  // of its border nodes, at that node's cost over the regions; so the region
  // settled outward from its border nodes at those costs, with `near` in the
  // objective's region, costs each of its nodes as the areas alone would.
  const std::size_t region = region_of_area(from.area);
  auto table = search.regions.find(region);
  if (table == search.regions.end()) {
    settle_borders(search, region);
    Costs starts;
    const RegionGraph& graph = regions_.at(region);
    for (std::size_t index = 0; index < graph.border.size(); ++index) {
      const CoarseCost held =
          search.border.at(first_border_.at(region) + index);
      if (held != UNREACHED) {
        starts.emplace_back(graph.border.at(index), held);
      }
    }
    std::map<Key, CoarseCost> costs;
    for (const auto& [ref, held] : settle_region(starts, region)) {
      costs.emplace(key_of(ref), held);
    }
    if (region == search.region) {
      for (const auto& [key, held] : search.near) {
        const auto [place, fresh] = costs.emplace(key, held);
        if (!fresh) {
          place->second = std::min(place->second, held);
        }
      }
    }
    table = search.regions.emplace(region, std::move(costs)).first;
  }
  const auto found = table->second.find(key_of(from));
  if (found == table->second.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::optional<CoarseCost> CoarseGraph::cost_over_areas(const Map& map,
                                                       Cell objective,
                                                       NodeRef from) const {
  std::vector<CoarseCost> cost(first_node_.back(), UNREACHED);
  using Entry = std::pair<CoarseCost, std::size_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
  for (const auto& [ref, held] : inserted(map, objective)) {
    cost.at(id_of(ref)) = held;
    frontier.emplace(held, id_of(ref));
  }
  const std::size_t wanted = id_of(from);
  while (!frontier.empty()) {
    const CoarseCost held = frontier.top().first;
    const std::size_t here = frontier.top().second;
    frontier.pop();
    if (held != cost.at(here)) {
      continue;
    }
    if (here == wanted) {
      return held;
    }
    expand_areas(ref_of(here),
                 [this, &cost, &frontier, held](NodeRef next, CoarseCost step) {
                   const CoarseCost through = held + step;
                   if (through < cost.at(id_of(next))) {
                     cost.at(id_of(next)) = through;
                     frontier.emplace(through, id_of(next));
                   }
                 });
  }
  return std::nullopt;
}

std::optional<CoarseCost> CoarseGraph::path_cost(const Map& map, Walk walk,
                                                 bool by_regions) const {
  if (!joined(map, walk)) {
    return std::nullopt;
  }
  const std::size_t area = area_of(walk.from);
  if (area == area_of(walk.to) &&
      component_of(walk.from) == component_of(walk.to)) {
    // Within one component of one area, the walk within it, as a field
    // restricted to the area finds it.
    return area_costs(map, area, walk.from).at(slot_in(area, walk.to));
  }
  return between(Terminals{inserted(map, walk.from), inserted(map, walk.to)},
                 by_regions);
}

}  // namespace murim
