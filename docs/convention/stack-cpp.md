# C++ stack

The concrete C++ form of naming, file layout, tests, comments and the check entry points, for the code under `core/`.

Checked against C++17 (ISO/IEC 14882:2017), CMake 4.4.4, Ninja 1.13.2, LLVM 22.1.8 (clang-format, clang-tidy) and GoogleTest 1.18.0.

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
- Code under `core/` lives in the `murim` namespace; a helper used by one source file goes in an unnamed namespace inside it.

## File layout

- `core/CMakeLists.txt` is the one CMake project; it declares the library, its tests and their dependencies.
- A header opens with `#pragma once`.
- A source file includes its own header first, then a blank line, then every other include; clang-format orders each block.
- `core/.clang-format` and `core/.clang-tidy` are the formatter and linter configuration.
- Build output goes to `build/` at the repository root, which `.gitignore` excludes.

## Tests

- Framework: GoogleTest, which CMake fetches at configure time from the release archive pinned by hash in `core/CMakeLists.txt`.
- A unit test is `<unit>_test.cpp` next to `<unit>.cpp`, built as a test executable in `core/CMakeLists.txt` and registered with `gtest_discover_tests`.
- A test is `TEST(<Unit>, <BehaviourInPascalCase>)`: GoogleTest 1.18.0's primer says suite and test names should not contain underscores.
- Assertions read the public API's return values, such as `World::hash()`.

## Comments

- Doc comment: `///` on the lines above the declaration in the header.
- Every other comment: `//`. No `/* */` blocks.

## Entry points

| Name | What it runs |
|---|---|
| `make build` | CMake configures `core/` into `build/` with the Ninja generator, then builds every target, the tests included. |
| `make lint` | `check-contents-list.sh` over every Markdown file, then `clang-tidy -p build` over every `core/*.cpp` with warnings as errors. Runs `make build` first for `compile_commands.json`. |
| `make format` | `clang-format --dry-run --Werror` over every `core/*.cpp` and `core/*.hpp`. `clang-format -i <file>` applies the style. |
| `make test` | `ctest --no-tests=error` in `build/`. Runs `make build` first. |
| `make ci` | `lint`, `format`, `test` and `build`, in that order. |

The C++ compiler is the platform's default. CMake, Ninja, clang-format and clang-tidy come only from `core/requirements.txt`, installed into `.venv/` at the repository root, and every entry point runs them from `.venv/bin/`:

```sh
python3 -m venv .venv
.venv/bin/pip install --require-hashes -r core/requirements.txt
```

Re-run the install after `core/requirements.txt` changes.
