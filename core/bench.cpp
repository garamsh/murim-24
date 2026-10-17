// Measures the tick cost of the M1 and the live benchmark scenarios against
// the tick budget. `make bench` builds it optimised and runs it; it is not a
// test.

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

#include "area_fields.hpp"
#include "areas.hpp"
#include "calendar.hpp"
#include "coarse_graph.hpp"
#include "limits.hpp"
#include "live_scenario.hpp"
#include "m1_scenario.hpp"
#include "map.hpp"
#include "village.hpp"
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
  // The two caster armies.
  const std::vector<bool> caster =
      serving(world, scenario.casters_first, scenario.casters_first + 2);
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

// A kibibyte, for the memory reports.
constexpr std::size_t KIB = 1'024;

// The nodes a search toward an objective has costed: its objective's
// region's, the border nodes it settled, and each costed region's.
std::size_t costed(const GoalSearch& search) {
  std::size_t count = search.near.size();
  count += static_cast<std::size_t>(
      std::count(search.settled.begin(), search.settled.end(), true));
  for (const auto& [region, costs] : search.regions) {
    count += costs.size();
  }
  return count;
}

// About the memory a search holds: each node it costed in a tree, a tree
// node of three pointers and a colour beside its value, and a cost and a
// flag for every border node, with its frontier.
std::size_t search_bytes(const GoalSearch& search) {
  constexpr std::size_t TREE_NODE = 4 * sizeof(void*);
  using Node = std::pair<std::size_t, std::size_t>;
  std::size_t in_trees = search.near.size();
  for (const auto& [region, costs] : search.regions) {
    in_trees += costs.size() + 1;
  }
  return (in_trees * (TREE_NODE + sizeof(std::pair<const Node, CoarseCost>))) +
         (search.border.size() * sizeof(CoarseCost)) +
         (search.settled.size() / CHAR_BIT) +
         (search.frontier.size() * sizeof(std::pair<CoarseCost, std::size_t>));
}

// Times the live scenario against the tick budget as a target, printing
// whether it meets it; it returns whether it did, which the exit status does
// not read (exit_status()).
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
  const std::size_t built_before = world.fields().built_in_all();
  std::size_t most_built = 0;
  // The searches toward objectives a build read in a tick, which end_tick()
  // keeps, at their most over the timed ticks.
  std::size_t most_objectives = 0;
  std::size_t most_costed = 0;
  std::size_t most_walk_bytes = 0;
  for (int tick = 0; tick < TIMED_TICKS; ++tick) {
    const auto before = cooldowns(world);
    const std::size_t built_at = world.fields().built_in_all();
    const auto start = std::chrono::steady_clock::now();
    world.tick();
    durations.push_back(std::chrono::steady_clock::now() - start);
    most_built = std::max(most_built, world.fields().built_in_all() - built_at);
    most_walk_bytes = std::max(most_walk_bytes, world.fields().walk_bytes());
    const std::vector<const GoalSearch*> searches = world.fields().searches();
    most_objectives = std::max(most_objectives, searches.size());
    for (const GoalSearch* search : searches) {
      most_costed = std::max(most_costed, costed(*search));
    }
    peak = std::max(peak, living(world, world.army_count()));
    const Strikes strikes = strikes_between(before, cooldowns(world), fighting);
    timed.all += strikes.all;
    timed.area += strikes.area;
  }
  const auto [p50, p99, max] = times_of(durations);
  const bool pass = p99 <= BUDGET_MS;
  const std::size_t built_timed = world.fields().built_in_all() - built_before;
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
            << "fields: " << world.fields().held()
            << " held when timing ended; built while timed "
            << static_cast<double>(built_timed) / TIMED_TICKS
            << " a tick, at most " << most_built << " in one tick\n"
            << "objectives searched toward in a tick: at most "
            << most_objectives << ", costing at most " << most_costed
            << " nodes each\n"
            << "walks within areas seeding the fields: at most "
            << most_walk_bytes / KIB << " KiB held in a tick\n"
            << "TARGET " << (pass ? "MET" : "MISSED") << ": p99 " << p99
            << " ms against a " << BUDGET_MS
            << " ms target, not yet in force (#216; ADR 0053 makes it "
               "binding), so it does not decide the exit status\n";
  return pass;
}

