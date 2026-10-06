# Simulation

The simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The simulation core is a C++ library in `core/`, with no engine dependency.
- A world is created from a seed, a map of passable and blocked cells, armies, structures and rules. The rules are data: the separation distance (also an agent's footprint), the hostility between factions, and the floors of the structure stages. It advances one `tick()` at a time.
- An army has a start, an objective, a count, a strength, a faction and skills. It is a list of bodies: one aggregate, or one body per agent.
- Every army follows an order: an objective cell, and the skills, by index among its own, it strikes with. An army created from data follows its objective with all its skills. `order()` gives an army another order, which it follows from the next tick. That is the port through which a driver steers a body, whether the driver is Godot input or a test, and the same type and resolution serve an NPC and the player alike. Nothing records who steers an army.
- The player's body is an army of one agent, born into a village (`world-generation.md`).
- A focus point sets each army's level of detail:
  - near: agents, moving every tick
  - middle: agents, moving every 4 ticks
  - far: one aggregate, moving every 16 ticks
- Each army moves at its own phase within its tier's interval, set by its index, so a tier's movement spreads over the interval's ticks. Every army of a tier strikes on the ticks the interval divides, so a step's strikes land together.
- Crossing into a tier with a different body limit merges the army and splits it again. Count and strength carry over exactly.
- Bodies move toward their order's objective along the army's flow field, rebuilt when an order changes the objective, and never onto a blocked cell. Agents keep the separation distance from neighbours found through a spatial hash, rebuilt each tick. A query visits the bodies it finds in place, with no result list to allocate or sort; every rule that reads it either adds up what it finds or breaks ties by index, so the order of the visit decides nothing.
- A skill is data: range, radius, damage to bodies, damage to structures and cooldown. One rule resolves every skill, from an agent, an aggregate or the player's body alike:
  - a body whose army is due and whose cooldown has passed draws one of its order's skills by the seed
  - it aims at the nearest living hostile body in range
  - every hostile body and structure in the disc around that target takes the damage, times the agents the user stands for and the uses its cooldown allows in its tier's step
  - a body struck takes it once for each of its agents the disc covers, at one agent per separation square, so a battle has the same winner near the focus and far from it
  - damage lands after every body has struck
- Damage folds into count and strength, which is all of an agent's state that a collapse keeps. Strength falls by the damage and count in proportion; a cooldown merges to the latest. Dead bodies leave when the army regroups.
- A structure is a cell and a durability, found by area through a spatial hash of its own. Its stage — intact, damaged, destroyed or rubble — follows from durability by the floors in the rules.
- All simulation state is integers: positions are fixed-point at 1/256 of a cell.
- A world exposes a hash of its whole state. Determinism is tested by comparing hashes: the same seed gives the same hash after 10,000 ticks with armies moving, crossing tiers, fighting and breaking structures. A test pins that hash, so it must be the same on every compiler and platform. The hash covers each army's order, and a log of orders, each with the tick it was given before, replays to the same hash when the focus is set on the steered body's lead before each tick.
- The world ticks 30 times a second, so a rate given per second, such as skills, becomes work per tick.
- The tick budget is a p99 tick time of at most 4 ms on the development Mac (Apple Silicon), on the M1 benchmark scenario in `core/m1_scenario.cpp`: 20,000 agents across every tier, 500 of them fighting in the near tier, and about 50 area strikes a second breaking structures. `make bench` measures it. Timing depends on the machine, so `make ci` does not run it.
- The core is built, tested, linted and formatted with the toolchain ADR 0003 chose.

## Rationale

- The core is C++ and loads no engine, so its rules run headless: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The build, test framework, formatter and linter, and how CI installs them: ADR 0003 (`adr/0003-build-the-core-with-cmake-googletest-and-llvm-tools.md`).
- Tiers that differ only in data, the army aggregate and its exact totals, flow fields, the spatial hash, and fixed-point numbers for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`).
- Skills as data, the single rule that resolves them, structures and their stages, and damage folded into count and strength: ADR 0006 (`adr/0006-resolve-every-skill-through-one-rule-scaled-by-count.md`).
- Movement at a per-army phase, with strikes kept on the tier's common tick for tier-invariance: ADR 0007 (`adr/0007-move-each-army-at-its-own-phase-within-its-tier.md`).
- Orders as the one shape of every army's intent, the player's body as a one-agent army, and the focus set on the steered body by its driver: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`).

## Open questions

- **Structure footprints.** ADR 0012 has a structure occupy a footprint of width x height cells, set by data, and the village house is drawn over 3 x 3. A structure here is still one cell; giving it a footprint changes how an area skill finds it, whether its cells block movement while it stands, and how village templates lay structures out.

- **Whether destroyed structures change passability.** A structure's stage does not affect movement yet. Making rubble or intact walls block or free cells would mean rebuilding the flow fields that cross them.
- **Whether a body can always leave a crowd.** A body standing among agents closer than the separation steps away from them, and a step that lengthens its path is not taken, so a body inside a standing crowd can stay where it is however it is ordered. A body born at a village's centre, among its residents, did so; `born_in` places it at the corner instead.
