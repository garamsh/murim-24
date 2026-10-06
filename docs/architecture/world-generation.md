# World generation

How the core generates a region from a seed: what exists of it now, and the decisions it rests on.

## Current decisions

- `core/region.cpp` generates a region from a seed and three data tables: terrain kinds, factions and village templates. `region_data()` returns the game's tables, each defined there once; the generator reads only what it is passed, so a table changes without the generator changing.
- A region is the inputs a `World` is created from (seed, map rows, armies, structures, rules) and what the world does not hold: the terrain kind and owning faction of each cell, and the villages.
- A terrain kind is a name, whether bodies may stand on it, and a weight. The map rows mark the cells of an impassable kind as blocked.
- A faction is a name and the factions it attacks; the rules' hostility matrix is built from those lists.
- A village template is a name, the area it occupies, width x height cells, and its structures, each with a footprint and its top-left cell as an offset within that area. A template's structures lie inside its area, do not overlap, and leave its centre free. A village has a name, an owner, its area, its structures and a resident army at its centre, of a fixed number of agents for each structure.
- The game's two templates follow the approved target mockup (`art/mockup/target_screen.lua`): the village is its 17 x 9 cells, houses of 3 x 3 and a hall of 5 x 3 north of the road through its centre and two houses south of it; the hamlet is its west end, 7 x 9 cells with three houses.
- Terrain and territory each partition the grid by nearest site: terrain around sites at random cells, each of a kind drawn by weight; territory around one capital per faction, each the farthest from those already placed among a fixed number of random cells.
- `born_in` makes an army of one agent born into a village: of the village's faction, on the first cell of its area, row by row, that no structure covers and its residents do not crowd, with their skills and one agent's share of their strength. The player's body is born this way.
- Six to eight villages are owned in turn by each faction. Each takes a template drawn by the seed among those with room, and a corner drawn among every corner where that template's area lies on free passable cells of its owner's territory; a summed-area table of those cells tests each corner. No template with room is a failure the generator reports.
- Generation uses integers only, and the seed is its only source of randomness, so the same seed and tables give the same region. The draws come from the same generator as the M1 scenario's (`core/random.cpp`).
- Tests over 200 seeds show every village on passable cells in its owner's territory, its structures inside it and off its residents' cell, no two villages overlapping, every region a valid world, and a fourth faction added to the table alone generated with territory and villages.

## Rationale

- Templates of footprints from the approved mockup, a template drawn among those with room, and the region at 128 cells: ADR 0013 (`adr/0013-give-structures-footprints-that-block-movement-in-every-stage.md`).
- Content as data tables, nearest-site partitions, capitals spread by best candidate, and villages placed among every corner that fits: ADR 0009 (`adr/0009-generate-a-region-from-data-tables-by-nearest-site-partitions.md`).
- A body born into a village as a one-agent army, the shape the player's body takes: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`).
- Integers only, with the seed as the only randomness, for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`).

## Open questions

- **Whether territory should follow terrain.** Territory ignores terrain, so a border can cross a mountain range, and a faction's territory can be mostly impassable. No seed tested lacks room for a village, but nothing guarantees it.
- **How large a region the game needs.** The region is 128 cells square, the smallest size tested at which every one of 200 seeds held room for its villages with four factions; it is not measured against play.
