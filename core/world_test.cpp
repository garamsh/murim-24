#include "world.hpp"

#include <gtest/gtest.h>

namespace murim {
namespace {

constexpr int TICKS = 10'000;

StateHash run(Seed seed) {
  World world{seed};
  for (int i = 0; i < TICKS; ++i) {
    world.tick();
  }
  return world.hash();
}

TEST(World, SameSeedGivesSameHashAfterTenThousandTicks) {
  const Seed seed{42};
  // The run must have changed the state, or equal hashes would prove nothing.
  ASSERT_NE(run(seed), World{seed}.hash());

  EXPECT_EQ(run(seed), run(seed));
  // Control: a different seed must diverge, so equality is not a constant hash.
  EXPECT_NE(run(seed), run(Seed{43}));
}

}  // namespace
}  // namespace murim
