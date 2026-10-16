#include "schedule.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "village.hpp"

namespace murim {
namespace {

constexpr std::uint64_t TICKS_A_DAY = 1'800;
// A day at home, at work from 7 and home again from 20.
constexpr std::uint32_t TO_WORK = 7;
constexpr std::uint32_t BACK_HOME = 20;
// Waking at 6 and sleeping at 22.
constexpr Waking AWAKE{6, 22};
// The side of the door tests' open map.
constexpr std::size_t DOOR_MAP_SIDE = 7;

TEST(Schedule, AnHourIsTheTickWithinItsDayScaledTo24) {
  EXPECT_EQ(hour_of(0, TICKS_A_DAY), 0U);
  EXPECT_EQ(hour_of(74, TICKS_A_DAY), 0U);
  EXPECT_EQ(hour_of(75, TICKS_A_DAY), 1U);
  EXPECT_EQ(hour_of(TICKS_A_DAY - 1, TICKS_A_DAY), 23U);
  EXPECT_EQ(hour_of(TICKS_A_DAY + 75, TICKS_A_DAY), 1U);
  // A day of 5 ticks, as the sweep lives, spans a band of hours a tick.
  EXPECT_EQ(hour_of(4, 5), 19U);
}

TEST(Schedule, TheBandInForceIsTheLastBeginningAtOrBeforeTheHour) {
  const std::vector<Band> day{
      {0, Where::HOME}, {TO_WORK, Where::WORK}, {BACK_HOME, Where::HOME}};
  EXPECT_EQ(band_at(day, 0), 0U);
  EXPECT_EQ(band_at(day, TO_WORK - 1), 0U);
  EXPECT_EQ(band_at(day, TO_WORK), 1U);
  EXPECT_EQ(band_at(day, BACK_HOME - 1), 1U);
  EXPECT_EQ(band_at(day, BACK_HOME), 2U);
  EXPECT_EQ(band_at(day, HOURS_PER_DAY - 1), 2U);
}

TEST(Schedule, APersonIsAwakeFromItsWakingHourUntilItsSleepingOne) {
  EXPECT_FALSE(awake(AWAKE, AWAKE.from - 1));
  EXPECT_TRUE(awake(AWAKE, AWAKE.from));
  EXPECT_TRUE(awake(AWAKE, AWAKE.to - 1));
  EXPECT_FALSE(awake(AWAKE, AWAKE.to));
}

// A living person married, in order, to `spouses`, child of `parents`.
Person person(std::vector<std::size_t> spouses,
              std::vector<std::size_t> parents) {
  Person made;
  made.health = 1;
  made.spouses = std::move(spouses);
  made.parents = std::move(parents);
  return made;
}

TEST(Schedule, AHouseholdIsACoupleWithTheirUnwedChildrenAndAnyoneElseAlone) {
  // 0 and 1 are married, with 2 unwed and 3 married to 4; 5 is single; 6 is
  // widowed of 7, who is dead, with 8 unwed.
  constexpr std::size_t WIDOW = 6;
  constexpr std::size_t LATE_HUSBAND = 7;
  std::vector<Person> people{person({1}, {}),
                             person({0}, {}),
                             person({}, {0, 1}),
                             person({4}, {0, 1}),
                             person({3}, {}),
                             person({}, {}),
                             person({LATE_HUSBAND}, {}),
                             person({WIDOW}, {}),
                             person({}, {WIDOW, LATE_HUSBAND})};
  people.at(LATE_HUSBAND).health = 0;
  // The unwed child 2 stands before its parents, so their household comes
  // second, after 5's, and lists 2 first.
  const std::vector<std::size_t> residents{5, 2, 0, 1, 3, 4, 6, 8};
  EXPECT_EQ(households(residents, people),
            (std::vector<std::vector<std::size_t>>{
                {5}, {2, 0, 1}, {3, 4}, {6}, {8}}));
  // Control: with 1 no resident, 0 is married to no one living among them,
  // so 0 and its child 2 each live alone.
  const std::vector<std::size_t> without_wife{5, 2, 0, 3, 4, 6, 8};
  EXPECT_EQ(
      households(without_wife, people),
      (std::vector<std::vector<std::size_t>>{{5}, {2}, {0}, {3, 4}, {6}, {8}}));
}

TEST(Schedule, AHouseholdTakesTheFirstHouseWithRoomForAllOfIt) {
  const std::vector<std::optional<std::size_t>> homes =
      house({3, 2, 4, 1}, {4, 4});
  EXPECT_EQ(homes,
            (std::vector<std::optional<std::size_t>>{0, 1, std::nullopt, 0}));
}

TEST(Schedule, ADoorIsThePassableCellBesideTheFootprintNearestTheCentre) {
  const Structure house{{2, 2}, 1, {2, 2}, true, Role::HOME};
  Map open{
      std::vector<std::string>(DOOR_MAP_SIDE, std::string(DOOR_MAP_SIDE, '.'))};
  EXPECT_EQ(door(open, house, {6, 3}).value_or(Cell{-1, -1}).x, 4);
  EXPECT_EQ(door(open, house, {6, 3}).value_or(Cell{-1, -1}).y, 3);
  // Control: with that cell blocked, the next nearest is the door.
  open.block({4, 3});
  EXPECT_EQ(door(open, house, {6, 3}).value_or(Cell{-1, -1}).y, 2);
}

TEST(Schedule, AmongDoorsEquallyNearTheFirstRowByRowIsTaken) {
  const Structure hut{{3, 3}, 1, {1, 1}, true, Role::HOME};
  Map open{
      std::vector<std::string>(DOOR_MAP_SIDE, std::string(DOOR_MAP_SIDE, '.'))};
  // Each cell beside the hut lies one from its centre, the hut's own cell.
  const Cell entry = door(open, hut, {3, 3}).value_or(Cell{-1, -1});
  EXPECT_EQ(entry.x, 3);
  EXPECT_EQ(entry.y, 2);
  for (const Cell beside : {Cell{3, 2}, Cell{2, 3}, Cell{4, 3}, Cell{3, 4}}) {
    open.block(beside);
  }
  EXPECT_FALSE(door(open, hut, {3, 3}).has_value());
}

TEST(Schedule, AnErrandDrawDependsOnItsKeyAlone) {
  EXPECT_EQ(errand_draw(1, 2, 3, 4), errand_draw(1, 2, 3, 4));
  EXPECT_NE(errand_draw(1, 2, 3, 4), errand_draw(1, 2, 3, 5));
  EXPECT_NE(errand_draw(1, 2, 3, 4), errand_draw(1, 3, 3, 4));
}

}  // namespace
}  // namespace murim
