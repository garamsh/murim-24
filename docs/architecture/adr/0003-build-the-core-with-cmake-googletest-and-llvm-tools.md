# ADR 0003: Build the core with CMake, GoogleTest and the LLVM tools

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0002 makes the simulation core a C++ library with no engine dependency, loaded into Godot through GDExtension later. The core needs a build, a test framework, a formatter and a linter, run by the same names locally and in CI. Whatever builds it now has to reach Android and iOS later through godot-cpp, which is not built here. Versions below were read on 2026-10-06. Options weighed:

- **Build system.** CMake: godot-cpp ships CMake support with `cmake/android.cmake` and `cmake/ios.cmake` (godotengine/godot-cpp@507ed9d), so the core and the binding can share one build. SCons is godot-cpp's other build, but it would tie an engine-independent library to the engine's build tool. Meson and Bazel have no build files in godot-cpp at that commit.
- **Generator.** Ninja, a CMake generator that runs on macOS, Linux and Windows hosts. Make as the generator was rejected because the root `Makefile` already holds the entry points, and two layers of Make read as one.
- **C++ standard.** C++17, the standard godot-cpp builds itself with (`CXX_STANDARD 17` in `cmake/godotcpp.cmake` at godot-cpp@507ed9d) and the minimum GoogleTest 1.18.0 requires. C++20 was rejected: nothing in the core needs it yet.
- **Test framework.** GoogleTest 1.18.0, fetched by CMake from its release archive pinned by SHA-256, with tests registered through the `GoogleTest` module that ships with CMake. Catch2 v3.16.0 and doctest v2.5.3 were rejected: each ships its own registration script with the library, and neither offers the core anything GoogleTest lacks.
- **Formatter and linter.** clang-format and clang-tidy from LLVM 22.1.8. The newest LLVM in Homebrew is 23.1.2, but the newest clang-tidy wheel on PyPI is 22.1.8, and both tools come from the same release.
- **How CI installs them.** `actions/setup-python` and a requirements file that pins clang-format, clang-tidy, CMake and Ninja by version and SHA-256, moved by Dependabot's pip ecosystem. Rejected: the runner's preinstalled LLVM, whose version is not the local one; apt.llvm.org, which pins a major version and not a release; Homebrew on the runner, which installs whatever is current.

## Decision

The core is a CMake project at `core/`, generated with Ninja, written in C++17 and tested with GoogleTest 1.18.0. clang-format and clang-tidy from LLVM 22.1.8 check it. Locally they come from Homebrew, and in CI from PyPI wheels pinned by hash.

## Consequences

- The core builds and tests with no engine attached, and the binding can add `core/` to its own CMake build when it lands.
- Configuring `core/` downloads GoogleTest, so the first build needs network access.
- Homebrew and PyPI release separately, so local and CI agree on LLVM only while both are at 22.1.8. A Dependabot bump to one LLVM tool needs the other and `llvm@22` checked in the same pull request.
- A C++20 feature, or a newer LLVM, is a new ADR.

## Signs the decision was wrong

- godot-cpp's CMake build, the Android NDK or the iOS toolchain cannot build the core target without changing `core/CMakeLists.txt` per platform.
- `make format` or `make lint` passes locally and fails in CI, or the reverse, on the same commit.
- Dependabot keeps proposing clang-format and clang-tidy at different versions, and the bumps are left open.
- A feature the core needs is held back because it needs C++20.
