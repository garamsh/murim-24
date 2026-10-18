# ADR 0083: Build every field a tick asks for, with no per-tick limit

> Status: proposed
> Date: 2026-10-18

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0043 shares fields within areas by area and target and builds them on demand "within a per-tick limit". A limit leaves some fields unbuilt in a tick, and the people bound for them stand still. Which people those are then depends on what the cache held, not only on the world's state.
- **A loaded world moves differently.** Its cache starts empty, so a world reloaded mid-walk would hold different people still than the world that ran on. That breaks ADR 0049's determinism (#188).
- **A fixed constant decides who moves.** The limit is a tuning number standing in for what the machine can afford, the kind `simulation.md` §The principle calls a defect: the technical capacity is not a rule of play.

Building ADR 0077's step 4 (#371, #375) measured the limit's absence:
- **Builds a tick:** the live scenario of 5,000 people builds 0.12 fields a tick in steady state, at most 14 in one tick, once a field is held while a person bound for it stands in or beside its area.
- **Tick time:** live p99 is under the 4 ms budget in each of 5 interleaved rounds, at 3.31 to 3.89 ms.
- **M1:** its p99 rises 0.08 to 0.1 ms over `main`, within the 0.2 ms steady-cost bar.

The PM ruled on #371 to drop the limit.

## Decision

Every field a tick asks for is built in that tick, with no per-tick limit. A field is a function of the map, the coarse graph and its key alone, so what is cached never decides who moves, and a loaded world moves as one that ran on.

This supersedes ADR 0043's per-tick limit on field builds. The rest of ADR 0043 stands as its other successors leave it.

## Consequences

- No one stands still for want of a field, so the visible stall ADR 0043 names, people held when many armies enter new areas at once, does not occur.
- The cost of a burst of first builds lands in the tick that asks for them. The tick budget, not a quota, bounds it, and the bench measures it.
- A field's buffers come from a pool, and a steady-state tick allocates nothing in the cache.

## Signs the decision was wrong

- Bursts of first builds push the live p99 past the 4 ms budget in the bench, on the development Mac or on the lowest supported device (ADR 0048).
- A larger map or more armies make builds a tick grow with the people rather than with the areas they enter.
