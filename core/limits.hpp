#pragma once

#include <cstddef>
#include <cstdint>

#include "map.hpp"

namespace murim {

/// The most a chance per mille may be.
constexpr std::uint32_t PER_MILLE = 1'000;

/// The most cells a structure's footprint may span along either side.
constexpr std::int32_t MAX_FOOTPRINT = 256;

/// The largest range, radius and separation a world accepts, in sub-cell
/// units; it keeps the products of their squares within 64 bits.
constexpr std::int32_t MAX_REACH = std::int32_t{1} << 26;

/// The most living people a world holds unless its rules set another
/// capacity (ADR 0017).
constexpr std::size_t MAX_PEOPLE = 500;

/// The highest capacity for the living a world's rules may set: every
/// person's strike landing on one body in a tick still fits 64 bits.
constexpr std::size_t PEOPLE_CEILING = std::size_t{1} << 20;

/// The most health one person may have.
constexpr std::uint64_t MAX_HEALTH = std::uint64_t{1} << 43;

/// The most damage a skill may do to a body.
constexpr std::uint64_t MAX_BODY_DAMAGE = std::uint64_t{1} << 19;

/// The longest cooldown a skill may have, in ticks.
constexpr std::uint64_t MAX_COOLDOWN = std::uint64_t{1} << 32;

/// The most a talent's growth or limit may be drawn as, its widenings
/// included.
constexpr std::uint32_t MAX_TALENT = std::uint32_t{1} << 20;

/// The most a realm's might may multiply body damage by, per mille.
constexpr std::uint32_t MAX_MIGHT = 100 * PER_MILLE;

/// The longest lifespan a birth may draw, in days.
constexpr std::uint32_t MAX_LIFESPAN_DAYS = std::uint32_t{1} << 20;

/// The range a separation, range or radius must lie in.
[[nodiscard]] bool reachable(std::int32_t units);

/// Whether a footprint spans 1 to MAX_FOOTPRINT cells along each side.
[[nodiscard]] bool spans(Footprint footprint);

}  // namespace murim
