#include "life.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "arts.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "limits.hpp"
#include "skill.hpp"
#include "talent.hpp"
#include "trade.hpp"

namespace murim {
namespace {

// Throws std::invalid_argument when `life`'s cultivation, realms, arts or
// learning lie outside the limits the constructor documents.
void check_arts(const Life& life) {
  const auto excessive = [](Spread values) {
    return std::uint64_t{values.mean} + values.spread > MAX_TALENT;
  };
  const bool bad_cultivation =
      life.cultivation && (excessive(life.cultivation->growth) ||
                           excessive(life.cultivation->limit) ||
                           life.cultivation->recovery > PER_MILLE ||
                           life.cultivation->recovery_ticks == 0);
  bool unordered = false;
  for (std::size_t realm = 0; realm < life.realms.size(); ++realm) {
    unordered =
        unordered || life.realms.at(realm).might > MAX_MIGHT ||
        (realm > 0 && life.realms.at(realm).qi < life.realms.at(realm - 1).qi);
  }
  const auto stray_art = [&life](const Art& art) {
    return art.discipline >= life.disciplines.size() ||
           std::any_of(art.skills.begin(), art.skills.end(),
                       [&life](const ArtSkill& taught) {
                         return invalid_skill(taught.skill) ||
                                taught.rank > life.realms.size();
                       });
  };
  const Learning& learning = life.learning;
  const bool bad_learning = learning.comprehension > PER_MILLE ||
                            learning.teaching > PER_MILLE ||
                            learning.reading_cost > MAX_TALENT ||
                            !reachable(learning.reading_reach) ||
                            learning.orientation_ease > PER_MILLE ||
                            learning.polarity_ease > PER_MILLE ||
                            learning.orientation_potency > PER_MILLE ||
                            learning.polarity_potency > PER_MILLE;
  if (bad_cultivation || unordered || bad_learning ||
      std::any_of(life.arts.begin(), life.arts.end(), stray_art)) {
    throw std::invalid_argument(
        "cultivation must draw within MAX_TALENT and recover within PER_MILLE "
        "on a tick or more, realms ascend with a might within MAX_MIGHT, arts "
        "name known disciplines and hold valid skills of known ranks, and "
        "learning's chances, eases and potencies lie within PER_MILLE");
  }
}

}  // namespace

void check_life(const Life& life) {
  const auto too_talented = [&life](const DisciplineDraw& draw) {
    return std::uint64_t{draw.growth.mean} + draw.growth.spread > MAX_TALENT ||
           std::uint64_t{draw.limit.mean} + draw.limit.spread +
                   life.discipline_widening + life.upper_widening >
               MAX_TALENT;
  };
  const bool too_long = life.lifespan && std::uint64_t{life.lifespan->mean} +
                                                 life.lifespan->spread >
                                             MAX_LIFESPAN_DAYS;
  if (std::any_of(life.disciplines.begin(), life.disciplines.end(),
                  too_talented) ||
      too_long || life.newborn_health == 0 ||
      life.newborn_health > MAX_HEALTH) {
    throw std::invalid_argument(
        "life must draw talents and lifespans within their limits, and give "
        "newborns health");
  }
  const bool too_old =
      life.founding_age &&
      std::uint64_t{life.founding_age->mean} + life.founding_age->spread >
          MAX_LIFESPAN_DAYS;
  const auto unkind = [](const Kinship& kinship) {
    const auto inverted = [](Range range) { return range.low > range.high; };
    return inverted(kinship.marrying_age) || inverted(kinship.bearing_age) ||
           kinship.marriage_chance > PER_MILLE ||
           kinship.birth_chance > PER_MILLE || kinship.neighbourhood < 0 ||
           kinship.neighbourhood > MAX_REACH;
  };
  if (too_old || (life.kinship && unkind(*life.kinship))) {
    throw std::invalid_argument(
        "a founding age must lie within the longest lifespan, and kinship's "
        "ages run upward, its chances within a thousand per mille and its "
        "neighbourhood within reach");
  }
  if (life.growth) {
    check_growth(*life.growth);
  }
  check_arts(life);
  if (life.diplomacy && (life.diplomacy->war_chance > PER_MILLE ||
                         life.diplomacy->peace_chance > PER_MILLE)) {
    throw std::invalid_argument(
        "diplomacy's chances must lie within a thousand per mille");
  }
  if (life.trade) {
    check_trade(*life.trade, life.disciplines.size());
  }
}

Range limit_range(const Life& life, const Soul& soul, std::size_t discipline) {
  const Spread& limit = life.disciplines.at(discipline).limit;
  const std::uint32_t lives = std::max<std::uint32_t>(soul.lives, 1);
  const bool practised =
      discipline < soul.practised.size() && soul.practised.at(discipline);
  const std::uint32_t wider = practised ? life.discipline_widening / lives : 0;
  const std::uint32_t higher = soul.lives > 0 ? life.upper_widening / lives : 0;
  const std::uint32_t reach = limit.spread + wider;
  return Range{limit.mean - std::min(limit.mean, reach),
               limit.mean + reach + higher};
}

}  // namespace murim
