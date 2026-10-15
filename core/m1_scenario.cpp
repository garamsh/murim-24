#include "m1_scenario.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "attack.hpp"
#include "calendar.hpp"
#include "map.hpp"
#include "skill.hpp"
#include "village.hpp"
#include "world.hpp"

namespace murim {
namespace {

constexpr std::uint64_t SCENARIO_SEED = 2026;
constexpr std::int32_t MAP_SIDE = 128;
constexpr Cell CENTRE{64, 64};
// People stand a cell apart, as wide as a body is drawn.
constexpr std::int32_t SEPARATION = CELL_SIZE;

// Two hostile lines of ten armies, each a block of five by five people, with
// a barricade of barrels between them. The middle army on each side are
// casters. Every one of the world's people is in the battle.
constexpr std::int32_t LINE_ARMIES = 10;
constexpr std::int32_t BLOCK = 5;
constexpr std::int32_t FIRST_ROW = CENTRE.y - (LINE_ARMIES * BLOCK / 2);
constexpr std::int32_t LINE_GAP = 1;
constexpr std::uint64_t HEALTH = 3'000;
constexpr std::uint32_t DURABILITY = 3'000;
constexpr std::size_t FACTION_WEST = 0;
constexpr std::size_t FACTION_EAST = 1;

// A melee strike every half second at one foe at arm's length, a body and a
// half, and an area strike every second whose disc reaches the barricade: two
// caster armies of 25 people make 50 area strikes a second.
constexpr std::uint64_t JAB_DAMAGE = 10;
constexpr Skill JAB{3 * CELL_SIZE / 2,    0, JAB_DAMAGE,     0,
                    TICKS_PER_SECOND / 2, 0, Shape::TARGETED};
constexpr Skill SWEEP{3 * CELL_SIZE, (3 * CELL_SIZE) / 2, JAB_DAMAGE, 1,
                      TICKS_PER_SECOND};

// People close on a foe within the region's sight, so the lines meet
// through the gaps between the barrels at arm's length.
constexpr std::int32_t SIGHT = 6;

Rules rules() {
  const std::vector<std::vector<bool>> hostile{{false, true}, {true, false}};
  Rules battle{SEPARATION, hostile, {DURABILITY, DURABILITY / 2, 1}};
  battle.sight = SIGHT;
  battle.people_capacity = M1_CAPACITY;
  battle.tokens = default_tokens();
  return battle;
}

// A block of BLOCK by BLOCK people from `corner`.
std::vector<Recruit> block(Cell corner) {
  std::vector<Recruit> members;
  for (const Cell cell : cells_of(corner, Footprint{BLOCK, BLOCK})) {
    members.push_back(Recruit{cell, HEALTH});
  }
  return members;
}

}  // namespace

M1Scenario m1_scenario() {
  M1Scenario scenario{
      Seed{SCENARIO_SEED},
      std::vector<std::string>(MAP_SIDE, std::string(MAP_SIDE, '.')),
      {},
      {},
      {},
      rules(),
      CENTRE,
      0};
  const std::vector<Skill> skills{JAB};
  for (std::int32_t line = 0; line < LINE_ARMIES; ++line) {
    const std::int32_t top = FIRST_ROW + (line * BLOCK);
    const std::int32_t middle = top + (BLOCK / 2);
    scenario.armies.push_back(
        ArmySpec{{CENTRE.x - 1, middle},
                 block({CENTRE.x - LINE_GAP - BLOCK, top}),
                 FACTION_WEST,
                 skills});
    scenario.armies.push_back(ArmySpec{{CENTRE.x + 1, middle},
                                       block({CENTRE.x + LINE_GAP, top}),
                                       FACTION_EAST,
                                       skills});
    scenario.structures.push_back(
        Structure{Cell{CENTRE.x, middle}, DURABILITY});
    scenario.structure_kinds.emplace_back("barrels");
  }
  // The middle pair of the lines are the casters.
  scenario.casters_first = LINE_ARMIES;
  scenario.armies.at(scenario.casters_first).skills = {SWEEP};
  scenario.armies.at(scenario.casters_first + 1).skills = {SWEEP};
  return scenario;
}

}  // namespace murim
