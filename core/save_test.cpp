#include "save.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
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
#include "gear.hpp"
#include "growth.hpp"
#include "json_packs.hpp"
#include "leadership.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "region.hpp"
#include "schedule.hpp"
#include "skill.hpp"
#include "talent.hpp"
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
                  std::nullopt,
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

TEST(Save, AVersion21SaveFromBeforeGearIsRefused) {
  // Version 21 saves hold no gear and no person's worn slots (ADR 0050).
  constexpr std::uint32_t BEFORE_GEAR = 21;
  ASSERT_LT(BEFORE_GEAR, SAVE_VERSION);
  MemorySaves store;
  ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(BEFORE_GEAR);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION);
  // Control: the same bytes at the current version load.
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(SAVE_VERSION);
  EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt);
}

// How many of `world`'s people wear anything.
std::size_t dressed(const World& world) {
  std::size_t count = 0;
  for (std::size_t person = 0; person < world.population(); ++person) {
    const Worn& worn = world.person(person).worn;
    count += std::any_of(worn.begin(), worn.end(),
                         [](const std::optional<std::size_t>& kind) {
                           return kind.has_value();
                         })
                 ? 1U
                 : 0U;
  }
  return count;
}

TEST(Save, ALoadedWorldKeepsWhatEachPersonWears) {
  const Saved lived = lived_region(REGION_SEED);
  // Setup: some of its people wear something.
  ASSERT_GT(dressed(lived.world), 0U);
  MemorySaves store;
  ASSERT_TRUE(save(lived, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  ASSERT_EQ(restored.world.population(), lived.world.population());
  for (std::size_t person = 0; person < lived.world.population(); ++person) {
    EXPECT_EQ(restored.world.person(person).worn,
              lived.world.person(person).worn)
        << person;
  }
  EXPECT_EQ(restored.world.hash(), lived.world.hash());
}

TEST(Save, AVersion17SaveFromBeforeTiersNamedSquareKindsIsRefused) {
  // Version 17 saves hold no growth tier's square kinds.
  constexpr std::uint32_t BEFORE_SQUARE_KINDS = 17;
  ASSERT_LT(BEFORE_SQUARE_KINDS, SAVE_VERSION);
  MemorySaves store;
  ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
  store.bytes(SLOT).at(VERSION_AT) =
      static_cast<std::uint8_t>(BEFORE_SQUARE_KINDS);
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

TEST(Save, AVersion16SaveFromBeforeOccupationsIsRefused) {
  // Version 16 saves hold no structure's kind, no person's occupation and no
  // occupations (ADR 0081).
  constexpr std::uint32_t BEFORE_OCCUPATIONS = 16;
  ASSERT_LT(BEFORE_OCCUPATIONS, SAVE_VERSION);
  MemorySaves store;
  ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
  store.bytes(SLOT).at(VERSION_AT) =
      static_cast<std::uint8_t>(BEFORE_OCCUPATIONS);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION);
  // Control: the same bytes at the current version load.
  store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(SAVE_VERSION);
  EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt);
}

TEST(Save, AVersion17Or18SaveFromBeforeWakingHoursIsRefused) {
  // Version 17 saves, and 18's, hold no waking hours among the occupations
  // (ADR 0081).
  for (const std::uint32_t before_waking : {17U, 18U}) {
    ASSERT_LT(before_waking, SAVE_VERSION);
    MemorySaves store;
    ASSERT_TRUE(save(lived_region(REGION_SEED), store, SLOT));
    store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(before_waking);
    EXPECT_EQ(refusal(load(store, SLOT)), LoadError::UNSUPPORTED_VERSION)
        << before_waking;
    // Control: the same bytes at the current version load.
    store.bytes(SLOT).at(VERSION_AT) = static_cast<std::uint8_t>(SAVE_VERSION);
    EXPECT_EQ(refusal(load(store, SLOT)), std::nullopt) << before_waking;
  }
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

constexpr std::size_t FIRST_ARMY_PEOPLE = 5;
// The one person of the second army, after the first army's.
constexpr std::size_t LONE = FIRST_ARMY_PEOPLE;
// The column the lone person stands in.
constexpr std::int32_t LONE_AT = 5;

// An army of persons 0 to 4 of faction 0 and an army of person 5 alone, of
// faction 1.
World two_rosters() {
  std::vector<Recruit> first;
  first.reserve(FIRST_ARMY_PEOPLE);
  for (std::int32_t column = 0;
       column < static_cast<std::int32_t>(FIRST_ARMY_PEOPLE); ++column) {
    first.push_back(Recruit{{column, 0}, 1});
  }
  return World{
      Seed{1},
      Map{{"......", "......"}},
      {ArmySpec{{0, 0}, std::move(first), 0, {Skill{}}},
       ArmySpec{{LONE_AT, 1}, {Recruit{{LONE_AT, 1}, 1}}, 1, {Skill{}}}},
      {},
      Rules{0, {{false, false}, {false, false}}, STAGE_FLOORS}};
}

TEST(Save, ASaveWithAPersonOnTwoRostersIsRefused) {
  const World world = two_rosters();
  ASSERT_EQ(world.roster(1), std::vector<std::size_t>{LONE});
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // The second army's roster as saved: a count of one, then person 5. Setup:
  // it is found once, so the edit below changes that roster and nothing else.
  std::vector<std::uint8_t> lone(2 * sizeof(std::uint64_t), 0);
  lone.at(0) = 1;
  lone.at(sizeof(std::uint64_t)) = static_cast<std::uint8_t>(LONE);
  std::vector<std::size_t> found;
  for (auto at = payload.begin();
       (at = std::search(at, payload.end(), lone.begin(), lone.end())) !=
       payload.end();
       ++at) {
    found.push_back(static_cast<std::size_t>(at - payload.begin()));
  }
  ASSERT_EQ(found.size(), 1U);

  // Control: resealed unchanged, it loads, and each person serves in the
  // army whose roster holds it.
  store.bytes(SLOT) = resealed(header, payload);
  const Saved kept = loaded(load(store, SLOT));
  for (std::size_t person = 0; person < LONE; ++person) {
    EXPECT_EQ(kept.world.faction_of(person), std::optional<std::size_t>{0});
  }
  EXPECT_EQ(kept.world.faction_of(LONE), std::optional<std::size_t>{1});

  // The second army now names person 4, who stands on the first army's
  // roster too, and person 5 stands on none.
  payload.at(found.front() + sizeof(std::uint64_t)) =
      static_cast<std::uint8_t>(LONE - 1);
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
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
  growth.tiers = {Tier{0, {1, 1}, 1, {}}, Tier{4, {3, 3}, 1, {}}};
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
              {VillageSpec{{}, 0, {}, 0, {}, {}}},
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

// A village of four on an open map with no structure, whose growth's one
// tier names a temple of the square role as its square kind when `named`.
World temple_world(bool named) {
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.ticks_per_day = 1;
  Life life;
  Growth growth;
  growth.plans = {Plan{"temple", {1, 1}, true, {}, Role::SQUARE}};
  growth.tiers = {
      Tier{0,
           {1, 1},
           4,
           named ? std::vector<std::size_t>{0} : std::vector<std::size_t>{}}};
  growth.stances = {{}};
  life.growth = growth;
  constexpr std::int32_t PEOPLE = 4;
  std::vector<Recruit> residents;
  residents.reserve(PEOPLE);
  for (std::int32_t person = 0; person < PEOPLE; ++person) {
    residents.push_back(Recruit{{person, 0}, 1});
  }
  const std::vector<std::string> rows(8, std::string(8, '.'));
  return World{Seed{1},
               Map{rows},
               {ArmySpec{{4, 4}, residents, 0, {Skill{0, 0, 0, 0, 1}}}},
               {},
               rules,
               {VillageSpec{{}, 0, {}, 0, {}, {}}},
               {},
               life};
}

TEST(Save, ALoadedWorldKeepsItsTiersSquareKinds) {
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, temple_world(true)}, store, SLOT));
  Saved restored = loaded(load(store, SLOT));
  // The hash folds no growth data.
  EXPECT_EQ(restored.world.hash(), temple_world(false).hash());
  restored.world.tick();
  ASSERT_EQ(restored.world.structure_count(), 1U);
  EXPECT_EQ(restored.world.laid_kind(0), std::optional<std::string>{"temple"});
  // Control: a tier naming none lays nothing.
  World plain = temple_world(false);
  plain.tick();
  EXPECT_EQ(plain.structure_count(), 0U);
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

// A day spent at home, the schedule of every occupation below.
std::vector<Band> at_home() { return {{0, Where::HOME}}; }

// Two people of one faction side by side, the second at home in a village
// where it is the herbalist, who sells a herb for 10 silver; the first has
// bought `bought` of them from the second.
World traders(std::uint64_t bought) {
  constexpr std::uint64_t PRICE = 10;
  constexpr std::uint64_t PURSE = 50;
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.talk_reach = 2;
  Life life;
  life.disciplines = {{"medicine", {0, 0}, {1, 0}}};
  life.trade = Trade{{Ware{"herb", PRICE, 1, 0}}, PURSE, 1, 0, 0};
  life.occupations =
      Occupations{{Occupation{"herbalist", OnSquare{1}, at_home(), {0}}},
                  0,
                  0,
                  at_home(),
                  Waking{}};
  World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}},
       ArmySpec{{1, 0}, {Recruit{{1, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {},
      rules,
      {VillageSpec{{}, 1, {}, 0, {}, {}}},
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

// The position of `text` as the save encodes it, its length then its
// bytes, in `payload`, which holds it once.
std::size_t encoded_at(const std::vector<std::uint8_t>& payload,
                       const std::string& text) {
  std::vector<std::uint8_t> encoded(sizeof(std::uint64_t), 0);
  encoded.at(0) = static_cast<std::uint8_t>(text.size());
  encoded.insert(encoded.end(), text.begin(), text.end());
  const auto found = std::search(payload.begin(), payload.end(),
                                 encoded.begin(), encoded.end());
  EXPECT_NE(found, payload.end()) << text;
  EXPECT_EQ(std::search(std::next(found), payload.end(), encoded.begin(),
                        encoded.end()),
            payload.end())
      << text;
  return static_cast<std::size_t>(found - payload.begin());
}

TEST(Save, AVillageNamingMoreKindsThanStructuresIsRefused) {
  // One village of one structure, a workshop, and its residents.
  const World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {Structure{{3, 1}, 100, {1, 1}, true}},
      Rules{0, {{false}}, STAGE_FLOORS},
      {VillageSpec{{0}, 0, {}, 0, {}, {"workshop"}}}};
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // Control: resealing the same payload loads, so the reseal refuses nothing.
  store.bytes(SLOT) = resealed(header, payload);
  ASSERT_EQ(loaded(load(store, SLOT)).world.hash(), world.hash());

  // The village's kinds, a count then each, gain a second workshop, so they
  // are two for its one structure.
  const std::size_t kind = encoded_at(payload, "workshop");
  const std::size_t count = kind - sizeof(std::uint64_t);
  ASSERT_EQ(payload.at(count), 1);
  payload.at(count) = 2;
  const std::size_t kind_bytes =
      sizeof(std::uint64_t) + std::string{"workshop"}.size();
  const std::vector<std::uint8_t> again(
      payload.begin() + static_cast<std::ptrdiff_t>(kind),
      payload.begin() + static_cast<std::ptrdiff_t>(kind + kind_bytes));
  payload.insert(
      payload.begin() + static_cast<std::ptrdiff_t>(kind + kind_bytes),
      again.begin(), again.end());
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

TEST(Save, AnOccupationComesBackAsSavedAndOneTheDataLacksIsRefused) {
  // One person at home in a village where `first` opens no place, so it
  // takes the fallback, `spare`, the second and last occupation.
  Life life;
  life.occupations =
      Occupations{{Occupation{"first", Nowhere{}, at_home(), {}},
                   Occupation{"spare", Nowhere{}, at_home(), {}}},
                  0,
                  1,
                  at_home(),
                  Waking{}};
  const World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {},
      Rules{0, {{false}}, STAGE_FLOORS},
      {VillageSpec{{}, 0, {}, 0, {}, {}}},
      {},
      life};
  ASSERT_EQ(world.occupation_of(0), std::optional<std::size_t>{1});
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const Saved restored = loaded(load(store, SLOT));
  EXPECT_EQ(restored.world.occupation_of(0), std::optional<std::size_t>{1});
  EXPECT_EQ(restored.world.hash(), world.hash());

  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // Control: resealing the same payload loads, so the reseal refuses nothing.
  store.bytes(SLOT) = resealed(header, payload);
  ASSERT_EQ(loaded(load(store, SLOT)).world.hash(), world.hash());

  // The person's record ends with its silver, none, its wares, none, and its
  // occupation, the second. Holding the first instead, the world loads and
  // hashes otherwise, so the hash folds the occupation itself.
  std::vector<std::uint8_t> held(2 * sizeof(std::uint64_t), 0);
  held.push_back(1);
  held.push_back(1);
  held.insert(held.end(), sizeof(std::uint64_t) - 1, 0);
  const auto record =
      std::search(payload.begin(), payload.end(), held.begin(), held.end());
  ASSERT_NE(record, payload.end());
  ASSERT_EQ(
      std::search(std::next(record), payload.end(), held.begin(), held.end()),
      payload.end());
  std::vector<std::uint8_t> other = payload;
  other.at(static_cast<std::size_t>(record - payload.begin()) +
           (2 * sizeof(std::uint64_t)) + 1) = 0;
  store.bytes(SLOT) = resealed(header, other);
  const Saved first = loaded(load(store, SLOT));
  EXPECT_EQ(first.world.occupation_of(0), std::optional<std::size_t>{0});
  EXPECT_NE(first.world.hash(), world.hash());

  // Drop `spare` from the data: its name, its openings' one tag byte for
  // nowhere, one band and no wares; then its count of occupations falls to
  // one and the fallback, after the working age, becomes `first`. The person
  // still holds the second occupation, which the data no longer lists.
  constexpr std::size_t BAND_BYTES = sizeof(std::uint32_t) + 1;
  const std::size_t spare = encoded_at(payload, "spare");
  const std::size_t spare_bytes =
      sizeof(std::uint64_t) + std::string{"spare"}.size() + 1 +
      sizeof(std::uint64_t) + BAND_BYTES + sizeof(std::uint64_t);
  const std::size_t count =
      encoded_at(payload, "first") - sizeof(std::uint64_t);
  ASSERT_EQ(payload.at(count), 2);
  const std::size_t fallback = spare + spare_bytes + sizeof(std::uint32_t);
  ASSERT_EQ(payload.at(fallback), 1);
  payload.at(fallback) = 0;
  payload.at(count) = 1;
  payload.erase(
      payload.begin() + static_cast<std::ptrdiff_t>(spare),
      payload.begin() + static_cast<std::ptrdiff_t>(spare + spare_bytes));
  store.bytes(SLOT) = resealed(header, payload);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
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

// The errand tests' map, 30 cells wide, the centre their residents start
// round and are bound for, how durable their structures are, and their
// houses' footprint.
constexpr std::size_t ERRAND_WIDTH = 30;
constexpr std::uint32_t BUILT = 100;
constexpr Footprint HOUSE_FOOTPRINT{2, 2};

std::vector<std::string> errand_rows(std::size_t height) {
  return {height, std::string(ERRAND_WIDTH, '.')};
}

// Whether `people` of `one` and `other` stand where each other does.
bool stand_alike(const World& one, const World& other,
                 const std::vector<std::size_t>& people) {
  return std::all_of(people.begin(), people.end(), [&](std::size_t person) {
    const Position here = one.person(person).position;
    const Position there = other.person(person).position;
    return here.x == there.x && here.y == there.y;
  });
}

// Runs each of `worlds` for `ticks` ticks.
void run_all(const std::vector<World*>& worlds, std::uint64_t ticks) {
  for (std::uint64_t tick = 0; tick < ticks; ++tick) {
    for (World* world : worlds) {
      world->tick();
    }
  }
}

// Two smiths at the centre of an open 30 by 10 map, with a house at its west
// and a smithy at its east; smiths are home until hour 2, then at work. A
// day is 240 ticks, so an hour is 10.
constexpr std::uint64_t ERRAND_DAY = 240;
constexpr std::size_t ERRAND_HEIGHT = 10;
constexpr Cell ERRAND_CENTRE{15, 5};
constexpr Cell BESIDE_CENTRE{15, 6};
constexpr Cell WEST_HOUSE{4, 2};
constexpr Cell EAST_SMITHY{24, 2};

World errand_world() {
  Life life;
  life.founding_age = Spread{2 * DAYS_PER_YEAR, 0};
  life.occupations =
      Occupations{{Occupation{"smith",
                              AtWorkplace{"smithy", 2},
                              {{0, Where::HOME}, {2, Where::WORK}},
                              {}}},
                  0,
                  0,
                  at_home(),
                  Waking{}};
  Rules rules{0, {{false}}, STAGE_FLOORS};
  rules.ticks_per_day = ERRAND_DAY;
  Holding house;
  house.housing = 2;
  return World{
      Seed{1},
      Map{errand_rows(ERRAND_HEIGHT)},
      {ArmySpec{ERRAND_CENTRE,
                {Recruit{ERRAND_CENTRE, 1}, Recruit{BESIDE_CENTRE, 1}},
                0,
                {Skill{0, 0, 0, 0, 1}}}},
      {Structure{WEST_HOUSE, BUILT, HOUSE_FOOTPRINT, true, Role::HOME},
       Structure{EAST_SMITHY, BUILT, {3, 2}, true, Role::SQUARE}},
      rules,
      {VillageSpec{{0, 1}, 0, {house, {}}, 0, {}, {"house", "smithy"}}},
      {},
      life};
}

TEST(Save, AWorldLoadedMidErrandWalksOnAsTheOneThatRanOn) {
  // Home by hour 2, then halfway to the smithy; and long enough after for
  // the walk to end.
  constexpr std::uint64_t MID_WALK = 100;
  constexpr std::uint64_t WALKED_ON = 200;
  World ran = errand_world();
  run_all({&ran}, MID_WALK);
  ASSERT_EQ(ran.activity(0), Activity::WALKING);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, ran}, store, SLOT));
  World again = loaded(load(store, SLOT)).world;
  World steered = loaded(load(store, SLOT)).world;
  steered.steer(0, true);
  run_all({&ran, &again, &steered}, WALKED_ON);
  EXPECT_EQ(again.hash(), ran.hash());
  EXPECT_TRUE(stand_alike(again, ran, {0, 1}));
  EXPECT_EQ(again.activity(0), ran.activity(0));
  EXPECT_EQ(again.activity(1), ran.activity(1));
  // Control: a loaded world whose residents a driver steers walks no errand,
  // so it does not go on as the one that ran on.
  EXPECT_FALSE(stand_alike(steered, ran, {0}));
}

TEST(Save, AWorldLoadedBuildsEveryPlacesFieldBeforeItsFirstTick) {
  // The square, the house and the smithy.
  constexpr std::uint64_t PLACES = 3;
  constexpr std::uint64_t MID_WALK = 100;
  World ran = errand_world();
  run_all({&ran}, MID_WALK);
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, ran}, store, SLOT));
  const World again = loaded(load(store, SLOT)).world;
  EXPECT_EQ(again.schedule_fields().fields, PLACES);
  EXPECT_EQ(again.schedule_fields().builds, PLACES);
  EXPECT_EQ(again.schedule_fields().bytes, ran.schedule_fields().bytes);
  // Control: the world that ran built them as it was created, and no tick
  // since has built one again.
  EXPECT_EQ(ran.schedule_fields().builds, PLACES);
}

