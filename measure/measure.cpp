// Measurements for issue #173 phase 1. Not part of the repository's build:
// it links a copy of core/ at origin/main 1d943f0 with MAX_PEOPLE and
// REGION_SIDE made settable (MEASURE_MAX_PEOPLE, MEASURE_REGION_SIDE).
//
//   measure battle <people> <side>   the M1 battle scaled to <people>, on a
//                                    <side>-cell open map
//   measure field <side>             flow-field build time and bytes
//   measure region <people> <seed>   the generated region at REGION_SIDE,
//                                    its villages' residents raised to
//                                    <people>; tick times, daily tick, save
//                                    bytes, peak memory

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "region.hpp"
#include "save.hpp"
#include "world.hpp"

namespace murim {
namespace {

using Clock = std::chrono::steady_clock;

double ms(Clock::duration d) {
  return std::chrono::duration<double, std::milli>(d).count();
}

struct Times {
  double p50, p99, max;
};

Times summarise(std::vector<Clock::duration> d) {
  std::sort(d.begin(), d.end());
  return Times{ms(d.at(d.size() / 2)),
               ms(d.at(((d.size() * 99 + 99) / 100) - 1)), ms(d.back())};
}

long peak_mb() {
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss / (1024 * 1024);  // bytes on macOS
}

class Memory : public SaveStore {
 public:
  bool put(Slot, const std::vector<std::uint8_t>& bytes) override {
    bytes_ = bytes;
    return true;
  }
  std::optional<std::vector<std::uint8_t>> get(Slot) const override {
    return bytes_;
  }
  std::size_t size() const { return bytes_.size(); }

