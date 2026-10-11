#include "save.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "arts.hpp"
#include "calendar.hpp"
#include "diplomacy.hpp"
#include "json_packs.hpp"
#include "leadership.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "region.hpp"
#include "skill.hpp"
#include "talk.hpp"
#include "trade.hpp"
#include "village.hpp"
#include "world.hpp"

namespace murim {
namespace {

// The base pack's tables, read once from the packs directory.
const RegionData& base_data() {
  static const RegionData data = murim_packs::load_base(MURIM_PACKS_DIR);
  return data;
}

constexpr std::uint64_t TICKS_PER_SHORT_DAY = 5;
constexpr std::uint64_t YEARS = 10;
constexpr int MORE_TICKS = 1'000;
constexpr Seed REGION_SEED{7};
constexpr Seed OTHER_SEED{8};
constexpr Slot SLOT{0};
constexpr Slot OTHER_SLOT{1};
// The magic, the version and the payload's length.
constexpr std::size_t HEADER_BYTES = 20;
constexpr std::size_t VERSION_AT = 8;
constexpr std::size_t CHECKSUM_BYTES = sizeof(std::uint64_t);
constexpr std::size_t BYTE_BITS = 8;
constexpr std::array<std::uint32_t, 3> STAGE_FLOORS{75, 25, 1};

// The test harness's save store: bytes in memory, by slot.
class MemorySaves : public SaveStore {
 public:
  bool put(Slot slot, const std::vector<std::uint8_t>& bytes) override {
    saves_[slot] = bytes;
    return true;
  }

  [[nodiscard]] std::optional<std::vector<std::uint8_t>> get(
      Slot slot) const override {
    const auto found = saves_.find(slot);
    if (found == saves_.end()) {
      return std::nullopt;
    }
    return found->second;
  }

  [[nodiscard]] std::vector<std::uint8_t>& bytes(Slot slot) {
    return saves_.at(slot);
  }

 private:
  std::map<Slot, std::vector<std::uint8_t>> saves_;
};

// The generated region of `seed`, with days of five ticks and a player born
// into its first village carrying a soul, lived `YEARS` years.
Saved lived_region(Seed seed) {
  Region region = generate_region(seed, base_data());
  region.rules.ticks_per_day = TICKS_PER_SHORT_DAY;
  region.armies.push_back(born_in(region, 0));
  World world{region.seed,       Map{region.map_rows}, region.armies,
              region.structures, region.rules,         village_specs(region),
              region.leadership, region.life,          region.sects};
  world.ensoul(world.population() - 1);
  for (std::uint64_t tick = 0;
       tick < YEARS * DAYS_PER_YEAR * TICKS_PER_SHORT_DAY; ++tick) {
    world.tick();
  }
  return Saved{Origin{seed, 0}, std::move(world)};
}

World ticked(World world, int ticks) {
  for (int tick = 0; tick < ticks; ++tick) {
    world.tick();
  }
  return world;
}

Saved loaded(std::variant<Saved, LoadError> result) {
  return std::get<Saved>(std::move(result));
}

std::optional<LoadError> refusal(const std::variant<Saved, LoadError>& result) {
  if (const auto* error = std::get_if<LoadError>(&result)) {
    return *error;
  }
  return std::nullopt;
}

// FNV-1a, as a save's checksum is computed.
std::uint64_t fnv(const std::vector<std::uint8_t>& bytes, std::size_t last) {
  constexpr std::uint64_t OFFSET = 0xCBF29CE484222325ULL;
  constexpr std::uint64_t PRIME = 0x100000001B3ULL;
  std::uint64_t digest = OFFSET;
  for (std::size_t index = 0; index < last; ++index) {
    digest = (digest ^ bytes.at(index)) * PRIME;
  }
  return digest;
}

// Writes `value` little-endian over the last eight of `bytes`.
void write_last_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  const std::size_t first = bytes.size() - sizeof(value);
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    bytes.at(first + byte) =
        static_cast<std::uint8_t>(value >> (byte * BYTE_BITS));
  }
}

std::vector<std::uint8_t> first_bytes(const std::vector<std::uint8_t>& bytes,
                                      std::size_t count) {
  return {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(count)};
}

