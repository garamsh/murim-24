#include "attack.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

constexpr std::size_t PEOPLE = 4;
constexpr std::size_t FOE = 3;

// Person `person` on slot `slot` of `ring` round FOE, holding a token or not.
Engaged on(std::size_t person, Ring ring, std::uint8_t slot, bool token) {
  return Engaged{person, Engagement{FOE, ring, slot, token, 0, 0}};
}

TEST(Tokens, ATableIsValidWithARowForNoRealmAndHoldsWithinBounds) {
  const Tokens region{{2, 2, 3, 3, 4, 4, 6}, 30, 90};
  // Control: the region's table is valid, so each refusal is its change's.
  EXPECT_TRUE(valid(region));
  Tokens refused = region;
  refused.by_rank.clear();
  EXPECT_FALSE(valid(refused));
  refused = region;
  refused.by_rank.back() = MAX_TOKENS + 1;
  EXPECT_FALSE(valid(refused));
  refused = region;
  refused.circling = MAX_HOLD_TICKS + 1;
  EXPECT_FALSE(valid(refused));
  refused = region;
  refused.lease = MAX_HOLD_TICKS + 1;
  EXPECT_FALSE(valid(refused));
  // The bounds themselves are accepted.
  const Tokens widest{{MAX_TOKENS}, MAX_HOLD_TICKS, MAX_HOLD_TICKS};
  EXPECT_TRUE(valid(widest));
  // No table is no limit, and a table given is checked.
  EXPECT_TRUE(valid(std::optional<Tokens>{}));
  EXPECT_TRUE(valid(std::optional<Tokens>{region}));
  EXPECT_FALSE(valid(std::optional<Tokens>{refused}));
}

TEST(Attacks, AnEngagedPersonHoldsItsSlotAndTokenUntilReleased) {
  Attacks attacks{PEOPLE};
  ASSERT_FALSE(attacks.any());
  ASSERT_EQ(attacks.people(), PEOPLE);

  constexpr std::uint8_t SLOT = 5;
  constexpr std::uint32_t WAITING_SINCE = 40;
  constexpr std::uint32_t TOKEN_SINCE = 70;
  attacks.engage(
      0, Engagement{FOE, Ring::LONG, SLOT, true, WAITING_SINCE, TOKEN_SINCE});
  ASSERT_TRUE(attacks.engagement(0).has_value());
  const Engagement held = attacks.engagement(0).value_or(Engagement{});
  EXPECT_EQ(held.foe, FOE);
  EXPECT_EQ(held.ring, Ring::LONG);
  EXPECT_EQ(held.slot, SLOT);
  EXPECT_TRUE(held.token);
  EXPECT_EQ(held.waiting_since, WAITING_SINCE);
  EXPECT_EQ(held.token_since, TOKEN_SINCE);
  EXPECT_TRUE(attacks.any());
  EXPECT_TRUE(attacks.taken(FOE, Ring::LONG, SLOT));
  // Control: the same slot on the other rings, and the next on this one,
  // stay free.
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, SLOT));
  EXPECT_FALSE(attacks.taken(FOE, Ring::WAITING, SLOT));
  EXPECT_FALSE(attacks.taken(FOE, Ring::LONG, SLOT + 1));
  EXPECT_EQ(attacks.tokens_held(FOE), 1U);
  EXPECT_EQ(attacks.engaged().size(), 1U);

  attacks.release(0);
  EXPECT_FALSE(attacks.engagement(0).has_value());
  EXPECT_FALSE(attacks.taken(FOE, Ring::LONG, SLOT));
  EXPECT_EQ(attacks.tokens_held(FOE), 0U);
  EXPECT_FALSE(attacks.any());
}

TEST(Attacks, GrowingKeepsWhatEveryoneHolds) {
  Attacks attacks{PEOPLE};
  attacks.engage(0, Engagement{FOE, Ring::NEAR, 1, true, 0, 0});
  attacks.grow_to(2 * PEOPLE);
  EXPECT_EQ(attacks.people(), 2 * PEOPLE);
  EXPECT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, 1);
  EXPECT_EQ(attacks.tokens_held(FOE), 1U);
  // A person past the old room engages once there is room for it.
  attacks.engage(PEOPLE, Engagement{FOE, Ring::NEAR, 2, true, 0, 0});
  EXPECT_EQ(attacks.tokens_held(FOE), 2U);
  // Control: growing to fewer people keeps the room there is.
  attacks.grow_to(1);
  EXPECT_EQ(attacks.people(), 2 * PEOPLE);
}

