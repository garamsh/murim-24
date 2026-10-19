#include "gear.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "limits.hpp"
#include "skill.hpp"

namespace murim {
namespace {

// The kind worn in the striking slot, if any.
std::optional<std::size_t> weapon(const Gear& gear, const Worn& worn) {
  return gear.striking < worn.size() ? worn.at(gear.striking) : std::nullopt;
}

bool valid_strike(const Skill& skill) {
  return skill.body_damage <= MAX_BODY_DAMAGE && skill.cooldown <= MAX_COOLDOWN;
}

bool valid_kind(const Gear& gear, const ItemKind& kind) {
  const bool striking = kind.slot == gear.striking;
  return kind.slot < gear.slots.size() && striking == kind.family.has_value() &&
         (!kind.family || *kind.family < gear.families.size()) &&
         kind.damage.value_or(0) <= MAX_BODY_DAMAGE;
}

bool valid_loadout(const Gear& gear, const Loadout& loadout,
                   std::size_t factions, std::size_t sects) {
  const auto valid_slot = [&gear](const std::vector<Choice>& choices) {
    std::uint64_t weight = 0;
    for (const Choice& choice : choices) {
      if (choice.kind && *choice.kind >= gear.kinds.size()) {
        return false;
      }
      weight += choice.weight;
    }
    return choices.empty() || weight > 0;
  };
  bool fits =
      loadout.slots.size() <= gear.slots.size() &&
      (!loadout.faction || *loadout.faction < factions) &&
      (!loadout.sect || *loadout.sect < sects) &&
      std::all_of(loadout.slots.begin(), loadout.slots.end(), valid_slot);
  // Each choice is of a kind worn in the slot it is listed for.
  for (std::size_t slot = 0; fits && slot < loadout.slots.size(); ++slot) {
    for (const Choice& choice : loadout.slots.at(slot)) {
      fits = !choice.kind || gear.kinds.at(*choice.kind).slot == slot;
      if (!fits) {
        break;
      }
    }
  }
  return fits;
}

}  // namespace

std::size_t family_of(const Gear& gear, const Worn& worn) {
  const std::optional<std::size_t> held = weapon(gear, worn);
  return held ? gear.kinds.at(*held).family.value_or(gear.bare) : gear.bare;
}

Skill basic_strike(const Gear& gear, const Worn& worn) {
  Skill strike = gear.families.at(family_of(gear, worn)).strike;
  const std::optional<std::size_t> held = weapon(gear, worn);
  if (held) {
    strike.body_damage =
        gear.kinds.at(*held).damage.value_or(strike.body_damage);
  }
  return strike;
}

std::uint32_t edge_of(const Gear& gear, const Worn& worn) {
  const std::optional<std::size_t> held = weapon(gear, worn);
  return held ? gear.kinds.at(*held).edge : PER_MILLE;
}

std::uint32_t guard_of(const Gear& gear, const Worn& worn) {
  std::uint32_t guard = 0;
  for (const std::optional<std::size_t>& kind : worn) {
    guard += kind ? gear.kinds.at(*kind).guard : 0;
  }
  return std::min(guard, MAX_GUARD);
}

bool armed_for(const Gear& gear, const Worn& worn, std::size_t discipline) {
  if (discipline >= gear.needs.size() || gear.needs.at(discipline).empty()) {
    return true;
  }
  const std::vector<std::size_t>& needed = gear.needs.at(discipline);
  return std::find(needed.begin(), needed.end(), family_of(gear, worn)) !=
         needed.end();
}

const Loadout* loadout_for(const Gear& gear, std::size_t faction,
                           std::optional<std::size_t> sect, bool soldier) {
  const auto fits = [&](const Loadout& loadout) {
    return loadout.faction.value_or(faction) == faction &&
           (loadout.sect ? loadout.sect == sect : loadout.soldiers == soldier);
  };
  const auto found =
      std::find_if(gear.loadouts.begin(), gear.loadouts.end(), fits);
  return found == gear.loadouts.end() ? nullptr : &*found;
}

void check_gear(const Gear& gear, std::size_t disciplines, std::size_t factions,
                std::size_t sects) {
  const auto valid_family = [](const WeaponFamily& family) {
    return valid_strike(family.strike);
  };
  const auto within = [&gear](const std::vector<std::size_t>& needed) {
    return std::all_of(
        needed.begin(), needed.end(),
        [&gear](std::size_t family) { return family < gear.families.size(); });
  };
  const auto valid = [&gear](const ItemKind& kind) {
    return valid_kind(gear, kind);
  };
  const auto fitting = [&](const Loadout& loadout) {
    return valid_loadout(gear, loadout, factions, sects);
  };
  if (gear.striking >= gear.slots.size() || gear.bare >= gear.families.size() ||
      gear.needs.size() > disciplines ||
      !std::all_of(gear.families.begin(), gear.families.end(), valid_family) ||
      !std::all_of(gear.needs.begin(), gear.needs.end(), within) ||
      !std::all_of(gear.kinds.begin(), gear.kinds.end(), valid) ||
      !std::all_of(gear.loadouts.begin(), gear.loadouts.end(), fitting)) {
    throw std::invalid_argument(
        "gear must name only the slots, families, kinds, factions and sects "
        "it has, give each striking weapon a family and no other item one, "
        "weigh each listed slot's choices above 0, and keep its strikes "
        "within MAX_BODY_DAMAGE and MAX_COOLDOWN");
  }
}

}  // namespace murim