// Whether each scenario met the tick budget: M1's is in force, the live
// scenario's a target #216 has not met yet.
struct Met {
  bool m1;
  bool live;
};

// The 8,192-cell map the path measure crosses: open ground split by seven
// ranges, one every 1,024 cells, each crossed by one pass of 16 cells at a
// row that moves from range to range.
constexpr std::int32_t MAP_SIDE = 8'192;
constexpr std::int32_t RANGE_EVERY = 1'024;
constexpr std::int32_t PASS_WIDTH = 16;
constexpr std::int32_t PASS_STRIDE = 1'777;
constexpr int PATH_RUNS = 5;
constexpr std::int32_t FOOTPRINT = 3;
// A frame at 60 frames a second, in milliseconds.
constexpr double FRAME_MS = 16.7;

Map ranged_map() {
  std::vector<std::string> rows(static_cast<std::size_t>(MAP_SIDE),
                                std::string(MAP_SIDE, '.'));
  for (std::int32_t range = RANGE_EVERY; range < MAP_SIDE;
       range += RANGE_EVERY) {
    const std::int32_t pass =
        (range / RANGE_EVERY * PASS_STRIDE) % (MAP_SIDE - PASS_WIDTH);
    for (std::int32_t row = 0; row < MAP_SIDE; ++row) {
      if (row < pass || row >= pass + PASS_WIDTH) {
        rows.at(static_cast<std::size_t>(row))
            .at(static_cast<std::size_t>(range)) = '#';
      }
    }
  }
  return Map{rows};
}

// The median of `runs` timings of `measure`, in milliseconds.
template <typename Measure>
double median_ms(int runs, Measure measure) {
  std::vector<double> times;
  for (int run = 0; run < runs; ++run) {
    const auto start = std::chrono::steady_clock::now();
    measure();
    times.push_back(milliseconds(std::chrono::steady_clock::now() - start));
  }
  std::sort(times.begin(), times.end());
  return times.at(times.size() / 2);
}

