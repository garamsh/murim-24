#include "spatial_hash.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr std::size_t POPULATION = 400;
constexpr std::uint64_t FIELD = std::uint64_t{64} * CELL_SIZE;
constexpr std::int32_t RADIUS = CELL_SIZE;
// Knuth's MMIX linear congruential generator; the high bits are the output.
constexpr std::uint64_t LCG_MULTIPLIER = 6364136223846793005ULL;
constexpr std::uint64_t LCG_INCREMENT = 1442695040888963407ULL;
constexpr unsigned LCG_SHIFT = 33U;

std::vector<Position> random_population(std::uint64_t seed) {
  std::uint64_t state = seed;
  const auto next = [&state]() {
    state = (state * LCG_MULTIPLIER) + LCG_INCREMENT;
    return static_cast<std::int32_t>((state >> LCG_SHIFT) % FIELD);
  };
  std::vector<Position> positions;
  positions.reserve(POPULATION);
  for (std::size_t i = 0; i < POPULATION; ++i) {
    const std::int32_t across = next();
    positions.push_back(Position{across, next()});
  }
  return positions;
}

std::vector<std::size_t> brute_force(const std::vector<Position>& positions,
                                     std::size_t index) {
  const Position centre = positions.at(index);
  std::vector<std::size_t> found;
  for (std::size_t other = 0; other < positions.size(); ++other) {
    const std::int64_t offset_x =
        std::int64_t{positions.at(other).x} - centre.x;
    const std::int64_t offset_y =
        std::int64_t{positions.at(other).y} - centre.y;
    if ((offset_x * offset_x) + (offset_y * offset_y) <=
        std::int64_t{RADIUS} * RADIUS) {
      found.push_back(other);
    }
  }
  return found;
}

TEST(SpatialHash, WithinMatchesABruteForceScan) {
  const std::vector<Position> positions = random_population(7);
  const SpatialHash hash{positions};

  std::size_t with_neighbours = 0;
  std::size_t without_neighbours = 0;
  for (std::size_t index = 0; index < POPULATION; ++index) {
    const std::vector<std::size_t> expected = brute_force(positions, index);
    EXPECT_EQ(hash.within(positions.at(index), RADIUS), expected)
        << "agent " << index;
    // Every position lies within the radius of itself.
    ++(expected.size() == 1 ? without_neighbours : with_neighbours);
  }
  // The population must hold both cases, or one of them went untested.
  EXPECT_GT(with_neighbours, 0U);
  EXPECT_GT(without_neighbours, 0U);
}

}  // namespace
}  // namespace murim
