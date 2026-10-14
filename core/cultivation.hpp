#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "arts.hpp"
#include "life.hpp"
#include "person.hpp"

namespace murim {

/// How a skill is learned: from a teacher or a manual, or by comprehension,
/// which only a comprehensible skill allows.
enum class Lesson : std::uint8_t { TAUGHT, COMPREHENDED };

/// Returns how many of `life`'s realms person `person`'s qi capacity reaches,
/// its realm rank, 0 for none.
[[nodiscard]] std::uint32_t rank_of(const Person& person, const Life& life);

/// Returns person `person`'s nature, that of the art it learned first, its
/// heart method, or nothing while it knows no art.
[[nodiscard]] std::optional<Nature> nature_of(const Person& person,
                                              const Life& life);

/// Returns the art person `person` learned first, its heart method, or
/// nothing while it knows no art.
[[nodiscard]] std::optional<std::size_t> heart_of(const Person& person);

/// Returns whether skill `skill` of art `art` lies within person `person`'s
/// realm: the skill's rank is no higher than the person's. Throws
/// std::out_of_range when `life` has no such art or skill.
[[nodiscard]] bool within_realm(const Person& person, std::size_t art,
                                std::size_t skill, const Life& life);

/// Returns how far art `art`'s nature agrees with person `person`'s: the
/// product, per mille, of `life`'s learning's ease, or its potency when
/// `potency`, for each axis on which the two differ; PER_MILLE while the
/// person knows no art.
[[nodiscard]] std::uint64_t agreement(const Person& person, std::size_t art,
                                      bool potency, const Life& life);

/// Returns the first skill of art `art` person `person` lacks, of rank
/// `rank` or below, that it may learn `how`, or nothing.
[[nodiscard]] std::optional<std::size_t> lesson(const Person& person,
                                                std::size_t art, Lesson how,
                                                std::uint32_t rank,
                                                const Life& life);

/// Grows person `person`'s qi capacity by its cultivation's growth, up to its
/// limit, and never lowers it.
void cultivate(Person& person);

/// Recovers person `person`'s qi by `cultivation`'s recovery per mille of its
/// capacity, rounded up, up to its capacity.
void recover(Person& person, const Cultivation& cultivation);

/// Returns the arts person `person` knows a skill of, each once, in the order
/// it first learned one.
[[nodiscard]] std::vector<std::size_t> practised(const Person& person);

/// Returns the chance, per mille, that person `person` comprehends a skill of
/// art `art` on a realm gained: `life`'s learning's comprehension, scaled by
/// its talent limit in the art's discipline over that discipline's mean
/// limit, and by how far the art agrees with it.
[[nodiscard]] std::uint64_t comprehension_chance(const Person& person,
                                                 std::size_t art,
                                                 const Life& life);

/// Returns whether person `reader` may read a manual of art `art`: the art's
/// nature does not wholly disagree with its own, it has the rank for a skill
/// of the art it lacks, and it holds the qi the reading costs it.
[[nodiscard]] bool may_read(const Person& reader, std::size_t art,
                            const Life& life);

/// Returns the qi person `reader` spends to read a manual of art `art`:
/// `life`'s reading cost over how far the art agrees with it. Throws
/// std::domain_error when the art wholly disagrees with it.
[[nodiscard]] std::uint32_t reading_cost(const Person& reader, std::size_t art,
                                         const Life& life);

/// Returns the chance, per mille a year, that person `pupil` learns a skill
/// of art `art` from a sect whose highest-ranked elder is of rank `teacher`:
/// `life`'s teaching for each of the teacher's ranks, scaled by how far the
/// art agrees with the pupil.
[[nodiscard]] std::uint64_t teaching_chance(const Person& pupil,
                                            std::size_t art,
                                            std::uint32_t teacher,
                                            const Life& life);

}  // namespace murim
