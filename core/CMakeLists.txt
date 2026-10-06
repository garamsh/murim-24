cmake_minimum_required(VERSION 3.16...4.4)
project(murim_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(murim_core flow_field.cpp m1_scenario.cpp map.cpp random.cpp spatial_hash.cpp world.cpp)
target_include_directories(murim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(murim_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

add_executable(murim_core_bench bench.cpp)
target_link_libraries(murim_core_bench PRIVATE murim_core)
target_compile_options(murim_core_bench PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

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

add_executable(murim_core_test spatial_hash_test.cpp world_test.cpp)
target_link_libraries(murim_core_test PRIVATE murim_core GTest::gtest_main)
target_compile_options(murim_core_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
gtest_discover_tests(murim_core_test)
