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

#include "areas.hpp"
#include "arts.hpp"
#include "attack.hpp"
#include "calendar.hpp"
#include "chart.hpp"
#include "diplomacy.hpp"
#include "growth.hpp"
#include "json_packs.hpp"
#include "leadership.hpp"
#include "limits.hpp"
#include "m1_scenario.hpp"
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

// Where the parts of the save `bytes` end, as save_parts() finds them by
// decoding it.
SaveParts parts_of(const std::vector<std::uint8_t>& bytes) {
  return std::get<SaveParts>(save_parts(bytes));
}

// The index in `payload` of the last byte it differs in from the payload of
// the save `other`, both read back from their ends.
std::size_t last_difference(const std::vector<std::uint8_t>& payload,
                            const std::vector<std::uint8_t>& other) {
  const auto from_end =
      std::mismatch(
          payload.rbegin(), payload.rend(),
          other.rbegin() + static_cast<std::ptrdiff_t>(CHECKSUM_BYTES))
          .first -
      payload.rbegin();
  return payload.size() - 1 - static_cast<std::size_t>(from_end);
}

// Two hostile people a cell apart after one tick, in which the one not
// `struck` strikes `struck` dead and is struck by nothing that harms.
World struck_dead(std::size_t struck) {
  const Skill deadly{2 * CELL_SIZE, 0, 1, 0, 1'000};
  const Skill harmless{2 * CELL_SIZE, 0, 0, 0, 1'000};
  World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{
           {0, 0}, {Recruit{{0, 0}, 1}}, 0, {struck == 1 ? deadly : harmless}},
       ArmySpec{
           {1, 0}, {Recruit{{1, 0}, 1}}, 1, {struck == 0 ? deadly : harmless}}},
      {},
      Rules{0, {{false, true}, {true, false}}, STAGE_FLOORS}};
  world.tick();
  return world;
}

constexpr std::uint32_t DAILY_QI = 10;

// One person cultivating `DAILY_QI` a day, up to twice that, among `realms`,
// after a day for each realm, days of one tick.
World cultivated(const std::vector<Realm>& realms) {
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
                  Cultivation{{DAILY_QI, 0}, {2 * DAILY_QI, 0}, 0, 1},
                  realms,
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
  for (std::size_t day = 0; day < realms.size(); ++day) {
    world.tick();
  }
  return world;
}

// The person and rank of `world`'s latest step of progress, when it gained a
// realm.
std::optional<std::pair<std::size_t, std::size_t>> latest_gain(
    const World& world) {
  if (world.progress().empty()) {
    return std::nullopt;
  }
  const auto* gained = std::get_if<RealmGained>(&world.progress().back().what);
  if (gained == nullptr) {
    return std::nullopt;
  }
  return std::pair{gained->person, gained->rank};
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

// The header's layout is under test here, so its offsets are named in the
// test.
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

TEST(Save, AVersion13SaveFromBeforeVillagesWereLaidOutByRoleIsRefused) {
  // Version 13 saves hold no village tier or centre, no plan or structure
  // role, no seed and none of growth's apart, terrace and tiers.
  constexpr std::uint32_t BEFORE_ROLES = 13;
  ASSERT_LT(BEFORE_ROLES, SAVE_VERSION);
  MemorySaves store;
  ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(BEFORE_ROLES);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION);
  // Control: the same bytes at the current version load.
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(SAVE_VERSION);
  EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt);
}

TEST(Save, AVersion14SaveFromBeforeAnEngagementCouldHoldNoSlotIsRefused) {
  // Version 14 saves hold every engagement's slot as a bare index, so they
  // cannot hold a token holder offered no slot or a waiter yet to take one.
  constexpr std::uint32_t BEFORE_NO_SLOT = 14;
  ASSERT_LT(BEFORE_NO_SLOT, SAVE_VERSION);
  MemorySaves store;
  ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(BEFORE_NO_SLOT);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION);
  // Control: the same bytes at the current version load.
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(SAVE_VERSION);
  EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt);
}

TEST(Save, AVersion15SaveFromBeforeTiersIsRefused) {
  // Version 15 saves hold no area's tier and no army's far anchor.
  constexpr std::uint32_t BEFORE_TIERS = 15;
  ASSERT_LT(BEFORE_TIERS, SAVE_VERSION);
  MemorySaves store;
  ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(BEFORE_TIERS);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION);
  // Control: the same bytes at the current version load.
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(SAVE_VERSION);
  EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt);
}

