#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "map.hpp"

namespace murim {

/// The most slots a ring around a foe holds, so a ring's slots fit a 16-bit
/// mask (ADR 0068).
constexpr std::size_t MAX_SLOTS = 16;

/// The most tokens a person grants: as many as its two attack rings seat.
constexpr std::uint32_t MAX_TOKENS = 2 * MAX_SLOTS;

/// The most ticks a waiter circles at or a holder keeps a token, so the
/// ticks between when it began and now fit the 32 bits they are kept in.
constexpr std::uint64_t MAX_HOLD_TICKS = UINT32_MAX;

/// How many people may strike one person at once, and how those waiting
/// circle, as rules data (ADR 0068).
struct Tokens {
  /// The tokens a person grants by its realm rank: the first row for no
  /// realm, and the last for every rank past the table.
  std::vector<std::uint32_t> by_rank;
  /// Ticks a waiter keeps its waiting slot before moving on to the next.
  std::uint64_t circling = 0;
  /// Ticks a holder keeps a token while someone waits; 0 for no lease.
  std::uint64_t lease = 0;
};

/// ADR 0068's default table: 2 tokens for no realm and the lowest rank, 3
/// for the next two, 4 for the next two and 6 above, a waiter circling every
/// second and a holder leasing its token for three while others wait.
[[nodiscard]] Tokens default_tokens();

/// Whether `tokens` gives a row for no realm, no row above MAX_TOKENS, and a
/// circling period and lease within MAX_HOLD_TICKS.
[[nodiscard]] bool valid(const Tokens& tokens);

/// Whether `tokens` is none, for no limit, or valid as above.
[[nodiscard]] bool valid(const std::optional<Tokens>& tokens);

/// The ring around a foe a slot lies on: arm's length, long reach, or the
/// waiting ring outside both.
enum class Ring : std::uint8_t { NEAR, LONG, WAITING };

/// The rings a slot may lie on, for a tag read from a save.
constexpr std::uint8_t RING_KINDS = 3;

/// What one person holds against the foe it engages: its slot on a ring,
/// whether it holds one of the foe's tokens, and the ticks it began to wait
/// and took the token, each kept modulo 2^32, since only the ticks between
/// them and now are read.
struct Engagement {
  std::size_t foe = 0;
  Ring ring = Ring::NEAR;
  std::uint8_t slot = 0;
  bool token = false;
  std::uint32_t waiting_since = 0;
  std::uint32_t token_since = 0;
};

/// One engaged person and what it holds, as a save keeps it.
struct Engaged {
  std::size_t person = 0;
  Engagement engagement;
};

/// The longest reach that strikes from a slot at arm's length, and the
/// longest that strikes from one at long reach; a longer reach strikes from
/// where it stands.
constexpr std::int32_t ARM_REACH = 2 * CELL_SIZE;
constexpr std::int32_t LONG_REACH = 4 * CELL_SIZE;

/// How far inside the shortest reach on it a ring lies: two steps, so a
/// striker jostled off its slot stays in reach.
constexpr std::int32_t RING_INSET = 32;

/// How near its slot a striker counts as on it.
constexpr std::int32_t ON_SLOT = CELL_SIZE / 4;

/// The ring a striker of reach `reach` takes a slot on, or nothing for no
/// reach or one beyond LONG_REACH.
[[nodiscard]] std::optional<Ring> ring_for(std::int32_t reach);

/// A ring of attack slots round a foe: how far out it lies and how many
/// slots it holds.
struct SlotRing {
  std::int32_t radius = 0;
  std::size_t count = 0;
};

/// The ring of radius `radius` holding the most slots, up to MAX_SLOTS, whose
/// neighbours lie at least `separation` apart; none for no radius.
[[nodiscard]] SlotRing slot_ring(std::int32_t radius, std::int32_t separation);

/// Slot `slot` of `ring`, as an offset from its foe: `slot / count` of a turn
/// from east, toward south, from a fixed table.
[[nodiscard]] Position slot_offset(const SlotRing& ring, std::size_t slot);

/// A person engaging a foe on this tick, with where both stood as it began,
/// and the point it closes on, which Attacks::assign gives.
struct Engaging {
  std::size_t person = 0;
  std::size_t foe = 0;
  std::int32_t reach = 0;
  Position from{0, 0};
  Position foe_at{0, 0};
  std::optional<Position> aim = std::nullopt;
  std::int64_t distance = 0;
};

/// The attack slots and tokens of every person, a person's as an engager
/// and a foe's as the one engaged, in arrays indexed by person that grow
/// only as the world's people do.
class Attacks {
 public:
  Attacks() = default;

