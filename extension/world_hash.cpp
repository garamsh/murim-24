// Prints the core's hash after running a scenario, its bits read as a signed
// int as GDScript reads them, then one line `army x y health` per living
// person: the reference extension/world_adapter_test.gd checks the adapter
// against. The scenario is one argument of whitespace-separated fields in the
// order the core's types declare them, each list preceded by its length:
// seed ticks rows... armies... structures... separation hostile... floors...
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace {

template <typename Value>
Value read(std::istream& scenario) {
  Value value{};
  scenario >> value;
  return value;
}

// Reads a length, then that many items with `read_item`.
template <typename ReadItem>
auto read_list(std::istream& scenario, ReadItem read_item) {
  const auto length = read<std::size_t>(scenario);
  std::vector<decltype(read_item(scenario))> items;
  items.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    items.push_back(read_item(scenario));
  }
  return items;
}

murim::Cell read_cell(std::istream& scenario) {
  const auto column = read<std::int32_t>(scenario);
  return murim::Cell{column, read<std::int32_t>(scenario)};
}

murim::Skill read_skill(std::istream& scenario) {
  murim::Skill skill{};
  scenario >> skill.range >> skill.radius >> skill.body_damage >>
      skill.structure_damage >> skill.cooldown;
  return skill;
}

murim::Recruit read_member(std::istream& scenario) {
  const murim::Cell cell = read_cell(scenario);
  return murim::Recruit{cell, read<std::uint64_t>(scenario)};
}

murim::ArmySpec read_army(std::istream& scenario) {
  murim::ArmySpec army{};
  army.objective = read_cell(scenario);
  army.members = read_list(scenario, read_member);
  scenario >> army.faction;
  army.skills = read_list(scenario, read_skill);
  return army;
}

murim::Structure read_structure(std::istream& scenario) {
  const murim::Cell cell = read_cell(scenario);
  return murim::Structure{cell, read<std::uint32_t>(scenario)};
}

murim::Rules read_rules(std::istream& scenario) {
  murim::Rules rules{};
  scenario >> rules.separation;
  rules.hostile = read_list(scenario, [](std::istream& row) {
    return read_list(row,
                     [](std::istream& cell) { return read<int>(cell) != 0; });
  });
  for (std::uint32_t& floor : rules.stage_floors) {
    scenario >> floor;
  }
  return rules;
}

}  // namespace

int main(int argc, char** argv) {
  // The one handler for an unreadable scenario and for the core's refusal.
  try {
    const std::vector<std::string> args(argv, std::next(argv, argc));
    if (args.size() != 2) {
      std::cerr << "usage: world_hash '<scenario>'\n";
      return 2;
    }
    std::istringstream scenario(args.back());
    scenario.exceptions(std::istream::failbit | std::istream::badbit);
    const auto seed = read<std::uint64_t>(scenario);
    const auto ticks = read<std::uint64_t>(scenario);
    const std::vector<std::string> rows =
        read_list(scenario, read<std::string>);
    std::vector<murim::ArmySpec> armies = read_list(scenario, read_army);
    std::vector<murim::Structure> structures =
        read_list(scenario, read_structure);
    const std::size_t army_total = armies.size();
    murim::World world{murim::Seed{seed}, murim::Map{rows}, std::move(armies),
                       std::move(structures), read_rules(scenario)};
    for (std::uint64_t i = 0; i < ticks; ++i) {
      world.tick();
    }
    std::cout << static_cast<std::int64_t>(
                     static_cast<std::uint64_t>(world.hash()))
              << '\n';
    for (std::size_t army = 0; army < army_total; ++army) {
      for (const std::size_t member : world.roster(army)) {
        const murim::Person& person = world.person(member);
        std::cout << army << ' ' << person.position.x << ' '
                  << person.position.y << ' ' << person.health << '\n';
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "world_hash: " << error.what() << '\n';
    return 1;
  }
}
