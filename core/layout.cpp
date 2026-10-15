#include "layout.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "map.hpp"
#include "village.hpp"

namespace murim {
namespace {

// The cost of a straight and of a diagonal step, as the flow field counts
// them (ADR 0037).
constexpr std::int64_t STRAIGHT = 5;
constexpr std::int64_t DIAGONAL = 7;
// How many straight steps past the farthest home an edge plan lies.
constexpr std::int64_t EDGE_STEPS = 2;
constexpr std::int64_t UNREACHED = std::numeric_limits<std::int64_t>::max();

// SplitMix64's constants (Steele, Lea and Flood, 2014).
constexpr std::uint64_t GOLDEN = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t MIX_A = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t MIX_B = 0x94D049BB133111EBULL;
constexpr int SHIFT_A = 30;
constexpr int SHIFT_B = 27;
constexpr int SHIFT_C = 31;

std::uint64_t mix(std::uint64_t value) {
  value += GOLDEN;
  value = (value ^ (value >> SHIFT_A)) * MIX_A;
  value = (value ^ (value >> SHIFT_B)) * MIX_B;
  return value ^ (value >> SHIFT_C);
}

// A rectangle of cells, [left, right) by [top, bottom).
struct Box {
  std::int32_t left;
  std::int32_t top;
  std::int32_t right;
  std::int32_t bottom;
};

bool holds(const Box& box, Cell cell) {
  return cell.x >= box.left && cell.y >= box.top && cell.x < box.right &&
         cell.y < box.bottom;
}

Box box_of(Cell corner, Footprint footprint) {
  return Box{corner.x, corner.y, corner.x + footprint.width,
             corner.y + footprint.height};
}

bool overlap(const Box& one, const Box& other) {
  return one.left < other.right && other.left < one.right &&
         one.top < other.bottom && other.top < one.bottom;
}

Box grown(const Box& box, std::int32_t margin) {
  return Box{box.left - margin, box.top - margin, box.right + margin,
             box.bottom + margin};
}

// The path distance from a site's centre to each cell of its vicinity, over
// cells bodies pass.
class Distances {
 public:
  Distances(const Lot& lot, const Townsite& site)
      : box_{std::max(site.centre.x - site.vicinity, 0),
             std::max(site.centre.y - site.vicinity, 0),
             std::min(site.centre.x + site.vicinity + 1, lot.size.width),
             std::min(site.centre.y + site.vicinity + 1, lot.size.height)},
        lot_(&lot),
        distance_(
            static_cast<std::size_t>(std::max((box_.right - box_.left), 0)) *
                static_cast<std::size_t>(std::max(box_.bottom - box_.top, 0)),
            UNREACHED) {
    if (!holds(box_, site.centre)) {
      return;
    }
    using Entry = std::pair<std::int64_t, Cell>;
    const auto later = [](const Entry& one, const Entry& other) {
      return one.first > other.first;
    };
    std::priority_queue<Entry, std::vector<Entry>, decltype(later)> open(later);
    distance_.at(slot(site.centre)) = 0;
    open.emplace(0, site.centre);
    while (!open.empty()) {
      const auto [cost, cell] = open.top();
      open.pop();
      if (cost <= distance_.at(slot(cell))) {
        for (const auto& [next, reached] : steps(cell, cost)) {
          if (reached < distance_.at(slot(next))) {
            distance_.at(slot(next)) = reached;
            open.emplace(reached, next);
          }
        }
      }
    }
  }

  [[nodiscard]] const Box& box() const { return box_; }

  // The least distance to a cell of `area` or beside it, or UNREACHED.
  [[nodiscard]] std::int64_t to(const Box& area) const {
    std::int64_t least = UNREACHED;
    const Box around = grown(area, 1);
    for (Cell cell{0, std::max(around.top, box_.top)};
         cell.y < std::min(around.bottom, box_.bottom); ++cell.y) {
      for (cell.x = std::max(around.left, box_.left);
           cell.x < std::min(around.right, box_.right); ++cell.x) {
        least = std::min(least, distance_.at(slot(cell)));
      }
    }
    return least;
  }

 private:
  // The cells one step from `cell`, reached at `cost`, with what reaching
  // each costs: a diagonal step only where both cells beside it pass.
  [[nodiscard]] std::vector<std::pair<Cell, std::int64_t>> steps(
      Cell cell, std::int64_t cost) const {
    std::vector<std::pair<Cell, std::int64_t>> found;
    for (std::int32_t row = -1; row <= 1; ++row) {
      for (std::int32_t column = -1; column <= 1; ++column) {
        const Cell next{cell.x + column, cell.y + row};
        const bool diagonal = column != 0 && row != 0;
        const bool cut = diagonal && (!passes(Cell{cell.x + column, cell.y}) ||
                                      !passes(Cell{cell.x, cell.y + row}));
        if ((column != 0 || row != 0) && passes(next) && !cut) {
          found.emplace_back(next, cost + (diagonal ? DIAGONAL : STRAIGHT));
        }
      }
    }
    return found;
  }

  [[nodiscard]] bool passes(Cell cell) const {
    return holds(box_, cell) && lot_->passable.at(index(cell));
  }
  [[nodiscard]] std::size_t index(Cell cell) const {
    return (static_cast<std::size_t>(cell.y) *
            static_cast<std::size_t>(lot_->size.width)) +
           static_cast<std::size_t>(cell.x);
  }
  [[nodiscard]] std::size_t slot(Cell cell) const {
    return (static_cast<std::size_t>(cell.y - box_.top) *
            static_cast<std::size_t>((box_.right - box_.left))) +
           static_cast<std::size_t>(cell.x - box_.left);
  }