// The side of the tier tests' map, 8 by 8 areas, how far from its edges
// their armies stand, a cell's further step inward, and a third army's
// distance in.
constexpr std::int32_t TIERED_SIDE = 8 * AREA_SIDE;
constexpr std::int32_t EDGE = 4;
constexpr std::int32_t INWARD = EDGE + 1;
constexpr std::int32_t DEEPER = 2 * EDGE;

// A map of 8 by 8 areas with an army at either corner, the first steered on
// the standard profile, so the second, 7 areas away, is far.
World tiered() {
  constexpr std::int32_t SIDE = TIERED_SIDE;
  const std::vector<std::string> rows(static_cast<std::size_t>(SIDE),
                                      std::string(SIDE, '.'));
  const Skill guard{1, 0, 1, 0, 1};
  World world{Seed{1},
              Map{rows},
              {ArmySpec{{EDGE, EDGE}, {Recruit{{EDGE, EDGE}, 1}}, 0, {guard}},
               ArmySpec{{SIDE - EDGE, SIDE - EDGE},
                        {Recruit{{SIDE - EDGE, SIDE - EDGE}, 1},
                         Recruit{{SIDE - INWARD, SIDE - EDGE}, 1}},
                        0,
                        {guard}}},
              {},
              Rules{0, {{false}}, STAGE_FLOORS}};
  world.set_profile(STANDARD_PROFILE);
  world.steer(0, true);
  world.retier();
  return world;
}

// A world of one army, steered at a corner of a map of 8 by 8 areas, on
// `profile`, its tiers just recomputed.
World alone(std::optional<Profile> profile) {
  const std::vector<std::string> rows(static_cast<std::size_t>(TIERED_SIDE),
                                      std::string(TIERED_SIDE, '.'));
  World world{
      Seed{1},
      Map{rows},
      {ArmySpec{
          {EDGE, EDGE}, {Recruit{{EDGE, EDGE}, 1}}, 0, {Skill{1, 0, 1, 0, 1}}}},
      {},
      Rules{0, {{false}}, STAGE_FLOORS}};
  world.set_profile(profile);
  world.steer(0, true);
  world.retier();
  return world;
}

TEST(Save, TheAreasTiersAreHashed) {
  // The same army on the same cell: only the areas' tiers differ.
  const World tiered_alone = alone(STANDARD_PROFILE);
  const World untiered = alone(std::nullopt);
  ASSERT_EQ(tiered_alone.areas().tier(tiered_alone.areas().count() - 1),
            AreaTier::FAR);
  ASSERT_EQ(untiered.areas().tier(untiered.areas().count() - 1),
            AreaTier::LIVE);
  EXPECT_NE(tiered_alone.hash(), untiered.hash());
  // Control: the same tiers hash the same.
  EXPECT_EQ(alone(STANDARD_PROFILE).hash(), tiered_alone.hash());
}

// A world whose second army, far at the map's far corner, loses its first
// member to a third far army after it went far, when `left_before` is false,
// so its anchor is that member's cell; or before, so its anchor is its second
// member's. Either way it ends with the same member on the same cell, under
// the same tiers.
World left_far_army(bool left_before) {
  constexpr std::int32_t SIDE = TIERED_SIDE;
  const std::vector<std::string> rows(static_cast<std::size_t>(SIDE),
                                      std::string(SIDE, '.'));
  const Skill guard{1, 0, 1, 0, 1};
  World world{Seed{1},
              Map{rows},
              {ArmySpec{{EDGE, EDGE}, {Recruit{{EDGE, EDGE}, 1}}, 0, {guard}},
               ArmySpec{{SIDE - EDGE, SIDE - EDGE},
                        {Recruit{{SIDE - EDGE, SIDE - EDGE}, 1},
                         Recruit{{SIDE - INWARD, SIDE - EDGE}, 1}},
                        0,
                        {guard}},
               ArmySpec{{SIDE - DEEPER, SIDE - DEEPER},
                        {Recruit{{SIDE - DEEPER, SIDE - DEEPER}, 1}},
                        0,
                        {guard}}},
              {},
              Rules{0, {{false}}, STAGE_FLOORS}};
  world.set_profile(STANDARD_PROFILE);
  world.steer(0, true);
  const std::size_t first = world.roster(1).front();
  if (left_before) {
    world.conscript(first, 2);
  }
  world.retier();
  if (!left_before) {
    world.conscript(first, 2);
  }
  return world;
}

