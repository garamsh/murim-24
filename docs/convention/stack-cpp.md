# C++ stack

The concrete C++ form of naming, file layout, tests, comments and the check entry points, for the code under `core/`, `packs_json/` and `extension/`.

Checked against C++17 (ISO/IEC 14882:2017), CMake 4.4.4, Ninja 1.13.2, LLVM 22.1.8 (clang-format, clang-tidy), GoogleTest 1.18.0 and godot-cpp 10.0.0.

## Naming

| Kind | Form | Example |
|---|---|---|
| Class, struct, enum | `PascalCase` | `World`, `StateHash` |
| Function, variable, parameter | `snake_case` | `tick`, `tick_count` |
| Private data member | `snake_case_` | `rng_state_` |
| `constexpr` constant | `UPPER_SNAKE_CASE` | `SPLITMIX_INCREMENT` |
| Namespace | `snake_case` | `murim` |
| File | `snake_case`, header `.hpp`, source `.cpp`, both named for the unit | `world.hpp`, `world.cpp` |

- `readability-identifier-naming` in `core/.clang-tidy` enforces the identifier forms, so `make lint` fails on a name in the wrong form.
- Code under `core/` lives in the `murim` namespace, code under `packs_json/` (the JSON adapter for content packs, ADR 0049) in `murim_packs`, and code under `extension/` in `murim_godot`; a helper used by one source file goes in an unnamed namespace inside it.
- A class registered with Godot is named as GDScript sees it, since godot-cpp's `GDCLASS` registers the C++ name.
- `_bind_methods` keeps the name godot-cpp requires.

## File layout

- `core/CMakeLists.txt` declares the core library, its tests and their dependencies.
- `extension/CMakeLists.txt` declares the GDExtension library and `world_hash`; it adds `core/` as a subdirectory, and CMake fetches godot-cpp at configure time from the release archive pinned by hash there.
- A header opens with `#pragma once`.
- A source file includes its own header first, then a blank line, then every other include; clang-format orders each block.
- `core/.clang-format` and `core/.clang-tidy` are the formatter and linter configuration for both directories.
- Build output goes to `build/` at the repository root, which `.gitignore` excludes.

## Tests

- Framework: GoogleTest, which CMake fetches at configure time from the release archive pinned by hash in `core/CMakeLists.txt`.
- A unit test is `<unit>_test.cpp` next to `<unit>.cpp`, built as a test executable in `core/CMakeLists.txt` and registered with `gtest_discover_tests`.
- A test is `TEST(<Unit>, <BehaviourInPascalCase>)`: GoogleTest 1.18.0's primer says suite and test names should not contain underscores.
- Assertions read the public API's return values, such as `World::hash()`.

## Comments

- Doc comment: `///` on the lines above the declaration in the header.
- Every other comment: `//`. No `/* */` blocks.
- `// NOLINTNEXTLINE(<checks>)` goes only above a line where a godot-cpp macro, such as `GDCLASS`, expands into code the checks reject, and names each check.

## Entry points

