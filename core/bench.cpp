// Measures the tick cost of the M1 benchmark scenario against the tick
// budget. `make bench` builds it optimised and runs it; it is not a test.

#include <algorithm>
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

// The cooldown of every person in the world; a person whose cooldown moved
// between two ticks struck in the second.
std::vector<std::uint64_t> cooldowns(const World& world) {
  std::vector<std::uint64_t> ready;
  ready.reserve(world.population());
  for (std::size_t person = 0; person < world.population(); ++person) {
    ready.push_back(world.person(person).ready_at);
  }
  return ready;
}

// Whether each person serves in one of the two caster armies.
std::vector<bool> casters(const World& world, std::size_t casters_first) {
  std::vector<bool> caster(world.population(), false);
  for (const std::size_t army : {casters_first, casters_first + 1}) {
    for (const std::size_t member : world.roster(army)) {
      caster.at(member) = true;
    }
  }
  return caster;
}

struct Strikes {
  std::uint64_t all;
  std::uint64_t area;
};

Strikes strikes_between(const std::vector<std::uint64_t>& before,
                        const std::vector<std::uint64_t>& after,
                        const std::vector<bool>& caster) {
  Strikes strikes{0, 0};
  for (std::size_t person = 0; person < before.size(); ++person) {
    const bool struck = after.at(person) != before.at(person);
    strikes.all += static_cast<std::uint64_t>(struck);
    strikes.area += static_cast<std::uint64_t>(struck && caster.at(person));
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

// The people still alive in the world's armies.
std::size_t living(const World& world, std::size_t armies) {
  std::size_t people = 0;
  for (std::size_t army = 0; army < armies; ++army) {
    people += world.roster(army).size();
  }
  return people;
}

int run() {
  const M1Scenario scenario = m1_scenario();
  const std::size_t armies = scenario.armies.size();
  World world{scenario.seed, Map{scenario.map_rows}, scenario.armies,
              scenario.structures, scenario.rules};
  const std::size_t people = living(world, armies);
  const std::vector<bool> caster = casters(world, scenario.casters_first);

  Strikes warm_up{0, 0};
  for (int tick = 0; tick < WARM_UP_TICKS; ++tick) {
    const auto before = cooldowns(world);
    world.tick();
    const Strikes strikes = strikes_between(before, cooldowns(world), caster);
    warm_up.all += strikes.all;
    warm_up.area += strikes.area;
  }
  require(people == MAX_PEOPLE,
          "the world must hold MAX_PEOPLE people, the most it may");
  require(warm_up.all > 0, "the armies must strike during the warm-up");
  require(warm_up.area > 0, "area strikes must happen during the warm-up");

  std::vector<std::chrono::steady_clock::duration> durations;
  Strikes timed{0, 0};
  for (int tick = 0; tick < TIMED_TICKS; ++tick) {
    const auto before = cooldowns(world);
    const auto start = std::chrono::steady_clock::now();
    world.tick();
    durations.push_back(std::chrono::steady_clock::now() - start);
    const Strikes strikes = strikes_between(before, cooldowns(world), caster);
    timed.all += strikes.all;
    timed.area += strikes.area;
  }
  std::sort(durations.begin(), durations.end());
  const double p50 = milliseconds(durations.at(durations.size() / 2));
  const double p99 =
      milliseconds(durations.at((((durations.size() * 99) + 99) / 100) - 1));
  const double max = milliseconds(durations.back());
  const bool pass = p99 <= BUDGET_MS;
  require(living(world, armies) > people / 2,
          "most of the battle must still be fighting when timing ends");

  std::cout << std::fixed << std::setprecision(3)
            << "scenario: M1 (core/m1_scenario.cpp), seed "
            << static_cast<std::uint64_t>(scenario.seed) << ", "
            << scenario.map_rows.size() << "x" << scenario.map_rows.size()
            << " cells, " << armies << " armies, " << scenario.structures.size()
            << " structures\n"
            << "ticks: " << WARM_UP_TICKS << " warm-up, then " << TIMED_TICKS
            << " timed, at " << TICKS_PER_SECOND << " ticks per second\n"
            << "people: " << people << ", all in one battle; "
            << living(world, armies) << " alive when timing ended\n"
            << "strikes per tick: "
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
