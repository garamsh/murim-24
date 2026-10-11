# The JSON adapter of the core's PackSource port (ADR 0049): it parses pack
# documents with nlohmann/json, pinned by hash, into the core's value tree.
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_MakeAvailable(json)

add_library(murim_packs_json json_packs.cpp)
target_include_directories(murim_packs_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(murim_packs_json PUBLIC murim_core PRIVATE nlohmann_json::nlohmann_json)
target_compile_options(murim_packs_json PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
