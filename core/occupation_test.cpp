#include "occupation.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace murim {
namespace {

using Held = std::vector<std::optional<std::size_t>>;

constexpr std::size_t SMITH = 0;
constexpr std::size_t MERCHANT = 1;
constexpr std::size_t BEGGAR = 2;
constexpr std::size_t PER_SMITHY = 2;
constexpr std::uint32_t PER_MERCHANT = 30;

constexpr std::uint32_t WORKING_AGE = 16;
constexpr std::uint32_t WORK_FROM = 8;
constexpr std::uint32_t HOURS_PER_DAY = 24;

std::vector<Band> day() { return {{0, Where::HOME}, {WORK_FROM, Where::WORK}}; }

// A smith, two to each smithy; a merchant on the square, one for every 30
// people, rounded up; and the beggar, the fallback, which opens no place.
Occupations occupations() {
  return Occupations{
      {Occupation{"smith", AtWorkplace{"smithy", PER_SMITHY}, day(), {}},
       Occupation{"merchant", OnSquare{PER_MERCHANT}, day(), {0}},
       Occupation{"beggar", Nowhere{}, day(), {}}},
      WORKING_AGE,
      BEGGAR,
      day()};
}

Candidate adult(std::optional<std::size_t> held = std::nullopt) {
  return Candidate{true, held};
}

Candidate child() { return Candidate{false, std::nullopt}; }

TEST(Occupation, OpeningsFillInTheOccupationsOrderThenTheCandidates) {
  const Held held = assign(occupations(), {adult(), adult(), adult(), adult()},
                           {"smithy", "house"});
  // The smithy's two places go to the first two, the merchant's one to the
  // next, and the last takes the fallback.
  EXPECT_EQ(held, (Held{SMITH, SMITH, MERCHANT, BEGGAR}));
}

TEST(Occupation, ACandidateUnderTheWorkingAgeHoldsNone) {
  const Held held =
      assign(occupations(), {child(), adult(), child()}, {"smithy"});
  EXPECT_EQ(held, (Held{std::nullopt, SMITH, std::nullopt}));
  // Control: the same candidate of age takes a place.
  EXPECT_EQ(assign(occupations(), {adult(), adult(), child()}, {"smithy"}),
            (Held{SMITH, SMITH, std::nullopt}));
}

TEST(Occupation, AHeldOccupationIsKeptAheadOfAnEarlierCandidate) {
  // One smithy, two places: the third keeps the smithing it holds, so only
  // the first of the others takes the second place.
  const Held held =
      assign(occupations(), {adult(), adult(), adult(SMITH)}, {"smithy"});
  EXPECT_EQ(held, (Held{SMITH, MERCHANT, SMITH}));
  // Control: holding nothing, the third is filled last.
  EXPECT_EQ(assign(occupations(), {adult(), adult(), adult()}, {"smithy"}),
            (Held{SMITH, SMITH, MERCHANT}));
}

TEST(Occupation, AnOccupationIsLostWhenItsWorkplaceFalls) {
  const std::vector<Candidate> smiths{adult(SMITH), adult(SMITH)};
  // Control: while the smithy stands, both keep smithing.
  ASSERT_EQ(assign(occupations(), smiths, {"smithy"}), (Held{SMITH, SMITH}));
  // With no smithy standing, the first takes the merchant's one place and
  // the second the fallback.
  EXPECT_EQ(assign(occupations(), smiths, {"house"}), (Held{MERCHANT, BEGGAR}));
}

TEST(Occupation, AFallbackHolderTakesAPlaceThatOpens) {
  const Held held = assign(occupations(), {adult(BEGGAR)}, {"smithy"});
  EXPECT_EQ(held, (Held{SMITH}));
  // Control: with no place open beyond the merchant's, a second beggar stays
  // one.
  EXPECT_EQ(assign(occupations(), {adult(BEGGAR), adult(BEGGAR)}, {}),
            (Held{MERCHANT, BEGGAR}));
}

TEST(Occupation, SquarePlacesAreOneForEverySoManyPeopleRoundedUp) {
  const auto merchants = [](std::size_t people) {
    const Held held =
        assign(occupations(), std::vector<Candidate>(people, adult()), {});
    std::size_t count = 0;
    for (const std::optional<std::size_t>& each : held) {
      count += each == MERCHANT ? std::size_t{1} : std::size_t{0};
    }
    return count;
  };
  EXPECT_EQ(merchants(1), 1U);
  EXPECT_EQ(merchants(PER_MERCHANT), 1U);
  EXPECT_EQ(merchants(PER_MERCHANT + 1), 2U);
  // Control: a village with no one opens none.
  EXPECT_TRUE(assign(occupations(), {}, {}).empty());
}

TEST(Occupation, OccupationsOutsideTheirLimitsAreRefused) {
  // Control: the occupations above are accepted, with one ware.
  EXPECT_NO_THROW(check_occupations(occupations(), 1));
  EXPECT_THROW(check_occupations(occupations(), 0), std::invalid_argument);
  Occupations stray = occupations();
  stray.fallback = stray.list.size();
  EXPECT_THROW(check_occupations(stray, 1), std::invalid_argument);
  Occupations none = occupations();
  none.list.clear();
  none.fallback = 0;
  EXPECT_THROW(check_occupations(none, 1), std::invalid_argument);
  Occupations idle_smiths = occupations();
  idle_smiths.list.at(SMITH).openings = AtWorkplace{"smithy", 0};
  EXPECT_THROW(check_occupations(idle_smiths, 1), std::invalid_argument);
  Occupations unnamed = occupations();
  unnamed.list.at(SMITH).openings = AtWorkplace{"", PER_SMITHY};
  EXPECT_THROW(check_occupations(unnamed, 1), std::invalid_argument);
  Occupations no_square = occupations();
  no_square.list.at(MERCHANT).openings = OnSquare{0};
  EXPECT_THROW(check_occupations(no_square, 1), std::invalid_argument);
  Occupations late = occupations();
  late.idle = {{1, Where::HOME}};
  EXPECT_THROW(check_occupations(late, 1), std::invalid_argument);
  Occupations backwards = occupations();
  backwards.list.at(0).schedule = {
      {0, Where::HOME}, {WORK_FROM, Where::WORK}, {WORK_FROM, Where::SQUARE}};
  EXPECT_THROW(check_occupations(backwards, 1), std::invalid_argument);
  Occupations overnight = occupations();
  overnight.list.at(0).schedule = {{0, Where::HOME},
                                   {HOURS_PER_DAY, Where::WORK}};
  EXPECT_THROW(check_occupations(overnight, 1), std::invalid_argument);
}

}  // namespace
}  // namespace murim