// A village of a house of one and a house of two, a married couple and a
// single smith, all three smiths, at the centre of an open 30 by 12 map: the
// couple fill the second house and the single smith the first. A foe of the
// hostile faction, steered, stands by the second house's door: its one blow
// kills the first resident in its reach, and their one blow kills it. Smiths
// are home all day, awake throughout; a day is 2,400 ticks, so no day's rules
// fall in the test.
constexpr std::size_t DOORSTEP_HEIGHT = 12;
constexpr Cell DOORSTEP_CENTRE{15, 6};
constexpr std::array<Cell, 3> DOORSTEP_RESIDENTS{{{15, 7}, {16, 7}, {18, 2}}};
constexpr Cell SOUTH_HOUSE{4, 8};
constexpr Cell BY_SOUTH_DOOR{9, 9};
constexpr std::size_t THE_SINGLE = 2;
constexpr std::size_t THE_FOE = 3;

World doorstep_world() {
  constexpr std::uint32_t NEVER = 1'000'000;
  constexpr std::int32_t CLAIM = 15;
  constexpr std::uint64_t LONG_DAY = 2'400;
  constexpr std::int32_t SIGHT = 12;
  Life life;
  life.founding_age = Spread{2 * DAYS_PER_YEAR, 0};
  life.occupations =
      Occupations{{Occupation{"smith", Nowhere{}, at_home(), {}}},
                  0,
                  0,
                  at_home(),
                  Waking{}};
  Growth growth;
  growth.interval = NEVER;
  growth.claim = CLAIM;
  life.growth = growth;
  Rules rules{0, {{false, true}, {true, false}}, STAGE_FLOORS};
  rules.ticks_per_day = LONG_DAY;
  rules.sight = SIGHT;
  const Skill blow{2 * CELL_SIZE, 0, 1, 0, NEVER};
  Holding one;
  one.housing = 1;
  Holding two;
  two.housing = 2;
  std::vector<Recruit> residents;
  residents.reserve(DOORSTEP_RESIDENTS.size());
  for (const Cell cell : DOORSTEP_RESIDENTS) {
    residents.push_back(Recruit{cell, 1});
  }
  World world{
      Seed{1},
      Map{errand_rows(DOORSTEP_HEIGHT)},
      {ArmySpec{DOORSTEP_CENTRE, residents, 0, {blow}},
       ArmySpec{BY_SOUTH_DOOR, {Recruit{BY_SOUTH_DOOR, 1}}, 1, {blow}}},
      {Structure{WEST_HOUSE, BUILT, HOUSE_FOOTPRINT, true, Role::HOME},
       Structure{SOUTH_HOUSE, BUILT, HOUSE_FOOTPRINT, true, Role::HOME}},
      rules,
      {VillageSpec{{0, 1}, 0, {one, two}, 1, {}, {"house", "house"}}},
      {},
      life};
  world.steer(1, true);
  return world;
}

