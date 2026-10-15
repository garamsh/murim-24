#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "arts.hpp"
#include "diplomacy.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "names.hpp"
#include "occupation.hpp"
#include "talent.hpp"
#include "trade.hpp"
#include "village.hpp"

namespace murim {

/// How people are born, age and are born again, as data. Each birth draws a
/// talent per discipline and a lifespan in days. A person ages out when its
/// age passes its lifespan plus `days_per_attainment` days for every point of
/// attainment it holds. A soul's past lives widen its next talent draw: a
/// discipline it practised draws its limit `discipline_widening` further
/// down and up, and every discipline `upper_widening` further up, each
/// divided by the lives it has lived, so the widening shrinks as lives
/// repeat. Without a lifespan no one ages. A soul is born again, and a child
/// born, with `newborn_health`, which is also as far as a village's healing
/// restores anyone. The people a world starts with are
/// `founding_age` days old, or newborn without one. People marry and have
/// children by `kinship`, if given, and bear `names`. Villages feed their
/// people by `husbandry`, if given, which caps how many a village holds, and
/// grow by `growth`, if given, as they press against their caps. Sects hold
/// `manuals`, if given. People cultivate qi by `cultivation`, if given, into
/// `realms`, ascending by qi, and learn `arts` by `learning`. Silver and
/// wares go round by `trade`, if given, and people take `occupations`, if
/// given.
struct Life {
  std::vector<DisciplineDraw> disciplines;
  std::uint64_t newborn_health = 1;
  std::optional<Spread> lifespan;
  std::uint32_t days_per_attainment = 0;
  std::uint32_t discipline_widening = 0;
  std::uint32_t upper_widening = 0;
  std::optional<Spread> founding_age;
  std::optional<Kinship> kinship;
  Names names;
  std::optional<Husbandry> husbandry;
  std::optional<Growth> growth;
  /// The secret manuals sects start with, and where a fallen sect's go.
  std::optional<Manuals> manuals;
  std::optional<Cultivation> cultivation;
  std::vector<Realm> realms;
  std::vector<Art> arts;
  Learning learning;
  std::optional<Diplomacy> diplomacy;
  std::optional<Trade> trade;
  std::optional<Occupations> occupations;
};

/// Returns the range a soul with `soul` history draws discipline
/// `discipline`'s talent limit from under `life`; an empty history draws
/// from the discipline's own spread.
[[nodiscard]] Range limit_range(const Life& life, const Soul& soul,
                                std::size_t discipline);

/// Throws std::invalid_argument when `life` could draw a talent above
/// MAX_TALENT or a lifespan above MAX_LIFESPAN_DAYS, or gives a newborn no
/// health or more than MAX_HEALTH.
void check_life(const Life& life);

}  // namespace murim