// The cell army `army` of `world` is anchored on while far, or none.
std::optional<Cell> anchor_of(const World& world, std::size_t army) {
  const std::optional<FarAnchor> far = world.far(army);
  return far ? std::optional<Cell>{far.value_or(FarAnchor{}).cell}
             : std::nullopt;
}

TEST(Save, AFarArmysAnchorIsHashed) {
  const World after = left_far_army(false);
  const World before = left_far_army(true);
  const std::optional<Cell> after_anchor = anchor_of(after, 1);
  const std::optional<Cell> before_anchor = anchor_of(before, 1);
  ASSERT_TRUE(after_anchor.has_value());
  ASSERT_TRUE(before_anchor.has_value());
  ASSERT_NE(after_anchor.value_or(Cell{}).x, before_anchor.value_or(Cell{}).x);
  ASSERT_EQ(after.roster(1), before.roster(1));
  ASSERT_EQ(after.areas().tiers(), before.areas().tiers());
  EXPECT_NE(after.hash(), before.hash());
  // Control: made the same way, the same hash.
  EXPECT_EQ(left_far_army(false).hash(), after.hash());
}

TEST(Save, ASaveKeepsTheAreasTiersAndAFarArmysAnchor) {
  const World world = tiered();
  ASSERT_TRUE(world.far(1).has_value());
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.hash(), world.hash());
  const std::optional<FarAnchor> held = world.far(1);
  const std::optional<FarAnchor> kept = restored.world.far(1);
  ASSERT_TRUE(kept.has_value());
  EXPECT_EQ(kept.value_or(FarAnchor{}).area, held.value_or(FarAnchor{}).area);
  EXPECT_EQ(anchor_of(restored.world, 1).value_or(Cell{}).x,
            anchor_of(world, 1).value_or(Cell{}).x);
  EXPECT_EQ(anchor_of(restored.world, 1).value_or(Cell{}).y,
            anchor_of(world, 1).value_or(Cell{}).y);
  EXPECT_EQ(restored.world.areas().tiers(), world.areas().tiers());
  // Given its profile again, the loaded world ticks on as the saved one.
  World saved = world;
  restored.world.set_profile(STANDARD_PROFILE);
  for (int tick = 0; tick < 2 * TICKS_PER_SECOND; ++tick) {
    saved.tick();
    restored.world.tick();
  }
  EXPECT_EQ(restored.world.hash(), saved.hash());
  // Control: a world whose second army was never far hashes otherwise.
  World near = tiered();
  near.steer(0, false);
  near.retier();
  ASSERT_FALSE(near.far(1).has_value());
  EXPECT_NE(near.hash(), world.hash());
}

