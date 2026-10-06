# Godot adapter

The GDExtension adapter through which Godot drives the simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The adapter is a GDExtension library built from `extension/`, linking `core/` unchanged. `murim.gdextension` at the repository root loads it into the Godot project there.
- It registers one class, `WorldAdapter`, which creates a core world, ticks it and returns its hash. GDScript cannot construct it; `WorldAdapter.create` is the only way to get one.
- `create` takes what the core's constructor takes, as Godot types: a seed, map rows as a `PackedStringArray`, armies as `Dictionary`s with `start` and `objective` (`Vector2i` cells) and `count` and `strength` (ints), and a separation. Converting them to the core's types, and the hash back to a signed int, is the adapter's work.
- Input the core refuses, or that does not fit the core's integer types, makes `create` report it with `push_error` and return null. No exception crosses into Godot.
- It holds no game rule: every call is passed to the core, which decides what is valid.
- A headless Godot test shows that a world with armies, driven through the adapter, gives the core's hash for the same scenario and tick count, against `world_hash`, a program that runs the core with no Godot; and that invalid scenarios return null while a valid one does not.

## Rationale

- Godot is the presentation adapter and the core reaches it through GDExtension: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The binding release, its pin, the build path, the scenario format, how refusals reach GDScript, the test runner and how CI installs Godot: ADR 0004 (`adr/0004-bind-the-core-to-godot-with-godot-cpp-through-cmake.md`).

## Open questions

- **Which platforms the library is built for.** It is built for the host only, macOS arm64 and Linux x86_64 so far; Android and iOS exports are not built.
