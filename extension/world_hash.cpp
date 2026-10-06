// Prints the core's state hash for a seed and a tick count, as the reference
// the Godot test compares the adapter against. The hash's bits are printed as a
// signed integer, as GDScript reads them.
#include <cstdint>
#include <iostream>
#include <string>

#include "world.hpp"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: world_hash <seed> <ticks>\n";
    return 2;
  }
  murim::World world{
      murim::Seed{static_cast<std::uint64_t>(std::stoll(argv[1]))}};
  const long long ticks = std::stoll(argv[2]);
  for (long long i = 0; i < ticks; ++i) {
    world.tick();
  }
  std::cout << static_cast<std::int64_t>(
                   static_cast<std::uint64_t>(world.hash()))
            << '\n';
  return 0;
}
