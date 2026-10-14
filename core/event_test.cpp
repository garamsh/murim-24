#include "event.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "map.hpp"
#include "person.hpp"
#include "village.hpp"

namespace murim {
namespace {

constexpr std::size_t CULT = 0;
constexpr std::size_t ORTHODOX = 1;
constexpr std::int32_t REACH = 5;
constexpr Cell HOME{10, 10};
constexpr Cell FAR{40, 40};
// Persons: 0 listens; 1 is its spouse, 2 its child, 3 of its family, 4 a
// stranger.
constexpr std::size_t LISTENER = 0;
constexpr std::size_t SPOUSE = 1;
constexpr std::size_t CHILD = 2;
constexpr std::size_t COUSIN = 3;
constexpr std::size_t STRANGER = 4;
constexpr std::size_t FAMILY = 7;
constexpr std::size_t OTHER_FAMILY = 8;

std::vector<Person> people() {
  std::vector<Person> people(STRANGER + 1);
  for (Person& person : people) {
    person.position = centre_of(HOME);
    person.family = OTHER_FAMILY + 1;
  }
  people.at(LISTENER).family = FAMILY;
  people.at(LISTENER).spouses = {SPOUSE};
  people.at(LISTENER).children = {CHILD};
  people.at(CHILD).parents = {LISTENER, SPOUSE};
  people.at(COUSIN).family = FAMILY;
  people.at(STRANGER).family = OTHER_FAMILY;
  return people;
}

Event at(Cell cell, Happening what) { return Event{0, 0, cell, what}; }

TEST(Events, NameOnlyRecordsTheWorldHolds) {
  const Records held{2, 5, 3, 4, 2};
  // Control: every kind naming records within the counts is held.
  EXPECT_TRUE(names_held(Founded{2, 1}, held));
  EXPECT_TRUE(names_held(Raided{2, 1, 0}, held));
  EXPECT_TRUE(names_held(Ruined{2, 1, Destroyer{3, 0}}, held));
  EXPECT_TRUE(names_held(SectFell{1, 1}, held));
  EXPECT_TRUE(names_held(LeaderChanged{1, 4, ChosenBy::DUEL}, held));
  EXPECT_TRUE(names_held(Born{4}, held));
  EXPECT_TRUE(names_held(Died{4}, held));
  // Each one past its count is not.
  EXPECT_FALSE(names_held(Founded{3, 1}, held));
  EXPECT_FALSE(names_held(Founded{2, 2}, held));
  EXPECT_FALSE(names_held(Raided{3, 1, 0}, held));
  EXPECT_FALSE(names_held(Raided{2, 2, 0}, held));
  EXPECT_FALSE(names_held(Raided{2, 1, 2}, held));
  EXPECT_FALSE(names_held(Ruined{3, 1, Destroyer{3, 0}}, held));
  EXPECT_FALSE(names_held(Ruined{2, 2, Destroyer{3, 0}}, held));
  EXPECT_FALSE(names_held(Ruined{2, 1, Destroyer{4, 0}}, held));
  EXPECT_FALSE(names_held(Ruined{2, 1, Destroyer{3, 2}}, held));
  EXPECT_FALSE(names_held(SectFell{2, 1}, held));
  EXPECT_FALSE(names_held(SectFell{1, 2}, held));
  EXPECT_FALSE(names_held(LeaderChanged{2, 4, ChosenBy::DUEL}, held));
  EXPECT_FALSE(names_held(LeaderChanged{1, 5, ChosenBy::DUEL}, held));
  EXPECT_FALSE(names_held(Born{5}, held));
  EXPECT_FALSE(names_held(Died{5}, held));
}

TEST(Events, CountAPersonsFamilySpousesParentsAndChildrenAsKin) {
  const std::vector<Person> kin = people();
  EXPECT_TRUE(related(kin, LISTENER, LISTENER));
  EXPECT_TRUE(related(kin, LISTENER, SPOUSE));
  EXPECT_TRUE(related(kin, LISTENER, CHILD));
  EXPECT_TRUE(related(kin, CHILD, LISTENER));
  EXPECT_TRUE(related(kin, LISTENER, COUSIN));
  // Control: a stranger is no kin.
  EXPECT_FALSE(related(kin, LISTENER, STRANGER));
  EXPECT_THROW(static_cast<void>(related(kin, LISTENER, kin.size())),
               std::out_of_range);
}

TEST(Events, ReachAListenerNearbyOrWhenTheyConcernItsKinOrFaction) {
  std::vector<Person> kin = people();
  const Listener listener{LISTENER, CULT, REACH};
  // Anything within reach is heard, a stranger's birth included.
  EXPECT_TRUE(heard_by(at(HOME, Born{STRANGER}), listener, kin));
  EXPECT_TRUE(heard_by(at(Cell{HOME.x + REACH, HOME.y - REACH}, Born{STRANGER}),
                       listener, kin));
  // Control: one cell further, a stranger's birth is not.
  EXPECT_FALSE(heard_by(at(Cell{HOME.x + REACH + 1, HOME.y}, Born{STRANGER}),
                        listener, kin));
  EXPECT_FALSE(heard_by(at(Cell{HOME.x, HOME.y + REACH + 1}, Born{STRANGER}),
                        listener, kin));
  // Far away, the births and deaths of its kin.
  EXPECT_TRUE(heard_by(at(FAR, Born{CHILD}), listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, Died{SPOUSE}), listener, kin));
  EXPECT_FALSE(heard_by(at(FAR, Died{STRANGER}), listener, kin));
  // Far away, what befell its faction, as owner, raider or destroyer.
  EXPECT_TRUE(heard_by(at(FAR, Founded{0, CULT}), listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, Raided{0, ORTHODOX, CULT}), listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, Raided{0, CULT, ORTHODOX}), listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, Ruined{0, CULT, Destroyer{0, ORTHODOX}}),
                       listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, Ruined{0, ORTHODOX, Destroyer{0, CULT}}),
                       listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, SectFell{0, CULT}), listener, kin));
  EXPECT_TRUE(heard_by(at(FAR, LeaderChanged{CULT, STRANGER, ChosenBy::DUEL}),
                       listener, kin));
  // Control: what befell another faction alone is not heard.
  EXPECT_FALSE(heard_by(at(FAR, Founded{0, ORTHODOX}), listener, kin));
  EXPECT_FALSE(heard_by(at(FAR, Raided{0, ORTHODOX, ORTHODOX}), listener, kin));
  EXPECT_FALSE(heard_by(at(FAR, Ruined{0, ORTHODOX, Destroyer{0, ORTHODOX}}),
                        listener, kin));
  EXPECT_FALSE(heard_by(at(FAR, SectFell{0, ORTHODOX}), listener, kin));
  EXPECT_FALSE(
      heard_by(at(FAR, LeaderChanged{ORTHODOX, STRANGER, ChosenBy::DUEL}),
               listener, kin));
  // One serving in no army hears no faction's news from afar.
  const Listener unserving{LISTENER, std::nullopt, REACH};
  EXPECT_FALSE(heard_by(at(FAR, Founded{0, CULT}), unserving, kin));
  // Where the listener stands decides what is near: moved far, the far
  // stranger's death is heard.
  kin.at(LISTENER).position = centre_of(FAR);
  EXPECT_TRUE(heard_by(at(FAR, Died{STRANGER}), listener, kin));
}

TEST(Events, GiveTheRumoursAListenerHearsNewestFirst) {
  const std::vector<Person> kin = people();
  const Listener listener{LISTENER, CULT, REACH};
  std::vector<Event> events{at(FAR, Born{CHILD}), at(FAR, Born{STRANGER}),
                            at(FAR, Founded{0, CULT})};
  for (std::size_t index = 0; index < events.size(); ++index) {
    events.at(index).sequence = index;
  }
  const std::vector<Event> heard = rumours(events, listener, kin);
  ASSERT_EQ(heard.size(), 2U);
  EXPECT_EQ(heard.at(0).sequence, 2U);
  EXPECT_EQ(heard.at(1).sequence, 0U);
  // Control: no events, no rumours.
  EXPECT_TRUE(rumours({}, listener, kin).empty());
}

}  // namespace
}  // namespace murim
