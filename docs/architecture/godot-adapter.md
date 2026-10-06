# Godot adapter

The GDExtension adapter through which Godot drives the simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The adapter is a GDExtension library built from `extension/`, linking `core/` unchanged. `murim.gdextension` at the repository root loads it into the Godot project there.
- It registers one class, `WorldAdapter`, which creates a core world from a seed, ticks it and returns its hash, converting Godot's signed 64-bit int to and from the core's seed and hash types.
- It holds no game rule: every call is passed to the core.
- A headless Godot test shows that a world driven through the adapter gives the core's hash for the same seed and tick count, against `world_hash`, a program that runs the core with no Godot.

## Rationale

- Godot is the presentation adapter and the core reaches it through GDExtension: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The binding release, its pin, the build path, the test runner and how CI installs Godot: ADR 0004 (`adr/0004-bind-the-core-to-godot-with-godot-cpp-through-cmake.md`).

## Open questions

- **Whether the adapter's C++ is linted.** clang-tidy reads none of it, because godot-cpp's class macro fails the core's checks; a configuration for `extension/` is undecided.
- **Which platforms the library is built for.** It is built for the host only, macOS arm64 and Linux x86_64 so far; Android and iOS exports are not built.
