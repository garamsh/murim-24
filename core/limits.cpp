#include "limits.hpp"

#include <cstdint>

#include "map.hpp"

namespace murim {

bool reachable(std::int32_t units) { return units >= 0 && units <= MAX_REACH; }

bool spans(Footprint footprint) {
  const auto side = [](std::int32_t cells) {
    return cells >= 1 && cells <= MAX_FOOTPRINT;
  };
  return side(footprint.width) && side(footprint.height);
}

}  // namespace murim
