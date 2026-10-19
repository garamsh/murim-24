#include "region.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "areas.hpp"
#include "calendar.hpp"
#include "chart.hpp"
#include "compiled_map.hpp"
#include "flow_field.hpp"
#include "gear.hpp"
#include "growth.hpp"
#include "json_packs.hpp"
#include "kinship.hpp"
#include "layout.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "names.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "sect.hpp"
#include "skill.hpp"
#include "talent.hpp"
#include "village.hpp"
#include "world.hpp"

namespace murim {
namespace {

// The base pack's tables, read once from the packs directory.
const RegionData& base_data() {
  static const RegionData data = murim_packs::load_base(MURIM_PACKS_DIR);
  return data;
}

// Clears `data`'s sects, and the manuals and loadouts that name them.
void clear_sects(RegionData& data) {
  data.sects.clear();
  data.life.manuals.reset();
  if (data.life.gear) {
    std::vector<Loadout>& loadouts = data.life.gear->loadouts;
    loadouts.erase(std::remove_if(loadouts.begin(), loadouts.end(),
                                  [](const Loadout& loadout) {
                                    return loadout.sect.has_value();
                                  }),
                   loadouts.end());
  }
}

constexpr std::uint64_t SEEDS = 200;
constexpr int TICKS = 300;
constexpr Seed SEED{24};
constexpr Seed OTHER_SEED{25};
// A region whose every village has another within the marrying
// neighbourhood, and a Demonic village within raiding reach of a hostile one.
constexpr Seed NEIGHBOURLY{2026};
constexpr std::size_t MIN_VILLAGES = 6;
constexpr std::size_t MAX_VILLAGES = 8;

std::size_t index_of(Cell cell) {
  return (static_cast<std::size_t>(cell.y) *
          static_cast<std::size_t>(REGION_SIDE)) +
         static_cast<std::size_t>(cell.x);
}

std::vector<Cell> area_of(const Village& village) {
  return cells_of(village.corner, village.area);
}

bool inside(const Village& village, Cell cell) {
  return cell.x >= village.corner.x && cell.y >= village.corner.y &&
         cell.x < village.corner.x + village.area.width &&
         cell.y < village.corner.y + village.area.height;
}

bool same(Cell left, Cell right) {
  return left.x == right.x && left.y == right.y;
}

// Every field of a region, as integers, so two regions compare equal exactly
// when every field does.
std::vector<std::int64_t> flatten(const Region& region) {
  std::vector<std::int64_t> values{
      static_cast<std::int64_t>(static_cast<std::uint64_t>(region.seed))};
  const auto add = [&values](auto value) {
    values.push_back(static_cast<std::int64_t>(value));
  };
  const auto add_text = [&add](const std::string& text) {
    add(text.size());
    for (const char letter : text) {
      add(letter);
    }
  };
  for (const std::string& row : region.map_rows) {
    add_text(row);
  }
  for (const ArmySpec& army : region.armies) {
    add(army.objective.x);
    add(army.objective.y);
    add(army.members.size());
    for (const Recruit& member : army.members) {
      add(member.cell.x);
      add(member.cell.y);
      add(member.health);
    }
    add(army.faction);
    for (const Skill& skill : army.skills) {
      add(skill.range);
      add(skill.radius);
      add(skill.body_damage);
      add(skill.structure_damage);
      add(skill.cooldown);
    }
  }
  for (const Structure& structure : region.structures) {
    add(structure.cell.x);
    add(structure.cell.y);
    add(structure.durability);
    add(structure.footprint.width);
    add(structure.footprint.height);
    add(structure.role);
  }
  add(region.rules.separation);
  for (const std::vector<bool>& row : region.rules.hostile) {
    for (const bool hostile : row) {
      add(hostile);
    }
  }
  for (const std::uint32_t floor : region.rules.stage_floors) {
    add(floor);
  }
  for (const std::size_t kind : region.terrain) {
    add(kind);
  }
  for (const std::size_t faction : region.territory) {
    add(faction);
  }
  for (const std::string& kind : region.structure_kinds) {
    add_text(kind);
  }
  for (const Village& village : region.villages) {
    add_text(village.name);
    add_text(village.kind);
    add(village.faction);
    add(village.corner.x);
    add(village.corner.y);
    add(village.area.width);
    add(village.area.height);
    for (const std::size_t structure : village.structures) {
      add(structure);
    }
    add(village.army);
  }
  return values;
}

World world_of(const Region& region) {
  return World{region.seed,       Map{region.map_rows}, region.armies,
               region.structures, region.rules,         village_specs(region),
               region.leadership, region.life,          region.sects};
}

// Whether the region is a world's valid input, footprints and armies
// included.
bool builds_a_world(const Region& region) {
  try {
    static_cast<void>(world_of(region));
    return true;
  } catch (const std::invalid_argument&) {
    return false;
  }
}

StateHash hash_after_ticks(const Region& region) {
  World world = world_of(region);
  for (int tick = 0; tick < TICKS; ++tick) {
    world.tick();
  }
  return world.hash();
}

// Whether every cell of each village's area, its square or a seat's, is
// buildable, every cell of its structures lies on buildable cells, or on rough
// ones for an edge, within the last tier's vicinity of its centre, and each
// of its residents stands inside its area on a cell no structure covers.
bool villages_on_buildable_cells(const Region& region) {
  const Map map{region.map_rows};
  const std::int32_t vicinity =
      region.life.growth.value_or(Growth{}).tiers.back().vicinity;
  for (const Village& village : region.villages) {
    const ArmySpec& army = region.armies.at(village.army);
    const std::vector<Recruit>& residents = army.members;
    bool fits = true;
    for (const Cell cell : area_of(village)) {
      fits = fits && map.buildable(cell);
    }
    for (const Recruit& resident : residents) {
      fits =
          fits && inside(village, resident.cell) && map.passable(resident.cell);
    }
    for (const std::size_t index : village.structures) {
      const Structure& structure = region.structures.at(index);
      for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
        const bool footed =
            map.buildable(cell) ||
            (structure.role == Role::EDGE && map.passable(cell));
        fits = fits && footed &&
               std::abs(cell.x - army.objective.x) <= vicinity &&
               std::abs(cell.y - army.objective.y) <= vicinity &&
               std::none_of(residents.begin(), residents.end(),
                            [cell](const Recruit& resident) {
                              return same(cell, resident.cell);
                            });
      }
    }
    if (!fits) {
      return false;
    }
  }
  return true;
}

// Whether every cell of each village's square belongs to its owner, and its
// resident army is of its owner.
bool villages_in_owner_territory(const Region& region) {
  for (const Village& village : region.villages) {
    for (const Cell cell : area_of(village)) {
      if (region.territory.at(index_of(cell)) != village.faction) {
        return false;
      }
    }
    if (region.armies.at(village.army).faction != village.faction) {
      return false;
    }
  }
  return true;
}

// Whether no cell lies in two villages' areas or structures.
bool villages_disjoint(const Region& region) {
  std::vector<bool> taken(region.territory.size(), false);
  for (const Village& village : region.villages) {
    std::vector<Cell> cells = area_of(village);
    for (const std::size_t index : village.structures) {
      const Structure& structure = region.structures.at(index);
      for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
        if (!inside(village, cell)) {
          cells.push_back(cell);
        }
      }
    }
    for (const Cell cell : cells) {
      if (taken.at(index_of(cell))) {
        return false;
      }
      taken.at(index_of(cell)) = true;
    }
  }
  return true;
}

std::set<std::size_t> owners(const Region& region) {
  std::set<std::size_t> factions;
  for (const Village& village : region.villages) {
    factions.insert(village.faction);
  }
  return factions;
}

std::set<std::size_t> territories(const Region& region) {
  return {region.territory.begin(), region.territory.end()};
}

std::set<std::size_t> all_of_factions(std::size_t factions) {
  std::set<std::size_t> all;
  for (std::size_t faction = 0; faction < factions; ++faction) {
    all.insert(faction);
  }
  return all;
}

// Whether the region has `factions` factions, each holding territory and a
// village, and six to eight villages, each with its own resident army.
bool populated(const Region& region, std::size_t factions) {
  const std::size_t villages = region.villages.size() - region.sects.size();
  return region.rules.hostile.size() == factions &&
         territories(region) == all_of_factions(factions) &&
         owners(region) == all_of_factions(factions) &&
         villages >= MIN_VILLAGES && villages <= MAX_VILLAGES &&
         region.armies.size() == region.villages.size();
}

TEST(Region, SameSeedGivesTheSameRegionAndWorld) {
  const Region first = generate_region(SEED, base_data());
  const Region second = generate_region(SEED, base_data());
  EXPECT_EQ(flatten(first), flatten(second));
  EXPECT_EQ(hash_after_ticks(first), hash_after_ticks(second));

  // Control: another seed's region, given this seed so that only what was
  // generated from the seed differs, is another region and another world.
  Region other = generate_region(OTHER_SEED, base_data());
  other.seed = SEED;
  EXPECT_NE(flatten(first), flatten(other));
  EXPECT_NE(hash_after_ticks(first), hash_after_ticks(other));
}

// The seeds the Godot tests start regions from: a seed generation came to
// refuse would end their whole run at its first region, so it fails here,
// named, instead.
TEST(Region, TheSeedsTheGodotTestsStartFromGenerate) {
  const std::vector<std::uint64_t> seeds{
      0,           // hud/hud_test.gd
      3,           // extension/file_saves_test.gd, play/play_test.gd GROWN_SEED
      8,           // extension/file_saves_test.gd OTHER_SEED
      18,          // hud/hud_test.gd LONG_NAME_SEED
      42,          // extension/world_adapter_test.gd SEED
      43,          // extension/world_adapter_test.gd OTHER_SEED
      2026,        // most Godot tests' SEED or REGION_SEED
      4817,        // play/play_test.gd TYPED_SEED
      4259094339,  // play/play_test.gd DRAWN
  };
  for (const std::uint64_t seed : seeds) {
    EXPECT_NO_THROW(static_cast<void>(generate_region(Seed{seed}, base_data())))
        << "seed " << seed;
  }
}

TEST(Region, HasThreeFactionsWithTerritoryAndSixToEightVillages) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, base_data());
    EXPECT_TRUE(populated(region, 3)) << "seed " << seed;

    // Control: a region of three factions is not one of four.
    EXPECT_FALSE(populated(region, 4)) << "seed " << seed;
  }
}

// `region` with `cell` marked as of `footing`, checked to read back as such.
Region marked(const Region& region, Cell cell, Footing footing) {
  Region changed = region;
  changed.map_rows.at(static_cast<std::size_t>(cell.y))
      .at(static_cast<std::size_t>(cell.x)) = tile_of(footing);
  const Map map{changed.map_rows};
  EXPECT_EQ(map.passable(cell), footing != Footing::BLOCKED);
  EXPECT_EQ(map.buildable(cell), footing == Footing::BUILDABLE);
  return changed;
}

TEST(Region, VillagesStandOnBuildableCells) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, base_data());
    EXPECT_TRUE(villages_on_buildable_cells(region)) << "seed " << seed;
    EXPECT_TRUE(builds_a_world(region)) << "seed " << seed;

    // Control: blocking the cell a resident army stands on breaks it.
    const Cell home =
        region.armies.at(region.villages.at(0).army).members.front().cell;
    EXPECT_FALSE(
        villages_on_buildable_cells(marked(region, home, Footing::BLOCKED)))
        << "seed " << seed;

    // Control: a rough cell, which bodies cross, under a village breaks it.
    EXPECT_FALSE(villages_on_buildable_cells(
        marked(region, region.villages.at(0).corner, Footing::ROUGH)))
        << "seed " << seed;
  }
}

TEST(Region, VillagesLieInTheirOwnersTerritory) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, base_data());
    EXPECT_TRUE(villages_in_owner_territory(region)) << "seed " << seed;

    // Control: handing a village to another faction breaks it.
    Region handed = region;
    Village& village = handed.villages.at(0);
    village.faction = (village.faction + 1) % handed.rules.hostile.size();
    ASSERT_NE(village.faction, region.villages.at(0).faction);
    EXPECT_FALSE(villages_in_owner_territory(handed)) << "seed " << seed;
  }
}

TEST(Region, VillagesDoNotOverlap) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, base_data());
    EXPECT_TRUE(villages_disjoint(region)) << "seed " << seed;

    // Control: moving one village onto another breaks it.
    Region stacked = region;
    stacked.villages.at(1).corner = stacked.villages.at(0).corner;
    EXPECT_FALSE(villages_disjoint(stacked)) << "seed " << seed;
  }
}

// The least squared distance between two villages' centres in `region`.
std::int64_t nearest_centres(const Region& region) {
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (std::size_t one = 0; one < region.villages.size(); ++one) {
    for (std::size_t other = one + 1; other < region.villages.size(); ++other) {
      const Cell first =
          region.armies.at(region.villages.at(one).army).objective;
      const Cell second =
          region.armies.at(region.villages.at(other).army).objective;
      const std::int64_t across = std::int64_t{first.x} - second.x;
      const std::int64_t down = std::int64_t{first.y} - second.y;
      nearest = std::min(nearest, (across * across) + (down * down));
    }
  }
  return nearest;
}

TEST(Region, NoTwoVillagesOrSeatsStandNearerThanApart) {
  const std::int32_t apart = base_data().life.growth.value_or(Growth{}).apart;
  ASSERT_GT(apart, 0);
  bool closer_without = false;
  RegionData unspaced = base_data();
  Growth growth = unspaced.life.growth.value_or(Growth{});
  growth.apart = 0;
  unspaced.life.growth = growth;
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, base_data());
    EXPECT_GE(nearest_centres(region), std::int64_t{apart} * apart)
        << "seed " << seed;
    // Control: at an apart of 0 some seed sets two centres nearer.
    closer_without = closer_without ||
                     nearest_centres(generate_region(Seed{seed}, unspaced)) <
                         std::int64_t{apart} * apart;
  }
  EXPECT_TRUE(closer_without);
}

// The structures of `village` as kinds at offsets from its centre, in the
// order they were laid.
std::vector<std::pair<std::string, std::pair<std::int32_t, std::int32_t>>>
layout_of(const Region& region, const Village& village) {
  const Cell centre = region.armies.at(village.army).objective;
  std::vector<std::pair<std::string, std::pair<std::int32_t, std::int32_t>>>
      laid;
  for (const std::size_t structure : village.structures) {
    const Cell cell = region.structures.at(structure).cell;
    laid.emplace_back(region.structure_kinds.at(structure),
                      std::make_pair(cell.x - centre.x, cell.y - centre.y));
  }
  return laid;
}

// How many pairs of villages of one template, seats left out, `region`
// holds, and how many of them share a layout round their centres.
std::pair<std::size_t, std::size_t> pairs_alike(const Region& region,
                                                const RegionData& data) {
  std::pair<std::size_t, std::size_t> counted{0, 0};
  for (std::size_t one = 0; one < region.villages.size(); ++one) {
    for (std::size_t other = one + 1; other < region.villages.size(); ++other) {
      const Village& first = region.villages.at(one);
      const Village& second = region.villages.at(other);
      if (first.kind == second.kind && first.kind != data.seat.name) {
        ++counted.first;
        counted.second += static_cast<std::size_t>(layout_of(region, first) ==
                                                   layout_of(region, second));
      }
    }
  }
  return counted;
}

TEST(Region, TwoVillagesOfOneTemplateOnTwoSitesAreLaidOutApart) {
  // Every pair of villages of one template on other sites of the same
  // region has its own layout round its centre.
  const RegionData& data = base_data();
  std::size_t pairs = 0;
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const auto [counted, alike] =
        pairs_alike(generate_region(Seed{seed}, data), data);
    pairs += counted;
    EXPECT_EQ(alike, 0U) << "seed " << seed;
  }
  EXPECT_GT(pairs, 0U);
  // Control: the same site and seed give the same layout.
  const Region once = generate_region(SEED, data);
  const Region again = generate_region(SEED, data);
  for (std::size_t village = 0; village < once.villages.size(); ++village) {
    EXPECT_EQ(layout_of(once, once.villages.at(village)),
              layout_of(again, again.villages.at(village)));
  }
}

// The name of the kind person `person` of `world` wears in slot `slot`, by
// `gear`, or nothing.
std::string worn_in(const World& world, const Gear& gear, std::size_t person,
                    std::size_t slot) {
  const std::optional<std::size_t> kind = world.person(person).worn.at(slot);
  return kind ? gear.kinds.at(*kind).name : std::string{};
}

