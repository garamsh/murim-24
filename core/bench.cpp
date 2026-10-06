// Measures the tick cost of the M1 benchmark scenario against the tick
// budget. `make bench` builds it optimised and runs it; it is not a test.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <ratio>
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
constexpr int TICKS_PER_SECOND = 30;
constexpr int WARM_UP_TICKS = 2 * TICKS_PER_SECOND;
constexpr int TIMED_TICKS = 20 * TICKS_PER_SECOND;
constexpr double BUDGET_MS = 4.0;

// The armies of each tier: how many, of how many agents each.
constexpr std::size_t NEAR_ARMIES = 20;
constexpr std::uint32_t NEAR_COUNT = 25;
constexpr std::size_t MIDDLE_ARMIES = 30;
constexpr std::uint32_t MIDDLE_COUNT = 150;
constexpr std::size_t FAR_ARMIES = 60;
constexpr std::uint32_t FAR_COUNT = 250;
constexpr std::uint64_t EXPECTED_AGENTS = 20'000;
constexpr std::uint64_t EXPECTED_NEAR_AGENTS = 500;
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

struct Scenario {
  std::vector<ArmySpec> armies;
  std::vector<Structure> structures;
  std::size_t casters_first;
};

Scenario build_scenario() {
  Scenario scenario{{}, {}, 0};
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

Rules rules() {
  // The two lines are at war; the marching armies are at peace with all.
  const std::vector<std::vector<bool>> hostile{
      {false, true, false}, {true, false, false}, {false, false, false}};
  return Rules{SEPARATION, hostile, {DURABILITY, DURABILITY / 2, 1}};
}

// Agents of every army, by the tier its lead body stands in.
std::array<std::uint64_t, 3> agents_per_tier(const World& world,
                                             std::size_t armies) {
  std::array<std::uint64_t, 3> agents{0, 0, 0};
  const Position focus = centre_of(FOCUS);
  for (std::size_t army = 0; army < armies; ++army) {
    const std::vector<Body>& bodies = world.bodies(army);
    const Position lead = bodies.front().position;
    const std::int64_t distance =
        std::max(std::abs(std::int64_t{lead.x} - focus.x),
                 std::abs(std::int64_t{lead.y} - focus.y));
    std::size_t tier = 0;
    while (distance > TIERS.at(tier).reach) {
      ++tier;
    }
    for (const Body& body : bodies) {
      agents.at(tier) += body.count;
    }
  }
  return agents;
}

// The cooldowns of the near armies' bodies; a body whose cooldown moved
// between two ticks struck in the second.
std::vector<std::vector<std::uint64_t>> cooldowns(const World& world) {
  std::vector<std::vector<std::uint64_t>> ready;
  for (std::size_t army = 0; army < NEAR_ARMIES; ++army) {
    std::vector<std::uint64_t> bodies;
    for (const Body& body : world.bodies(army)) {
      bodies.push_back(body.ready_at);
    }
    ready.push_back(bodies);
  }
  return ready;
}

struct Strikes {
  std::uint64_t all;
  std::uint64_t area;
};

Strikes strikes_between(const std::vector<std::vector<std::uint64_t>>& before,
                        const std::vector<std::vector<std::uint64_t>>& after,
                        std::size_t casters_first) {
  Strikes strikes{0, 0};
  for (std::size_t army = 0; army < NEAR_ARMIES; ++army) {
    const std::vector<std::uint64_t>& earlier = before.at(army);
    const std::vector<std::uint64_t>& later = after.at(army);
    const bool caster = army == casters_first || army == casters_first + 1;
    for (std::size_t body = 0; body < std::min(earlier.size(), later.size());
         ++body) {
      const bool struck = later.at(body) != earlier.at(body);
      strikes.all += static_cast<std::uint64_t>(struck);
      strikes.area += static_cast<std::uint64_t>(struck && caster);
    }
  }
  return strikes;
}

std::size_t structures_broken(const World& world, std::size_t structures) {
  std::size_t broken = 0;
  for (std::size_t structure = 0; structure < structures; ++structure) {
    broken += static_cast<std::size_t>(world.stage(structure) != Stage::INTACT);
  }
  return broken;
}

// Prints `message` and exits with `status` when `holds` is false, so a
// degenerate scenario fails instead of timing quickly.
void require(bool holds, const std::string& message) {
  if (!holds) {
    std::cerr << "bench: setup check failed: " << message << "\n";
    std::exit(2);
  }
}

double milliseconds(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

int run() {
  const Scenario scenario = build_scenario();
  const std::size_t armies = scenario.armies.size();
  World world{
      Seed{SCENARIO_SEED},
      Map{std::vector<std::string>(MAP_SIDE, std::string(MAP_SIDE, '.'))},
      scenario.armies, scenario.structures, rules()};
  world.set_focus(centre_of(FOCUS));

  std::uint64_t agents = 0;
  for (const ArmySpec& army : scenario.armies) {
    agents += army.count;
  }
  Strikes warm_up{0, 0};
  for (int tick = 0; tick < WARM_UP_TICKS; ++tick) {
    const auto before = cooldowns(world);
    world.tick();
    const Strikes strikes =
        strikes_between(before, cooldowns(world), scenario.casters_first);
    warm_up.all += strikes.all;
    warm_up.area += strikes.area;
  }
  const std::array<std::uint64_t, 3> tiers = agents_per_tier(world, armies);
  require(agents == EXPECTED_AGENTS, "the world must hold 20000 agents");
  require(tiers.at(0) == EXPECTED_NEAR_AGENTS,
          "the near tier must hold the 500 agents of the battle");
  require(tiers.at(1) > 0 && tiers.at(2) > 0,
          "the middle and far tiers must hold armies");
  require(warm_up.all > 0, "the near armies must strike during the warm-up");
  require(warm_up.area > 0, "area strikes must happen during the warm-up");

  std::vector<std::chrono::steady_clock::duration> durations;
  Strikes timed{0, 0};
  for (int tick = 0; tick < TIMED_TICKS; ++tick) {
    const auto before = cooldowns(world);
    const auto start = std::chrono::steady_clock::now();
    world.tick();
    durations.push_back(std::chrono::steady_clock::now() - start);
    const Strikes strikes =
        strikes_between(before, cooldowns(world), scenario.casters_first);
    timed.all += strikes.all;
    timed.area += strikes.area;
  }
  std::sort(durations.begin(), durations.end());
  const double p50 = milliseconds(durations.at(durations.size() / 2));
  const double p99 =
      milliseconds(durations.at((((durations.size() * 99) + 99) / 100) - 1));
  const double max = milliseconds(durations.back());
  const bool pass = p99 <= BUDGET_MS;

  std::cout << std::fixed << std::setprecision(3)
            << "scenario: M1 (core/bench.cpp), seed " << SCENARIO_SEED << ", "
            << MAP_SIDE << "x" << MAP_SIDE << " cells, " << armies
            << " armies, " << scenario.structures.size() << " structures\n"
            << "ticks: " << WARM_UP_TICKS << " warm-up, then " << TIMED_TICKS
            << " timed, at " << TICKS_PER_SECOND << " ticks per second\n"
            << "agents: " << agents << " (near " << tiers.at(0) << ", middle "
            << tiers.at(1) << ", far " << tiers.at(2) << " after warm-up)\n"
            << "strikes per tick (near armies): "
            << static_cast<double>(timed.all) / TIMED_TICKS
            << ", of which area "
            << static_cast<double>(timed.area) / TIMED_TICKS << " ("
            << static_cast<double>(timed.area) * TICKS_PER_SECOND / TIMED_TICKS
            << " per second)\n"
            << "structures broken: "
            << structures_broken(world, scenario.structures.size()) << " of "
            << scenario.structures.size() << "\n"
            << "tick time: p50 " << p50 << " ms, p99 " << p99 << " ms, max "
            << max << " ms\n"
            << (pass ? "PASS" : "FAIL") << ": p99 " << p99 << " ms against a "
            << BUDGET_MS << " ms budget\n";
  return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace
}  // namespace murim

int main() {
  try {
    return murim::run();
  } catch (const std::exception& error) {
    std::cerr << "bench: " << error.what() << "\n";
    return 2;
  }
}
