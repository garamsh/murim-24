# Simulation

The simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The simulation core is a C++ library in `core/`, with no engine dependency.
- A world is created from a seed, a map of passable and blocked cells, armies, structures and rules. The rules are data: the separation distance, the hostility between factions, and the floors of the structure stages. It advances one `tick()` at a time.
- An army has a start, an objective, a count, a strength, a faction and skills. It is a list of bodies: one aggregate, or one body per agent.
- A focus point sets each army's level of detail:
  - near: agents, moving every tick
  - middle: agents, moving every 4 ticks
  - far: one aggregate, moving every 16 ticks
- Crossing into a tier with a different body limit merges the army and splits it again. Count and strength carry over exactly.
- Bodies move toward their army's objective along the army's flow field, and never onto a blocked cell. Agents keep the separation distance from neighbours found through a spatial hash, rebuilt each tick.
- A skill is data: range, radius, damage to bodies, damage to structures and cooldown. One rule resolves every skill, from an agent, an aggregate or the player's body alike:
  - a body whose army is due and whose cooldown has passed draws one of its army's skills by the seed
  - it aims at the nearest living hostile body in range
  - every hostile body and structure in the disc around that target takes the damage, times the agents the user stands for
  - damage lands after every body has struck
- Damage folds into count and strength, which is all of an agent's state that a collapse keeps. Strength falls by the damage and count in proportion; a cooldown merges to the latest. Dead bodies leave when the army regroups.
- A structure is a cell and a durability. Its stage — intact, damaged, destroyed or rubble — follows from durability by the floors in the rules.
- All simulation state is integers: positions are fixed-point at 1/256 of a cell.
- A world exposes a hash of its whole state. Determinism is tested by comparing hashes: the same seed gives the same hash after 10,000 ticks with armies moving, crossing tiers, fighting and breaking structures. A test pins that hash, so it must be the same on every compiler and platform.
- The core is built, tested, linted and formatted with the toolchain ADR 0003 chose.

## Rationale

- The core is C++ and loads no engine, so its rules run headless: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The build, test framework, formatter and linter, and how CI installs them: ADR 0003 (`adr/0003-build-the-core-with-cmake-googletest-and-llvm-tools.md`).
- Tiers that differ only in data, the army aggregate and its exact totals, flow fields, the spatial hash, and fixed-point numbers for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`).
- Skills as data, the single rule that resolves them, structures and their stages, and damage folded into count and strength: ADR 0006 (`adr/0006-resolve-every-skill-through-one-rule-scaled-by-count.md`).

## Open questions

- **Whether destroyed structures change passability.** A structure's stage does not affect movement yet. Making rubble or intact walls block or free cells would mean rebuilding the flow fields that cross them.
