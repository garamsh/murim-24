# Simulation

The simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

- The simulation core is a C++ library in `core/`, with no engine dependency.
- A world is created from a seed, a map of passable and blocked cells, armies, structures, rules, villages and each faction's leadership. The rules are data: the separation people keep, the hostility between factions, and the floors of the structure stages. It advances one `tick()` at a time.
- Every NPC is a person: a position, health, a cooldown and a home village or none. A person's identity is its index among the world's people, kept for as long as the world runs, through every army it serves in and after it dies. A world holds at most `MAX_PEOPLE`, 500, living people. That is a hard upper bound, not a target.
- An army is a roster of the living people serving in it, never a count, with an objective, a faction and skills. Every living person stands on exactly one roster. A person who dies leaves its roster but keeps its record, so the dead cost no work on a tick and stay out of the spatial index.
- `conscript()` moves one living person from its roster to another army's. `disband()` returns every member with a home to that village's residents; members without one stay. Neither creates nor removes a person, so the people on rosters are conserved.
- Every army follows an order: an objective cell, and the skills, by index among its own, it strikes with. An army created from data follows its objective with all its skills. `order()` gives an army another order, which it follows from the next tick. That is the port through which a driver steers a body, whether the driver is Godot input or a test, and the same type and resolution serve an NPC and the player alike. Nothing records who steers an army.
- The player's body is an army of one person, born into a village (`world-generation.md`).
- A faction may have a leader, a person, and a succession rule, as data. The only rule is the duel; a faction without one keeps its leader. An order may name a faction to challenge, the same order for the player's body as for an NPC:
  - The challenger is the ordered army's lead. A duel taking members only refuses a challenger from an army of another faction, and a faction with no duel or no leader refuses every challenge.
  - The challenger and the leader become hostile to each other alone and fight by the one skill rule until one has no more than the duel's yield health left. The leader keeps the lead if both fall together.
  - A challenger who wins leads the faction. If the duel says so, every faction at peace with it turns hostile to it, a change to the hostility the world holds.
- Every person moves and strikes on every tick; there are no levels of detail. At 500 people the whole world costs less than the tick budget (below).
- People move toward their order's objective along their army's flow field, rebuilt when an order changes the objective. A step enters any cell the flow field reaches and never a blocked one. An objective must be reachable from where each member stands, when the army is created and when it is given an order.
- People keep the separation, about a cell, from neighbours found through a spatial hash rebuilt each tick:
  - a person steps away from anyone closer than the separation, the harder the closer, and otherwise along its pull toward the objective
  - a person feels no pull within its arrival ring, as many cells from the objective as the square root of its rank in the army, so an arrived army spreads out to the separation
  - a person within two cells of its ring counts as standing still. A person on the move does not give way to people standing still, and they step aside for it, across the way it is going, so a standing crowd parts for someone walking out of it
  - positions are read as the tick began, so the order people move in decides nothing
- A query visits the people it finds in place, with no result list to allocate or sort; every rule that reads it either adds up what it finds or breaks ties by index, so the order of the visit decides nothing.
- A skill is data: range, radius, damage to people, damage to structures and cooldown. One rule resolves every skill, for every person alike, the player's body included:
  - a person whose cooldown has passed draws one of its order's skills by the seed
  - it aims at the nearest living hostile person in range; a skill of range 0 reaches only its user, so it lands where the user stands, with or without anyone hostile near
  - every hostile person in the disc around that point takes the damage once, and every structure whose footprint the disc reaches takes the structure damage once
  - damage to structures is signed: a negative amount restores durability, up to the intact stage's floor, and never lowers a structure above that floor. Rebuilding is this rule, not a second one
  - damage to people lands after everyone has struck, and a person whose health reaches 0 dies
