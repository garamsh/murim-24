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

#include "m1_scenario.hpp"
#include "map.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr int WARM_UP_TICKS = 2 * TICKS_PER_SECOND;
constexpr int TIMED_TICKS = 20 * TICKS_PER_SECOND;
constexpr double BUDGET_MS = 4.0;
constexpr std::uint64_t EXPECTED_AGENTS = 20'000;
constexpr std::uint64_t EXPECTED_NEAR_AGENTS = 500;

// Agents of every army, by the tier its lead body stands in.
std::array<std::uint64_t, 3> agents_per_tier(const World& world,
                                             std::size_t armies,
                                             Cell focus_cell) {
  std::array<std::uint64_t, 3> agents{0, 0, 0};
  const Position focus = centre_of(focus_cell);
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
std::vector<std::vector<std::uint64_t>> cooldowns(const World& world,
                                                  std::size_t near_armies) {
  std::vector<std::vector<std::uint64_t>> ready;
  for (std::size_t army = 0; army < near_armies; ++army) {
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
  for (std::size_t army = 0; army < before.size(); ++army) {
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
  const M1Scenario scenario = m1_scenario();
  const std::size_t armies = scenario.armies.size();
  World world{scenario.seed, Map{scenario.map_rows}, scenario.armies,
              scenario.structures, scenario.rules};
  world.set_focus(centre_of(scenario.focus));

  std::uint64_t agents = 0;
  for (const ArmySpec& army : scenario.armies) {
    agents += army.count;
  }
  Strikes warm_up{0, 0};
  for (int tick = 0; tick < WARM_UP_TICKS; ++tick) {
    const auto before = cooldowns(world, scenario.near_armies);
    world.tick();
    const Strikes strikes = strikes_between(
        before, cooldowns(world, scenario.near_armies), scenario.casters_first);
    warm_up.all += strikes.all;
    warm_up.area += strikes.area;
  }
  const std::array<std::uint64_t, 3> tiers =
      agents_per_tier(world, armies, scenario.focus);
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
    const auto before = cooldowns(world, scenario.near_armies);
    const auto start = std::chrono::steady_clock::now();
    world.tick();
    durations.push_back(std::chrono::steady_clock::now() - start);
    const Strikes strikes = strikes_between(
        before, cooldowns(world, scenario.near_armies), scenario.casters_first);
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
            << "scenario: M1 (core/m1_scenario.cpp), seed "
            << static_cast<std::uint64_t>(scenario.seed) << ", "
            << scenario.map_rows.size() << "x" << scenario.map_rows.size()
            << " cells, " << armies << " armies, " << scenario.structures.size()
            << " structures\n"
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
