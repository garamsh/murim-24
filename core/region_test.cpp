#include "region.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr std::uint64_t SEEDS = 200;
constexpr int TICKS = 300;
constexpr Seed SEED{24};
constexpr Seed OTHER_SEED{25};
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
               region.structures, region.rules,         village_specs(region)};
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

// Whether every cell of each village's area is passable, every cell of its
// structures' footprints lies inside that area, and each of its residents
// stands inside it on a cell no structure covers.
bool villages_on_passable_cells(const Region& region) {
  const Map map{region.map_rows};
  for (const Village& village : region.villages) {
    const std::vector<Recruit>& residents =
        region.armies.at(village.army).members;
    bool fits = true;
    for (const Cell cell : area_of(village)) {
      fits = fits && map.passable(cell);
    }
    for (const Recruit& resident : residents) {
      fits =
          fits && inside(village, resident.cell) && map.passable(resident.cell);
    }
    for (const std::size_t index : village.structures) {
      const Structure& structure = region.structures.at(index);
      for (const Cell cell : cells_of(structure.cell, structure.footprint)) {
        fits = fits && inside(village, cell) &&
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

bool villages_disjoint(const Region& region) {
  std::vector<bool> taken(region.territory.size(), false);
  for (const Village& village : region.villages) {
    for (const Cell cell : area_of(village)) {
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
  const std::size_t villages = region.villages.size();
  return region.rules.hostile.size() == factions &&
         territories(region) == all_of_factions(factions) &&
         owners(region) == all_of_factions(factions) &&
         villages >= MIN_VILLAGES && villages <= MAX_VILLAGES &&
         region.armies.size() == villages;
}

TEST(Region, SameSeedGivesTheSameRegionAndWorld) {
  const Region first = generate_region(SEED, region_data());
  const Region second = generate_region(SEED, region_data());
  EXPECT_EQ(flatten(first), flatten(second));
  EXPECT_EQ(hash_after_ticks(first), hash_after_ticks(second));

  // Control: another seed's region, given this seed so that only what was
  // generated from the seed differs, is another region and another world.
  Region other = generate_region(OTHER_SEED, region_data());
  other.seed = SEED;
  EXPECT_NE(flatten(first), flatten(other));
  EXPECT_NE(hash_after_ticks(first), hash_after_ticks(other));
}

TEST(Region, HasThreeFactionsWithTerritoryAndSixToEightVillages) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, region_data());
    EXPECT_TRUE(populated(region, 3)) << "seed " << seed;

    // Control: a region of three factions is not one of four.
    EXPECT_FALSE(populated(region, 4)) << "seed " << seed;
  }
}

TEST(Region, VillagesStandOnPassableCells) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, region_data());
    EXPECT_TRUE(villages_on_passable_cells(region)) << "seed " << seed;
    EXPECT_TRUE(builds_a_world(region)) << "seed " << seed;

    // Control: blocking the cell a resident army stands on breaks it.
    Region blocked = region;
    const Cell home =
        blocked.armies.at(blocked.villages.at(0).army).members.front().cell;
    blocked.map_rows.at(static_cast<std::size_t>(home.y))
        .at(static_cast<std::size_t>(home.x)) = '#';
    ASSERT_FALSE(Map{blocked.map_rows}.passable(home));
    EXPECT_FALSE(villages_on_passable_cells(blocked)) << "seed " << seed;
  }
}

TEST(Region, VillagesLieInTheirOwnersTerritory) {
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region region = generate_region(Seed{seed}, region_data());
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
    const Region region = generate_region(Seed{seed}, region_data());
    EXPECT_TRUE(villages_disjoint(region)) << "seed " << seed;

    // Control: moving one village onto another breaks it.
    Region stacked = region;
    stacked.villages.at(1).corner = stacked.villages.at(0).corner;
    EXPECT_FALSE(villages_disjoint(stacked)) << "seed " << seed;
  }
}

TEST(Region, GeneratesAFourthFactionAddedAsData) {
  RegionData data = region_data();
  data.factions.push_back(
      Faction{"Martial clans", {1}, data.factions.at(0).skills});
  data.factions.at(1).hostile_to.push_back(3);

  const Region region = generate_region(SEED, data);
  EXPECT_TRUE(region.rules.hostile.at(3).at(1));
  EXPECT_TRUE(region.rules.hostile.at(1).at(3));
  EXPECT_NO_THROW(world_of(region));
  for (std::uint64_t seed = 0; seed < SEEDS; ++seed) {
    const Region generated = generate_region(Seed{seed}, data);
    EXPECT_TRUE(
        populated(generated, 4) && villages_on_passable_cells(generated) &&
        villages_in_owner_territory(generated) && villages_disjoint(generated))
        << "seed " << seed;
  }
}

