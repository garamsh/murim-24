# ADR 0005: Simulate armies at three levels of detail in fixed-point integers

> Status: superseded by ADR-0017
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The world runs wars whether or not the player is near them, and the player is one agent under the same rules (`docs/concept.md`). Simulating every soldier at full rate everywhere does not scale, so detail follows a focus point: agents near it, aggregates far from it. Movement is the core's first numeric state, which forces the question `simulation.md` left open: whether the same seed must give the same world on every compiler and platform. Options weighed:

- **How tiers differ.** One representation for every tier — an army is a list of bodies, and a tier is data: how far it reaches, how often its armies move, how many bodies it allows. Rejected: an agent type and an aggregate type with a code path each, which puts a branch per tier into every rule written later, the player's tier included.
- **Expanding and collapsing.** An army is merged into its totals and split into at most the tier's number of bodies, with count and strength divided and the remainder handed out one unit at a time, so both totals carry over exactly. Rejected: keeping per-agent state through a collapse, which makes a far army as costly as a near one; and averaging strength per agent, which loses the remainder.
- **Movement.** One flow field per army, a breadth-first search over 4-connected passable cells from its objective, built when the army is created. Rejected: per-agent pathfinding (A*), whose cost grows with agents rather than armies; and rebuilding fields each tick, which the static map does not need.
- **Moving without entering walls.** A body moves one axis at a time, at most one cell per move, and a move counts only where it does not lengthen the path to the objective. That one rule keeps bodies off blocked cells and out of corners and stops separation from pushing them back along their path. Rejected: a separate collision check against blocked cells, which is a second rule for the same case.
- **Neighbours.** A spatial hash rebuilt each tick, bucketed by cell with the coordinate hash of Teschner et al. (2003). Near and middle agents step away from neighbours closer than the separation distance, and follow the flow field otherwise. Every move reads neighbours from where bodies stood when the tick began, so update order does not decide outcomes. Rejected: a brute-force scan, which costs the square of the agent count; and a k-d tree, which costs more to rebuild each tick and more code for the same queries.
- **Numbers.** Fixed-point integers: positions are 32-bit integers counting 1/256 of a cell, and distances are compared as 64-bit integers. Rejected: `float` or `double`, whose results change with compiler settings such as `-ffp-contract`, which lets a multiply and an add be fused into one rounding, so every compiler and platform would need the same settings held by hand; and a software floating-point library, which costs speed and a dependency to recover what integers give for free.

## Decision

An army is a list of bodies: one aggregate holding its count and strength, or one body per agent. The focus point sets each army's tier by Chebyshev distance from its lead body: near (within 8 cells) moves every tick as agents, middle (within 32 cells) every 4 ticks as agents, far every 16 ticks as one aggregate. A move covers the distance of the ticks it skipped. Crossing into a tier with a different body limit merges and re-splits the army with its totals exact. Bodies move along a per-army flow field, and agents keep a separation distance from neighbours found through a spatial hash. All simulation state is integers, so determinism holds across compilers and platforms: the same seed gives the same hash on every build, and a test pins that hash.

## Consequences

- Rules written later run unchanged in every tier. A rule that needs per-agent state reads it off the bodies, and that state does not survive a collapse unless it folds into count and strength.
- No floating point enters simulation state. A rule needing a fraction expresses it in fixed point.
- The pinned hash changes with every change to the rules, on purpose, and the pull request making the change records the new value.
- Each army holds a flow field the size of the map.
- An objective is one cell, so an army whose agents cannot all fit there at the separation distance keeps jostling at its edge.
- Lockstep multiplayer or replays across platforms need no float workarounds, since equal inputs give equal states.

## Signs the decision was wrong

- The pinned hash differs between two supported compilers or platforms.
- An army sitting on a tier boundary expands and collapses every few ticks, and the cost shows in profiles.
- Combat or skills need per-agent state, such as wounds, that count and strength cannot carry through a collapse, so every rule grows a case for aggregates.
- Precision of 1/256 of a cell, or the 32-bit range, is too coarse or too small for a rule, and values are scaled per rule to fit.
- Flow fields per army dominate memory on large maps with many armies.
- Agents at their objective never settle, or queue behind one another without reaching it.
