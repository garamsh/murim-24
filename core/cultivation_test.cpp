#include "cultivation.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "arts.hpp"
#include "life.hpp"
#include "limits.hpp"
#include "person.hpp"
#include "talent.hpp"

namespace murim {

namespace {

// Art 0, orthodox yang: skills of rank 0 and 1, the second learned only from
// a teacher. Art 1, demonic yin: one skill of rank 1. Realms reached at 100
// and 300 qi.
constexpr std::size_t ORTHODOX_ART = 0;
constexpr std::size_t DEMONIC_ART = 1;
constexpr std::uint32_t FIRST_REALM = 100;
constexpr std::uint32_t SECOND_REALM = 300;
constexpr std::uint32_t ORIENTATION_EASE = 500;
constexpr std::uint32_t POLARITY_EASE = 800;
constexpr std::uint32_t ORIENTATION_POTENCY = 600;
constexpr std::uint32_t POLARITY_POTENCY = 900;
constexpr std::uint32_t COMPREHENSION = 200;
constexpr std::uint32_t TEACHING = 30;
constexpr std::uint32_t READING_COST = 40;
constexpr std::uint32_t MEAN_LIMIT = 50;

Life arts_life() {
  Life life;
  Art orthodox;
  orthodox.nature = Nature{Orientation::ORTHODOX, Polarity::YANG};
  orthodox.skills.resize(2);
  orthodox.skills.at(0).rank = 0;
  orthodox.skills.at(1).rank = 1;
  orthodox.skills.at(1).comprehensible = false;
  Art demonic;
  demonic.nature = Nature{Orientation::DEMONIC, Polarity::YIN};
  demonic.skills.resize(1);
  demonic.skills.at(0).rank = 1;
  life.arts = {orthodox, demonic};
  life.realms.resize(2);
  life.realms.at(0).qi = FIRST_REALM;
  life.realms.at(1).qi = SECOND_REALM;
  life.learning.orientation_ease = ORIENTATION_EASE;
  life.learning.polarity_ease = POLARITY_EASE;
  life.learning.orientation_potency = ORIENTATION_POTENCY;
  life.learning.polarity_potency = POLARITY_POTENCY;
  life.learning.comprehension = COMPREHENSION;
  life.learning.teaching = TEACHING;
  life.learning.reading_cost = READING_COST;
  life.disciplines.resize(1);
  life.disciplines.at(0).limit.mean = MEAN_LIMIT;
  return life;
}

Person knowing(std::vector<Known> known, std::uint32_t capacity) {
  Person person;
  person.known = std::move(known);
  person.qi_capacity = capacity;
  return person;
}

TEST(Cultivation, RanksAPersonByTheRealmsItsCapacityReaches) {
  const Life life = arts_life();
  EXPECT_EQ(rank_of(knowing({}, FIRST_REALM - 1), life), 0U);
  EXPECT_EQ(rank_of(knowing({}, FIRST_REALM), life), 1U);
  EXPECT_EQ(rank_of(knowing({}, SECOND_REALM), life), 2U);
}

TEST(Cultivation, TakesAPersonsNatureFromTheArtItLearnedFirst) {
  const Life life = arts_life();
  EXPECT_FALSE(nature_of(knowing({}, 0), life).has_value());
  const Person learned =
      knowing({Known{DEMONIC_ART, 0, 0}, Known{ORTHODOX_ART, 0, 0}}, 0);
  const std::optional<Nature> nature = nature_of(learned, life);
  ASSERT_TRUE(nature.has_value());
  EXPECT_EQ(nature.value_or(Nature{}).orientation, Orientation::DEMONIC);
  EXPECT_EQ(nature.value_or(Nature{}).polarity, Polarity::YIN);
}

TEST(Cultivation, NamesTheArtLearnedFirstAsTheHeartMethod) {
  EXPECT_FALSE(heart_of(knowing({}, 0)).has_value());
  const Person turned =
      knowing({Known{DEMONIC_ART, 0, 0}, Known{ORTHODOX_ART, 0, 0}}, 0);
  EXPECT_EQ(heart_of(turned), std::optional<std::size_t>{DEMONIC_ART});
  // Control: learned the other way about, the other art.
  const Person orthodox =
      knowing({Known{ORTHODOX_ART, 0, 0}, Known{DEMONIC_ART, 0, 0}}, 0);
  EXPECT_EQ(heart_of(orthodox), std::optional<std::size_t>{ORTHODOX_ART});
}

TEST(Cultivation, HoldsASkillWithinARealmItsRankDoesNotPass) {
  const Life life = arts_life();
  // Rank 0 reaches the orthodox art's first skill, of rank 0, alone.
  EXPECT_TRUE(within_realm(knowing({}, 0), ORTHODOX_ART, 0, life));
  EXPECT_FALSE(within_realm(knowing({}, 0), ORTHODOX_ART, 1, life));
  // Control: at the first realm, rank 1, the second skill too.
  EXPECT_TRUE(within_realm(knowing({}, FIRST_REALM), ORTHODOX_ART, 1, life));
  EXPECT_TRUE(within_realm(knowing({}, FIRST_REALM), DEMONIC_ART, 0, life));
  EXPECT_THROW(
      static_cast<void>(within_realm(knowing({}, 0), DEMONIC_ART, 1, life)),
      std::out_of_range);
}

TEST(Cultivation, EasesAnArtByHowFarItsNatureAgrees) {
  const Life life = arts_life();
  const Person orthodox = knowing({Known{ORTHODOX_ART, 0, 0}}, 0);
  // Control: one who knows no art, or an art of its own nature, agrees fully.
  EXPECT_EQ(agreement(knowing({}, 0), DEMONIC_ART, false, life), PER_MILLE);
  EXPECT_EQ(agreement(orthodox, ORTHODOX_ART, false, life), PER_MILLE);
  // Both axes differ: the two eases multiply, or the two potencies.
  EXPECT_EQ(agreement(orthodox, DEMONIC_ART, false, life),
            std::uint64_t{ORIENTATION_EASE} * POLARITY_EASE / PER_MILLE);
  EXPECT_EQ(agreement(orthodox, DEMONIC_ART, true, life),
            std::uint64_t{ORIENTATION_POTENCY} * POLARITY_POTENCY / PER_MILLE);
  // The heart method is the art learned first, whatever is learned after.
  const Person turned =
      knowing({Known{ORTHODOX_ART, 0, 0}, Known{DEMONIC_ART, 0, 0}}, 0);
  EXPECT_EQ(agreement(turned, ORTHODOX_ART, false, life), PER_MILLE);
  EXPECT_EQ(agreement(turned, DEMONIC_ART, false, life),
            std::uint64_t{ORIENTATION_EASE} * POLARITY_EASE / PER_MILLE);
  Life one_axis = arts_life();
  one_axis.arts.at(DEMONIC_ART).nature.polarity = Polarity::YANG;
  EXPECT_EQ(agreement(orthodox, DEMONIC_ART, false, one_axis),
            ORIENTATION_EASE);
}

TEST(Cultivation, OffersTheFirstSkillAPersonLacksWithinItsRank) {
  const Life life = arts_life();
  const Person novice = knowing({}, 0);
  EXPECT_EQ(lesson(novice, ORTHODOX_ART, Lesson::TAUGHT, 0, life),
            std::optional<std::size_t>{0});
  const Person adept = knowing({Known{ORTHODOX_ART, 0, 0}}, 0);
  // Control: a teacher offers the next skill once the rank allows it.
  EXPECT_EQ(lesson(adept, ORTHODOX_ART, Lesson::TAUGHT, 1, life),
            std::optional<std::size_t>{1});
  // Below its rank, nothing.
  EXPECT_FALSE(lesson(adept, ORTHODOX_ART, Lesson::TAUGHT, 0, life));
  // Comprehension passes over a skill only a teacher gives.
  EXPECT_FALSE(lesson(adept, ORTHODOX_ART, Lesson::COMPREHENDED, 1, life));
  EXPECT_EQ(lesson(adept, DEMONIC_ART, Lesson::COMPREHENDED, 1, life),
            std::optional<std::size_t>{0});
}

TEST(Cultivation, GrowsQiCapacityToItsLimitAndRecoversQiWithinIt) {
  constexpr std::uint32_t GROWTH = 7;
  constexpr std::uint32_t LIMIT = 105;
  Person person = knowing({}, FIRST_REALM);
  person.cultivation = Aptitude{GROWTH, LIMIT};
  cultivate(person);
  EXPECT_EQ(person.qi_capacity, LIMIT);
  // Control: at its limit, it stays.
  cultivate(person);
  EXPECT_EQ(person.qi_capacity, LIMIT);
  // Above its limit, it is never lowered.
  person.qi_capacity = LIMIT + GROWTH;
  cultivate(person);
  EXPECT_EQ(person.qi_capacity, LIMIT + GROWTH);
  // Below its limit by more than its growth, it grows by its growth.
  constexpr std::uint32_t LOW = 90;
  person.qi_capacity = LOW;
  cultivate(person);
  EXPECT_EQ(person.qi_capacity, LOW + GROWTH);

  // A recovery of 15 per mille of 100 is 1.5, rounded up to 2.
  constexpr std::uint32_t RECOVERY = 15;
  Cultivation cultivation;
  cultivation.recovery = RECOVERY;
  Person tired = knowing({}, FIRST_REALM);
  recover(tired, cultivation);
  EXPECT_EQ(tired.qi, 2U);
  tired.qi = FIRST_REALM - 1;
  recover(tired, cultivation);
  EXPECT_EQ(tired.qi, FIRST_REALM);
}

TEST(Cultivation, PractisesEachArtOnceInTheOrderFirstLearned) {
  const Person person =
      knowing({Known{DEMONIC_ART, 0, 0}, Known{ORTHODOX_ART, 0, 0},
               Known{DEMONIC_ART, 1, 0}},
              0);
  EXPECT_EQ(practised(person),
            (std::vector<std::size_t>{DEMONIC_ART, ORTHODOX_ART}));
  EXPECT_TRUE(practised(knowing({}, 0)).empty());
}

TEST(Cultivation, ScalesComprehensionByTalentAndAgreement) {
  const Life life = arts_life();
  Person gifted = knowing({Known{ORTHODOX_ART, 0, 0}}, 0);
  // Control: no talent drawn reads as the mean, so the chance is the rule's.
  EXPECT_EQ(comprehension_chance(gifted, ORTHODOX_ART, life), COMPREHENSION);
  gifted.talent = {Aptitude{0, 2 * MEAN_LIMIT}};
  EXPECT_EQ(comprehension_chance(gifted, ORTHODOX_ART, life),
            2 * COMPREHENSION);
  EXPECT_EQ(comprehension_chance(gifted, DEMONIC_ART, life),
            std::uint64_t{2} * COMPREHENSION * ORIENTATION_EASE *
                POLARITY_EASE / PER_MILLE / PER_MILLE);
}

TEST(Cultivation, LetsAReaderWithTheRankAndQiReadAManual) {
  const Life life = arts_life();
  Person reader = knowing({Known{ORTHODOX_ART, 0, 0}}, FIRST_REALM);
  reader.qi = READING_COST;
  // Control: with the rank and the qi, it reads, spending the cost.
  EXPECT_TRUE(may_read(reader, ORTHODOX_ART, life));
  EXPECT_EQ(reading_cost(reader, ORTHODOX_ART, life), READING_COST);
  reader.qi = READING_COST - 1;
  EXPECT_FALSE(may_read(reader, ORTHODOX_ART, life));
  // Without the rank for a skill it lacks, it reads nothing.
  Person low = knowing({Known{ORTHODOX_ART, 0, 0}}, 0);
  low.qi = READING_COST;
  EXPECT_FALSE(may_read(low, ORTHODOX_ART, life));
  // An art of another nature costs more, by how far it disagrees.
  reader.qi = SECOND_REALM;
  const std::uint64_t ease =
      std::uint64_t{ORIENTATION_EASE} * POLARITY_EASE / PER_MILLE;
  EXPECT_TRUE(may_read(reader, DEMONIC_ART, life));
  EXPECT_EQ(reading_cost(reader, DEMONIC_ART, life),
            std::uint64_t{READING_COST} * PER_MILLE / ease);
  // An art that wholly disagrees is never read.
  Life refused = arts_life();
  refused.learning.orientation_ease = 0;
  EXPECT_FALSE(may_read(reader, DEMONIC_ART, refused));
  EXPECT_THROW(static_cast<void>(reading_cost(reader, DEMONIC_ART, refused)),
               std::domain_error);
  // Not even when reading costs nothing.
  refused.learning.reading_cost = 0;
  EXPECT_FALSE(may_read(reader, DEMONIC_ART, refused));
  // Control: a free reading of an art that agrees is allowed.
  EXPECT_TRUE(may_read(reader, ORTHODOX_ART, refused));
}

TEST(Cultivation, ScalesTeachingByTheTeachersRankAndAgreement) {
  const Life life = arts_life();
  const Person pupil = knowing({Known{ORTHODOX_ART, 0, 0}}, 0);
  EXPECT_EQ(teaching_chance(pupil, ORTHODOX_ART, 2, life), 2 * TEACHING);
  EXPECT_EQ(teaching_chance(pupil, ORTHODOX_ART, 0, life), 0U);
  EXPECT_EQ(teaching_chance(pupil, DEMONIC_ART, 1, life),
            std::uint64_t{TEACHING} * ORIENTATION_EASE * POLARITY_EASE /
                PER_MILLE / PER_MILLE);
}

}  // namespace
}  // namespace murim
