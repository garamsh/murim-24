# Godot adapter

The GDExtension adapter through which Godot drives the simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The adapter is a GDExtension library built from `extension/`, linking `core/` unchanged. `murim.gdextension` at the repository root loads it into the Godot project there.
- It registers one class, `WorldAdapter`, which creates a core world, sets its focus, ticks it, and reads back its hash, each army's body positions and agent counts, and each structure's cell and stage. GDScript cannot construct it; `WorldAdapter.create`, `WorldAdapter.create_m1` and `WorldAdapter.create_region` are the only ways to get one.
- `create_m1` builds the core's M1 benchmark scenario (`core/m1_scenario.cpp`), the same one the tick benchmark runs, and places the focus where the scenario does.
- `create_region` builds the region the core generates from a seed, with one agent born into its first village as the last army, and places the focus on that agent.
- `order` passes an order to the core: an army, an objective cell as a `Vector2i`, and skill indices as a `PackedInt64Array`. An order the core refuses makes it report the refusal with `push_error` and return false. `map_rows` and `skill_count` read back what a driver needs to aim one.
- Positions cross in sub-cell units as `Vector2`s, exact while a coordinate stays within 2^24. The tick rate, the cell size and the structure stages cross as the class's integer constants, so GDScript repeats none of them.
- `create` takes what the core's constructor takes, as Godot types: a seed, map rows as a `PackedStringArray`, and the armies (with their skills), structures and rules as `Dictionary`s keyed by the core's field names, with cells as `Vector2i`. Converting them to the core's types, and the hash back to a signed int, is the adapter's work.
- Input the core refuses, or that is missing, of the wrong type or outside the core's integer types, makes `create` report it with `push_error` and return null. No exception crosses into Godot.
- It holds no game rule: every call is passed to the core, which decides what is valid.
- `world_view/world_view.tscn`, the project's main scene, shows the M1 scenario. It draws every body through one `MultiMesh`, with no node per agent, sets the camera's centre as the focus before each tick, and ticks at the core's tick rate whatever the frame rate. Colours and sizes are presentation only.
- The world is presented in a three-quarter top-down view, one core cell to one 16-pixel tile, with combat in real time on the core's tick. Ground and roofs are seen from above; facades and standing bodies are drawn upright and sorted by the bottom edge of their footprint. A structure is drawn over a footprint of width x height cells, which the core gives it; the adapter does not yet read a footprint back or take one in `create`, where every structure is 1 x 1. Art is pixel art on that grid, from the Aseprite sources under `art/`, made to `docs/art-direction.md`. UI text is Korean first, with English supported.
- `play/play.tscn` steers the agent `create_region` bears. A left click orders it toward the cell clicked, keys 1 to 9 order it to strike with that skill, and 0 with all its skills; an order keeps whatever part the input does not change. Before each tick it sets the focus on the agent's lead body, and its camera follows. The focus is set this way so that a recorded order log, replayed under the same rule, gives the same hash. It draws plainly and is not the main scene; the visible-game task dresses it with the approved art.
- A headless Godot test shows that a world of two hostile armies with skills, and structures, driven through the adapter, gives the core's hash, body positions and counts for the same scenario, focus and tick count, against `world_hash`, a program that runs the core with no Godot; and that invalid scenarios return null while a valid one does not. It also shows that an agent ordered through the adapter leaves home and strikes a hostile camp down, while one left without an order does neither. A second test shows `play.tscn` turning a click and keys into orders: a click on a passable cell moves the agent toward it, and a click on a blocked cell and a key for a skill the agent lacks are refused.

## Rationale

- Godot is the presentation adapter and the core reaches it through GDExtension: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The binding release, its pin, the build path, the scenario format, how refusals reach GDScript, the test runner and how CI installs Godot: ADR 0004 (`adr/0004-bind-the-core-to-godot-with-godot-cpp-through-cmake.md`).
- Steering through the orders every army follows, and the focus set on the steered body before each tick: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`).
- The three-quarter top-down view, structure footprints, real-time combat, pixel art on a 16-pixel tile grid and Korean-first UI text, and the alternatives rejected: ADR 0012 (`adr/0012-present-the-world-in-a-three-quarter-top-down-view.md`), which supersedes ADR 0008 (`adr/0008-present-the-world-top-down-in-real-time-as-pixel-art.md`).

## Open questions

- **When the view draws the production art.** `world_view/world_view.tscn` still draws bodies and structures as coloured squares. The art under `art/` is not wired in, and the house waits on the adapter reading back each structure's footprint, which the core now holds.

- **Which platforms the library is built for.** It is built for the host only, macOS arm64 and Linux x86_64 so far; Android and iOS exports are not built.