TEST(Attacks, EngagingAgainMovesAPersonOffItsOldSlot) {
  Attacks attacks{PEOPLE};
  attacks.engage(0, Engagement{FOE, Ring::NEAR, 1, true, 0, 0});
  attacks.engage(0, Engagement{FOE, Ring::WAITING, 2, false, 0, 0});
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, 1));
  EXPECT_TRUE(attacks.taken(FOE, Ring::WAITING, 2));
  EXPECT_EQ(attacks.tokens_held(FOE), 0U);
  EXPECT_EQ(attacks.engaged().size(), 1U);
}

TEST(Attacks, ASavedEngagementIsRefusedWhenNoWorldCouldHoldIt) {
  const std::vector<Engaged> kept{on(0, Ring::NEAR, 0, true),
                                  on(1, Ring::NEAR, 1, true),
                                  on(2, Ring::WAITING, 0, false)};
  // Control: the intact entries give back the same engagements.
  ASSERT_TRUE(Attacks::of(PEOPLE, kept).has_value());
  const Attacks intact = Attacks::of(PEOPLE, kept).value_or(Attacks{PEOPLE});
  EXPECT_EQ(intact.engaged().size(), kept.size());
  EXPECT_EQ(intact.tokens_held(FOE), 2U);
  EXPECT_TRUE(intact.taken(FOE, Ring::WAITING, 0));

  const std::vector<std::vector<Engaged>> refused{
      // A person past the people.
      {on(PEOPLE, Ring::NEAR, 0, true)},
      // A foe past the people.
      {Engaged{0, Engagement{PEOPLE, Ring::NEAR, 0, true, 0, 0}}},
      // A person engaging itself.
      {on(FOE, Ring::NEAR, 0, true)},
      // A slot past the most a ring holds.
      {on(0, Ring::NEAR, MAX_SLOTS, true)},
      // A waiter holding a token.
      {on(0, Ring::WAITING, 0, true)},
      // One person given twice.
      {on(0, Ring::NEAR, 0, true), on(0, Ring::NEAR, 1, true)},
      // Two people on one slot.
      {on(0, Ring::NEAR, 0, true), on(1, Ring::NEAR, 0, false)},
  };
  for (std::size_t form = 0; form < refused.size(); ++form) {
    EXPECT_FALSE(Attacks::of(PEOPLE, refused.at(form)).has_value()) << form;
  }
}

// The basic strike's reach, a cell and a half, and the separation people
// keep, a cell.
constexpr std::int32_t BASIC_REACH = 3 * CELL_SIZE / 2;
constexpr std::int32_t APART = CELL_SIZE;
constexpr std::int32_t BASIC_RING = BASIC_REACH - RING_INSET;

TEST(Slots, AReachTakesARingByItsClass) {
  EXPECT_EQ(ring_for(BASIC_REACH), std::optional<Ring>{Ring::NEAR});
  EXPECT_EQ(ring_for(ARM_REACH), std::optional<Ring>{Ring::NEAR});
  EXPECT_EQ(ring_for(ARM_REACH + 1), std::optional<Ring>{Ring::LONG});
  EXPECT_EQ(ring_for(LONG_REACH), std::optional<Ring>{Ring::LONG});
  // A dart's reach strikes from where it stands, and no reach takes none.
  EXPECT_EQ(ring_for(LONG_REACH + 1), std::optional<Ring>{Ring::FAR});
  EXPECT_EQ(ring_for(0), std::nullopt);
}

std::int64_t distance_squared(Position from, Position toward) {
  const std::int64_t across = std::int64_t{toward.x} - from.x;
  const std::int64_t down = std::int64_t{toward.y} - from.y;
  return (across * across) + (down * down);
}

// The basic strike's ring, holding as many slots as lie a separation apart.
SlotRing basic_ring() { return slot_ring(BASIC_RING, APART); }