TEST(Region, EachPersonIsBornWearingTheLoadoutOfItsCalling) {
  constexpr std::size_t WEAPON = 0;
  constexpr std::size_t ARMOUR = 1;
  constexpr std::size_t HEAD = 2;
  constexpr std::uint64_t YEARS = 10;
  const RegionData& data = base_data();
  ASSERT_TRUE(data.life.gear.has_value());
  const Gear& gear = data.life.gear.value_or(Gear{});
  Region region = generate_region(SEED, data);
  region.rules.ticks_per_day = 1;
  // A soldier: one born in a village, serving in an army of its own.
  region.armies.push_back(born_in(region, 0));
  std::vector<bool> residents(region.armies.size(), false);
  for (const Village& village : region.villages) {
    residents.at(village.army) = true;
  }
  std::map<std::size_t, std::string> founded;
  for (const SectSpec& sect : region.sects) {
    for (const Founder& founder : sect.founders) {
      founded[founder.person] = sect.name;
    }
  }
  World world = world_of(region);
  std::size_t person = 0;
  std::size_t founders = 0;
  std::size_t soldiers = 0;
  std::size_t armed = 0;
  std::size_t unarmed = 0;
  for (std::size_t army = 0; army < region.armies.size(); ++army) {
    for (std::size_t member = 0; member < region.armies.at(army).members.size();
         ++member, ++person) {
      ASSERT_EQ(world.person(person).worn.size(), gear.slots.size());
      if (founded.count(person) > 0) {
        // A sect's founders carry steel, or darts, in leather.
        const std::string weapon = worn_in(world, gear, person, WEAPON);
        EXPECT_TRUE(weapon == "steel_sword" || weapon == "steel_saber" ||
                    weapon == "darts")
            << weapon;
        EXPECT_EQ(worn_in(world, gear, person, ARMOUR), "leather");
        ++founders;
      } else if (!residents.at(army)) {
        EXPECT_EQ(worn_in(world, gear, person, ARMOUR), "lamellar");
        EXPECT_EQ(worn_in(world, gear, person, HEAD), "helmet");
        ++soldiers;
      } else {
        EXPECT_EQ(worn_in(world, gear, person, ARMOUR), "cloth");
        ++(worn_in(world, gear, person, WEAPON).empty() ? unarmed : armed);
      }
    }
  }
  // Setup: the region starts with each calling, and villagers drawn both
  // armed and not.
  EXPECT_GT(founders, 0U);
  EXPECT_GT(soldiers, 0U);
  EXPECT_GT(armed, 0U);
  EXPECT_GT(unarmed, 0U);
  // Those born later are dressed by the same birth rule.
  const std::size_t first_born = world.population();
  for (std::uint64_t day = 0; day < YEARS * DAYS_PER_YEAR; ++day) {
    world.tick();
  }
  ASSERT_GT(world.population(), first_born);
  for (std::size_t born = first_born; born < world.population(); ++born) {
    EXPECT_FALSE(worn_in(world, gear, born, ARMOUR).empty()) << born;
  }
  // Control: with no loadouts, no one wears anything.
  RegionData bare = data;
  if (bare.life.gear) {
    bare.life.gear->loadouts.clear();
  }
  Region undressed = generate_region(SEED, bare);
  const World plain = world_of(undressed);
  for (std::size_t each = 0; each < plain.population(); ++each) {
    for (const std::optional<std::size_t>& kind : plain.person(each).worn) {
      EXPECT_FALSE(kind.has_value());
    }
  }
}

TEST(Region, GeneratesAFourthFactionAddedAsData) {
  RegionData data = base_data();
  data.factions.push_back(Faction{"Martial clans",
                                  {1},
                                  data.factions.at(0).skills,
                                  std::nullopt,
                                  {},
                                  std::nullopt,
                                  std::nullopt});
  data.factions.at(1).hostile_to.push_back(3);
  // Without sects: four factions' territories leave a sect's seat no room on
  // 5 of 200 seeds.
  clear_sects(data);

  const Region region = generate_region(SEED, data);
  EXPECT_TRUE(region.rules.hostile.at(3).at(1));
  EXPECT_TRUE(region.rules.hostile.at(1).at(3));
  EXPECT_NO_THROW(world_of(region));
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region generated = generate_region(Seed{seed}, data);
    EXPECT_TRUE(
        populated(generated, 4) && villages_on_buildable_cells(generated) &&
        villages_in_owner_territory(generated) && villages_disjoint(generated))
        << "seed " << seed;
  }
}

TEST(Region, ASeatKeepingMoreThanAVillagesWindowHoldsIsRefused) {
  // A window holds 128 cells a side with 4 round what a village keeps: a seat
  // 120 cells wide keeps 120 across its middle, wider than its growth keeps.
  constexpr std::int32_t WIDEST = 120;
  RegionData widest = base_data();
  widest.seat.area.width = WIDEST;
  // Control: the widest seat a window holds is accepted.
  EXPECT_TRUE(consistent(widest));
  RegionData wider = widest;
  wider.seat.area.width = WIDEST + 1;
  EXPECT_FALSE(consistent(wider));
}

TEST(Region, RejectsInconsistentData) {
  EXPECT_NO_THROW(static_cast<void>(generate_region(SEED, base_data())));

  RegionData unknown_enemy = base_data();
  unknown_enemy.factions.at(0).hostile_to.push_back(3);
  EXPECT_THROW(static_cast<void>(generate_region(SEED, unknown_enemy)),
               std::invalid_argument);

  RegionData weightless = base_data();
  for (TerrainKind& kind : weightless.terrain) {
    kind.weight = 0;
  }
  EXPECT_THROW(static_cast<void>(generate_region(SEED, weightless)),
               std::invalid_argument);

  RegionData shapeless = base_data();
  shapeless.villages.at(0).buildings.push_back(
      Building{Structure{{0, 0}, 1, {0, 1}}, "house", 0});
  EXPECT_THROW(static_cast<void>(generate_region(SEED, shapeless)),
               std::invalid_argument);

  RegionData outside = base_data();
  outside.seat.buildings.push_back(Building{
      Structure{{outside.seat.area.width - 1, 0}, 1, {2, 1}}, "house", 1});
  EXPECT_THROW(static_cast<void>(generate_region(SEED, outside)),
               std::invalid_argument);

  RegionData overlapping = base_data();
  overlapping.seat.buildings.push_back(overlapping.seat.buildings.front());
  EXPECT_THROW(static_cast<void>(generate_region(SEED, overlapping)),
               std::invalid_argument);

  RegionData over_centre = base_data();
  VillageTemplate& central = over_centre.seat;
  central.buildings.push_back(Building{
      Structure{{central.area.width / 2, central.area.height / 2}, 1, {1, 1}},
      "barrels", 0});
  EXPECT_THROW(static_cast<void>(generate_region(SEED, over_centre)),
               std::invalid_argument);

  RegionData ungrowing = base_data();
  ungrowing.life.growth.reset();
  EXPECT_THROW(static_cast<void>(generate_region(SEED, ungrowing)),
               std::invalid_argument);

  RegionData empty = base_data();
  for (Building& building : empty.villages.at(0).buildings) {
    building.households = 0;
  }
  EXPECT_THROW(static_cast<void>(generate_region(SEED, empty)),
               std::invalid_argument);
}

// The template `village` was laid out from, the seat template included, or
// null when `data` has none of its name.
const VillageTemplate* template_of(const RegionData& data,
                                   const Village& village) {
  if (village.kind == data.seat.name) {
    return &data.seat;
  }
  const auto made_from =
      std::find_if(data.villages.begin(), data.villages.end(),
                   [&village](const VillageTemplate& kind) {
                     return kind.name == village.kind;
                   });
  return made_from == data.villages.end() ? nullptr : &*made_from;
}

// Whether the village's structures carry its template buildings' kinds, one
// each, in whatever order their roles placed them.
bool kinds_follow(const Region& region, const Village& village,
                  const VillageTemplate& made_from) {
  std::vector<std::string> laid;
  laid.reserve(village.structures.size());
  for (const std::size_t structure : village.structures) {
    laid.push_back(region.structure_kinds.at(structure));
  }
  std::vector<std::string> listed;
  listed.reserve(made_from.buildings.size());
  for (const Building& building : made_from.buildings) {
    listed.push_back(building.kind);
  }
  std::sort(laid.begin(), laid.end());
  std::sort(listed.begin(), listed.end());
  return laid == listed;
}

// The people `made_from`'s buildings start with: two for each household.
std::uint32_t housed_by(const VillageTemplate& made_from) {
  std::uint32_t housed = 0;
  for (const Building& building : made_from.buildings) {
    housed += 2 * building.households;
  }
  return housed;
}

// Whether `village` was laid out from a template of `data` whose buildings'
// kinds its structures carry, and whose households its army counts, two
// people each. A flat count per structure would differ: most buildings start
// with no household.
bool follows_its_template(const RegionData& data, const Region& region,
                          const Village& village) {
  const VillageTemplate* made_from = template_of(data, village);
  return made_from != nullptr && kinds_follow(region, village, *made_from) &&
         region.armies.at(village.army).members.size() ==
             housed_by(*made_from) &&
         housed_by(*made_from) != 2 * village.structures.size();
}

TEST(Region, StructuresCarryTheirKindAndBuildingsHouseTheHouseholds) {
  const RegionData& data = base_data();
  const Region region = generate_region(SEED, data);
  EXPECT_EQ(region.structure_kinds.size(), region.structures.size());
  for (const Village& village : region.villages) {
    EXPECT_TRUE(follows_its_template(data, region, village)) << village.name;
  }
}

TEST(Region, EachSeatKeepsItsOpenCourtClearOfGrowth) {
  const Region region = generate_region(SEED, base_data());
  const std::vector<VillageSpec> specs = village_specs(region);
  ASSERT_FALSE(region.sects.empty());
  for (const SectSpec& sect : region.sects) {
    const Village& seat = region.villages.at(sect.seat);
    const std::vector<Clearing>& clearings = specs.at(sect.seat).clearings;
    // The world keeps clear what the seat's template does, from its corner,
    // and every cell of it inside the seat is open, as a court is.
    ASSERT_EQ(clearings.size(), base_data().seat.clearings.size());
    const std::vector<Cell> open = open_cells(region, seat);
    for (const Clearing& clearing : clearings) {
      for (const Cell cell : cells_of(clearing.corner, clearing.area)) {
        const bool inside = cell.x < seat.corner.x + seat.area.width &&
                            cell.y < seat.corner.y + seat.area.height;
        EXPECT_TRUE(!inside || std::any_of(open.begin(), open.end(),
                                           [cell](Cell free) {
                                             return free.x == cell.x &&
                                                    free.y == cell.y;
                                           }))
            << seat.name << " (" << cell.x << ", " << cell.y << ")";
      }
    }
  }
  // Control: an ordinary village keeps nothing clear.
  EXPECT_TRUE(specs.front().clearings.empty());
}

// The index of the occupation named `name`, or how many there are for none.
std::size_t occupation_named(const Occupations& occupations,
                             const std::string& name) {
  const auto found = std::find_if(
      occupations.list.begin(), occupations.list.end(),
      [&name](const Occupation& each) { return each.name == name; });
  return static_cast<std::size_t>(found - occupations.list.begin());
}

// Whether a village is home to someone of working age, and to someone
// holding a given occupation.
struct Staffed {
  bool adult = false;
  bool holding = false;
};

// Each of `region`'s villages' staffing in `world`, for occupation
// `occupation` of `occupations`, from its working age.
std::vector<Staffed> staffing(const World& world, const Region& region,
                              const Occupations& occupations,
                              std::size_t occupation) {
  std::vector<Staffed> staffed(region.villages.size());
  for (std::size_t person = 0; person < world.population(); ++person) {
    if (const std::optional<std::size_t> home = world.person(person).home) {
      Staffed& village = staffed.at(*home);
      village.adult =
          village.adult ||
          world.age_in_years(person) >= std::int64_t{occupations.working_age};
      village.holding =
          village.holding || world.occupation_of(person) == occupation;
    }
  }
  return staffed;
}

TEST(Region, EveryPeopledVillageHasAMerchantFromTheStart) {
  const Region region = generate_region(SEED, base_data());
  const World world = world_of(region);
  // An empty table leaves no merchant, which the assertion below catches.
  const Occupations occupations =
      region.life.occupations.value_or(Occupations{});
  const std::size_t merchant = occupation_named(occupations, "merchant");
  ASSERT_LT(merchant, occupations.list.size());
  const std::vector<Staffed> staffed =
      staffing(world, region, occupations, merchant);
  // The world fills its openings as it is created, so every village with a
  // resident of working age has its merchant before the first tick.
  std::size_t peopled = 0;
  for (std::size_t village = 0; village < staffed.size(); ++village) {
    peopled += staffed.at(village).adult ? std::size_t{1} : std::size_t{0};
    EXPECT_EQ(staffed.at(village).holding, staffed.at(village).adult)
        << region.villages.at(village).name;
  }
  EXPECT_GT(peopled, 0U);
  // The base pack's merchant sells the herb, so each of those villages has
  // a seller of it.
  const std::vector<std::size_t>& sold = occupations.list.at(merchant).wares;
  EXPECT_NE(std::find(sold.begin(), sold.end(), 0), sold.end());
}

// Of the villages laid from one template: how many there are, how many have
// a temple and an inn, how many have an adult, and how many an innkeeper.
struct Tally {
  std::size_t villages = 0;
  std::size_t temples = 0;
  std::size_t inns = 0;
  std::size_t adults = 0;
  std::size_t innkeepers = 0;
};

// The tally of `region`'s villages of template `kind`, from their staffing
// `staffed` for the innkeeper.
Tally tally(const Region& region, const std::vector<Staffed>& staffed,
            const std::string& kind) {
  Tally counted;
  for (std::size_t village = 0; village < region.villages.size(); ++village) {
    const Village& laid = region.villages.at(village);
    if (laid.kind != kind) {
      continue;
    }
    const auto has = [&region, &laid](const std::string& structure_kind) {
      return std::any_of(laid.structures.begin(), laid.structures.end(),
                         [&](std::size_t structure) {
                           return region.structure_kinds.at(structure) ==
                                  structure_kind;
                         });
    };
    ++counted.villages;
    counted.temples += has("temple") ? 1U : 0U;
    counted.inns += has("inn") ? 1U : 0U;
    counted.adults += staffed.at(village).adult ? 1U : 0U;
    counted.innkeepers += staffed.at(village).holding ? 1U : 0U;
  }
  return counted;
}

TEST(Region, OnlyTheVillageTemplateStartsWithATempleAndAnInnKeeper) {
  const Region region = generate_region(SEED, base_data());
  const World world = world_of(region);
  const Occupations occupations =
      region.life.occupations.value_or(Occupations{});
  const std::size_t innkeeper = occupation_named(occupations, "innkeeper");
  ASSERT_LT(innkeeper, occupations.list.size());
  const std::vector<Staffed> staffed =
      staffing(world, region, occupations, innkeeper);
  const Tally villages = tally(region, staffed, "village");
  const Tally hamlets = tally(region, staffed, "hamlet");
  // Setup: both templates are laid, with adults in each.
  ASSERT_GT(villages.adults, 0U);
  ASSERT_GT(hamlets.adults, 0U);
  EXPECT_EQ(villages.temples, villages.villages);
  EXPECT_EQ(villages.inns, villages.villages);
  EXPECT_EQ(villages.innkeepers, villages.adults);
  // Control: a hamlet has neither, so its adults hold no innkeeper's place.
  EXPECT_EQ(hamlets.temples, 0U);
  EXPECT_EQ(hamlets.inns, 0U);
  EXPECT_EQ(hamlets.innkeepers, 0U);
}

TEST(Region, TheBasePacksVillageTierNamesATempleThenAnInn) {
  const Growth growth = base_data().life.growth.value_or(Growth{});
  ASSERT_EQ(growth.tiers.size(), 3U);
  std::vector<std::vector<std::string>> named;
  for (const Tier& tier : growth.tiers) {
    std::vector<std::string> kinds;
    kinds.reserve(tier.square_kinds.size());
    for (const std::size_t plan : tier.square_kinds) {
      kinds.push_back(growth.plans.at(plan).kind);
    }
    named.push_back(kinds);
  }
  // The hamlet and the town name none; the town holds the village's.
  EXPECT_EQ(named,
            (std::vector<std::vector<std::string>>{{}, {"temple", "inn"}, {}}));
}

