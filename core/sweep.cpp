// Lives the generated region of each seed in a range for a number of years,
// with days of five ticks, and prints per seed the households each faction
// starts with, the living and the peopled villages at the start and the end,
// the most living at once, whether every faction held a peopled village at
// the end of every year, the living at each realm rank and those knowing an
// art at the end, how many people ever learned from a manual lying on a
// cell, how many wars leaders declared and how many peaces they made, and
// how many smithies and apothecaries growth raised; then how many seeds
// raised at least one of each. For the first seeds it prints each decade's
// people, food, money and villages, and over every seed it measures ADR
// 0063's bands. It exits 1 when a seed's living reach the capacity, given
// as an optional fourth argument or else the region's.
// `make sweep` builds it optimised and runs it; it is not a test.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "arts.hpp"
#include "calendar.hpp"
#include "diplomacy.hpp"
#include "growth.hpp"
#include "json_packs.hpp"
#include "map.hpp"
#include "person.hpp"
#include "region.hpp"
#include "village.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr std::uint64_t TICKS_PER_SWEPT_DAY = 5;
constexpr std::size_t GROWTH_FACTOR = 2;
// The seeds, from the first, whose decades are printed (ADR 0063).
constexpr std::uint64_t DECADE_SEEDS = 50;
constexpr std::uint64_t DECADE = 10;
constexpr std::uint64_t AUTUMN = 2;
// ADR 0063's bands. Each "x of y" holds when x * y_of reaches y * x_of.
constexpr std::size_t KEPT_OF = 48;
constexpr std::size_t KEPT_SEEDS = 50;
constexpr std::size_t LIVING_FACTOR = 3;
constexpr std::size_t PEOPLED_FACTOR = 2;
constexpr std::uint64_t SETTLED_YEARS = 20;
constexpr std::size_t SETTLED_OF = 45;
constexpr std::size_t SETTLED_LOW = 4;
constexpr std::size_t SETTLED_HIGH = 5;
constexpr std::uint64_t LEAST_FOOD = 120;
constexpr std::uint64_t MOST_FOOD = 200;
constexpr std::uint64_t STARVED_SHARE = 20;
constexpr std::uint64_t MONEY_FROM = 10;
constexpr std::uint64_t MONEY_FACTOR = 2;
constexpr std::size_t PEAK_OF = 3;
constexpr std::size_t PEAK_SHARE = 5;

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

// One year of a seed: what the year counts, and the rest as at its end,
// the food as at the end of its autumn.
struct Year {
  std::size_t born = 0;
  std::size_t died = 0;
  std::size_t starved = 0;
  bool at_war = false;
  std::uint64_t harvest = 0;
  std::uint64_t stock = 0;
  std::size_t fed = 0;
  std::size_t living = 0;
  std::uint64_t silver = 0;
  std::size_t peopled = 0;
  std::size_t villages = 0;
};