// A save of `header`, as a save begins, and `payload`, its length and checksum
// made to match, so only what the payload holds can refuse it.
std::vector<std::uint8_t> resealed(
    const std::array<std::uint8_t, HEADER_BYTES>& header,
    const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> bytes(header.begin(), header.end());
  write_last_u64(bytes, payload.size());
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  bytes.resize(bytes.size() + CHECKSUM_BYTES);
  write_last_u64(bytes, fnv(bytes, bytes.size() - CHECKSUM_BYTES));
  return bytes;
}

TEST(Save, ALoadedRegionHashesAndTicksOnAsTheSavedOne) {
  const Saved saved = lived_region(REGION_SEED);
  ASSERT_GT(saved.world.date().year, YEARS);
  MemorySaves store;
  ASSERT_TRUE(save(saved, store, SLOT));

  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.hash(), saved.world.hash());
  EXPECT_TRUE(restored.origin && restored.origin->region == REGION_SEED);
  EXPECT_EQ(ticked(restored.world, MORE_TICKS).hash(),
            ticked(saved.world, MORE_TICKS).hash());
  // Control: another region's world differs at both points, so equality is
  // not a hash every world shares.
  const Saved other = lived_region(OTHER_SEED);
  EXPECT_NE(other.world.hash(), saved.world.hash());
  EXPECT_NE(ticked(other.world, MORE_TICKS).hash(),
            ticked(saved.world, MORE_TICKS).hash());
}

// Each person's qi, store, cultivation and known art skills in `world`.
std::vector<Person> cultivated(const World& world) {
  std::vector<Person> people;
  for (std::size_t person = 0; person < world.population(); ++person) {
    const Person& each = world.person(person);
    Person kept;
    kept.cultivation = each.cultivation;
    kept.qi = each.qi;
    kept.qi_capacity = each.qi_capacity;
    kept.known = each.known;
    people.push_back(kept);
  }
  return people;
}

TEST(Save, ALoadedRegionKeepsEveryonesQiAndArts) {
  const Saved saved = lived_region(REGION_SEED);
  const std::vector<Person> before = cultivated(saved.world);
  // Ten years in, people hold qi and sects have taught their arts.
  ASSERT_TRUE(std::any_of(before.begin(), before.end(), [](const Person& each) {
    return !each.known.empty() && each.qi > 0;
  }));
  MemorySaves store;
  ASSERT_TRUE(save(saved, store, SLOT));
  const std::vector<Person> after = cultivated(loaded(load(store, SLOT)).world);
  ASSERT_EQ(after.size(), before.size());
  for (std::size_t person = 0; person < before.size(); ++person) {
    const Person& then = before.at(person);
    const Person& now = after.at(person);
    EXPECT_EQ(now.qi, then.qi);
    EXPECT_EQ(now.qi_capacity, then.qi_capacity);
    EXPECT_EQ(now.cultivation.growth, then.cultivation.growth);
    EXPECT_EQ(now.cultivation.limit, then.cultivation.limit);
    ASSERT_EQ(now.known.size(), then.known.size());
    for (std::size_t known = 0; known < then.known.size(); ++known) {
      EXPECT_EQ(now.known.at(known).art, then.known.at(known).art);
      EXPECT_EQ(now.known.at(known).skill, then.known.at(known).skill);
      EXPECT_EQ(now.known.at(known).ready_at, then.known.at(known).ready_at);
    }
  }
}

