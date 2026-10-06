# Godot adapter

The GDExtension adapter through which Godot drives the simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The adapter is a GDExtension library built from `extension/`, linking `core/` unchanged. `murim.gdextension` at the repository root loads it into the Godot project there.
- It registers one class, `WorldAdapter`, which creates a core world, sets its focus, ticks it, and reads back its hash, each army's body positions and agent counts, and each structure's cell and stage. GDScript cannot construct it; `WorldAdapter.create` and `WorldAdapter.create_m1` are the only ways to get one.
- `create_m1` builds the core's M1 benchmark scenario (`core/m1_scenario.cpp`), the same one the tick benchmark runs, and places the focus where the scenario does.
- Positions cross in sub-cell units as `Vector2`s, exact while a coordinate stays within 2^24. The tick rate, the cell size and the structure stages cross as the class's integer constants, so GDScript repeats none of them.
- `create` takes what the core's constructor takes, as Godot types: a seed, map rows as a `PackedStringArray`, and the armies (with their skills), structures and rules as `Dictionary`s keyed by the core's field names, with cells as `Vector2i`. Converting them to the core's types, and the hash back to a signed int, is the adapter's work.
- Input the core refuses, or that is missing, of the wrong type or outside the core's integer types, makes `create` report it with `push_error` and return null. No exception crosses into Godot.
- It holds no game rule: every call is passed to the core, which decides what is valid.
- `world_view/world_view.tscn`, the project's main scene, shows the M1 scenario. It draws every body through one `MultiMesh`, with no node per agent, sets the camera's centre as the focus before each tick, and ticks at the core's tick rate whatever the frame rate. Colours and sizes are presentation only.
- A headless Godot test shows that a world of two hostile armies with skills, and structures, driven through the adapter, gives the core's hash, body positions and counts for the same scenario, focus and tick count, against `world_hash`, a program that runs the core with no Godot; and that invalid scenarios return null while a valid one does not.

## Rationale

- Godot is the presentation adapter and the core reaches it through GDExtension: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The binding release, its pin, the build path, the scenario format, how refusals reach GDScript, the test runner and how CI installs Godot: ADR 0004 (`adr/0004-bind-the-core-to-godot-with-godot-cpp-through-cmake.md`).

## Open questions

- **Which platforms the library is built for.** It is built for the host only, macOS arm64 and Linux x86_64 so far; Android and iOS exports are not built.