// Times a path corner to corner across the 8,192-cell map over the coarse
// graph with its level of regions, against a frame, with the graph of areas
// alone as the control (ADR 0070, step 2). Reports only: no budget in force
// reads it.
void run_paths() {
  const Map map = ranged_map();
  const auto built_at = std::chrono::steady_clock::now();
  const CoarseGraph graph{map};
  const double build_ms =
      milliseconds(std::chrono::steady_clock::now() - built_at);
  const Cell from{10, 10};
  const Cell corner{MAP_SIDE - 10, MAP_SIDE - 10};
  std::optional<CoarseCost> by_regions;
  std::optional<CoarseCost> by_areas;
  const double regions_ms = median_ms(PATH_RUNS, [&] {
    by_regions = graph.path_cost(map, Walk{from, corner}, true);
  });
  const double areas_ms = median_ms(PATH_RUNS, [&] {
    by_areas = graph.path_cost(map, Walk{from, corner}, false);
  });
  require(by_regions.has_value() && by_areas.has_value(),
          "the path measure's corners must be joined");
  // Growth's lay rebuilds the graph where a structure blocks: here a 3 by 3
  // footprint in the first area.
  Map built = map;
  std::vector<Cell> blocked;
  for (std::int32_t row = 0; row < FOOTPRINT; ++row) {
    for (std::int32_t col = 0; col < FOOTPRINT; ++col) {
      blocked.push_back(Cell{from.x + col + 2, from.y + row + 2});
      built.block(blocked.back());
    }
  }
  // Each attempt rebuilds the graph in place and, when refused, reverts it.
  CoarseGraph laid = graph;
  const double lay_ms =
      median_ms(PATH_RUNS, [&] { laid.revert(laid.rebuild(built, blocked)); });
  require(laid == graph, "a reverted rebuild must leave the graph as it was");
  // The first field toward a fresh objective: the reverse search from it
  // costs the exits of the walker's area, and the field is built over them;
  // the search over the areas alone, for one exit, is its control.
  AreaFields fields;
  const auto asked_at = std::chrono::steady_clock::now();
  static_cast<void>(fields.field_for(map, graph, from, corner));
  const double first_field_ms =
      milliseconds(std::chrono::steady_clock::now() - asked_at);
  const GoalSearch& search = *fields.searches().at(0);
  const CoarseExit& exit = graph.exits(graph.area_of(from)).at(0);
  const NodeRef far{exit.area, exit.far_node};
  std::optional<CoarseCost> over_areas;
  const auto control_at = std::chrono::steady_clock::now();
  over_areas = graph.cost_over_areas(map, corner, far);
  const double over_areas_ms =
      milliseconds(std::chrono::steady_clock::now() - control_at);
  GoalSearch again = graph.toward(map, corner);
  require(over_areas.has_value() && graph.cost_from(again, far) == over_areas,
          "the search by regions must cost an exit as the areas alone do");
  // A map whose every area holds a blocked cell, so no area is costed in
  // closed form: the build's bound.
  const double obstructed_ms = [] {
    std::vector<std::string> rows(static_cast<std::size_t>(MAP_SIDE),
                                  std::string(MAP_SIDE, '.'));
    for (std::int32_t row = AREA_SIDE / 2; row < MAP_SIDE; row += AREA_SIDE) {
      for (std::int32_t col = AREA_SIDE / 2; col < MAP_SIDE; col += AREA_SIDE) {
        rows.at(static_cast<std::size_t>(row))
            .at(static_cast<std::size_t>(col)) = '#';
      }
    }
    const Map obstructed{rows};
    const auto start = std::chrono::steady_clock::now();
    const CoarseGraph built_graph{obstructed};
    return milliseconds(std::chrono::steady_clock::now() - start);
  }();
  std::cout << "scenario: path across the " << MAP_SIDE << "-cell map, "
            << graph.node_count() << " nodes, built in " << build_ms << " ms\n"
            << "path by regions: median " << regions_ms << " ms of "
            << PATH_RUNS << ", cost " << by_regions.value_or(0) << "; "
            << (regions_ms <= FRAME_MS ? "within" : "past") << " a frame of "
            << FRAME_MS << " ms\n"
            << "control, by areas alone: median " << areas_ms << " ms, cost "
            << by_areas.value_or(0) << "; "
            << (areas_ms <= FRAME_MS ? "within" : "past") << " a frame\n"
            << "graph build with a blocked cell in every area: "
            << obstructed_ms << " ms\n"
            << "lay's attempt, rebuilding the graph in place under a "
            << FOOTPRINT << " by " << FOOTPRINT
            << " footprint and reverting it: median " << lay_ms << " ms\n"
            << "first field toward a fresh objective across the map: "
            << first_field_ms << " ms, its search costing " << costed(search)
            << " nodes, about " << search_bytes(search) / KIB
            << " KiB, and its walks within areas " << fields.walk_bytes() / KIB
            << " KiB\n"
            << "control, one exit's cost over the areas alone: "
            << over_areas_ms << " ms\n";
}

// The bench's exit status: failure when a budget in force is missed. Only
// M1's tick is in force; the live scenario's is a target until ADR 0053
// binds it, so missing it leaves the status a success. A failed setup or a
// world reaching its capacity exits 2 before this is read.
int exit_status(Met met) { return met.m1 ? EXIT_SUCCESS : EXIT_FAILURE; }

// Runs both scenarios, each reporting before either decides the exit status,
// after checking the rule that decides it: a live miss alone leaves it at
// success, and an M1 miss does not.
int run() {
  require(exit_status(Met{true, false}) == EXIT_SUCCESS &&
              exit_status(Met{false, true}) == EXIT_FAILURE,
          "the exit status must follow M1's budget and not the live target");
  const bool m1_met = run_m1();
  const bool live_met = run_live();
  run_paths();
  return exit_status(Met{m1_met, live_met});
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
