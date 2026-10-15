#include "attack.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
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

// The slot below `count` nearest `from` of those `open` accepts, each where
// `place` puts it, the lower among equals; none when it accepts none.
template <typename Place, typename Open>
std::optional<std::uint8_t> nearest_open(Position from, std::size_t count,
                                         const Place& place, const Open& open) {
  std::optional<std::uint8_t> nearest;
  std::int64_t best = 0;
  for (std::size_t slot = 0; slot < count; ++slot) {
    const std::int64_t distance = distance_squared(from, place(slot));
    if (open(slot) && (!nearest || distance < best)) {
      nearest = static_cast<std::uint8_t>(slot);
      best = distance;
    }
  }
  return nearest;
}

// The first slot of `count` that `open` accepts going round from `slot`,
// onward for an even `person` and back for an odd one; none when it accepts
// none.
template <typename Open>
std::optional<std::uint8_t> next_round(std::uint8_t slot, std::size_t count,
                                       std::size_t person, const Open& open) {
  const std::size_t step = person % 2 == 0 ? 1 : count - 1;
  for (std::size_t turn = 1; turn < count; ++turn) {
    const std::size_t next = (slot + (step * turn)) % count;
    if (open(next)) {
      return static_cast<std::uint8_t>(next);
    }
  }
  return std::nullopt;
}

}  // namespace

Tokens default_tokens() {
  constexpr std::array<std::uint32_t, 7> BY_RANK{4, 4, 4, 4, 5, 5, 6};
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
  if (reach <= 0) {
    return std::nullopt;
  }
  if (reach > LONG_REACH) {
    return Ring::FAR;
  }
  return reach <= ARM_REACH ? Ring::NEAR : Ring::LONG;
}

