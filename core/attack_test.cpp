#include "attack.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace murim {
namespace {

constexpr std::size_t PEOPLE = 4;
constexpr std::size_t FOE = 3;

// Person `person` on slot `slot` of `ring` round FOE, holding a token or not.
Engaged on(std::size_t person, Ring ring, std::uint8_t slot, bool token) {
  return Engaged{person, Engagement{FOE, ring, slot, token, 0, 0}};
}

TEST(Tokens, ATableIsValidWithARowForNoRealmAndHoldsWithinBounds) {
  const Tokens region{{2, 2, 3, 3, 4, 4, 6}, 30, 90};
  // Control: the region's table is valid, so each refusal is its change's.
  EXPECT_TRUE(valid(region));
  Tokens refused = region;
  refused.by_rank.clear();
  EXPECT_FALSE(valid(refused));
  refused = region;
  refused.by_rank.back() = MAX_TOKENS + 1;
  EXPECT_FALSE(valid(refused));
  refused = region;
  refused.circling = MAX_HOLD_TICKS + 1;
  EXPECT_FALSE(valid(refused));
  refused = region;
  refused.lease = MAX_HOLD_TICKS + 1;
  EXPECT_FALSE(valid(refused));
  // The bounds themselves are accepted.
  const Tokens widest{{MAX_TOKENS}, MAX_HOLD_TICKS, MAX_HOLD_TICKS};
  EXPECT_TRUE(valid(widest));
}

TEST(Attacks, AnEngagedPersonHoldsItsSlotAndTokenUntilReleased) {
  Attacks attacks{PEOPLE};
  ASSERT_FALSE(attacks.any());
  ASSERT_EQ(attacks.people(), PEOPLE);

  constexpr std::uint8_t SLOT = 5;
  constexpr std::uint32_t WAITING_SINCE = 40;
  constexpr std::uint32_t TOKEN_SINCE = 70;
  attacks.engage(
      0, Engagement{FOE, Ring::LONG, SLOT, true, WAITING_SINCE, TOKEN_SINCE});
  const std::optional<Engagement> held = attacks.engagement(0);
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(held->foe, FOE);
  EXPECT_EQ(held->ring, Ring::LONG);
  EXPECT_EQ(held->slot, SLOT);
  EXPECT_TRUE(held->token);
  EXPECT_EQ(held->waiting_since, WAITING_SINCE);
  EXPECT_EQ(held->token_since, TOKEN_SINCE);
  EXPECT_TRUE(attacks.any());
  EXPECT_TRUE(attacks.taken(FOE, Ring::LONG, SLOT));
  // Control: the same slot on the other rings, and the next on this one,
  // stay free.
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, SLOT));
  EXPECT_FALSE(attacks.taken(FOE, Ring::WAITING, SLOT));
  EXPECT_FALSE(attacks.taken(FOE, Ring::LONG, SLOT + 1));
  EXPECT_EQ(attacks.tokens_held(FOE), 1U);
  EXPECT_EQ(attacks.engaged().size(), 1U);

  attacks.release(0);
  EXPECT_FALSE(attacks.engagement(0).has_value());
  EXPECT_FALSE(attacks.taken(FOE, Ring::LONG, SLOT));
  EXPECT_EQ(attacks.tokens_held(FOE), 0U);
  EXPECT_FALSE(attacks.any());
}

TEST(Attacks, GrowingKeepsWhatEveryoneHolds) {
  Attacks attacks{PEOPLE};
  attacks.engage(0, Engagement{FOE, Ring::NEAR, 1, true, 0, 0});
  attacks.grow_to(2 * PEOPLE);
  EXPECT_EQ(attacks.people(), 2 * PEOPLE);
  EXPECT_EQ(attacks.engagement(0)->slot, 1);
  EXPECT_EQ(attacks.tokens_held(FOE), 1U);
  // A person past the old room engages once there is room for it.
  attacks.engage(PEOPLE, Engagement{FOE, Ring::NEAR, 2, true, 0, 0});
  EXPECT_EQ(attacks.tokens_held(FOE), 2U);
  // Control: growing to fewer people keeps the room there is.
  attacks.grow_to(1);
  EXPECT_EQ(attacks.people(), 2 * PEOPLE);
}

TEST(Attacks, EngagingAgainMovesAPersonOffItsOldSlot) {
  Attacks attacks{PEOPLE};
  attacks.engage(0, Engagement{FOE, Ring::NEAR, 1, true, 0, 0});
  attacks.engage(0, Engagement{FOE, Ring::WAITING, 2, false, 0, 0});
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, 1));
  EXPECT_TRUE(attacks.taken(FOE, Ring::WAITING, 2));
  EXPECT_EQ(attacks.tokens_held(FOE), 0U);
  EXPECT_EQ(attacks.engaged().size(), 1U);
}

TEST(Attacks, ASavedEngagementIsRefusedWhenNoWorldCouldHoldIt) {
  const std::vector<Engaged> kept{on(0, Ring::NEAR, 0, true),
                                  on(1, Ring::NEAR, 1, true),
                                  on(2, Ring::WAITING, 0, false)};
  // Control: the intact entries give back the same engagements.
  const std::optional<Attacks> intact = Attacks::of(PEOPLE, kept);
  ASSERT_TRUE(intact.has_value());
  EXPECT_EQ(intact->engaged().size(), kept.size());
  EXPECT_EQ(intact->tokens_held(FOE), 2U);
  EXPECT_TRUE(intact->taken(FOE, Ring::WAITING, 0));

  const std::vector<std::vector<Engaged>> refused{
      // A person past the people.
      {on(PEOPLE, Ring::NEAR, 0, true)},
      // A foe past the people.
      {Engaged{0, Engagement{PEOPLE, Ring::NEAR, 0, true, 0, 0}}},
      // A person engaging itself.
      {on(FOE, Ring::NEAR, 0, true)},
      // A slot past the most a ring holds.
      {on(0, Ring::NEAR, MAX_SLOTS, true)},
      // A waiter holding a token.
      {on(0, Ring::WAITING, 0, true)},
      // One person given twice.
      {on(0, Ring::NEAR, 0, true), on(0, Ring::NEAR, 1, true)},
      // Two people on one slot.
      {on(0, Ring::NEAR, 0, true), on(1, Ring::NEAR, 0, false)},
  };
  for (std::size_t form = 0; form < refused.size(); ++form) {
    EXPECT_FALSE(Attacks::of(PEOPLE, refused.at(form)).has_value()) << form;
  }
}

}  // namespace
}  // namespace murim
