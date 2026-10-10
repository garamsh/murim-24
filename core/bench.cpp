// Measures the tick cost of the M1 and the live benchmark scenarios against
// the tick budget. `make bench` builds it optimised and runs it; it is not a
// test.

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

#include "live_scenario.hpp"
#include "m1_scenario.hpp"
#include "map.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr int WARM_UP_TICKS = 2 * TICKS_PER_SECOND;
constexpr int TIMED_TICKS = 20 * TICKS_PER_SECOND;
constexpr double BUDGET_MS = 4.0;
// The strikes a tick the battle made with its lines striking across the
// barricade at three cells; fewer and it no longer bounds a battle in play.
constexpr std::uint64_t LEAST_STRIKES_PER_TICK = 16;

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

// Whether each person serves in one of armies `first` to `last`, `last`
// excluded.
std::vector<bool> serving(const World& world, std::size_t first,
                          std::size_t last) {
  std::vector<bool> serves(world.population(), false);
  for (std::size_t army = first; army < last; ++army) {
    for (const std::size_t member : world.roster(army)) {
      serves.at(member) = true;
    }
  }
  return serves;
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

struct TickTimes {
  double p50;
  double p99;
  double max;
};

// The median, 99th percentile and longest of `durations`, in milliseconds.
TickTimes times_of(std::vector<std::chrono::steady_clock::duration> durations) {
  constexpr std::size_t PERCENTILE = 99;
  constexpr std::size_t PERCENT = 100;
  std::sort(durations.begin(), durations.end());
  const std::size_t rank =
      ((durations.size() * PERCENTILE) + PERCENTILE) / PERCENT;
  return TickTimes{milliseconds(durations.at(durations.size() / 2)),
                   milliseconds(durations.at(rank - 1)),
                   milliseconds(durations.back())};
}

// The people still alive in the world's armies.
std::size_t living(const World& world, std::size_t armies) {
  std::size_t people = 0;
  for (std::size_t army = 0; army < armies; ++army) {
    people += world.roster(army).size();
  }
  return people;
}

// Fails the bench when `peak`, the most people alive at once, reached the
// capacity `capacity` the scenario's rules set: a world that fills its
// capacity is held back by it, and its timing no longer measures the people
// it was built for.
void require_below_capacity(std::size_t peak, std::size_t capacity) {
  require(peak < capacity,
          "the living must never reach the scenario's capacity, " +
              std::to_string(capacity) + "; they peaked at " +
              std::to_string(peak));
}

bool run_m1() {
  const M1Scenario scenario = m1_scenario();
  const std::size_t armies = scenario.armies.size();
  World world{scenario.seed, Map{scenario.map_rows}, scenario.armies,
              scenario.structures, scenario.rules};
  const std::size_t people = living(world, armies);
  const std::vector<bool> caster = casters(world, scenario.casters_first);
  std::size_t peak = living(world, world.army_count());

  Strikes warm_up{0, 0};
  for (int tick = 0; tick < WARM_UP_TICKS; ++tick) {
    const auto before = cooldowns(world);
    world.tick();
    peak = std::max(peak, living(world, world.army_count()));
    const Strikes strikes = strikes_between(before, cooldowns(world), caster);
    warm_up.all += strikes.all;
    warm_up.area += strikes.area;
  }
  require(people == MAX_PEOPLE, "the world must hold MAX_PEOPLE people");
  require(warm_up.all > 0, "the armies must strike during the warm-up");
  require(warm_up.area > 0, "area strikes must happen during the warm-up");

  std::vector<std::chrono::steady_clock::duration> durations;
  Strikes timed{0, 0};
  for (int tick = 0; tick < TIMED_TICKS; ++tick) {
    const auto before = cooldowns(world);
    const auto start = std::chrono::steady_clock::now();
    world.tick();
    durations.push_back(std::chrono::steady_clock::now() - start);
    peak = std::max(peak, living(world, world.army_count()));
    const Strikes strikes = strikes_between(before, cooldowns(world), caster);
    timed.all += strikes.all;
    timed.area += strikes.area;
  }
  const auto [p50, p99, max] = times_of(durations);
  const bool pass = p99 <= BUDGET_MS;
  require(living(world, armies) > people / 2,
          "most of the battle must still be fighting when timing ends");
  require_below_capacity(peak, scenario.rules.people_capacity);
  require(timed.all >=
              LEAST_STRIKES_PER_TICK * static_cast<std::uint64_t>(TIMED_TICKS),
          "the battle must strike at least 16 times a tick while timed");

  std::cout << std::fixed << std::setprecision(3)
            << "scenario: M1 (core/m1_scenario.cpp), seed "
            << static_cast<std::uint64_t>(scenario.seed) << ", "
            << scenario.map_rows.size() << "x" << scenario.map_rows.size()
            << " cells, " << armies << " armies, " << scenario.structures.size()
            << " structures\n"
            << "ticks: " << WARM_UP_TICKS << " warm-up, then " << TIMED_TICKS
            << " timed, at " << TICKS_PER_SECOND << " ticks per second\n"
            << "people: " << people << ", all in one battle; "
            << living(world, armies) << " alive when timing ended; at most "
            << peak << " alive at once, under a capacity of "
            << scenario.rules.people_capacity << "\n"
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
  return pass;
}

// The live scenario's battles must strike at least this often a tick while
// timed, or they no longer bound two battles in play.
constexpr std::uint64_t LEAST_BATTLE_STRIKES_PER_TICK = 16;

bool run_live() {
  const LiveScenario scenario = live_scenario();
  const std::size_t armies = scenario.armies.size();
  World world{scenario.seed,
              Map{scenario.map_rows},
              scenario.armies,
              {},
              scenario.rules};
  const std::size_t people = living(world, armies);
  const std::vector<bool> fighting = serving(world, 0, scenario.battle_armies);
  std::size_t peak = living(world, world.army_count());
  require(people == LIVE_PEOPLE, "the world must hold LIVE_PEOPLE people");

  Strikes warm_up{0, 0};
  for (int tick = 0; tick < WARM_UP_TICKS; ++tick) {
    const auto before = cooldowns(world);
    world.tick();
    peak = std::max(peak, living(world, world.army_count()));
    const Strikes strikes = strikes_between(before, cooldowns(world), fighting);
    warm_up.all += strikes.all;
    warm_up.area += strikes.area;
  }
  require(warm_up.area > 0, "the battles must strike during the warm-up");

  std::vector<std::chrono::steady_clock::duration> durations;
  Strikes timed{0, 0};
  for (int tick = 0; tick < TIMED_TICKS; ++tick) {
    const auto before = cooldowns(world);
    const auto start = std::chrono::steady_clock::now();
    world.tick();
    durations.push_back(std::chrono::steady_clock::now() - start);
    peak = std::max(peak, living(world, world.army_count()));
    const Strikes strikes = strikes_between(before, cooldowns(world), fighting);
    timed.all += strikes.all;
    timed.area += strikes.area;
  }
  const auto [p50, p99, max] = times_of(durations);
  const bool pass = p99 <= BUDGET_MS;
  require(living(world, armies) > people / 2,
          "most of the world must still live when timing ends");
  require_below_capacity(peak, scenario.rules.people_capacity);
  require(timed.area >= LEAST_BATTLE_STRIKES_PER_TICK *
                            static_cast<std::uint64_t>(TIMED_TICKS),
          "the battles must strike at least 16 times a tick while timed");

  std::cout << std::fixed << std::setprecision(3)
            << "scenario: live (core/live_scenario.cpp), seed "
            << static_cast<std::uint64_t>(scenario.seed) << ", "
            << scenario.map_rows.size() << "x" << scenario.map_rows.size()
            << " cells, " << armies << " armies\n"
            << "ticks: " << WARM_UP_TICKS << " warm-up, then " << TIMED_TICKS
            << " timed, at " << TICKS_PER_SECOND << " ticks per second\n"
            << "people: " << people << ", "
            << std::count(fighting.begin(), fighting.end(), true)
            << " in two battles, the rest in villages and on roads; "
            << living(world, armies) << " alive when timing ended; at most "
            << peak << " alive at once, under a capacity of "
            << scenario.rules.people_capacity << "\n"
            << "strikes per tick: "
            << static_cast<double>(timed.all) / TIMED_TICKS
            << ", of which in battle "
            << static_cast<double>(timed.area) / TIMED_TICKS << "\n"
            << "tick time: p50 " << p50 << " ms, p99 " << p99 << " ms, max "
            << max << " ms\n"
            << (pass ? "PASS" : "FAIL") << ": p99 " << p99 << " ms against a "
            << BUDGET_MS << " ms budget\n";
  return pass;
}

// Runs both scenarios, each reporting before either decides the exit status.
int run() {
  const bool m1_passed = run_m1();
  const bool live_passed = run_live();
  return m1_passed && live_passed ? EXIT_SUCCESS : EXIT_FAILURE;
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
