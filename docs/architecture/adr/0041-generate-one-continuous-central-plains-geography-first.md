# ADR 0041: Generate one continuous Central Plains of 512 cells square, geography first

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The user decided on 2026-10-08 (issue #173, `docs/concept.md` §World and scale) that the map is too small and looks cut off. The game needs one continuous world of about 512 x 512 cells, shaped like the Central Plains: a great river, famous mountains, plains, cities and roads, closed by natural borders. Each sect's seat stands on a famous mountain.

The region today, at `origin/main` 1d943f0:
- It is 128 cells square (ADR 0013), and its edge is a hard cut.
- Terrain is a nearest-site partition of sites at random cells, each of a kind drawn by weight (ADR 0009). No river, range or coast exists as such, and two seeds share no landmark.
- Territory ignores terrain (`world-generation.md`, open question).
- A seat stands at a corner drawn in its faction's territory.

Measured on the development Mac (Apple M1, 8 GB, macOS, on AC power, sharing it with other workers' test runs at a load average of 7 to 9) by the `measure` program on branch `evidence/central-plains`. The program links a copy of `core/` at 1d943f0 with `REGION_SIDE` raised. Generating the region with today's tables took 4.6 to 5.7 ms at 128 cells, seeds 2026 and 7, and 77 to 88 ms at 512 cells, seed 2026. At 512 cells the tables still placed only ten villages, seats included, so placement does not scale with area by itself.

Options weighed:

- **Size and shape.** One map of 512 x 512 cells. Chosen, as the user decided.
  - Rejected: sixteen 128-cell regions generated apart and joined. Rivers, ranges and territories would end at the seams, and the seams would be the hard edge again.
  - Rejected: a world streamed without end. Its save would have no bound, and the jianghu is a closed place.
- **Macro geography.** Features come first, from a geography table that is data:
  - the borders: sea, ranges and desert
  - the great rivers, each a course from a source to the sea, with its crossings
  - the named mountains
  - the cities
  - the plains between them

  Each feature's place is given in thousandths of the side and moved by the seed within a radius the table gives. Every seed is then recognisably the Central Plains: the sea in the east, ranges in the west and south, desert in the north, rivers flowing east. No two seeds are the same in detail. Chosen.
  - Rejected: a noise heightfield with rivers traced downhill. It needs a noise implementation in integers and thresholds per kind. It gives no named mountain or city at a place the game can name, and two seeds share no landmark.
  - Rejected: one hand-drawn map. Every world would be the same, and every change to the map would be art work.
- **Natural borders.** Every cell within the table's border width of the edge, 16 cells to start with, is impassable: sea, a range or deep desert. So no one reaches the edge and the edge is never seen as a cut. Rough ground, foothills and desert, lies inside the border band.
  - Rejected: a passable edge that refuses movement. The edge would still be visible.
- **Filling the land.** The cells no feature claims take a biome by today's nearest-site partition (ADR 0009). Each zone of the geography has its own weights: grassland along the rivers, woodland and hills toward the ranges. A terrain kind is a biome: ground with a footing, as ADR 0024 gives it, and no trees or rocks of its own (below). Roads are least-cost paths over the filled terrain from each city to its neighbours and to each seat, crossing rivers only at the crossings. A road is a terrain kind with buildable footing that growth keeps clear, as it keeps a clearing clear (ADR 0023). Movement costs stay ADR 0037's on every kind. Whether a road speeds travel is for #172.
- **Territory.** Each faction's sites are its cities and its sects' mountains, as data. A cell belongs to the faction whose nearest site is nearest by path cost over the coarse graph of ADR 0043, not by straight distance. Borders then follow ranges and rivers. This answers `world-generation.md`'s open question on whether territory should follow terrain.
  - Rejected: keeping straight-distance territory. A faction's land would cross a range it cannot walk over.
- **Natural objects.** The user decided on 2026-10-09 (issue #187) that terrain is biome ground. Trees, rocks and fruit trees are natural objects that grow and spread on it, and can be felled, mined or picked. Their rules, gathering and building from materials are #187's design. The world reserves their place:
  - A natural object is a record of a kind, a cell and a stage. Each kind is data with a biome affinity, as a terrain kind is data. Today's forest becomes a woodland biome whose trees are objects, and the landforms drawn on hills become rock objects in the same way.
  - Generation seeds objects after the land is filled, by each kind's density on each biome, drawn by the seed.
  - Each area of ADR 0042 holds at most a bound of objects, 256 to start with, a quarter of its 1,024 cells. So the world holds at most 65,536, whatever growth and spread do.
  - Whether an object blocks movement is its kind's data. A blocking object enters the coarse graph and the area fields as a blocking structure does (ADR 0043).
  - Measured by `measure terrain` on `evidence/central-plains` over seeds 2026, 7, 24, 0 and 1 of today's tables at 512 cells: forest covers 22,327 to 59,500 cells and hills 13,280 to 60,254. At most one object per such cell would put 45,890 to 119,754 objects on the land, so the area bound decides the count, not the biomes.
  - Rejected: forest as a terrain kind that can be cleared, by rewriting a cell's kind. #187 asks for trees that grow, spread and are felled one by one, and a kind per cell cannot hold a stage.
- **Villages and seats.** Seats come before villages. Each sect names its mountain in the table. Its seat template is laid on a buildable shelf the generator keeps at that mountain's foot, on the side its faction holds. Villages come after: placed within their owner's territory on buildable cells, as many as the table's density per thousand buildable cells gives, with spacing between them. The spacing and the forms villages take are #169's. The starting population therefore grows with the land and not with a fixed six to eight villages.
  - Rejected: seats drawn at a corner of territory as today. A sect would not stand on its mountain.

## Decision

The world is one generated map of 512 x 512 cells. Generation lays the macro geography first, from a geography table of features placed in thousandths of the side and moved by the seed:
- natural borders of sea, ranges and desert, impassable within a border band
- great rivers from source to sea, crossed only at their crossings
- named mountains, each a sect's seat or a landmark
- cities

It then fills the remaining land with biomes by zone-weighted nearest-site partitions, and seeds natural objects on them by kind and biome, at most 256 to a 32-cell area. It draws roads between cities and seats, and gives territory to each faction by path cost from its cities and mountains. Seats go on their mountains, then villages within territories by a density in the data. Integers only, and the seed is the only randomness, as ADR 0009 requires. This would supersede ADR 0013's 128-cell region and ADR 0009's terrain from sites alone. ADR 0009's tables, partitions as the fill and village placement among every corner that fits stay.

## Consequences

- The region tables grow a geography table, new terrain kinds (sea, river, crossing, desert, deep desert, road), a table of natural object kinds and a mountain named by each sect. Forest stops being a terrain kind and becomes a biome and its trees. The three sects of today are seated on Hwasan, the Ten Thousand Mountains (Sipmandaesan) and the Taihang range. The game's first tables give those, and further named mountains stand as landmarks.
- Generation costs more than the 77 to 88 ms measured with today's tables, by the geography and the road search. The implementing pull request measures it against 1 s on the development Mac.
- Tests over 200 seeds at 512 cells take sixteen times the area. The implementing pull request states how many seeds its tests generate and why that number suffices.
- Every flow field over the whole map would cost 3 MB and 60 to 127 ms to build at 512 cells (ADR 0043). Generation at 512 cells therefore lands after the coarse graph and area fields, or worlds are created too slowly to play.
- `make sweep`'s numbers change with the start population. The growth data is retuned by the measuring pull request of ADR 0045, not here.
- The view draws a 512-cell map: the terrain, a camera kept off the border band, and the explored map of #168.

## Signs the decision was wrong

- Seeds look alike enough in play that the user names the world as the same map every time, or unlike enough that the Central Plains are not recognisable.
- A faction's territory, bounded by ranges and rivers, leaves it no room for its villages or its seat on some seeds.
- Roads and rivers cut the land so finely that villages lose room they had on the 128-cell region.
- Generation takes longer than a load can hide on the development Mac.