TEST(Save, ARefusedSaveNamesWhyAndAnIntactOneLoads) {
  const World world{Seed{1},
                    Map{{"....", "...."}},
                    {},
                    {},
                    Rules{0, {{false}}, STAGE_FLOORS}};
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  // Control: the intact bytes load, so each refusal below is the change's.
  EXPECT_EQ(loaded(load(store, SLOT)).world.hash(), world.hash());
  EXPECT_FALSE(loaded(load(store, SLOT)).origin.has_value());

  EXPECT_EQ(refusal(load(store, OTHER_SLOT)), LoadError::MISSING);

  for (const std::size_t flipped :
       {HEADER_BYTES, intact.size() / 2, intact.size() - 1}) {
    store.bytes(SLOT) = intact;
    store.bytes(SLOT).at(flipped) ^= 1U;
    EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED) << flipped;
  }

  for (const std::size_t length :
       {std::size_t{0}, std::size_t{3}, VERSION_AT, HEADER_BYTES - 1,
        HEADER_BYTES, intact.size() / 2, intact.size() - 1}) {
    store.bytes(SLOT) = first_bytes(intact, length);
    EXPECT_EQ(refusal(load(store, SLOT)), LoadError::TRUNCATED) << length;
  }

  store.bytes(SLOT) = intact;
  store.bytes(SLOT).push_back(0);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);

  store.bytes(SLOT) = intact;
  store.bytes(SLOT).at(0) = 'X';
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::NOT_A_SAVE);

  for (const std::uint32_t version : {SAVE_VERSION - 1, SAVE_VERSION + 1}) {
    store.bytes(SLOT) = intact;
    store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(version);
    EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION)
        << version;
  }
}

TEST(Save, ASealedSaveNamingARecordTheWorldLacksIsRefused) {
  // One structure no village holds and no plan laid: the payload ends with
  // its plan, absent, as one flag byte.
  const World world{Seed{1},
                    Map{{"....", "...."}},
                    {},
                    {Structure{{0, 0}, 100, {1, 1}, true}},
                    Rules{0, {{false}}, STAGE_FLOORS}};
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  ASSERT_EQ(payload.back(), 0);

  // Control: resealing the same payload loads, so the reseal refuses nothing.
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(loaded(load(store, SLOT)).world.hash(), world.hash());

  // The structure now names plan 0 of a world without growth, so no plans.
  payload.back() = 1;
  payload.resize(payload.size() + sizeof(std::uint64_t), 0);
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

TEST(Save, ASealedSaveNamingAPersonNoEventCanNameIsRefused) {
  // Two hostile people strike each other dead on the first tick: the save
  // keeps both deaths, then the souls, surnames, structures' villages and
  // plans, each an empty list.
  const Skill once{2 * CELL_SIZE, 0, 1, 0, 1'000};
  World world{Seed{1},
              Map{{"....", "...."}},
              {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {once}},
               ArmySpec{{1, 0}, {Recruit{{1, 0}, 1}}, 1, {once}}},
              {},
              Rules{0, {{false, true}, {true, false}}, STAGE_FLOORS}};
  world.tick();
  ASSERT_EQ(world.events().size(), 2U);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The last event's person, before four empty lists.
  constexpr std::size_t EMPTY_LISTS = 4 * sizeof(std::uint64_t);
  const std::size_t person =
      payload.size() - EMPTY_LISTS - sizeof(std::uint64_t);
  ASSERT_EQ(payload.at(person), 1);
  // Control: resealing the same payload loads with its events.
  store.bytes(SLOT) = resealed(header, payload);
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.hash(), world.hash());
  EXPECT_EQ(restored.world.events().size(), 2U);
  // The death now names a person the world never held.
  payload.at(person) = 2;
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

TEST(Save, ASealedSaveNamingARealmNoProgressCanNameIsRefused) {
  // One person cultivating 10 qi a day reaches the one realm, at 10, on the
  // first day: the save keeps that step of progress, then the events, souls,
  // surnames, structures' villages and plans, each an empty list.
  constexpr std::uint32_t DAILY = 10;
  const Life life{{{"sword", {1, 0}, {1, 0}}},
                  1,
                  std::nullopt,
                  0,
                  0,
                  0,
                  std::nullopt,
                  std::nullopt,
                  {},
                  std::nullopt,
                  std::nullopt,
                  std::nullopt,
                  Cultivation{{DAILY, 0}, {DAILY, 0}, 0, 1},
                  {Realm{{"삼류", "Third-rate"}, DAILY, PER_MILLE}},
                  {},
                  Learning{},
                  std::nullopt,
                  std::nullopt};
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.ticks_per_day = 1;
  World world{Seed{1},
              Map{{"....", "...."}},
              {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{}}}},
              {},
              rules,
              {},
              {},
              life};
  world.tick();
  ASSERT_EQ(world.progress().size(), 1U);
  ASSERT_TRUE(world.events().empty());
  const auto* gained = std::get_if<RealmGained>(&world.progress().back().what);
  ASSERT_NE(gained, nullptr);
  ASSERT_EQ(gained->rank, 1U);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The step's rank, before five empty lists.
  constexpr std::size_t EMPTY_LISTS = 5 * sizeof(std::uint64_t);
  const std::size_t rank = payload.size() - EMPTY_LISTS - sizeof(std::uint64_t);
  ASSERT_EQ(payload.at(rank), 1);
  // Control: resealing the same payload loads with its step.
  store.bytes(SLOT) = resealed(header, payload);
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.hash(), world.hash());
  ASSERT_EQ(restored.world.progress().size(), 1U);
  const auto* kept =
      std::get_if<RealmGained>(&restored.world.progress().back().what);
  ASSERT_NE(kept, nullptr);
  EXPECT_EQ(kept->person, 0U);
  EXPECT_EQ(kept->rank, 1U);
  // The step now names a realm past the one the world holds.
  payload.at(rank) = 2;
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

