#include "random.hpp"

#include <cstdint>

namespace murim {

Random::Random(std::uint64_t seed) : state_(seed) {}

std::int32_t Random::below(std::int32_t bound) {
  constexpr std::uint64_t MULTIPLIER = 6364136223846793005ULL;
  constexpr std::uint64_t INCREMENT = 1442695040888963407ULL;
  constexpr unsigned SHIFT = 33U;
  state_ = (state_ * MULTIPLIER) + INCREMENT;
  return static_cast<std::int32_t>((state_ >> SHIFT) %
                                   static_cast<std::uint64_t>(bound));
}

}  // namespace murim