TEST(Slots, ARingHoldsTheMostSlotsASeparationApart) {
  // ADR 0068: the basic strike's ring of 352 units holds 8.
  constexpr std::size_t EIGHT = 8;
  ASSERT_EQ(basic_ring().count, EIGHT);
  // Control: a separation just past the 8 slots' spacing leaves 7.
  const std::int64_t spacing = distance_squared(slot_offset(basic_ring(), 0),
                                                slot_offset(basic_ring(), 1));
  std::int32_t past = 0;
  while (std::int64_t{past} * past <= spacing) {
    ++past;
  }
  EXPECT_EQ(slot_ring(BASIC_RING, past).count, EIGHT - 1);
  EXPECT_EQ(slot_ring(BASIC_RING, past - 1).count, EIGHT);
  // A wide ring holds no more than MAX_SLOTS, and no ring holds none.
  constexpr std::int32_t WIDE = 16 * CELL_SIZE;
  EXPECT_EQ(slot_ring(WIDE, APART).count, MAX_SLOTS);
  EXPECT_EQ(slot_ring(0, APART).count, 0U);
}

// A slot's offset on the basic ring's radius, as pinned.
struct Pinned {
  std::size_t count;
  std::size_t slot;
  std::int32_t x;
  std::int32_t y;
};

TEST(Slots, SlotOffsetsArePinnedFromEastTowardSouth) {
  constexpr std::array<Pinned, 8> PINNED{{{8, 0, 352, 0},
                                          {8, 1, 249, 249},
                                          {8, 2, 0, 352},
                                          {8, 4, -352, 0},
                                          {8, 6, 0, -352},
                                          {3, 1, -176, 305},
                                          {16, 1, 325, 135},
                                          {1, 0, 352, 0}}};
  for (const Pinned& pinned : PINNED) {
    const Position offset =
        slot_offset(SlotRing{BASIC_RING, pinned.count}, pinned.slot);
    EXPECT_TRUE(offset.x == pinned.x && offset.y == pinned.y)
        << "slot " << pinned.slot << " of " << pinned.count << ": " << offset.x
        << ", " << offset.y;
  }
}

// An open map ten cells a side, the foe at the centre of its cell (5, 5).
constexpr std::size_t SIDE = 10;
constexpr std::int32_t FOE_CELL = 5;
constexpr Position FOE_AT{(FOE_CELL * CELL_SIZE) + (CELL_SIZE / 2),
                          (FOE_CELL * CELL_SIZE) + (CELL_SIZE / 2)};
// How far from the foe people stand: well outside the ring, outside it by
// less, and just outside it.
constexpr std::int32_t AFAR = 640;
constexpr std::int32_t NEARBY = 400;
constexpr std::int32_t BEYOND_RING = 600;

Map open_ground() {
  return Map{std::vector<std::string>(SIDE, std::string(SIDE, '.'))};
}

// FOE_AT moved `across` east and `down` south.
Position from_foe(std::int32_t across, std::int32_t down) {
  return Position{FOE_AT.x + across, FOE_AT.y + down};
}

// Person `person` at `from` engaging FOE at FOE_AT with the basic reach, the
// foe granting as many tokens as can be, so everyone strikes.
Engaging toward(std::size_t person, Position from) {
  return Engaging{person, FOE, BASIC_REACH, from, FOE_AT, MAX_TOKENS};
}

// ADR 0068's default table, for tests where every foe grants all the tokens
// anyone engaging it asks for.
const Tokens STRIKING = default_tokens();

// The point `engaging` closes on toward its slot, or the origin without one.
Position aimed(const Engaging& engaging) {
  const auto* slot = std::get_if<ToSlot>(&engaging.approach);
  return slot != nullptr ? slot->at : Position{};
}

// The point slot `slot` of the basic ring round FOE_AT lies at.
Position seat(std::size_t slot) {
  const Position offset = slot_offset(basic_ring(), slot);
  return from_foe(offset.x, offset.y);
}

TEST(Slots, ANewcomerTakesTheFreeSlotNearestItAndAHolderKeepsItsSlot) {
  constexpr std::uint8_t WEST = 4;
  // Person 0 west of the foe takes the west slot and closes on it.
  Attacks attacks{PEOPLE};
  std::vector<Engaging> engaging{toward(0, from_foe(-AFAR, 0))};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  ASSERT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, WEST);
  EXPECT_EQ(aimed(engaging.front()).x, seat(WEST).x);
  // Person 1, nearer the foe and nearer the west slot than person 0, takes
  // another: person 0 keeps the slot it holds.
  const Position nearer{seat(WEST).x - 1, seat(WEST).y};
  engaging = {toward(0, from_foe(-AFAR, 0)), toward(1, nearer)};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, WEST);
  EXPECT_NE(attacks.engagement(1).value_or(Engagement{}).slot, WEST);
  // Control: with nobody holding a slot, person 1 goes first, being nearer
  // the foe, and takes the west slot.
  Attacks fresh{PEOPLE};
  fresh.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(fresh.engagement(1).value_or(Engagement{}).slot, WEST);
}

