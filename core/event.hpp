#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

#include "map.hpp"
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

}  // namespace murim