TEST(Region, TheBasePacksTempleTakesTwoMonksBeforeTheFields) {
  const Occupations occupations =
      base_data().life.occupations.value_or(Occupations{});
  const std::vector<Candidate> adults(4, Candidate{true, std::nullopt});
  const auto named = [&occupations](const std::string& name) {
    return std::optional<std::size_t>{occupation_named(occupations, name)};
  };
  // A village of four with an inn, a temple and a field fills the temple
  // before the field; without the temple, the field takes the two.
  EXPECT_EQ(assign(occupations, adults, {"inn", "temple", "field"}),
            (std::vector<std::optional<std::size_t>>{
                named("merchant"), named("innkeeper"), named("monk"),
                named("monk")}));
  EXPECT_EQ(assign(occupations, adults, {"inn", "field"}),
            (std::vector<std::optional<std::size_t>>{
                named("merchant"), named("innkeeper"), named("farmer"),
                named("farmer")}));
}

TEST(Region, AVillageIsNamedForItsKindAndNumber) {
  EXPECT_EQ(village_name("hamlet", 0), "hamlet 1");
  EXPECT_EQ(village_name("village", 11), "village 12");
  // The generator names the villages it lays from a template so.
  const Region region = generate_region(SEED, base_data());
  ASSERT_FALSE(region.villages.empty());
  EXPECT_EQ(region.villages.front().name,
            village_name(region.villages.front().kind, 0));
}

TEST(Region, EachSeatsGateIsTheSeatGateAmongItsStructures) {
  const Region region = generate_region(SEED, base_data());
  ASSERT_FALSE(region.sects.empty());
  for (const SectSpec& sect : region.sects) {
    const Village& seat = region.villages.at(sect.seat);
    const std::optional<std::size_t> gate =
        gate_of(seat, region.structure_kinds);
    ASSERT_TRUE(gate.has_value()) << seat.name;
    const std::size_t structure = gate.value_or(0);
    EXPECT_EQ(region.structure_kinds.at(structure), SEAT_GATE);
    EXPECT_NE(
        std::find(seat.structures.begin(), seat.structures.end(), structure),
        seat.structures.end());
  }
  // Control: an ordinary village has no gate, nor a seat whose kinds are not
  // known.
  EXPECT_FALSE(
      gate_of(region.villages.front(), region.structure_kinds).has_value());
  EXPECT_FALSE(
      gate_of(region.villages.at(region.sects.front().seat), {}).has_value());
}

TEST(Region, APersonBornIntoAVillageIsOfItsFactionInsideIt) {
  Region region = generate_region(SEED, base_data());
  const Village& village = region.villages.at(1);
  const ArmySpec& residents = region.armies.at(village.army);
  const ArmySpec born = born_in(region, 1);

  ASSERT_EQ(born.members.size(), 1U);
  const Recruit& person = born.members.front();
  EXPECT_EQ(born.faction, village.faction);
  EXPECT_EQ(person.health, residents.members.front().health);
  EXPECT_TRUE(inside(village, person.cell));
  EXPECT_TRUE(same(born.objective, person.cell));
  EXPECT_TRUE(std::none_of(residents.members.begin(), residents.members.end(),
                           [&person](const Recruit& resident) {
                             return same(resident.cell, person.cell);
                           }));
  region.armies.push_back(born);
  EXPECT_NO_THROW(world_of(region));
  // Control: a village the region lacks has no one born into it.
  EXPECT_THROW(static_cast<void>(born_in(region, region.villages.size())),
               std::out_of_range);
}

TEST(Region, NoOneIsBornIntoAVillageWithNoFreeCell) {
  Region region = generate_region(SEED, base_data());
  const Village& village = region.villages.at(1);
  // Control: with its residents as generated, a person is born into it.
  EXPECT_NO_THROW(static_cast<void>(born_in(region, 1)));
  // Every open cell of the village taken by a resident leaves none to be born
  // on, a refusal of the region's room rather than a broken region.
  ArmySpec& residents = region.armies.at(village.army);
  const std::uint64_t health = residents.members.front().health;
  residents.members.clear();
  for (const Cell cell : open_cells(region, village)) {
    residents.members.push_back(Recruit{cell, health});
  }
  ASSERT_FALSE(residents.members.empty());
  EXPECT_THROW(static_cast<void>(born_in(region, 1)), std::runtime_error);
}

// The first sect of faction `faction` in `world`.
std::size_t sect_of_faction(const World& world, std::size_t faction) {
  std::size_t sect = 0;
  while (sect + 1 < world.sect_count() && world.sect_faction(sect) != faction) {
    ++sect;
  }
  return sect;
}

// The region of seed 1, with days of one tick and a person born into village
// `village`, homed there unless `homeless`, as the last army, lived `days`
// days. On that seed, with no diplomacy, whose wars would change who lives
// to marry, partners for the body marry in its village within 40 years, as
// the test's setup checks; on seed 7, where they once did, none does now.
constexpr Seed MARRYING_SEED{1};

World born_and_lived(std::size_t village, bool homeless, std::uint64_t days) {
  RegionData data = base_data();
  data.life.diplomacy.reset();
  Region region = generate_region(MARRYING_SEED, data);
  region.rules.ticks_per_day = 1;
  ArmySpec born = born_in(region, village);
  if (homeless) {
    born.home.reset();
  }
  region.armies.push_back(born);
  World world = world_of(region);
  for (std::uint64_t day = 0; day < days; ++day) {
    world.tick();
  }
  return world;
}

TEST(Region, APersonBornIntoAVillageMakesItHomeToMarryFromAndApplyWith) {
  constexpr std::size_t VILLAGE = 1;
  constexpr std::uint64_t YEARS = 40;
  // The body is the last person the world is created with.
  const World young = born_and_lived(VILLAGE, false, 0);
  const std::size_t body = young.population() - 1;
  ASSERT_EQ(young.person(body).home, std::optional<std::size_t>{VILLAGE});
  const World homeless = born_and_lived(VILLAGE, true, YEARS * DAYS_PER_YEAR);
  ASSERT_FALSE(homeless.person(body).home.has_value());
  // Setup: where the body takes no one, partners for it, of the other sex and
  // another family and unwed as the world began, married in its village.
  const Person& self = young.person(body);
  std::size_t partners_wed = 0;
  for (std::size_t other = 0; other < homeless.population(); ++other) {
    const Person& partner = homeless.person(other);
    const bool unwed_at_start =
        other >= young.population() || young.person(other).spouses.empty();
    if (other != body && partner.sex != self.sex &&
        partner.family != self.family && partner.home == self.home &&
        unwed_at_start && !partner.spouses.empty()) {
      ++partners_wed;
    }
  }
  ASSERT_GT(partners_wed, 0U);
  const World world = born_and_lived(VILLAGE, false, YEARS * DAYS_PER_YEAR);
  EXPECT_FALSE(world.person(body).spouses.empty());
  // Control: the same body with no home never marries.
  EXPECT_TRUE(homeless.person(body).spouses.empty());
  // It may apply to a sect as far as its home goes, where the homeless body
  // is refused for having none.
  const std::size_t sect =
      sect_of_faction(young, young.faction(young.army_count() - 1));
  EXPECT_NE(young.application_refusal(body, sect), ApplicationRefusal::NO_HOME);
  EXPECT_EQ(born_and_lived(VILLAGE, true, 0).application_refusal(body, sect),
            ApplicationRefusal::NO_HOME);
}

// Health of each person gathered().
constexpr std::uint64_t GATHERED_HEALTH = 100'000;

// The cells round village `home`'s centre that bodies pass, no structure of
// the region covers and none of its people stands on, nearest the centre
// first, row by row among equals.
std::vector<Cell> open_round(const Region& region, const Village& home) {
  const Map map{region.map_rows};
  const Cell centre = region.armies.at(home.army).objective;
  std::vector<bool> taken(region.territory.size(), false);
  for (const Structure& structure : region.structures) {
    for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
      taken.at(index_of(cell)) = true;
    }
  }
  for (const ArmySpec& army : region.armies) {
    for (const Recruit& member : army.members) {
      taken.at(index_of(member.cell)) = true;
    }
  }
  const std::int32_t reach =
      region.life.growth.value_or(Growth{}).tiers.back().vicinity;
  std::vector<Cell> open;
  for (Cell cell{0, std::max(centre.y - reach, 0)};
       cell.y <= std::min(centre.y + reach, REGION_SIDE - 1); ++cell.y) {
    for (cell.x = std::max(centre.x - reach, 0);
         cell.x <= std::min(centre.x + reach, REGION_SIDE - 1); ++cell.x) {
      if (map.passable(cell) && !taken.at(index_of(cell))) {
        open.push_back(cell);
      }
    }
  }
  std::stable_sort(open.begin(), open.end(), [centre](Cell one, Cell other) {
    const auto far = [centre](Cell cell) {
      return ((cell.x - centre.x) * (cell.x - centre.x)) +
             ((cell.y - centre.y) * (cell.y - centre.y));
    };
    return far(one) < far(other);
  });
  return open;
}

// `people` people of GATHERED_HEALTH on the open cells round village
// `home`'s centre, nearest first.
std::vector<Recruit> gathered(const Region& region, const Village& home,
                              std::size_t people) {
  std::vector<Recruit> members;
  for (const Cell cell : open_round(region, home)) {
    if (members.size() < people) {
      members.push_back(Recruit{cell, GATHERED_HEALTH});
    }
  }
  return members;
}

// The first village owned by `faction`.
std::size_t first_of(const Region& region, std::size_t faction) {
  std::size_t village = 0;
  while (region.villages.at(village).faction != faction) {
    ++village;
  }
  return village;
}