TEST(Save, ALoadedRegionKeepsItsLessonsAndRealmsNoted) {
  const Saved saved = lived_region(REGION_SEED);
  const std::vector<Progress>& before = saved.world.progress();
  // Ten years in, the progress kept holds skills learned and realms gained.
  ASSERT_TRUE(
      std::any_of(before.begin(), before.end(), [](const Progress& each) {
        return std::holds_alternative<Learned>(each.what);
      }));
  ASSERT_TRUE(
      std::any_of(before.begin(), before.end(), [](const Progress& each) {
        return std::holds_alternative<RealmGained>(each.what);
      }));
  MemorySaves store;
  ASSERT_TRUE(save(saved, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  const std::vector<Progress>& after = restored.world.progress();
  ASSERT_EQ(after.size(), before.size());
  for (std::size_t step = 0; step < before.size(); ++step) {
    EXPECT_EQ(after.at(step).sequence, before.at(step).sequence);
    EXPECT_EQ(after.at(step).what.index(), before.at(step).what.index());
    const auto* then = std::get_if<Learned>(&before.at(step).what);
    const auto* now = std::get_if<Learned>(&after.at(step).what);
    if (then != nullptr && now != nullptr) {
      EXPECT_EQ(now->person, then->person);
      EXPECT_EQ(now->art, then->art);
      EXPECT_EQ(now->skill, then->skill);
      EXPECT_EQ(now->by, then->by);
    }
  }
}

// Two hostile people a cell apart, the first casting its skill at `target`
// on the next tick.
World casting(const Target& target) {
  constexpr std::uint64_t HEALTH = 10;
  const Skill bolt{2 * CELL_SIZE, 0, 1, 0, 1'000};
  World world{Seed{1},
              Map{{"....", "...."}},
              {ArmySpec{{0, 0}, {Recruit{{0, 0}, HEALTH}}, 0, {bolt}},
               ArmySpec{{1, 0}, {Recruit{{1, 0}, HEALTH}}, 1, {bolt}}},
              {},
              Rules{0, {{false, true}, {true, false}}, STAGE_FLOORS}};
  world.cast(0, 0, target);
  return world;
}

TEST(Save, APendingCastLoadsAndLandsAsTheSavedOne) {
  const World world = casting(Point{centre_of(Cell{1, 0})});
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));

  EXPECT_EQ(restored.world.hash(), world.hash());
  EXPECT_EQ(ticked(restored.world, 1).hash(), ticked(world, 1).hash());
  // Control: a cast at itself is another state.
  EXPECT_NE(casting(Itself{}).hash(), world.hash());
}

// Two people of factions at peace, the first leading its faction, with
// diplomacy holding a declaration a day; the first's army steered and at
// war with the second's when `ordered`.
World diplomatic(bool ordered) {
  Life life;
  life.diplomacy = Diplomacy{PER_MILLE, 0, 0, 1};
  World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}},
       ArmySpec{{3, 1}, {Recruit{{3, 1}, 1}}, 1, {Skill{0, 0, 0, 0, 1}}}},
      {},
      Rules{0, {{false, false}, {false, false}}, STAGE_FLOORS},
      {},
      {Leadership{0, std::nullopt, std::nullopt, std::nullopt}},
      life};
  if (ordered) {
    world.steer(0, true);
    world.declare(0, 1, Declaration::WAR);
  }
  return world;
}