TEST(Slots, OfTwoFreeSlotsAsNearAsEachOtherTheLowerIsTaken) {
  // Person 0, east of the foe and nearer it, takes slot 0; person 1, due east
  // beyond it, is then as near slot 1 as slot 7, which lie mirrored about
  // the line it stands on.
  constexpr std::uint8_t MIRRORED = 7;
  Attacks attacks{PEOPLE};
  std::vector<Engaging> engaging{toward(0, from_foe(NEARBY, 0)),
                                 toward(1, from_foe(AFAR, 0))};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  ASSERT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, 0U);
  ASSERT_EQ(distance_squared(from_foe(AFAR, 0), seat(1)),
            distance_squared(from_foe(AFAR, 0), seat(MIRRORED)));
  EXPECT_EQ(attacks.engagement(1).value_or(Engagement{}).slot, 1U);
}

TEST(Slots, ASlotFreesWhenItsHolderStopsEngagingOrTurnsToAnotherFoe) {
  constexpr std::uint8_t WEST = 4;
  Attacks attacks{PEOPLE};
  std::vector<Engaging> engaging{toward(0, from_foe(-AFAR, 0)),
                                 toward(1, from_foe(AFAR, 0))};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  // Setup: both hold a slot round FOE.
  ASSERT_TRUE(attacks.engagement(0).has_value());
  ASSERT_TRUE(attacks.engagement(1).has_value());
  // Person 0 engages no one, and person 1 turns on person 2.
  Engaging turned = toward(1, from_foe(AFAR, 0));
  turned.foe = 2;
  engaging = {turned};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_FALSE(attacks.engagement(0).has_value());
  EXPECT_EQ(attacks.engagement(1).value_or(Engagement{}).foe, 2U);
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, 0));
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, WEST));
}

TEST(Slots, AReachBeyondTheLongRingStrikesFromWhereItStandsOnNoSlot) {
  Attacks attacks{PEOPLE};
  Engaging dart = toward(0, from_foe(-AFAR, 0));
  dart.reach = LONG_REACH + 1;
  std::vector<Engaging> engaging{dart};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  const Engagement held = attacks.engagement(0).value_or(Engagement{});
  EXPECT_TRUE(held.ring == Ring::FAR && held.token && !held.slot);
  EXPECT_TRUE(std::holds_alternative<OnFoe>(engaging.front().approach));
  // Control: at the long ring's reach it takes a slot on that ring.
  engaging.front().reach = LONG_REACH;
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(attacks.engagement(0).value_or(Engagement{}).ring, Ring::LONG);
  EXPECT_TRUE(std::holds_alternative<ToSlot>(engaging.front().approach));
}

TEST(Slots, NoSlotIsOfferedOnABlockedCell) {
  // Everyone east of the foe, a wall in the next column blocking slot 0's
  // cell.
  std::vector<std::string> rows(SIDE, std::string(SIDE, '.'));
  for (std::string& row : rows) {
    row.at(FOE_CELL + 1) = '#';
  }
  const Map walled{rows};
  ASSERT_FALSE(walled.passable(cell_of(seat(0))));
  std::vector<Engaging> engaging{toward(0, from_foe(BEYOND_RING, 0))};
  Attacks attacks{PEOPLE};
  attacks.assign(engaging, walled, APART, STRIKING, 0);
  ASSERT_TRUE(attacks.engagement(0).has_value());
  EXPECT_NE(attacks.engagement(0).value_or(Engagement{}).slot, 0U);
  // Control: on open ground it takes slot 0, the one nearest it.
  Attacks open{PEOPLE};
  open.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(open.engagement(0).value_or(Engagement{}).slot, 0U);
}