struct Lived {
  std::size_t capacity;
  std::vector<Year> years;
  std::size_t villages_before;
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

// Returns whether a war stands between any two factions.
bool at_war(const World& world, std::size_t factions) {
  for (std::size_t one = 0; one < factions; ++one) {
    for (std::size_t other = one + 1; other < factions; ++other) {
      const std::optional<Declared> standing = world.declaration(one, other);
      if (standing && standing->what == Declaration::WAR) {
        return true;
      }
    }
  }
  return false;
}

// Counts in `year` the people who died since the last call, those dead of
// hunger apart, and in `alive` who lives now. Starving leaves a person's
// hunger at `hunger_days` or more, where migrating or eating resets it.
void note_deaths(const World& world, std::uint32_t hunger_days,
                 std::vector<bool>& alive, Year& year) {
  alive.resize(world.population(), false);
  for (std::size_t person = 0; person < world.population(); ++person) {
    const Person& who = world.person(person);
    if (alive.at(person) && who.health == 0) {
      ++year.died;
      year.starved += hunger_days > 0 && who.hunger >= hunger_days ? 1 : 0;
    }
    alive.at(person) = who.health > 0;
  }
}

// Sets in `year` the villages' harvest and stock, and the living they feed.
void note_food(const World& world, Year& year) {
  year.harvest = 0;
  year.stock = 0;
  for (std::size_t village = 0; village < world.village_count(); ++village) {
    year.harvest += world.harvest(village);
    year.stock += world.stock(village);
  }
  year.fed = world.living();
}

// Counts in `year` the day's deaths and, in autumn, sets its food.
void note_day(const World& world, std::uint32_t hunger_days,
              std::vector<bool>& alive, Year& year) {
  note_deaths(world, hunger_days, alive, year);
  if (world.date().season == AUTUMN) {
    note_food(world, year);
  }
}

// Sets in `year`, at its end, the people born since the world held
// `people`, and the living, their silver and the villages.
void close_year(const World& world, std::size_t factions, Year& year,
                std::size_t people) {
  year.born = world.population() - people;
  year.living = world.living();
  for (std::size_t person = 0; person < world.population(); ++person) {
    year.silver +=
        world.person(person).health > 0 ? world.person(person).silver : 0;
  }
  year.peopled = peopled(world, factions).first;
  year.villages = world.village_count();
}

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

Lived live(Seed seed, std::uint64_t years, std::optional<std::size_t> capacity,
           const RegionData& data) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = TICKS_PER_SWEPT_DAY;
  region.rules.people_capacity =
      capacity.value_or(region.rules.people_capacity);
  World world{region.seed,       Map{region.map_rows}, region.armies,
              region.structures, region.rules,         village_specs(region),
              region.leadership, region.life,          region.sects};
  const std::size_t factions = data.factions.size();
  Lived lived{region.rules.people_capacity,
              {},
              world.village_count(),
              world.living(),
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
  std::vector<bool> alive;
  Year before;
  note_deaths(world, 0, alive, before);
  const std::uint32_t hunger_days =
      data.life.husbandry ? data.life.husbandry->hunger_days : 0;
  for (std::uint64_t year = 0; year < years; ++year) {
    Year& now = lived.years.emplace_back();
    const std::size_t people = world.population();
    for (std::uint64_t tick = 0; tick < DAYS_PER_YEAR * TICKS_PER_SWEPT_DAY;
         ++tick) {
      world.tick();
      lived.peak = std::max(lived.peak, world.living());
      note_readers(world, data.life.learning.reading_reach, known, read);
      note_declarations(world, factions, seen, lived);
      now.at_war = now.at_war || at_war(world, factions);
      if ((tick + 1) % TICKS_PER_SWEPT_DAY == 0) {
        note_day(world, hunger_days, alive, now);
      }
    }
    close_year(world, factions, now, people);
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

// The rows of seed `seed`'s decades: the people born, dead and dead of
// hunger over each; the living, silver a head and the peopled villages at
// its end; its last autumn's harvest, wild food and stock a head; and the
// villages founded over it. Wild food is 0 until ADR 0056's slots exist.
std::string decades(std::uint64_t seed, const Lived& lived) {
  std::ostringstream rows;
  std::size_t born = 0;
  std::size_t died = 0;
  std::size_t starved = 0;
  std::size_t villages = lived.villages_before;
  for (std::size_t index = 0; index < lived.years.size(); ++index) {
    const Year& year = lived.years.at(index);
    born += year.born;
    died += year.died;
    starved += year.starved;
    if ((index + 1) % DECADE != 0 && index + 1 != lived.years.size()) {
      continue;
    }
    const std::uint64_t fed = std::max<std::size_t>(year.fed, 1);
    rows << "| " << seed << " | " << index + 1 << " | " << year.living << " | "
         << born << " | " << died << " | " << starved << " | "
         << year.harvest / fed << " | 0 | " << year.stock / fed << " | "
         << year.silver / std::max<std::size_t>(year.living, 1) << " | "
         << year.peopled << " | " << year.villages - villages << " |\n";
    born = 0;
    died = 0;
    starved = 0;
    villages = year.villages;
  }
  return rows.str();
}

// "met" when `holds`, else "missed".
const char* met(bool holds) { return holds ? "met" : "missed"; }

// What ADR 0063's bands read, summed over the seeds swept.
struct Totals {
  std::size_t kept = 0;
  std::size_t living_before = 0;
  std::size_t living_after = 0;
  std::size_t peopled_before = 0;
  std::size_t peopled_after = 0;
  std::size_t settled = 0;
  std::uint64_t harvest = 0;
  std::size_t fed = 0;
  std::size_t peace_died = 0;
  std::size_t peace_starved = 0;
  std::vector<std::uint64_t> silver;
  std::size_t peak = 0;
  std::size_t capacity = 0;
};

// Returns whether a seed's living at the end lie within the settled share
// of its living `SETTLED_YEARS` before; false with fewer years.
bool stayed_settled(const Lived& lived) {
  const std::size_t years = lived.years.size();
  if (years < SETTLED_YEARS) {
    return false;
  }
  const std::size_t then =
      years > SETTLED_YEARS ? lived.years.at(years - SETTLED_YEARS - 1).living
                            : lived.living_before;
  return SETTLED_HIGH * lived.living_after >= SETTLED_LOW * then &&
         SETTLED_LOW * lived.living_after <= SETTLED_HIGH * then;
}

// Sums what the bands read over every seed in `swept`, each lived `years`
// years; the peak is the seed's nearest its capacity.
Totals sum(const std::vector<Lived>& swept, std::uint64_t years) {
  Totals totals;
  totals.silver.assign(years, 0);
  for (const Lived& lived : swept) {
    totals.kept += lived.every_faction_peopled ? 1 : 0;
    totals.living_before += lived.living_before;
    totals.living_after += lived.living_after;
    totals.peopled_before += lived.peopled_before;
    totals.peopled_after += lived.peopled_after;
    totals.settled += stayed_settled(lived) ? 1U : 0U;
    totals.harvest += lived.years.back().harvest;
    totals.fed += lived.years.back().fed;
    for (std::size_t year = 0; year < years; ++year) {
      const Year& each = lived.years.at(year);
      totals.peace_died += each.at_war ? 0 : each.died;
      totals.peace_starved += each.at_war ? 0 : each.starved;
      totals.silver.at(year) += each.silver;
    }
    if (lived.peak * totals.capacity >= totals.peak * lived.capacity) {
      totals.peak = lived.peak;
      totals.capacity = lived.capacity;
    }
  }
  return totals;
}

// Prints ADR 0063's bands over every seed in `swept`, each with what the
// seeds measured and whether it is met.
void print_bands(const std::vector<Lived>& swept, std::uint64_t years) {
  const std::size_t seeds = swept.size();
  const Totals totals = sum(swept, years);
  const auto& [kept, living_before, living_after, peopled_before, peopled_after,
               settled, harvest, fed, peace_died, peace_starved, silver, peak,
               capacity] = totals;
  const std::uint64_t food = harvest / std::max<std::size_t>(fed, 1);
  std::cout << "\n| Band | Measured | Result |\n|---|---|---|\n"
            << "| Every faction peopled at every year's end on at least "
            << KEPT_OF << " of " << KEPT_SEEDS << " seeds | " << kept << " of "
            << seeds << " | " << met(kept * KEPT_SEEDS >= seeds * KEPT_OF)
            << " |\n| Living at the end at least " << LIVING_FACTOR
            << " times the start | " << living_before << " -> " << living_after
            << " | " << met(living_after >= LIVING_FACTOR * living_before)
            << " |\n| Peopled villages at the end at least " << PEOPLED_FACTOR
            << " times the start | " << peopled_before << " -> "
            << peopled_after << " | "
            << met(peopled_after >= PEOPLED_FACTOR * peopled_before)
            << " |\n| Living at the end within " << SETTLED_LOW << "/"
            << SETTLED_HIGH << " and " << SETTLED_HIGH << "/" << SETTLED_LOW
            << " of " << SETTLED_YEARS << " years before, on at least "
            << SETTLED_OF << " of " << KEPT_SEEDS << " seeds | ";
  if (years >= SETTLED_YEARS) {
    std::cout << settled << " of " << seeds << " | "
              << met(settled * KEPT_SEEDS >= seeds * SETTLED_OF);
  } else {
    std::cout << "fewer years | not measurable";
  }
  std::cout << " |\n| Food a living person has in the last year, harvest and "
               "wild, "
            << LEAST_FOOD << " to " << MOST_FOOD << " | " << food << " | "
            << met(food >= LEAST_FOOD && food <= MOST_FOOD)
            << " |\n| Deaths by hunger in years without a war at most 1/"
            << STARVED_SHARE << " of deaths | " << peace_starved << " of "
            << peace_died << " | "
            << met(peace_starved * STARVED_SHARE <= peace_died)
            << " |\n| Hungry households in years without a war | no households "
               "before ADR 0052 | not measurable |\n| Money supply from year "
            << MONEY_FROM << " within half and double its level then | ";
  if (years >= MONEY_FROM) {
    const std::uint64_t base = silver.at(MONEY_FROM - 1);
    const auto [low, high] =
        std::minmax_element(silver.begin() + MONEY_FROM - 1, silver.end());
    std::cout << *low << " to " << *high << " against " << base << " | "
              << met(*low * MONEY_FACTOR >= base &&
                     *high <= MONEY_FACTOR * base);
  } else {
    std::cout << "fewer years | not measurable";
  }
  std::cout << " |\n| Peak living at most " << PEAK_OF << "/" << PEAK_SHARE
            << " of capacity | " << peak << " of " << capacity << " | "
            << met(peak * PEAK_SHARE <= PEAK_OF * capacity)
            << " |\n| Births refused at capacity, none | counted from ADR "
               "0063's step 2; the living reaching it stand in below | not "
               "measurable |\n";
}

// The households each faction starts with, comma-separated.
std::string households(const Region& region, std::size_t factions) {
  std::vector<std::uint32_t> count(factions, 0);
  for (const Village& village : region.villages) {
    count.at(village.faction) += village.households;
  }
  return listed(count);
}

int run(std::uint64_t first, std::uint64_t last, std::uint64_t years,
        std::optional<std::size_t> capacity) {
  if (years == 0) {
    std::cerr << "sweep: the years to live must be at least 1\n";
    return 2;
  }
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
  std::vector<Lived> swept;
  std::string decade_rows;
  std::vector<std::uint64_t> reached;
  for (std::uint64_t seed = first; seed <= last; ++seed) {
    const Lived& lived =
        swept.emplace_back(live(Seed{seed}, years, capacity, data));
    decade_rows += seed < DECADE_SEEDS ? decades(seed, lived) : "";
    if (lived.peak >= lived.capacity) {
      reached.push_back(seed);
    }
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
  if (!decade_rows.empty()) {
    std::cout << "\n| Seed | Year | Living | Born | Died | Died of hunger | "
                 "Harvest a head | Wild food a head | Stock a head after "
                 "autumn | Silver a head | Peopled villages | Villages "
                 "founded |\n|---|---|---|---|---|---|---|---|---|---|---|---"
                 "|\n"
              << decade_rows;
  }
  print_bands(swept, years);
  if (reached.empty()) {
    std::cout << "\nNo seed's living reached its capacity.\n";
    return 0;
  }
  std::cout << "\nSeeds whose living reached their capacity: "
            << listed(reached) << ".\n";
  return 1;
}

}  // namespace
}  // namespace murim

// Arguments: the first seed, the last seed, the years to live and,
// optionally, the capacity for the living.
int main(int argc, char** argv) {
  constexpr int ARGUMENTS = 4;
  constexpr int WITH_CAPACITY = 5;
  if (argc != ARGUMENTS && argc != WITH_CAPACITY) {
    std::cerr << "usage: murim_core_sweep <first seed> <last seed> <years> "
                 "[<capacity>]\n";
    return 2;
  }
  try {
    const std::vector<std::string> given(argv, std::next(argv, argc));
    const std::optional<std::size_t> capacity =
        argc == WITH_CAPACITY
            ? std::optional<std::size_t>{std::stoull(given.at(ARGUMENTS))}
            : std::nullopt;
    return murim::run(std::stoull(given.at(1)), std::stoull(given.at(2)),
                      std::stoull(given.at(3)), capacity);
  } catch (const std::exception& error) {
    std::cerr << "sweep: " << error.what() << "\n";
    return 2;
  }
}
