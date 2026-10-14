#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

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

  [[nodiscard]] std::uint16_t& taken_mask(std::size_t foe, Ring ring);
  [[nodiscard]] std::uint16_t taken_mask(std::size_t foe, Ring ring) const;
};

}  // namespace murim