TEST(Save, ASealedSaveNamingARecordTheWorldLacksIsRefused) {
  // One structure no village holds and no plan laid: the plans part ends
  // with its plan, absent, as one flag byte, and the engaged and charts parts
  // that follow hold their counts, none. Only growth lays a planned structure,
  // so no legal world differs from this one in that field alone, and the test
  // reads it by its place in the encoding.
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
  const SaveParts parts = parts_of(intact);
  const std::size_t plan = parts.plans_end - 1;
  ASSERT_EQ(parts.engaged_end, parts.plans_end + sizeof(std::uint64_t));
  ASSERT_EQ(parts.charts_end, parts.engaged_end + sizeof(std::uint64_t));
  ASSERT_EQ(payload.at(plan), 0);
  ASSERT_TRUE(std::all_of(
      payload.begin() + static_cast<std::ptrdiff_t>(plan),
      payload.begin() + static_cast<std::ptrdiff_t>(parts.charts_end),
      [](std::uint8_t byte) { return byte == 0; }));

  // Control: resealing the same payload loads, so the reseal refuses nothing.
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(loaded(load(store, SLOT)).world.hash(), world.hash());

  // The structure now names plan 0 of a world without growth, so no plans.
  payload.at(plan) = 1;
  payload.insert(payload.begin() + static_cast<std::ptrdiff_t>(plan) + 1,
                 sizeof(std::uint64_t), 0);
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

// The generated region of `seed` with its player steered, so its soul
// charts what it sees, lived `ticks` ticks at days of five ticks.
Saved charted_region(Seed seed, std::uint64_t ticks) {
  Region region = generate_region(seed, base_data());
  region.rules.ticks_per_day = TICKS_PER_SHORT_DAY;
  region.armies.push_back(born_in(region, 0));
  World world{region.seed,       Map{region.map_rows}, region.armies,
              region.structures, region.rules,         village_specs(region),
              region.leadership, region.life,          region.sects};
  world.ensoul(world.population() - 1);
  world.steer(region.armies.size() - 1, true);
  for (std::uint64_t tick = 0; tick < ticks; ++tick) {
    world.tick();
  }
  return Saved{Origin{seed, 0}, std::move(world)};
}

TEST(Save, AChartComesBackAsSavedAndAStageNoChartHoldsIsRefused) {
  constexpr std::uint8_t STAGES = 3;
  const Saved saved = charted_region(REGION_SEED, TICKS_PER_SHORT_DAY);
  const Chart& chart = saved.world.chart(0);
  // Setup: the player saw its home village, the last village it remembers.
  ASSERT_FALSE(chart.blocks().empty());
  ASSERT_FALSE(chart.villages().empty());
  MemorySaves store;
  ASSERT_TRUE(save(saved, store, SLOT));

  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.hash(), saved.world.hash());
  const std::vector<ChartBlock> blocks = restored.world.chart(0).blocks();
  ASSERT_EQ(blocks.size(), chart.blocks().size());
  for (std::size_t block = 0; block < blocks.size(); ++block) {
    EXPECT_EQ(blocks.at(block).area, chart.blocks().at(block).area);
    EXPECT_EQ(blocks.at(block).bits, chart.blocks().at(block).bits);
  }
  EXPECT_EQ(restored.world.chart(0).villages(), chart.villages());

  // The charts part ends with the last village remembered, its stage last.
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  const std::size_t stage = parts_of(intact).charts_end - 1;
  ASSERT_EQ(payload.at(stage),
            static_cast<std::uint8_t>(
                chart.villages()
                    .back()
                    .value_or(VillageMemory{0, false, VillageStage::RUINS})
                    .stage));
  // Control: the last stage there is loads, so the stage byte is read.
  payload.at(stage) = STAGES - 1;
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt);
  payload.at(stage) = STAGES;
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

TEST(Save, ARememberedClaimCentreIsSavedHashedAndRefusedOffTheMap) {
  const Saved saved = charted_region(REGION_SEED, TICKS_PER_SHORT_DAY);
  const auto& memories = saved.world.chart(0).villages();
  ASSERT_FALSE(memories.empty());
  ASSERT_TRUE(memories.back().has_value());
  const Cell first = memories.back().value_or(VillageMemory{}).centre;
  MemorySaves store;
  ASSERT_TRUE(save(saved, store, SLOT));
  const auto intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The charts part ends with the last memory, which ends with its two centre
  // coordinates and its stage.
  const std::size_t x_offset =
      parts_of(intact).charts_end - 1 - (2 * sizeof(std::int32_t));
  ASSERT_EQ(payload.at(x_offset), static_cast<std::uint8_t>(first.x));
  const auto put_x = [&payload, x_offset](std::uint32_t value) {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
      payload.at(x_offset + byte) =
          static_cast<std::uint8_t>(value >> (byte * BYTE_BITS));
    }
  };
  const std::uint32_t changed = first.x == 0 ? 1U : 0U;
  put_x(changed);
  store.bytes(SLOT) = resealed(header, payload);
  const Saved restored = loaded(load(store, SLOT));
  const Cell centre = restored.world.chart(0)
                          .villages()
                          .back()
                          .value_or(VillageMemory{})
                          .centre;
  EXPECT_EQ(centre.x, static_cast<std::int32_t>(changed));
  EXPECT_EQ(centre.y, first.y);
  EXPECT_NE(restored.world.hash(), saved.world.hash());
  // Control above: another centre on the map loads; its first off-map x does
  // not.
  put_x(static_cast<std::uint32_t>(REGION_SIDE));
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