| Name | What it runs |
|---|---|
| `make build` | CMake configures `core/` into `build/` with the Ninja generator, then builds every target, the tests included; then configures `extension/` into `build/extension/` the same way, as a Release build with `SDKROOT` set to the macOS SDK, and builds its default targets. |
| `make lint` | `check-contents-list.sh` over every Markdown file, then `clang-tidy -p build` over every `core/*.cpp`, then `clang-tidy -p build/extension` over every `extension/*.cpp` with `core/.clang-tidy` and the `extension/` headers, each with warnings as errors. Runs `make build` first for `compile_commands.json`. |
| `make format` | `clang-format --style=file:core/.clang-format --dry-run --Werror` over every `.cpp` and `.hpp` in `core/` and `extension/`. `clang-format --style=file:core/.clang-format -i <file>` applies the style. |
| `make test` | `ctest --no-tests=error` in `build/`. Runs `make build` first. |
| `make ci` | `lint`, `format`, `test` and `build`, in that order. |
| `make bench` | Runs `make build` first. CMake configures `core/` into `build/bench/` as a Release build, builds `murim_core_bench`, and runs it. It prints the scenario it ran and the tick times against the budget, and exits non-zero on a failed setup check or a missed budget. Not part of `make ci`. |
| `make sweep` | Runs `make build` first. CMake configures `core/` into `build/bench/` as a Release build, builds `murim_core_sweep`, and runs it over seeds `FIRST_SEED` to `LAST_SEED` for `YEARS` years, printing a Markdown table with, per seed, its households, the living and peopled villages at the start and the end, the peak, whether every faction stays peopled, the living at the end by realm rank, how many know an art, how many learned from a manual on a cell, the wars declared and peaces made, and the smithies and apothecaries raised; then totals over every seed: the seeds keeping every faction peopled and growing the living, the living by realm rank, the manual readers, the wars and peaces, and the seeds raising a smithy and an apothecary with their counts. Not part of `make ci`. |
| `make gcc` | CMake configures `core/` into `build/gcc/` with `GCC` as its C++ compiler, Homebrew's `g++-16` unless given another, under `core/CMakeLists.txt`'s own flags; builds every target and runs `ctest` there. Fails with a message when the compiler is not on `PATH`. A change to C++ runs it before it is pushed (owner, 2026-10-16, #374), since hosted CI builds with GCC, which warns where Apple Clang does not. Not part of `make ci`. |
| `make android` | Runs `make build` first. CMake configures `extension/` into `build/android/` with the NDK's `build/cmake/android.toolchain.cmake` for `arm64-v8a` at API level 24, as a Release build, and builds `murim_extension` and `murim_core_bench`. Not part of `make ci`: CI has no Android SDK. |
| `make export` | Runs `make build` first. CMake configures `extension/` into `build/macos/` for `arm64;x86_64` with `SDKROOT` set to the macOS SDK, into `build/windows/` for Windows x86_64 with llvm-mingw's `x86_64-w64-mingw32-clang++`, and into `build/linux/` for Linux x86_64 with llvm-mingw's `clang++` targeting `x86_64-linux-gnu`, linked by its LLD against the sysroot in `build/linux-sysroot/`, each as a Release build with `GODOTCPP_TARGET=template_release`, and builds `murim_extension` in each. `linux-elf-check.sh` then fails the export unless the Linux library is an x86_64 ELF shared object needing only glibc, libgcc_s and libstdc++, at glibc symbol versions up to 2.31, after checking that it refuses a control library needing `GLIBC_2.39`. Not part of `make ci`: it runs on a Mac only. |

The C++ compiler is the platform's default, except for the Windows and Linux libraries `make export` cross-builds. Their compiler is llvm-mingw 20261006, `llvm-mingw-20261006-ucrt-macos-universal.tar.xz` from `https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/`, SHA-256 `a7a19b11048e89372637d26279cdb7c430ed3cf8c1e224310421312bf986ebf6`. `mingw-toolchain.sh` fetches it into `build/mingw/`, checks the hash and unpacks it. Measured on the development Mac on 2026-10-08, a cold build of the library took 97 s, with no warnings, and the 1.2 MB DLL imports only `KERNEL32.dll` and the UCRT. The Linux library links against Ubuntu 20.04's glibc 2.31 and GCC 10's libstdc++ and libgcc, which godot-cpp links statically: `linux-sysroot.sh` fetches seven packages from the Ubuntu snapshot of 2026-10-06 into `build/linux-sysroot/`, checks each against its SHA-256 and unpacks them. Measured on the development Mac on 2026-10-11, a cold build of the library took 473 s, with no warnings, and the 1.7 MB shared library needs only `libc.so.6`, `libm.so.6` and `ld-linux-x86-64.so.2`, at glibc symbol versions up to `GLIBC_2.14`. CMake, Ninja, clang-format and clang-tidy come only from `core/requirements.txt`, installed into `.venv/` at the repository root, and every entry point runs them from `.venv/bin/`:

```sh
python3 -m venv .venv
.venv/bin/pip install --require-hashes -r core/requirements.txt
```

Re-run the install after `core/requirements.txt` changes.
