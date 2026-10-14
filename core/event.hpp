#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "map.hpp"
#include "person.hpp"
#include "village.hpp"

namespace murim {

/// The most events a world keeps; the oldest gives way to the newest.
constexpr std::size_t EVENT_CAPACITY = 64;

/// Village `village` was founded by people of `faction`.
struct Founded {
  std::size_t village = 0;
  std::size_t faction = 0;
};

/// A party of `raider` was sent against village `village`, owned by `owner`.
struct Raided {
  std::size_t village = 0;
  std::size_t owner = 0;
  std::size_t raider = 0;
};

/// Village `village`, owned by `owner`, was brought to ruins by `destroyer`.
struct Ruined {
  std::size_t village = 0;
  std::size_t owner = 0;
  Destroyer destroyer{};
};

/// Sect `sect`, of `faction`, lost its last member.
struct SectFell {
  std::size_t sect = 0;
  std::size_t faction = 0;
};

/// How a faction's lead changed hands.
enum class ChosenBy : std::uint8_t { ELECTION, DUEL };

/// Person `leader` came to lead faction `faction`, chosen `by`.
struct LeaderChanged {
  std::size_t faction = 0;
  std::size_t leader = 0;
  ChosenBy by = ChosenBy::ELECTION;
};

/// Person `person` was born, as a child or a soul born again.
struct Born {
  std::size_t person = 0;
};

/// Person `person` died.
struct Died {
  std::size_t person = 0;
};

/// What happened in an event.
using Happening =
    std::variant<Founded, Raided, Ruined, SectFell, LeaderChanged, Born, Died>;

/// Something that happened in the world: the `sequence`-th event recorded,
/// counted from 0, on tick `tick`, at `cell`.
struct Event {
  std::uint64_t sequence = 0;
  std::uint64_t tick = 0;
  Cell cell{};
  Happening what;
};

/// How many of each record a world holds, which an event may name.
struct Records {
  std::size_t factions = 0;
  std::size_t people = 0;
  std::size_t villages = 0;
  std::size_t armies = 0;
  std::size_t sects = 0;
};

/// Returns whether every record `what` names is one of `held`.
[[nodiscard]] bool names_held(const Happening& what, const Records& held);

/// Returns whether person `other` is person `one`'s kin among `people`:
/// itself, of its family, its spouse, its parent or its child. Throws
/// std::out_of_range when either is no person.
[[nodiscard]] bool related(const std::vector<Person>& people, std::size_t one,
                           std::size_t other);

/// A person who hears of events: `person`, among the world's people, of
/// `faction` while it serves in an army, who hears of whatever happens
/// within `reach` cells along each axis of where it stands.
struct Listener {
  std::size_t person = 0;
  std::optional<std::size_t> faction;
  std::int32_t reach = 0;
};

/// Returns whether `event` reaches `listener` as a rumour, among `people`:
/// within its reach of where it stands, and wherever it happened, the birth
/// or death of its kin and what befell its faction, a village of it founded,
/// raided or ruined, or by it raided or ruined, a sect of it fallen, or its
/// lead changing hands.
[[nodiscard]] bool heard_by(const Event& event, const Listener& listener,
                            const std::vector<Person>& people);

/// Returns those of `events`, kept oldest first, that reach `listener` as
/// rumours among `people`, newest first.
[[nodiscard]] std::vector<Event> rumours(const std::vector<Event>& events,
                                         const Listener& listener,
                                         const std::vector<Person>& people);

}  // namespace murim