// Whether a point lies within half a cell of slot `slot` of the basic ring
// round FOE_AT: a crowd standing on that slot alone.
Crowded crowding(std::size_t slot) {
  return [slot](Position point, std::size_t foe) {
    return foe == FOE && distance_squared(point, seat(slot)) <
                             std::int64_t{CELL_SIZE / 2} * (CELL_SIZE / 2);
  };
}

TEST(Slots, ACrowdedSlotIsNotOffered) {
  constexpr std::uint8_t WEST = 4;
  std::vector<Engaging> engaging{toward(0, from_foe(-AFAR, 0))};
  Attacks attacks{PEOPLE};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0, crowding(WEST));
  ASSERT_TRUE(attacks.engagement(0).has_value());
  EXPECT_NE(attacks.engagement(0).value_or(Engagement{}).slot, WEST);
  // Control: uncrowded, the west slot is the one it takes.
  Attacks clear{PEOPLE};
  clear.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(clear.engagement(0).value_or(Engagement{}).slot, WEST);
}

TEST(Slots, AHeldSlotThatBecomesCrowdedIsFreedAndAnotherTaken) {
  constexpr std::uint8_t WEST = 4;
  std::vector<Engaging> engaging{toward(0, from_foe(-AFAR, 0))};
  Attacks attacks{PEOPLE};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  // Setup: it holds the west slot.
  ASSERT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, WEST);
  Attacks kept = attacks;
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0, crowding(WEST));
  const std::optional<Engagement> moved = attacks.engagement(0);
  ASSERT_TRUE(moved.has_value());
  EXPECT_NE(moved.value_or(Engagement{}).slot, WEST);
  EXPECT_FALSE(attacks.taken(FOE, Ring::NEAR, WEST));
  // Control: a crowd on another foe's slots crowds none of FOE's, and it
  // keeps its slot.
  kept.assign(engaging, open_ground(), APART, STRIKING, 0,
              [](Position, std::size_t foe) { return foe != FOE; });
  EXPECT_EQ(kept.engagement(0).value_or(Engagement{}).slot, WEST);
}

TEST(Slots, ASeekerReachesItsSlotRoundTheRing) {
  constexpr std::size_t EAST = 0;
  constexpr std::size_t WEST = 4;
  constexpr std::size_t NORTH = 6;
  const SlotRing ring = basic_ring();
  // Off the ring's band, east of the foe, it first closes on the east slot.
  EXPECT_EQ(waypoint(toward(0, from_foe(AFAR, 0)), ring, WEST), EAST);
  // On the band beside the east slot, it walks round a slot at a time: to
  // the west, half a turn either way, forward; to the north, back.
  const Engaging on_band = toward(0, seat(EAST));
  EXPECT_EQ(waypoint(on_band, ring, WEST), EAST + 1);
  EXPECT_EQ(waypoint(on_band, ring, NORTH), ring.count - 1);
  // Control: beside its own slot, or on it, it closes on that slot.
  EXPECT_EQ(waypoint(on_band, ring, EAST + 1), EAST + 1);
  EXPECT_EQ(waypoint(on_band, ring, EAST), EAST);
  EXPECT_EQ(waypoint(toward(0, from_foe(AFAR, 0)), ring, ring.count - 1),
            ring.count - 1);
}

TEST(Slots, ASeekerOnTheBandAimsAtTheNextSlotRoundTheRing) {
  constexpr std::uint8_t WEST = 4;
  constexpr std::size_t EAST = 0;
  Attacks attacks{PEOPLE};
  std::vector<Engaging> engaging{toward(0, from_foe(-AFAR, 0))};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  // Setup: it holds the west slot.
  ASSERT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, WEST);
  // Standing on the east slot, half a turn from its own, it aims at the next
  // slot round.
  engaging = {toward(0, seat(EAST))};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  ASSERT_EQ(attacks.engagement(0).value_or(Engagement{}).slot, WEST);
  const Position next = aimed(engaging.front());
  EXPECT_TRUE(next.x == seat(EAST + 1).x && next.y == seat(EAST + 1).y);
  // Control: beside its own slot, it aims at that slot.
  engaging = {toward(0, seat(WEST - 1))};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  const Position own = aimed(engaging.front());
  EXPECT_TRUE(own.x == seat(WEST).x && own.y == seat(WEST).y);
}

// People enough for a crowd round FOE, and person `person` at `from`
// engaging it, the foe granting `tokens`, claiming as `claim`.
constexpr std::size_t CROWD = 10;

