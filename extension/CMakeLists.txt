cmake_minimum_required(VERSION 3.16...4.4)
project(murim_extension LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The core links into a shared library, so its static archive needs position-independent code.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(../core core EXCLUDE_FROM_ALL)

include(FetchContent)
FetchContent_Declare(
  godot-cpp
  URL https://github.com/godotengine/godot-cpp/archive/refs/tags/10.0.0-stable.tar.gz
  URL_HASH SHA256=ebbf64f0f8f8b8b66bc4bc4d3797db97f95b19cf0dfb5b3f8cf56683d9df7db4
)
set(GODOTCPP_API_VERSION 4.7 CACHE STRING "" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
# The adapter catches the core's std::invalid_argument, so exceptions stay enabled.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(godot-cpp)

add_library(murim_extension SHARED body_view.cpp extension.cpp file_saves.cpp world_adapter.cpp)
target_link_libraries(murim_extension PRIVATE murim_core murim_packs_json godot-cpp)
target_compile_options(murim_extension PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
set_target_properties(murim_extension PROPERTIES PREFIX "lib" OUTPUT_NAME "murim${GODOTCPP_SUFFIX}")

add_executable(world_hash world_hash.cpp)
target_link_libraries(world_hash PRIVATE murim_core)
target_compile_options(world_hash PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
