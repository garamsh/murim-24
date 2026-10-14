#include "progress.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "arts.hpp"

namespace murim {
namespace {

constexpr std::size_t PEOPLE = 3;

TEST(Progress, NamesOnlyPeopleArtsSkillsAndRealmsTheWorldHolds) {
  std::vector<Art> arts(2);
  arts.at(1).skills.resize(2);
  const std::vector<Realm> realms(3);
  // Control: a person, art, skill and realm within the counts are held.
  EXPECT_TRUE(names_held(Learned{2, 1, 1}, PEOPLE, arts, realms));
  EXPECT_TRUE(names_held(RealmGained{2, 3}, PEOPLE, arts, realms));
  EXPECT_TRUE(names_held(RealmGained{0, 1}, PEOPLE, arts, realms));
  EXPECT_FALSE(names_held(Learned{3, 1, 1}, PEOPLE, arts, realms));
  EXPECT_FALSE(names_held(Learned{2, 2, 0}, PEOPLE, arts, realms));
  EXPECT_FALSE(names_held(Learned{2, 1, 2}, PEOPLE, arts, realms));
  // Art 0 has no skill to learn.
  EXPECT_FALSE(names_held(Learned{2, 0, 0}, PEOPLE, arts, realms));
  EXPECT_FALSE(names_held(RealmGained{3, 1}, PEOPLE, arts, realms));
  EXPECT_FALSE(names_held(RealmGained{2, 4}, PEOPLE, arts, realms));
  // Ranks count from 1.
  EXPECT_FALSE(names_held(RealmGained{2, 0}, PEOPLE, arts, realms));
}

}  // namespace
}  // namespace murim
