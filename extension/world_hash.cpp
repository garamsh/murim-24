// Prints the core's hash after running a scenario, its bits read as a signed
// int as GDScript reads them: the reference extension/world_adapter_test.gd
// checks the adapter against. Armies are `sx,sy,ox,oy,count,strength` joined
// by `;`, and every argument after them is a map row.
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace {

std::vector<murim::ArmySpec> parse_armies(const std::string& text) {
  std::vector<murim::ArmySpec> armies;
  std::istringstream list(text);
  std::string army;
  while (std::getline(list, army, ';')) {
    std::istringstream fields(army);
    murim::ArmySpec spec{};
    char comma = 0;
    fields >> spec.start.x >> comma >> spec.start.y >> comma >>
        spec.objective.x >> comma >> spec.objective.y >> comma >> spec.count >>
        comma >> spec.strength;
    armies.push_back(spec);
  }
  return armies;
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<std::string> args(argv + 1, argv + argc);
  if (args.size() < 5) {
    std::cerr << "usage: world_hash <seed> <ticks> <separation> <armies> "
                 "<map row>...\n";
    return 2;
  }
  murim::World world{
      murim::Seed{static_cast<std::uint64_t>(std::stoll(args.at(0)))},
      murim::Map{std::vector<std::string>(args.begin() + 4, args.end())},
      parse_armies(args.at(3)), std::stoi(args.at(2))};
  const long long ticks = std::stoll(args.at(1));
  for (long long i = 0; i < ticks; ++i) {
    world.tick();
  }
  std::cout << static_cast<std::int64_t>(
                   static_cast<std::uint64_t>(world.hash()))
            << '\n';
  return 0;
}