// Whether the foe and one of the couple of doorstep_world() have fallen.
bool fallen(const World& world) {
  return world.person(THE_FOE).health == 0 &&
         (world.person(0).health == 0 || world.person(1).health == 0);
}

// Cells along the farther axis between person `person` of `world`, a world
// doorstep_world() made, and its first house's door.
std::int32_t from_first_door(const World& world, std::size_t person) {
  const Cell first_door =
      door(Map{errand_rows(DOORSTEP_HEIGHT)},
           Structure{WEST_HOUSE, BUILT, HOUSE_FOOTPRINT, true, Role::HOME},
           DOORSTEP_CENTRE)
          .value_or(Cell{-1, -1});
  const Cell stands = cell_of(world.person(person).position);
  return std::max(std::abs(stands.x - first_door.x),
                  std::abs(stands.y - first_door.y));
}

// Runs `world`, a world doorstep_world() made, until the foe and one of the
// couple have fallen, or for 400 ticks.
void fight(World& world) {
  constexpr std::uint64_t LONGEST = 400;
  for (std::uint64_t tick = 0; tick < LONGEST && !fallen(world); ++tick) {
    world.tick();
  }
}

TEST(Save, AWorldLoadedAfterADeathHousesItsResidentsAsTheOneThatRanOn) {
  // Ticks enough for the walks after the fight.
  constexpr std::uint64_t WALKED_ON = 300;
  World ran = doorstep_world();
  fight(ran);
  ASSERT_TRUE(fallen(ran));
  ASSERT_GT(ran.person(THE_SINGLE).health, 0U);
  const std::size_t widowed = ran.person(0).health > 0 ? 0 : 1;
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, ran}, store, SLOT));
  World again = loaded(load(store, SLOT)).world;
  run_all({&ran, &again}, WALKED_ON);
  EXPECT_TRUE(stand_alike(again, ran, {widowed, THE_SINGLE}));
  EXPECT_EQ(again.hash(), ran.hash());
  // Widowed, it lives alone and comes first in the roster, so it takes the
  // first house, and the single smith moves to the second.
  EXPECT_LE(from_first_door(ran, widowed), 1);
}