Engaging claiming(std::size_t person, Position from, std::uint32_t tokens,
                  Claim claim = Claim::QUEUED) {
  Engaging engaging = toward(person, from);
  engaging.tokens = tokens;
  engaging.claim = claim;
  return engaging;
}

// The people of `engaging` who strike, in person order.
std::vector<std::size_t> strikers(const std::vector<Engaging>& engaging) {
  std::vector<std::size_t> striking;
  for (const Engaging& each : engaging) {
    if (each.strikes) {
      striking.push_back(each.person);
    }
  }
  std::sort(striking.begin(), striking.end());
  return striking;
}

// Whether `person` holds a token of FOE.
bool holds(const Attacks& attacks, std::size_t person) {
  const std::optional<Engagement> held = attacks.engagement(person);
  return held && held->foe == FOE && held->token;
}

// The basic reach's waiting ring round FOE_AT, and where slot `slot` of it
// lies.
SlotRing waiting_ring() {
  return slot_ring(BASIC_REACH + APART + RING_INSET, APART);
}

Position waiting_seat(std::size_t slot) {
  const Position offset = slot_offset(waiting_ring(), slot);
  return from_foe(offset.x, offset.y);
}

TEST(Tokens, TheNearestTakeTheTokensAndTheRestWaitOnTheWaitingRing) {
  constexpr std::uint32_t TWO = 2;
  const auto crowd = [](std::uint32_t tokens) {
    return std::vector<Engaging>{
        claiming(0, from_foe(-NEARBY, 0), tokens),
        claiming(1, from_foe(0, -NEARBY - 50), tokens),
        claiming(2, from_foe(NEARBY + 100, 0), tokens),
        claiming(4, from_foe(0, NEARBY + 150), tokens)};
  };
  Attacks attacks{CROWD};
  std::vector<Engaging> engaging = crowd(TWO);
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(strikers(engaging), (std::vector<std::size_t>{0, 1}));
  EXPECT_EQ(attacks.tokens_held(FOE), TWO);
  for (const Engaging& each : engaging) {
    const Engagement held =
        attacks.engagement(each.person).value_or(Engagement{});
    if (each.strikes) {
      EXPECT_TRUE(held.token && held.ring == Ring::NEAR &&
                  std::holds_alternative<ToSlot>(each.approach))
          << each.person;
    } else {
      EXPECT_TRUE(!held.token && held.ring == Ring::WAITING && held.slot &&
                  std::holds_alternative<ToWait>(each.approach))
          << each.person;
    }
  }
  // Control: a foe granting four tokens has all four strike.
  Attacks plenty{CROWD};
  engaging = crowd(PEOPLE);
  plenty.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(strikers(engaging), (std::vector<std::size_t>{0, 1, 2, 4}));
}

TEST(Tokens, AFreedTokenGoesToTheLongestWaitingBeforeANearerNewcomer) {
  constexpr std::uint32_t ONE = 1;
  constexpr std::uint32_t LATER = 3;
  constexpr std::uint32_t FREED = 6;
  // Person 2 waits from tick 0; person 1, nearer and of a lower index, from
  // tick 3; person 4, nearest of all, arrives on the tick person 0 leaves.
  const Engaging holder = claiming(0, from_foe(-NEARBY, 0), ONE);
  const Engaging first = claiming(2, from_foe(0, BEYOND_RING), ONE);
  const Engaging second = claiming(1, from_foe(0, -NEARBY - 100), ONE);
  const Engaging newcomer = claiming(4, from_foe(NEARBY, 0), ONE);
  Attacks attacks{CROWD};
  std::vector<Engaging> engaging{holder, first};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  ASSERT_TRUE(holds(attacks, 0));
  engaging = {holder, first, second};
  attacks.assign(engaging, open_ground(), APART, STRIKING, LATER);
  ASSERT_TRUE(holds(attacks, 0));
  engaging = {first, second, newcomer};
  attacks.assign(engaging, open_ground(), APART, STRIKING, FREED);
  EXPECT_TRUE(holds(attacks, 2));
  EXPECT_FALSE(holds(attacks, 1) || holds(attacks, 4));
  // Control: arriving together with nobody queued, the nearest takes it.
  Attacks fresh{CROWD};
  engaging = {first, second, newcomer};
  fresh.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_TRUE(holds(fresh, 4));
  EXPECT_FALSE(holds(fresh, 1) || holds(fresh, 2));
}