TEST(Save, ASealedSaveNamingAPersonNoEventCanNameIsRefused) {
  // One person strikes the other dead on the first tick: person 1 in one
  // world and person 0 in the other, which differ in nothing the save holds
  // after the death.
  const World world = struck_dead(1);
  ASSERT_EQ(world.events().size(), 1U);
  ASSERT_EQ(struck_dead(0).events().size(), 1U);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, struck_dead(0)}, store, SLOT));
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, OTHER_SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(OTHER_SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The last payload byte the two saves differ in is the death's person.
  const std::size_t person = last_difference(payload, store.bytes(SLOT));
  ASSERT_EQ(payload.at(person), 1);
  // Control: resealing the same payload loads with its event.
  store.bytes(OTHER_SLOT) = resealed(header, payload);
  const Saved restored = loaded(load(store, OTHER_SLOT));
  EXPECT_EQ(restored.world.hash(), world.hash());
  EXPECT_EQ(restored.world.events().size(), 1U);
  // The death now names a person the world never held.
  payload.at(person) = 2;
  store.bytes(OTHER_SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, OTHER_SLOT)), LoadError::CORRUPTED);
}

TEST(Save, ASealedSaveNamingARealmNoProgressCanNameIsRefused) {
  // A person reaches its world's one realm, at 10, on the first day; another
  // the second of two, at 20, on the second. The two differ in nothing the
  // save holds after the latest step.
  const Realm third{{"삼류", "Third-rate"}, DAILY_QI, PER_MILLE};
  const Realm second{{"이류", "Second-rate"}, 2 * DAILY_QI, PER_MILLE};
  const std::vector<Realm> one{third};
  const std::vector<Realm> two{third, second};
  const World world = cultivated(one);
  const World further = cultivated(two);
  using Gain = std::pair<std::size_t, std::size_t>;
  ASSERT_EQ(world.progress().size(), 1U);
  ASSERT_EQ(latest_gain(world), Gain(0, 1));
  ASSERT_EQ(latest_gain(further), Gain(0, 2));
  ASSERT_TRUE(world.events().empty() && further.events().empty());
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, further}, store, SLOT));
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, OTHER_SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(OTHER_SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The last payload byte the two saves differ in is the latest step's rank.
  const std::size_t rank = last_difference(payload, store.bytes(SLOT));
  ASSERT_EQ(payload.at(rank), 1);
  // Control: resealing the same payload loads with its step.
  store.bytes(OTHER_SLOT) = resealed(header, payload);
  const Saved restored = loaded(load(store, OTHER_SLOT));
  EXPECT_EQ(restored.world.hash(), world.hash());
  EXPECT_EQ(restored.world.progress().size(), 1U);
  EXPECT_EQ(latest_gain(restored.world), Gain(0, 1));
  // The step now names a realm past the one the world holds.
  payload.at(rank) = 2;
  store.bytes(OTHER_SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, OTHER_SLOT)), LoadError::CORRUPTED);
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

// A village of `people` with no food, whose growth's second tier is reached
// at four residents, lived STARVING_DAYS days of a tick each, unfed people
// dying after a day.
constexpr int STARVING_DAYS = 5;

World starving(std::size_t people) {
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.ticks_per_day = 1;
  Life life;
  life.husbandry = Husbandry{1, 1, 0};
  Growth growth;
  growth.tiers = {Tier{0, {1, 1}, 1}, Tier{4, {3, 3}, 1}};
  growth.stances = {{}};
  life.growth = growth;
  std::vector<Recruit> residents;
  residents.reserve(people);
  for (std::size_t person = 0; person < people; ++person) {
    residents.push_back(Recruit{{static_cast<std::int32_t>(person), 0}, 1});
  }
  World world{Seed{1},
              Map{{"......", "......", "......"}},
              {ArmySpec{{2, 2}, residents, 0, {Skill{0, 0, 0, 0, 1}}}},
              {},
              rules,
              {VillageSpec{{}, 0, {}, 0, {}}},
              {},
              life};
  return ticked(std::move(world), STARVING_DAYS);
}

TEST(Save, AVillageThatShrankKeepsTheTierItReachedAfterLoading) {
  const World shrunk = starving(4);
  ASSERT_EQ(shrunk.residents_of(0), 0U);
  ASSERT_EQ(shrunk.tier(0), 1U);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, shrunk}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.tier(0), 1U);
  EXPECT_EQ(restored.world.hash(), shrunk.hash());
  // Control: three people never reach the tier, so the same village starved
  // keeps the first: the tier kept is the one its peak reached.
  const World never = starving(3);
  ASSERT_EQ(never.residents_of(0), 0U);
  EXPECT_EQ(never.tier(0), 0U);
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

