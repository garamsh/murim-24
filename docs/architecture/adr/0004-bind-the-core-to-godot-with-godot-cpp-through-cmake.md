# ADR 0004: Bind the core to Godot with godot-cpp 10 through CMake

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0002 loads the C++ core into Godot 4.7 through GDExtension, and ADR 0003 builds the core with CMake so that godot-cpp's CMake build can take it in. The binding needs a godot-cpp release and a way to pin it, a build path that links `core/` unchanged, a GDScript test that runs headless locally and in CI, and a pinned Godot in CI, which has none. Versions below were read on 2026-10-06. Options weighed:

- **Binding release.** godot-cpp 10.0.0 (tag `10.0.0-stable`, godotengine/godot-cpp@507ed9d), the first release versioned apart from Godot; it ships `gdextension/extension_api-4-7.json` and selects it with `GODOTCPP_API_VERSION=4.7`. Rejected: the `godot-4.5-stable` tag, the newest per-Godot release, which predates Godot 4.7's API; generating the API file from the local Godot with `--dump-extension-api`, which ties the build to whichever Godot binary configures it.
- **Fetch and pin.** CMake `FetchContent` from the release archive pinned by SHA-256, as `core/CMakeLists.txt` fetches GoogleTest. Rejected: a git submodule, a second pinning mechanism beside `FetchContent` that every checkout has to initialise; a vendored copy, which puts godot-cpp's sources in the project's diffs.
- **Build path.** `extension/CMakeLists.txt` is its own CMake project, configured into `build/extension/`: it adds `core/` as a subdirectory, excluded from its default targets, and turns on position-independent code so the core's static library links into the shared one. Rejected: one tree with the extension as the top project, which would move the core's tests into another project's build; godot-cpp's SCons build, rejected for the core in ADR 0003.
- **Scenario format.** `WorldAdapter.create(seed, map_rows, armies, separation)` mirrors the core's constructor: map rows as a `PackedStringArray`, and each army as a `Dictionary` with `start` and `objective` as `Vector2i` cells and `count` and `strength` as ints. Rejected: an army class registered with Godot, a second class to bind for four fields that cross once, at creation.
- **Refusals.** The core refuses invalid input with `std::invalid_argument`, and the adapter's conversion to the core's integer types refuses the same way; `create` catches that one type, reports it with `push_error` and returns null, so no exception crosses into Godot. Exceptions are enabled through godot-cpp's `GODOTCPP_DISABLE_EXCEPTIONS=OFF`, the option its CMake build documents (Godot 4.7 documentation, `tutorials/scripting/cpp/build_system/cmake.html`); with the default `ON` it compiles godot-cpp with `-fno-exceptions`, and on MSVC defines `_HAS_EXCEPTIONS=0` for every target linking it. Rejected: checking the core's preconditions in the adapter before calling it, which copies a rule out of the core.
- **Reference hash.** `world_hash`, a program in `extension/` linking only the core, runs a scenario given on its command line and prints the core's hash; the Godot test passes it the scenario it gives the adapter. Rejected: a hash value copied into the test, which goes stale whenever the core's state changes.
- **GDScript test framework.** None: `run_tests.gd` loads each `*_test.gd`, calls its `run()`, and exits non-zero unless all loaded and passed. Godot 4.7.2 exits 0 from `--script` when the script fails to parse, so a test naming a class from an extension that did not load would pass if run directly; the runner counts that load as a failure. Rejected: GUT 9.7.1 and gdUnit4 v6.2.1, each an editor plugin vendored under `addons/` for a suite of one test.
- **Registering the extension.** `make build` writes `.godot/extension_list.cfg`, the list of extensions Godot loads at startup, naming every committed `*.gdextension`. Rejected: `godot --headless --import`, which registers it by discovery: on Godot 4.7.2, with no `.godot/extension_list.cfg`, it crashed on exit with signal 11 in three of three runs on macOS arm64 and in CI on Linux x86_64, and exited 0 once the list existed; godotengine/godot#111645 tracks a headless import crash with GDExtensions. Shipping a class reference through godot-cpp's `target_doc_sources`, the workaround reported there, still crashed.
- **Godot in CI.** The Linux x86_64 archive of Godot 4.7.2, checked against the SHA-512 in that release's `SHA512-SUMS.txt`. Rejected: chickensoft-games/setup-godot v2.4.3, which downloads by version from the release URL and checks no hash, while the godotengine/godot `4.7.2-stable` release is not marked immutable.
- **Lint.** clang-format checks `extension/` with the core's style. clang-tidy does not read it: `GDCLASS` expands into code that `core/.clang-tidy` rejects (`cppcoreguidelines-special-member-functions`, `misc-const-correctness`, `modernize-use-auto`), and passing would take suppressions.

## Decision

The GDExtension library is built from `extension/` with godot-cpp 10.0.0 targeting the Godot 4.7 API, fetched by CMake from its release archive pinned by SHA-256, and it links the core as a CMake subdirectory. `WorldAdapter.create` takes the core constructor's inputs as Godot types and turns the core's `std::invalid_argument` into `push_error` and a null return, with exceptions enabled through `GODOTCPP_DISABLE_EXCEPTIONS=OFF`. `make build` registers it in `.godot/extension_list.cfg`. GDScript tests run under Godot 4.7.2 headless through `run_tests.gd`, with no test framework, and CI installs that Godot from its release archive pinned by SHA-512. The godot-cpp and Godot pins are moved by hand, together, in a pull request that also moves the versions `docs/convention/stack-godot.md` is checked against.

## Consequences

- `make build` builds the core twice, once in each tree, and the first build downloads godot-cpp and GoogleTest.
- No bot proposes godot-cpp or Godot bumps; a new Godot release stays unadopted until someone moves the pins.
- A GDScript test is plain GDScript with a pass result, and a test that needs fixtures or doubles has none to reach for.
- The GDExtension C++ is formatted but not linted.
- godot-cpp is compiled with exceptions, which the comment beside its `-fno-exceptions` flag puts at about 20% of binary size.
- The build writes into `.godot/`, Godot's own cache, a file whose format Godot does not document.
- Android and iOS builds go through godot-cpp's `cmake/android.cmake` and `cmake/ios.cmake`, which are not built here.

## Signs the decision was wrong

- `extension/CMakeLists.txt` needs per-platform branches to build for Android or iOS.
- A failure other than `std::invalid_argument` needs handling at the adapter, or an exception reaches Godot.
- Rendering needs more of the world than `create`, `tick` and `hash`, and the Dictionary scenario format no longer carries what a scenario holds.
- `run_tests.gd` grows fixtures, assertions or reporting a framework already ships.
- Godot moves past 4.7 while the pins stay, or a godot-cpp bump breaks the build and is left open.
- A defect clang-tidy would have caught reaches `extension/`.
- The second core build dominates `make ci`'s time.
- A Godot release stops reading `.godot/extension_list.cfg` at startup, or changes its format, and the GDScript tests fail on a clean checkout.
