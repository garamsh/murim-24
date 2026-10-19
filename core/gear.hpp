#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "limits.hpp"
#include "skill.hpp"

namespace murim {

/// The most of the body damage a person takes that everything it wears
/// guards against, per mille, so a strike always lands (ADR 0050).
constexpr std::uint32_t MAX_GUARD = 600;

/// A weapon family (ADR 0050): its `name`, and the `strike`, a person's
/// basic strike while its striking slot holds a weapon of the family.
struct WeaponFamily {
  std::string name;
  Skill strike;
};

/// An item kind, as data: its `name` and the `slot` it is worn in. Worn, a
/// weapon of `family` strikes with its family's strike, at `damage` in
/// place of the strike's own when given, and multiplies the body damage of
/// the art skills used with it by `edge` per mille; armour and headgear
/// guard against `guard` per mille of the body damage their wearer takes.
struct ItemKind {
  std::string name;
  std::size_t slot = 0;
  std::optional<std::size_t> family;
  std::optional<std::uint64_t> damage;
  std::uint32_t edge = PER_MILLE;
  std::uint32_t guard = 0;
};

/// One choice for a slot: item kind `kind`, the `art`'s weapon of the
/// wearer's faction (art_weapons()), or else the slot left empty, drawn
/// with `weight` against the slot's other choices.
struct Choice {
  std::optional<std::size_t> kind;
  std::uint32_t weight = 1;
  bool art = false;
};

/// What the people a loadout fits wear as they are born: in each slot, by
/// index, one of its choices, drawn by the seed, and nothing in a slot with
/// none. It fits people of `faction`, if given, and of them the founders of
/// `sect`, if given, whatever they serve as; otherwise `soldiers`, serving in
/// an army other than their home village's residents, or else the rest.
struct Loadout {
  std::optional<std::size_t> faction;
  std::optional<std::size_t> sect;
  bool soldiers = false;
  std::vector<std::vector<Choice>> slots;
};

/// The slots people wear items in, and the items, as data (ADR 0050): the
/// `slots` by name; the `striking` slot, whose weapon gives a person's basic
/// strike, and the `bare` family, whose strike it strikes with while that
/// slot is empty; the weapon `families`; the families each discipline's arts
/// need, by discipline, any at all for a discipline that names none; the
/// item `kinds`; and the `loadouts`, of which a person born wears the first
/// that fits it.
struct Gear {
  std::vector<std::string> slots;
  std::size_t striking = 0;
  std::vector<WeaponFamily> families;
  std::size_t bare = 0;
  std::vector<std::vector<std::size_t>> needs;
  std::vector<ItemKind> kinds;
  std::vector<Loadout> loadouts;
};

/// What a person wears: by slot, an item kind, or nothing.
using Worn = std::vector<std::optional<std::size_t>>;

/// Returns the family whose strike a person wearing `worn` strikes with:
/// its striking slot's weapon's, or the bare family's.
[[nodiscard]] std::size_t family_of(const Gear& gear, const Worn& worn);

/// Returns the basic strike of a person wearing `worn`.
[[nodiscard]] Skill basic_strike(const Gear& gear, const Worn& worn);

/// Returns the edge per mille on art skills of a person wearing `worn`: its
/// striking slot's weapon's, or PER_MILLE with that slot empty.
[[nodiscard]] std::uint32_t edge_of(const Gear& gear, const Worn& worn);

/// Returns the share per mille of body damage taken that a person wearing
/// `worn` guards against: the sum of its items' guards, up to MAX_GUARD.
[[nodiscard]] std::uint32_t guard_of(const Gear& gear, const Worn& worn);

/// Returns whether a person wearing `worn` may use the art skills of
/// discipline `discipline`.
[[nodiscard]] bool armed_for(const Gear& gear, const Worn& worn,
                             std::size_t discipline);

/// Returns the first loadout that fits a person born into `faction`, a
/// founder of `sect` if given, serving as a soldier or not; none when none
/// fits.
[[nodiscard]] const Loadout* loadout_for(const Gear& gear, std::size_t faction,
                                         std::optional<std::size_t> sect,
                                         bool soldier);

/// Returns, by faction, the weapon an art's-weapon choice gives its people:
/// the first kind of the first family needed by the discipline of the first
/// art its first sect holding a manual holds, of `held` by sect and of
/// `disciplines` by art; nothing where its art needs no family or none of
/// `gear`'s kinds is of it. `sects` gives each sect's faction.
[[nodiscard]] std::vector<std::optional<std::size_t>> art_weapons(
    const Gear& gear, std::size_t factions,
    const std::vector<std::size_t>& sects,
    const std::vector<std::vector<std::size_t>>& held,
    const std::vector<std::size_t>& disciplines);

/// Throws std::invalid_argument when `gear` names a slot, family or kind it
/// lacks, a faction or sect past `factions` or `sects`, needs for more than
/// `disciplines` disciplines, a striking weapon without a family or another
/// slot's item with one, an art's-weapon choice naming a kind or outside the
/// striking slot, choices for a slot whose weights sum to 0, or a
/// strike or damage outside MAX_BODY_DAMAGE or MAX_COOLDOWN.
void check_gear(const Gear& gear, std::size_t disciplines, std::size_t factions,
                std::size_t sects);

}  // namespace murim
