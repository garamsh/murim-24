#include "region.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "m1_scenario.hpp"
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
               region.structures, region.rules,         village_specs(region),
               region.leadership, region.life};
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
  data.factions.push_back(Faction{
      "Martial clans", {1}, data.factions.at(0).skills, std::nullopt, {}});
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

TEST(Region, EachFactionIsLedByTheFirstResidentOfItsFirstVillage) {
  const Region region = generate_region(SEED, region_data());
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

// Combat pace, in ticks: the targets region_data() is tuned to.
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
// Seeds a walled village is raided on, each with room west of it for the
// whole band.
constexpr std::array<Seed, 3> WALLED{SEED, Seed{7}, Seed{99}};
// Seeds the pace is measured on, the region tests' own first.
constexpr std::array<Seed, 3> PACED{SEED, Seed{2026}, Seed{5}};

// Whether ticks `ticks` lie within [least, most].
bool within(std::uint64_t ticks, std::uint64_t least, std::uint64_t most) {
  return ticks >= least && ticks <= most;
}

// The ticks from a person born into the first Demonic village of the region
// `data` generates from `seed` challenging its leader, as the player does,
// until the duel is settled, or `limit` if it is not by then.
std::uint64_t duel_ticks(const RegionData& data, Seed seed,
                         std::uint64_t limit) {
  Region region = generate_region(seed, data);
  region.armies.push_back(born_in(region, first_of(region, DEMONIC)));
  const std::size_t challenger = region.armies.size() - 1;
  World world = world_of(region);
  const std::size_t leader = world.leader(DEMONIC).value_or(0);
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
  const auto clear = [&region](Cell corner) {
    const auto passable = [&region](Cell cell) {
      return region.map_rows.at(static_cast<std::size_t>(cell.y))
                 .at(static_cast<std::size_t>(cell.x)) == '.';
    };
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
// no one left, or `limit` if neither falls by then.
std::uint64_t skirmish_ticks(const RegionData& data, Seed seed,
                             std::uint64_t limit) {
  constexpr std::int32_t LINE = 4;
  constexpr std::int32_t APART = 6;
  Region region = generate_region(seed, data);
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

// How a raid went: the ticks it ran, and the residents still living.
struct Raided {
  std::uint64_t ticks;
  std::size_t residents;
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
// `limit` ticks if it is not by then.
Raided raided(const RegionData& data, Seed seed, const Raid& raid,
              std::uint64_t limit, bool bound = true) {
  Region region = generate_region(seed, data);
  const std::size_t target = first_raided(region, raid.kind);
  const Village& village = region.villages.at(target);
  const std::size_t residents = village.army;
  const Cell centre = region.armies.at(residents).objective;
  ArmySpec raiders{centre, {}, DEMONIC, data.factions.at(DEMONIC).skills};
  const Footprint west{village.corner.x, village.area.height};
  std::vector<Cell> cells = cells_of(Cell{0, village.corner.y}, west);
  std::stable_sort(cells.begin(), cells.end(),
                   [](Cell left, Cell right) { return left.x > right.x; });
  for (const Cell cell : cells) {
    if (raiders.members.size() < raid.band &&
        region.map_rows.at(static_cast<std::size_t>(cell.y))
                .at(static_cast<std::size_t>(cell.x)) == '.') {
      raiders.members.push_back(Recruit{cell, data.life.newborn_health});
    }
  }
  raiders.objective = bound ? centre : raiders.members.front().cell;
  region.armies.push_back(raiders);
  World world = world_of(region);
  // Setup: the whole band against a village that stands.
  EXPECT_EQ(world.roster(region.armies.size() - 1).size(), raid.band);
  EXPECT_TRUE(std::all_of(village.structures.begin(), village.structures.end(),
                          [&world](std::size_t index) {
                            return world.stage(index) == Stage::INTACT;
                          }));
  std::uint64_t ticks = 0;
  for (; ticks < limit && world.village_state(target) != VillageState::RUINS;
       ++ticks) {
    world.tick();
  }
  return Raided{ticks, world.roster(residents).size()};
}

// region_data() with the Demonic Cult's first skill, its strike, doing
// `damage` to a person.
RegionData striking(std::uint64_t damage) {
  RegionData data = region_data();
  data.factions.at(DEMONIC).skills.front().body_damage = damage;
  data.factions.at(0).skills.front().body_damage = damage;
  return data;
}

TEST(Region, ADuelEndsWithinItsTargetTime) {
  for (const Seed seed : PACED) {
    EXPECT_TRUE(within(duel_ticks(region_data(), seed, DUEL_MOST + 1),
                       DUEL_LEAST, DUEL_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: the placeholder strike of 1 leaves the duel fought past it.
  EXPECT_GT(duel_ticks(striking(1), SEED, DUEL_MOST + 1), DUEL_MOST);
}

TEST(Region, ASkirmishOfTwentyASideEndsWithinItsTargetTime) {
  for (const Seed seed : PACED) {
    EXPECT_TRUE(within(skirmish_ticks(region_data(), seed, SKIRMISH_MOST + 1),
                       SKIRMISH_LEAST, SKIRMISH_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: the placeholder strike of 1 leaves both lines standing past it.
  EXPECT_GT(skirmish_ticks(striking(1), SEED, SKIRMISH_MOST + 1),
            SKIRMISH_MOST);
}

TEST(Region, ARaidLeavesAHamletInRuinsWithinItsTargetTime) {
  for (const Seed seed : PACED) {
    EXPECT_TRUE(
        within(raided(region_data(), seed, HAMLET_RAID, RAID_MOST + 1).ticks,
               RAID_LEAST, RAID_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
  // Control: the placeholder torch of a tenth the damage leaves the hamlet
  // standing past it.
  RegionData slow = region_data();
  Skill& torch = slow.factions.at(DEMONIC).skills.back();
  constexpr std::int32_t SLOWER = 10;
  torch.structure_damage /= SLOWER;
  EXPECT_GT(raided(slow, SEED, HAMLET_RAID, RAID_MOST + 1).ticks, RAID_MOST);
}

TEST(Region, ARaidLeavesTheWalledVillageInRuinsWithinItsTargetTime) {
  for (const Seed seed : WALLED) {
    EXPECT_TRUE(within(
        raided(region_data(), seed, VILLAGE_RAID, VILLAGE_RAID_MOST + 1).ticks,
        RAID_LEAST, VILLAGE_RAID_MOST))
        << "seed " << static_cast<std::uint64_t>(seed);
  }
}

TEST(Region, ABandTheWalledVillageOutnumbersStillClosesToStrike) {
  // The hamlet's band of RAIDERS, which the village's 26 residents outnumber,
  // passes its walls and strikes its residents down.
  constexpr Raid OUTNUMBERED{"village", RAIDERS};
  const Region region = generate_region(SEED, region_data());
  const std::size_t residents =
      region.armies.at(region.villages.at(first_raided(region, "village")).army)
          .members.size();
  ASSERT_EQ(residents, 26U);
  EXPECT_LT(raided(region_data(), SEED, OUTNUMBERED, SKIRMISH_MOST).residents,
            residents);
  // Control: the same band kept where it stands strikes no one.
  EXPECT_EQ(
      raided(region_data(), SEED, OUTNUMBERED, SKIRMISH_MOST, false).residents,
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
// its father's family.
bool born_in_wedlock(const std::vector<Person>& people) {
  return std::all_of(people.begin(), people.end(), [&](const Person& child) {
    if (child.parents.size() != 2 || child.soul) {
      return child.parents.empty() || child.soul.has_value();
    }
    const Person& father = people.at(child.parents.front());
    const Person& mother = people.at(child.parents.back());
    return father.sex == Sex::MALE && mother.sex == Sex::FEMALE &&
           std::count(mother.spouses.begin(), mother.spouses.end(),
                      child.parents.front()) == 1 &&
           child.family == father.family && father.family != mother.family;
  });
}

TEST(Region, FamiliesReachAThirdGenerationWithinThirtyYears) {
  const Lived region = lived(region_data(), SEED, GENERATIONS_YEARS);
  const std::vector<int> generation = generations(region.people);
  EXPECT_GE(*std::max_element(generation.begin(), generation.end()), 3);
  EXPECT_LE(region.peak, MAX_PEOPLE);
  EXPECT_TRUE(born_in_wedlock(region.people));
  // Control: without kinship no one marries, and the world starts with the
  // only generation it has.
  RegionData unwed = region_data();
  unwed.life.kinship.reset();
  const Lived control = lived(unwed, SEED, GENERATIONS_YEARS);
  const std::vector<int> only = generations(control.people);
  EXPECT_EQ(*std::max_element(only.begin(), only.end()), 1);
  EXPECT_TRUE(std::none_of(
      control.people.begin(), control.people.end(),
      [](const Person& person) { return !person.spouses.empty(); }));
}

TEST(Region, BirthsStopOnceMaxPeopleLive) {
  // Everyone free marries and every wife bears a child each day she may,
  // with no village's economy to cap them.
  RegionData fertile = region_data();
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
  EXPECT_LT(lived(region_data(), SEED, GENERATIONS_YEARS).peak, MAX_PEOPLE);
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
  const Lived region = lived(region_data(), SEED, GENERATIONS_YEARS);
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
  EXPECT_EQ(spelled(lived(region_data(), SEED, 0)),
            spelled(lived(region_data(), SEED, 0)));
  EXPECT_NE(spelled(lived(region_data(), SEED, 0)),
            spelled(lived(region_data(), OTHER_SEED, 0)));
}

// The most people above its cap any village of the region `data` generates
// from SEED ever held, measured against `capping`'s caps, and the most people
// its villages together held, over GENERATIONS_YEARS years of one-tick days.
std::pair<std::int64_t, std::size_t> crowding(const RegionData& data,
                                              const RegionData& capping) {
  Region region = generate_region(SEED, data);
  region.rules.ticks_per_day = 1;
  World world = world_of(region);
  const Region capped_region = generate_region(SEED, capping);
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
  // which raises the caps measured against, is left out.
  RegionData fertile = region_data();
  fertile.life.growth.reset();
  ASSERT_TRUE(fertile.life.kinship.has_value());
  Kinship kinship = fertile.life.kinship.value_or(Kinship{});
  kinship.marriage_chance = PER_MILLE;
  kinship.birth_chance = PER_MILLE;
  kinship.birth_spacing = 0;
  fertile.life.kinship = kinship;
  const Region region = generate_region(SEED, fertile);
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

// How a region grew over GROWTH_YEARS years: its villages and territory at
// the start and the end, its armies at the end, the most living at once,
// whether a founded village came to house its settlers and a building was
// raised, and the hash.
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
};

constexpr std::uint64_t GROWTH_YEARS = 50;
// Days of five ticks: marching, fighting and raising run far faster against
// the calendar than in play, so fifty years see many growth checks resolve.
constexpr std::uint64_t GROWTH_TICKS_PER_DAY = 5;

Grew grew(const RegionData& data, Seed seed) {
  Region region = generate_region(seed, data);
  region.rules.ticks_per_day = GROWTH_TICKS_PER_DAY;
  World world = world_of(region);
  Grew result{world.village_count(), 0,     0,     world.territory(), {},
              world.living(),        false, false, StateHash{}};
  for (std::uint64_t tick = 0;
       tick < GROWTH_YEARS * DAYS_PER_YEAR * GROWTH_TICKS_PER_DAY; ++tick) {
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
  const std::size_t builds = data.life.growth.value_or(Growth{}).builds.size();
  for (std::size_t structure = region.structures.size();
       structure < world.structure_count(); ++structure) {
    result.building_raised =
        result.building_raised ||
        (world.plan_of(structure).value_or(builds) < builds &&
         world.stage(structure) == Stage::INTACT);
  }
  result.hash = world.hash();
  return result;
}

TEST(Region, VillagesBuildFoundAndRaidOverFiftyYearsWithinTheBound) {
  const Grew region = grew(region_data(), SEED);
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
  EXPECT_EQ(static_cast<std::uint64_t>(grew(region_data(), SEED).hash),
            static_cast<std::uint64_t>(region.hash));
  EXPECT_NE(static_cast<std::uint64_t>(grew(region_data(), OTHER_SEED).hash),
            static_cast<std::uint64_t>(region.hash));
  // Control: with no faction taking a stance, no village is founded.
  RegionData still = region_data();
  for (Faction& faction : still.factions) {
    faction.stance.clear();
  }
  const Grew unchanged = grew(still, SEED);
  EXPECT_EQ(unchanged.villages_after, unchanged.villages_before);
  EXPECT_EQ(unchanged.armies_after, unchanged.villages_after);
  EXPECT_FALSE(unchanged.building_raised);
}

}  // namespace
}  // namespace murim