  /// No one engaged, among `people` people.
  explicit Attacks(std::size_t people);

  /// `engaged`, as a save gives it, among `people` people; nothing when a
  /// person or foe lies at or past `people`, a person engages itself or is
  /// given twice, a slot lies at or past MAX_SLOTS, a waiter holds a token,
  /// or two people take one slot.
  [[nodiscard]] static std::optional<Attacks> of(
      std::size_t people, const std::vector<Engaged>& engaged);

  /// Makes room for `people` people, keeping what everyone holds.
  void grow_to(std::size_t people);

  /// Gives `person` `engagement`, replacing what it held. `person` and the
  /// foe lie below the people there is room for, and the slot below
  /// MAX_SLOTS.
  void engage(std::size_t person, const Engagement& engagement);

  /// Frees what `person` holds.
  void release(std::size_t person);

  /// Gives each of `engaging` a slot around its foe, on the ring for its
  /// reach on `map`, keeping one it holds there; frees what anyone else holds.
  /// A ring's radius is the shortest reach on it less RING_INSET, and its
  /// slots are those slot_ring() spaces `separation` apart, each on a
  /// passable cell. Those without a slot take, in order of distance squared
  /// from their foe then person, the free slot nearest them, the lower slot
  /// among equals. Each gets its slot's position as its aim, or none.
  /// `engaging` is reordered.
  void assign(std::vector<Engaging>& engaging, const Map& map,
              std::int32_t separation);

  /// What `person` holds, or nothing while it engages no one.
  [[nodiscard]] std::optional<Engagement> engagement(std::size_t person) const;

  /// Whether `foe`'s slot `slot` on `ring` is taken.
  [[nodiscard]] bool taken(std::size_t foe, Ring ring, std::uint8_t slot) const;

  /// The tokens `foe` has granted.
  [[nodiscard]] std::uint32_t tokens_held(std::size_t foe) const;

  /// Whether anyone engages anyone.
  [[nodiscard]] bool any() const;

  /// The engaged people and what each holds, by person.
  [[nodiscard]] std::vector<Engaged> engaged() const;

  /// The people there is room for.
  [[nodiscard]] std::size_t people() const;

 private:
  static constexpr std::uint32_t NONE = UINT32_MAX;
  // A person's flags: the token held in the lowest bit, the ring above it.
  static constexpr std::uint8_t TOKEN = 1U;
  static constexpr unsigned RING_SHIFT = 1U;

  std::vector<std::uint32_t> foe_;
  std::vector<std::uint8_t> slot_;
  std::vector<std::uint8_t> flags_;
  std::vector<std::uint32_t> waiting_since_;
  std::vector<std::uint32_t> token_since_;
  std::vector<std::uint16_t> near_taken_;
  std::vector<std::uint16_t> long_taken_;
  std::vector<std::uint16_t> waiting_taken_;
  std::vector<std::uint8_t> tokens_held_;
  std::size_t engaged_ = 0;
  // Whether each person engages on the tick being assigned.
  std::vector<bool> engaging_;

  // Seats the people of `first` to `last`, all engaging one foe on `ring`, on
  // its `slots`, as assign() does.
  void seat(std::vector<Engaging>::iterator first,
            std::vector<Engaging>::iterator last, Ring ring,
            const SlotRing& slots, const Map& map);
  [[nodiscard]] std::uint16_t& taken_mask(std::size_t foe, Ring ring);
  [[nodiscard]] std::uint16_t taken_mask(std::size_t foe, Ring ring) const;
};

}  // namespace murim
