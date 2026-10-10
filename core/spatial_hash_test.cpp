#include "spatial_hash.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr std::size_t POPULATION = 400;
constexpr std::uint64_t FIELD = std::uint64_t{64} * CELL_SIZE;
constexpr std::int32_t RADIUS = CELL_SIZE;
constexpr std::uint64_t UINT64_LIMIT =
    std::numeric_limits<std::uint64_t>::max();
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

TEST(SpatialHash, VisitWithinMatchesABruteForceScan) {
  const std::vector<Position> positions = random_population(7);
  const SpatialHash hash{positions};

  std::size_t with_neighbours = 0;
  std::size_t without_neighbours = 0;
  for (std::size_t index = 0; index < POPULATION; ++index) {
    const std::vector<std::size_t> expected = brute_force(positions, index);
    std::vector<std::size_t> found;
    hash.visit_within(positions.at(index), RADIUS,
                      [&found](std::size_t other) { found.push_back(other); });
    std::sort(found.begin(), found.end());
    EXPECT_EQ(found, expected) << "agent " << index;
    // Every position lies within the radius of itself.
    ++(expected.size() == 1 ? without_neighbours : with_neighbours);
  }
  // The population must hold both cases, or one of them went untested.
  EXPECT_GT(with_neighbours, 0U);
  EXPECT_GT(without_neighbours, 0U);
}

// The position nearest `index`'s within `radius`, other than itself, among
// those `accepted`, the lowest index among equals, by checking every position.
std::optional<std::size_t> nearest_by_force(
    std::size_t index, const std::vector<Position>& positions,
    std::int32_t radius, const std::vector<bool>& accepted) {
  const Position centre = positions.at(index);
  std::optional<std::size_t> best;
  std::int64_t best_distance = std::int64_t{radius} * radius;
  for (std::size_t other = 0; other < positions.size(); ++other) {
    const std::int64_t offset_x =
        std::int64_t{positions.at(other).x} - centre.x;
    const std::int64_t offset_y =
        std::int64_t{positions.at(other).y} - centre.y;
    const std::int64_t distance = (offset_x * offset_x) + (offset_y * offset_y);
    if (other != index && accepted.at(other) &&
        (distance < best_distance || (distance == best_distance && !best))) {
      best = other;
      best_distance = distance;
    }
  }
  return best;
}

TEST(SpatialHash, NearestMatchesABruteForceScan) {
  const std::vector<Position> positions = random_population(11);
  // Each position is of one of four kinds and the search asks for two of
  // them; of those, it accepts the indices not divisible by three.
  constexpr std::uint64_t SOUGHT = 0b0110U;
  std::vector<SpatialHash::Placed> placed;
  std::vector<bool> accepted(positions.size());
  for (std::size_t index = 0; index < positions.size(); ++index) {
    const std::uint64_t kind = std::uint64_t{1} << (index % 4);
    placed.push_back(SpatialHash::Placed{positions.at(index), kind});
    accepted.at(index) = (kind & SOUGHT) != 0 && index % 3 != 0;
  }
  const SpatialHash hash{placed};
  constexpr std::int32_t SIGHT = 3 * CELL_SIZE;

  std::size_t found = 0;
  std::size_t none = 0;
  for (std::size_t index = 0; index < positions.size(); ++index) {
    const std::optional<std::size_t> expected =
        nearest_by_force(index, positions, SIGHT, accepted);
    const std::optional<std::size_t> nearest = hash.nearest(
        SOUGHT, positions.at(index), SIGHT, [index](std::size_t other) {
          return other != index && other % 3 != 0;
        });
    EXPECT_EQ(nearest, expected) << "agent " << index;
    ++(expected ? found : none);
  }
  // Both cases must occur, or one of them went untested.
  EXPECT_GT(found, 0U);
  EXPECT_GT(none, 0U);
}

