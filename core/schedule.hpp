#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "map.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "village.hpp"

namespace murim {

/// What a person following a schedule is doing (ADR 0081): walking to its
/// spot, working at its workplace in a work band, at home in a home band, or
/// idle.
enum class Activity : std::uint8_t { WALKING, WORKING, AT_HOME, IDLE };

/// Ticks between a person's wanders to a new spot.
constexpr std::uint64_t WANDER_TICKS = 90;

/// Cells along each axis from its spot within which a person steps straight
/// to it rather than by its place's field.
constexpr std::int32_t STRAIGHT_REACH = 3;

/// Returns the hour of a day of `ticks_per_day` ticks that tick `tick` falls
/// in, 0 to 23.
[[nodiscard]] std::uint32_t hour_of(std::uint64_t tick,
                                    std::uint64_t ticks_per_day);

/// Returns the index of the band of `schedule` in force at hour `hour`: the
/// last beginning at or before it. `schedule` must begin at hour 0.
[[nodiscard]] std::size_t band_at(const std::vector<Band>& schedule,
                                  std::uint32_t hour);

/// Returns whether hour `hour` lies in the waking hours `waking`.
[[nodiscard]] bool awake(const Waking& waking, std::uint32_t hour);

/// Returns the households of `residents`, each in roster order, the
/// households in the roster order of their first members: a living married
/// couple, both among `residents`, with those of the husband's or wife's
/// children among `residents` who are unmarried or widowed; anyone else is a
/// household of one.
[[nodiscard]] std::vector<std::vector<std::size_t>> households(
    const std::vector<std::size_t>& residents,
    const std::vector<Person>& people);

/// Returns the house, by index into `housing`, each of `households` of
/// `sizes` people takes: the first, in order, whose housing left holds all
/// of it; none where no house does.
[[nodiscard]] std::vector<std::optional<std::size_t>> house(
    const std::vector<std::size_t>& sizes,
    const std::vector<std::uint32_t>& housing);

/// Returns the door of `structure` on `map`: of the passable cells beside its
/// footprint along an axis, the one nearest `centre`, the first row by row
/// among equals; none when no cell beside it is passable.
[[nodiscard]] std::optional<Cell> door(const Map& map,
                                       const Structure& structure, Cell centre);

/// Returns the draw for person `person` of a world of `seed` at counter
/// `counter` of stream `stream`: a counter-based hash, so it depends on
/// nothing else the world drew.
[[nodiscard]] std::uint64_t errand_draw(std::uint64_t seed, std::size_t person,
                                        std::uint64_t stream,
                                        std::uint64_t counter);

}  // namespace murim
