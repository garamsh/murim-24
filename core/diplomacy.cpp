#include "diplomacy.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace murim {

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
