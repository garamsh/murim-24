cmake_minimum_required(VERSION 3.16...4.4)
project(murim_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(murim_core area_fields.cpp areas.cpp attack.cpp chart.cpp coarse_graph.cpp compiled_map.cpp content.cpp cultivation.cpp diplomacy.cpp event.cpp flow_field.cpp growth.cpp layout.cpp leadership.cpp life.cpp limits.cpp live_scenario.cpp m1_scenario.cpp map.cpp occupation.cpp progress.cpp random.cpp region.cpp save.cpp schedule.cpp shipped_graph.cpp skill.cpp spatial_hash.cpp talent.cpp trade.cpp village.cpp village_fields.cpp world.cpp)
target_include_directories(murim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(murim_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

# The packs the game's content is loaded from, read by the tools and tests
# through the JSON adapter.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../packs_json ${CMAKE_CURRENT_BINARY_DIR}/packs_json)
set(MURIM_PACKS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packs")

add_executable(murim_core_bench bench.cpp)
target_link_libraries(murim_core_bench PRIVATE murim_core)
target_compile_options(murim_core_bench PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

add_executable(murim_core_sweep sweep.cpp)
target_link_libraries(murim_core_sweep PRIVATE murim_core murim_packs_json)
target_compile_definitions(murim_core_sweep PRIVATE MURIM_PACKS_DIR="${MURIM_PACKS_DIR}")
target_compile_options(murim_core_sweep PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_executable(murim_core_test area_fields_test.cpp areas_test.cpp attack_test.cpp chart_test.cpp chronicle_test.cpp coarse_graph_test.cpp compiled_map_test.cpp content_test.cpp cultivation_test.cpp diplomacy_test.cpp event_test.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../packs_json/json_packs_test.cpp flow_field_test.cpp layout_test.cpp leadership_test.cpp occupation_test.cpp progress_test.cpp region_test.cpp save_test.cpp schedule_test.cpp shipped_graph_test.cpp spatial_hash_test.cpp village_fields_test.cpp village_test.cpp world_test.cpp)
target_link_libraries(murim_core_test PRIVATE murim_core murim_packs_json GTest::gtest_main)
target_compile_definitions(murim_core_test PRIVATE MURIM_PACKS_DIR="${MURIM_PACKS_DIR}")
target_compile_options(murim_core_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
gtest_discover_tests(murim_core_test)
