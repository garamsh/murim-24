#include "region.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
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
  std::vector<Cell> cells;
  for (std::int32_t row = 0; row < village.side; ++row) {
    for (std::int32_t column = 0; column < village.side; ++column) {
      cells.push_back(Cell{village.corner.x + column, village.corner.y + row});
    }
  }
  return cells;
}

bool inside(const Village& village, Cell cell) {
  return cell.x >= village.corner.x && cell.y >= village.corner.y &&
         cell.x < village.corner.x + village.side &&
         cell.y < village.corner.y + village.side;
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
    for (const auto value :
         {std::int64_t{army.start.x}, std::int64_t{army.start.y},
          std::int64_t{army.objective.x}, std::int64_t{army.objective.y}}) {
      add(value);
    }
    add(army.count);
    add(army.strength);
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
  for (const Village& village : region.villages) {
    add_text(village.name);
    add(village.faction);
    add(village.corner.x);
    add(village.corner.y);
    add(village.side);
    for (const std::size_t structure : village.structures) {
      add(structure);
    }
    add(village.army);
  }
  return values;
}

World world_of(const Region& region) {
  return World{region.seed, Map{region.map_rows}, region.armies,
               region.structures, region.rules};
}

StateHash hash_after_ticks(const Region& region) {
  World world = world_of(region);
  for (int tick = 0; tick < TICKS; ++tick) {
    world.tick();
  }
  return world.hash();
}

// Whether every cell of each village's square is passable, and its structures
// and resident army stand inside that square.
bool villages_on_passable_cells(const Region& region) {
  const Map map{region.map_rows};
  for (const Village& village : region.villages) {
    for (const Cell cell : area_of(village)) {
      if (!map.passable(cell)) {
        return false;
      }
    }
    for (const std::size_t structure : village.structures) {
      if (!inside(village, region.structures.at(structure).cell)) {
        return false;
      }
    }
    if (!inside(village, region.armies.at(village.army).start)) {
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

    // Control: blocking the cell a resident army stands on breaks it.
    Region blocked = region;
    const Cell home = blocked.armies.at(blocked.villages.at(0).army).start;
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
  data.factions.push_back(Faction{"Martial clans", {1}});
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
  VillageTemplate& village = outside.villages.at(0);
  village.structures.push_back(Cell{village.side, 0});
  EXPECT_THROW(static_cast<void>(generate_region(SEED, outside)),
               std::invalid_argument);
}

TEST(Region, ABodyBornIntoAVillageIsOneAgentOfItsFactionInsideIt) {
  Region region = generate_region(SEED, region_data());
  const Village& village = region.villages.at(1);
  const ArmySpec& residents = region.armies.at(village.army);
  const ArmySpec born = born_in(region, 1);

  EXPECT_EQ(born.count, 1U);
  EXPECT_EQ(born.faction, village.faction);
  EXPECT_EQ(born.strength, residents.strength / residents.count);
  EXPECT_TRUE(inside(village, born.start));
  EXPECT_TRUE(inside(village, born.objective));
  region.armies.push_back(born);
  EXPECT_NO_THROW(world_of(region));
  // Control: a village the region lacks has no one born into it.
  EXPECT_THROW(static_cast<void>(born_in(region, region.villages.size())),
               std::out_of_range);
}

}  // namespace
}  // namespace murim
