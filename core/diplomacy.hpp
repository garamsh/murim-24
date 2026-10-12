#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace murim {

/// How the leaders of factions no driver steers declare war and make peace,
/// as data, checked once a day, one rule for every such leader. Toward each
/// other faction with living people, in index order, a leader whose faction
/// is allied with it or neutral toward it declares war with `war_chance` per
/// mille a year while its own living people number at least `war_margin`
/// per mille of the other's; and one whose faction is at a war declared with
/// it, while its own living people are fewer, makes peace with
/// `peace_chance` per mille a year. Hostility by the rules alone it leaves
/// as it is. Every declaration, the driver's included, holds for
/// `hold_days` days before another may change it.
struct Diplomacy {
  std::uint32_t war_margin = 0;
  std::uint32_t war_chance = 0;
  std::uint32_t peace_chance = 0;
  std::uint32_t hold_days = 0;
};

/// What a faction's leader declares toward another faction: war, so that
/// each attacks the other whatever their stances, or peace, so that neither
/// does.
enum class Declaration : std::uint8_t { WAR, PEACE };

/// The declaration standing between two factions: what it is, the faction
/// whose leader made it, and the tick it was made on.
struct Declared {
  Declaration what = Declaration::WAR;
  std::size_t by = 0;
  std::uint64_t tick = 0;
};

/// How one faction stands toward another, the one read of it: its own;
/// allied, neither hostile to the other by the rules' hostility; neutral,
/// at a peace declared; hostile, either hostile to the other by the rules'
/// hostility; or at war declared.
enum class Relation : std::uint8_t { OWN, ALLY, NEUTRAL, HOSTILE, AT_WAR };

/// Why a person may not declare war on, or make peace with, a faction, the
/// first that holds in this order; `NONE` when it may.
enum class DeclarationRefusal : std::uint8_t {
  NONE,
  /// The person is dead.
  DEAD,
  /// The person does not lead the faction of the army it serves in.
  NOT_THE_LEADER,
  /// The faction is the person's own.
  SAME_FACTION,
  /// War is declared on a faction already at war with the person's.
  ALREADY_AT_WAR,
  /// Peace is made with a faction neither hostile to nor at war with the
  /// person's.
  ALREADY_AT_PEACE,
  /// The two factions' last declaration was made fewer than the diplomacy's
  /// `hold_days` days ago.
  TOO_SOON,
};

/// The declarations standing between factions: one place for each two of
/// `factions`, so a declaration holds for both whichever names it first.
class Declarations {
 public:
  Declarations() = default;

  /// No declaration between any two of `factions`.
  explicit Declarations(std::size_t factions);

  /// The declarations `slots` give for `factions`, as a save holds them,
  /// unchecked until kept() is asked.
  Declarations(std::size_t factions,
               std::vector<std::optional<Declared>> slots);

  /// Returns the declaration standing between `faction` and `other`, if one
  /// does. Throws std::out_of_range when either is no faction.
  [[nodiscard]] const std::optional<Declared>& between(std::size_t faction,
                                                       std::size_t other) const;

  /// Makes `declared` the declaration standing between `faction` and
  /// `other`. Throws std::out_of_range when either is no faction.
  void declare(std::size_t faction, std::size_t other, Declared declared);

  /// Whether the declarations are as declare() keeps them: one place for
  /// each two factions, each standing under the lower's row, made by one of
  /// them no later than tick `now`.
  [[nodiscard]] bool kept(std::uint64_t now) const;

  /// Every place, by the lower faction's index times the factions plus the
  /// higher's; the rest stay empty.
  [[nodiscard]] const std::vector<std::optional<Declared>>& slots() const;

 private:
  [[nodiscard]] std::size_t pair_of(std::size_t faction,
                                    std::size_t other) const;

  std::size_t factions_ = 0;
  std::vector<std::optional<Declared>> slots_;
};

}  // namespace murim
