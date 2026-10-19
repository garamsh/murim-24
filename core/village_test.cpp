#include "village.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

// Intact from 10, damaged from 5, destroyed from 1, rubble at 0.
constexpr std::array<std::uint32_t, 3> FLOORS{10, 5, 1};
constexpr std::uint32_t INTACT = 10;
constexpr std::uint32_t DAMAGED = 5;
constexpr std::uint32_t DESTROYED = 1;
constexpr std::uint32_t RUBBLE = 0;
constexpr std::size_t ARMIES = 4;
constexpr std::size_t FACTIONS = 2;
constexpr std::size_t CAPACITY = 500;
constexpr std::uint32_t HOUSE_STORAGE = 400;
constexpr std::uint32_t FIELD_STORAGE = 800;
constexpr std::uint32_t FIELD_HARVEST = 720;
constexpr std::uint32_t BARN_HOUSING = 9;
constexpr std::uint32_t BARN_STORAGE = 240;
constexpr std::uint32_t BIG_HARVEST = 1'200;
constexpr std::uint32_t HEALER_HEALING = 50;

std::vector<Structure> built(const std::vector<std::uint32_t>& durabilities) {
  std::vector<Structure> structures;
  structures.reserve(durabilities.size());
  for (const std::uint32_t durability : durabilities) {
    structures.push_back(Structure{Cell{0, 0}, durability, {1, 1}, true});
  }
  return structures;
}

// A village of structures 0 and 1, with army 0 as its residents, housing 3
// and 4, storing 400 and 800, yielding 0 and 720 a harvest.
VillageSpec homestead() {
  Holding house;
  house.housing = 3;
  house.storage = HOUSE_STORAGE;
  Holding field;
  field.housing = 4;
  field.storage = FIELD_STORAGE;
  field.harvest = FIELD_HARVEST;
  return VillageSpec{{0, 1}, 0, {house, field}, 0, {}, {}};
}

// Two structures in one village, added with both at `durability`.
Villages one_village(const std::vector<Structure>& structures) {
  Villages villages{structures.size()};
  villages.add(homestead(), ARMIES, 2, Stages{structures, FLOORS});
  return villages;
}

TEST(Stages, ReadEachStructureByTheFloorsItIsBelow) {
  const std::vector<Structure> structures =
      built({INTACT, DAMAGED, DESTROYED, RUBBLE});
  const Stages stages{structures, FLOORS};
  EXPECT_EQ(stages.of(0), Stage::INTACT);
  EXPECT_EQ(stages.of(1), Stage::DAMAGED);
  EXPECT_EQ(stages.of(2), Stage::DESTROYED);
  EXPECT_EQ(stages.of(3), Stage::RUBBLE);
  EXPECT_THROW(static_cast<void>(stages.of(4)), std::out_of_range);
}

