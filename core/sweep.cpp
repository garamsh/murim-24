// Lives the generated region of each seed in a range for a number of years,
// with days of five ticks, and prints per seed the households each faction
// starts with, the living and the peopled villages at the start and the end,
// the most living at once, whether every faction held a peopled village at
// the end of every year, the living at each realm rank and those knowing an
// art at the end, how many people ever learned from a manual lying on a
// cell, how many wars leaders declared and how many peaces they made, and
// how many smithies and apothecaries growth raised; then how many seeds
// raised at least one of each.
// `make sweep` builds it optimised and runs it; it is not a test.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "json_packs.hpp"
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
  std::vector<std::size_t> ranks;
  std::size_t artful;
  std::size_t readers;
  std::size_t wars;
  std::size_t peaces;
  std::size_t smithies;
  std::size_t apothecaries;
};

// Marks, among `raised`, every structure growth laid that now stands intact
// and arms or tends people: a smithy or an apothecary.
void note_workshops(const World& world, const Growth& growth,
                    std::size_t laid_from, std::vector<bool>& raised) {
  raised.resize(world.structure_count(), false);
  for (std::size_t structure = laid_from; structure < world.structure_count();
       ++structure) {
    const std::optional<std::size_t> plan = world.plan_of(structure);
    if (plan && *plan < growth.plans.size() &&
        world.stage(structure) == Stage::INTACT) {
      const Holding& holding = growth.plans.at(*plan).holding;
      raised.at(structure) =
          raised.at(structure) || holding.arms > 0 || holding.tends > 0;
    }
  }
}

// Counts in `lived` each declaration made since `seen`, the tick of the
// declaration last seen between each two factions, and brings `seen` up to
// date.
void note_declarations(const World& world, std::size_t factions,
                       std::vector<std::uint64_t>& seen, Lived& lived) {
  seen.resize(factions * factions, 0);
  for (std::size_t one = 0; one < factions; ++one) {
    for (std::size_t other = one + 1; other < factions; ++other) {
      const std::optional<Declared> standing = world.declaration(one, other);
      std::uint64_t& last = seen.at((one * factions) + other);
      if (standing && standing->tick != last) {
        last = standing->tick;
        ++(standing->what == Declaration::WAR ? lived.wars : lived.peaces);
      }
    }
  }
}

// Marks, among `read`, everyone who learned a skill on this tick while
// within the reading reach of a manual lying on a cell; `known` holds how
// many skills each person knew before the tick, and is brought up to date.
void note_readers(const World& world, std::int64_t reach,
                  std::vector<std::size_t>& known, std::vector<bool>& read) {
  known.resize(world.population(), 0);
  read.resize(world.population(), false);
  for (std::size_t person = 0; person < world.population(); ++person) {
    const Person& who = world.person(person);
    if (who.known.size() > known.at(person)) {
      for (const Manual& manual : world.manuals()) {
        const auto* cell = std::get_if<Cell>(&manual.place);
        if (cell == nullptr) {
          continue;
        }
        const Position centre = centre_of(*cell);
        const std::int64_t across = std::int64_t{who.position.x} - centre.x;
        const std::int64_t down = std::int64_t{who.position.y} - centre.y;
        read.at(person) = read.at(person) ||
                          (across * across) + (down * down) <= reach * reach;
      }
    }
    known.at(person) = who.known.size();
  }
}