std::int64_t waiting_reach(std::int32_t reach, std::int32_t separation) {
  return std::int64_t{reach} + separation + RING_INSET;
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
    const bool out_of_range =
        each.person >= people || held.foe >= people ||
        each.person == held.foe ||
        (held.slot && (*held.slot >= MAX_SLOTS || held.ring == Ring::FAR)) ||
        (held.token && held.ring == Ring::WAITING);
    if (out_of_range || attacks.foe_.at(each.person) != NONE ||
        (held.slot && attacks.taken(held.foe, held.ring, *held.slot)) ||
        (held.token && attacks.tokens_held_.at(held.foe) >= MAX_TOKENS)) {
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
  flags_.at(person) = static_cast<std::uint8_t>(
      (static_cast<unsigned>(engagement.ring) << RING_SHIFT) |
      (engagement.token ? TOKEN : 0U));
  waiting_since_.at(person) = engagement.waiting_since;
  token_since_.at(person) = engagement.token_since;
  sit(person, engagement.slot);
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
  sit(person, std::nullopt);
  if (held->token) {
    --tokens_held_.at(held->foe);
  }
  foe_.at(person) = NONE;
  flags_.at(person) = 0;
  --engaged_;
}

void Attacks::assign(std::vector<Engaging>& engaging, const Map& map,
                     std::int32_t separation, const Tokens& tokens,
                     std::uint32_t now, const Crowded& crowded) {
  const auto within = free_stale(engaging, separation);
  std::sort(engaging.begin(), within,
            [](const Engaging& left, const Engaging& right) {
              return std::tuple{left.foe, left.distance, left.person} <
                     std::tuple{right.foe, right.distance, right.person};
            });
  for (auto first = engaging.begin(); first != within;) {
    const auto last = std::find_if(first, within, [&](const Engaging& each) {
      return each.foe != first->foe;
    });
    grant(first, last, tokens, now);
    const auto waiters = std::find_if(
        first, last, [](const Engaging& each) { return !each.strikes; });
    for (auto ring = first; ring != waiters;) {
      const Ring kind = ring_for(ring->reach).value_or(Ring::NEAR);
      const auto next = std::find_if(ring, waiters, [&](const Engaging& each) {
        return ring_for(each.reach) != kind;
      });
      std::int32_t shortest = ring->reach;
      for (auto each = ring; each != next; ++each) {
        shortest = std::min(shortest, each->reach);
      }
      if (kind != Ring::FAR) {
        seat(ring, next, kind, slot_ring(shortest - RING_INSET, separation),
             map, crowded);
      }
      ring = next;
    }
    seat_waiting(waiters, last, separation, tokens, now, map, crowded);
    first = last;
  }
}

Attacks::Group Attacks::free_stale(std::vector<Engaging>& engaging,
                                   std::int32_t separation) {
  for (Engaging& each : engaging) {
    each.distance = distance_squared(each.from, each.foe_at);
    each.strikes = false;
    each.approach = OnFoe{};
    const std::int64_t reach = waiting_reach(each.reach, separation);
    const std::optional<Engagement> held = engagement(each.person);
    const std::optional<Ring> ring = ring_for(each.reach);
    const bool beyond = !ring || each.distance > reach * reach;
    if (held && (beyond || held->foe != each.foe ||
                 (held->ring != Ring::WAITING && held->ring != *ring))) {
      release(each.person);
    }
    engaging_.at(each.person) = !beyond;
  }
  for (std::size_t person = 0; person < foe_.size(); ++person) {
    if (!engaging_.at(person)) {
      release(person);
    }
  }
  const auto within = std::stable_partition(
      engaging.begin(), engaging.end(),
      [this](const Engaging& each) { return engaging_.at(each.person); });
  for (const Engaging& each : engaging) {
    engaging_.at(each.person) = false;
  }
  return within;
}

void Attacks::grant(Group first, Group last, const Tokens& tokens,
                    std::uint32_t now) {
  const std::uint32_t granted = std::min(first->tokens, MAX_TOKENS);
  line_up(first, last, now, lines_);
  lapse(lines_, granted, tokens, now);
  claims(lines_, granted, order_);
  hand_out(first, last, lines_, order_, now);
}

void Attacks::line_up(Group first, Group last, std::uint32_t now,
                      Lines& lines) const {
  lines.holders.clear();
  lines.queued.clear();
  lines.newcomers.clear();
  lines.lapsed.clear();
  for (auto each = first; each != last; ++each) {
    const std::optional<Engagement> held = engagement(each->person);
    if (each->claim == Claim::EXEMPT) {
      each->strikes = true;
    } else if (held && held->token) {
      lines.holders.push_back(each);
    } else if (held && held->ring == Ring::WAITING) {
      lines.queued.push_back(each);
    } else {
      lines.newcomers.push_back(each);
    }
  }
  // The ticks since `ticks` notes for a person, how long it held its token
  // or waited; each here is engaged, so reads its own.
  const auto since = [&](Group each, const std::vector<std::uint32_t>& ticks) {
    return now - ticks.at(each->person);
  };
  std::sort(lines.holders.begin(), lines.holders.end(),
            [&](Group left, Group right) {
              return std::tuple{since(left, token_since_), right->person} <
                     std::tuple{since(right, token_since_), left->person};
            });
  std::stable_sort(
      lines.queued.begin(), lines.queued.end(), [&](Group left, Group right) {
        return std::tuple{since(right, waiting_since_), left->person} <
               std::tuple{since(left, waiting_since_), right->person};
      });
}

void Attacks::lapse(Lines& lines, std::uint32_t granted, const Tokens& tokens,
                    std::uint32_t now) const {
  std::vector<Group>& holders = lines.holders;
  const auto give_up = [&](std::size_t index) {
    lines.lapsed.push_back(holders.at(index));
    holders.erase(holders.begin() + static_cast<std::ptrdiff_t>(index));
  };
  while (holders.size() > granted) {
    give_up(0);
  }
  const std::size_t waiting = lines.queued.size() + lines.newcomers.size();
  if (tokens.lease == 0 || waiting <= granted - holders.size()) {
    return;
  }
  for (std::size_t index = holders.size(); index-- > 0;) {
    const Engagement held =
        engagement(holders.at(index)->person).value_or(Engagement{});
    if (holders.at(index)->claim != Claim::FIRST &&
        now - held.token_since >= tokens.lease) {
      give_up(index);
    }
  }
}

void Attacks::claims(Lines& lines, std::uint32_t granted,
                     std::vector<Group>& order) {
  // Claim::FIRST ahead of the queue, displacing the holder that took its
  // token last when none is free; then the queue, newcomers, and those who
  // gave theirs up.
  const std::array<const std::vector<Group>*, 3> waiting{
      &lines.queued, &lines.newcomers, &lines.lapsed};
  order.clear();
  for (const std::vector<Group>* line : waiting) {
    std::copy_if(line->begin(), line->end(), std::back_inserter(order),
                 [](Group each) { return each->claim == Claim::FIRST; });
  }
  std::vector<Group>& holders = lines.holders;
  for (std::size_t first = order.size();
       first > 0 && holders.size() + first > granted && !holders.empty() &&
       holders.front()->claim != Claim::FIRST;
       --first) {
    lines.lapsed.push_back(holders.front());
    holders.erase(holders.begin());
  }
  for (const std::vector<Group>* line : waiting) {
    std::copy_if(line->begin(), line->end(), std::back_inserter(order),
                 [](Group each) { return each->claim != Claim::FIRST; });
  }
}

void Attacks::hand_out(Group first, Group last, const Lines& lines,
                       const std::vector<Group>& order, std::uint32_t now) {
  const std::size_t foe = first->foe;
  const std::uint32_t granted = std::min(first->tokens, MAX_TOKENS);
  std::size_t free =
      granted - std::min<std::size_t>(lines.holders.size(), granted);
  for (const auto each : lines.holders) {
    each->strikes = true;
  }
  for (const auto each : order) {
    const std::optional<Engagement> held = engagement(each->person);
    const Ring ring = ring_for(each->reach).value_or(Ring::NEAR);
    if (free > 0) {
      --free;
      each->strikes = true;
      engage(each->person, Engagement{foe, ring, std::nullopt, true,
                                      held ? held->waiting_since : now, now});
    } else if (!held || held->ring != Ring::WAITING) {
      engage(each->person,
             Engagement{foe, Ring::WAITING, std::nullopt, false, now, 0});
    }
  }
  for (auto each = first; each != last; ++each) {
    const std::optional<Engagement> held = engagement(each->person);
    if (each->claim == Claim::EXEMPT &&
        (!held || held->token || held->ring == Ring::WAITING)) {
      engage(each->person,
             Engagement{foe, ring_for(each->reach).value_or(Ring::NEAR),
                        std::nullopt, false, now, 0});
    }
  }
  // Strikers by ring, distance, then person; then waiters in queue order.
  ranked_.clear();
  std::int64_t position = 0;
  for (const auto each : order) {
    if (!each->strikes) {
      ranked_.emplace_back(Rank{1, 0, position++, 0}, *each);
    }
  }
  for (auto each = first; each != last; ++each) {
    if (each->strikes) {
      const Ring ring = ring_for(each->reach).value_or(Ring::NEAR);
      ranked_.emplace_back(
          Rank{0, static_cast<int>(ring), each->distance, each->person}, *each);
    }
  }
  std::sort(ranked_.begin(), ranked_.end(),
            [](const auto& left, const auto& right) {
              return left.first < right.first;
            });
  std::transform(ranked_.begin(), ranked_.end(), first,
                 [](const auto& entry) { return entry.second; });
}

void Attacks::seat(Group first, Group last, Ring ring, const SlotRing& slots,
                   const Map& map, const Crowded& crowded) {
  const std::size_t count = slots.count;
  const Position foe_at = first->foe_at;
  const std::size_t foe = first->foe;
  const auto place = [&](std::size_t slot) {
    return shifted(foe_at, slot_offset(slots, slot));
  };
  const std::uint16_t offered = offered_slots(*first, slots, map, crowded);
  const auto open = [&](std::size_t slot) {
    return (offered & bit(static_cast<std::uint8_t>(slot))) != 0 &&
           !taken(foe, ring, static_cast<std::uint8_t>(slot));
  };
  // A slot held stays held while the ring still offers it.
  unseat(first, last, count, offered);
  for (auto each = first; each != last; ++each) {
    std::optional<std::uint8_t> slot =
        engagement(each->person).value_or(Engagement{}).slot;
    if (!slot) {
      slot = nearest_open(each->from, count, place, open);
      sit(each->person, slot);
    }
    if (slot) {
      each->approach = ToSlot{place(waypoint(*each, slots, *slot))};
    }
  }
}

void Attacks::seat_waiting(Group first, Group last, std::int32_t separation,
                           const Tokens& tokens, std::uint32_t now,
                           const Map& map, const Crowded& crowded) {
  if (first == last) {
    return;
  }
  // The longest reach waiting on the ring sizes it; a reach beyond LONG_REACH
  // waits on none.
  std::int32_t longest = 0;
  for (auto each = first; each != last; ++each) {
    if (each->reach <= LONG_REACH) {
      longest = std::max(longest, each->reach);
    }
  }
  const SlotRing slots = slot_ring(
      longest > 0 ? longest + separation + RING_INSET : 0, separation);
  const std::size_t count = slots.count;
  const std::size_t foe = first->foe;
  const auto place = [&](std::size_t slot) {
    return shifted(first->foe_at, slot_offset(slots, slot));
  };
  const std::uint16_t offered =
      count > 0 ? offered_slots(*first, slots, map, crowded) : 0;
  const auto open = [&](std::size_t slot) {
    return (offered & bit(static_cast<std::uint8_t>(slot))) != 0 &&
           !taken(foe, Ring::WAITING, static_cast<std::uint8_t>(slot));
  };
  unseat(first, last, count, offered);
  for (auto each = first; each != last; ++each) {
    const Engagement held = engagement(each->person).value_or(Engagement{});
    std::optional<std::uint8_t> slot = held.slot;
    const std::uint32_t waited = now - held.waiting_since;
    const bool circles =
        tokens.circling > 0 && waited > 0 && waited % tokens.circling == 0;
    if (slot && circles) {
      slot = next_round(*slot, count, each->person, open).value_or(*slot);
      sit(each->person, slot);
    }
    if (!slot && each->reach <= LONG_REACH) {
      slot = nearest_open(each->from, count, place, open);
      sit(each->person, slot);
    }
    each->approach = slot ? Approach{ToWait{place(*slot)}} : Approach{Hold{}};
  }
}

void Attacks::unseat(Group first, Group last, std::size_t count,
                     std::uint16_t offered) {
  for (auto each = first; each != last; ++each) {
    const std::optional<std::uint8_t> slot =
        engagement(each->person).value_or(Engagement{}).slot;
    if (slot && (*slot >= count || (offered & bit(*slot)) == 0 ||
                 each->reach > LONG_REACH)) {
      sit(each->person, std::nullopt);
    }
  }
}

std::uint16_t offered_slots(const Engaging& engaging, const SlotRing& slots,
                            const Map& map, const Crowded& crowded) {
  std::uint16_t offered = 0;
  for (std::size_t slot = 0; slot < slots.count; ++slot) {
    const Position point = shifted(engaging.foe_at, slot_offset(slots, slot));
    if (map.passable(cell_of(point)) &&
        !(crowded && crowded(point, engaging.foe))) {
      offered |= bit(static_cast<std::uint8_t>(slot));
    }
  }
  return offered;
}

std::size_t waypoint(const Engaging& seeker, const SlotRing& slots,
                     std::size_t slot) {
  const std::size_t count = slots.count;
  std::size_t nearest = 0;
  std::int64_t best = 0;
  for (std::size_t other = 0; other < count; ++other) {
    const std::int64_t distance = distance_squared(
        seeker.from, shifted(seeker.foe_at, slot_offset(slots, other)));
    if (other == 0 || distance < best) {
      nearest = other;
      best = distance;
    }
  }
  // Slots round from the nearest to its own, the way the slots are numbered.
  const std::size_t round = (slot + count - nearest) % count;
  if (round <= 1 || round >= count - 1) {
    return slot;
  }
  const std::int64_t band = std::int64_t{slots.radius} + (CELL_SIZE / 2);
  if (distance_squared(seeker.from, seeker.foe_at) > band * band) {
    return nearest;
  }
  return round <= count / 2 ? (nearest + 1) % count
                            : (nearest + count - 1) % count;
}

std::optional<Engagement> Attacks::engagement(std::size_t person) const {
  if (person >= foe_.size() || foe_.at(person) == NONE) {
    return std::nullopt;
  }
  const std::uint8_t flags = flags_.at(person);
  return Engagement{foe_.at(person),
                    static_cast<Ring>(flags >> RING_SHIFT),
                    (flags & SEATED) != 0
                        ? std::optional<std::uint8_t>{slot_.at(person)}
                        : std::nullopt,
                    (flags & TOKEN) != 0,
                    waiting_since_.at(person),
                    token_since_.at(person)};
}

void Attacks::sit(std::size_t person, std::optional<std::uint8_t> slot) {
  const std::size_t foe = foe_.at(person);
  const auto ring = static_cast<Ring>(flags_.at(person) >> RING_SHIFT);
  if ((flags_.at(person) & SEATED) != 0) {
    taken_mask(foe, ring) &= static_cast<std::uint16_t>(~bit(slot_.at(person)));
  }
  flags_.at(person) = static_cast<std::uint8_t>(
      (flags_.at(person) & static_cast<std::uint8_t>(~SEATED)) |
      (slot ? SEATED : 0U));
  slot_.at(person) = slot.value_or(0);
  if (slot) {
    taken_mask(foe, ring) |= bit(*slot);
  }
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
    case Ring::FAR:
      break;
  }
  throw std::logic_error("Attacks: Ring::FAR holds no slot");
}

std::uint16_t Attacks::taken_mask(std::size_t foe, Ring ring) const {
  switch (ring) {
    case Ring::NEAR:
      return near_taken_.at(foe);
    case Ring::LONG:
      return long_taken_.at(foe);
    case Ring::WAITING:
      return waiting_taken_.at(foe);
    case Ring::FAR:
      break;
  }
  return 0;
}

}  // namespace murim