TEST(Tokens, AHolderGivesItsTokenUpAfterTheLeaseWhileSomeoneWaits) {
  constexpr std::uint32_t ONE = 1;
  const auto lease = static_cast<std::uint32_t>(STRIKING.lease);
  const std::vector<Engaging> pair{claiming(0, from_foe(-NEARBY, 0), ONE),
                                   claiming(1, from_foe(0, BEYOND_RING), ONE)};
  Attacks attacks{CROWD};
  std::vector<Engaging> engaging = pair;
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  engaging = pair;
  attacks.assign(engaging, open_ground(), APART, STRIKING, lease - 1);
  ASSERT_TRUE(holds(attacks, 0));
  engaging = pair;
  attacks.assign(engaging, open_ground(), APART, STRIKING, lease);
  EXPECT_TRUE(holds(attacks, 1));
  EXPECT_EQ(attacks.engagement(0).value_or(Engagement{}).ring, Ring::WAITING);
  // Control: with nobody waiting, the holder keeps its token past the lease.
  Attacks alone{CROWD};
  engaging = {pair.front()};
  alone.assign(engaging, open_ground(), APART, STRIKING, 0);
  engaging = {pair.front()};
  alone.assign(engaging, open_ground(), APART, STRIKING, lease);
  EXPECT_TRUE(holds(alone, 0));
}

TEST(Tokens, TheSteeredTakesATokenAheadOfTheQueueFromTheLatestHolder) {
  constexpr std::uint32_t TWO = 2;
  constexpr std::uint32_t LATER = 10;
  constexpr std::uint32_t CLAIMS = 20;
  const auto with = [](Claim claim) {
    return std::vector<Engaging>{
        claiming(0, from_foe(-NEARBY, 0), TWO),
        claiming(1, from_foe(NEARBY, 0), TWO),
        claiming(2, from_foe(0, BEYOND_RING), TWO),
        claiming(4, from_foe(0, -BEYOND_RING), TWO, claim)};
  };
  const auto run = [&](Claim claim, Attacks& attacks) {
    std::vector<Engaging> engaging{claiming(0, from_foe(-NEARBY, 0), TWO)};
    attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
    engaging = {claiming(0, from_foe(-NEARBY, 0), TWO),
                claiming(1, from_foe(NEARBY, 0), TWO),
                claiming(2, from_foe(0, BEYOND_RING), TWO)};
    attacks.assign(engaging, open_ground(), APART, STRIKING, LATER);
    engaging = with(claim);
    attacks.assign(engaging, open_ground(), APART, STRIKING, CLAIMS);
  };
  Attacks attacks{CROWD};
  run(Claim::FIRST, attacks);
  // Person 1 took its token last, so it gives it up; person 2 still waits.
  EXPECT_TRUE(holds(attacks, 4));
  EXPECT_TRUE(holds(attacks, 0));
  EXPECT_FALSE(holds(attacks, 1));
  EXPECT_FALSE(holds(attacks, 2));
  // Control: claiming in the queue, it waits behind person 2.
  Attacks queued{CROWD};
  run(Claim::QUEUED, queued);
  EXPECT_TRUE(holds(queued, 0) && holds(queued, 1));
  EXPECT_FALSE(holds(queued, 4));
}

TEST(Tokens, ADuellistStrikesWithoutATokenAndTakesNone) {
  constexpr std::uint32_t ONE = 1;
  const auto with = [](Claim claim) {
    return std::vector<Engaging>{claiming(0, from_foe(-NEARBY, 0), ONE),
                                 claiming(1, from_foe(NEARBY, 0), ONE, claim)};
  };
  Attacks attacks{CROWD};
  std::vector<Engaging> engaging = with(Claim::EXEMPT);
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(strikers(engaging), (std::vector<std::size_t>{0, 1}));
  EXPECT_EQ(attacks.tokens_held(FOE), ONE);
  const Engagement duel = attacks.engagement(1).value_or(Engagement{});
  EXPECT_TRUE(!duel.token && duel.ring == Ring::NEAR && duel.slot);
  // Control: claiming in the queue, it waits.
  Attacks queued{CROWD};
  engaging = with(Claim::QUEUED);
  queued.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(strikers(engaging), (std::vector<std::size_t>{0}));
}

