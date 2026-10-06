#pragma once

#include <cstdint>

namespace murim {

/// A seeded stream of integers: Knuth's MMIX linear congruential generator,
/// whose high bits are the output. The same seed gives the same stream.
class Random {
 public:
  explicit Random(std::uint64_t seed);

  /// Returns the next draw in [0, bound); `bound` must be positive.
  std::int32_t below(std::int32_t bound);

 private:
  std::uint64_t state_;
};

}  // namespace murim
