#include "diplomacy.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace murim {
namespace {

constexpr std::size_t FACTIONS = 3;

TEST(Declarations, HoldForBothFactionsWhicheverNamesThemFirst) {
  Declarations declarations{FACTIONS};
  EXPECT_FALSE(declarations.between(2, 0).has_value());
  declarations.declare(2, 0, Declared{Declaration::WAR, 2, 4});
  const std::optional<Declared> forward = declarations.between(0, 2);
  const std::optional<Declared> backward = declarations.between(2, 0);
  ASSERT_TRUE(forward.has_value());
  ASSERT_TRUE(backward.has_value());
  EXPECT_EQ(forward.value_or(Declared{}).by, 2U);
  EXPECT_EQ(backward.value_or(Declared{}).tick, 4U);
  // Control: a pair no one declared toward stays empty.
  EXPECT_FALSE(declarations.between(0, 1).has_value());
  EXPECT_THROW(static_cast<void>(declarations.between(0, FACTIONS)),
               std::out_of_range);
}

TEST(Declarations, RefuseSlotsNotAsDeclareKeepsThem) {
  // Control: slots as declare() makes them are kept.
  Declarations made{FACTIONS};
  made.declare(1, 0, Declared{Declaration::PEACE, 1, 3});
  EXPECT_TRUE(made.kept(3));

  const auto with = [](std::size_t slot, Declared declared) {
    std::vector<std::optional<Declared>> slots(FACTIONS * FACTIONS);
    slots.at(slot) = declared;
    return Declarations{FACTIONS, slots};
  };
  // Row 0, column 1 is the pair of factions 0 and 1.
  constexpr std::size_t PAIR = 1;
  // Control: the same slot through the saved form is kept.
  EXPECT_TRUE(with(PAIR, Declared{Declaration::WAR, 0, 3}).kept(3));
  // Under the higher faction's row.
  EXPECT_FALSE(with(FACTIONS, Declared{Declaration::WAR, 1, 3}).kept(3));
  // A faction toward itself.
  EXPECT_FALSE(with(0, Declared{Declaration::WAR, 0, 3}).kept(3));
  // Made by neither faction of the pair.
  EXPECT_FALSE(with(PAIR, Declared{Declaration::WAR, 2, 3}).kept(3));
  // Made later than now.
  EXPECT_FALSE(with(PAIR, Declared{Declaration::WAR, 0, 3}).kept(2));
  // Not one place for each two factions.
  EXPECT_FALSE(
      (Declarations{FACTIONS, std::vector<std::optional<Declared>>(FACTIONS)})
          .kept(0));
}

// Faction 0 is hostile to 1 by its stance, and 1 is not to 0; 2 is at peace
// with both by stance.
Stances stances() {
  return {{false, true, false}, {false, false, false}, {false, false, false}};
}

TEST(Declarations, MakeFactionsHostileByTheirDeclarationElseTheirStances) {
  Declarations declarations{FACTIONS};
  const Stances stance = stances();
  // Control: with no declaration the stances decide, one way only.
  EXPECT_TRUE(declarations.hostile(0, 1, stance));
  EXPECT_FALSE(declarations.hostile(1, 0, stance));
  declarations.declare(1, 0, Declared{Declaration::PEACE, 1, 0});
  EXPECT_FALSE(declarations.hostile(0, 1, stance));
  declarations.declare(2, 1, Declared{Declaration::WAR, 2, 0});
  EXPECT_TRUE(declarations.hostile(1, 2, stance));
  EXPECT_TRUE(declarations.hostile(2, 1, stance));
  // Control: a pair no one declared toward keeps its stance.
  EXPECT_FALSE(declarations.hostile(0, 2, stance));
}

TEST(Declarations, TellHowOneFactionStandsTowardAnother) {
  Declarations declarations{FACTIONS};
  const Stances stance = stances();
  EXPECT_EQ(declarations.relation(1, 1, stance), Relation::OWN);
  // Either one hostile by stance makes the two hostile.
  EXPECT_EQ(declarations.relation(1, 0, stance), Relation::HOSTILE);
  EXPECT_EQ(declarations.relation(0, 1, stance), Relation::HOSTILE);
  EXPECT_EQ(declarations.relation(0, 2, stance), Relation::ALLY);
  declarations.declare(0, 2, Declared{Declaration::WAR, 0, 0});
  EXPECT_EQ(declarations.relation(2, 0, stance), Relation::AT_WAR);
  declarations.declare(0, 1, Declared{Declaration::PEACE, 0, 0});
  EXPECT_EQ(declarations.relation(1, 0, stance), Relation::NEUTRAL);
}

TEST(Declarations, RefuseADeclarationForTheFirstReasonInOrder) {
  constexpr std::uint64_t HOLD = 10;
  constexpr std::uint64_t MADE = 5;
  Declarations declarations{FACTIONS};
  const Stances stance = stances();
  // Control: war on an ally, peace with a foe by stance, are allowed.
  EXPECT_EQ(declarations.refusal(0, 2, Declaration::WAR, stance, 0, HOLD),
            DeclarationRefusal::NONE);
  EXPECT_EQ(declarations.refusal(1, 0, Declaration::PEACE, stance, 0, HOLD),
            DeclarationRefusal::NONE);
  EXPECT_EQ(declarations.refusal(1, 1, Declaration::WAR, stance, 0, HOLD),
            DeclarationRefusal::SAME_FACTION);
  EXPECT_EQ(declarations.refusal(2, 0, Declaration::PEACE, stance, 0, HOLD),
            DeclarationRefusal::ALREADY_AT_PEACE);
  declarations.declare(0, 2, Declared{Declaration::WAR, 0, MADE});
  // At war, and too soon: being at war is named first.
  EXPECT_EQ(
      declarations.refusal(2, 0, Declaration::WAR, stance, MADE + 1, HOLD),
      DeclarationRefusal::ALREADY_AT_WAR);
  EXPECT_EQ(
      declarations.refusal(2, 0, Declaration::PEACE, stance, MADE + 1, HOLD),
      DeclarationRefusal::TOO_SOON);
  // Control: once the hold has passed, peace may be made.
  EXPECT_EQ(
      declarations.refusal(2, 0, Declaration::PEACE, stance, MADE + HOLD, HOLD),
      DeclarationRefusal::NONE);
  declarations.declare(0, 2, Declared{Declaration::PEACE, 2, MADE + HOLD});
  EXPECT_EQ(
      declarations.refusal(0, 2, Declaration::PEACE, stance, MADE + HOLD, HOLD),
      DeclarationRefusal::ALREADY_AT_PEACE);
}

TEST(Declarations, SeekWarWhenStrongAndPeaceWhenWeak) {
  constexpr Diplomacy DIPLOMACY{1'500, 100, 200, 0};
  Declarations declarations{FACTIONS};
  const Stances stance = stances();
  // Faction 2 is allied with 0: war once it numbers 1.5 times 0's people.
  const std::optional<Sought> war =
      declarations.sought(DIPLOMACY, 2, 0, {10, 10, 15}, stance);
  ASSERT_TRUE(war.has_value());
  EXPECT_EQ(war.value_or(Sought{}).what, Declaration::WAR);
  EXPECT_EQ(war.value_or(Sought{}).chance, 100U);
  // Control: below the margin it seeks nothing.
  EXPECT_FALSE(declarations.sought(DIPLOMACY, 2, 0, {10, 10, 14}, stance));
  // Hostile by stance and not at war, it seeks neither.
  EXPECT_FALSE(declarations.sought(DIPLOMACY, 1, 0, {1, 100, 1}, stance));
  // Nothing toward itself or a faction with no one living.
  EXPECT_FALSE(declarations.sought(DIPLOMACY, 2, 2, {10, 10, 15}, stance));
  EXPECT_FALSE(declarations.sought(DIPLOMACY, 2, 0, {0, 10, 15}, stance));
  declarations.declare(0, 2, Declared{Declaration::WAR, 0, 0});
  const std::optional<Sought> peace =
      declarations.sought(DIPLOMACY, 2, 0, {10, 10, 9}, stance);
  ASSERT_TRUE(peace.has_value());
  EXPECT_EQ(peace.value_or(Sought{}).what, Declaration::PEACE);
  EXPECT_EQ(peace.value_or(Sought{}).chance, 200U);
  // Control: at war and not fewer, it seeks nothing.
  EXPECT_FALSE(declarations.sought(DIPLOMACY, 2, 0, {10, 10, 10}, stance));
}

TEST(Declarations, TurnAWinnersAlliesHostileToItAlone) {
  Stances stance = stances();
  turn_allies_hostile(stance, 2);
  // Factions 0 and 1 were at peace with 2: both now attack it.
  EXPECT_TRUE(stance.at(0).at(2));
  EXPECT_TRUE(stance.at(1).at(2));
  // Control: how 2 stands toward them, and the others' stances toward each
  // other, are as they were.
  EXPECT_FALSE(stance.at(2).at(0));
  EXPECT_FALSE(stance.at(2).at(1));
  EXPECT_FALSE(stance.at(2).at(2));
  EXPECT_TRUE(stance.at(0).at(1));
  EXPECT_FALSE(stance.at(1).at(0));

  Stances wary = stances();
  turn_allies_hostile(wary, 1);
  // 0 was already hostile to 1 and stays so; 2 was at peace with 1.
  EXPECT_TRUE(wary.at(0).at(1));
  EXPECT_TRUE(wary.at(2).at(1));
  // Control: 1 itself turns on no one.
  EXPECT_FALSE(wary.at(1).at(0));

  Stances feared = stances();
  turn_allies_hostile(feared, 0);
  // 0 is hostile to 1 by its stance, so 1 is no ally of it and does not turn;
  // 2 was at peace with 0 and does.
  EXPECT_FALSE(feared.at(1).at(0));
  EXPECT_TRUE(feared.at(2).at(0));
}

}  // namespace
}  // namespace murim
