#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace murim {

/// How a faction's leadership passes by duel. A person may challenge the
/// leader, if `members_only` holds, only from an army of the faction; the two
/// then fight by the one skill rule, hostile to each other alone, until one
/// has no more than `yield_health` left, the leader keeping the lead when
/// both fall together. A challenger who wins leads the faction, and, if
/// `allies_turn_hostile` holds, every faction at peace with it turns hostile
/// to it.
struct Duel {
  bool members_only = true;
  std::uint64_t yield_health = 0;
  bool allies_turn_hostile = true;
};

/// Why an army's lead may not challenge a faction's leader, the first that
/// holds in this order; `NONE` when it may.
enum class ChallengeRefusal : std::uint8_t {
  NONE,
  /// The faction does not pass its lead by duel.
  NO_SUCCESSION,
  /// The faction has no leader.
  NO_LEADER,
  /// The army has no living member to challenge.
  NO_CHALLENGER,
  /// The duel takes members only, and the army is of another faction.
  NOT_A_MEMBER,
  /// The army's lead is the leader.
  IS_LEADER,
  /// A duel for the faction's lead is being fought.
  DUEL_IN_PROGRESS,
};

/// A duel being fought for a faction's lead: the challenger and the leader,
/// as person indices.
struct Duellists {
  std::size_t challenger;
  std::size_t leader;
};

/// Who a faction's sects admit, as data, checked once a day: a living person
/// of no sect, serving in an army of the faction, at least `age` days old,
/// with a talent limit of at least `talent` and an attainment of at least
/// `attainment` in one discipline, whose home's centre lies within `reach`
/// cells of a sect's seat's centre along each axis, joins with `chance` per
/// mille a year. It joins a sect of the faction whose seat is held, with a
/// living elder and fewer members than its cap, drawn by the seed among
/// those. Its elders are the `elder_share` per mille of a sect's members of
/// the highest best attainment, the lower index among equals, and at least
/// one while the sect stands.
struct Admission {
  std::uint32_t age = 0;
  std::uint32_t talent = 0;
  std::uint32_t attainment = 0;
  std::int32_t reach = 0;
  std::uint32_t chance = 0;
  std::uint32_t elder_share = 0;
};

/// How a faction elects its leader: when its leader is dead or no one leads,
/// and every `term` days if `term` is not 0, the master of its sects with the
/// most favour leads, each elder of its sects favouring its own sect's
/// master; ties go to the higher best attainment, then the lower index.
struct Election {
  std::uint32_t term = 0;
};

/// A faction's leadership as a world is created with: the person leading
/// it, if anyone does, how the lead passes by duel, if it does, who its
/// sects admit, if they admit anyone, and how it elects its leader, if it
/// does.
struct Leadership {
  std::optional<std::size_t> leader;
  std::optional<Duel> succession;
  std::optional<Admission> admission;
  std::optional<Election> election;
};

}  // namespace murim