- A structure is a footprint of width x height cells from its top-left cell, and a durability. One cell is the 1 x 1 footprint, with no rule of its own. Structures are found by area through a spatial hash of their footprints' centres, queried as far beyond a disc as the widest footprint reaches, and a disc hits a structure when the footprint's nearest point lies within it. Its stage — intact, damaged, destroyed or rubble — follows from durability by the floors in the rules.
- A village is data: its structures and the army living in it, its residents, whose members make it their home. Its owner is its residents' faction. While its residents have a living member, it takes damage only from factions hostile to its owner, and repair only from the others. Once none lives, it takes either, and the first army to restore one of its structures becomes its residents, and its members' home, so the owner becomes that army's faction.
- A village's state is derived on each read and never stored:
  - ruins while every structure is rubble
  - otherwise raided while a living person of a faction hostile to its owner could damage one of its structures from where it stands: a skill that damages structures, its range and radius away
  - otherwise prosperous while every structure is intact, and rebuilding while one is not
- A village records who ruined it: the army, and its faction, whose strike brought its last standing structure to rubble. The record outlives rebuilding and is part of the hash.
- A structure's footprint blocks movement for as long as the world runs, rubble included, so the flow fields never change under an army. Footprints must lie on passable cells, cover no other structure, and leave every person's start uncovered.
- All simulation state is integers: positions are fixed-point at 1/256 of a cell.
- A world exposes a hash of its whole state. Determinism is tested by comparing hashes: the same seed gives the same hash after 10,000 ticks with armies moving, fighting, losing people and breaking structures. A test pins that hash, so it must be the same on every compiler and platform. The hash covers every person, living or dead, each army's roster and order, and the villages, and a log of orders, each with the tick it was given before, replays to the same hash.
- The world ticks 30 times a second, so a rate given per second, such as skills, becomes work per tick.
- The tick budget is a p99 tick time of at most 4 ms on the development Mac (Apple Silicon), on the M1 benchmark scenario in `core/m1_scenario.cpp`, the worst case: MAX_PEOPLE people, every one in one battle of two lines with a barricade between them, and two caster armies breaking it. On 2026-10-07 it measured a tick p99 of 0.487 ms. `make bench` measures it. Timing depends on the machine, so `make ci` does not run it.
- The core is built, tested, linted and formatted with the toolchain ADR 0003 chose. Lint parses it on every host against one pinned Ubuntu 24.04 sysroot, unpacked by `lint-sysroot.sh`, so local and CI lint read the same standard library.

## Rationale

- The core is C++ and loads no engine, so its rules run headless: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The build, test framework, formatter and linter, and how CI installs them: ADR 0003 (`adr/0003-build-the-core-with-cmake-googletest-and-llvm-tools.md`).
- Lint against one pinned Linux sysroot, because clang-tidy's findings depend on the standard library it parses: ADR 0016 (`adr/0016-lint-against-one-pinned-linux-sysroot-on-every-host.md`).
- Every NPC a person with an identity, armies as rosters, conscription and disbanding, no levels of detail, the population bound, and the movement rule: ADR 0017 (`adr/0017-make-every-npc-an-individual-and-every-army-a-roster.md`), which supersedes ADRs 0005 and 0007 and ADR 0006's multiplicities.
- Flow fields, the spatial hash, and fixed-point numbers for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`), whose tiers and aggregates ADR 0017 removes.
- Skills as data, the single rule that resolves them, and structures and their stages: ADR 0006 (`adr/0006-resolve-every-skill-through-one-rule-scaled-by-count.md`), whose scaling by count ADR 0017 removes.
- Villages, their derived state, repair as a signed structure effect, owners following residents, and the recorded destroyer: ADR 0015 (`adr/0015-run-the-village-life-cycle-from-structures-and-residents.md`).
- Footprints, the hit rule they share, footprints that block in every stage, and objectives that must be reachable: ADR 0013 (`adr/0013-give-structures-footprints-that-block-movement-in-every-stage.md`).
- Leaders as people, the duel as faction data fought by the one skill rule, and challenges through the order port: ADR 0018 (`adr/0018-change-a-factions-leader-by-duel-as-faction-data.md`).
- Orders as the one shape of every army's intent, and the player's body as an army of one: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`).

## Open questions

- **Whether rubble should free its cells.** Rubble blocks as an intact structure does. Freeing it would mean rebuilding the flow fields that cross it when a structure falls, on the tick it falls.
- **Whether two people on the move can block each other.** A person on the move gives way to another on the move as to anyone, so two marching head-on into each other may hold each other up. No test or scenario covers it yet.
