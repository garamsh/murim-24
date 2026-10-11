#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace murim {

/// A member a sect is created with: a person by index, and the attainment its
/// discipline of highest talent limit starts at, up to that limit.
struct Founder {
  std::size_t person = 0;
  std::uint32_t attainment = 0;
};

/// A sect as a world is created with: its name, its faction, its seat
/// village by index, and its founding members.
struct SectSpec {
  std::string name;
  std::size_t faction = 0;
  std::size_t seat = 0;
  std::vector<Founder> founders;
};

/// Why a person may not join the sect it applies to, the first that holds in
/// this order; `NONE` when it may. The daily admission reads the same
/// thresholds.
enum class ApplicationRefusal : std::uint8_t {
  NONE,
  /// The person is dead.
  DEAD,
  /// The person belongs to a sect already.
  IN_A_SECT,
  /// The person has no home.
  NO_HOME,
  /// The person serves in no army of the sect's faction, or the faction's
  /// sects admit no one.
  NOT_OF_FACTION,
  /// The person is younger than the admission's age.
  TOO_YOUNG,
  /// No discipline of the person's meets both the admission's talent and its
  /// attainment.
  UNPROVEN,
  /// The person's home lies beyond the admission's reach of the seat.
  TOO_FAR,
  /// The sect's seat is not held.
  SEAT_LOST,
  /// The sect has no living elder to admit anyone.
  NO_ELDER,
  /// The sect holds as many members as its cap.
  FULL,
};

}  // namespace murim
