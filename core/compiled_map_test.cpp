#include "compiled_map.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {
namespace {

// An open map of 64 cells painted per 8, with places `places`.
MapSource open_map(std::vector<Place> places) {
  constexpr std::int32_t SIDE = 64;
  constexpr std::int32_t BLOCK = 8;
  constexpr std::size_t BLOCKS = std::size_t{8} * 8;
  MapSource source;
  source.side = SIDE;
  source.block = BLOCK;
  source.terrains.assign(BLOCKS, 0);
  source.footings.assign(BLOCKS, Footing::BUILDABLE);
  source.levels.assign(BLOCKS, 0);
  source.lands.assign(BLOCKS, 0);
  source.places = std::move(places);
  return source;
}

TEST(CompiledMap, AVillagePlaceAndASeatPlaceHashApartWhereTheirFieldsAgree) {
  // Read field by field, cells and indices alike, the two maps' places give
  // the same numbers, 10, 10, 1, 1, 40, 0, 1, with a seat's field one place
  // before a village's owner, so only what each place settles, a village or
  // a seat, tells them apart.
  const CompiledMap village_first{
      open_map({Place{Cell{10, 10}, VillagePlace{1, 1}},
                Place{Cell{40, 0}, SeatPlace{1}}})};
  const CompiledMap seat_first{
      open_map({Place{Cell{10, 10}, SeatPlace{1}},
                Place{Cell{1, 40}, VillagePlace{0, 1}}})};
  EXPECT_NE(village_first.hash(), seat_first.hash());
  // Control: the same places hash the same.
  EXPECT_EQ(village_first.hash(),
            CompiledMap{open_map({Place{Cell{10, 10}, VillagePlace{1, 1}},
                                  Place{Cell{40, 0}, SeatPlace{1}}})}
                .hash());
}

}  // namespace
}  // namespace murim