  Box box_;
  const Lot* lot_;
  std::vector<std::int64_t> distance_;
};

Plot plot_at(const Lot& lot, Cell cell) {
  return lot.plots.at((static_cast<std::size_t>(cell.y) *
                       static_cast<std::size_t>(lot.size.width)) +
                      static_cast<std::size_t>(cell.x));
}

// Whether every cell of `area` offers what `role` needs, and none near it is
// built where the role keeps a cell clear.
bool fits(const Lot& lot, const Box& area, Role role) {
  const auto offered = [role](Plot plot) {
    return plot == Plot::FREE || (role == Role::EDGE && plot == Plot::ROUGH);
  };
  for (Cell cell{0, area.top}; cell.y < area.bottom; ++cell.y) {
    for (cell.x = area.left; cell.x < area.right; ++cell.x) {
      if (!offered(plot_at(lot, cell))) {
        return false;
      }
    }
  }
  if (role != Role::HOME) {
    return true;
  }
  const Box around = grown(area, 1);
  for (Cell cell{0, std::max(around.top, 0)};
       cell.y < std::min(around.bottom, lot.size.height); ++cell.y) {
    for (cell.x = std::max(around.left, 0);
         cell.x < std::min(around.right, lot.size.width); ++cell.x) {
      if (plot_at(lot, cell) == Plot::BUILT) {
        return false;
      }
    }
  }
  return true;
}

// How near an edge plan may lie: two straight steps past the farthest of
// `homes` from the centre.
std::int64_t beyond(const Distances& distances,
                    const std::vector<Structure>& homes) {
  std::int64_t farthest = 0;
  for (const Structure& home : homes) {
    const std::int64_t reached =
        distances.to(box_of(home.cell, home.footprint));
    if (reached != UNREACHED) {
      farthest = std::max(farthest, reached);
    }
  }
  return farthest + (EDGE_STEPS * STRAIGHT);
}

// A plan being placed: the land, the distances over it, the plan's
// footprint and role, and the least distance a corner may lie at.
struct Placing {
  const Lot* lot;
  const Distances* distances;
  Footprint footprint;
  Role role;
  std::int64_t least;
};

// The corners in the vicinity where `placing` fits nearest the centre, each
// touching `square` when it names one.
std::vector<Cell> nearest(const Placing& placing, const Box* square) {
  const Box& vicinity = placing.distances->box();
  const Footprint footprint = placing.footprint;
  std::vector<Cell> found;
  std::int64_t least = UNREACHED;
  for (Cell corner{0, vicinity.top};
       corner.y + footprint.height <= vicinity.bottom; ++corner.y) {
    for (corner.x = vicinity.left; corner.x + footprint.width <= vicinity.right;
         ++corner.x) {
      const Box area = box_of(corner, footprint);
      const bool placed =
          (square == nullptr || overlap(grown(area, 1), *square)) &&
          fits(*placing.lot, area, placing.role);
      const std::int64_t reached =
          placed ? placing.distances->to(area) : UNREACHED;
      if (reached == UNREACHED || reached < placing.least || reached > least) {
        continue;
      }
      if (reached < least) {
        least = reached;
        found.clear();
      }
      found.push_back(corner);
    }
  }
  return found;
}

}  // namespace

Cell square_corner(Cell centre, Footprint square) {
  return Cell{centre.x - (square.width / 2), centre.y - (square.height / 2)};
}

bool open_site(Cell site, const std::vector<Cell>& centres,
               std::int32_t apart) {
  const std::int64_t least = std::int64_t{apart} * apart;
  return std::none_of(
      centres.begin(), centres.end(), [site, least](Cell centre) {
        const std::int64_t offset_x = std::int64_t{centre.x} - site.x;
        const std::int64_t offset_y = std::int64_t{centre.y} - site.y;
        return (offset_x * offset_x) + (offset_y * offset_y) < least;
      });
}

std::uint64_t placement_draw(std::uint64_t seed, std::size_t village,
                             std::size_t index) {
  return mix(mix(mix(seed) ^ village) ^ index);
}

std::optional<Cell> place(const Lot& lot, const Townsite& site,
                          Footprint footprint, Role role,
                          const std::vector<Structure>& homes,
                          std::uint64_t draw) {
  const Distances distances{lot, site};
  const Box square =
      box_of(square_corner(site.centre, site.square), site.square);
  const Placing placing{&lot, &distances, footprint, role,
                        role == Role::EDGE ? beyond(distances, homes) : 0};
  std::vector<Cell> found =
      nearest(placing, role == Role::SQUARE ? &square : nullptr);
  if (found.empty() && role == Role::SQUARE) {
    found = nearest(placing, nullptr);
  }
  if (found.empty()) {
    return std::nullopt;
  }
  return found.at(static_cast<std::size_t>(draw % found.size()));
}

void build_on(Lot& lot, Cell corner, Footprint footprint, bool blocks) {
  for (const Cell cell : cells_of(corner, footprint)) {
    if (cell.x < 0 || cell.y < 0 || cell.x >= lot.size.width ||
        cell.y >= lot.size.height) {
      continue;
    }
    const std::size_t index = (static_cast<std::size_t>(cell.y) *
                               static_cast<std::size_t>(lot.size.width)) +
                              static_cast<std::size_t>(cell.x);
    lot.plots.at(index) = Plot::BUILT;
    lot.passable.at(index) = lot.passable.at(index) && !blocks;
  }
}

}  // namespace murim
