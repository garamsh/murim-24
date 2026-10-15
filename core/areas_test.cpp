#include "areas.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

// A map of 8 by 8 areas, and the index of the area at `column` and `row`.
constexpr std::int32_t MAP_AREAS = 8;
constexpr Footprint MAP{MAP_AREAS * AREA_SIDE, MAP_AREAS * AREA_SIDE};
constexpr std::int32_t FAR_CORNER = MAP_AREAS - 1;

std::size_t area_at(std::int32_t column, std::int32_t row) {
  return (static_cast<std::size_t>(row) * MAP_AREAS) +
         static_cast<std::size_t>(column);
}

// No one stands anywhere.
std::vector<std::size_t> nobody() {
  return std::vector<std::size_t>(
      static_cast<std::size_t>(MAP_AREAS) * MAP_AREAS, 0);
}

TEST(Areas, EveryAreaStartsLiveSoTheFirstTiersKeepOneAreaPastTheLiveRadius) {
  Areas areas{MAP};
  areas.recompute(Census{{area_at(0, 0)}, nobody()}, STANDARD_PROFILE);
  EXPECT_EQ(areas.tier(area_at(3, 0)), AreaTier::LIVE);
  EXPECT_EQ(areas.tier(area_at(4, 0)), AreaTier::STEPPED);
  EXPECT_EQ(areas.tier(area_at(5, 0)), AreaTier::FAR);
}

TEST(Areas, TiersReachTheirRadiiFromTheFocusAlongEachAxis) {
  // The focus comes from the far corner, so no area near it was live.
  Areas areas{MAP};
  areas.recompute(Census{{area_at(FAR_CORNER, FAR_CORNER)}, nobody()},
                  STANDARD_PROFILE);
  areas.recompute(Census{{area_at(0, 0)}, nobody()}, STANDARD_PROFILE);
  // Standard: live within 2 areas, stepped within 4, far beyond.
  EXPECT_EQ(areas.tier(area_at(2, 2)), AreaTier::LIVE);
  EXPECT_EQ(areas.tier(area_at(3, 0)), AreaTier::STEPPED);
  EXPECT_EQ(areas.tier(area_at(4, 4)), AreaTier::STEPPED);
  EXPECT_EQ(areas.tier(area_at(5, 1)), AreaTier::FAR);
  // Control: with no focus every area is live.
  areas.recompute(Census{{}, nobody()}, STANDARD_PROFILE);
  EXPECT_EQ(areas.tier(area_at(5, 1)), AreaTier::LIVE);
  EXPECT_EQ(areas.tier(area_at(FAR_CORNER, FAR_CORNER)), AreaTier::LIVE);
}

TEST(Areas, AnAreaMovesFartherOnlyOnceItPassesItsRadiusByOneArea) {
  Areas areas{MAP};
  areas.recompute(Census{{area_at(0, 0)}, nobody()}, STANDARD_PROFILE);
  ASSERT_EQ(areas.tier(area_at(2, 0)), AreaTier::LIVE);
  ASSERT_EQ(areas.tier(area_at(4, 0)), AreaTier::STEPPED);
  // The focus moves three areas south: the live area, now 3 areas away, one
  // past the live radius, stays live, and the stepped one, now 4 away and
  // within the stepped radius, stays stepped.
  areas.recompute(Census{{area_at(0, 3)}, nobody()}, STANDARD_PROFILE);
  EXPECT_EQ(areas.tier(area_at(2, 0)), AreaTier::LIVE);
  EXPECT_EQ(areas.tier(area_at(4, 0)), AreaTier::STEPPED);
  // Control: an area that was not live, at the same 3 areas, is stepped.
  Areas fresh{MAP};
  fresh.recompute(Census{{area_at(FAR_CORNER, FAR_CORNER)}, nobody()},
                  STANDARD_PROFILE);
  fresh.recompute(Census{{area_at(0, 3)}, nobody()}, STANDARD_PROFILE);
  EXPECT_EQ(fresh.tier(area_at(2, 0)), AreaTier::STEPPED);
  // Two past the radius, it moves farther.
  areas.recompute(Census{{area_at(0, 4)}, nobody()}, STANDARD_PROFILE);
  EXPECT_EQ(areas.tier(area_at(2, 0)), AreaTier::STEPPED);
  // And it moves nearer at once.
  areas.recompute(Census{{area_at(0, 0)}, nobody()}, STANDARD_PROFILE);
  EXPECT_EQ(areas.tier(area_at(2, 0)), AreaTier::LIVE);
}

TEST(Areas, TheLiveTierTakesOnlyTheRingsItsCapHolds) {
  // The second ring holds more people than the low profile's cap.
  std::vector<std::size_t> people = nobody();
  people.at(area_at(1, 0)) = LOW_PROFILE.live_cap;
  people.at(area_at(2, 0)) = 1;
  Profile wide = LOW_PROFILE;
  wide.live_radius = 2;
  // From the far corner, so the rings near the focus were not live.
  Areas areas{MAP};
  areas.recompute(Census{{area_at(FAR_CORNER, FAR_CORNER)}, people}, wide);
  areas.recompute(Census{{area_at(0, 0)}, people}, wide);
  EXPECT_EQ(areas.tier(area_at(1, 1)), AreaTier::LIVE);
  EXPECT_EQ(areas.tier(area_at(2, 2)), AreaTier::STEPPED);
  // Control: one fewer in the second ring, it is live too.
  people.at(area_at(2, 0)) = 0;
  Areas held{MAP};
  held.recompute(Census{{area_at(FAR_CORNER, FAR_CORNER)}, people}, wide);
  held.recompute(Census{{area_at(0, 0)}, people}, wide);
  EXPECT_EQ(held.tier(area_at(2, 2)), AreaTier::LIVE);
}

}  // namespace
}  // namespace murim