TEST(Villages, StartWithStorageFullAndClaimTheirStructures) {
  const std::vector<Structure> structures = built({INTACT, DAMAGED, INTACT});
  const Villages villages = one_village(structures);
  ASSERT_EQ(villages.size(), 1U);
  // Both structures stand, so both stores are full.
  EXPECT_EQ(villages.at(0).stock, 1'200U);
  EXPECT_EQ(villages.village_of(0), std::optional<std::size_t>{0});
  EXPECT_EQ(villages.village_of(1), std::optional<std::size_t>{0});
  // Control: a structure the village does not name belongs to none.
  EXPECT_FALSE(villages.village_of(2).has_value());
  EXPECT_TRUE(villages.houses(0));
  EXPECT_FALSE(villages.houses(1));
}

TEST(Villages, RefuseAVillageNamingWhatIsNotFreeToName) {
  const std::vector<Structure> structures = built({INTACT, INTACT, INTACT});
  const Stages stages{structures, FLOORS};
  Villages villages = one_village(structures);
  // Control: structure 2 and army 1 are free, and one household pairs two.
  const VillageSpec free{{2}, 1, {}, 1, {}, {}};
  EXPECT_NO_THROW(villages.add(free, ARMIES, 2, stages));

  Villages fresh = one_village(structures);
  // A structure another village names.
  EXPECT_THROW(fresh.add(VillageSpec{{1}, 1, {}, 0, {}, {}}, ARMIES, 2, stages),
               std::invalid_argument);
  // A structure that does not exist.
  EXPECT_THROW(fresh.add(VillageSpec{{3}, 1, {}, 0, {}, {}}, ARMIES, 2, stages),
               std::invalid_argument);
  // An army another village houses.
  EXPECT_THROW(fresh.add(VillageSpec{{2}, 0, {}, 0, {}, {}}, ARMIES, 2, stages),
               std::invalid_argument);
  // An army that does not exist.
  EXPECT_THROW(
      fresh.add(VillageSpec{{2}, ARMIES, {}, 0, {}, {}}, ARMIES, 2, stages),
      std::invalid_argument);
  // Holdings that are not one for each structure.
  EXPECT_THROW(fresh.add(VillageSpec{{2}, 1, {Holding{}, Holding{}}, 0, {}, {}},
                         ARMIES, 2, stages),
               std::invalid_argument);
  // More households than its residents pair into.
  EXPECT_THROW(fresh.add(VillageSpec{{2}, 1, {}, 1, {}, {}}, ARMIES, 1, stages),
               std::invalid_argument);
  // Nothing was added by a refusal.
  EXPECT_EQ(fresh.size(), 1U);
  EXPECT_FALSE(fresh.village_of(2).has_value());
}

TEST(Villages, HoldWhatTheirStandingStructuresHoldAndIntactFieldsYield) {
  const std::vector<Structure> intact = built({INTACT, INTACT});
  // Control: every structure intact holds and yields in full.
  const Holding whole =
      one_village(intact).held(0, Stage::DAMAGED, Stages{intact, FLOORS});
  EXPECT_EQ(whole.housing, 7U);
  EXPECT_EQ(whole.storage, 1'200U);
  EXPECT_EQ(whole.harvest, 720U);

  const std::vector<Structure> damaged = built({INTACT, DAMAGED});
  const Stages stages{damaged, FLOORS};
  const Villages villages = one_village(damaged);
  // A damaged field still stands and stores, but yields no harvest.
  const Holding standing = villages.held(0, Stage::DAMAGED, stages);
  EXPECT_EQ(standing.housing, 7U);
  EXPECT_EQ(standing.storage, 1'200U);
  EXPECT_EQ(standing.harvest, 0U);
  // Counting only intact structures leaves the damaged one out.
  const Holding unbroken = villages.held(0, Stage::INTACT, stages);
  EXPECT_EQ(unbroken.housing, 3U);
  EXPECT_EQ(unbroken.storage, 400U);

  const std::vector<Structure> destroyed = built({INTACT, DESTROYED});
  // A destroyed structure no longer stands.
  EXPECT_EQ(one_village(destroyed)
                .held(0, Stage::DAMAGED, Stages{destroyed, FLOORS})
                .housing,
            3U);
}

TEST(Villages, CapAtTheLeastOfHousingHarvestAndStorage) {
  const Husbandry eating{1, 3, 10};
  const std::vector<Structure> intact = built({INTACT, INTACT});
  const Stages stages{intact, FLOORS};
  const Villages villages = one_village(intact);
  // Housing 7, harvest 720 / 120 a year = 6, storage 1200 / 120 = 10.
  EXPECT_EQ(villages.cap(0, stages, eating, CAPACITY), 6U);
  // Control: without husbandry the cap is the capacity.
  EXPECT_EQ(villages.cap(0, stages, std::nullopt, CAPACITY), CAPACITY);
  // Eating nothing, food binds no one: housing does.
  EXPECT_EQ(villages.cap(0, stages, Husbandry{0, 3, 10}, CAPACITY), 7U);

  Holding barn;
  barn.housing = BARN_HOUSING;
  barn.storage = BARN_STORAGE;
  Holding field;
  field.harvest = BIG_HARVEST;
  Villages small_store{intact.size()};
  small_store.add(VillageSpec{{0, 1}, 0, {barn, field}, 0, {}, {}}, ARMIES, 0,
                  stages);
  // Housing 9, harvest 10, storage 2: what it stores binds.
  EXPECT_EQ(small_store.cap(0, stages, eating, CAPACITY), 2U);

  const std::vector<Structure> burnt = built({INTACT, DAMAGED});
  // A burnt field yields nothing, so it feeds no one.
  EXPECT_EQ(one_village(burnt).cap(0, Stages{burnt, FLOORS}, eating, CAPACITY),
            0U);
}

TEST(Villages, FillAtHarvestUpToStorageAndFeedAllOrNone) {
  const std::vector<Structure> intact = built({INTACT, INTACT});
  const Stages stages{intact, FLOORS};
  Villages villages = one_village(intact);
  // Control: a stock that feeds everyone feeds everyone.
  EXPECT_EQ(villages.feed({5}, 2, false, stages), std::vector<bool>{true});
  EXPECT_EQ(villages.at(0).stock, 1'190U);
  // A harvest fills the stock only up to its storage.
  EXPECT_EQ(villages.feed({0}, 2, true, stages), std::vector<bool>{true});
  EXPECT_EQ(villages.at(0).stock, 1'200U);
  // A stock short of feeding them all feeds none, and is eaten up.
  EXPECT_EQ(villages.feed({601}, 2, false, stages), std::vector<bool>{false});
  EXPECT_EQ(villages.at(0).stock, 0U);
  // Without a harvest an empty stock stays empty.
  EXPECT_EQ(villages.feed({0}, 2, false, stages), std::vector<bool>{true});
  EXPECT_EQ(villages.at(0).stock, 0U);
  // The harvest refills it.
  EXPECT_EQ(villages.feed({0}, 2, true, stages), std::vector<bool>{true});
  EXPECT_EQ(villages.at(0).stock, 720U);
}

TEST(Villages, ServeOnlyWhileTheirWorkshopsCoverTheirPeople) {
  Holding healer;
  healer.tends = 4;
  healer.healing = HEALER_HEALING;
  const std::vector<Structure> intact = built({INTACT});
  const Stages stages{intact, FLOORS};
  Villages villages{intact.size()};
  villages.add(VillageSpec{{0}, 0, {healer}, 0, {}, {}}, ARMIES, 0, stages);
  // Within what it tends, it heals.
  EXPECT_EQ(villages.served({4}, 0, stages).healing,
            std::vector<std::uint32_t>{HEALER_HEALING});
  // Past what the healer tends, it heals no one.
  EXPECT_EQ(villages.served({5}, 0, stages).healing,
            std::vector<std::uint32_t>{0});
  // Self-reliance covers the rest.
  EXPECT_EQ(villages.served({5}, 1, stages).healing,
            std::vector<std::uint32_t>{HEALER_HEALING});
}

TEST(Villages, ShelterOnlyWithRoomAndFood) {
  const Husbandry eating{1, 3, 10};
  const std::vector<Structure> intact = built({INTACT, INTACT});
  const Stages stages{intact, FLOORS};
  Villages villages = one_village(intact);
  // Control: below its cap of 6, with food in store, it takes one in.
  EXPECT_TRUE(villages.shelters(0, 5, stages, eating, CAPACITY));
  // At its cap there is no room.
  EXPECT_FALSE(villages.shelters(0, 6, stages, eating, CAPACITY));
  // With room but nothing in store, it takes no one in.
  static_cast<void>(
      villages.feed({HOUSE_STORAGE + FIELD_STORAGE + 1}, 1, false, stages));
  ASSERT_EQ(villages.at(0).stock, 0U);
  EXPECT_FALSE(villages.shelters(0, 0, stages, eating, CAPACITY));
}

TEST(Villages, NoteTheirDestroyerOnceAndGiveOnlyAnEmptyVillageAway) {
  const Destroyer striker{2, 1};
  const std::vector<Structure> ruined = built({RUBBLE, RUBBLE});
  const Stages stages{ruined, FLOORS};
  Villages villages = one_village(built({INTACT, INTACT}));
  // A strike leaving every structure rubble after one stood ruins it.
  EXPECT_EQ(villages.shifted(0, true, false, true, striker, stages),
            VillageChange::RUINED);
  ASSERT_TRUE(villages.at(0).destroyer.has_value());
  EXPECT_EQ(villages.at(0).destroyer.value_or(Destroyer{0, 0}).army, 2U);
  // Striking rubble that stood no longer does not ruin it again.
  EXPECT_EQ(villages.shifted(0, false, false, true, Destroyer{3, 0}, stages),
            VillageChange::NONE);
  EXPECT_EQ(villages.at(0).destroyer.value_or(Destroyer{0, 0}).army, 2U);

  const std::vector<Structure> rising = built({DESTROYED, RUBBLE});
  const Stages raised{rising, FLOORS};
  // Control: a structure left standing does not ruin the village.
  EXPECT_EQ(villages.shifted(0, true, false, true, striker, raised),
            VillageChange::NONE);
  // Raising one while a resident lives leaves the village its residents'.
  EXPECT_EQ(villages.shifted(0, false, true, true, striker, raised),
            VillageChange::NONE);
  EXPECT_EQ(villages.at(0).spec.residents, 0U);
  // Raising one while none lives gives it to the striker's army.
  EXPECT_EQ(villages.shifted(0, false, true, false, striker, raised),
            VillageChange::RETAKEN);
  EXPECT_EQ(villages.at(0).spec.residents, 2U);
  // The destroyer is kept through the rebuilding.
  EXPECT_EQ(villages.at(0).destroyer.value_or(Destroyer{0, 0}).army, 2U);
}

TEST(Villages, ShowAFieldBySeasonUntilItIsBurnt) {
  const std::vector<Structure> intact = built({INTACT, INTACT, INTACT});
  const Villages villages = one_village(intact);
  const Stages stages{intact, FLOORS};
  EXPECT_EQ(villages.field_state(1, stages, 2), FieldState::RIPE);
  // A structure yielding no harvest, or in no village, is no field.
  EXPECT_FALSE(villages.field_state(0, stages, 2).has_value());
  EXPECT_FALSE(villages.field_state(2, stages, 2).has_value());
  const std::vector<Structure> damaged = built({INTACT, DAMAGED, INTACT});
  EXPECT_EQ(villages.field_state(1, Stages{damaged, FLOORS}, 2),
            FieldState::BURNT);
}

TEST(Villages, TakeLaidStructuresAndFoundedVillages) {
  const std::vector<Structure> intact = built({INTACT, INTACT});
  Villages villages = one_village(intact);
  villages.add_structure();
  ASSERT_EQ(villages.membership().size(), 3U);
  EXPECT_FALSE(villages.village_of(2).has_value());
  const std::size_t founded = villages.found(3, Cell{4, 5});
  EXPECT_EQ(founded, 1U);
  EXPECT_EQ(villages.at(founded).stock, 0U);
  const std::optional<Cell> corner = villages.at(founded).founded;
  ASSERT_TRUE(corner.has_value());
  EXPECT_EQ(corner.value_or(Cell{0, 0}).x, 4);
  EXPECT_EQ(corner.value_or(Cell{0, 0}).y, 5);
  EXPECT_TRUE(villages.houses(3));
  Holding house;
  house.housing = 2;
  villages.attach(founded, 2, house, "house");
  EXPECT_EQ(villages.village_of(2), std::optional<std::size_t>{founded});
  EXPECT_EQ(villages.at(founded).spec.structures, std::vector<std::size_t>{2});
  EXPECT_EQ(villages.at(founded).spec.holdings.size(), 1U);
  // Control: a village the world was created with records no founding.
  EXPECT_FALSE(villages.at(0).founded.has_value());
}

TEST(Villages, RecallTheirPartyOrLetItGo) {
  Villages villages = one_village(built({INTACT, INTACT}));
  villages.send(0, Raid{2, 0});
  EXPECT_EQ(villages.at(0).party, std::optional<std::size_t>{2});
  EXPECT_EQ(villages.at(0).raiding, std::optional<std::size_t>{0});
  villages.end_raid(0);
  EXPECT_FALSE(villages.at(0).raiding.has_value());
  EXPECT_EQ(villages.at(0).party, std::optional<std::size_t>{2});
  villages.send(0, Raid{2, 0});
  villages.release(0);
  EXPECT_FALSE(villages.at(0).raiding.has_value());
  EXPECT_FALSE(villages.at(0).party.has_value());
}

TEST(Villages, RefuseSavedVillagesNamingWhatDoesNotExist) {
  const std::vector<Structure> structures = built({INTACT, INTACT});
  const VillageRecord home{homestead(),  std::nullopt, 0,
                           std::nullopt, std::nullopt, std::nullopt};
  const std::vector<std::optional<std::size_t>> both{0, 0};
  // Control: a village as add() makes it is kept.
  EXPECT_TRUE(one_village(structures).kept(structures, ARMIES, FACTIONS));
  // Control: the same village through the saved form is kept.
  EXPECT_TRUE(Villages({home}, both).kept(structures, ARMIES, FACTIONS));
  const auto kept = [&both, &structures](const VillageRecord& record) {
    return Villages({record}, both).kept(structures, ARMIES, FACTIONS);
  };
  VillageRecord stray = home;
  stray.spec.structures = {0, structures.size()};
  EXPECT_FALSE(kept(stray));
  stray = home;
  stray.spec.residents = ARMIES;
  EXPECT_FALSE(kept(stray));
  stray = home;
  stray.spec.holdings.pop_back();
  EXPECT_FALSE(kept(stray));
  stray = home;
  stray.destroyer = Destroyer{ARMIES, 0};
  EXPECT_FALSE(kept(stray));
  stray = home;
  stray.destroyer = Destroyer{0, FACTIONS};
  EXPECT_FALSE(kept(stray));
  stray = home;
  stray.party = ARMIES;
  EXPECT_FALSE(kept(stray));
  stray = home;
  stray.raiding = 1;
  EXPECT_FALSE(kept(stray));
  // A structure's village that does not exist.
  EXPECT_FALSE(Villages({home}, {0, 1}).kept(structures, ARMIES, FACTIONS));
  // Not one village or none for each structure.
  EXPECT_FALSE(Villages({home}, {0}).kept(structures, ARMIES, FACTIONS));
}

// The holder of `cell` in `held`, a row-by-row grid of `size`.
std::optional<std::size_t> holder_at(
    const std::vector<std::optional<std::size_t>>& held, Footprint size,
    Cell cell) {
  return held.at((static_cast<std::size_t>(cell.y) *
                  static_cast<std::size_t>(size.width)) +
                 static_cast<std::size_t>(cell.x));
}

TEST(Villages, ClaimEachCellForTheNearestCentreWithinTheClaim) {
  constexpr Footprint SIZE{10, 10};
  constexpr std::int32_t CLAIM = 2;
  const std::vector<std::pair<Cell, std::size_t>> centres{{Cell{2, 2}, 0},
                                                          {Cell{6, 2}, 1}};
  const std::vector<std::optional<std::size_t>> held =
      claimed(SIZE, CLAIM, centres);
  ASSERT_EQ(held.size(), 100U);
  EXPECT_EQ(holder_at(held, SIZE, Cell{2, 2}), std::optional<std::size_t>{0});
  EXPECT_EQ(holder_at(held, SIZE, Cell{5, 2}), std::optional<std::size_t>{1});
  // Equally near both, the earlier centre holds it.
  EXPECT_EQ(holder_at(held, SIZE, Cell{4, 2}), std::optional<std::size_t>{0});
  EXPECT_EQ(holder_at(held, SIZE, Cell{4, 4}), std::optional<std::size_t>{0});
  // Beyond the claim along an axis, no one holds it.
  EXPECT_FALSE(holder_at(held, SIZE, Cell{2, 5}).has_value());
  // Control: with no centres, no cell is held.
  const std::vector<std::optional<std::size_t>> none = claimed(SIZE, CLAIM, {});
  EXPECT_TRUE(std::none_of(none.begin(), none.end(),
                           [](const std::optional<std::size_t>& holder) {
                             return holder.has_value();
                           }));
}

}  // namespace
}  // namespace murim
