#include "appraisal.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

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

// ADR 0071's thresholds by temperament.
constexpr Thresholds BOLD_AT{2'500, 5'000};
constexpr Thresholds CAUTIOUS_AT{1'500, 3'000};
constexpr Thresholds COWARDLY_AT{1'000, 1'800};

// A reading of `ratio` with an ally in sight, off its home ground, with a
// refuge.
Reading among_allies(std::uint32_t ratio) {
  return Reading{ratio, false, false, true};
}

TEST(Appraisal, ACautiousPersonBacksOffAtTwoThousandAndClosesAtOne) {
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, among_allies(2'000), CAUTIOUS_AT),
            Stance::BACK_OFF);
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, among_allies(1'000), CAUTIOUS_AT),
            Stance::CLOSE);
  // Control: a bold one closes at two thousand.
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, among_allies(2'000), BOLD_AT),
            Stance::CLOSE);
}

TEST(Appraisal, FleeingNeedsARefugeAndHoldingHomeGround) {
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, among_allies(2'000), COWARDLY_AT),
            Stance::FLEE);
  Reading stranded = among_allies(2'000);
  stranded.refuge = false;
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, stranded, COWARDLY_AT),
            Stance::BACK_OFF);
  Reading home = among_allies(2'000);
  home.home_ground = true;
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, home, CAUTIOUS_AT), Stance::HOLD);
  // A holder off its home ground cannot hold, so it backs off at once.
  EXPECT_EQ(next_stance(Stance::HOLD, 0, among_allies(2'000), CAUTIOUS_AT),
            Stance::BACK_OFF);
}

TEST(Appraisal, OneAloneRegroupsAboveHalfItsBackOffRatio) {
  const Reading alone{1'000, true, false, true};
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, alone, CAUTIOUS_AT), Stance::REGROUP);
  // Control: with an ally in sight it closes.
  EXPECT_EQ(next_stance(Stance::CLOSE, 0, among_allies(1'000), CAUTIOUS_AT),
            Stance::CLOSE);
  // Regroup releases below three quarters of half the back-off ratio.
  const Reading easing{500, true, false, true};
  EXPECT_EQ(next_stance(Stance::REGROUP, STANCE_TICKS, easing, CAUTIOUS_AT),
            Stance::CLOSE);
  const Reading still{600, true, false, true};
  EXPECT_EQ(next_stance(Stance::REGROUP, STANCE_TICKS, still, CAUTIOUS_AT),
            Stance::REGROUP);
}

TEST(Appraisal, WorseningRiskFleesAtOnceFromAnyStance) {
  constexpr std::uint64_t ONE_PHASE = 15;
  for (const Stance held : {Stance::BACK_OFF, Stance::HOLD, Stance::REGROUP}) {
    Reading worse{4'000, held == Stance::REGROUP, held == Stance::HOLD, true};
    EXPECT_EQ(next_stance(held, ONE_PHASE, worse, CAUTIOUS_AT), Stance::FLEE)
        << static_cast<int>(held);
  }
  // Control: 2,000 with allies in sight keeps backing off.
  EXPECT_EQ(next_stance(Stance::BACK_OFF, ONE_PHASE, among_allies(2'000),
                        CAUTIOUS_AT),
            Stance::BACK_OFF);
}

TEST(Appraisal, DecliningOrOscillatingRiskReleasesOnlyAfterBothConditions) {
  constexpr std::uint64_t ONE_PHASE = 15;
  // 1,000 a phase in still backs off; at 30 ticks it closes.
  EXPECT_EQ(next_stance(Stance::BACK_OFF, ONE_PHASE, among_allies(1'000),
                        CAUTIOUS_AT),
            Stance::BACK_OFF);
  EXPECT_EQ(next_stance(Stance::BACK_OFF, STANCE_TICKS, among_allies(1'000),
                        CAUTIOUS_AT),
            Stance::CLOSE);
  // Control: 1,125, three quarters of 1,500 exactly, does not release.
  EXPECT_EQ(next_stance(Stance::BACK_OFF, STANCE_TICKS, among_allies(1'125),
                        CAUTIOUS_AT),
            Stance::BACK_OFF);
  // Oscillating at 1,400 and 1,600 holds past 30 ticks.
  for (const std::uint32_t ratio : {1'400U, 1'600U}) {
    EXPECT_EQ(next_stance(Stance::BACK_OFF, 2 * STANCE_TICKS,
                          among_allies(ratio), CAUTIOUS_AT),
              Stance::BACK_OFF);
  }
}

TEST(Appraisal, FlightAtTwoThousandHoldsThirtyTicksThenBacksOff) {
  constexpr std::uint64_t ONE_PHASE = 15;
  EXPECT_EQ(
      next_stance(Stance::FLEE, ONE_PHASE, among_allies(2'000), CAUTIOUS_AT),
      Stance::FLEE);
  EXPECT_EQ(
      next_stance(Stance::FLEE, STANCE_TICKS, among_allies(2'000), CAUTIOUS_AT),
      Stance::BACK_OFF);
  // Controls: 2,250 keeps fleeing, and 1,000 with allies closes.
  EXPECT_EQ(
      next_stance(Stance::FLEE, STANCE_TICKS, among_allies(2'250), CAUTIOUS_AT),
      Stance::FLEE);
  EXPECT_EQ(
      next_stance(Stance::FLEE, STANCE_TICKS, among_allies(1'000), CAUTIOUS_AT),
      Stance::CLOSE);
}

TEST(Appraisal, OnlyClosingAndHoldingEngage) {
  EXPECT_TRUE(engages(Stance::CLOSE));
  EXPECT_TRUE(engages(Stance::HOLD));
  EXPECT_FALSE(engages(Stance::BACK_OFF));
  EXPECT_FALSE(engages(Stance::FLEE));
  EXPECT_FALSE(engages(Stance::REGROUP));
}

TEST(Appraisal, ACentroidWeighsEachOffsetByItsStrength) {
  Centroid centroid;
  EXPECT_FALSE(centroid.mean().has_value());
  constexpr std::int64_t FAR = 400;
  centroid.add(FAR, 0, 3);
  centroid.add(0, FAR, 1);
  const auto mean =
      centroid.mean().value_or(std::pair<std::int64_t, std::int64_t>{});
  EXPECT_EQ(mean.first, 3 * FAR / 4);
  EXPECT_EQ(mean.second, FAR / 4);
  // Strengths too great to sum keep their shares.
  constexpr std::uint64_t MOST = std::numeric_limits<std::uint64_t>::max();
  constexpr std::int64_t EDGE = std::int64_t{1} << 32;
  Centroid huge;
  huge.add(EDGE, 0, MOST);
  huge.add(-EDGE, 0, MOST);
  huge.add(0, EDGE, MOST);
  const auto even =
      huge.mean().value_or(std::pair<std::int64_t, std::int64_t>{});
  EXPECT_LE(std::abs(even.first), 1);
  EXPECT_NEAR(static_cast<double>(even.second), static_cast<double>(EDGE / 3),
              static_cast<double>(EDGE) / 1'000);
}

TEST(Appraisal, AStepAlongALineTakesTheOpenStepNearestIt) {
  const auto all = [](Step) { return true; };
  constexpr std::int64_t FAR = 100;
  const Step east = step_along(FAR, 1, all).value_or(Step{0, 0});
  EXPECT_EQ(east.x, 1);
  EXPECT_EQ(east.y, 0);
  const Step diagonal = step_along(FAR, FAR, all).value_or(Step{0, 0});
  EXPECT_EQ(diagonal.x, 1);
  EXPECT_EQ(diagonal.y, 1);
  // East shut, it takes the nearest open one along the line.
  const auto no_east = [](Step step) { return step.x != 1 || step.y != 0; };
  const Step around = step_along(FAR, 1, no_east).value_or(Step{0, 0});
  EXPECT_EQ(around.x, 1);
  EXPECT_EQ(around.y, 1);
  // Nothing leading along the line is open: none.
  const auto only_back = [](Step step) { return step.x < 0; };
  EXPECT_FALSE(step_along(FAR, 0, only_back).has_value());
}

}  // namespace
}  // namespace murim
