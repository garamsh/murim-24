#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

namespace murim {

/// The most steps of people's progress a world keeps; the oldest gives way to
/// the newest.
constexpr std::size_t PROGRESS_CAPACITY = 32;

/// How a person came by a skill: by comprehension on a realm gained, by
/// reading a manual lying on a cell, or by its sect's teaching.
enum class LearnedBy : std::uint8_t { COMPREHENSION, MANUAL, TEACHING };

/// Person `person` learned skill `skill` of art `art`, `by`.
struct Learned {
  std::size_t person = 0;
  std::size_t art = 0;
  std::size_t skill = 0;
  LearnedBy by = LearnedBy::COMPREHENSION;
};

/// Person `person` reached the realm of rank `rank`, counted from 1.
struct RealmGained {
  std::size_t person = 0;
  std::size_t rank = 0;
};

/// A step a person took in the martial arts.
using Advance = std::variant<Learned, RealmGained>;

/// A step of progress: the `sequence`-th recorded, counted from 0, on tick
/// `tick`.
struct Progress {
  std::uint64_t sequence = 0;
  std::uint64_t tick = 0;
  Advance what;
};

}  // namespace murim
