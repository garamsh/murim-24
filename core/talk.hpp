#pragma once

#include <cstdint>

namespace murim {

/// Why a person may not talk to another, the first that holds in this order;
/// `NONE` when it may.
enum class TalkRefusal : std::uint8_t {
  NONE,
  /// Either person is dead.
  DEAD,
  /// The two are the same person.
  SELF,
  /// The other stands beyond the rules' talk reach of the person.
  OUT_OF_REACH,
  /// The two serve factions hostile to each other or at war, as relation()
  /// reads them.
  HOSTILE,
};

/// What a person offers one who talks to it: the news it has heard, its
/// rumours(), and the wares it sells.
enum class Offer : std::uint8_t { NEWS, TRADE };

}  // namespace murim
