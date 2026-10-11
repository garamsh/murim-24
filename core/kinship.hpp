#pragma once

#include <cstdint>

#include "talent.hpp"

namespace murim {

/// How people marry and have children, as data, checked once a day. A
/// living person of `marrying_age`, in days, with a home and no living
/// spouse marries with a chance of `marriage_chance` per mille a year: a
/// partner of the other sex and another family, as free to marry, whose
/// home is the same village or one whose centre lies within `neighbourhood`
/// cells of its own, drawn by the seed. A wife of `bearing_age` whose husband
/// lives bears a child with a chance of `birth_chance` per mille a year, no
/// sooner than `birth_spacing` days after her last, while the world holds
/// fewer than MAX_PEOPLE living. The child is born by the one birth rule
/// into its father's family, or its mother's unless `fathers_family`, and
/// at that parent's side, home and army. With `balanced_sexes` it takes the
/// sex fewer among the living unmarried whose home is its own and who are not
/// past marrying age, children included; otherwise, or when they are as many,
/// its sex is drawn at even odds.
struct Kinship {
  Range marrying_age;
  std::int32_t neighbourhood = 0;
  std::uint32_t marriage_chance = 0;
  Range bearing_age;
  std::uint32_t birth_chance = 0;
  std::uint32_t birth_spacing = 0;
  bool fathers_family = true;
  bool balanced_sexes = false;
};

}  // namespace murim
