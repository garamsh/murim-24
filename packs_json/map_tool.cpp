// The build tool for a hand-made map (ADR 0047): writes the coarse graph of
// the map a set of packs paints into GRAPH_FILE of the pack painting it, so a
// world on the map takes the graph rather than building it.

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

#include "content.hpp"
#include "json_packs.hpp"

namespace {

// Writes the graph of the map `base` and `mods` paint, from the pack
// directories under `root`, into the pack painting it; returns the exit
// status.
int ship(const std::filesystem::path& root, const std::string& base,
         const std::vector<std::string>& mods) {
  const murim_packs::DirectoryPacks source{root};
  const auto shipping = murim::graph_to_ship(source, base, mods);
  if (const auto* errors =
          std::get_if<std::vector<murim::ContentError>>(&shipping)) {
    for (const murim::ContentError& error : *errors) {
      std::cerr << "refused: " << error.pack << " " << error.document << " "
                << error.entry << " " << error.path << "\n";
    }
    return EXIT_FAILURE;
  }
  if (std::holds_alternative<murim::NoMap>(shipping)) {
    std::cout << "the packs paint no map, so no graph was written\n";
    return EXIT_SUCCESS;
  }
  const auto& made = std::get<murim::GraphToShip>(shipping);
  std::string bytes;
  bytes.reserve(made.bytes.size());
  for (const std::uint8_t byte : made.bytes) {
    bytes.push_back(static_cast<char>(byte));
  }
  const std::filesystem::path file = root / made.pack / murim_packs::GRAPH_FILE;
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << bytes;
  if (!out) {
    std::cerr << "could not write " << file.string() << "\n";
    return EXIT_FAILURE;
  }
  std::cout << "wrote " << bytes.size() << " bytes to " << file.string()
            << "\n";
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  constexpr int LEAST_ARGUMENTS = 3;
  if (argc < LEAST_ARGUMENTS) {
    std::cerr << "usage: murim_map <packs directory> <base pack> [<mod>...]\n";
    return 2;
  }
  try {
    const std::vector<std::string> given(argv, std::next(argv, argc));
    return ship(std::filesystem::path{given.at(1)}, given.at(2),
                std::vector<std::string>(
                    std::next(given.begin(), LEAST_ARGUMENTS), given.end()));
  } catch (const std::exception& error) {
    std::cerr << "murim_map: " << error.what() << "\n";
    return 2;
  }
}
