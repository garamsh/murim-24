#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "person.hpp"

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

/// A duel fought for faction `faction`'s lead, under its rule.
struct Fight {
  std::size_t faction = 0;
  std::size_t challenger = 0;
  std::size_t leader = 0;
  Duel rule;
};

/// An army as a challenge reads it: the faction it fights for, and its lead,
/// if it has a living member.
struct Challenging {
  std::size_t faction = 0;
  std::optional<std::size_t> lead;
};

/// A duel the challenger won: the faction it now leads, and whether the
/// factions at peace with it turn hostile to it.
struct Won {
  std::size_t faction;
  std::size_t challenger;
  bool allies_turn_hostile;
};

/// Every faction's leadership and the duels fought for their leads: the one
/// place either is kept. It decides who may challenge a faction's leader, how
/// a duel ends and whom it leaves leading, and when a faction elects.
class Leaders {
 public:
  /// No factions and no duels.
  Leaders() = default;

  /// `leadership` for `factions` factions, those it leaves out led by no one,
  /// and no duel. Throws std::invalid_argument when it names more factions
  /// than `factions` or a leader who is none of `people`.
  Leaders(std::vector<Leadership> leadership, std::size_t factions,
          const std::vector<Person>& people);

  /// Each faction's leadership and the duels fought, as a save holds them,
  /// unchecked until kept() is asked.
  Leaders(std::vector<Leadership> factions, std::vector<Fight> duels);

  /// Returns every faction's leadership, by faction.
  [[nodiscard]] const std::vector<Leadership>& factions() const;

  /// Returns the duels being fought, in the order they began.
  [[nodiscard]] const std::vector<Fight>& duels() const;

  /// Returns faction `faction`'s leadership. Throws std::out_of_range when
  /// there is no such faction.
  [[nodiscard]] const Leadership& of(std::size_t faction) const;

  /// Returns whether person `person` leads a faction.
  [[nodiscard]] bool leads(std::size_t person) const;

  /// Returns why `challenging`'s lead may not challenge faction `faction`'s
  /// leader, the first reason that holds in ChallengeRefusal's order, or
  /// ChallengeRefusal::NONE when it may.
  [[nodiscard]] ChallengeRefusal refusal(Challenging challenging,
                                         std::size_t faction) const;

  /// Starts a duel between `challenging`'s lead and faction `faction`'s
  /// leader, under the faction's succession. Throws std::invalid_argument
  /// when refusal() names a reason.
  void challenge(Challenging challenging, std::size_t faction);

  /// Returns the duel being fought for faction `faction`'s lead, if one is.
  [[nodiscard]] std::optional<Duellists> duel(std::size_t faction) const;

  /// Returns whether persons `one` and `other` fight a duel with each other.
  [[nodiscard]] bool duelling(std::size_t one, std::size_t other) const;

  /// Returns whether person `person` fights a duel.
  [[nodiscard]] bool in_duel(std::size_t person) const;

  /// Ends every duel in which a duellist has no more than its rule's yield
  /// health left among `people`: the leader keeps the lead unless only the
  /// challenger stands, who then leads. Returns the duels challengers won,
  /// in the order they began.
  std::vector<Won> settle(const std::vector<Person>& people);

  /// Returns whether faction `faction` elects on day `day`: it has an
  /// election, and its leader is dead among `people` or missing, or `day`
  /// completes its term.
  [[nodiscard]] bool elects_on(std::size_t faction,
                               const std::vector<Person>& people,
                               std::uint64_t day) const;

  /// Makes `leader` faction `faction`'s leader. Returns whether its leader
  /// changed.
  bool lead(std::size_t faction, std::optional<std::size_t> leader);

  /// Whether the saved-state invariant holds: a leadership for each of
  /// `factions` factions, its leader one of `people` and its admission within
  /// the limits a world is created with, and every duel's faction and
  /// duellists ones that exist.
  [[nodiscard]] bool kept(std::size_t factions,
                          const std::vector<Person>& people) const;

 private:
  std::vector<Leadership> factions_;
  std::vector<Fight> duels_;
};

}  // namespace murim
