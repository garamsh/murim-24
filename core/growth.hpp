#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "limits.hpp"
#include "map.hpp"
#include "village.hpp"

namespace murim {

/// What a village pressing against its cap may do: raise a structure, found
/// a village on free land, or raid a hostile neighbour.
enum class Expansion : std::uint8_t { BUILD, FOUND, RAID };

/// A structure growth may lay: the `kind` of building it is, which names its
/// art, its footprint, whether it blocks movement, what it holds and its role.
struct Plan {
  std::string kind;
  Footprint footprint{1, 1};
  bool blocks = true;
  Holding holding;
  Role role = Role::HOME;
};

/// A size of village (ADR 0076): reached once `residents` people make it home,
/// it keeps a `square` clear round its centre and builds within `vicinity`
/// cells of it.
struct Tier {
  std::uint32_t residents = 0;
  Footprint square{1, 1};
  std::int32_t vicinity = 0;
};

/// How villages grow, as data, checked every `interval` days. A village with
/// living residents presses against its cap once the people whose home it is
/// reach `pressure` per mille of it; it then takes the first expansion of its
/// owner's stance, `stances` by faction, that it can:
/// - build: while every structure of its own is intact, it lays the plan of
///   `builds` that most raises its scarcest holding, in people, as rubble,
///   placed by its role within its tier's vicinity, for its residents to raise
///   by the repair rule
/// - found: with more residents than `settlers` and none of its people still
///   settling, it lays the plans of `core` as rubble round a new centre drawn
///   by the seed among the open sites within `reach` cells of its own: on
///   free buildable land room for the first tier's square, `spacing` cells
///   clear of any structure or village's clearing, held by no other faction
///   and no nearer than `apart` cells to any village's centre; `settlers` of
///   its residents, a founder's household first, march there as the new
///   village's residents, and the founder heads a new family; it makes a
///   village of `settlement_kind`, which names its art
/// - raid: with more residents than `raiders` and no party out, it sends
///   `raiders` of them against the nearest village of a faction it is hostile
///   to, with living residents and its centre within `reach` cells
/// A village takes the last of `tiers` its residents have reached and keeps
/// it. An edge plan may lie on rough cells as a terraced field, which repair
/// raises at `terrace` per mille of the labour of one on buildable cells.
/// A village arms and tends `self_reliance` people with no workshop, beside
/// what its standing workshops arm and tend; building counts both among its
/// holdings, after housing, food and storage. By default it arms and tends
/// any number, so it never builds for them.
/// A cell lies in the territory of the owner of the nearest village with
/// living residents whose centre lies within `claim` cells of it.
struct Growth {
  std::uint32_t interval = 1;
  std::uint32_t pressure = PER_MILLE;
  std::int32_t reach = 0;
  std::int32_t spacing = 0;
  std::int32_t claim = 0;
  std::int32_t apart = 0;
  std::uint32_t terrace = PER_MILLE;
  std::vector<Tier> tiers{Tier{}};
  std::vector<Plan> plans;
  std::vector<std::size_t> builds;
  std::string settlement_kind;
  std::vector<std::size_t> core;
  std::uint32_t settlers = 0;
  std::uint32_t raiders = 0;
  std::vector<std::vector<Expansion>> stances;
  std::uint32_t self_reliance = std::numeric_limits<std::uint32_t>::max();
};

/// Throws std::invalid_argument when `growth` checks at no interval, presses
/// above PER_MILLE, reaches outside [0, MAX_REACH], raises terraces at no
/// labour, has no tiers, a first tier past no one, tiers out of order or a
/// square of no cell, or names a plan it does not have.
void check_growth(const Growth& growth);

/// Returns the index of the last of `growth`'s tiers that `residents` reach.
[[nodiscard]] std::size_t tier_for(const Growth& growth,
                                   std::uint64_t residents);

}  // namespace murim
