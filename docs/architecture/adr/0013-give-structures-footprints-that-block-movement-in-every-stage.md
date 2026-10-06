# ADR 0013: Give structures footprints that block movement in every stage

> Status: accepted; its rule that every footprint blocks superseded by ADR-0022, which makes blocking per-kind data
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0012 has a structure occupy a footprint of width x height cells, and the approved village house covers 3 x 3 (`art/mockup/target_screen.lua`). The core gave every structure one cell: an area skill hit it when the cell's centre lay in its disc, nothing blocked movement, and village templates set single cells side by side (issue #39). ADR 0006 derives a structure's stage from its durability, so whether a broken structure still blocks is a choice of that stage. Options weighed:

- **The hit rule.** A disc hits a structure when the footprint's nearest point lies within it, for every footprint, the 1 x 1 included. Candidates come from the spatial hash of footprint centres, queried as far beyond the disc as the widest footprint reaches. Rejected: testing each footprint cell's centre, which misses a footprint the disc reaches between centres and costs a test per cell; and indexing every cell of every footprint, which visits one structure many times.
- **Passability.** A footprint blocks its cells from the world's creation, in every stage, so a flow field never changes under an army. Rejected: rubble freeing its cells, which would rebuild every flow field crossing a structure on the tick it falls. That is a spike on a tick the M1 benchmark has ten times, and ADR 0012 names flow-field cost as a sign of a wrong decision.
- **Reachability.** Once footprints can seal a gap, an objective can be passable yet out of reach. A body that cannot reach its objective counted every unreachable cell, the map's outside included, as no farther, walked off the map and failed a bounds check (`core/flow_field.cpp`). The same held for an order to an enclosed pocket of terrain. An objective must be reachable from the army's start when it is created and from its lead body when it is ordered. Rejected: letting such an army stand still, which turns a mistaken order into a silent one.
- **Village templates.** An area of width x height cells with structures that carry footprints, laid out from the mockup: the 17 x 9 village, and its 7 x 9 west end as a hamlet. A village draws its template among those with room in its owner's territory. Rejected: drawing among all templates, which failed 5 of 200 seeds at 96 cells when the 17 x 9 village found no room; and smaller templates than the mockup's.
- **Region size.** 128 cells square. With a fourth faction, 1 of 200 seeds left no room for any template at 96 or 112 cells, and none did at 128.

## Decision

A structure is a footprint of width x height cells from its top-left cell, the 1 x 1 being the smallest. An area skill hits it where its disc reaches the footprint, and its cells block movement in every stage, rubble included. An objective must be reachable from where its army stands. Village templates are areas of footprints laid out from the approved mockup, a village draws its template among those with room, and the region is 128 cells square.

## Consequences

- The approved house is 3 x 3 cells in the simulation as in the art, and a body walks around it.
- A 1 x 1 structure is hit when a disc reaches any part of its cell, not only its centre, so more strikes break structures. The pinned determinism hash changed.
- Ruins stay in the way after a raid until a later decision frees them.
- An order to a cell the army cannot reach is refused at the port, and the play scene's click on such a cell is refused rather than crashing.
- Generating a region costs more: twice the area, and a summed-area table per template per village.

## Signs the decision was wrong

- Players expect to walk through rubble, and ruins read as walls.
- Villages crowd a faction's territory so that armies cannot path between them, and orders are refused for cells players can see.
- The widest footprint grows until the structure query beyond each disc visits many structures it does not hit.
