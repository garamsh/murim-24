#include "diplomacy.hpp"

#include <gtest/gtest.h>

#include <cstddef>
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

}  // namespace
}  // namespace murim
