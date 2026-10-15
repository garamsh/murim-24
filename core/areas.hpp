#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "map.hpp"

namespace murim {

/// Cells along each side of an area: the square tiers are kept by (ADR
/// 0042) and a chart keeps seen cells in.
constexpr std::int32_t AREA_SIDE = 32;

/// How much of the rules an area runs (ADR 0042): every rule on every tick,
/// the same rules on one tick a stride, or the far rules. Ordered nearest
/// first, so the nearer of two tiers is the lesser.
enum class AreaTier : std::uint8_t { LIVE, STEPPED, FAR };

/// What a device's quality profile sets of the tiers (ADR 0048): the live
/// radius and the stepped radius, in areas, the most people the live tier
/// holds, and the stepped tier's stride, in ticks. A world is given one at
/// creation and on load; it is neither saved nor hashed.
struct Profile {
  std::int32_t live_radius;
  std::size_t live_cap;
  std::int32_t stepped_radius;
  std::uint32_t stride;
};

/// The quality profiles (ADR 0048). Standard's live cap is ADR 0077's, the
/// live scenario's 5,000 (#216); high's is held to standard's until it is
/// measured.
constexpr Profile LOW_PROFILE{1, 500, 3, 8};
constexpr Profile STANDARD_PROFILE{2, 5'000, 4, 8};
constexpr Profile HIGH_PROFILE{3, 5'000, 6, 4};

/// Where the people stand that the tiers are recomputed from: `focus`, the
/// areas holding a living person of a steered army, and `people`, how many
/// living people each area holds.
struct Census {
  std::vector<std::size_t> focus;
  std::vector<std::size_t> people;
};

/// The tier of each area of a map, row by row, kept from one recomputation to
/// the next for its hysteresis.
class Areas {
 public:
  /// Every area of a map of `size` cells, each live.
  explicit Areas(Footprint size);

  /// The areas of a map `columns` areas wide holding `tiers`, row by row, as
  /// a save holds them.
  Areas(std::int32_t columns, std::vector<AreaTier> tiers);

  [[nodiscard]] std::int32_t columns() const;
  [[nodiscard]] std::size_t count() const;
  [[nodiscard]] const std::vector<AreaTier>& tiers() const;

  /// Returns the index of the area holding `cell`, which lies on the map.
  [[nodiscard]] std::size_t area_of(Cell cell) const;

  /// Returns the tier of area `area`.
  [[nodiscard]] AreaTier tier(std::size_t area) const;

  /// Makes every area live.
  void make_live();

  /// Recomputes every area's tier by `profile` from `census`. An area's
  /// distance is counted in areas along each axis from the nearest focus
  /// area. The live tier takes the rings of
  /// areas nearest first, up to the live radius, while the people within
  /// them stay within the live cap; the focus ring is always live. The
  /// stepped tier reaches the stepped radius, and the far tier is the rest.
  /// An area moves to a nearer tier at once, and to a farther one only once
  /// its distance passes its tier's radius by one area. With no focus, every
  /// area is live.
  void recompute(const Census& census, const Profile& profile);

 private:
  std::int32_t columns_;
  std::int32_t rows_;
  std::vector<AreaTier> tiers_;
};

}  // namespace murim
