// Prints the core's hash after running a scenario, its bits read as a signed
// int as GDScript reads them: the reference extension/world_adapter_test.gd
// checks the adapter against. Armies are `sx,sy,ox,oy,count,strength` joined
// by `;`, and every argument after them is a map row.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
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

// Positions in the arguments, the program's name first.
constexpr std::size_t SEED = 1;
constexpr std::size_t TICKS = 2;
constexpr std::size_t SEPARATION = 3;
constexpr std::size_t ARMIES = 4;
constexpr std::size_t FIRST_MAP_ROW = 5;

int main(int argc, char** argv) {
  // The one handler for unparsable arguments and for the core's refusal.
  try {
    const std::vector<std::string> args(argv, std::next(argv, argc));
    if (args.size() <= FIRST_MAP_ROW) {
      std::cerr << "usage: world_hash <seed> <ticks> <separation> <armies> "
                   "<map row>...\n";
      return 2;
    }
    murim::World world{
        murim::Seed{static_cast<std::uint64_t>(std::stoll(args.at(SEED)))},
        murim::Map{std::vector<std::string>(
            std::next(args.begin(), FIRST_MAP_ROW), args.end())},
        parse_armies(args.at(ARMIES)), std::stoi(args.at(SEPARATION))};
    const long long ticks = std::stoll(args.at(TICKS));
    for (long long i = 0; i < ticks; ++i) {
      world.tick();
    }
    std::cout << static_cast<std::int64_t>(
                     static_cast<std::uint64_t>(world.hash()))
              << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "world_hash: " << error.what() << '\n';
    return 1;
  }
}
