#include "leadership.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "limits.hpp"
#include "person.hpp"

namespace murim {
namespace {

constexpr std::size_t FACTIONS = 3;
constexpr std::size_t CULT = 0;
constexpr std::size_t ORTHODOX = 1;
constexpr std::size_t UNORTHODOX = 2;
constexpr std::size_t LEADER = 0;
constexpr std::size_t CHALLENGER = 1;
constexpr std::size_t BYSTANDER = 2;
constexpr std::uint64_t HEALTH = 100;
constexpr std::uint64_t YIELD = 10;
constexpr std::uint32_t TERM = 30;

std::vector<Person> people_of(std::size_t count) {
  std::vector<Person> people(count);
  for (Person& person : people) {
    person.health = HEALTH;
  }
  return people;
}

// The Cult passes its lead by duel, members only, to LEADER; the Orthodox
// elect every TERM days, led by no one; the Unorthodox have neither.
Leaders leaders(const std::vector<Person>& people) {
  Leadership cult;
  cult.leader = LEADER;
  cult.succession = Duel{true, YIELD, true};
  Leadership orthodox;
  orthodox.election = Election{TERM};
  return Leaders{{cult, orthodox}, FACTIONS, people};
}

TEST(Leaders, LeaveOutFactionsLedByNoOneAndRefuseStrangers) {
  const std::vector<Person> people = people_of(3);
  const Leaders made = leaders(people);
  ASSERT_EQ(made.factions().size(), FACTIONS);
  EXPECT_EQ(made.of(CULT).leader, std::optional<std::size_t>{LEADER});
  // A faction it leaves out has no leader.
  EXPECT_FALSE(made.of(UNORTHODOX).leader.has_value());
  EXPECT_TRUE(made.leads(LEADER));
  EXPECT_FALSE(made.leads(CHALLENGER));
  Leadership stranger;
  stranger.leader = people.size();
  EXPECT_THROW((Leaders{{stranger}, FACTIONS, people}), std::invalid_argument);
  EXPECT_THROW(
      (Leaders{std::vector<Leadership>(FACTIONS + 1), FACTIONS, people}),
      std::invalid_argument);
}

TEST(Leaders, RefuseAChallengeForTheFirstReasonInOrder) {
  const std::vector<Person> people = people_of(3);
  Leaders made = leaders(people);
  const Challenging member{CULT, CHALLENGER};
  // Control: a member of the Cult may challenge its leader.
  EXPECT_EQ(made.refusal(member, CULT), ChallengeRefusal::NONE);
  EXPECT_EQ(made.refusal(member, UNORTHODOX), ChallengeRefusal::NO_SUCCESSION);
  EXPECT_EQ(made.refusal(member, FACTIONS), ChallengeRefusal::NO_SUCCESSION);
  EXPECT_EQ(made.refusal(Challenging{CULT, std::nullopt}, CULT),
            ChallengeRefusal::NO_CHALLENGER);
  EXPECT_EQ(made.refusal(Challenging{ORTHODOX, CHALLENGER}, CULT),
            ChallengeRefusal::NOT_A_MEMBER);
  EXPECT_EQ(made.refusal(Challenging{CULT, LEADER}, CULT),
            ChallengeRefusal::IS_LEADER);
  // The first reason wins: an outsider with no lead has no challenger.
  EXPECT_EQ(made.refusal(Challenging{ORTHODOX, std::nullopt}, CULT),
            ChallengeRefusal::NO_CHALLENGER);
  made.challenge(member, CULT);
  EXPECT_EQ(made.refusal(Challenging{CULT, BYSTANDER}, CULT),
            ChallengeRefusal::DUEL_IN_PROGRESS);
  EXPECT_THROW(made.challenge(Challenging{CULT, BYSTANDER}, CULT),
               std::invalid_argument);
  ASSERT_EQ(made.duels().size(), 1U);

  Leadership open;
  open.leader = LEADER;
  open.succession = Duel{false, YIELD, false};
  const Leaders anyone{{open}, FACTIONS, people};
  // Control: a duel open to all takes an outsider.
  EXPECT_EQ(anyone.refusal(Challenging{ORTHODOX, CHALLENGER}, CULT),
            ChallengeRefusal::NONE);
  Leadership leaderless;
  leaderless.succession = Duel{};
  EXPECT_EQ((Leaders{{leaderless}, FACTIONS, people}).refusal(member, CULT),
            ChallengeRefusal::NO_LEADER);
}

TEST(Leaders, KnowWhoDuelsWhom) {
  const std::vector<Person> people = people_of(3);
  Leaders made = leaders(people);
  EXPECT_FALSE(made.duel(CULT).has_value());
  EXPECT_FALSE(made.in_duel(CHALLENGER));
  made.challenge(Challenging{CULT, CHALLENGER}, CULT);
  const std::optional<Duellists> fought = made.duel(CULT);
  ASSERT_TRUE(fought.has_value());
  EXPECT_EQ(fought.value_or(Duellists{0, 0}).challenger, CHALLENGER);
  EXPECT_EQ(fought.value_or(Duellists{0, 0}).leader, LEADER);
  EXPECT_TRUE(made.in_duel(CHALLENGER));
  EXPECT_TRUE(made.in_duel(LEADER));
  EXPECT_TRUE(made.duelling(CHALLENGER, LEADER));
  EXPECT_TRUE(made.duelling(LEADER, CHALLENGER));
  // Control: a bystander fights no one.
  EXPECT_FALSE(made.in_duel(BYSTANDER));
  EXPECT_FALSE(made.duelling(CHALLENGER, BYSTANDER));
  EXPECT_FALSE(made.duel(ORTHODOX).has_value());
}

TEST(Leaders, EndADuelWhenADuellistYieldsAndCrownOnlyAStandingChallenger) {
  std::vector<Person> people = people_of(3);
  Leaders made = leaders(people);
  made.challenge(Challenging{CULT, CHALLENGER}, CULT);
  // Control: while both stand above the yield, the duel goes on.
  people.at(LEADER).health = YIELD + 1;
  EXPECT_TRUE(made.settle(people).empty());
  ASSERT_EQ(made.duels().size(), 1U);
  // Both falling together leave the leader leading, and end the duel.
  people.at(LEADER).health = YIELD;
  people.at(CHALLENGER).health = YIELD;
  EXPECT_TRUE(made.settle(people).empty());
  EXPECT_TRUE(made.duels().empty());
  EXPECT_EQ(made.of(CULT).leader, std::optional<std::size_t>{LEADER});

  // The challenger standing over a fallen leader leads.
  people.at(CHALLENGER).health = HEALTH;
  made.challenge(Challenging{CULT, CHALLENGER}, CULT);
  const std::vector<Won> won = made.settle(people);
  ASSERT_EQ(won.size(), 1U);
  EXPECT_EQ(won.front().faction, CULT);
  EXPECT_EQ(won.front().challenger, CHALLENGER);
  EXPECT_TRUE(won.front().allies_turn_hostile);
  EXPECT_EQ(made.of(CULT).leader, std::optional<std::size_t>{CHALLENGER});
  EXPECT_TRUE(made.duels().empty());

  // A fallen challenger ends the duel and changes nothing.
  people.at(LEADER).health = HEALTH;
  made.challenge(Challenging{CULT, LEADER}, CULT);
  people.at(LEADER).health = 0;
  EXPECT_TRUE(made.settle(people).empty());
  EXPECT_TRUE(made.duels().empty());
  EXPECT_EQ(made.of(CULT).leader, std::optional<std::size_t>{CHALLENGER});
}

TEST(Leaders, ElectWhenTheLeaderIsGoneOrTheTermEnds) {
  std::vector<Person> people = people_of(3);
  Leaders made = leaders(people);
  // Led by no one, the Orthodox elect any day.
  EXPECT_TRUE(made.elects_on(ORTHODOX, people, 1));
  EXPECT_TRUE(made.lead(ORTHODOX, BYSTANDER));
  // Control: with a living leader mid-term, no election.
  EXPECT_FALSE(made.elects_on(ORTHODOX, people, 1));
  EXPECT_TRUE(made.elects_on(ORTHODOX, people, TERM));
  EXPECT_TRUE(made.elects_on(ORTHODOX, people, std::uint64_t{2} * TERM));
  people.at(BYSTANDER).health = 0;
  EXPECT_TRUE(made.elects_on(ORTHODOX, people, 1));
  // A faction with no election never elects, even led by no one.
  EXPECT_FALSE(made.elects_on(UNORTHODOX, people, TERM));
  // Making the leader lead again changes nothing.
  EXPECT_FALSE(made.lead(ORTHODOX, BYSTANDER));
  EXPECT_EQ(made.of(ORTHODOX).leader, std::optional<std::size_t>{BYSTANDER});
}

TEST(Leaders, RefuseSavedLeadershipNamingWhatDoesNotExist) {
  const std::vector<Person> people = people_of(3);
  Leaders made = leaders(people);
  made.challenge(Challenging{CULT, CHALLENGER}, CULT);
  // Control: leadership as the world makes it is kept.
  EXPECT_TRUE(made.kept(FACTIONS, people));
  // Control: the same through the saved form is kept.
  EXPECT_TRUE((Leaders{made.factions(), made.duels()}.kept(FACTIONS, people)));
  const auto with_fight = [&made, &people](Fight fight) {
    return Leaders{made.factions(), {fight}}.kept(FACTIONS, people);
  };
  EXPECT_FALSE(with_fight(Fight{FACTIONS, CHALLENGER, LEADER, Duel{}}));
  EXPECT_FALSE(with_fight(Fight{CULT, people.size(), LEADER, Duel{}}));
  EXPECT_FALSE(with_fight(Fight{CULT, CHALLENGER, people.size(), Duel{}}));
  const auto with_faction = [&made, &people](Leadership faction) {
    std::vector<Leadership> factions = made.factions();
    factions.at(UNORTHODOX) = faction;
    return Leaders{factions, made.duels()}.kept(FACTIONS, people);
  };
  Leadership stranger;
  stranger.leader = people.size();
  EXPECT_FALSE(with_faction(stranger));
  Leadership eager;
  eager.admission = Admission{};
  eager.admission->chance = PER_MILLE + 1;
  EXPECT_FALSE(with_faction(eager));
  Leadership far;
  far.admission = Admission{};
  far.admission->reach = MAX_REACH + 1;
  EXPECT_FALSE(with_faction(far));
  Leadership crowded;
  crowded.admission = Admission{};
  crowded.admission->elder_share = PER_MILLE + 1;
  EXPECT_FALSE(with_faction(crowded));
  // Not a leadership for each faction.
  EXPECT_FALSE((Leaders{std::vector<Leadership>(FACTIONS - 1), {}}.kept(
      FACTIONS, people)));
}

}  // namespace
}  // namespace murim
