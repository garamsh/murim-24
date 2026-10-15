#include "layout.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include "map.hpp"
#include "village.hpp"

namespace murim {
namespace {

constexpr std::int32_t SIDE = 31;
constexpr Cell CENTRE{15, 15};
constexpr Footprint SQUARE{3, 3};
constexpr std::int32_t VICINITY = 12;
constexpr Footprint HOUSE{3, 3};
constexpr Footprint FIELD{3, 2};
constexpr std::uint64_t SEED = 7;
constexpr std::size_t HOUSES = 4;

// The index of `cell` on a lot SIDE cells a side.
std::size_t index_on_lot(Cell cell) {
  return (static_cast<std::size_t>(cell.y) * SIDE) +
         static_cast<std::size_t>(cell.x);
}

// An open lot of SIDE cells a side, its square kept clear round CENTRE.
Lot open_lot() {
  const auto cells = static_cast<std::size_t>(SIDE) * SIDE;
  Lot lot{Footprint{SIDE, SIDE}, std::vector<Plot>(cells, Plot::FREE),
          std::vector<bool>(cells, true)};
  build_on(lot, square_corner(CENTRE, SQUARE), SQUARE, false);
  return lot;
}

// `lot` with `cells` offering `plot`, rough as hills are or blocked, which
// bodies do not pass, as water is.
Lot with(Lot lot, const std::vector<Cell>& cells, Plot plot) {
  for (const Cell cell : cells) {
    lot.plots.at(index_on_lot(cell)) = plot;
    lot.passable.at(index_on_lot(cell)) = plot != Plot::BLOCKED;
  }
  return lot;
}

// The cells of a lot SIDE cells a side for which `chosen` holds, row by row.
template <typename Chosen>
std::vector<Cell> cells_where(const Chosen& chosen) {
  std::vector<Cell> cells;
  for (const Cell cell : cells_of(Cell{0, 0}, Footprint{SIDE, SIDE})) {
    if (chosen(cell)) {
      cells.push_back(cell);
    }
  }
  return cells;
}

// Whether `chosen` holds for any cell of `area` from `corner`.
template <typename Chosen>
bool any_cell(Cell corner, Footprint area, const Chosen& chosen) {
  const std::vector<Cell> cells = cells_of(corner, area);
  return std::any_of(cells.begin(), cells.end(), chosen);
}

// The straight-line distance from CENTRE to the middle of `area` from
// `corner`, in half cells.
double distance(Cell corner, Footprint area) {
  const std::int64_t across = (2 * std::int64_t{corner.x}) + area.width -
                              (2 * std::int64_t{CENTRE.x}) - 1;
  const std::int64_t down = (2 * std::int64_t{corner.y}) + area.height -
                            (2 * std::int64_t{CENTRE.y}) - 1;
  return std::sqrt(static_cast<double>((across * across) + (down * down)));
}

// A village's layout on `lot` round CENTRE: HOUSES houses as homes, then a
// field as `field_role`, each placed by its role in turn with `seed`'s draw
// and built on the lot; the corners, the field's last.
std::vector<Cell> laid_out(Lot lot, Role field_role,
                           std::uint64_t seed = SEED) {
  const Townsite site{CENTRE, SQUARE, VICINITY};
  std::vector<Cell> corners;
  std::vector<Structure> homes;
  for (std::size_t house = 0; house < HOUSES; ++house) {
    const std::optional<Cell> corner =
        place(lot, site, HOUSE, Role::HOME, homes,
              placement_draw(seed, 0, corners.size()));
    if (!corner) {
      return corners;
    }
    build_on(lot, *corner, HOUSE, true);
    homes.push_back(Structure{*corner, 0, HOUSE, true, Role::HOME});
    corners.push_back(*corner);
  }
  const std::optional<Cell> field =
      place(lot, site, FIELD, field_role, homes,
            placement_draw(seed, 0, corners.size()));
  if (field) {
    corners.push_back(*field);
  }
  return corners;
}

// The farthest of a layout's houses from CENTRE, in half cells.
double farthest_house(const std::vector<Cell>& layout) {
  double farthest = 0;
  for (std::size_t house = 0; house < HOUSES && house < layout.size();
       ++house) {
    farthest = std::max(farthest, distance(layout.at(house), HOUSE));
  }
  return farthest;
}

// Whether two layouts lay every structure at the same corner.
bool same_layout(const std::vector<Cell>& one, const std::vector<Cell>& other) {
  return std::equal(one.begin(), one.end(), other.begin(), other.end(),
                    [](Cell left, Cell right) {
                      return left.x == right.x && left.y == right.y;
                    });
}

TEST(Layout, FieldsLieBeyondTheHousesAndAmongThemWhenPlacedAsHomes) {
  // Distances from the centre are in half cells, so two cells are 4.
  constexpr double TWO_CELLS = 4;
  const std::vector<Cell> edge = laid_out(open_lot(), Role::EDGE);
  ASSERT_EQ(edge.size(), HOUSES + 1);
  // The field lies at least two cells farther from the centre than every
  // house.
  EXPECT_GE(distance(edge.back(), FIELD), farthest_house(edge) + TWO_CELLS);
  // Control: the same field placed as a home lies among the houses, less
  // than two cells past the farthest of them.
  const std::vector<Cell> home = laid_out(open_lot(), Role::HOME);
  ASSERT_EQ(home.size(), HOUSES + 1);
  EXPECT_LT(distance(home.back(), FIELD), farthest_house(edge) + TWO_CELLS);
}

TEST(Layout, AnEdgePlanTerracesHillCellsWhenFlatLandIsFarther) {
  // Hills surround a flat patch reaching five cells from the centre, room
  // for the houses and not for a field two steps beyond them.
  constexpr std::int32_t FLAT = 5;
  const auto hill = [](Cell cell) {
    return std::abs(cell.x - CENTRE.x) > FLAT ||
           std::abs(cell.y - CENTRE.y) > FLAT;
  };
  const std::vector<Cell> terraced =
      laid_out(with(open_lot(), cells_where(hill), Plot::ROUGH), Role::EDGE);
  ASSERT_EQ(terraced.size(), HOUSES + 1);
  EXPECT_TRUE(any_cell(terraced.back(), FIELD, hill));
  // No house stands on a hill.
  for (std::size_t house = 0; house < HOUSES; ++house) {
    EXPECT_FALSE(any_cell(terraced.at(house), HOUSE, hill)) << house;
  }
  // Control: the same village with no hills lays its field on flat land,
  // where the hills were.
  const std::vector<Cell> flat = laid_out(open_lot(), Role::EDGE);
  ASSERT_EQ(flat.size(), HOUSES + 1);
  const Lot plain = open_lot();
  EXPECT_FALSE(any_cell(flat.back(), FIELD, [&plain](Cell cell) {
    return plain.plots.at(index_on_lot(cell)) != Plot::FREE;
  }));
}

TEST(Layout, HousesStringAlongABankNearerByPathThanAcrossTheWater) {
  // Water runs north to south a cell east of the square, with one ford far
  // to the south, so the far bank is far by path though near in a line.
  const std::vector<Cell> water = cells_where([](Cell cell) {
    return cell.y < SIDE - 1 &&
           (cell.x == CENTRE.x + 3 || cell.x == CENTRE.x + 4);
  });
  const std::vector<Cell> riverside =
      laid_out(with(open_lot(), water, Plot::BLOCKED), Role::EDGE);
  ASSERT_EQ(riverside.size(), HOUSES + 1);
  for (std::size_t house = 0; house < HOUSES; ++house) {
    EXPECT_LT(riverside.at(house).x, CENTRE.x + 3) << "house " << house;
  }
  // Control: with no water, a house is laid east of the square, where the
  // far bank would be.
  const std::vector<Cell> open = laid_out(open_lot(), Role::EDGE);
  EXPECT_TRUE(std::any_of(open.begin(), open.begin() + HOUSES,
                          [](Cell corner) { return corner.x > CENTRE.x + 1; }));
}

TEST(Layout, OneSiteAndDrawGiveOneLayoutAndAnotherDrawAnother) {
  const std::vector<Cell> first = laid_out(open_lot(), Role::EDGE);
  ASSERT_EQ(first.size(), HOUSES + 1);
  EXPECT_TRUE(same_layout(first, laid_out(open_lot(), Role::EDGE)));
  // The same site with another seed's draw, ties broken otherwise: the
  // layout differs somewhere.
  const std::vector<Cell> other = laid_out(open_lot(), Role::EDGE, SEED + 1);
  ASSERT_EQ(other.size(), first.size());
  EXPECT_FALSE(same_layout(first, other));
}

TEST(Layout, ASiteNearerThanApartToAnotherCentreIsNotOpen) {
  constexpr std::int32_t APART = 20;
  const std::vector<Cell> centres{Cell{0, 0}};
  EXPECT_FALSE(open_site(Cell{APART - 1, 0}, centres, APART));
  EXPECT_FALSE(open_site(Cell{14, 14}, centres, APART));
  EXPECT_TRUE(open_site(Cell{APART, 0}, centres, APART));
  EXPECT_TRUE(open_site(Cell{15, 15}, centres, APART));
  // Control: at an apart of 0 every site is open, the centre's own included.
  EXPECT_TRUE(open_site(Cell{0, 0}, centres, 0));
}

}  // namespace
}  // namespace murim