TEST(Region, AVillageRunsItsWholeLifeCycleInAGeneratedRegion) {
  constexpr std::size_t ORTHODOX = 0;
  constexpr std::size_t DEMONIC = 1;
  constexpr std::size_t UNORTHODOX = 2;
  constexpr std::uint64_t LIMIT = 20'000;
  constexpr std::size_t RAIDERS = 12;
  constexpr std::size_t REBUILDERS = 12;
  Region region = generate_region(SEED, base_data());
  const RegionData& data = base_data();
  const std::size_t target = first_of(region, ORTHODOX);
  const Cell centre =
      region.armies.at(region.villages.at(target).army).objective;

  // Raiders from a Demonic village, with a killing strike beside their
  // faction's skills; rebuilders from an Unorthodox one, with their
  // faction's skills alone.
  const Skill kill{3 * CELL_SIZE, CELL_SIZE, 20'000, 0, 15};
  ArmySpec raiders = born_in(region, first_of(region, DEMONIC));
  raiders.members =
      gathered(region, region.villages.at(first_of(region, DEMONIC)), RAIDERS);
  raiders.objective = raiders.members.front().cell;
  raiders.skills = data.factions.at(DEMONIC).skills;
  raiders.skills.push_back(kill);
  const Cell raiders_home = raiders.objective;
  ArmySpec rebuilders = born_in(region, first_of(region, UNORTHODOX));
  rebuilders.members = gathered(
      region, region.villages.at(first_of(region, UNORTHODOX)), REBUILDERS);
  rebuilders.objective = rebuilders.members.front().cell;
  // Setup: the seed's villages each had room for the whole band, since
  // gathered() gives at most as many as asked.
  ASSERT_EQ(raiders.members.size() + rebuilders.members.size(),
            RAIDERS + REBUILDERS);
  const std::size_t raider = region.armies.size();
  const std::size_t rebuilder = raider + 1;
  region.armies.push_back(raiders);
  region.armies.push_back(rebuilders);
  World world = world_of(region);

  std::vector<VillageState> states{world.village_state(target)};
  std::vector<std::size_t> owners{world.owner(target)};
  const auto run_until = [&](const auto& done) {
    for (std::uint64_t tick = 0; tick < LIMIT && !done(); ++tick) {
      world.tick();
      if (world.village_state(target) != states.back()) {
        states.push_back(world.village_state(target));
      }
      if (world.owner(target) != owners.back()) {
        owners.push_back(world.owner(target));
      }
    }
  };

  world.order(raider, {centre, {0, 1, 2}});
  run_until([&] { return world.village_state(target) == VillageState::RUINS; });
  world.order(raider, {raiders_home, {0, 1, 2}});
  run_until([&] {
    return same(cell_of(world.person(world.roster(raider).front()).position),
                raiders_home);
  });
  world.order(rebuilder, {centre, {0, 1}});
  run_until(
      [&] { return world.village_state(target) == VillageState::PROSPEROUS; });

  const std::vector<VillageState> cycle{
      VillageState::PROSPEROUS, VillageState::RAIDED, VillageState::RUINS,
      VillageState::REBUILDING, VillageState::PROSPEROUS};
  EXPECT_EQ(states, cycle);
  EXPECT_EQ(owners, (std::vector<std::size_t>{ORTHODOX, UNORTHODOX}));
  const Destroyer none{region.armies.size(), data.factions.size()};
  const Destroyer who = world.destroyer(target).value_or(none);
  EXPECT_EQ((std::pair{who.army, who.faction}), (std::pair{raider, DEMONIC}));
}

TEST(Region, EachFactionIsLedByTheFirstResidentOfItsFirstVillage) {
  const Region region = generate_region(SEED, base_data());
  const World world = world_of(region);
  for (std::size_t faction = 0; faction < region.leadership.size(); ++faction) {
    const std::size_t first = first_of(region, faction);
    EXPECT_EQ(world.leader(faction),
              std::optional<std::size_t>{
                  world.roster(region.villages.at(first).army).front()})
        << "faction " << faction;
  }
  // Only the Demonic Cult passes its lead by duel.
  EXPECT_FALSE(region.leadership.at(0).succession.has_value());
  EXPECT_TRUE(region.leadership.at(1).succession.has_value());
  EXPECT_FALSE(region.leadership.at(2).succession.has_value());
}

// Combat pace, in ticks: the targets base_data() is tuned to.
constexpr std::uint64_t SECOND = TICKS_PER_SECOND;
constexpr std::uint64_t MINUTE = 60 * SECOND;
constexpr std::uint64_t DUEL_LEAST = 10 * SECOND;
constexpr std::uint64_t DUEL_MOST = 30 * SECOND;
constexpr std::uint64_t SKIRMISH_LEAST = 10 * SECOND;
constexpr std::uint64_t SKIRMISH_MOST = 2 * MINUTE;
constexpr std::uint64_t RAID_LEAST = MINUTE;
constexpr std::uint64_t RAID_MOST = 5 * MINUTE;
constexpr std::size_t DEMONIC = 1;
constexpr std::size_t SKIRMISHERS = 20;
constexpr std::size_t RAIDERS = 20;
// A raid on the walled village, scaled from the hamlet's by its 26
// residents to the hamlet's 12: at most twice as long.
constexpr std::uint64_t VILLAGE_RAID_MOST = 2 * RAID_MOST;
// Seeds a walled village is raided on: the first three from 1 with room
// round it for the whole band.
constexpr std::array<Seed, 3> WALLED{Seed{1}, Seed{2}, Seed{3}};
// How far from the leader a duel's challenger stands, in cells.
constexpr std::int32_t DUEL_AWAY = 5;
// Seeds the pace is measured on, the region tests' own first.
constexpr std::array<Seed, 3> PACED{SEED, Seed{2026}, Seed{5}};

// Whether ticks `ticks` lie within [least, most].
bool within(std::uint64_t ticks, std::uint64_t least, std::uint64_t most) {
  return ticks >= least && ticks <= most;
}

// Whether a test region keeps the attack tokens it sets, or drops them.
enum class TokenGate : std::uint8_t { TOKENS, NONE };

// The ticks from a person born into the first Demonic village of the region
// `data` generates from `seed` challenging its leader, as the player does,
// until the duel is settled, or `limit` if it is not by then; with `beside`,
// born where born_in() has it, beside the leader, with no occupations; without
// the region's attack tokens under TokenGate::NONE.
std::uint64_t duel_ticks(const RegionData& data, Seed seed, std::uint64_t limit,
                         bool beside = false,
                         TokenGate gate = TokenGate::TOKENS) {
  Region region = generate_region(seed, data);
  if (gate == TokenGate::NONE) {
    region.rules.tokens.reset();
  }
  const std::size_t home = first_of(region, DEMONIC);
  ArmySpec born = born_in(region, home);
  if (beside) {
    // No occupations, so the leader stands where it was born, beside the
    // challenger, rather than stepping to its spot at home.
    region.life.occupations.reset();
  } else {
    // The challenger stands DUEL_AWAY cells from the leader, the first
    // resident, on the first open cell that far, row by row: where the
    // village templates' corner once had it born.
    const Cell lead =
        region.armies.at(region.villages.at(home).army).members.front().cell;
    const std::vector<Cell> open = open_round(region, region.villages.at(home));
    const auto away = std::find_if(open.begin(), open.end(), [lead](Cell cell) {
      return std::max(std::abs(cell.x - lead.x), std::abs(cell.y - lead.y)) ==
             DUEL_AWAY;
    });
    EXPECT_NE(away, open.end());
    if (away != open.end()) {
      born.members.front().cell = *away;
      born.objective = *away;
    }
  }
  region.armies.push_back(born);
  const std::size_t challenger = region.armies.size() - 1;
  const std::size_t residents = region.villages.at(home).army;
  World world = world_of(region);
  const std::size_t leader = world.leader(DEMONIC).value_or(0);
  // The challenger is the leader's fellow resident standing nearest the
  // challenger's cell, so both duelists draw a villager's loadout; the one
  // born there takes its place among the residents.
  const std::size_t stand_in = world.roster(challenger).front();
  const Position cell = world.person(stand_in).position;
  std::optional<std::size_t> fellow;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (const std::size_t resident : world.roster(residents)) {
    const Position at = world.person(resident).position;
    const std::int64_t across = std::int64_t{at.x} - cell.x;
    const std::int64_t down = std::int64_t{at.y} - cell.y;
    const std::int64_t apart = (across * across) + (down * down);
    if (resident != leader && apart < nearest) {
      nearest = apart;
      fellow = resident;
    }
  }
  EXPECT_TRUE(fellow.has_value());
  const std::size_t duelist = fellow.value_or(leader);
  // Setup: both duelists wear what the residents' loadout drew for them.
  EXPECT_EQ(world.person(duelist).worn.at(1), world.person(leader).worn.at(1));
  world.conscript(duelist, challenger);
  world.conscript(stand_in, residents);
  // It first takes the challenger's cell, so the duel's walk-in is the one
  // the test places.
  constexpr int LONGEST_WALK = 1'000;
  const Cell start = cell_of(cell);
  world.order(challenger, Order{start, {0, 1}});
  for (int walked = 0; walked < LONGEST_WALK &&
                       !same(cell_of(world.person(duelist).position), start);
       ++walked) {
    world.tick();
  }
  EXPECT_TRUE(same(cell_of(world.person(duelist).position), start));
  world.order(challenger, Order{cell_of(world.person(leader).position), {0, 1}},
              DEMONIC);
  // Setup: the challenge started a duel.
  EXPECT_TRUE(world.duel(DEMONIC).has_value());
  std::uint64_t ticks = 0;
  for (; ticks < limit && world.duel(DEMONIC).has_value(); ++ticks) {
    world.tick();
  }
  return ticks;
}

// The block of passable cells a skirmish is fought on, and how far from
// every village it lies.
constexpr Footprint FIELD{10, 5};
constexpr std::int32_t AWAY = 12;

// The top-left cell of the first FIELD of passable cells, AWAY cells from
// every village, row by row.
std::optional<Cell> open_field(const Region& region) {
  const Map map{region.map_rows};
  const auto clear = [&region, &map](Cell corner) {
    const auto passable = [&map](Cell cell) { return map.passable(cell); };
    const auto near = [corner](const Village& village) {
      return corner.x + FIELD.width + AWAY > village.corner.x &&
             corner.x - AWAY < village.corner.x + village.area.width &&
             corner.y + FIELD.height + AWAY > village.corner.y &&
             corner.y - AWAY < village.corner.y + village.area.height;
    };
    const std::vector<Cell> cells = cells_of(corner, FIELD);
    return std::all_of(cells.begin(), cells.end(), passable) &&
           std::none_of(region.villages.begin(), region.villages.end(), near);
  };
  for (const Cell corner : cells_of(
           Cell{0, 0},
           Footprint{REGION_SIDE - FIELD.width, REGION_SIDE - FIELD.height})) {
    if (clear(corner)) {
      return corner;
    }
  }
  return std::nullopt;
}

// The ticks two lines of SKIRMISHERS, Orthodox and Demonic, newborn and with
// their factions' skills, two cells apart on an open field of the region
// `data` generates from `seed` and bound for each other, fight until one has
// no one left, or `limit` if neither falls by then; without the region's
// attack tokens unless `tokens`.
std::uint64_t skirmish_ticks(const RegionData& data, Seed seed,
                             std::uint64_t limit, bool tokens = true) {
  constexpr std::int32_t LINE = 4;
  constexpr std::int32_t APART = 6;
  Region region = generate_region(seed, data);
  if (!tokens) {
    region.rules.tokens.reset();
  }
  const std::optional<Cell> field = open_field(region);
  EXPECT_TRUE(field.has_value());
  const Cell corner = field.value_or(Cell{0, 0});
  const Cell west{corner.x + 2, corner.y + 2};
  const Cell east{corner.x + APART + 2, corner.y + 2};
  ArmySpec orthodox{east, {}, 0, data.factions.at(0).skills};
  ArmySpec demonic{west, {}, DEMONIC, data.factions.at(DEMONIC).skills};
  for (const Cell cell : cells_of(corner, Footprint{LINE, 5})) {
    orthodox.members.push_back(Recruit{cell, data.life.newborn_health});
    demonic.members.push_back(
        Recruit{Cell{cell.x + APART, cell.y}, data.life.newborn_health});
  }
  const std::size_t first = region.armies.size();
  region.armies.push_back(orthodox);
  region.armies.push_back(demonic);
  World world = world_of(region);
  // Setup: two lines of SKIRMISHERS each.
  EXPECT_EQ(world.roster(first).size(), SKIRMISHERS);
  EXPECT_EQ(world.roster(first + 1).size(), SKIRMISHERS);
  std::uint64_t ticks = 0;
  for (; ticks < limit && !world.roster(first).empty() &&
         !world.roster(first + 1).empty();
       ++ticks) {
    world.tick();
  }
  return ticks;
}

// A raid: the kind of village raided, the first of that kind not owned by
// the Demonic Cult, and the size of the Demonic band raiding it.
struct Raid {
  const char* kind;
  std::size_t band;
};

// How a raid went: the ticks it ran, and the residents still living; whether
// an outpost asked for was placed, how many times the band's objective
// moved, and whether it moved while a foe a member saw as the tick began
// still lived.
struct Raided {
  std::uint64_t ticks;
  std::size_t residents;
  bool outpost_placed = false;
  std::size_t moves = 0;
  bool moved_past_foe = false;
};

constexpr std::size_t VILLAGE_RAIDERS = 44;
constexpr Raid HAMLET_RAID{"hamlet", RAIDERS};
// The hamlet's band scaled to the village's 26 residents.
constexpr Raid VILLAGE_RAID{"village", VILLAGE_RAIDERS};

// The first village of kind `kind` in the region not owned by the Demonic
// Cult.
std::size_t first_raided(const Region& region, const std::string& kind) {
  std::size_t village = 0;
  while (region.villages.at(village).faction == DEMONIC ||
         region.villages.at(village).kind != kind) {
    ++village;
  }
  return village;
}

// How `raid` goes on the region `data` generates from `seed`: its band,
// newborn and with its faction's skills, stands on the passable cells west
// of the village, column by column, bound for its centre if `bound`, and
// otherwise for where it stands, until the village is in ruins, or for
// `limit` ticks if it is not by then; people close on foes in sight if
// `sighted`.
// Adds to `village` of `region` a copy of its first structure on its
// edge, a cell wide, on the first passable cell no structure or person stands
// on from `apart` cells away from its centre outward, east, then west, south
// and north of it. Returns whether there was such a structure and such a cell.
bool add_outpost(Region& region, Village& village, std::int32_t apart) {
  const auto edge =
      std::find_if(village.structures.begin(), village.structures.end(),
                   [&region](std::size_t index) {
                     return region.structures.at(index).role == Role::EDGE;
                   });
  if (edge == village.structures.end()) {
    return false;
  }
  const Map map{region.map_rows};
  const Cell centre = region.armies.at(village.army).objective;
  const auto stood_on = [&region](Cell cell) {
    return std::any_of(region.armies.begin(), region.armies.end(),
                       [cell](const ArmySpec& army) {
                         return std::any_of(army.members.begin(),
                                            army.members.end(),
                                            [cell](const Recruit& recruit) {
                                              return recruit.cell.x == cell.x &&
                                                     recruit.cell.y == cell.y;
                                            });
                       });
  };
  const auto free = [&](Cell cell) {
    return map.passable(cell) && !stood_on(cell) &&
           std::none_of(region.structures.begin(), region.structures.end(),
                        [cell](const Structure& structure) {
                          const std::vector<Cell> covered =
                              cells_of(structure.cell, structure.footprint);
                          return std::any_of(covered.begin(), covered.end(),
                                             [cell](Cell spot) {
                                               return spot.x == cell.x &&
                                                      spot.y == cell.y;
                                             });
                        });
  };
  for (const Cell way : {Cell{1, 0}, Cell{-1, 0}, Cell{0, 1}, Cell{0, -1}}) {
    for (Cell cell{centre.x + (way.x * apart), centre.y + (way.y * apart)};
         cell.x >= 0 && cell.y >= 0 && cell.x < map.width() &&
         cell.y < map.height();
         cell = Cell{cell.x + way.x, cell.y + way.y}) {
      if (!free(cell)) {
        continue;
      }
      Structure outpost = region.structures.at(*edge);
      outpost.cell = cell;
      outpost.footprint = Footprint{1, 1};
      village.structures.push_back(region.structures.size());
      region.holdings.push_back(region.holdings.at(*edge));
      region.structure_kinds.push_back(region.structure_kinds.at(*edge));
      region.structures.push_back(outpost);
      return true;
    }
  }
  return false;
}

// Ticks `world` until village `target` lies in ruins, for at most `limit`
// ticks, noting in `result` the ticks run, each move of the objective of the
// band, the world's last army, and whether it moved while a foe a member saw
// as the tick began still lived.
void watch_raid(World& world, std::size_t target, Raided& result,
                std::uint64_t limit) {
  const std::size_t band = world.army_count() - 1;
  for (; result.ticks < limit &&
         world.village_state(target) != VillageState::RUINS;
       ++result.ticks) {
    std::vector<std::size_t> seen;
    for (const std::size_t member : world.roster(band)) {
      const std::vector<std::size_t> foes = world.foes_in_sight(member);
      seen.insert(seen.end(), foes.begin(), foes.end());
    }
    const Cell before = world.objective(band);
    world.tick();
    const Cell after = world.objective(band);
    if (after.x != before.x || after.y != before.y) {
      ++result.moves;
      result.moved_past_foe =
          result.moved_past_foe ||
          std::any_of(seen.begin(), seen.end(), [&world](std::size_t foe) {
            return world.person(foe).health > 0;
          });
    }
  }
}

// With `outpost` above 0, the village raided holds one more structure on its
// edge, at least `outpost` cells from its centre.
Raided raided(const RegionData& data, Seed seed, const Raid& raid,
              std::uint64_t limit, bool bound = true, bool sighted = true,
              std::int32_t outpost = 0) {
  Region region = generate_region(seed, data);
  if (!sighted) {
    region.rules.sight = 0;
  }
  const std::size_t target = first_raided(region, raid.kind);
  const Village& village = region.villages.at(target);
  const std::size_t residents = village.army;
  const Cell centre = region.armies.at(residents).objective;
  ArmySpec raiders{centre, {}, DEMONIC, data.factions.at(DEMONIC).skills};
  // The band stands on the open cells round the village two cells clear of
  // everything it holds, its square and its structures, nearest its centre
  // by path round what blocks, so it starts on the near side of any water
  // or range rather than behind another village.
  std::int32_t left = village.corner.x;
  std::int32_t top = village.corner.y;
  std::int32_t right = village.corner.x + village.area.width;
  std::int32_t bottom = village.corner.y + village.area.height;
  std::vector<std::string> rows = region.map_rows;
  for (const std::size_t index : village.structures) {
    const Structure& structure = region.structures.at(index);
    left = std::min(left, structure.cell.x);
    top = std::min(top, structure.cell.y);
    right = std::max(right, structure.cell.x + structure.footprint.width);
    bottom = std::max(bottom, structure.cell.y + structure.footprint.height);
  }
  for (const Structure& structure : region.structures) {
    for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
      if (structure.blocks) {
        rows.at(static_cast<std::size_t>(cell.y))
            .at(static_cast<std::size_t>(cell.x)) = tile_of(Footing::BLOCKED);
      }
    }
  }
  const FlowField paths{Map{rows}, centre};
  constexpr std::int32_t CLEAR = 2;
  std::vector<Cell> cells = open_round(region, village);
  cells.erase(std::remove_if(cells.begin(), cells.end(),
                             [&](Cell cell) {
                               const bool near = cell.x >= left - CLEAR &&
                                                 cell.x < right + CLEAR &&
                                                 cell.y >= top - CLEAR &&
                                                 cell.y < bottom + CLEAR;
                               return near || paths.distance(cell) ==
                                                  FlowField::UNREACHABLE;
                             }),
              cells.end());
  std::stable_sort(cells.begin(), cells.end(), [&paths](Cell one, Cell other) {
    return paths.distance(one) < paths.distance(other);
  });
  for (const Cell cell : cells) {
    if (raiders.members.size() < raid.band) {
      raiders.members.push_back(Recruit{cell, data.life.newborn_health});
    }
  }
  raiders.objective = bound ? centre : raiders.members.front().cell;
  region.armies.push_back(raiders);
  // After the band, which stands round the village as it was laid.
  const bool placed =
      outpost > 0 && add_outpost(region, region.villages.at(target), outpost);
  World world = world_of(region);
  // Setup: the whole band against a village that stands.
  EXPECT_EQ(world.roster(region.armies.size() - 1).size(), raid.band);
  EXPECT_TRUE(std::all_of(village.structures.begin(), village.structures.end(),
                          [&world](std::size_t index) {
                            return world.stage(index) == Stage::INTACT;
                          }));
  Raided result{0, 0, placed};
  watch_raid(world, target, result, limit);
  result.residents = world.roster(residents).size();
  return result;
}

// base_data() with every village starting as full as a grown one, as the
// combat targets and caps are measured against: the hamlet with 6
// households, 12 people, and the walled village with 13, 26 people.
RegionData grown() {
  RegionData data = base_data();
  for (VillageTemplate& kind : data.villages) {
    const std::uint32_t households = kind.name == "village" ? 13 : 6;
    for (Building& building : kind.buildings) {
      building.households = building.households > 0 ? households : 0;
    }
  }
  return data;
}

// base_data() with every weapon family's strike, the basic strike, doing
// `damage` to a person, whatever weapon strikes it.
RegionData striking(std::uint64_t damage) {
  RegionData data = base_data();
  Gear& gear = data.life.gear.value();
  for (WeaponFamily& family : gear.families) {
    family.strike.body_damage = damage;
  }
  for (ItemKind& kind : gear.kinds) {
    kind.damage.reset();
  }
  return data;
}

