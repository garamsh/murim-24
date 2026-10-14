#include "attack.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

#include "map.hpp"

namespace murim {

namespace {

std::uint16_t bit(std::uint8_t slot) {
  return static_cast<std::uint16_t>(1U << slot);
}

// The scale of UNIT_CIRCLE's entries.
constexpr std::int64_t UNIT = 4096;

// Each ring of 1 to MAX_SLOTS slots in turn, slot k of n at the cosine and
// sine of k / n of a turn times UNIT, rounded; ring n starts at entry
// n (n - 1) / 2.
constexpr std::array<std::array<std::int16_t, 2>, MAX_SLOTS*(MAX_SLOTS + 1) / 2>
    UNIT_CIRCLE{
        {{4096, 0},      {4096, 0},      {-4096, 0},     {4096, 0},
         {-2048, 3547},  {-2048, -3547}, {4096, 0},      {0, 4096},
         {-4096, 0},     {0, -4096},     {4096, 0},      {1266, 3896},
         {-3314, 2408},  {-3314, -2408}, {1266, -3896},  {4096, 0},
         {2048, 3547},   {-2048, 3547},  {-4096, 0},     {-2048, -3547},
         {2048, -3547},  {4096, 0},      {2554, 3202},   {-911, 3993},
         {-3690, 1777},  {-3690, -1777}, {-911, -3993},  {2554, -3202},
         {4096, 0},      {2896, 2896},   {0, 4096},      {-2896, 2896},
         {-4096, 0},     {-2896, -2896}, {0, -4096},     {2896, -2896},
         {4096, 0},      {3138, 2633},   {711, 4034},    {-2048, 3547},
         {-3849, 1401},  {-3849, -1401}, {-2048, -3547}, {711, -4034},
         {3138, -2633},  {4096, 0},      {3314, 2408},   {1266, 3896},
         {-1266, 3896},  {-3314, 2408},  {-4096, 0},     {-3314, -2408},
         {-1266, -3896}, {1266, -3896},  {3314, -2408},  {4096, 0},
         {3446, 2214},   {1702, 3726},   {-583, 4054},   {-2682, 3096},
         {-3930, 1154},  {-3930, -1154}, {-2682, -3096}, {-583, -4054},
         {1702, -3726},  {3446, -2214},  {4096, 0},      {3547, 2048},
         {2048, 3547},   {0, 4096},      {-2048, 3547},  {-3547, 2048},
         {-4096, 0},     {-3547, -2048}, {-2048, -3547}, {0, -4096},
         {2048, -3547},  {3547, -2048},  {4096, 0},      {3627, 1904},
         {2327, 3371},   {494, 4066},    {-1452, 3830},  {-3066, 2716},
         {-3977, 980},   {-3977, -980},  {-3066, -2716}, {-1452, -3830},
         {494, -4066},   {2327, -3371},  {3627, -1904},  {4096, 0},
         {3690, 1777},   {2554, 3202},   {911, 3993},    {-911, 3993},
         {-2554, 3202},  {-3690, 1777},  {-4096, 0},     {-3690, -1777},
         {-2554, -3202}, {-911, -3993},  {911, -3993},   {2554, -3202},
         {3690, -1777},  {4096, 0},      {3742, 1666},   {2741, 3044},
         {1266, 3896},   {-428, 4074},   {-2048, 3547},  {-3314, 2408},
         {-4006, 852},   {-4006, -852},  {-3314, -2408}, {-2048, -3547},
         {-428, -4074},  {1266, -3896},  {2741, -3044},  {3742, -1666},
         {4096, 0},      {3784, 1567},   {2896, 2896},   {1567, 3784},
         {0, 4096},      {-1567, 3784},  {-2896, 2896},  {-3784, 1567},
         {-4096, 0},     {-3784, -1567}, {-2896, -2896}, {-1567, -3784},
         {0, -4096},     {1567, -3784},  {2896, -2896},  {3784, -1567}}};

// `value` times `radius` over UNIT, rounded half away from zero.
std::int32_t scaled(std::int16_t value, std::int32_t radius) {
  const std::int64_t product = std::int64_t{value} * radius;
  const std::int64_t half = product < 0 ? -(UNIT / 2) : UNIT / 2;
  return static_cast<std::int32_t>((product + half) / UNIT);
}

std::int64_t distance_squared(Position from, Position toward) {
  const std::int64_t across = std::int64_t{toward.x} - from.x;
  const std::int64_t down = std::int64_t{toward.y} - from.y;
  return (across * across) + (down * down);
}

Position shifted(Position origin, Position offset) {
  return Position{origin.x + offset.x, origin.y + offset.y};
}

}  // namespace

Tokens default_tokens() {
  constexpr std::array<std::uint32_t, 7> BY_RANK{2, 2, 3, 3, 4, 4, 6};
  constexpr std::uint64_t CIRCLING = 30;
  constexpr std::uint64_t LEASE = 90;
  return Tokens{{BY_RANK.begin(), BY_RANK.end()}, CIRCLING, LEASE};
}

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

std::optional<Ring> ring_for(std::int32_t reach) {
  if (reach <= 0 || reach > LONG_REACH) {
    return std::nullopt;
  }
  return reach <= ARM_REACH ? Ring::NEAR : Ring::LONG;
}

SlotRing slot_ring(std::int32_t radius, std::int32_t separation) {
  std::size_t count = radius > 0 ? MAX_SLOTS : 0;
  // Neighbouring slots lie closer than the separation.
  const auto crowded = [](const SlotRing& ring, std::int32_t apart) {
    return distance_squared(slot_offset(ring, 0), slot_offset(ring, 1)) <
           std::int64_t{apart} * apart;
  };
  while (count > 1 && crowded(SlotRing{radius, count}, separation)) {
    --count;
  }
  return SlotRing{radius, count};
}

Position slot_offset(const SlotRing& ring, std::size_t slot) {
  const std::array<std::int16_t, 2>& unit =
      UNIT_CIRCLE.at((ring.count * (ring.count - 1) / 2) + slot);
  return Position{scaled(unit.at(0), ring.radius),
                  scaled(unit.at(1), ring.radius)};
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
  engaging_.resize(people, false);
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

void Attacks::assign(std::vector<Engaging>& engaging, const Map& map,
                     std::int32_t separation) {
  // Frees what anyone holds against another foe or on another ring, and
  // what anyone not engaging holds.
  for (Engaging& each : engaging) {
    each.distance = distance_squared(each.from, each.foe_at);
    each.aim.reset();
    const std::optional<Engagement> held = engagement(each.person);
    const std::optional<Ring> ring = ring_for(each.reach);
    if (held && (!ring || held->foe != each.foe || held->ring != *ring)) {
      release(each.person);
    }
    engaging_.at(each.person) = true;
  }
  for (std::size_t person = 0; person < foe_.size(); ++person) {
    if (!engaging_.at(person)) {
      release(person);
    }
    engaging_.at(person) = false;
  }
  // Each foe's rings in turn, the people on one in the order they choose.
  const auto key = [](const Engaging& each) {
    const std::optional<Ring> ring = ring_for(each.reach);
    return std::tuple{each.foe, ring ? static_cast<int>(*ring) : -1,
                      each.distance, each.person};
  };
  std::sort(engaging.begin(), engaging.end(),
            [&key](const Engaging& left, const Engaging& right) {
              return key(left) < key(right);
            });
  for (auto first = engaging.begin(); first != engaging.end();) {
    const std::optional<Ring> ring = ring_for(first->reach);
    const auto last =
        std::find_if(first, engaging.end(), [&](const Engaging& each) {
          return each.foe != first->foe || ring_for(each.reach) != ring;
        });
    if (ring) {
      std::int32_t shortest = first->reach;
      for (auto each = first; each != last; ++each) {
        shortest = std::min(shortest, each->reach);
      }
      seat(first, last, *ring, slot_ring(shortest - RING_INSET, separation),
           map);
    }
    first = last;
  }
}

void Attacks::seat(std::vector<Engaging>::iterator first,
                   std::vector<Engaging>::iterator last, Ring ring,
                   const SlotRing& slots, const Map& map) {
  const std::size_t count = slots.count;
  const Position foe_at = first->foe_at;
  const auto place = [&](std::size_t slot) {
    return shifted(foe_at, slot_offset(slots, slot));
  };
  const auto offered = [&](std::size_t slot) {
    return map.passable(cell_of(place(slot)));
  };
  // A slot held stays held while the ring still offers it.
  for (auto each = first; each != last; ++each) {
    const std::optional<Engagement> held = engagement(each->person);
    if (held && (held->slot >= count || !offered(held->slot))) {
      release(each->person);
    }
  }
  for (auto each = first; each != last; ++each) {
    std::optional<Engagement> held = engagement(each->person);
    if (!held) {
      std::optional<std::size_t> nearest;
      std::int64_t best = 0;
      for (std::size_t slot = 0; slot < count; ++slot) {
        const auto index = static_cast<std::uint8_t>(slot);
        const std::int64_t distance = distance_squared(each->from, place(slot));
        if (!taken(each->foe, ring, index) && offered(slot) &&
            (!nearest || distance < best)) {
          nearest = slot;
          best = distance;
        }
      }
      if (nearest) {
        held = Engagement{each->foe, ring, static_cast<std::uint8_t>(*nearest),
                          false,     0,    0};
        engage(each->person, *held);
      }
    }
    if (held) {
      each->aim = place(held->slot);
    }
  }
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
