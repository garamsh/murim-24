#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "map.hpp"

namespace murim {

/// How a village feeds its people, as data, checked once a day. Each person
/// whose home it is eats `consumption` food a day from its stock; while the
/// stock cannot feed them all, none eats, and a person unfed for
/// `hunger_days` days moves, by the seed, to a village within
/// `migration_reach` cells with room and food, or dies. On the first day of
/// winter its intact fields add their harvest to its stock, which never
/// holds more than its storage.
struct Husbandry {
  std::uint32_t consumption = 0;
  std::uint32_t hunger_days = 0;
  std::int32_t migration_reach = 0;
};

/// What a field shows: sown in spring, growing in summer, ripe in autumn,
/// fallow in winter, and burnt whenever it is below its intact stage.
enum class FieldState : std::uint8_t { SOWN, GROWING, RIPE, FALLOW, BURNT };

/// What a structure gives its village: the people it houses, the food it
/// stores, the food it yields each harvest, which makes it a field, and the
/// pupils it teaches a sect seated in the village. A workshop arms `arms` and
/// tends `tends` of the people whose home the village is; while it stands
/// and its village's arms cover them, they strike `edge` per mille harder,
/// and while its tends cover them, they recover `healing` health a day. A
/// village's capacities add up over its standing structures; its edge and
/// healing are the greatest among them.
struct Holding {
  std::uint32_t housing = 0;
  std::uint32_t storage = 0;
  std::uint32_t harvest = 0;
  std::uint32_t pupils = 0;
  std::uint32_t arms = 0;
  std::uint32_t tends = 0;
  std::uint32_t edge = 0;
  std::uint32_t healing = 0;
};

/// A breakable object covering `footprint` from `cell`, its top-left cell.
/// Its cells block movement for as long as the world runs, whatever its
/// stage.
struct Structure {
  Cell cell{};
  std::uint32_t durability = 0;
  Footprint footprint{1, 1};
  /// Whether its footprint blocks movement; a field does not.
  bool blocks = true;
};

/// Land growth keeps clear as though a structure covered it: `area` from
/// `corner`, its top-left cell.
struct Clearing {
  Cell corner{};
  Footprint area{1, 1};
};

/// A village as a world is created with: its structures by index, and the
/// army living in it, whose members make it their home. The first two
/// members of each of its `households` start as one: a man and a woman,
/// married to each other, each heading a family of their own.
struct VillageSpec {
  std::vector<std::size_t> structures;
  std::size_t residents;
  /// What each of `structures` holds, in their order; none holds nothing.
  std::vector<Holding> holdings;
  std::uint32_t households = 0;
  /// The land growth keeps clear in it, such as a seat's court; cells off the
  /// map are ignored.
  std::vector<Clearing> clearings;
};

/// Where a village stands in its life cycle, derived from its structures,
/// its residents and the bodies standing in it.
enum class VillageState : std::uint8_t {
  PROSPEROUS,
  RAIDED,
  RUINS,
  REBUILDING
};

/// The land a founded village was laid out on: the growth's settlement `area`
/// from `corner`, making a village of `kind`.
struct Settlement {
  std::string kind;
  Cell corner;
  Footprint area;
};

/// The army whose strike brought a village's last standing structure to
/// rubble, and its faction.
struct Destroyer {
  std::size_t army;
  std::size_t faction;
};

/// How broken a structure is, from durability by the floors in Rules.
enum class Stage : std::uint8_t { INTACT, DAMAGED, DESTROYED, RUBBLE };

}  // namespace murim