TEST(Region, RejectsInconsistentData) {
  EXPECT_NO_THROW(static_cast<void>(generate_region(SEED, region_data())));

  RegionData unknown_enemy = region_data();
  unknown_enemy.factions.at(0).hostile_to.push_back(3);
  EXPECT_THROW(static_cast<void>(generate_region(SEED, unknown_enemy)),
               std::invalid_argument);

  RegionData weightless = region_data();
  for (TerrainKind& kind : weightless.terrain) {
    kind.weight = 0;
  }
  EXPECT_THROW(static_cast<void>(generate_region(SEED, weightless)),
               std::invalid_argument);

  RegionData outside = region_data();
  VillageTemplate& beyond = outside.villages.at(0);
  beyond.buildings.push_back(
      Building{Structure{{beyond.area.width - 1, 0}, 1, {2, 1}}, "house", 1});
  EXPECT_THROW(static_cast<void>(generate_region(SEED, outside)),
               std::invalid_argument);

  RegionData overlapping = region_data();
  VillageTemplate& doubled = overlapping.villages.at(0);
  doubled.buildings.push_back(doubled.buildings.front());
  EXPECT_THROW(static_cast<void>(generate_region(SEED, overlapping)),
               std::invalid_argument);

  RegionData over_centre = region_data();
  VillageTemplate& central = over_centre.villages.at(0);
  central.buildings.push_back(Building{
      Structure{{central.area.width / 2, central.area.height / 2}, 1, {1, 1}},
      "barrels", 0});
  EXPECT_THROW(static_cast<void>(generate_region(SEED, over_centre)),
               std::invalid_argument);

  RegionData empty = region_data();
  for (Building& building : empty.villages.at(0).buildings) {
    building.residents = 0;
  }
  EXPECT_THROW(static_cast<void>(generate_region(SEED, empty)),
               std::invalid_argument);
}

// The template `village` was laid out from, or null when `data` has none of its
// name.
const VillageTemplate* template_of(const RegionData& data,
                                   const Village& village) {
  const auto made_from =
      std::find_if(data.villages.begin(), data.villages.end(),
                   [&village](const VillageTemplate& kind) {
                     return kind.name == village.kind;
                   });
  return made_from == data.villages.end() ? nullptr : &*made_from;
}

// Whether each of the village's structures carries its template building's
// kind, in order.
bool kinds_follow(const Region& region, const Village& village,
                  const VillageTemplate& made_from) {
  if (village.structures.size() != made_from.buildings.size()) {
    return false;
  }
  for (std::size_t index = 0; index < village.structures.size(); ++index) {
    if (region.structure_kinds.at(village.structures.at(index)) !=
        made_from.buildings.at(index).kind) {
      return false;
    }
  }
  return true;
}

std::uint32_t housed_by(const VillageTemplate& made_from) {
  std::uint32_t housed = 0;
  for (const Building& building : made_from.buildings) {
    housed += building.residents;
  }
  return housed;
}

// Whether `village` was laid out from a template of `data` whose buildings'
// kinds its structures carry, and whose residents its army counts. A flat count
// per structure would differ: wells, barrels and the fence house no one, and a
// hall houses more than a house.
bool follows_its_template(const RegionData& data, const Region& region,
                          const Village& village) {
  const VillageTemplate* made_from = template_of(data, village);
  return made_from != nullptr && kinds_follow(region, village, *made_from) &&
         region.armies.at(village.army).members.size() ==
             housed_by(*made_from) &&
         housed_by(*made_from) != 4 * village.structures.size();
}

TEST(Region, StructuresCarryTheirKindAndBuildingsHouseTheResidents) {
  const RegionData data = region_data();
  const Region region = generate_region(SEED, data);
  EXPECT_EQ(region.structure_kinds.size(), region.structures.size());
  for (const Village& village : region.villages) {
    EXPECT_TRUE(follows_its_template(data, region, village)) << village.name;
  }
}

TEST(Region, APersonBornIntoAVillageIsOfItsFactionInsideIt) {
  Region region = generate_region(SEED, region_data());
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

// Health of each person gathered().
constexpr std::uint64_t GATHERED_HEALTH = 100'000;

// `people` people of GATHERED_HEALTH on the open cells of village `home`
// that none of its residents stands on, row by row.
std::vector<Recruit> gathered(const Region& region, const Village& home,
                              std::size_t people) {
  const std::vector<Recruit>& residents = region.armies.at(home.army).members;
  std::vector<Recruit> members;
  for (const Cell cell : open_cells(region, home)) {
    const bool taken = std::any_of(
        residents.begin(), residents.end(),
        [cell](const Recruit& resident) { return same(resident.cell, cell); });
    if (!taken && members.size() < people) {
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
  Region region = generate_region(SEED, region_data());
  const RegionData data = region_data();
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

}  // namespace
}  // namespace murim
