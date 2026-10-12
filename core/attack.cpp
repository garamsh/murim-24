#include "attack.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace murim {

namespace {

std::uint16_t bit(std::uint8_t slot) {
  return static_cast<std::uint16_t>(1U << slot);
}

}  // namespace

bool valid(const Tokens& tokens) {
  const auto too_many = [](std::uint32_t granted) {
    return granted > MAX_TOKENS;
  };
  return !tokens.by_rank.empty() &&
         std::none_of(tokens.by_rank.begin(), tokens.by_rank.end(), too_many) &&
         tokens.circling <= MAX_HOLD_TICKS && tokens.lease <= MAX_HOLD_TICKS;
}

bool valid(const std::optional<Tokens>& tokens) {
  return !tokens || valid(*tokens);
}

Attacks::Attacks(std::size_t people) { grow_to(people); }

std::optional<Attacks> Attacks::of(std::size_t people,
                                   const std::vector<Engaged>& engaged) {
  Attacks attacks{people};
  for (const Engaged& each : engaged) {
    const Engagement& held = each.engagement;
    const bool out_of_range = each.person >= people || held.foe >= people ||
                              each.person == held.foe ||
                              held.slot >= MAX_SLOTS ||
                              (held.token && held.ring == Ring::WAITING);
    if (out_of_range || attacks.foe_.at(each.person) != NONE ||
        attacks.taken(held.foe, held.ring, held.slot)) {
      return std::nullopt;
    }
    attacks.engage(each.person, held);
  }
  return attacks;
}

void Attacks::grow_to(std::size_t people) {
  if (people <= foe_.size()) {
    return;
  }
  foe_.resize(people, NONE);
  slot_.resize(people, 0);
  flags_.resize(people, 0);
  waiting_since_.resize(people, 0);
  token_since_.resize(people, 0);
  near_taken_.resize(people, 0);
  long_taken_.resize(people, 0);
  waiting_taken_.resize(people, 0);
  tokens_held_.resize(people, 0);
}

void Attacks::engage(std::size_t person, const Engagement& engagement) {
  release(person);
  foe_.at(person) = static_cast<std::uint32_t>(engagement.foe);
  slot_.at(person) = engagement.slot;
  flags_.at(person) = static_cast<std::uint8_t>(
      (static_cast<unsigned>(engagement.ring) << RING_SHIFT) |
      (engagement.token ? TOKEN : 0U));
  waiting_since_.at(person) = engagement.waiting_since;
  token_since_.at(person) = engagement.token_since;
  taken_mask(engagement.foe, engagement.ring) |= bit(engagement.slot);
  if (engagement.token) {
    ++tokens_held_.at(engagement.foe);
  }
  ++engaged_;
}

void Attacks::release(std::size_t person) {
  const std::optional<Engagement> held = engagement(person);
  if (!held) {
    return;
  }
  taken_mask(held->foe, held->ring) &=
      static_cast<std::uint16_t>(~bit(held->slot));
  if (held->token) {
    --tokens_held_.at(held->foe);
  }
  foe_.at(person) = NONE;
  flags_.at(person) = 0;
  --engaged_;
}

std::optional<Engagement> Attacks::engagement(std::size_t person) const {
  if (person >= foe_.size() || foe_.at(person) == NONE) {
    return std::nullopt;
  }
  const std::uint8_t flags = flags_.at(person);
  return Engagement{
      foe_.at(person),           static_cast<Ring>(flags >> RING_SHIFT),
      slot_.at(person),          (flags & TOKEN) != 0,
      waiting_since_.at(person), token_since_.at(person)};
}

bool Attacks::taken(std::size_t foe, Ring ring, std::uint8_t slot) const {
  return (taken_mask(foe, ring) & bit(slot)) != 0;
}

std::uint32_t Attacks::tokens_held(std::size_t foe) const {
  return tokens_held_.at(foe);
}

bool Attacks::any() const { return engaged_ != 0; }

std::vector<Engaged> Attacks::engaged() const {
  std::vector<Engaged> all;
  for (std::size_t person = 0; person < foe_.size(); ++person) {
    if (const std::optional<Engagement> held = engagement(person)) {
      all.push_back(Engaged{person, *held});
    }
  }
  return all;
}

std::size_t Attacks::people() const { return foe_.size(); }

std::uint16_t& Attacks::taken_mask(std::size_t foe, Ring ring) {
  switch (ring) {
    case Ring::NEAR:
      return near_taken_.at(foe);
    case Ring::LONG:
      return long_taken_.at(foe);
    case Ring::WAITING:
      return waiting_taken_.at(foe);
  }
  return waiting_taken_.at(foe);
}

std::uint16_t Attacks::taken_mask(std::size_t foe, Ring ring) const {
  switch (ring) {
    case Ring::NEAR:
      return near_taken_.at(foe);
    case Ring::LONG:
      return long_taken_.at(foe);
    case Ring::WAITING:
      return waiting_taken_.at(foe);
  }
  return waiting_taken_.at(foe);
}

}  // namespace murim