Lived live(Seed seed, std::uint64_t years, const RegionData& data) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = TICKS_PER_SWEPT_DAY;
  World world{region.seed,       Map{region.map_rows}, region.armies,
              region.structures, region.rules,         village_specs(region),
              region.leadership, region.life,          region.sects};
  const std::size_t factions = data.factions.size();
  Lived lived{world.living(),
              0,
              peopled(world, factions).first,
              0,
              world.living(),
              true,
              std::vector<std::size_t>(data.life.realms.size() + 1, 0),
              0,
              0,
              0,
              0,
              0,
              0};
  const Growth growth = data.life.growth.value_or(Growth{});
  std::vector<bool> workshops;
  std::vector<std::size_t> known;
  std::vector<std::uint64_t> seen;
  std::vector<bool> read;
  for (std::uint64_t year = 0; year < years; ++year) {
    for (std::uint64_t tick = 0; tick < DAYS_PER_YEAR * TICKS_PER_SWEPT_DAY;
         ++tick) {
      world.tick();
      lived.peak = std::max(lived.peak, world.living());
      note_readers(world, data.life.learning.reading_reach, known, read);
      note_declarations(world, factions, seen, lived);
    }
    note_workshops(world, growth, region.structures.size(), workshops);
    const std::vector<std::size_t> held = peopled(world, factions).second;
    lived.every_faction_peopled =
        lived.every_faction_peopled &&
        std::none_of(held.begin(), held.end(),
                     [](std::size_t villages) { return villages == 0; });
  }
  lived.living_after = world.living();
  lived.peopled_after = peopled(world, factions).first;
  for (std::size_t person = 0; person < world.population(); ++person) {
    if (world.person(person).health > 0) {
      ++lived.ranks.at(world.rank(person));
      lived.artful += world.person(person).known.empty() ? 0U : 1U;
    }
  }
  lived.readers =
      static_cast<std::size_t>(std::count(read.begin(), read.end(), true));
  for (std::size_t structure = 0; structure < workshops.size(); ++structure) {
    if (workshops.at(structure)) {
      const Holding& holding =
          growth.plans.at(world.plan_of(structure).value_or(0)).holding;
      lived.smithies += holding.arms > 0 ? 1 : 0;
      lived.apothecaries += holding.tends > 0 ? 1 : 0;
    }
  }
  return lived;
}

// `counts`, comma-separated.
template <typename Count>
std::string listed(const std::vector<Count>& counts) {
  std::string joined;
  for (const Count each : counts) {
    joined += (joined.empty() ? "" : ",") + std::to_string(each);
  }
  return joined;
}

// The households each faction starts with, comma-separated.
std::string households(const Region& region, std::size_t factions) {
  std::vector<std::uint32_t> count(factions, 0);
  for (const Village& village : region.villages) {
    count.at(village.faction) += village.households;
  }
  return listed(count);
}

int run(std::uint64_t first, std::uint64_t last, std::uint64_t years) {
  const RegionData data = murim_packs::load_base(MURIM_PACKS_DIR);
  std::cout
      << "| Seed | Households | Living | Peopled villages | Peak | "
         "Every faction peopled | Living by rank | Knowing an art | Manual "
         "readers | Wars declared | Peaces made | Smithies raised | "
         "Apothecaries raised "
         "|\n|---|---|---|---|---|---|---|---|---|---|---|---"
         "|---|\n";
  std::size_t seeds = 0;
  std::size_t kept = 0;
  std::size_t grew = 0;
  std::vector<std::size_t> ranks(data.life.realms.size() + 1, 0);
  std::size_t readers = 0;
  std::size_t wars = 0;
  std::size_t peaces = 0;
  std::size_t smithies = 0;
  std::size_t apothecaries = 0;
  std::size_t smithy_seeds = 0;
  std::size_t apothecary_seeds = 0;
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
              << (lived.every_faction_peopled ? "yes" : "no") << " | "
              << listed(lived.ranks) << " | " << lived.artful << " | "
              << lived.readers << " | " << lived.wars << " | " << lived.peaces
              << " | " << lived.smithies << " | " << lived.apothecaries
              << " |\n"
              << std::flush;
    for (std::size_t rank = 0; rank < ranks.size(); ++rank) {
      ranks.at(rank) += lived.ranks.at(rank);
    }
    readers += lived.readers;
    wars += lived.wars;
    peaces += lived.peaces;
    smithies += lived.smithies;
    apothecaries += lived.apothecaries;
    smithy_seeds += lived.smithies > 0 ? 1 : 0;
    apothecary_seeds += lived.apothecaries > 0 ? 1 : 0;
  }
  std::cout << "\nEvery faction peopled through " << years << " years on "
            << kept << " of " << seeds << " seeds; the living at least "
            << GROWTH_FACTOR << " times as many on " << grew
            << ".\nThe living at the end by realm rank, from none up, over "
               "every seed: "
            << listed(ranks)
            << "; people who learned from a manual on a cell: " << readers
            << ".\nWars declared: " << wars << ", peaces made: " << peaces
            << ", over " << seeds << " seeds of " << years << " years."
            << "\nSeeds raising a smithy: " << smithy_seeds << " of " << seeds
            << " (" << smithies
            << " in all); an apothecary: " << apothecary_seeds << " of "
            << seeds << " (" << apothecaries << " in all).\n";
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