// Two hostile people a cell apart, with attack tokens by `tokens` or none.
World tokened(std::optional<Tokens> tokens) {
  Rules rules{0, {{false, true}, {true, false}}, STAGE_FLOORS};
  rules.tokens = std::move(tokens);
  return World{Seed{1},
               Map{{"....", "...."}},
               {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{}}},
                ArmySpec{{1, 0}, {Recruit{{1, 0}, 1}}, 1, {Skill{}}}},
               {},
               rules};
}

TEST(Save, ALoadedWorldKeepsItsAttackTokens) {
  const Tokens region{{2, 2, 3, 3, 4, 4, 6}, 30, 90};
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, tokened(region)}, store, SLOT));
  EXPECT_EQ(loaded(load(store, SLOT)).world.hash(), tokened(region).hash());
  // Control: tokens are folded into the hash, so their loss would show, and
  // a world without them hashes as before they existed.
  EXPECT_NE(tokened(region).hash(), tokened(std::nullopt).hash());
  Tokens leased = region;
  leased.lease = 0;
  EXPECT_NE(tokened(region).hash(), tokened(leased).hash());
}

// A save of `bytes`' world holding, in place of its empty list of engaged
// people, the one engaged person `entry` encodes. The list is the engaged
// part, which runs from the end of the plans to its own end, found by
// decoding the save; empty, it is its count alone.
std::vector<std::uint8_t> engaging(const std::vector<std::uint8_t>& bytes,
                                   const std::vector<std::uint8_t>& entry) {
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(bytes.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      bytes.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      bytes.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  const SaveParts parts = parts_of(bytes);
  EXPECT_EQ(parts.engaged_end - parts.plans_end, sizeof(std::uint64_t));
  std::vector<std::uint8_t> list(sizeof(std::uint64_t), 0);
  list.front() = 1;
  list.insert(list.end(), entry.begin(), entry.end());
  payload.erase(
      payload.begin() + static_cast<std::ptrdiff_t>(parts.plans_end),
      payload.begin() + static_cast<std::ptrdiff_t>(parts.engaged_end));
  payload.insert(payload.begin() + static_cast<std::ptrdiff_t>(parts.plans_end),
                 list.begin(), list.end());
  return resealed(header, payload);
}

// An engaged person as a save encodes it, its ring as a raw tag.
struct Entry {
  std::uint64_t person = 0;
  std::uint64_t foe = 0;
  std::uint8_t ring = 0;
};

// `entry`'s bytes: person, foe, ring, slot 0, a token, and 0 for the ticks it
// began to wait and took the token.
std::vector<std::uint8_t> encoded(const Entry& entry) {
  std::vector<std::uint8_t> bytes;
  for (const std::uint64_t value : {entry.person, entry.foe}) {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
      bytes.push_back(static_cast<std::uint8_t>(value >> (byte * BYTE_BITS)));
    }
  }
  bytes.insert(bytes.end(), {entry.ring, 0, 1});
  bytes.resize(bytes.size() + (2 * sizeof(std::uint32_t)), 0);
  return bytes;
}

// The engaged people's encoding is under test here: no world engages anyone
// yet, so the test writes an entry in place of the save's empty list.
TEST(Save, AnEngagedPersonLoadsAndSavesAgainAsItWasWritten) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, tokened(std::nullopt)}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  constexpr std::uint8_t NEAR_RING = 0;
  store.bytes(SLOT) = engaging(intact, encoded({0, 1, NEAR_RING}));
  const std::vector<std::uint8_t> written = store.bytes(SLOT);
  const Saved restored = loaded(load(store, SLOT));
  // The engagement is held: it is folded into the hash, and saved again as
  // it was written.
  EXPECT_NE(restored.world.hash(), tokened(std::nullopt).hash());
  ASSERT_TRUE(save(restored, store, OTHER_SLOT));
  EXPECT_EQ(store.bytes(OTHER_SLOT), written);

  // A foe the world never held, and a ring past the three, are refused.
  constexpr std::uint8_t NO_RING = RING_KINDS;
  store.bytes(SLOT) = engaging(intact, encoded({0, 2, NEAR_RING}));
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
  store.bytes(SLOT) = engaging(intact, encoded({0, 1, NO_RING}));
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

}  // namespace
}  // namespace murim