TEST(Save, ALoadedWorldKeepsItsSteeredArmyAndItsDeclarations) {
  const World world = diplomatic(true);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));

  EXPECT_TRUE(restored.world.steered(0));
  EXPECT_FALSE(restored.world.steered(1));
  EXPECT_EQ(restored.world.relation(1, 0), Relation::AT_WAR);
  const std::optional<Declared> made = restored.world.declaration(0, 1);
  ASSERT_TRUE(made.has_value());
  EXPECT_EQ(made.value_or(Declared{}).by, 0U);
  // The declaration's hold came through: it may not be undone yet.
  EXPECT_EQ(restored.world.declaration_refusal(0, 1, Declaration::PEACE),
            DeclarationRefusal::TOO_SOON);
  EXPECT_EQ(restored.world.hash(), world.hash());
  // Control: the same world unsteered and at peace is another state.
  EXPECT_NE(diplomatic(false).hash(), world.hash());
}

// Two people of one faction 3 cells apart, who talk within `reach` cells.
World talkers(std::int32_t reach) {
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.talk_reach = reach;
  return World{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}},
       ArmySpec{{3, 1}, {Recruit{{3, 1}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {},
      rules};
}

// talkers(3) with a capacity of `capacity` people.
World with_capacity(std::size_t capacity) {
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.talk_reach = 3;
  rules.people_capacity = capacity;
  return World{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}},
       ArmySpec{{3, 1}, {Recruit{{3, 1}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {},
      rules};
}

TEST(Save, ALoadedWorldKeepsItsPeopleCapacity) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, with_capacity(2)}, store, SLOT));
  EXPECT_EQ(loaded(load(store, SLOT)).world.hash(), with_capacity(2).hash());
  // Control: the capacity is what tells the two apart.
  EXPECT_NE(with_capacity(2).hash(), with_capacity(MAX_PEOPLE).hash());
}

// Two hostile people three cells apart in sight of each other, the first's
// army, if `intent`, holding an attack intent on the second.
World facing(bool intent) {
  constexpr std::int32_t SIGHT = 6;
  Rules rules{0, {{false, true}, {true, false}}, STAGE_FLOORS};
  rules.sight = SIGHT;
  World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}},
       ArmySpec{{3, 0}, {Recruit{{3, 0}, 1}}, 1, {Skill{0, 0, 0, 0, 1}}}},
      {},
      rules};
  if (intent) {
    world.order(0, Order{{0, 0}, {0}, 1});
  }
  return world;
}

TEST(Save, ALoadedWorldKeepsItsAttackIntent) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, facing(true)}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.target(0), std::optional<std::size_t>{1});
  EXPECT_EQ(restored.world.hash(), facing(true).hash());
  // Control: the intent is what tells the two apart.
  EXPECT_NE(facing(true).hash(), facing(false).hash());
}

TEST(Save, ALoadedWorldKeepsItsTalkReach) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, talkers(3)}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.talk_refusal(0, 1), TalkRefusal::NONE);
  EXPECT_EQ(restored.world.hash(), talkers(3).hash());
  // Control: a reach of 2 saved and loaded leaves them out of reach.
  ASSERT_TRUE(save(Saved{std::nullopt, talkers(2)}, store, OTHER_SLOT));
  EXPECT_EQ(loaded(load(store, OTHER_SLOT)).world.talk_refusal(0, 1),
            TalkRefusal::OUT_OF_REACH);
  EXPECT_NE(talkers(2).hash(), talkers(3).hash());
}

