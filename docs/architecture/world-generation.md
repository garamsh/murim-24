# World generation

How the core generates a region from a seed: what exists of it now, and the decisions it rests on.

## Current decisions

- `core/region.cpp` generates a region from a seed and three data tables: terrain kinds, factions and village templates. `region_data()` returns the game's tables, each defined there once; the generator reads only what it is passed, so a table changes without the generator changing.
- A region is the inputs a `World` is created from (seed, map rows, armies, structures, rules) and what the world does not hold: the terrain kind and owning faction of each cell, and the villages.
- A terrain kind is a name, whether bodies may stand on it, and a weight. The map rows mark the cells of an impassable kind as blocked.
- A faction is a name and the factions it attacks; the rules' hostility matrix is built from those lists.
- A village template is a name, the side of the square it occupies, and the cells of its structures within that square. A village has a name, an owner, its square, its structures and a resident army at its centre, of a fixed number of agents for each structure.
- Terrain and territory each partition the grid by nearest site: terrain around sites at random cells, each of a kind drawn by weight; territory around one capital per faction, each the farthest from those already placed among a fixed number of random cells.
- Six to eight villages are owned in turn by each faction. Each takes a template drawn by the seed and a corner drawn among every corner where its square lies on free passable cells of its owner's territory; no corner left is a failure the generator reports.
- Generation uses integers only, and the seed is its only source of randomness, so the same seed and tables give the same region. The draws come from the same generator as the M1 scenario's (`core/random.cpp`).
- Tests over 200 seeds show every village on passable cells in its owner's territory, no two overlapping, and a fourth faction added to the table alone generated with territory and villages.

## Rationale

- Content as data tables, nearest-site partitions, capitals spread by best candidate, and villages placed among every corner that fits: ADR 0009 (`adr/0009-generate-a-region-from-data-tables-by-nearest-site-partitions.md`).
- Integers only, with the seed as the only randomness, for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`).

## Open questions

- **Whether territory should follow terrain.** Territory ignores terrain, so a border can cross a mountain range, and a faction's territory can be mostly impassable. No seed tested lacks room for a village, but nothing guarantees it.
- **How large a region the game needs.** The region is 96 cells square, a size chosen for this task and not measured against play.
