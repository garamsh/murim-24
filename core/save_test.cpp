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

#include "map.hpp"
#include "region.hpp"
#include "world.hpp"

namespace murim {
namespace {

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
  Region region = generate_region(seed, region_data());
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

}  // namespace
}  // namespace murim
