# Simulation

The simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The simulation core is a C++ library in `core/`, with no engine dependency.
- A world is created from a seed and advances one `tick()` at a time. Its state is a seeded pseudo-random sequence and a tick count; it holds no simulation content yet.
- A world exposes a hash of its whole state. Determinism is tested by comparing hashes: the same seed gives the same hash after the same number of ticks.
- The core is built, tested, linted and formatted with the toolchain ADR 0003 chose.

## Rationale

- The core is C++ and loads no engine, so its rules run headless: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The build, test framework, formatter and linter, and how CI installs them: ADR 0003 (`adr/0003-build-the-core-with-cmake-googletest-and-llvm-tools.md`).

## Open questions

- **Whether determinism holds across builds.** The test proves the same seed gives the same hash within one build. Whether that must also hold between compilers and platforms, for example between a PC and an Android player, is undecided.