TEST(Save, WakingHoursComeBackAsSavedAndTheHashFoldsThem) {
  // Waking from 7 until 21; until 20 instead; and until 25, past the day.
  constexpr Waking AWAKE{7, 21};
  constexpr std::uint8_t EARLIER = 20;
  constexpr std::uint8_t PAST_THE_DAY = 25;
  Life life;
  life.occupations = Occupations{
      {Occupation{"first", Nowhere{}, at_home(), {}}}, 0, 0, at_home(), AWAKE};
  const World world{
      Seed{1},
      Map{{"....", "...."}},
      {ArmySpec{{0, 0}, {Recruit{{0, 0}, 1}}, 0, {Skill{0, 0, 0, 0, 1}}}},
      {},
      Rules{0, {{false}}, STAGE_FLOORS},
      {VillageSpec{{}, 0, {}, 0, {}, {}}},
      {},
      life};
  MemorySaves store;
  ASSERT_TRUE(save(Saved{std::nullopt, world}, store, SLOT));
  const std::vector<std::uint8_t> intact = store.bytes(SLOT);
  std::array<std::uint8_t, HEADER_BYTES> header{};
  std::copy_n(intact.begin(), HEADER_BYTES, header.begin());
  std::vector<std::uint8_t> payload(
      intact.begin() + static_cast<std::ptrdiff_t>(HEADER_BYTES),
      intact.end() - static_cast<std::ptrdiff_t>(CHECKSUM_BYTES));
  // Control: resealing the same payload loads, so the reseal refuses nothing.
  store.bytes(SLOT) = resealed(header, payload);
  ASSERT_EQ(loaded(load(store, SLOT)).world.hash(), world.hash());

  // The waking hours, each four bytes.
  const std::vector<std::uint8_t> hours{
      static_cast<std::uint8_t>(AWAKE.from), 0, 0, 0,
      static_cast<std::uint8_t>(AWAKE.to),   0, 0, 0};
  const auto found =
      std::search(payload.begin(), payload.end(), hours.begin(), hours.end());
  ASSERT_NE(found, payload.end());
  ASSERT_EQ(
      std::search(std::next(found), payload.end(), hours.begin(), hours.end()),
      payload.end());
  const std::size_t until =
      static_cast<std::size_t>(found - payload.begin()) + sizeof(std::uint32_t);
  // Waking until earlier, the world loads and hashes otherwise.
  std::vector<std::uint8_t> earlier = payload;
  earlier.at(until) = EARLIER;
  store.bytes(SLOT) = resealed(header, earlier);
  EXPECT_NE(loaded(load(store, SLOT)).world.hash(), world.hash());
  // Waking past the day, the save is refused.
  std::vector<std::uint8_t> past = payload;
  past.at(until) = PAST_THE_DAY;
  store.bytes(SLOT) = resealed(header, past);
  EXPECT_EQ(refusal(load(store, SLOT)), LoadError::CORRUPTED);
}

}  // namespace
}  // namespace murim