TEST(Region, ADuelEndsWithinItsTargetTime) {
  for (const Seed seed : PACED) {
    EXPECT_TRUE(within(duel_ticks(base_data(), seed, DUEL_MOST + 1), DUEL_LEAST,
                       DUEL_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: the placeholder strike of 1 leaves the duel fought past it.
  EXPECT_GT(duel_ticks(striking(1), SEED, DUEL_MOST + 1), DUEL_MOST);
  // Control: beside the leader on its village's square, the challenger
  // settles the duel on seed 24 before the target's floor, so the walk-in
  // the test places is part of what it measures.
  EXPECT_LT(duel_ticks(base_data(), SEED, DUEL_MOST + 1, true), DUEL_LEAST);
}

TEST(Region, ADuelIsFoughtAtThePaceItIsWithoutTokens) {
  for (const Seed seed : PACED) {
    EXPECT_EQ(
        duel_ticks(base_data(), seed, DUEL_MOST + 1),
        duel_ticks(base_data(), seed, DUEL_MOST + 1, false, TokenGate::NONE))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: a skirmish, whose strikes tokens do limit, is fought at another
  // pace with them than without.
  EXPECT_NE(skirmish_ticks(base_data(), SEED, SKIRMISH_MOST + 1),
            skirmish_ticks(base_data(), SEED, SKIRMISH_MOST + 1, false));
}

TEST(Region, ASkirmishOfTwentyASideEndsWithinItsTargetTime) {
  for (const Seed seed : PACED) {
    EXPECT_TRUE(within(skirmish_ticks(base_data(), seed, SKIRMISH_MOST + 1),
                       SKIRMISH_LEAST, SKIRMISH_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: the placeholder strike of 1 leaves both lines standing past it.
  EXPECT_GT(skirmish_ticks(striking(1), SEED, SKIRMISH_MOST + 1),
            SKIRMISH_MOST);
}

// The health an Orthodox person, newborn, loses over a second to a Demonic
// one standing `apart` cells west of it on an open field of the region SEED
// generates, each with its faction's strike alone, where no one closes on a
// foe in sight.
std::uint64_t struck_from(std::int32_t apart) {
  constexpr std::uint64_t SECOND = 30;
  const RegionData& data = base_data();
  Region region = generate_region(SEED, data);
  region.rules.sight = 0;
  // Tokens decide who strikes, not how far; with no sight nobody engages, so
  // nobody would hold one.
  region.rules.tokens.reset();
  const Cell corner = open_field(region).value_or(Cell{0, 0});
  const Cell west{corner.x + 1, corner.y + 2};
  const Cell east{west.x + apart, west.y};
  const std::uint64_t health = data.life.newborn_health;
  const std::size_t first = region.armies.size();
  // Each strikes only with the basic strike of the weapon it is born with.
  region.armies.push_back(ArmySpec{east, {Recruit{east, health}}, 0, {}});
  region.armies.push_back(ArmySpec{west, {Recruit{west, health}}, DEMONIC, {}});
  World world = world_of(region);
  const std::size_t orthodox = world.roster(first).front();
  for (std::uint64_t tick = 0; tick < SECOND; ++tick) {
    world.tick();
  }
  return health - world.person(orthodox).health;
}

TEST(Region, AFactionsStrikeReachesAFoeAtArmsLengthAndNoFarther) {
  EXPECT_GT(struck_from(1), 0U);
  // Control: a foe two cells away stands out of reach.
  EXPECT_EQ(struck_from(2), 0U);
}

TEST(Region, ARaidLeavesAHamletInRuinsWithinItsTargetTime) {
  for (const Seed seed : PACED) {
    EXPECT_TRUE(within(raided(grown(), seed, HAMLET_RAID, RAID_MOST + 1).ticks,
                       RAID_LEAST, RAID_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: the placeholder torch of a tenth the damage leaves the hamlet
  // standing past it.
  RegionData slow = grown();
  Skill& torch = slow.factions.at(DEMONIC).skills.back();
  constexpr std::int32_t SLOWER = 10;
  torch.structure_damage /= SLOWER;
  EXPECT_GT(raided(slow, SEED, HAMLET_RAID, RAID_MOST + 1).ticks, RAID_MOST);
}

// How far from a raided village's centre an outpost stands: past any blow
// struck from round the centre, and within the village's claim.
constexpr std::int32_t OUTPOST = 15;

TEST(Region, ARaidBandGoesForWhatStandsBeyondItsCentresReach) {
  const Raided far =
      raided(grown(), SEED, HAMLET_RAID, RAID_MOST + 1, true, true, OUTPOST);
  // Setup: the outpost stands, past the torch's reach from round the centre.
  ASSERT_TRUE(far.outpost_placed);
  // The band, its residents gone, is ordered on to the outpost once, and
  // ruins the hamlet within the raid's target time, never moving while a foe
  // it saw lives.
  EXPECT_LE(far.ticks, RAID_MOST);
  EXPECT_EQ(far.moves, 1U);
  EXPECT_FALSE(far.moved_past_foe);
  // Control: without the outpost the band ruins the hamlet from round its
  // centre, its objective never moving.
  const Raided near = raided(grown(), SEED, HAMLET_RAID, RAID_MOST + 1);
  EXPECT_LE(near.ticks, RAID_MOST);
  EXPECT_EQ(near.moves, 0U);
}

TEST(Region, ARaidLeavesTheWalledVillageInRuinsWithinItsTargetTime) {
  for (const Seed seed : WALLED) {
    EXPECT_TRUE(
        within(raided(grown(), seed, VILLAGE_RAID, VILLAGE_RAID_MOST + 1).ticks,
               RAID_LEAST, VILLAGE_RAID_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
}

TEST(Region, ABandTheWalledVillageOutnumbersStillClosesToStrike) {
  // The hamlet's band of RAIDERS, which the village's 26 residents outnumber,
  // passes its walls and strikes its residents down.
  constexpr Raid OUTNUMBERED{"village", RAIDERS};
  const Region region = generate_region(SEED, grown());
  const std::size_t residents =
      region.armies.at(region.villages.at(first_raided(region, "village")).army)
          .members.size();
  ASSERT_EQ(residents, 26U);
  EXPECT_LT(raided(grown(), SEED, OUTNUMBERED, SKIRMISH_MOST).residents,
            residents);
  // Control: the same band kept where it stands, where no one closes on a
  // foe in sight, strikes no one.
  EXPECT_EQ(
      raided(grown(), SEED, OUTNUMBERED, SKIRMISH_MOST, false, false).residents,
      residents);
}

// Years a generated region lives through in the kinship tests.
constexpr std::uint64_t GENERATIONS_YEARS = 30;

// How a region lived: the most living at once, and the people it held.
struct Lived {
  std::size_t peak;
  std::vector<Person> people;
  std::vector<std::optional<Name>> names;
};

// The region `data` generates from `seed`, lived through `years` years with
// days of one tick, so that every life rule, counted in days, runs as in
// play while the test stays short.
Lived lived(const RegionData& data, Seed seed, std::uint64_t years) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = 1;
  World world = world_of(region);
  std::size_t peak = world.living();
  for (std::uint64_t day = 0; day < years * DAYS_PER_YEAR; ++day) {
    world.tick();
    peak = std::max(peak, world.living());
  }
  Lived result{peak, {}, {}};
  for (std::size_t person = 0; person < world.population(); ++person) {
    result.people.push_back(world.person(person));
    result.names.push_back(world.name(person));
  }
  return result;
}

// Each person's generation: 1 for those the world starts with, and one more
// than its eldest parent's for everyone born since.
std::vector<int> generations(const std::vector<Person>& people) {
  std::vector<int> generation(people.size(), 1);
  for (std::size_t person = 0; person < people.size(); ++person) {
    for (const std::size_t parent : people.at(person).parents) {
      generation.at(person) =
          std::max(generation.at(person), generation.at(parent) + 1);
    }
  }
  return generation;
}

// Whether every child was born to a married couple of a man and a woman, in
// its father's family or, once it founds a village, a branch bearing its
// surname.
bool born_in_wedlock(const Lived& lived) {
  const std::vector<Person>& people = lived.people;
  const auto surname = [&lived](std::size_t person) {
    return lived.names.at(person).value_or(Name{}).surname.romanised;
  };
  for (std::size_t index = 0; index < people.size(); ++index) {
    const Person& child = people.at(index);
    if (child.parents.size() != 2 || child.soul) {
      if (!child.parents.empty() && !child.soul) {
        return false;
      }
      continue;
    }
    const std::size_t father_index = child.parents.front();
    const Person& father = people.at(father_index);
    const Person& mother = people.at(child.parents.back());
    const bool kin = child.family == father.family ||
                     surname(index) == surname(father_index);
    if (father.sex != Sex::MALE || mother.sex != Sex::FEMALE ||
        std::count(mother.spouses.begin(), mother.spouses.end(),
                   father_index) != 1 ||
        !kin || father.family == mother.family) {
      return false;
    }
  }
  return true;
}

TEST(Region, FamiliesReachAThirdGenerationWithinThirtyYears) {
  const Lived region = lived(base_data(), SEED, GENERATIONS_YEARS);
  const std::vector<int> generation = generations(region.people);
  EXPECT_GE(*std::max_element(generation.begin(), generation.end()), 3);
  EXPECT_LE(region.peak, MAX_PEOPLE);
  EXPECT_TRUE(born_in_wedlock(region));
  // Control: without kinship no one marries but the households the world
  // starts with, and the world starts with the only generation it has.
  RegionData unwed = base_data();
  unwed.life.kinship.reset();
  const Lived control = lived(unwed, SEED, GENERATIONS_YEARS);
  const std::vector<int> only = generations(control.people);
  EXPECT_EQ(*std::max_element(only.begin(), only.end()), 1);
  std::size_t founders_wed = 0;
  for (const Village& village : generate_region(SEED, unwed).villages) {
    founders_wed += std::size_t{2} * village.households;
  }
  ASSERT_GT(founders_wed, 0U);
  EXPECT_EQ(std::count_if(
                control.people.begin(), control.people.end(),
                [](const Person& person) { return !person.spouses.empty(); }),
            static_cast<std::ptrdiff_t>(founders_wed));
}

TEST(Region, BirthsStopOnceMaxPeopleLive) {
  // Everyone free marries and every wife bears a child each day she may,
  // with no village's economy to cap them, from villages as full as grown
  // ones.
  RegionData fertile = grown();
  fertile.life.husbandry.reset();
  ASSERT_TRUE(fertile.life.kinship.has_value());
  Kinship kinship = fertile.life.kinship.value_or(Kinship{});
  kinship.marriage_chance = PER_MILLE;
  kinship.birth_chance = PER_MILLE;
  kinship.birth_spacing = 0;
  fertile.life.kinship = kinship;
  const Lived crowded = lived(fertile, SEED, GENERATIONS_YEARS);
  // The bound was met and never passed.
  EXPECT_EQ(crowded.peak, MAX_PEOPLE);
  // Control: the game's own odds leave the region below the bound.
  EXPECT_LT(lived(grown(), SEED, GENERATIONS_YEARS).peak, MAX_PEOPLE);
}

TEST(Region, BirthsStopAtTheCapacityTheRulesSet) {
  // As fertile as above, for a few years under a capacity a little above the
  // founders.
  RegionData fertile = grown();
  fertile.life.husbandry.reset();
  ASSERT_TRUE(fertile.life.kinship.has_value());
  Kinship kinship = fertile.life.kinship.value_or(Kinship{});
  kinship.marriage_chance = PER_MILLE;
  kinship.birth_chance = PER_MILLE;
  kinship.birth_spacing = 0;
  fertile.life.kinship = kinship;
  constexpr std::uint64_t YEARS = 5;
  constexpr std::size_t ROOM = 20;
  const auto peak = [&fertile](std::optional<std::size_t> room) {
    Region region = generate_region(SEED, fertile);
    region.rules.ticks_per_day = 1;
    std::size_t founders = 0;
    for (const ArmySpec& army : region.armies) {
      founders += army.members.size();
    }
    if (room) {
      region.rules.people_capacity = founders + *room;
    }
    World world = world_of(region);
    std::size_t most = world.living();
    for (std::uint64_t day = 0; day < YEARS * DAYS_PER_YEAR; ++day) {
      world.tick();
      most = std::max(most, world.living());
    }
    return std::pair<std::size_t, std::size_t>{founders, most};
  };
  const auto [founders, most] = peak(ROOM);
  // The bound was met and never passed.
  EXPECT_EQ(most, founders + ROOM);
  // Control: under the default capacity the same years pass it.
  EXPECT_GT(peak(std::nullopt).second, founders + ROOM);
}

TEST(Region, EveryChildBornHasRoomForAnAttackSlot) {
  // As fertile as above, a tick a day, until the first child is born.
  RegionData fertile = grown();
  fertile.life.husbandry.reset();
  ASSERT_TRUE(fertile.life.kinship.has_value());
  Kinship kinship = fertile.life.kinship.value_or(Kinship{});
  kinship.marriage_chance = PER_MILLE;
  kinship.birth_chance = PER_MILLE;
  kinship.birth_spacing = 0;
  fertile.life.kinship = kinship;
  Region region = generate_region(SEED, fertile);
  region.rules.ticks_per_day = 1;
  World world = world_of(region);
  const std::size_t founders = world.population();
  ASSERT_EQ(world.attacks().people(), founders);
  for (std::uint64_t day = 0;
       day < DAYS_PER_YEAR && world.population() == founders; ++day) {
    world.tick();
  }

  // Setup: a child was born, not a soul born again.
  ASSERT_GT(world.population(), founders);
  ASSERT_FALSE(world.person(founders).parents.empty());
  ASSERT_FALSE(world.person(founders).soul.has_value());
  EXPECT_EQ(world.attacks().people(), world.population());
}

// Whether every child born to a couple bears its father's surname.
bool fathers_surnames(const Lived& region) {
  for (std::size_t person = 0; person < region.people.size(); ++person) {
    const std::vector<std::size_t>& parents = region.people.at(person).parents;
    const bool born_to_couple =
        parents.size() == 2 && !region.people.at(person).soul;
    if (born_to_couple &&
        region.names.at(person).value_or(Name{}).surname.korean !=
            region.names.at(parents.front()).value_or(Name{}).surname.korean) {
      return false;
    }
  }
  return true;
}

TEST(Region, EveryPersonBearsTheirFamilySurnameAndAGivenName) {
  const Lived region = lived(base_data(), SEED, GENERATIONS_YEARS);
  const auto named = [](const std::optional<Name>& name) {
    return name && !name->surname.korean.empty() &&
           !name->surname.romanised.empty() && !name->given.korean.empty() &&
           !name->given.romanised.empty();
  };
  EXPECT_TRUE(std::all_of(region.names.begin(), region.names.end(), named));
  // A child bears its father's surname.
  EXPECT_TRUE(fathers_surnames(region));
  // The same seed names everyone alike; control: another seed does not.
  const auto spelled = [](const Lived& from) {
    std::vector<std::string> spellings;
    for (const std::optional<Name>& name : from.names) {
      const Name each = name.value_or(Name{});
      spellings.push_back(each.surname.romanised + " " + each.given.romanised);
    }
    return spellings;
  };
  EXPECT_EQ(spelled(lived(base_data(), SEED, 0)),
            spelled(lived(base_data(), SEED, 0)));
  EXPECT_NE(spelled(lived(base_data(), SEED, 0)),
            spelled(lived(base_data(), OTHER_SEED, 0)));
}

// The most people above its cap any village of the region `data` generates
// from NEIGHBOURLY ever held, measured against `capping`'s caps, and the most
// people its villages together held, over GENERATIONS_YEARS years of one-tick
// days.
std::pair<std::int64_t, std::size_t> crowding(const RegionData& data,
                                              const RegionData& capping) {
  Region region = generate_region(NEIGHBOURLY, data);
  region.rules.ticks_per_day = 1;
  World world = world_of(region);
  const Region capped_region = generate_region(NEIGHBOURLY, capping);
  const World capped = world_of(capped_region);
  std::int64_t over = std::numeric_limits<std::int64_t>::min();
  std::size_t most = 0;
  for (std::uint64_t day = 0; day < GENERATIONS_YEARS * DAYS_PER_YEAR; ++day) {
    world.tick();
    std::size_t together = 0;
    for (std::size_t village = 0; village < region.villages.size(); ++village) {
      const std::size_t held = world.residents_of(village);
      together += held;
      over = std::max(over, static_cast<std::int64_t>(held) -
                                static_cast<std::int64_t>(capped.cap(village)));
    }
    most = std::max(most, together);
  }
  return {over, most};
}

TEST(Region, VillagesFillToTheirCapsAndNoFurther) {
  // Everyone free marries and every wife may bear a child each day. Growth,
  // which raises the caps measured against, is left out, and the villages
  // start as full as grown ones.
  RegionData fertile = grown();
  for (Faction& faction : fertile.factions) {
    faction.stance.clear();
  }
  ASSERT_TRUE(fertile.life.kinship.has_value());
  Kinship kinship = fertile.life.kinship.value_or(Kinship{});
  kinship.marriage_chance = PER_MILLE;
  kinship.birth_chance = PER_MILLE;
  kinship.birth_spacing = 0;
  fertile.life.kinship = kinship;
  const Region region = generate_region(NEIGHBOURLY, fertile);
  const World start = world_of(region);
  std::size_t caps = 0;
  for (std::size_t village = 0; village < region.villages.size(); ++village) {
    caps += start.cap(village);
  }
  const auto [over, most] = crowding(fertile, fertile);
  // No village ever passes its cap, and together they come within a tenth
  // of their caps.
  EXPECT_LE(over, 0);
  EXPECT_GE(most * 10, caps * 9);
  // Control: without the economy the same villages pass those caps.
  RegionData unfed = fertile;
  unfed.life.husbandry.reset();
  EXPECT_GT(crowding(unfed, fertile).first, 0);
}

// How a region grew over some years, GROWTH_YEARS unless a test says: its
// villages and territory at the start and the end, its armies at the end,
// the most living at once, whether a founded village came to house its
// settlers and a building was raised, the kinds of those raised, and the
// hash.
struct Grew {
  std::size_t villages_before;
  std::size_t villages_after;
  std::size_t armies_after;
  std::vector<std::optional<std::size_t>> territory_before;
  std::vector<std::optional<std::size_t>> territory_after;
  std::size_t peak;
  bool founded_village_housed;
  bool building_raised;
  StateHash hash;
  std::vector<std::string> raised;
};

constexpr std::uint64_t GROWTH_YEARS = 50;
// Days of five ticks: marching, fighting and raising run far faster against
// the calendar than in play, so fifty years see many growth checks resolve.
constexpr std::uint64_t GROWTH_TICKS_PER_DAY = 5;

Grew grew(const RegionData& data, Seed seed,
          std::uint64_t years = GROWTH_YEARS) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = GROWTH_TICKS_PER_DAY;
  World world = world_of(region);
  Grew result{world.village_count(), 0,     0,     world.territory(), {},
              world.living(),        false, false, StateHash{},       {}};
  for (std::uint64_t tick = 0;
       tick < years * DAYS_PER_YEAR * GROWTH_TICKS_PER_DAY; ++tick) {
    world.tick();
    result.peak = std::max(result.peak, world.living());
  }
  result.villages_after = world.village_count();
  result.armies_after = world.army_count();
  result.territory_after = world.territory();
  for (std::size_t village = result.villages_before;
       village < result.villages_after; ++village) {
    result.founded_village_housed =
        result.founded_village_housed || world.residents_of(village) > 0;
  }
  const Growth growth = data.life.growth.value_or(Growth{});
  const std::size_t builds = growth.builds.size();
  for (std::size_t structure = region.structures.size();
       structure < world.structure_count(); ++structure) {
    const std::size_t plan = world.plan_of(structure).value_or(builds);
    if (plan < builds && world.stage(structure) == Stage::INTACT) {
      result.building_raised = true;
      result.raised.push_back(growth.plans.at(plan).kind);
    }
  }
  result.hash = world.hash();
  return result;
}

// Whether a Demonic village of `region` has a village of a faction it is
// hostile to with its centre within the growth's reach along each axis, as
// a raid needs.
bool raid_in_reach(const Region& region, const RegionData& data) {
  const std::int32_t reach = data.life.growth.value_or(Growth{}).reach;
  for (const Village& raider : region.villages) {
    if (raider.faction != DEMONIC) {
      continue;
    }
    const Cell origin = region.armies.at(raider.army).objective;
    for (const Village& target : region.villages) {
      const Cell reached = region.armies.at(target.army).objective;
      if (region.rules.hostile.at(DEMONIC).at(target.faction) &&
          std::abs(reached.x - origin.x) <= reach &&
          std::abs(reached.y - origin.y) <= reach) {
        return true;
      }
    }
  }
  return false;
}

// The seed the fifty years of growth are measured on: the first from 0 on
// which a Demonic village has a hostile one within raiding reach, so a raid
// can be sent, and on which every outcome the test names occurs.
constexpr Seed RAIDING{1};

TEST(Region, VillagesBuildFoundAndRaidOverFiftyYearsWithinTheBound) {
  // Setup: the seed holds a raid within reach, the property it was chosen
  // for, so a layout that loses it fails here rather than below.
  ASSERT_TRUE(
      raid_in_reach(generate_region(RAIDING, base_data()), base_data()));
  // Control: seed 0, the one before it, holds none.
  ASSERT_FALSE(
      raid_in_reach(generate_region(Seed{0}, base_data()), base_data()));
  const Grew region = grew(base_data(), RAIDING);
  // Villages were founded, and one came to house its settlers; a building
  // was raised; a raiding party, an army housing no village, was sent; the
  // factions' territories changed; the living stayed within the bound.
  EXPECT_GT(region.villages_after, region.villages_before);
  EXPECT_GT(region.armies_after, region.villages_after);
  EXPECT_TRUE(region.founded_village_housed);
  EXPECT_TRUE(region.building_raised);
  EXPECT_NE(region.territory_after, region.territory_before);
  EXPECT_LE(region.peak, MAX_PEOPLE);
  // The same seed grows the same way; control: another seed does not.
  EXPECT_EQ(static_cast<std::uint64_t>(grew(base_data(), RAIDING).hash),
            static_cast<std::uint64_t>(region.hash));
  EXPECT_NE(static_cast<std::uint64_t>(grew(base_data(), OTHER_SEED).hash),
            static_cast<std::uint64_t>(region.hash));
  // Control: with no faction taking a stance, no village is founded.
  RegionData still = base_data();
  for (Faction& faction : still.factions) {
    faction.stance.clear();
  }
  const Grew unchanged = grew(still, RAIDING);
  EXPECT_EQ(unchanged.villages_after, unchanged.villages_before);
  EXPECT_EQ(unchanged.armies_after, unchanged.villages_after);
  EXPECT_FALSE(unchanged.building_raised);
}

// What the rosters of a region went through over a run, and whether at the
// end of every day each person's army and faction read back as the army
// whose roster held it and its faction, or as none for a person on no roster.
struct Rostering {
  bool agreed = true;
  bool conscripted = false;
  std::size_t born = 0;
  std::size_t reborn = 0;
  std::size_t died = 0;
  std::size_t formed = 0;
};

// Every tenth founder carries a soul, so deaths bring rebirths.
constexpr std::size_t ENSOULED_EVERY = 10;

// Conscripts person 0 of `world` into the first army of another faction.
// Returns whether it then serves in that army.
bool conscripted_abroad(World& world) {
  const std::optional<std::size_t> own = world.faction_of(0);
  for (std::size_t army = 0; own && army < world.army_count(); ++army) {
    if (world.faction(army) != *own) {
      world.conscript(0, army);
      return world.army_of(0) == army;
    }
  }
  return false;
}

// The army whose roster holds each person of `world`, or none.
std::vector<std::optional<std::size_t>> rostered_in(const World& world) {
  std::vector<std::optional<std::size_t>> rostered(world.population());
  for (std::size_t army = 0; army < world.army_count(); ++army) {
    for (const std::size_t member : world.roster(army)) {
      rostered.at(member) = army;
    }
  }
  return rostered;
}

// Notes in `seen` whether each person of `world` serves in the army `now`
// names, of its faction, and how the rosters changed since `before`, when
// the world held `armies` armies.
void tally(const World& world,
           const std::vector<std::optional<std::size_t>>& before,
           const std::vector<std::optional<std::size_t>>& now,
           std::size_t armies, Rostering& seen) {
  for (std::size_t person = 0; person < now.size(); ++person) {
    const std::optional<std::size_t> army = now.at(person);
    const std::optional<std::size_t> faction =
        army ? std::optional<std::size_t>{world.faction(*army)} : std::nullopt;
    seen.agreed = seen.agreed && world.army_of(person) == army &&
                  world.faction_of(person) == faction;
    if (person >= before.size()) {
      seen.born += army ? std::size_t{1} : std::size_t{0};
      seen.reborn +=
          army && world.person(person).soul ? std::size_t{1} : std::size_t{0};
    } else if (before.at(person) && !army) {
      ++seen.died;
    } else if (before.at(person) != army && army && *army >= armies) {
      ++seen.formed;
    }
  }
}

// The region of seed RAIDING over `years` years of GROWTH_TICKS_PER_DAY-tick
// days, its first person conscripted into an army of another faction.
Rostering rostered_over(std::uint64_t years) {
  Region region = generate_region(RAIDING, base_data());
  region.rules.ticks_per_day = GROWTH_TICKS_PER_DAY;
  World world = world_of(region);
  for (std::size_t person = 0; person < world.population();
       person += ENSOULED_EVERY) {
    world.ensoul(person);
  }
  Rostering seen;
  seen.conscripted = conscripted_abroad(world);
  std::vector<std::optional<std::size_t>> before;
  std::size_t armies = world.army_count();
  for (std::uint64_t day = 0; day <= years * DAYS_PER_YEAR; ++day) {
    std::vector<std::optional<std::size_t>> now = rostered_in(world);
    tally(world, before, now, armies, seen);
    before = std::move(now);
    armies = world.army_count();
    for (std::uint64_t tick = 0; tick < GROWTH_TICKS_PER_DAY; ++tick) {
      world.tick();
    }
  }
  return seen;
}

TEST(Region, EveryPersonServesInTheArmyWhoseRosterHoldsItThroughFiftyYears) {
  const Rostering seen = rostered_over(GROWTH_YEARS);
  // Setup: the person conscripted serves in its new army, and people were
  // born, born again from a soul, died, and moved into armies formed as the
  // run went. World's tests cover recall and migration, which this run does
  // not reach.
  ASSERT_TRUE(seen.conscripted);
  ASSERT_GT(seen.born, 0U);
  ASSERT_GT(seen.reborn, 0U);
  ASSERT_GT(seen.died, 0U);
  ASSERT_GT(seen.formed, 0U);
  EXPECT_TRUE(seen.agreed);
}

// The workshops one village raises in the region of seed NEIGHBOURLY within
// WORKSHOP_YEARS years of GROWTH_TICKS_PER_DAY-tick days, in the order laid.
// The village is its first Orthodox one, a seat aside. All its housing, food
// and storage are put in its first structure, enough that its cap is `cap`, and
// people whose home it is are added until they press against that cap.
// `set_up` is whether its cap came out as asked, its people pressed against
// it before it laid a workshop, and it held none.
struct Workshops {
  bool set_up;
  std::vector<std::string> raised;
};

// The region's first Orthodox village, a seat aside.
std::size_t first_orthodox(const Region& region, const RegionData& data) {
  for (std::size_t village = 0; village < region.villages.size(); ++village) {
    const Village& candidate = region.villages.at(village);
    if (candidate.faction == 0 && candidate.kind != data.seat.name) {
      return village;
    }
  }
  return region.villages.size();
}

// Puts all of village `village`'s housing, food and storage in its first
// structure, enough for `cap` people, so its cap is exactly `cap`.
void hold_in_first(Region& region, std::size_t village, const RegionData& data,
                   std::uint32_t cap) {
  const std::uint64_t yearly =
      std::uint64_t{data.life.husbandry.value_or(Husbandry{}).consumption} *
      DAYS_PER_YEAR;
  const std::vector<std::size_t>& own = region.villages.at(village).structures;
  for (const std::size_t structure : own) {
    Holding& holding = region.holdings.at(structure);
    const bool first = structure == own.front();
    holding.housing = first ? cap : 0;
    holding.harvest = first ? static_cast<std::uint32_t>(cap * yearly) : 0;
    holding.storage = first ? static_cast<std::uint32_t>(cap * yearly) : 0;
  }
}

constexpr std::uint64_t WORKSHOP_YEARS = 40;

Workshops workshops_raised(std::uint32_t cap) {
  // No other faction grows, so no raid ruins the village the test watches.
  RegionData data = base_data();
  for (std::size_t faction = 1; faction < data.factions.size(); ++faction) {
    data.factions.at(faction).stance.clear();
  }
  Region region = generate_region(NEIGHBOURLY, data);
  region.rules.ticks_per_day = GROWTH_TICKS_PER_DAY;
  const Growth growth = data.life.growth.value_or(Growth{});
  const std::size_t chosen = first_orthodox(region, data);
  hold_in_first(region, chosen, data, cap);
  const std::vector<std::size_t>& own = region.villages.at(chosen).structures;
  const bool lacks_workshops =
      std::none_of(own.begin(), own.end(), [&region](std::size_t structure) {
        return region.holdings.at(structure).arms > 0 ||
               region.holdings.at(structure).tends > 0;
      });
  // Enough people whose home it is, beside its residents, to press a cap of
  // `cap`: an army of them appended, so no person's index moves.
  const ArmySpec& residents = region.armies.at(region.villages.at(chosen).army);
  const auto pressing = static_cast<std::size_t>(
      ((std::uint64_t{cap} * growth.pressure) + PER_MILLE - 1) / PER_MILLE);
  ArmySpec homed{
      residents.objective, {}, residents.faction, residents.skills, chosen};
  while (residents.members.size() + homed.members.size() < pressing) {
    homed.members.push_back(residents.members.front());
  }
  region.armies.push_back(homed);
  World world = world_of(region);
  const bool capped = world.cap(chosen) == cap;
  // A workshop the village laid: growth lays its buildings within the
  // vicinity of its seat, its residents' objective, on each axis, at most the
  // last tier's.
  const Cell seat = residents.objective;
  const std::int32_t vicinity = growth.tiers.back().vicinity;
  const auto workshop = [&growth, &world, seat,
                         vicinity](std::size_t structure) {
    const std::size_t plan =
        world.plan_of(structure).value_or(growth.plans.size());
    const Cell cell = world.structure(structure).cell;
    return plan < growth.plans.size() &&
           (growth.plans.at(plan).holding.arms > 0 ||
            growth.plans.at(plan).holding.tends > 0) &&
           std::abs(cell.x - seat.x) <= vicinity &&
           std::abs(cell.y - seat.y) <= vicinity;
  };
  // Whether its people pressed against its cap before it laid a workshop.
  bool pressed = false;
  bool laid = false;
  for (std::uint64_t tick = 0;
       tick < WORKSHOP_YEARS * DAYS_PER_YEAR * GROWTH_TICKS_PER_DAY; ++tick) {
    world.tick();
    for (std::size_t structure = region.structures.size();
         structure < world.structure_count() && !laid; ++structure) {
      laid = workshop(structure);
    }
    pressed = pressed ||
              (!laid && std::uint64_t{world.residents_of(chosen)} * PER_MILLE >=
                            std::uint64_t{cap} * growth.pressure);
  }
  Workshops found{capped && lacks_workshops && pressed, {}};
  for (std::size_t structure = region.structures.size();
       structure < world.structure_count(); ++structure) {
    if (workshop(structure) && world.stage(structure) == Stage::INTACT) {
      found.raised.push_back(
          growth.plans.at(world.plan_of(structure).value_or(0)).kind);
    }
  }
  return found;
}

TEST(Region, VillagesPastTheirSelfRelianceRaiseASmithyAndAnApothecary) {
  const std::uint32_t reliance =
      base_data().life.growth.value_or(Growth{}).self_reliance;
  const Workshops past = workshops_raised(reliance + 4);
  // Setup: the village's cap is past its self-reliance, its people pressed
  // against it before it laid a workshop, and it held none.
  ASSERT_TRUE(past.set_up);
  // Arms are its scarcest holding, then tends: it raises a smithy, then an
  // apothecary.
  ASSERT_GE(past.raised.size(), 2U);
  EXPECT_EQ(past.raised.at(0), "smithy");
  EXPECT_EQ(past.raised.at(1), "apothecary");
  // Control: the same village with a cap of half its self-reliance, pressed
  // as hard, raises neither in as long.
  const Workshops short_of = workshops_raised(reliance / 2);
  ASSERT_TRUE(short_of.set_up);
  EXPECT_TRUE(short_of.raised.empty());
}

// The most households any faction of the region `data` generates from
// `seed` starts with.
std::uint32_t most_households(const RegionData& data, Seed seed) {
  const Region region = generate_region(seed, data);
  std::vector<std::uint32_t> households(region.leadership.size(), 0);
  for (const Village& village : region.villages) {
    households.at(village.faction) += village.households;
  }
  return *std::max_element(households.begin(), households.end());
}

TEST(Region, EachFactionStartsWithAtMostFourHouseholds) {
  constexpr std::uint32_t MOST = 4;
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    EXPECT_LE(most_households(base_data(), Seed{seed}), MOST)
        << "seed " << seed;
  }
  // Control: villages as full as grown ones start with more.
  EXPECT_GT(most_households(grown(), SEED), MOST);
}

// How a region lived through CENTURY years with days of GROWTH_TICKS_PER_DAY
// ticks: the living and the villages, founded ones included, at the start
// and the end, the most living at once, and the fewest factions holding a
// peopled village at the end of any year; whether its standing sects' elders
// were few at the end; and, checked each day, the most
// members its sects held together beyond their founders, how often a sect's
// master changed, whether the first faction's leader ever died, and how often
// its dead leader gave way to another.
struct Century {
  std::size_t living_before;
  std::size_t living_after;
  std::size_t villages_before;
  std::size_t villages_after;
  std::size_t peak;
  std::size_t fewest_factions;
  std::size_t admitted;
  std::size_t master_changes;
  std::size_t successions;
  bool leader_died;
  /// Whether, at the end, every standing sect has at least one elder and at
  /// most a third of its members, or one, as elders.
  bool elders_few;
};

constexpr std::uint64_t CENTURY = 100;

// What a century run watches of its sects day by day: each sect's last
// master, the first faction's last leader, and how many founders the sects
// started with.
struct SectWatch {
  std::vector<std::optional<std::size_t>> masters;
  std::optional<std::size_t> leader;
  std::size_t founders;
};

// Counts, into `result`, today's members beyond the founders, each sect whose
// master changed, whether the first faction's leader has died, and its giving
// way after dying.
void watch_sects(const World& world, SectWatch& watch, Century& result) {
  std::size_t members = 0;
  for (std::size_t sect = 0; sect < world.sect_count(); ++sect) {
    members += world.sect_members(sect).size();
    const std::optional<std::size_t> master = world.sect_master(sect);
    std::optional<std::size_t>& last = watch.masters.at(sect);
    if (master && last && *master != *last) {
      ++result.master_changes;
    }
    last = master ? master : last;
  }
  result.admitted =
      std::max(result.admitted, members - std::min(members, watch.founders));
  if (watch.leader && world.person(*watch.leader).health == 0) {
    result.leader_died = true;
  }
  if (world.leader(0) != watch.leader) {
    if (watch.leader && world.person(*watch.leader).health == 0) {
      ++result.successions;
    }
    watch.leader = world.leader(0);
  }
}

Century century(const RegionData& data, Seed seed) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = GROWTH_TICKS_PER_DAY;
  World world = world_of(region);
  const std::size_t count = region.rules.hostile.size();
  const auto peopled = [&world, count]() {
    std::vector<bool> factions(count, false);
    for (std::size_t village = 0; village < world.village_count(); ++village) {
      if (world.residents_of(village) > 0) {
        factions.at(world.owner(village)) = true;
      }
    }
    return static_cast<std::size_t>(
        std::count(factions.begin(), factions.end(), true));
  };
  SectWatch watch{{}, world.leader(0), 0};
  for (const SectSpec& sect : region.sects) {
    watch.founders += sect.founders.size();
  }
  for (std::size_t sect = 0; sect < world.sect_count(); ++sect) {
    watch.masters.push_back(world.sect_master(sect));
  }
  Century result{world.living(),
                 0,
                 world.village_count(),
                 0,
                 world.living(),
                 peopled(),
                 0,
                 0,
                 0,
                 false,
                 false};
  for (std::uint64_t day = 0; day < CENTURY * DAYS_PER_YEAR; ++day) {
    for (std::uint64_t tick = 0; tick < GROWTH_TICKS_PER_DAY; ++tick) {
      world.tick();
      result.peak = std::max(result.peak, world.living());
    }
    watch_sects(world, watch, result);
    if ((day + 1) % DAYS_PER_YEAR == 0) {
      result.fewest_factions = std::min(result.fewest_factions, peopled());
    }
  }
  result.living_after = world.living();
  result.villages_after = world.village_count();
  result.elders_few = true;
  for (std::size_t sect = 0; sect < world.sect_count(); ++sect) {
    const std::size_t members = world.sect_members(sect).size();
    const std::size_t elders = world.sect_elders(sect).size();
    result.elders_few =
        result.elders_few &&
        (!world.sect_stands(sect) ||
         (elders >= 1 && elders <= std::max<std::size_t>(members / 3, 1)));
  }
  return result;
}

// The region of SEED with its player born into village 0, carrying a soul
// and steered, seeing the land within `map_sight` cells, lived a year at
// days of five ticks.
World charted_lived(std::int32_t map_sight) {
  constexpr std::uint64_t TICKS_PER_SHORT_DAY = 5;
  Region region = generate_region(SEED, base_data());
  region.rules.ticks_per_day = TICKS_PER_SHORT_DAY;
  region.rules.map_sight = map_sight;
  region.armies.push_back(born_in(region, 0));
  World world = world_of(region);
  world.ensoul(world.population() - 1);
  world.steer(region.armies.size() - 1, true);
  for (std::uint64_t tick = 0; tick < DAYS_PER_YEAR * TICKS_PER_SHORT_DAY;
       ++tick) {
    world.tick();
  }
  return world;
}

// Every outcome of `world` its public reads show beside its charts: each
// person's place, health and readiness, home, family, qi and silver; each
// army's roster and objective; each structure's durability; each village's
// owner, state and residents; the events; and the date.
std::vector<std::int64_t> outcomes(const World& world) {
  std::vector<std::int64_t> seen;
  const auto add = [&seen](auto value) {
    seen.push_back(static_cast<std::int64_t>(value));
  };
  for (std::size_t index = 0; index < world.population(); ++index) {
    const Person& person = world.person(index);
    add(person.position.x);
    add(person.position.y);
    add(person.health);
    add(person.ready_at);
    add(person.home.value_or(world.village_count()));
    add(person.family);
    add(person.qi);
    add(person.silver);
  }
  for (std::size_t army = 0; army < world.army_count(); ++army) {
    for (const std::size_t member : world.roster(army)) {
      add(member);
    }
    add(world.objective(army).x);
    add(world.objective(army).y);
  }
  for (std::size_t structure = 0; structure < world.structure_count();
       ++structure) {
    add(world.structure(structure).durability);
  }
  for (std::size_t village = 0; village < world.village_count(); ++village) {
    add(world.owner(village));
    add(world.village_state(village));
    add(world.residents_of(village));
  }
  add(world.events().size());
  add(world.date().day);
  return seen;
}

TEST(Region, NoOutcomeReadsTheChart) {
  constexpr std::int32_t MAP_SIGHT = 15;
  const World charted = charted_lived(MAP_SIGHT);
  const World blind = charted_lived(0);
  // Setup: one world charted a year of what its player saw, the other none.
  ASSERT_FALSE(charted.chart(0).empty());
  ASSERT_TRUE(blind.chart(0).empty());
  // The two differ in their charts, which the hash folds...
  EXPECT_NE(charted.hash(), blind.hash());
  // ...and in nothing else.
  EXPECT_EQ(outcomes(charted), outcomes(blind));
  // Control: a day more, five ticks, is an outcome the comparison sees.
  constexpr int A_DAY = 5;
  World later = charted_lived(MAP_SIGHT);
  for (int tick = 0; tick < A_DAY; ++tick) {
    later.tick();
  }
  EXPECT_NE(outcomes(later), outcomes(charted));
}

TEST(Region, AFewHouseholdsGrowIntoACivilisationOverACentury) {
  const Century grew = century(base_data(), NEIGHBOURLY);
  // The living at least triple and the villages, founded ones included, at
  // least double; every faction keeps a peopled village, and the living stay
  // within the bound.
  EXPECT_GE(grew.living_after, 3 * grew.living_before);
  EXPECT_GE(grew.villages_after, 2 * grew.villages_before);
  EXPECT_EQ(grew.fewest_factions, base_data().factions.size());
  EXPECT_LE(grew.peak, MAX_PEOPLE);

  // Control: with no faction growing, no village is founded, although
  // births still fill the villages there are.
  RegionData still = base_data();
  for (Faction& faction : still.factions) {
    faction.stance.clear();
  }
  const Century kept = century(still, NEIGHBOURLY);
  EXPECT_LE(kept.villages_after, kept.villages_before);
  // Control: without kinship no one is born, and the living do not rise.
  RegionData unwed = base_data();
  unwed.life.kinship.reset();
  const Century barren = century(unwed, NEIGHBOURLY);
  EXPECT_LE(barren.living_after, barren.living_before);
}

// A seed whose first children were 17 daughters of 18, with sexes drawn at
// even odds: their faction's founders aged out with no son to marry them.
constexpr Seed SONLESS{9};

// base_data() with no sects, whose seats would change the region SONLESS
// was chosen on, no cultivation, whose draws at birth would change its
// people, no diplomacy, whose draws and wars would change its history, and
// no occupations, whose sellers' healing would change who lives; ADR 0027's
// rule depends on none of them.
RegionData sectless() {
  RegionData data = base_data();
  clear_sects(data);
  data.life.cultivation.reset();
  data.life.diplomacy.reset();
  data.life.occupations.reset();
  return data;
}

TEST(Region, ChildrenOfTheScarcerSexKeepEveryFactionPeopled) {
  const Century balanced = century(sectless(), SONLESS);
  EXPECT_EQ(balanced.fewest_factions, base_data().factions.size());
  EXPECT_GE(balanced.living_after, 2 * balanced.living_before);
  // Control: with sexes drawn at even odds, a faction dies out.
  RegionData drawn = sectless();
  Kinship kinship = drawn.life.kinship.value_or(Kinship{});
  kinship.balanced_sexes = false;
  drawn.life.kinship = kinship;
  const Century unbalanced = century(drawn, SONLESS);
  EXPECT_LT(unbalanced.fewest_factions, drawn.factions.size());
  EXPECT_LT(unbalanced.living_after, 2 * unbalanced.living_before);
}

// base_data() with no faction's sects admitting anyone.
RegionData closed() {
  RegionData data = base_data();
  for (Faction& faction : data.factions) {
    faction.admission.reset();
  }
  return data;
}

// What a century of sects did: the members admitted beyond their founders,
// the masters changed and the leaders elected after a death.
std::array<std::size_t, 3> sect_deeds(const Century& lived) {
  return {lived.admitted, lived.master_changes, lived.successions};
}

TEST(Region, SectsAdmitChangeMastersAndElectOverACentury) {
  const Century lived = century(base_data(), NEIGHBOURLY);
  // Setup: the Orthodox leader died within the century.
  ASSERT_TRUE(lived.leader_died);
  // Sects admit members beyond their founders, a master changes, and the
  // Orthodox, who elect, are led by another after their leader dies.
  EXPECT_GT(lived.admitted, 0U);
  EXPECT_GT(lived.master_changes, 0U);
  EXPECT_GT(lived.successions, 0U);
  EXPECT_LE(lived.peak, MAX_PEOPLE);
  // Each standing sect's elders are at least one and at most a third of its
  // members.
  EXPECT_TRUE(lived.elders_few);
  // Control: without admission no sect admits anyone, no member is an elder,
  // so no sect has a master to change or to elect. Setup: its leader died
  // too, so no election is for want of a death.
  const Century shut = century(closed(), NEIGHBOURLY);
  ASSERT_TRUE(shut.leader_died);
  EXPECT_EQ(sect_deeds(shut), (std::array<std::size_t, 3>{0, 0, 0}));
}

// The settled test map: 256 cells painted per 16, all plain but for a block
// of mountain, with a village place for each faction and a seat place for
// each sect, ADR 0076's `apart` or more from one another.
constexpr std::int32_t MAP_SIDE = 256;
constexpr std::int32_t MAP_BLOCK = 16;
constexpr std::size_t MAP_BLOCKS = std::size_t{16} * 16;
// The base pack's terrain kinds: plain first, mountain fifth.
constexpr std::uint8_t PLAIN = 0;
constexpr std::uint8_t MOUNTAIN = 4;
constexpr Cell MOUNTAIN_BLOCK{3, 3};

struct PlacedVillage {
  Cell cell;
  std::size_t owner;
  std::size_t settlement;
};

constexpr std::array<PlacedVillage, 4> VILLAGE_PLACES{{{Cell{40, 120}, 0, 1},
                                                       {Cell{120, 40}, 1, 1},
                                                       {Cell{200, 120}, 2, 0},
                                                       {Cell{120, 200}, 0, 0}}};
constexpr std::array<Cell, 3> SEAT_PLACES{
    {Cell{200, 40}, Cell{40, 200}, Cell{200, 200}}};

MapSource map_source() {
  MapSource source;
  source.side = MAP_SIDE;
  source.block = MAP_BLOCK;
  source.terrains.assign(MAP_BLOCKS, PLAIN);
  source.footings.assign(MAP_BLOCKS, Footing::BUILDABLE);
  const std::size_t mountain =
      (static_cast<std::size_t>(MOUNTAIN_BLOCK.y) *
       static_cast<std::size_t>(MAP_SIDE / MAP_BLOCK)) +
      static_cast<std::size_t>(MOUNTAIN_BLOCK.x);
  source.terrains.at(mountain) = MOUNTAIN;
  source.footings.at(mountain) = Footing::BLOCKED;
  source.levels.assign(MAP_BLOCKS, 0);
  source.lands.assign(MAP_BLOCKS, 0);
  for (const PlacedVillage& place : VILLAGE_PLACES) {
    source.places.push_back(
        Place{place.cell, VillagePlace{place.owner, place.settlement}});
  }
  for (std::size_t sect = 0; sect < SEAT_PLACES.size(); ++sect) {
    source.places.push_back(Place{SEAT_PLACES.at(sect), SeatPlace{sect}});
  }
  return source;
}

TEST(SettledMap, AWorldOnTheMapHashesTheSameTwiceAndAnotherOnAnotherMap) {
  const CompiledMap map{map_source()};
  const Region settled = settle_map(SEED, base_data(), map);
  ASSERT_TRUE(builds_a_world(settled));
  EXPECT_EQ(hash_after_ticks(settled),
            hash_after_ticks(settle_map(SEED, base_data(), map)));
  // Control: the same map with its first village a block east.
  MapSource moved = map_source();
  moved.places.front().cell.x += MAP_BLOCK;
  const Region elsewhere = settle_map(SEED, base_data(), CompiledMap{moved});
  ASSERT_TRUE(builds_a_world(elsewhere));
  EXPECT_NE(hash_after_ticks(elsewhere), hash_after_ticks(settled));
}

// Whether village `place` of `region` is laid round its place, of its
// owner and template, its square's corner round the place.
bool laid_at_its_place(const Region& region, std::size_t place) {
  const PlacedVillage& placed = VILLAGE_PLACES.at(place);
  const Village& village = region.villages.at(place);
  const Footprint square =
      region.life.growth.value_or(Growth{}).tiers.front().square;
  return same(region.armies.at(village.army).objective, placed.cell) &&
         same(village.corner, square_corner(placed.cell, square)) &&
         village.faction == placed.owner &&
         village.kind == base_data().villages.at(placed.settlement).name;
}

// Whether seat `sect` of `region` has its area's middle on its place and is
// owned by its sect's faction.
bool seated_at_its_place(const Region& region, std::size_t sect) {
  const Village& seat = region.villages.at(VILLAGE_PLACES.size() + sect);
  const Cell middle{seat.corner.x + (seat.area.width / 2),
                    seat.corner.y + (seat.area.height / 2)};
  return same(middle, SEAT_PLACES.at(sect)) &&
         seat.faction == base_data().sects.at(sect).faction;
}

TEST(SettledMap, EachVillageIsLaidOutByRoleWhereTheMapPlacesIt) {
  const Region region =
      settle_map(SEED, base_data(), CompiledMap{map_source()});
  ASSERT_EQ(region.villages.size(), VILLAGE_PLACES.size() + SEAT_PLACES.size());
  for (std::size_t place = 0; place < VILLAGE_PLACES.size(); ++place) {
    EXPECT_TRUE(laid_at_its_place(region, place)) << place;
  }
  for (std::size_t sect = 0; sect < SEAT_PLACES.size(); ++sect) {
    EXPECT_TRUE(seated_at_its_place(region, sect)) << sect;
  }
  // Their buildings stand by role on buildable cells within the vicinity,
  // and their residents on open cells, as on the region.
  EXPECT_TRUE(villages_on_buildable_cells(region));
}

TEST(SettledMap, TheWorldsRowsAndTerrainAreTheCompiledMaps) {
  const Region region =
      settle_map(SEED, base_data(), CompiledMap{map_source()});
  const Cell inside{(MOUNTAIN_BLOCK.x * MAP_BLOCK) + 1,
                    (MOUNTAIN_BLOCK.y * MAP_BLOCK) + 1};
  const Cell outside{0, 0};
  const auto row = [&region](Cell cell) {
    return region.map_rows.at(static_cast<std::size_t>(cell.y))
        .at(static_cast<std::size_t>(cell.x));
  };
  const auto terrain = [&region](Cell cell) {
    return region.terrain.at((static_cast<std::size_t>(cell.y) *
                              static_cast<std::size_t>(MAP_SIDE)) +
                             static_cast<std::size_t>(cell.x));
  };
  EXPECT_EQ(row(inside), tile_of(Footing::BLOCKED));
  EXPECT_EQ(terrain(inside), MOUNTAIN);
  EXPECT_EQ(row(outside), tile_of(Footing::BUILDABLE));
  EXPECT_EQ(terrain(outside), PLAIN);
}

TEST(SettledMap, AVillagePlaceWithNoRoomIsRefused) {
  // A village placed in the middle of the mountain block has no square.
  MapSource walled = map_source();
  walled.places.front().cell =
      Cell{(MOUNTAIN_BLOCK.x * MAP_BLOCK) + (MAP_BLOCK / 2),
           (MOUNTAIN_BLOCK.y * MAP_BLOCK) + (MAP_BLOCK / 2)};
  EXPECT_THROW(
      static_cast<void>(settle_map(SEED, base_data(), CompiledMap{walled})),
      std::runtime_error);
  // Control: the map as given settles.
  EXPECT_NO_THROW(static_cast<void>(
      settle_map(SEED, base_data(), CompiledMap{map_source()})));
}

TEST(SettledMap, ASectWithoutASeatPlaceIsRefused) {
  MapSource unseated = map_source();
  unseated.places.pop_back();
  EXPECT_THROW(
      static_cast<void>(settle_map(SEED, base_data(), CompiledMap{unseated})),
      std::runtime_error);
  // Two seat places for one sect are refused too.
  MapSource twice = map_source();
  std::get<SeatPlace>(twice.places.back().settles).sect = 0;
  EXPECT_THROW(
      static_cast<void>(settle_map(SEED, base_data(), CompiledMap{twice})),
      std::runtime_error);
  // Control: a seat place for each sect settles.
  EXPECT_NO_THROW(static_cast<void>(
      settle_map(SEED, base_data(), CompiledMap{map_source()})));
}

// The settled map's armies the tier tests use: the residents of its first
// village, at (40, 120) in area (1, 3), of its fourth, at (120, 200) in area
// (3, 6), and of its third sect's seat, at (200, 200) in area (6, 6).
constexpr std::size_t WEST_VILLAGE = 0;
constexpr std::size_t SOUTH_VILLAGE = 3;
constexpr std::size_t SOUTH_EAST_SEAT = 6;

std::size_t residents(const Region& region, std::size_t village) {
  return region.villages.at(village).army;
}

TEST(Tiers, TiersFollowTheSteeredArmyAndAWorldWithoutOneRunsLive) {
  const Region region =
      settle_map(SEED, base_data(), CompiledMap{map_source()});
  World world = world_of(region);
  world.set_profile(STANDARD_PROFILE);
  world.steer(residents(region, WEST_VILLAGE), true);
  world.retier();
  // From area (1, 3), every area live before: area (3, 6), 3 areas away,
  // one past the live radius, stays live; (5, 4), 4 away, is stepped; and
  // (6, 6), 5 away, past the stepped radius, is far.
  EXPECT_EQ(world.areas().tier(world.areas().area_of(Cell{40, 120})),
            AreaTier::LIVE);
  EXPECT_EQ(world.areas().tier(world.areas().area_of(Cell{120, 200})),
            AreaTier::LIVE);
  EXPECT_EQ(world.areas().tier(world.areas().area_of(Cell{176, 144})),
            AreaTier::STEPPED);
  EXPECT_FALSE(world.far(residents(region, SOUTH_VILLAGE)).has_value());
  EXPECT_TRUE(world.far(residents(region, SOUTH_EAST_SEAT)).has_value());
  // Control: with no steered army every area is live, and the far army
  // comes back.
  world.steer(residents(region, WEST_VILLAGE), false);
  world.retier();
  EXPECT_FALSE(world.far(residents(region, SOUTH_EAST_SEAT)).has_value());
  for (const AreaTier tier : world.areas().tiers()) {
    EXPECT_EQ(tier, AreaTier::LIVE);
  }
  // Control: steered, but with no profile, every area is live.
  world.set_profile(std::nullopt);
  world.steer(residents(region, WEST_VILLAGE), true);
  world.retier();
  EXPECT_FALSE(world.far(residents(region, SOUTH_EAST_SEAT)).has_value());
}

// The names of the fields of `made` that differ from `held`'s, every saved
// field of a person but its position, which a handover moves.
std::vector<std::string> differing(const Person& held, const Person& made) {
  std::vector<std::string> fields;
  const auto check = [&fields](bool same_value, const char* name) {
    if (!same_value) {
      fields.emplace_back(name);
    }
  };
  const auto same_aptitude = [](const Aptitude& one, const Aptitude& other) {
    return one.growth == other.growth && one.limit == other.limit;
  };
  check(held.health == made.health, "health");
  check(held.ready_at == made.ready_at, "ready_at");
  check(held.home == made.home, "home");
  check(held.born == made.born, "born");
  check(held.family == made.family, "family");
  check(held.parents == made.parents, "parents");
  check(held.sex == made.sex, "sex");
  check(held.given == made.given, "given");
  check(held.spouses == made.spouses, "spouses");
  check(held.children == made.children, "children");
  check(held.hunger == made.hunger, "hunger");
  check(std::equal(held.talent.begin(), held.talent.end(), made.talent.begin(),
                   made.talent.end(), same_aptitude),
        "talent");
  check(held.attainment == made.attainment, "attainment");
  check(held.lifespan == made.lifespan, "lifespan");
  check(held.soul == made.soul, "soul");
  check(held.sect == made.sect, "sect");
  check(held.casting.has_value() == made.casting.has_value() &&
            (!held.casting ||
             (held.casting->skill == made.casting->skill &&
              held.casting->target.index() == made.casting->target.index())),
        "casting");
  check(same_aptitude(held.cultivation, made.cultivation), "cultivation");
  check(held.qi == made.qi, "qi");
  check(held.qi_capacity == made.qi_capacity, "qi_capacity");
  check(std::equal(held.known.begin(), held.known.end(), made.known.begin(),
                   made.known.end(),
                   [](const Known& one, const Known& other) {
                     return one.art == other.art && one.skill == other.skill &&
                            one.ready_at == other.ready_at;
                   }),
        "known");
  check(held.silver == made.silver, "silver");
  check(held.held == made.held, "held");
  return fields;
}

// The fields a person holds other than at their defaults, by the names
// differing() gives them.
std::vector<std::string> set_fields(const Person& person) {
  return differing(Person{}, person);
}

// The days of food the far-going army's village starts with, short of the
// first harvest of the field its growth lays, about day 90; and the most the
// handover's world lives waiting for that village to run out.
constexpr std::uint64_t STORED_DAYS = 85;
constexpr std::uint64_t HUNGRY_BY = 90;

// Whether a member of army `army` has gone hungry.
bool hungry(const World& world, std::size_t army) {
  const std::vector<std::size_t>& roster = world.roster(army);
  return std::any_of(
      roster.begin(), roster.end(),
      [&world](std::size_t member) { return world.person(member).hunger > 0; });
}

// The settled map with the handover test's one premise apart from the base
// pack, on the far-going army's own village, the seat, alone: it harvests
// nothing, and its stores hold what its first residents eat in STORED_DAYS,
// so its stock runs out before the first harvest of the field its growth
// lays, and its people go hungry. On the base pack no one on the settled map
// goes hungry within three years, and with only its harvest gone the seat's
// full granary lasts its two people some 1,200 days. Every other village is
// as the base pack has it.
Region starving_seat() {
  Region region = settle_map(SEED, base_data(), CompiledMap{map_source()});
  const Village& seat = region.villages.at(SOUTH_EAST_SEAT);
  const std::uint64_t eaten =
      region.armies.at(seat.army).members.size() *
      region.life.husbandry.value_or(Husbandry{}).consumption * STORED_DAYS;
  bool stored = false;
  for (const std::size_t structure : seat.structures) {
    Holding& holding = region.holdings.at(structure);
    holding.harvest = 0;
    holding.storage =
        !stored && holding.storage > 0 ? static_cast<std::uint32_t>(eaten) : 0;
    stored = stored || holding.storage > 0;
  }
  return region;
}

// The cells east of which the handed army is far from the steered one: 5 or
// more areas east of its area, x of 192 cells or more, in sub-cell units.
constexpr std::int32_t FAR_EAST = 192 * CELL_SIZE;

// Gathers into army `handed` a living person standing far east from each
// other army but `steered` holding a field its people hold only at its
// default, gives its first member a soul, and returns the fields its people
// then hold set.
std::set<std::string> gather(World& world, std::size_t handed,
                             std::size_t steered) {
  std::set<std::string> held;
  for (const std::size_t member : world.roster(handed)) {
    const std::vector<std::string> fields = set_fields(world.person(member));
    held.insert(fields.begin(), fields.end());
  }
  for (std::size_t army = 0; army < world.army_count(); ++army) {
    const std::vector<std::size_t> roster = army == handed || army == steered
                                                ? std::vector<std::size_t>{}
                                                : world.roster(army);
    for (const std::size_t member : roster) {
      const std::vector<std::string> fields = set_fields(world.person(member));
      const bool adds = world.person(member).position.x >= FAR_EAST &&
                        std::any_of(fields.begin(), fields.end(),
                                    [&held](const std::string& field) {
                                      return held.count(field) == 0;
                                    });
      if (adds) {
        world.conscript(member, handed);
        held.insert(fields.begin(), fields.end());
      }
    }
  }
  static_cast<void>(world.ensoul(world.roster(handed).front()));
  held.insert("soul");
  return held;
}

// Has the first member of army `army` that may cast anything cast it, so a
// cast is pending. Returns whether one did.
bool cast_any(World& world, std::size_t army) {
  for (const std::size_t member : world.roster(army)) {
    const std::vector<Target> targets{Point{world.person(member).position},
                                      Itself{}};
    for (std::size_t skill = 0; skill < world.skill_count(member); ++skill) {
      for (const Target& target : targets) {
        if (world.cast_refusal(member, skill, target) == CastRefusal::NONE) {
          world.cast(member, skill, target);
          return true;
        }
      }
    }
  }
  return false;
}

// The fields in which army `army`'s people differ from `snapshot`, rank by
// rank.
std::vector<std::string> changed_since(const World& world, std::size_t army,
                                       const std::vector<Person>& snapshot) {
  std::vector<std::string> changed;
  for (std::size_t rank = 0; rank < snapshot.size(); ++rank) {
    const std::vector<std::string> fields =
        differing(snapshot.at(rank), world.person(world.roster(army).at(rank)));
    changed.insert(changed.end(), fields.begin(), fields.end());
  }
  return changed;
}

// Every saved field of a person but its position, by differing()'s names.
const std::set<std::string>& every_field() {
  static const std::set<std::string> fields{
      "health",  "ready_at", "home",        "born",     "family",
      "parents", "sex",      "given",       "spouses",  "children",
      "hunger",  "talent",   "attainment",  "lifespan", "soul",
      "sect",    "casting",  "cultivation", "qi",       "qi_capacity",
      "known",   "silver",   "held"};
  return fields;
}

// Lives `world` until a member of army `army` goes hungry, or HUNGRY_BY
// days, whichever is first.
void live_until_hungry(World& world, std::size_t army) {
  for (std::uint64_t day = 0; day < HUNGRY_BY && !hungry(world, army); ++day) {
    for (std::uint64_t tick = 0; tick < world.ticks_per_day(); ++tick) {
      world.tick();
    }
  }
}

// The records of army `army`'s people, rank by rank.
std::vector<Person> records_of(const World& world, std::size_t army) {
  std::vector<Person> records;
  for (const std::size_t member : world.roster(army)) {
    records.push_back(world.person(member));
  }
  return records;
}

TEST(Tiers, AHandoverFarAndBackKeepsEveryPersonsRecord) {
  const Region region = starving_seat();
  World world = world_of(region);
  const std::size_t steered = residents(region, WEST_VILLAGE);
  const std::size_t handed = residents(region, SOUTH_EAST_SEAT);
  // It lives until the seat's stock runs out, and is handed over on the
  // first day a member goes hungry, within the husbandry's hunger days; by
  // then its people have families, attainment, stores and wares.
  live_until_hungry(world, handed);
  ASSERT_TRUE(hungry(world, handed));
  std::set<std::string> held = gather(world, handed, steered);
  ASSERT_TRUE(cast_any(world, handed));
  held.insert("casting");
  EXPECT_EQ(held, every_field());
  const std::vector<Person> snapshot = records_of(world, handed);
  world.set_profile(STANDARD_PROFILE);
  world.steer(steered, true);
  world.retier();
  ASSERT_TRUE(world.far(handed).has_value());
  EXPECT_EQ(changed_since(world, handed, snapshot), std::vector<std::string>{});
  world.steer(steered, false);
  world.retier();
  ASSERT_FALSE(world.far(handed).has_value());
  EXPECT_EQ(changed_since(world, handed, snapshot), std::vector<std::string>{});
  // Control: a handover that dropped a member's qi to its default is seen,
  // on that field alone.
  Person dropped = world.person(world.roster(handed).front());
  ASSERT_GT(dropped.qi, 0U);
  dropped.qi = 0;
  EXPECT_EQ(differing(snapshot.front(), dropped),
            std::vector<std::string>{"qi"});
}

// The ticks the steered army takes to walk away and back.
constexpr int WALK_TICKS = 3'000;
constexpr Cell FAR_CORNER{230, 230};

// The hash of a world on the settled map, settled from `seed`, whose fourth
// village's residents, steered on `profile`, walk to the far corner and back,
// and whether its first village's residents were far between and live
// after.
struct Walked {
  StateHash hash;
  bool far_between;
  bool live_after;
};

Walked walked(Seed seed, std::optional<Profile> profile) {
  const Region region =
      settle_map(seed, base_data(), CompiledMap{map_source()});
  World world = world_of(region);
  world.set_profile(profile);
  const std::size_t walker = residents(region, SOUTH_VILLAGE);
  const std::size_t watched = residents(region, WEST_VILLAGE);
  world.steer(walker, true);
  std::vector<std::size_t> skills(world.skills(walker).size());
  std::iota(skills.begin(), skills.end(), std::size_t{0});
  const Cell home = world.objective(walker);
  world.order(walker, Order{FAR_CORNER, skills});
  for (int tick = 0; tick < WALK_TICKS; ++tick) {
    world.tick();
  }
  const bool far_between = world.far(watched).has_value();
  world.order(walker, Order{home, skills});
  for (int tick = 0; tick < WALK_TICKS; ++tick) {
    world.tick();
  }
  return Walked{world.hash(), far_between, !world.far(watched).has_value()};
}

TEST(Tiers, ALiveFarAndLiveRunHashesTheSameTwice) {
  const Walked first = walked(SEED, STANDARD_PROFILE);
  ASSERT_TRUE(first.far_between);
  ASSERT_TRUE(first.live_after);
  EXPECT_EQ(walked(SEED, STANDARD_PROFILE).hash, first.hash);
  // Controls: another seed, and the same run with no tiers, so the far phase
  // is seen to act.
  EXPECT_NE(walked(OTHER_SEED, STANDARD_PROFILE).hash, first.hash);
  const Walked untiered = walked(SEED, std::nullopt);
  ASSERT_FALSE(untiered.far_between);
  EXPECT_NE(untiered.hash, first.hash);
}

// How far from the map's far edges the far fight stands, the second army a
// cell inside the first, and the structure stages' floors it is fought with.
constexpr std::int32_t CORNER = 4;
constexpr std::int32_t BESIDE = CORNER + 1;
constexpr std::array<std::uint32_t, 3> FIGHT_FLOORS{75, 25, 1};

// Two hostile armies side by side at the far corner of a map of 8 by 8 areas,
// and a third, steered, at the near one, on `profile`, after two seconds.
World far_fight(std::optional<Profile> profile) {
  constexpr std::int32_t SIDE = 8 * AREA_SIDE;
  const std::vector<std::string> rows(static_cast<std::size_t>(SIDE),
                                      std::string(SIDE, '.'));
  constexpr std::uint64_t HEALTH = 100;
  const Skill blow{2 * CELL_SIZE, 0, 1, 0, 1};
  World world{Seed{1},
              Map{rows},
              {ArmySpec{{4, 4}, {Recruit{{4, 4}, HEALTH}}, 0, {blow}},
               ArmySpec{{SIDE - CORNER, SIDE - CORNER},
                        {Recruit{{SIDE - CORNER, SIDE - CORNER}, HEALTH}},
                        1,
                        {blow}},
               ArmySpec{{SIDE - BESIDE - 1, SIDE - CORNER},
                        {Recruit{{SIDE - BESIDE, SIDE - CORNER}, HEALTH}},
                        0,
                        {blow}}},
              {},
              Rules{CELL_SIZE, {{false, true}, {true, false}}, FIGHT_FLOORS}};
  world.set_profile(profile);
  world.steer(0, true);
  world.retier();
  for (int tick = 0; tick < 2 * TICKS_PER_SECOND; ++tick) {
    world.tick();
  }
  return world;
}

TEST(Tiers, AFarArmyNeitherStrikesNorMoves) {
  const World far = far_fight(STANDARD_PROFILE);
  ASSERT_TRUE(far.far(1).has_value());
  ASSERT_TRUE(far.far(2).has_value());
  const Person& one = far.person(far.roster(1).front());
  const Person& other = far.person(far.roster(2).front());
  // Struck by no one, and standing where it stood once far.
  EXPECT_EQ(one.health, 100U);
  EXPECT_EQ(other.health, 100U);
  EXPECT_EQ(cell_of(other.position).x, (8 * AREA_SIDE) - BESIDE);
  // Control: with no tiers the two fight, and the second walks.
  const World live = far_fight(std::nullopt);
  EXPECT_LT(live.person(live.roster(1).front()).health, 100U);
}

}  // namespace
}  // namespace murim
