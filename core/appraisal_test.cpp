#include "appraisal.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

#include "limits.hpp"
#include "person.hpp"

namespace murim {
namespace {

// A newborn's health in the region, and the peak realm's might and weight
// in the base pack.
constexpr std::uint64_t HEALTH = 120;
constexpr std::uint32_t PEAK_MIGHT = 1'400;
constexpr std::uint32_t PEAK_WEIGHT = 16;

// ADR 0071's shares and thresholds.
constexpr Temperaments DEFAULT_TEMPERAMENTS{
    {{200, {2'500, 5'000}}, {600, {1'500, 3'000}}, {200, {1'000, 1'800}}}};

TEST(Appraisal, EqualSidesReadAThousandAndThreeAgainstOneThreeThousand) {
  const std::uint64_t one = strength(HEALTH, PER_MILLE, 1);
  EXPECT_EQ(ratio(one, one), PER_MILLE);
  EXPECT_EQ(ratio(3 * one, one), 3 * PER_MILLE);
}

TEST(Appraisal, APeakMasterWeighsThreeNovicesByItsRealmsWeight) {
  // ADR 0071's example, which rounds to 134 and 2,143 where the ratio
  // truncates.
  const std::uint64_t novices = 3 * strength(HEALTH, PER_MILLE, 1);
  EXPECT_EQ(novices, 360U);
  EXPECT_EQ(strength(HEALTH, PEAK_MIGHT, PEAK_WEIGHT), 2'688U);
  EXPECT_EQ(ratio(novices, strength(HEALTH, PEAK_MIGHT, PEAK_WEIGHT)), 133U);
  // Control: with every weight 1, the master is outmatched.
  EXPECT_EQ(ratio(novices, strength(HEALTH, PEAK_MIGHT, 1)), 2'142U);
}

TEST(Appraisal, NoOwnStrengthReadsNothingWithoutAThreatAndTheCapWithOne) {
  EXPECT_EQ(ratio(0, 0), 0U);
  EXPECT_EQ(ratio(1, 0), RATIO_CAP);
  // Control: a threat against some strength reads its ratio.
  EXPECT_EQ(ratio(1, 1), PER_MILLE);
}

TEST(Appraisal, StrengthAndItsSumSaturateRatherThanWrap) {
  constexpr std::uint64_t MOST = std::numeric_limits<std::uint64_t>::max();
  EXPECT_EQ(strength(MAX_HEALTH, MAX_MIGHT, UINT32_MAX), MOST);
  EXPECT_EQ(add_strength(MOST, 1), MOST);
  EXPECT_EQ(ratio(MOST, MOST / 2), 2 * PER_MILLE);
  EXPECT_EQ(ratio(MOST, 1), RATIO_CAP);
  // Control: within range nothing saturates.
  EXPECT_EQ(add_strength(2, 3), 5U);
}

TEST(Appraisal, TheSharesPickEachTemperamentAtItsBounds) {
  EXPECT_EQ(temperament_of(0, DEFAULT_TEMPERAMENTS), Temperament::BOLD);
  EXPECT_EQ(temperament_of(199, DEFAULT_TEMPERAMENTS), Temperament::BOLD);
  EXPECT_EQ(temperament_of(200, DEFAULT_TEMPERAMENTS), Temperament::CAUTIOUS);
  EXPECT_EQ(temperament_of(799, DEFAULT_TEMPERAMENTS), Temperament::CAUTIOUS);
  EXPECT_EQ(temperament_of(800, DEFAULT_TEMPERAMENTS), Temperament::COWARDLY);
  EXPECT_EQ(temperament_of(999, DEFAULT_TEMPERAMENTS), Temperament::COWARDLY);
  // The draw is taken modulo a thousand.
  EXPECT_EQ(temperament_of(1'000, DEFAULT_TEMPERAMENTS), Temperament::BOLD);
}

TEST(Appraisal, SharesNotSummingToAThousandOrAFleeAtItsBackOffAreRefused) {
  EXPECT_NO_THROW(check_temperaments(DEFAULT_TEMPERAMENTS));
  Temperaments short_share = DEFAULT_TEMPERAMENTS;
  --short_share.at(0).share;
  EXPECT_THROW(check_temperaments(short_share), std::invalid_argument);
  Temperaments level = DEFAULT_TEMPERAMENTS;
  level.at(1).thresholds.flee = level.at(1).thresholds.back_off;
  EXPECT_THROW(check_temperaments(level), std::invalid_argument);
  // Control: a flee a point above its back-off is accepted.
  level.at(1).thresholds.flee = level.at(1).thresholds.back_off + 1;
  EXPECT_NO_THROW(check_temperaments(level));
}

}  // namespace
}  // namespace murim