 private:
  std::vector<std::uint8_t> bytes_;
};

// The M1 battle (core/m1_scenario.cpp) with columns of ten 5x5 armies a
// side, one column per 500 people, each column one block further back.
int battle(std::size_t people, std::int32_t side) {
  constexpr std::int32_t BLOCK = 5;
  constexpr std::int32_t LINE = 10;
  const Cell centre{side / 2, side / 2};
  const std::int32_t first_row = centre.y - (LINE * BLOCK / 2);
  const Skill jab{3 * CELL_SIZE, 0, 10, 0, 15};
  const Skill sweep{3 * CELL_SIZE, (3 * CELL_SIZE) / 2, 10, 1, 30};
  std::vector<ArmySpec> armies;
  std::vector<Structure> structures;
  const auto columns = static_cast<std::int32_t>(people / 500);
  for (std::int32_t column = 0; column < columns; ++column) {
    for (std::int32_t line = 0; line < LINE; ++line) {
      const std::int32_t top = first_row + line * BLOCK;
      const std::int32_t middle = top + BLOCK / 2;
      const bool caster = line == LINE / 2;
      const std::vector<Skill> skills =
          caster ? std::vector<Skill>{sweep} : std::vector<Skill>{jab};
      for (const int sign : {-1, 1}) {
        std::vector<Recruit> members;
        const Cell corner{sign < 0 ? centre.x - 1 - BLOCK - column * BLOCK
                                   : centre.x + 1 + column * BLOCK,
                          top};
        for (const Cell cell : cells_of(corner, Footprint{BLOCK, BLOCK})) {
          members.push_back(Recruit{cell, 3'000});
        }
        armies.push_back(ArmySpec{{centre.x + sign, middle},
                                  members,
                                  sign < 0 ? 0U : 1U,
                                  skills});
      }
      if (column == 0) {
        structures.push_back(Structure{Cell{centre.x, middle}, 3'000});
      }
    }
  }
  const Rules rules{CELL_SIZE,
                    {{false, true}, {true, false}},
                    {3'000, 1'500, 1}};
  const auto built = Clock::now();
  World world{Seed{2026},
              Map{std::vector<std::string>(static_cast<std::size_t>(side),
                                           std::string(side, '.'))},
              armies, structures, rules};
  const double create = ms(Clock::now() - built);
  for (int tick = 0; tick < 60; ++tick) world.tick();
  std::vector<Clock::duration> d;
  for (int tick = 0; tick < 600; ++tick) {
    const auto start = Clock::now();
    world.tick();
    d.push_back(Clock::now() - start);
  }
  const Times t = summarise(d);
  std::cout << std::fixed << std::setprecision(3) << "battle people "
            << people << " side " << side << " armies " << armies.size()
            << " living_end " << world.living() << " create_ms " << create
            << " p50_ms " << t.p50 << " p99_ms " << t.p99 << " max_ms "
            << t.max << " p50_us_per_person "
            << t.p50 * 1000.0 / static_cast<double>(people) << " peak_mb "
            << peak_mb() << "\n";
  return 0;
}

int field(std::int32_t side) {
  const Map open{std::vector<std::string>(static_cast<std::size_t>(side),
                                          std::string(side, '.'))};
  std::vector<Clock::duration> d;
  for (int i = 0; i < 20; ++i) {
    const auto start = Clock::now();
    const FlowField f{open, Cell{side / 2, side / 2}};
    d.push_back(Clock::now() - start);
    if (f.distance(Cell{0, 0}) == 0) return 1;
  }
  const Times t = summarise(d);
  const double cells = static_cast<double>(side) * side;
  std::cout << std::fixed << std::setprecision(3) << "field side " << side
            << " build_p50_ms " << t.p50 << " build_max_ms " << t.max
            << " bytes " << cells * (sizeof(Step) + sizeof(std::uint32_t))
            << "\n";
  return 0;
}

// Adds residents to every village of `region`, on free passable cells
// nearest its centre, until the region holds `people`.
void inflate(Region& region, std::size_t people) {
  const auto side = static_cast<std::int32_t>(region.map_rows.size());
  std::vector<bool> taken(static_cast<std::size_t>(side) * side, false);
  auto at = [&](Cell c) {
    return static_cast<std::size_t>(c.y) * side + static_cast<std::size_t>(c.x);
  };
  const Map map{region.map_rows};
  for (const Structure& s : region.structures) {
    if (!s.blocks) continue;
    for (const Cell c : cells_of(s.cell, s.footprint)) taken.at(at(c)) = true;
  }
  std::size_t have = 0;
  for (const ArmySpec& a : region.armies) {
    have += a.members.size();
    for (const Recruit& r : a.members) taken.at(at(r.cell)) = true;
  }
  // Appended armies, one per village, homed there, so the people the region
  // already gives keep their indices (sect founders name them).
  const std::size_t first_extra = region.armies.size();
  for (const Village& v : region.villages) {
    const ArmySpec& residents = region.armies.at(v.army);
    const std::size_t index =
        static_cast<std::size_t>(&v - region.villages.data());
    region.armies.push_back(ArmySpec{
        {v.corner.x + v.area.width / 2, v.corner.y + v.area.height / 2},
        {},
        residents.faction,
        residents.skills,
        index});
  }
  std::size_t village = 0;
  while (have < people) {
    const Village& v = region.villages.at(village % region.villages.size());
    ArmySpec& army =
        region.armies.at(first_extra + village % region.villages.size());
    const ArmySpec& residents = region.armies.at(v.army);
    const Cell centre{v.corner.x + v.area.width / 2,
                      v.corner.y + v.area.height / 2};
    bool placed = false;
    for (std::int32_t r = 0; r < side && !placed; ++r) {
      for (std::int32_t dy = -r; dy <= r && !placed; ++dy) {
        for (std::int32_t dx = -r; dx <= r && !placed; ++dx) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const Cell c{centre.x + dx, centre.y + dy};
          if (c.x < 0 || c.y < 0 || c.x >= side || c.y >= side) continue;
          if (!map.passable(c) || taken.at(at(c))) continue;
          taken.at(at(c)) = true;
          army.members.push_back(Recruit{c, residents.members.front().health});
          placed = true;
        }
      }
    }
    ++have;
    ++village;
  }
}

World world_of(const Region& region) {
  return World{region.seed,       Map{region.map_rows}, region.armies,
               region.structures, region.rules,         village_specs(region),
               region.leadership, region.life,          region.sects};
}

int region_run(std::size_t people, std::uint64_t seed) {
  const auto generated = Clock::now();
  Region region = generate_region(Seed{seed}, region_data());
  const double generate = ms(Clock::now() - generated);
  const std::size_t start_people = [&] {
    std::size_t n = 0;
    for (const ArmySpec& a : region.armies) n += a.members.size();
    return n;
  }();
  inflate(region, people);
  const auto built = Clock::now();
  World world = world_of(region);
  const double create = ms(Clock::now() - built);
  for (int tick = 0; tick < 600; ++tick) world.tick();
  std::vector<Clock::duration> d;
  for (int tick = 0; tick < 900; ++tick) {
    const auto start = Clock::now();
    world.tick();
    d.push_back(Clock::now() - start);
  }
  const Times t = summarise(d);
  std::size_t runs = 0;
  for (const std::string& row : region.map_rows) {
    for (std::size_t x = 0; x < row.size(); ++x) {
      runs += static_cast<std::size_t>(x == 0 || row[x] != row[x - 1]);
    }
  }
  std::cout << "map_cells " << region.map_rows.size() * region.map_rows.size()
            << " row_runs " << runs << "\n";
  Memory store;
  const auto saved = Clock::now();
  save(Saved{Origin{Seed{seed}, 0}, world}, store, Slot{0});
  const double save_ms = ms(Clock::now() - saved);

  // The daily rules' cost: the same world with 60-tick days, the day's
  // ticks timed apart from the rest.
  Region daily = generate_region(Seed{seed}, region_data());
  inflate(daily, people);
  daily.rules.ticks_per_day = 60;
  World w2 = world_of(daily);
  std::vector<Clock::duration> day_ticks;
  std::vector<Clock::duration> other;
  for (int tick = 0; tick < 600; ++tick) {
    const bool day = w2.tick_count() % 60 == 0;
    const auto start = Clock::now();
    w2.tick();
    (day ? day_ticks : other).push_back(Clock::now() - start);
    if (day) std::cout << "day " << tick / 60 << " ms " << ms(day_ticks.back()) << "\n";
  }
  const Times dt = summarise(day_ticks);
  const Times ot = summarise(other);
  std::cout << std::fixed << std::setprecision(3) << "region side "
            << REGION_SIDE << " seed " << seed << " villages "
            << region.villages.size() << " armies " << region.armies.size()
            << " people_start " << start_people << " people " << people
            << " living_end " << world.living() << " generate_ms " << generate
            << " create_ms " << create << " p50_ms " << t.p50 << " p99_ms "
            << t.p99 << " max_ms " << t.max << " p50_us_per_person "
            << t.p50 * 1000.0 / static_cast<double>(people)
            << " daytick_p50_ms " << dt.p50 << " daytick_max_ms " << dt.max
            << " otherday_p50_ms " << ot.p50 << " save_bytes " << store.size()
            << " save_ms " << save_ms << " peak_mb " << peak_mb() << "\n";
  return 0;
}

}  // namespace
}  // namespace murim

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "battle" && argc == 4) {
    return murim::battle(std::stoul(argv[2]), std::stoi(argv[3]));
  }
  if (mode == "field" && argc == 3) return murim::field(std::stoi(argv[2]));
  if (mode == "region" && argc == 4) {
    return murim::region_run(std::stoul(argv[2]), std::stoull(argv[3]));
  }
  std::cerr << "usage: measure battle <people> <side> | field <side> | "
               "region <people> <seed>\n";
  return 2;
}
