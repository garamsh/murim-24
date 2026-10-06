# Simulation

The simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The simulation core is a C++ library in `core/`, with no engine dependency.
- A world is created from a seed, a map of passable and blocked cells, armies, and a separation distance. It advances one `tick()` at a time.
- An army has a start, an objective, a count and a strength. It is a list of bodies: one aggregate, or one body per agent.
- A focus point sets each army's level of detail:
  - near: agents, moving every tick
  - middle: agents, moving every 4 ticks
  - far: one aggregate, moving every 16 ticks
- Crossing into a tier with a different body limit merges the army and splits it again. Count and strength carry over exactly.
- Bodies move toward their army's objective along the army's flow field, and never onto a blocked cell. Agents keep the separation distance from neighbours found through a spatial hash, rebuilt each tick.
- All simulation state is integers: positions are fixed-point at 1/256 of a cell.
- A world exposes a hash of its whole state. Determinism is tested by comparing hashes: the same seed gives the same hash after 10,000 ticks with armies moving and crossing tiers. A test pins that hash, so it must be the same on every compiler and platform.
- The core is built, tested, linted and formatted with the toolchain ADR 0003 chose.

## Rationale

- The core is C++ and loads no engine, so its rules run headless: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The build, test framework, formatter and linter, and how CI installs them: ADR 0003 (`adr/0003-build-the-core-with-cmake-googletest-and-llvm-tools.md`).
- Tiers that differ only in data, the army aggregate and its exact totals, flow fields, the spatial hash, and fixed-point numbers for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`).

## Open questions

- **How per-agent state survives a collapse.** Count and strength are all an aggregate holds. Combat will show whether that is enough, or whether a far army needs more than its totals.
