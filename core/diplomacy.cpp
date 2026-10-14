#include "diplomacy.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "limits.hpp"

namespace murim {

void turn_allies_hostile(Stances& stances, std::size_t faction) {
  for (std::size_t other = 0; other < stances.size(); ++other) {
    const bool ally = other != faction && !stances.at(other).at(faction) &&
                      !stances.at(faction).at(other);
    stances.at(other).at(faction) = stances.at(other).at(faction) || ally;
  }
}

Declarations::Declarations(std::size_t factions)
    : factions_(factions), slots_(factions * factions) {}

Declarations::Declarations(std::size_t factions,
                           std::vector<std::optional<Declared>> slots)
    : factions_(factions), slots_(std::move(slots)) {}

const std::optional<Declared>& Declarations::between(std::size_t faction,
                                                     std::size_t other) const {
  return slots_.at(pair_of(faction, other));
}

void Declarations::declare(std::size_t faction, std::size_t other,
                           Declared declared) {
  slots_.at(pair_of(faction, other)) = declared;
}

bool Declarations::hostile(std::size_t from, std::size_t target,
                           const Stances& stances) const {
  const std::optional<Declared>& standing = between(from, target);
  if (standing) {
    return standing->what == Declaration::WAR;
  }
  return stances.at(from).at(target);
}

Relation Declarations::relation(std::size_t faction, std::size_t other,
                                const Stances& stances) const {
  const std::optional<Declared>& standing = between(faction, other);
  if (faction == other) {
    return Relation::OWN;
  }
  if (standing) {
    return standing->what == Declaration::WAR ? Relation::AT_WAR
                                              : Relation::NEUTRAL;
  }
  return stances.at(faction).at(other) || stances.at(other).at(faction)
             ? Relation::HOSTILE
             : Relation::ALLY;
}

DeclarationRefusal Declarations::refusal(std::size_t faction, std::size_t other,
                                         Declaration what,
                                         const Stances& stances,
                                         std::uint64_t now,
                                         std::uint64_t hold) const {
  if (faction == other) {
    return DeclarationRefusal::SAME_FACTION;
  }
  const Relation standing = relation(faction, other, stances);
  if (what == Declaration::WAR && standing == Relation::AT_WAR) {
    return DeclarationRefusal::ALREADY_AT_WAR;
  }
  if (what == Declaration::PEACE &&
      (standing == Relation::ALLY || standing == Relation::NEUTRAL)) {
    return DeclarationRefusal::ALREADY_AT_PEACE;
  }
  const std::optional<Declared>& last = between(faction, other);
  if (last && now - last->tick < hold) {
    return DeclarationRefusal::TOO_SOON;
  }
  return DeclarationRefusal::NONE;
}

std::optional<Sought> Declarations::sought(
    const Diplomacy& diplomacy, std::size_t faction, std::size_t other,
    const std::vector<std::uint64_t>& numbers, const Stances& stances) const {
  if (other == faction || numbers.at(other) == 0) {
    return std::nullopt;
  }
  const Relation standing = relation(faction, other, stances);
  if (standing == Relation::ALLY || standing == Relation::NEUTRAL) {
    if (numbers.at(faction) * PER_MILLE <
        numbers.at(other) * diplomacy.war_margin) {
      return std::nullopt;
    }
    return Sought{Declaration::WAR, diplomacy.war_chance};
  }
  if (standing == Relation::AT_WAR && numbers.at(faction) < numbers.at(other)) {
    return Sought{Declaration::PEACE, diplomacy.peace_chance};
  }
  return std::nullopt;
}

bool Declarations::kept(std::uint64_t now) const {
  if (slots_.size() != factions_ * factions_) {
    return false;
  }
  for (std::size_t index = 0; index < slots_.size(); ++index) {
    const std::optional<Declared>& standing = slots_.at(index);
    const std::size_t one = index / factions_;
    const std::size_t other = index % factions_;
    if (standing &&
        (one >= other || (standing->by != one && standing->by != other) ||
         standing->tick > now)) {
      return false;
    }
  }
  return true;
}

const std::vector<std::optional<Declared>>& Declarations::slots() const {
  return slots_;
}

std::size_t Declarations::pair_of(std::size_t faction,
                                  std::size_t other) const {
  if (faction >= factions_ || other >= factions_) {
    throw std::out_of_range("no such faction");
  }
  return (std::min(faction, other) * factions_) + std::max(faction, other);
}

}  // namespace murim
