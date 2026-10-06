#include "m1_scenario.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr std::uint64_t SCENARIO_SEED = 2026;
constexpr std::int32_t MAP_SIDE = 128;
constexpr Cell FOCUS{64, 64};
constexpr std::int32_t SEPARATION = 72;

// The armies of each tier: how many, of how many agents each.
constexpr std::size_t NEAR_ARMIES = 20;
constexpr std::uint32_t NEAR_COUNT = 25;
constexpr std::size_t MIDDLE_ARMIES = 30;
constexpr std::uint32_t MIDDLE_COUNT = 150;
constexpr std::size_t FAR_ARMIES = 60;
constexpr std::uint32_t FAR_COUNT = 250;
constexpr std::uint64_t STRENGTH_PER_AGENT = 100'000;

// The near battle: two hostile lines of ten armies a column apart, with a
// row of structures between them. One army on each side are casters.
constexpr std::int32_t LINE_ROWS = 10;
constexpr std::int32_t FIRST_ROW = FOCUS.y - (LINE_ROWS / 2);
constexpr std::uint32_t DURABILITY = 3'000;
constexpr std::size_t FACTION_WEST = 0;
constexpr std::size_t FACTION_EAST = 1;
constexpr std::size_t FACTION_NEUTRAL = 2;

// A melee strike every half second, and an area strike every second whose
// disc reaches the structures between the lines: two caster armies of 25
// agents make 50 area strikes a second.
constexpr Skill JAB{3 * CELL_SIZE, 0, 1, 0, TICKS_PER_SECOND / 2};
constexpr Skill SWEEP{3 * CELL_SIZE, (3 * CELL_SIZE) / 2, 1, 1,
                      TICKS_PER_SECOND};
constexpr Skill IDLE{0, 0, 0, 0, 0};

// Marching armies at peace with all, in a band of Chebyshev distances in
// cells from the focus: `armies` armies of `count` agents each.
struct Band {
  std::size_t armies;
  std::uint32_t count;
  std::int32_t low;
  std::int32_t high;
};

// The middle tier's band, and the far tier's out to the map's edge.
constexpr std::array<Band, 2> BANDS{{
    {MIDDLE_ARMIES, MIDDLE_COUNT, 12, 30},
    {FAR_ARMIES, FAR_COUNT, 36, MAP_SIDE},
}};

// Knuth's MMIX linear congruential generator; the high bits are the output.
class Random {
 public:
  explicit Random(std::uint64_t seed) : state_(seed) {}

  std::int32_t below(std::int32_t bound) {
    constexpr std::uint64_t MULTIPLIER = 6364136223846793005ULL;
    constexpr std::uint64_t INCREMENT = 1442695040888963407ULL;
    constexpr unsigned SHIFT = 33U;
    state_ = (state_ * MULTIPLIER) + INCREMENT;
    return static_cast<std::int32_t>((state_ >> SHIFT) %
                                     static_cast<std::uint64_t>(bound));
  }

 private:
  std::uint64_t state_;
};

std::int32_t from_focus(Cell cell) {
  return std::max(std::abs(cell.x - FOCUS.x), std::abs(cell.y - FOCUS.y));
}

// A random cell whose distance from the focus lies in the band.
Cell cell_in_band(Random& random, const Band& band) {
  while (true) {
    const Cell cell{random.below(MAP_SIDE), random.below(MAP_SIDE)};
    const std::int32_t distance = from_focus(cell);
    if (distance >= band.low && distance <= band.high) {
      return cell;
    }
  }
}

Rules rules() {
  // The two lines are at war; the marching armies are at peace with all.
  const std::vector<std::vector<bool>> hostile{
      {false, true, false}, {true, false, false}, {false, false, false}};
  return Rules{SEPARATION, hostile, {DURABILITY, DURABILITY / 2, 1}};
}

}  // namespace

M1Scenario m1_scenario() {
  M1Scenario scenario{
      Seed{SCENARIO_SEED},
      std::vector<std::string>(MAP_SIDE, std::string(MAP_SIDE, '.')),
      {},
      {},
      rules(),
      FOCUS,
      NEAR_ARMIES,
      0};
  for (std::int32_t row = 0; row < LINE_ROWS; ++row) {
    const Cell west{FOCUS.x - 1, FIRST_ROW + row};
    const Cell east{FOCUS.x + 1, FIRST_ROW + row};
    const std::vector<Skill> skills{JAB};
    scenario.armies.push_back(ArmySpec{west, west, NEAR_COUNT,
                                       NEAR_COUNT * STRENGTH_PER_AGENT,
                                       FACTION_WEST, skills});
    scenario.armies.push_back(ArmySpec{east, east, NEAR_COUNT,
                                       NEAR_COUNT * STRENGTH_PER_AGENT,
                                       FACTION_EAST, skills});
    scenario.structures.push_back(
        Structure{Cell{FOCUS.x, FIRST_ROW + row}, DURABILITY});
  }
  // The middle pair of the lines are the casters.
  scenario.casters_first = LINE_ROWS;
  scenario.armies.at(scenario.casters_first).skills = {SWEEP};
  scenario.armies.at(scenario.casters_first + 1).skills = {SWEEP};

  Random random{SCENARIO_SEED};
  for (const Band& band : BANDS) {
    for (std::size_t army = 0; army < band.armies; ++army) {
      const Cell start = cell_in_band(random, band);
      const Cell objective = cell_in_band(random, band);
      scenario.armies.push_back(ArmySpec{start,
                                         objective,
                                         band.count,
                                         band.count * STRENGTH_PER_AGENT,
                                         FACTION_NEUTRAL,
                                         {IDLE}});
    }
  }
  return scenario;
}

}  // namespace murim