TEST(Tokens, AWaiterCirclesToTheNextFreeWaitingSlotEveryCirclingPeriod) {
  constexpr std::uint32_t ONE = 1;
  const auto circling = static_cast<std::uint32_t>(STRIKING.circling);
  // Person 2, even, waits south; person 1, odd, north.
  constexpr std::uint8_t SOUTH = 4;
  constexpr std::uint8_t NORTH = 12;
  const std::vector<Engaging> crowd{claiming(0, from_foe(-NEARBY, 0), ONE),
                                    claiming(1, waiting_seat(NORTH), ONE),
                                    claiming(2, waiting_seat(SOUTH), ONE)};
  Attacks attacks{CROWD};
  const auto slot_at = [&](std::uint32_t tick, std::size_t person) {
    std::vector<Engaging> engaging = crowd;
    attacks.assign(engaging, open_ground(), APART, STRIKING, tick);
    return attacks.engagement(person).value_or(Engagement{}).slot;
  };
  ASSERT_EQ(slot_at(0, 2), SOUTH);
  ASSERT_EQ(attacks.engagement(1).value_or(Engagement{}).slot, NORTH);
  // A tick short of the period, both keep their slots.
  EXPECT_EQ(slot_at(circling - 1, 2), SOUTH);
  EXPECT_EQ(attacks.engagement(1).value_or(Engagement{}).slot, NORTH);
  // On it, the even one moves onward and the odd one back.
  EXPECT_EQ(slot_at(circling, 2), SOUTH + 1);
  EXPECT_EQ(attacks.engagement(1).value_or(Engagement{}).slot, NORTH - 1);
  EXPECT_EQ(waiting_ring().count, MAX_SLOTS);
}

TEST(Tokens, AnEngagerBeyondItsWaitingReachHoldsNothing) {
  constexpr std::uint32_t ONE = 1;
  const auto reach =
      static_cast<std::int32_t>(waiting_reach(BASIC_REACH, APART));
  Attacks attacks{CROWD};
  std::vector<Engaging> engaging{claiming(0, from_foe(-NEARBY, 0), ONE)};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0);
  ASSERT_TRUE(holds(attacks, 0));
  // Fled past its waiting reach, it frees the token and closes on the foe.
  engaging = {claiming(0, from_foe(-reach - 1, 0), ONE)};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 1);
  EXPECT_FALSE(attacks.engagement(0).has_value());
  EXPECT_EQ(attacks.tokens_held(FOE), 0U);
  EXPECT_TRUE(std::holds_alternative<OnFoe>(engaging.front().approach));
  // Control: at its waiting reach, it keeps the token.
  engaging = {claiming(0, from_foe(-NEARBY, 0), ONE)};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 2);
  engaging = {claiming(0, from_foe(-reach, 0), ONE)};
  attacks.assign(engaging, open_ground(), APART, STRIKING, 3);
  EXPECT_TRUE(holds(attacks, 0));
}

TEST(Tokens, AWaiterTakesNoCrowdedWaitingSlot) {
  constexpr std::uint32_t ONE = 1;
  constexpr std::uint8_t SOUTH = 4;
  const std::vector<Engaging> crowd{claiming(0, from_foe(-NEARBY, 0), ONE),
                                    claiming(2, waiting_seat(SOUTH), ONE)};
  const Crowded south = [](Position point, std::size_t foe) {
    return foe == FOE && distance_squared(point, waiting_seat(SOUTH)) <
                             std::int64_t{APART} * APART;
  };
  Attacks attacks{CROWD};
  std::vector<Engaging> engaging = crowd;
  attacks.assign(engaging, open_ground(), APART, STRIKING, 0, south);
  const std::optional<std::uint8_t> slot =
      attacks.engagement(2).value_or(Engagement{}).slot;
  ASSERT_TRUE(slot.has_value());
  EXPECT_NE(slot, SOUTH);
  // Control: uncrowded, it takes the south slot it stands on.
  Attacks clear{CROWD};
  engaging = crowd;
  clear.assign(engaging, open_ground(), APART, STRIKING, 0);
  EXPECT_EQ(clear.engagement(2).value_or(Engagement{}).slot, SOUTH);
}

}  // namespace
}  // namespace murim
