// Lives the generated region of each seed in a range for a number of years,
// with days of five ticks, and prints per seed the households each faction
// starts with, the living and the peopled villages at the start and the end,
// the most living at once, and whether every faction held a peopled village
// at the end of every year. `make sweep` builds it optimised and runs it; it
// is not a test.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"
#include "region.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr std::uint64_t TICKS_PER_SWEPT_DAY = 5;
constexpr std::size_t GROWTH_FACTOR = 2;

// The peopled villages, and how many each faction holds.
std::pair<std::size_t, std::vector<std::size_t>> peopled(const World& world,
                                                         std::size_t factions) {
  std::pair<std::size_t, std::vector<std::size_t>> held{
      0, std::vector<std::size_t>(factions, 0)};
  for (std::size_t village = 0; village < world.village_count(); ++village) {
    if (world.residents_of(village) > 0) {
      ++held.first;
      ++held.second.at(world.owner(village));
    }
  }
  return held;
}

struct Lived {
  std::size_t living_before;
  std::size_t living_after;
  std::size_t peopled_before;
  std::size_t peopled_after;
  std::size_t peak;
  bool every_faction_peopled;
};

Lived live(Seed seed, std::uint64_t years, const RegionData& data) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = TICKS_PER_SWEPT_DAY;
  World world{region.seed,       Map{region.map_rows}, region.armies,
              region.structures, region.rules,         village_specs(region),
              region.leadership, region.life};
  const std::size_t factions = data.factions.size();
  Lived lived{world.living(), 0,   peopled(world, factions).first, 0,
              world.living(), true};
  for (std::uint64_t year = 0; year < years; ++year) {
    for (std::uint64_t tick = 0; tick < DAYS_PER_YEAR * TICKS_PER_SWEPT_DAY;
         ++tick) {
      world.tick();
      lived.peak = std::max(lived.peak, world.living());
    }
    const std::vector<std::size_t> held = peopled(world, factions).second;
    lived.every_faction_peopled =
        lived.every_faction_peopled &&
        std::none_of(held.begin(), held.end(),
                     [](std::size_t villages) { return villages == 0; });
  }
  lived.living_after = world.living();
  lived.peopled_after = peopled(world, factions).first;
  return lived;
}

// The households each faction starts with, comma-separated.
std::string households(const Region& region, std::size_t factions) {
  std::vector<std::uint32_t> count(factions, 0);
  for (const Village& village : region.villages) {
    count.at(village.faction) += village.households;
  }
  std::string listed;
  for (const std::uint32_t each : count) {
    listed += (listed.empty() ? "" : ",") + std::to_string(each);
  }
  return listed;
}

int run(std::uint64_t first, std::uint64_t last, std::uint64_t years) {
  const RegionData data = region_data();
  std::cout << "| Seed | Households | Living | Peopled villages | Peak | "
               "Every faction peopled |\n|---|---|---|---|---|---|\n";
  std::size_t seeds = 0;
  std::size_t kept = 0;
  std::size_t grew = 0;
  for (std::uint64_t seed = first; seed <= last; ++seed) {
    const Lived lived = live(Seed{seed}, years, data);
    ++seeds;
    kept += lived.every_faction_peopled ? 1 : 0;
    grew += lived.living_after >= GROWTH_FACTOR * lived.living_before ? 1 : 0;
    std::cout << "| " << seed << " | "
              << households(generate_region(Seed{seed}, data),
                            data.factions.size())
              << " | " << lived.living_before << " -> " << lived.living_after
              << " | " << lived.peopled_before << " -> " << lived.peopled_after
              << " | " << lived.peak << " | "
              << (lived.every_faction_peopled ? "yes" : "no") << " |\n"
              << std::flush;
  }
  std::cout << "\nEvery faction peopled through " << years << " years on "
            << kept << " of " << seeds << " seeds; the living at least "
            << GROWTH_FACTOR << " times as many on " << grew << ".\n";
  return 0;
}

}  // namespace
}  // namespace murim

// Arguments: the first seed, the last seed and the years to live.
int main(int argc, char** argv) {
  constexpr int ARGUMENTS = 4;
  if (argc != ARGUMENTS) {
    std::cerr << "usage: murim_core_sweep <first seed> <last seed> <years>\n";
    return 2;
  }
  try {
    const std::vector<std::string> given(argv, std::next(argv, argc));
    return murim::run(std::stoull(given.at(1)), std::stoull(given.at(2)),
                      std::stoull(given.at(3)));
  } catch (const std::exception& error) {
    std::cerr << "sweep: " << error.what() << "\n";
    return 2;
  }
}