// Two people of one faction side by side, whose best discipline sells a
// herb for 10 silver, the first having bought `bought` of them from the
// second.
World traders(std::uint64_t bought) {
  constexpr std::uint64_t PRICE = 10;
  constexpr std::uint64_t PURSE = 50;
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.talk_reach = 2;
  Life life;
  life.disciplines = {{"medicine", {0, 0}, {1, 0}}};
  life.trade = Trade{{Ware{"herb", PRICE, 1, 0, 0}}, PURSE, 1, 0, 0};
  World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}},
       ArmySpec{{1, 0}, {Recruit{{1, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {},
      rules,
      {},
      {},
      life};
  for (std::uint64_t each = 0; each < bought; ++each) {
    world.buy(0, 1, 0);
  }
  return world;
}

TEST(Save, ALoadedWorldKeepsItsSilverWaresAndTrade) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, traders(2)}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.person(0).silver, traders(2).person(0).silver);
  EXPECT_EQ(restored.world.person(0).held, std::vector<std::uint64_t>{2});
  ASSERT_EQ(restored.world.wares().size(), 1U);
  EXPECT_EQ(restored.world.wares().at(0).name, "herb");
  EXPECT_TRUE(restored.world.sells(1, 0));
  EXPECT_EQ(restored.world.hash(), traders(2).hash());
  // Control: one herb fewer bought is another state.
  EXPECT_NE(traders(1).hash(), traders(2).hash());
}

// Two hostile people 4 cells apart, each sweeping an arc of `shape` at arm's
// length, who close on a foe within `sight` cells.
World engaging(Shape shape, std::int32_t sight) {
  constexpr std::uint64_t HEALTH = 10;
  Rules rules{0, {{false, true}, {true, false}}, STAGE_FLOORS};
  rules.sight = sight;
  const Skill strike{3 * CELL_SIZE / 2, 0, 1, 0, 1, 0, shape};
  return World{Seed{1},
               Map{{"......", "......"}},
               {ArmySpec{{0, 0}, {Recruit{{0, 0}, HEALTH}}, 0, {strike}},
                ArmySpec{{4, 0}, {Recruit{{4, 0}, HEALTH}}, 1, {strike}}},
               {},
               rules};
}

constexpr std::int32_t SIGHT = 6;

TEST(Save, ALoadedWorldKeepsItsSkillsShapesAndItsSight) {
  MemorySaves store;
  ASSERT_TRUE(
      save(Saved{std::nullopt, engaging(Shape::ARC, SIGHT)}, store, SLOT));
  Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.skill_of(0, 0).shape, Shape::ARC);
  EXPECT_EQ(restored.world.hash(), engaging(Shape::ARC, SIGHT).hash());
  // It closes on its foe as the saved world does.
  constexpr int TWO_SECONDS = 60;
  World lived = engaging(Shape::ARC, SIGHT);
  for (int tick = 0; tick < TWO_SECONDS; ++tick) {
    lived.tick();
    restored.world.tick();
  }
  EXPECT_GT(cell_of(lived.person(0).position).x, 0);
  EXPECT_EQ(restored.world.hash(), lived.hash());
  // Control: another shape, or no sight, is another state.
  EXPECT_NE(engaging(Shape::LINE, SIGHT).hash(),
            engaging(Shape::ARC, SIGHT).hash());
  EXPECT_NE(engaging(Shape::ARC, 0).hash(), engaging(Shape::ARC, SIGHT).hash());
}

TEST(Save, ASealedSaveCastingAtAPersonTheWorldLacksIsRefused) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, casting(AtPerson{0})}, store, SLOT));
  ASSERT_TRUE(
      save(Saved{std::nullopt, casting(AtPerson{1})}, store, OTHER_SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(OTHER_SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The one payload byte the two saves differ in is the cast's person.
  const std::vector<std::uint8_t>& at_first = store.bytes(SLOT);
  const auto person = static_cast<std::size_t>(
      std::mismatch(
          payload.begin(), payload.end(),
          at_first.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES))
          .first -
      payload.begin());
  ASSERT_EQ(payload.at(person), 1);

  // Control: resealing the same payload loads with its cast.
  store.bytes(OTHER_SLOT) = resealed(header, payload);
  EXPECT_EQ(loaded(load(store, OTHER_SLOT)).world.hash(),
            casting(AtPerson{1}).hash());
  // The cast now names a person the world never held.
  payload.at(person) = 2;
  store.bytes(OTHER_SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, OTHER_SLOT)), LoadError::CORRUPTED);
}

}  // namespace
}  // namespace murim
