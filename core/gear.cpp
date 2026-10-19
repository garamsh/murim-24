#include "gear.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "limits.hpp"
#include "skill.hpp"
#include "talent.hpp"

namespace murim {
namespace {

// The kind worn in the striking slot, if any.
std::optional<std::size_t> weapon(const Gear& gear, const Worn& worn) {
  return gear.striking < worn.size() ? worn.at(gear.striking) : std::nullopt;
}

bool valid_strike(const std::optional<Skill>& skill) {
  return !skill || (skill->body_damage <= MAX_BODY_DAMAGE &&
                    skill->cooldown <= MAX_COOLDOWN);
}

bool valid_kind(const Gear& gear, const ItemKind& kind) {
  return kind.slot < gear.slots.size() && kind.family < gear.families.size() &&
         (kind.slot != gear.striking ||
          gear.families.at(kind.family).strike.has_value()) &&
         kind.damage.value_or(0) <= MAX_BODY_DAMAGE;
}

// Whether `choice` may be listed for slot `slot`: the art's weapon only in
// the striking slot, and a family only with a kind worn in the slot.
bool valid_choice(const Gear& gear, const Choice& choice, std::size_t slot) {
  if (choice.art) {
    return !choice.family && slot == gear.striking;
  }
  if (!choice.family) {
    return true;
  }
  if (*choice.family >= gear.families.size()) {
    return false;
  }
  const std::vector<std::size_t> lowest = lowest_tier(gear, *choice.family);
  return !lowest.empty() &&
         std::all_of(lowest.begin(), lowest.end(), [&](std::size_t kind) {
           return gear.kinds.at(kind).slot == slot;
         });
}

bool valid_loadout(const Gear& gear, const Loadout& loadout,
                   std::size_t factions) {
  if (loadout.slots.size() > gear.slots.size() ||
      (loadout.faction && *loadout.faction >= factions)) {
    return false;
  }
  for (std::size_t slot = 0; slot < loadout.slots.size(); ++slot) {
    std::uint64_t weight = 0;
    for (const Choice& choice : loadout.slots.at(slot)) {
      if (!valid_choice(gear, choice, slot)) {
        return false;
      }
      weight += choice.weight;
    }
    if (!loadout.slots.at(slot).empty() && weight == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::size_t family_of(const Gear& gear, const Worn& worn) {
  const std::optional<std::size_t> held = weapon(gear, worn);
  return held ? gear.kinds.at(*held).family : gear.bare;
}

Skill basic_strike(const Gear& gear, const Worn& worn) {
  Skill strike =
      gear.families.at(family_of(gear, worn)).strike.value_or(Skill{});
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

std::vector<std::size_t> lowest_tier(const Gear& gear, std::size_t family) {
  std::vector<std::size_t> lowest;
  for (std::size_t kind = 0; kind < gear.kinds.size(); ++kind) {
    const ItemKind& each = gear.kinds.at(kind);
    if (each.family != family) {
      continue;
    }
    if (!lowest.empty() && each.tier < gear.kinds.at(lowest.front()).tier) {
      lowest.clear();
    }
    if (lowest.empty() || each.tier == gear.kinds.at(lowest.front()).tier) {
      lowest.push_back(kind);
    }
  }
  return lowest;
}

const Loadout* loadout_for(const Gear& gear, std::size_t faction, bool founder,
                           bool soldier) {
  const auto fits = [&](const Loadout& loadout) {
    return loadout.faction.value_or(faction) == faction &&
           (loadout.founders ? founder
                             : !founder && loadout.soldiers == soldier);
  };
  const auto found =
      std::find_if(gear.loadouts.begin(), gear.loadouts.end(), fits);
  return found == gear.loadouts.end() ? nullptr : &*found;
}

std::vector<std::optional<std::size_t>> art_families(
    const Gear& gear, std::size_t factions,
    const std::vector<std::size_t>& sects,
    const std::vector<std::vector<std::size_t>>& held,
    const std::vector<std::size_t>& disciplines) {
  std::vector<std::optional<std::size_t>> families(factions);
  std::vector<bool> settled(factions, false);
  for (std::size_t sect = 0; sect < sects.size() && sect < held.size();
       ++sect) {
    const std::size_t faction = sects.at(sect);
    if (faction >= factions || settled.at(faction) || held.at(sect).empty() ||
        held.at(sect).front() >= disciplines.size()) {
      continue;
    }
    settled.at(faction) = true;
    const std::size_t discipline = disciplines.at(held.at(sect).front());
    if (discipline < gear.needs.size() && !gear.needs.at(discipline).empty()) {
      families.at(faction) = gear.needs.at(discipline).front();
    }
  }
  return families;
}

void check_gear(const Gear& gear,
                const std::vector<DisciplineDraw>& disciplines,
                std::size_t factions) {
  const auto valid_family = [](const Family& family) {
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
    return valid_loadout(gear, loadout, factions);
  };
  if (gear.striking >= gear.slots.size() || gear.bare >= gear.families.size() ||
      !gear.families.at(gear.bare).strike ||
      gear.needs.size() > disciplines.size() ||
      !std::all_of(gear.families.begin(), gear.families.end(), valid_family) ||
      !std::all_of(gear.needs.begin(), gear.needs.end(), within) ||
      !std::all_of(gear.kinds.begin(), gear.kinds.end(), valid) ||
      !std::all_of(gear.loadouts.begin(), gear.loadouts.end(), fitting)) {
    throw std::invalid_argument(
        "gear must name only the slots, families, kinds and factions it has, "
        "strike from every family worn in the striking slot and bare hands, "
        "list each family only for the slot its lowest tier is worn in, weigh "
        "each listed slot's choices above 0, and keep its strikes within "
        "MAX_BODY_DAMAGE and MAX_COOLDOWN");
  }
}

}  // namespace murim
