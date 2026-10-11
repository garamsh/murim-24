#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "limits.hpp"
#include "map.hpp"
#include "names.hpp"
#include "skill.hpp"
#include "talent.hpp"

namespace murim {

/// The orientation of qi: orthodox, unorthodox or demonic.
enum class Orientation : std::uint8_t { ORTHODOX, UNORTHODOX, DEMONIC };

/// The polarity of qi: firm yang or soft yin.
enum class Polarity : std::uint8_t { YANG, YIN };

/// The nature of an art's qi, and of a person's, which is its heart
/// method's: the art it learned first.
struct Nature {
  Orientation orientation = Orientation::ORTHODOX;
  Polarity polarity = Polarity::YANG;
};

/// A skill of an art: its `name`, the `skill` itself, the realm `rank` a
/// person needs to learn it, and whether comprehension can unlock it.
struct ArtSkill {
  NameEntry name;
  Skill skill;
  std::uint32_t rank = 0;
  bool comprehensible = true;
};

/// A martial art: its `name`, the discipline its skills practise, its
/// `nature` and its `skills`, in the order they are learned.
struct Art {
  NameEntry name;
  std::size_t discipline = 0;
  Nature nature;
  std::vector<ArtSkill> skills;
};

/// The secret manuals the world's sects start with and where a fallen sect's
/// go, as data: `held` gives, by sect, the art of each manual it holds. When
/// a sect falls, each manual it holds scatters, with `scatter_chance` per
/// mille, to a passable cell drawn by the seed within `scatter_reach` cells
/// of its seat's centre along each axis, and is otherwise lost.
struct Manuals {
  std::vector<std::vector<std::size_t>> held;
  std::uint32_t scatter_chance = 0;
  std::int32_t scatter_reach = 0;
};

/// A realm of cultivation: its `name`, the qi capacity that reaches it, and
/// the `might` per mille its people strike with.
struct Realm {
  NameEntry name;
  std::uint32_t qi = 0;
  std::uint32_t might = PER_MILLE;
};

/// How people cultivate qi, as data. Each birth draws a growth a day and a
/// limit; once a day a person's capacity grows by its growth, up to its
/// limit, and every `recovery_ticks` ticks its qi recovers `recovery` per
/// mille of its capacity, rounded up, up to its capacity.
struct Cultivation {
  Spread growth;
  Spread limit;
  std::uint32_t recovery = 0;
  std::uint64_t recovery_ticks = 1;
};

/// How people learn arts, as data. On each realm gained, a person rolls, for
/// each art it knows, `comprehension` per mille scaled by its talent limit in
/// the art's discipline over that discipline's mean limit, to learn the art's
/// first comprehensible skill it lacks and has the rank for. A person within
/// `reading_reach` sub-cell units of a manual lying on a cell, holding
/// `reading_cost` qi, spends it to learn every skill of the manual's art it
/// has the rank for. A member of a standing sect whose seat is held learns,
/// with `teaching` per mille a year for each rank of its highest-ranked
/// elder, the first skill it lacks of an art its sect holds a manual of,
/// within both their ranks. An art whose nature differs from a person's in
/// orientation, or in polarity, multiplies those chances by that axis's
/// `ease` per mille, divides the reading cost by it, and multiplies the
/// art's body damage by that axis's `potency` per mille.
struct Learning {
  std::uint32_t comprehension = 0;
  std::uint32_t teaching = 0;
  std::uint32_t reading_cost = 0;
  std::int32_t reading_reach = 0;
  std::uint32_t orientation_ease = PER_MILLE;
  std::uint32_t polarity_ease = PER_MILLE;
  std::uint32_t orientation_potency = PER_MILLE;
  std::uint32_t polarity_potency = PER_MILLE;
};

/// A manual held by a sect, by index.
struct HeldBySect {
  std::size_t sect = 0;
};

/// A manual no one will find again.
struct Lost {};

/// A secret manual: the art it teaches, and where it is now.
struct Manual {
  std::size_t art = 0;
  std::variant<HeldBySect, Cell, Lost> place;
};

}  // namespace murim
