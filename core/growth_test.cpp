#include "growth.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "village.hpp"

namespace murim {
namespace {

constexpr Footprint SIZE{12, 12};
constexpr Cell MIDDLE{6, 6};
// The cell up and left of MIDDLE: a 2 by 2 footprint from it covers MIDDLE.
constexpr Cell BESIDE{MIDDLE.x - 1, MIDDLE.y - 1};
constexpr std::size_t HOUSE = 0;
constexpr std::size_t GRANARY = 1;
constexpr std::size_t FIELD = 2;
constexpr std::size_t SMITHY = 3;
constexpr std::uint32_t HOUSING = 4;
constexpr std::uint32_t STORAGE = 1'200;
constexpr std::uint32_t HARVEST = 2'400;
constexpr std::uint32_t ARMS = 6;
constexpr std::uint32_t PLENTY = 1'000'000;
constexpr std::uint32_t HOUSED = 5;
constexpr std::uint32_t STORED = 960;
constexpr std::size_t ORTHODOX = 0;
constexpr std::size_t CULT = 1;

// A house, a granary, a field and a smithy, each 2 by 2 and holding one
// thing, built in that order of preference among equals.
Growth growing() {
  Growth growth;
  Plan house{"house", {2, 2}, true, {}};
  house.holding.housing = HOUSING;
  Plan granary{"granary", {2, 2}, true, {}};
  granary.holding.storage = STORAGE;
  Plan field{"field", {2, 2}, false, {}};
  field.holding.harvest = HARVEST;
  Plan smithy{"smithy", {2, 2}, true, {}};
  smithy.holding.arms = ARMS;
  growth.plans = {house, granary, field, smithy};
  growth.builds = {HOUSE, GRANARY, FIELD, SMITHY};
  growth.vicinity = 3;
  growth.reach = 4;
  growth.spacing = 1;
  growth.settlement = {3, 3};
  growth.layout = {Placement{HOUSE, {0, 0}}, Placement{FIELD, {1, 2}}};
  growth.self_reliance = 0;
  return growth;
}

// What a village holds with plenty of everything but `scarce`, which it
// holds none of.
Holding plenty_but(std::size_t scarce) {
  Holding held;
  held.housing = scarce == HOUSE ? 0 : PLENTY;
  held.storage = scarce == GRANARY ? 0 : PLENTY;
  held.harvest = scarce == FIELD ? 0 : PLENTY;
  held.arms = scarce == SMITHY ? 0 : PLENTY;
  held.tends = PLENTY;
  return held;
}

Ground open_ground() {
  const auto cells = static_cast<std::size_t>(SIZE.width) *
                     static_cast<std::size_t>(SIZE.height);
  return Ground{std::vector<bool>(cells, false),
                std::vector<bool>(cells, false),
                std::vector<bool>(cells, false)};
}

std::size_t slot(Cell cell) {
  return (static_cast<std::size_t>(cell.y) *
          static_cast<std::size_t>(SIZE.width)) +
         static_cast<std::size_t>(cell.x);
}

Map open_map() {
  return Map{std::vector<std::string>(
      static_cast<std::size_t>(SIZE.height),
      std::string(static_cast<std::size_t>(SIZE.width), '.'))};
}

TEST(Plans, KeepThePlanEachStructureWasLaidFrom) {
  const Growth growth = growing();
  Plans plans{2};
  plans.add(GRANARY);
  plans.add(std::nullopt);
  ASSERT_EQ(plans.laid_from().size(), 4U);
  EXPECT_EQ(plans.plan_of(2), std::optional<std::size_t>{GRANARY});
  EXPECT_EQ(plans.holding_of(2, growth).storage, STORAGE);
  // Control: a structure the world was created with was laid from none.
  EXPECT_FALSE(plans.plan_of(0).has_value());
  EXPECT_FALSE(plans.plan_of(3).has_value());
  // One laid from none holds what the first plan holds.
  EXPECT_EQ(plans.holding_of(0, growth).housing, HOUSING);
  EXPECT_THROW(static_cast<void>(plans.plan_of(4)), std::out_of_range);
}

TEST(Plans, RefuseSavedPlansNamingWhatDoesNotExist) {
  const std::optional<Growth> growth = growing();
  const std::size_t known = growth->plans.size();
  // Control: plans as add() makes them are kept.
  Plans made{1};
  made.add(SMITHY);
  EXPECT_TRUE(made.kept(2, growth));
  // Control: the same plans through the saved form are kept.
  EXPECT_TRUE((Plans{{std::nullopt, SMITHY}}.kept(2, growth)));
  // A plan the growth does not have.
  EXPECT_FALSE((Plans{{std::nullopt, known}}.kept(2, growth)));
  // Not a plan or none for each structure.
  EXPECT_FALSE((Plans{{std::nullopt}}.kept(2, growth)));
}

TEST(Growth, BuildsWhatMostRaisesItsScarcestHolding) {
  const Growth growth = growing();
  const Husbandry eating{1, 3, 10};
  EXPECT_EQ(build_plan(plenty_but(HOUSE), growth, eating),
            std::optional<std::size_t>{HOUSE});
  EXPECT_EQ(build_plan(plenty_but(GRANARY), growth, eating),
            std::optional<std::size_t>{GRANARY});
  EXPECT_EQ(build_plan(plenty_but(FIELD), growth, eating),
            std::optional<std::size_t>{FIELD});
  EXPECT_EQ(build_plan(plenty_but(SMITHY), growth, eating),
            std::optional<std::size_t>{SMITHY});
  // A village arming itself needs no smithy: food is then its scarcest.
  Growth reliant = growth;
  reliant.self_reliance = PLENTY;
  EXPECT_EQ(build_plan(plenty_but(SMITHY), reliant, eating),
            std::optional<std::size_t>{FIELD});
  // No build raises what it lacks.
  Growth houses_only = growth;
  houses_only.builds = {HOUSE};
  EXPECT_FALSE(build_plan(plenty_but(GRANARY), houses_only, eating));
}

TEST(Growth, CountsFoodInPeopleFedAYear) {
  const Growth growth = growing();
  // Housing for 5, and 960 stored: 8 people a year eating one a day, 4
  // eating two.
  Holding held = plenty_but(GRANARY);
  held.housing = HOUSED;
  held.storage = STORED;
  EXPECT_EQ(build_plan(held, growth, Husbandry{1, 3, 10}),
            std::optional<std::size_t>{HOUSE});
  EXPECT_EQ(build_plan(held, growth, Husbandry{2, 3, 10}),
            std::optional<std::size_t>{GRANARY});
  // Without husbandry a person eats one a day.
  EXPECT_EQ(build_plan(held, growth, std::nullopt),
            std::optional<std::size_t>{HOUSE});
}

TEST(Growth, BuildsOnTheFreeCornerNearestItsCentre) {
  const Growth growth = growing();
  const Footprint two{2, 2};
  // Control: on open ground the nearest corner, the first row by row among
  // equals, centres the footprint on the centre's cell.
  const std::optional<Cell> open =
      build_site(two, MIDDLE, growth, open_ground(), SIZE);
  ASSERT_TRUE(open.has_value());
  EXPECT_EQ(open.value_or(Cell{}).x, BESIDE.x);
  EXPECT_EQ(open.value_or(Cell{}).y, BESIDE.y);

  // A covered cell keeps it off that cell and the cells around it.
  Ground covered = open_ground();
  covered.covered.at(slot(BESIDE)) = true;
  const std::optional<Cell> beside =
      build_site(two, MIDDLE, growth, covered, SIZE);
  ASSERT_TRUE(beside.has_value());
  const Cell away = beside.value_or(Cell{});
  EXPECT_TRUE(away.x >= 7 || away.y >= 7 || away.x + 1 <= 3 || away.y + 1 <= 3);

  // A rough cell keeps it off just as one covered does.
  Ground rough = open_ground();
  rough.rough.at(slot(BESIDE)) = true;
  const std::optional<Cell> off_rough =
      build_site(two, MIDDLE, growth, rough, SIZE);
  ASSERT_TRUE(off_rough.has_value());
  EXPECT_EQ(off_rough.value_or(Cell{}).x, away.x);
  EXPECT_EQ(off_rough.value_or(Cell{}).y, away.y);

  // A taken cell keeps it off only that cell: it may build beside it.
  Ground taken = open_ground();
  taken.taken.at(slot(BESIDE)) = true;
  const std::optional<Cell> by_taken =
      build_site(two, MIDDLE, growth, taken, SIZE);
  ASSERT_TRUE(by_taken.has_value());
  EXPECT_EQ(by_taken.value_or(Cell{}).x, MIDDLE.x);
  EXPECT_EQ(by_taken.value_or(Cell{}).y, BESIDE.y);

  // Nothing free within its vicinity.
  Ground full = open_ground();
  full.covered.assign(full.covered.size(), true);
  EXPECT_FALSE(build_site(two, MIDDLE, growth, full, SIZE).has_value());
}

TEST(Growth, FoundsOnlyOnFreeReachableUnclaimedLand) {
  const Growth growth = growing();
  const Map map = open_map();
  const FlowField paths{map, MIDDLE};
  const std::vector<std::optional<std::size_t>> unclaimed(
      static_cast<std::size_t>(SIZE.width) *
      static_cast<std::size_t>(SIZE.height));
  // Control: on open ground every 3 by 3 area whose centre lies within reach
  // and on the map is a site, row by row.
  const std::vector<Cell> open = settlement_sites(
      MIDDLE, growth, open_ground(), SIZE, paths, unclaimed, ORTHODOX);
  ASSERT_FALSE(open.empty());
  EXPECT_EQ(open.front().x, 1);
  EXPECT_EQ(open.front().y, 1);
  EXPECT_EQ(open.back().x, 9);
  EXPECT_EQ(open.back().y, 9);
  const std::size_t all = open.size();
  EXPECT_EQ(all, 81U);

  const auto has = [](const std::vector<Cell>& sites, Cell corner) {
    return std::any_of(sites.begin(), sites.end(), [corner](Cell site) {
      return site.x == corner.x && site.y == corner.y;
    });
  };
  // A covered cell keeps sites off it and within the spacing of it.
  Ground covered = open_ground();
  covered.covered.at(slot(MIDDLE)) = true;
  const std::vector<Cell> spaced = settlement_sites(
      MIDDLE, growth, covered, SIZE, paths, unclaimed, ORTHODOX);
  EXPECT_FALSE(has(spaced, BESIDE));
  EXPECT_FALSE(has(spaced, Cell{3, 3}));
  EXPECT_TRUE(has(spaced, Cell{2, 2}));
  // A taken cell keeps sites off it, but not around it.
  Ground taken = open_ground();
  taken.taken.at(slot(MIDDLE)) = true;
  const std::vector<Cell> by_taken =
      settlement_sites(MIDDLE, growth, taken, SIZE, paths, unclaimed, ORTHODOX);
  EXPECT_FALSE(has(by_taken, BESIDE));
  EXPECT_TRUE(has(by_taken, Cell{3, 3}));

  // A rough cell keeps sites off it as a taken one does.
  Ground rough = open_ground();
  rough.rough.at(slot(MIDDLE)) = true;
  const std::vector<Cell> off_rough =
      settlement_sites(MIDDLE, growth, rough, SIZE, paths, unclaimed, ORTHODOX);
  EXPECT_FALSE(has(off_rough, BESIDE));
  EXPECT_TRUE(has(off_rough, Cell{3, 3}));

  // Land another faction holds is no site; its own faction's is.
  std::vector<std::optional<std::size_t>> held = unclaimed;
  held.at(slot(Cell{1, 1})) = CULT;
  EXPECT_FALSE(has(settlement_sites(MIDDLE, growth, open_ground(), SIZE, paths,
                                    held, ORTHODOX),
                   Cell{1, 1}));
  EXPECT_TRUE(has(
      settlement_sites(MIDDLE, growth, open_ground(), SIZE, paths, held, CULT),
      Cell{1, 1}));

  // A site whose centre no path reaches is none.
  std::vector<std::string> walled(
      static_cast<std::size_t>(SIZE.height),
      std::string(static_cast<std::size_t>(SIZE.width), '.'));
  walled.at(4) = std::string(static_cast<std::size_t>(SIZE.width), '#');
  const Map split{walled};
  const FlowField cut{split, MIDDLE};
  const std::vector<Cell> reached = settlement_sites(
      MIDDLE, growth, open_ground(), SIZE, cut, unclaimed, ORTHODOX);
  EXPECT_FALSE(has(reached, Cell{1, 1}));
  EXPECT_TRUE(has(reached, Cell{1, 5}));
}

TEST(Growth, LaysItsLayoutFromTheSettlementsCorner) {
  const std::vector<Laid> laid = layout_at(Cell{3, 7}, growing());
  ASSERT_EQ(laid.size(), 2U);
  EXPECT_EQ(laid.at(0).plan, HOUSE);
  EXPECT_EQ(laid.at(0).cell.x, 3);
  EXPECT_EQ(laid.at(0).cell.y, 7);
  EXPECT_EQ(laid.at(1).plan, FIELD);
  EXPECT_EQ(laid.at(1).cell.x, 4);
  EXPECT_EQ(laid.at(1).cell.y, 9);
}

}  // namespace
}  // namespace murim
