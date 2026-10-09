# ADR 0043: Find paths over a coarse graph of areas, and step by fields within them

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Each army holds a flow field over the whole map (ADR 0005, kept by ADR 0017), 8-connected with straight steps of 5 and diagonal steps of 7, each cell's step the straightest of a shortest path (ADR 0037). The world builds full-map fields:
- when it is created
- when an army is given an order
- for every army whenever growth lays a blocking structure
- for each reachability check that founding, migration and raids make

A field holds a `Step` of two 32-bit integers and a 32-bit cost per cell (`core/flow_field.hpp`), 12 bytes.

Measured on the development Mac (Apple M1, 8 GB, macOS, on AC power) by the `measure` program on branch `evidence/central-plains`, over `core/` at `origin/main` 1d943f0. Other workers' test runs shared the machine, at a load average of 7 to 9, so each figure is the range over runs. Building one field toward the centre of an open map, the median of 20 builds, over two to six runs:

| Side (cells) | Build (ms) | Bytes |
|---|---|---|
| 32 | 0.149 to 0.304 | 12,288 |
| 64 | 0.672 to 0.717 | 49,152 |
| 128 | 3.611 to 3.819 | 196,608 |
| 256 | 15.031 to 15.912 | 786,432 |
| 512 | 60.388 to 127.370 | 3,145,728 |

Creating the generated region at 512 cells, with its 20 armies, took 874 to 949 ms. At 128 cells it took 48 to 53 ms.

A 512-cell world at the bound holds hundreds of villages, each with a resident army, besides parties and settlers. Full-map fields would hold about 3 MB each, near a gigabyte for 300 armies. Laying one blocking structure would rebuild them all at 60 to 127 ms each.

The concept's answer of 2026-10-06 (issue #173) is hierarchical pathfinding: between areas on a coarse graph, within an area in detail. Whatever replaces the field must keep ADR 0037 for people near the player: the straightest step on 8 ways, no step beside a blocked corner, and the hold before a foe on the move. Options weighed:

- **The coarse graph.** The map is divided into the areas of ADR 0042, 32 x 32 cells.
  - Within an area, the passable cells are labelled into connected components.
  - Where two adjacent areas meet, each maximal run of cells passable on both sides of the border is an entrance between the two components it joins.
  - A node is an entrance's middle cell on each side. Edges join the nodes of one component at the cost of ADR 0037's path between them within the area, and the two sides of an entrance at 5.

  This is HPA* (Botea, Müller and Schaeffer, "Near optimal hierarchical path-finding", *Journal of Game Development* 1(1), 2004), with components added so reachability is exact. It is built when the world is created. Laying or freeing a blocking structure rebuilds only the areas it touches. Chosen.
  - Rejected: a precomputed field per area toward every other area. At 256 areas that is 65,536 fields of about 14 KB.
  - Rejected: a navigation mesh with any-angle paths. ADR 0037 rejected free direction.
- **Paths.** An army's coarse path runs from its lead's area to its objective's area, by A* over the graph with both cells inserted as nodes for the search. It is found when the army is created or ordered, and again when an area on it is rebuilt. Rejected: A* per person, which ADR 0005 rejected because its cost grows with people rather than armies.
- **Steps in live and stepped areas.** A person steps by the field of the area it stands in, over the area and a one-cell margin, 34 x 34 cells.
  - Where the objective lies in the area, the field is ADR 0037's toward it, restricted to the area.
  - Otherwise it is seeded along the exit entrance of the army's coarse path. Each seeded cell is given its cost to the objective along the coarse path: the octile cost on open ground, where the straight line to the objective is unobstructed.
  - A cell's step is still the one, of those beginning a shortest path, nearest the straight line to the objective itself, not to the entrance.

  So over open ground a walk crossing areas stays on the line between the cells' centres, as ADR 0037's tests require. A diagonal step is still open only where both cells beside it are passable. The hold before a foe reads only pulls, reaches and positions, so it is unchanged. Fields are kept by area and target, so armies bound the same way share one, and are built on demand. At most a rules-given number of builds are made a tick, in army index order. A member whose field is not yet built feels no pull for that tick. Chosen.
  - Rejected: one field per army over the live window, 160 cells across. A build there would cost about 6 ms by the table above. Every live army would rebuild whenever the window moved, tens of builds at once.
- **Far armies.** A far army follows its coarse path's nodes by path cost and uses no field (ADR 0042).
- **Reachability.** An objective is reachable from a cell when their components are joined in the coarse graph. Growth's check that every person can still reach its objective asks the graph after the touched areas are rebuilt. No full-map field is built for it. Rejected: keeping full-map fields for the checks only, which keeps the builds of 60 ms or more on every check.

## Decision

The map is divided into 32-cell areas whose passable cells are labelled into components. A coarse graph joins components through the entrances along shared borders, at ADR 0037's costs, and is rebuilt only where a blocking structure changes an area. An army finds its coarse path by A* over that graph. In live and stepped areas a person steps by a field over the area it stands in and a margin, toward the objective or seeded along the path's exit at its coarse cost. Its step is the one nearest the straight line to the objective itself. Fields are shared by area and target and built on demand within a per-tick limit. Reachability is read from the graph. This would replace the per-army full-map field that ADR 0005 introduced and ADRs 0017 and 0037 keep. ADR 0037's 8-connected costs, its straightest step and its corner rule hold within every area.

## Consequences

- A field takes about 14 KB, 34 x 34 cells of 12 bytes, and costs between the 0.149 to 0.304 ms measured at 32 cells and the 0.672 to 0.717 ms at 64. Memory for movement grows with the areas armies stand in, not with armies times the map.
- Creating a world builds the graph and no full-map field. Laying a blocking structure rebuilds one to four areas' components and edges, not every army's field.
- Over obstacles a coarse path is near-optimal, not optimal: Botea, Müller and Schaeffer report paths within 1% of optimal, after smoothing, on their benchmark maps. A person may take a slightly longer way round a range than the full field would. Over open ground, and wherever the objective lies in the person's own area, steps are ADR 0037's.
- ADR 0037's tests are kept, and new ones walk a diagonal across three areas on open ground within a cell of the line. Reachability from the graph must equal reachability from a full-map field on random maps.
- The pinned hash changes where a coarse path differs from the full field's shortest path. The implementing pull request records the new value.
- A field not yet built holds its people still for a tick, which a test shows does not change who reaches what.

## Signs the decision was wrong

- Walks across area borders visibly bend at the borders, enough that the owner names it, as the staircase was named in #162.
- People queue at an entrance's middle cell rather than spreading across the entrance, so crowds jam at borders that open ground does not show.
- The per-tick limit on builds holds people still for long enough to be seen when many armies enter new areas at once, such as when a war begins.
- Paths round ranges are noticeably longer than the shortest, enough that the player names a detour.