TEST(SpatialHash, NearestTakesTheLowerIndexAmongEquals) {
  const Position centre = centre_of(Cell{10, 10});
  // Two cells below and two above: the walk meets the one above first.
  const std::vector<Position> positions{
      centre, Position{centre.x, centre.y + (2 * CELL_SIZE)},
      Position{centre.x, centre.y - (2 * CELL_SIZE)}};
  const SpatialHash hash{positions};
  constexpr std::int32_t SIGHT = 6 * CELL_SIZE;

  EXPECT_EQ(hash.nearest(UINT64_LIMIT, centre, SIGHT,
                         [](std::size_t other) { return other != 0; }),
            std::optional<std::size_t>{1});
  // The control: the one above is found when the one below is not offered.
  EXPECT_EQ(hash.nearest(UINT64_LIMIT, centre, SIGHT,
                         [](std::size_t other) { return other == 2; }),
            std::optional<std::size_t>{2});
}

TEST(SpatialHash, NearestPassesOverPositionsOfOtherKinds) {
  const Position centre = centre_of(Cell{10, 10});
  constexpr std::uint64_t NEAR_KIND = 0b01U;
  constexpr std::uint64_t FAR_KIND = 0b10U;
  // One of each kind, the near one a cell away and the far one three.
  const SpatialHash hash{std::vector<SpatialHash::Placed>{
      {Position{centre.x + CELL_SIZE, centre.y}, NEAR_KIND},
      {Position{centre.x + (3 * CELL_SIZE), centre.y}, FAR_KIND}}};
  constexpr std::int32_t SIGHT = 6 * CELL_SIZE;
  const auto any = [](std::size_t /*other*/) { return true; };

  EXPECT_EQ(hash.nearest(FAR_KIND, centre, SIGHT, any),
            std::optional<std::size_t>{1});
  // The control: asked for either kind, it finds the near one.
  EXPECT_EQ(hash.nearest(NEAR_KIND | FAR_KIND, centre, SIGHT, any),
            std::optional<std::size_t>{0});
}

TEST(SpatialHash, NearestFindsAPositionAtTheRadiusAndNoneOneUnitPast) {
  const Position centre = centre_of(Cell{10, 10});
  constexpr std::int32_t SIGHT = 3 * CELL_SIZE;
  const SpatialHash at_radius{
      std::vector<Position>{centre, Position{centre.x + SIGHT, centre.y}}};
  const SpatialHash past_radius{
      std::vector<Position>{centre, Position{centre.x + SIGHT + 1, centre.y}}};
  const auto other = [](std::size_t index) { return index != 0; };

  EXPECT_EQ(at_radius.nearest(UINT64_LIMIT, centre, SIGHT, other),
            std::optional<std::size_t>{1});
  EXPECT_EQ(past_radius.nearest(UINT64_LIMIT, centre, SIGHT, other),
            std::nullopt);
  // The control: the radius one unit wider finds the one past it.
  EXPECT_EQ(past_radius.nearest(UINT64_LIMIT, centre, SIGHT + 1, other),
            std::optional<std::size_t>{1});
}

TEST(SpatialHash, NearestWeighsAnEqualDistanceOnALaterRing) {
  const Position centre = centre_of(Cell{10, 10});
  // Both five cells away: the lower index five cells east, on ring five, and
  // the higher three east and four south, on ring four, which the walk
  // meets first.
  const std::vector<Position> positions{
      centre, Position{centre.x + (5 * CELL_SIZE), centre.y},
      Position{centre.x + (3 * CELL_SIZE), centre.y + (4 * CELL_SIZE)}};
  const SpatialHash hash{positions};
  constexpr std::int32_t SIGHT = 6 * CELL_SIZE;

  EXPECT_EQ(hash.nearest(UINT64_LIMIT, centre, SIGHT,
                         [](std::size_t other) { return other != 0; }),
            std::optional<std::size_t>{1});
  // The control: the one on ring four is found when the other is not
  // offered.
  EXPECT_EQ(hash.nearest(UINT64_LIMIT, centre, SIGHT,
                         [](std::size_t other) { return other == 2; }),
            std::optional<std::size_t>{2});
}

}  // namespace
}  // namespace murim
