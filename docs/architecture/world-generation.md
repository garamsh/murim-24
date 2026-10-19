# World generation

How the core generates a region from a seed: what exists of it now, and the decisions it rests on.

## Current decisions

### Inputs and outputs

- `core/region.cpp` generates a region from a seed and three data tables, terrain kinds, factions and village templates, and carries the rules of life to the world.
- The game's tables are the base content pack's, `packs/wuxia/`, which `load_content` reads and builds into a `RegionData` (ADR 0049, simulation.md). The generator reads only what it is passed, so a table changes without the generator changing.
- A region is the inputs a `World` is created from (seed, map rows, armies, structures, rules, leadership, life) and what the world does not hold: each cell's terrain kind and owning faction, and the villages.
- Generation uses integers only, and the seed is its only source of randomness, so the same seed and tables give the same region. The draws come from the same generator as the M1 scenario's (`core/random.cpp`).
- `village_specs` gives the villages as a `World` is created with them: each village's structures, what each holds, its resident army and its households.

### Terrain and territory

- A terrain kind is a name, its footing and a weight. Its footing is buildable, rough or blocked, and the map rows mark each cell with it. Plain and forest are buildable, hills rough, and water and mountains blocked. Landforms stand only on hills and mountains, so none stands where anything is built.
- Terrain and territory each partition the grid by nearest site: terrain around random sites, each of a kind drawn by weight; territory around one capital per faction, each placed farthest from those already placed among random candidates.

### Factions and sects

- A faction is a name, the factions it attacks, and the skills its armies use; the rules' hostility matrix is built from those lists. A village's residents take their faction's skills, which decide who rebuilds: the Orthodox and Unorthodox guard and mend, and the Demonic Cult guards and torches, rebuilding nothing.
- A faction may carry a succession rule: the Demonic Cult changes leader by duel to the death, and the old leader's allies turn on the winner; the Orthodox and Unorthodox carry none yet.
- Each faction is led at first by the first resident of its first village.
- Each faction carries who its sects admit and whether it elects its leader: the Orthodox and the Demonic Cult admit by talent, the Unorthodox by proven attainment, and the Orthodox alone elect their leader.
- Each faction has one sect, Hwasan for the Orthodox, the Heavenly Demon for the Demonic Cult and the Black Fang for the Unorthodox, each holding the manual of its art: the sword, the saber and hidden weapons.
- A sect's seat is a village laid out from the seat template in its faction's territory. Seats are sited before the villages and laid after them, so the first village's residents still lead their factions. Its founders are the seat's residents, the first of them its master. The seat's court, its path to the gate and the ground before the gate are its clearings, which growth keeps clear.

### Rules of life

- The region's data is held as constants of `core/region.cpp`: the rules of life, five disciplines, sword, saber, fist, hidden weapons and medicine, lifespans, kinship, names in Korean with romanised forms, husbandry, growth and trade; and the rules' rumour reach, talk reach and sight. They are tuned against `make sweep` and the region's tests.
- Each faction carries its growth stance. The Orthodox build first, then found, then raid; the Unorthodox found first, then raid, then build; the Demonic Cult only raids, since its torch raises nothing.
- A hamlet is too small to raise a workshop, and a village raises both.
- `growth_for` builds the growth's plans from the structure kinds, the builds and the settlement template the base pack's rules of life name, the hamlet, and names the hamlet as the kind of village a settlement makes; `generate_region` gives each faction's stance to it.
- A table of structure kinds gives each kind what it holds, housing, storage, harvest, pupils, arms or tends, and whether it blocks: fields and the training ground block nothing.
- Combat is paced by the tables' data against four targets, each measured on villages as full as grown ones:
  - a duel to the death
  - twenty a side on an open field until one side falls
  - a band of raiders leaving a hamlet in ruins
  - a larger band leaving the walled village in ruins

  A raiding band stands on the open cells two cells clear of everything the village holds, nearest its centre by path. With villages laid out by role (ADR 0076), a band set west of a village could start behind water or a range. On 6 of seeds 0 to 19 it then marched round past another village, which killed it before it reached its target. So the band stands nearest by path, and a band of 20 again ruins a hamlet within the target on every one of those seeds.

### Villages

- A village template, the hamlet or the village, is a name and the buildings a village starts with, each a footprint, a kind and the households it starts with, housing at least one household. Where each stands is not in the template: generation places them by their kinds' roles (ADR 0076). The seat template is authored, with an area and each building at an offset in it, laid inside it, apart, its centre free.
- A village has a name, its template's, an owner, its area, which is its first tier's square, or the seat's area, its structures, its households and a resident army of two people for each household, standing on the open cells of its area nearest its centre and bound for it; their home is the village.
- Each structure kind carries its role: the hall, the well, workshops, the temple, the inn and the seat's halls of the square; houses, granaries, storehouses and barrels homes; fields and the fence edges.
- Wells, barrels and the fence are structures, so they block movement and break like the houses. The mockup's fields are structures of the `field` kind, which block no movement and burn under a torch.
- The paving, road and banners never block or break; they are presentation data in `world_art/villages.json`, keyed by template name, from a village's area: the square for a village, the seat's area for a seat.
- Each faction owns villages in turn. How many there are and each one's template are drawn first, in the stream's order from before villages were kept apart, so a seed gives each faction the same templates; seats are sited next. Each village takes a centre among the open sites: where the first tier's square lies on free buildable cells of its owner's territory, as `Map::buildable` reads the map rows, and no village's or seat's centre lies nearer than the growth's `apart`, 20 cells in the base pack. The centre is the drawn one of 16 candidates farthest from the other centres, as capitals are placed. Its buildings are placed by role round the square on its owner's cells, within the last tier's vicinity, trying the next site when they do not all fit. No open site that holds them is a failure the generator reports.
- A seat takes a corner drawn among every corner where its area lies on free buildable cells of its faction's territory, its centre `apart` from the seats before it.
- `born_in` makes an army of one person born into a village, of its faction and with its residents' skills and health, making the village its home from creation as a rebirth does. The player's body is born this way.

### The hand-made map's format

- ADR 0070's and ADR 0077's first step exists: the format, its compiler and its validator. The base pack paints no map, so the region is still the game's only world; a world on a map is described below.
- A pack authors a map in its `map/` documents, read through `PackSource` like its `data/` ones and merged the same way:
  - `map/map` is the one entry of the map: its side, the block it is painted per, and three layers;
  - `map/lands` lists the lands, `map/places` the authored places by cell, and `map/routes` the key routes, each two places and a designed length in cells;
  - a place settles a village, of a faction (`owner`) and a village template (`settlement`), or a sect's seat (`sect`), one or the other.
- Each layer is one string a row of blocks, a character a block:
  - the ground, whose marks a legend gives terrain kinds, so a block takes its kind and the kind's footing; a pack painting a map holds at most 256 terrain kinds, a byte a block;
  - the levels, a digit from `0` to `f` a block;
  - the lands, whose marks a legend gives lands, at most 256.
- The core's bounds, each refused naming its field:
  - the side a power of two from 32 cells, one area, to 8,192, which keeps the 65,536 areas and 1,024 regions of ADR 0070 within theirs;
  - the block 8 or 16 cells;
  - rows of another shape than the side and block give, and marks no legend names.
- Compiling gives every cell its block's footing and level, a byte a cell, and keeps each block's terrain kind and land. The fill of detail between blocks, with relief, cliffs and slopes, rivers, roads and patches, comes with the first map's content (ADR 0077, step 6). The compiled map's hash covers every cell, terrain kind, land and place, and what each place settles.
- The validator refuses, at the entry each fault lies in:
  - a place on a blocked cell;
  - a place nearer than the growth's `apart` to an earlier one, by the test founding keeps villages apart by;
  - a key route whose shortest walk is shorter than its designed length or more than a fifth longer, ADR 0047's tolerance, which ADR 0070 keeps.
- A walk's length is this step's choice, since neither ADR defines it. It goes 8-connected through passable cells, and steps only between levels at most one apart, so it walks and never leaps (ADR 0058). A diagonal step costs 7 to a straight step's 5 and is taken only where both cells beside it could be stepped through. The length is the cost divided by 5, rounded down, as `FlowField` counts a path.
- ADR 0047's other checks wait for the layers and the coarse graph they read: every place reaching every other, regions joined only through their gates, rivers and the frame.
- The bare map's coarse graph ships with it (ADR 0047), so a world on the map does not build it:
  - `murim_map`, the build tool in `packs_json/`, loads the packs, compiles their map, builds the graph of its footing and writes it to `map/graph.bin` of the last pack to author a map document, ignoring any graph the packs already ship; `make export` runs it over the base pack first, and it writes nothing while the packs paint no map;
  - the file is the save's encoding (ADR 0030) under a magic, a format version, the compiled map's hash and a checksum, every index checked against the graph's own areas and regions on read;
  - `load_content` reads that pack's graph, if it ships one, and refuses the merged set at its `map/graph` as `INCONSISTENT` when the graph was built for another map's hash, and as `UNREADABLE` when it cannot be read;
  - a world created or loaded on the map takes the graph and rebuilds only the areas its blocking structures lie in (`simulation.md`).
- A compiled map of 8,192 cells holds 64 MiB of cells and 256 KiB of lands. Loading the base pack with the test map, which compiles, hashes and validates it, took about 145 ms in the Release build on the development Mac, at a load average of 30, measured by `MapContent.ATestMapOf8192CellsCompilesThroughTheBasePackToItsPinnedHash`, which loads two such maps in 283 to 295 ms over three runs. The delta layer, ADR 0070's 128 MiB budget with it full, and its 2 seconds' load wait for the steps that build the delta layer and the first map's content.

### A world on the map

- `settle_map` gives, from a compiled map and the tables, the inputs a `World` is created from, as `generate_region` gives them, so a world on the map is created and runs by the same code as one on the region (ADR 0077, step 2):
  - the map rows are the compiled footing, and each cell's terrain kind the compiled one;
  - no cell is any faction's territory: territory on the map is the claims of villages (ADR 0047);
  - each village place is laid out from its template, its centre the place's cell, its square and start list placed by role round it by the code the region's villages are laid by, over every cell the footing lets it build on, in the places' order;
  - each seat place is laid from the seat template, its area's middle on the place, its area kept from the villages and laid after them, so each faction's first village's first resident leads it, as on the region;
  - the seed draws only the people, since the map places every village.
- A place whose buildings or seat find no room, or a sect with no seat place or more than one, is refused.
- A world on the map takes the map's footing only. Its levels stay in `CompiledMap`, and no rule reads them until ADR 0058's leaps and cliffs are built (#244). Until then a world's people walk across a drop of two levels or more, which the validator's walk treats as a cliff.
- A world on the 8,192-cell map runs in the bench's tiers measure, on the ranged test map built in code (`simulation.md`); its hash and growth still scan every cell. The tests run a world on a compiled map of 256 cells.

## Rationale

- Footprints that block movement and the region at 128 cells: ADR 0013 (`adr/0013-give-structures-footprints-that-block-movement-in-every-stage.md`).
- Villages kept `apart` by one test generation and founding share, start lists placed by role round a square, tiers and terraced fields: ADR 0076 (`adr/0076-keep-villages-a-spacing-apart-and-grow-each-ones-form-from-its-square-by-the-ground-it-stands-on.md`). Its value is measured in `simulation.md`'s rationale for it.
- Each terrain kind's footing as data, read by one test for villages and growth alike, and the plain's weight of 10: ADR 0024 (`adr/0024-build-only-on-terrain-whose-kind-bears-buildings.md`). Over seeds 0 to 199, plain and forest buildable at that weight leave every seed room for its villages with three factions and with four; the plain alone at its old weight of 4 left 26 and 44 seeds without, and plain and forest at 4 left 2 and 4.
- Content as data tables, nearest-site partitions and capitals spread by best candidate: ADR 0009 (`adr/0009-generate-a-region-from-data-tables-by-nearest-site-partitions.md`).
- The rules of life as one table the world is given: ADR 0019 (`adr/0019-draw-talent-and-lifespan-at-birth-and-bear-a-soul-again.md`).
- The rumour reach, and the ring of events rumours are read from: ADR 0032 (`adr/0032-keep-a-bounded-ring-of-world-events-and-read-rumours-from-it.md`).
- Sects, their seats as villages, admission, the election and manuals: ADR 0026 (`adr/0026-keep-sect-membership-on-the-person-and-seat-each-sect-in-a-village.md`). Elders as a third of each sect's members: ADR 0028 (`adr/0028-make-a-sects-elders-the-share-of-its-strongest-members.md`).
- Children of the sex their village's unmarried lack: ADR 0027 (`adr/0027-bear-each-child-of-the-sex-its-village-lacks.md`). Over 100 years with days of five ticks, every faction keeps a peopled village on all of seeds 0 to 49 (42 with sexes at even odds) and on 48 of seeds 50 to 99 (44), and the living at least double on every one.
- Villages starting from households of married couples, each heading a family of its own, and the growth data tuned to start small: ADR 0025 (`adr/0025-start-each-village-from-households-of-married-couples.md`). Over 100 years with days of five ticks, seeds 24, 2026 and 7 go from 12, 14 and 14 living to 97, 123 and 125, and from 6, 7 and 7 peopled villages to 17, 18 and 19; with households but the old growth data, no village is founded.
- Residents as people with the village as their home, a cell apart: ADR 0017 (`adr/0017-make-every-npc-an-individual-and-every-army-a-roster.md`).
- A person born into a village as an army of one, the shape the player's body takes: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`).
- Integers only, with the seed as the only randomness, for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`).

## Open questions

- **The map changed in play.** ADR 0057 (`adr/0057-resolve-world-scale-arts-as-waves-over-area-indexes-absorbed-by-materials-and-reshape-terrain-in-a-conserving-delta-layer.md`), proposed. The core holds an elevation tier per cell, and changed areas hold delta blocks over the authored map, saved with the world. The build tool's cliff-and-slope rule runs again on changed cells. ADR 0047's validation holds for the authored map only.
- **Natural objects on the map.** ADR 0056 (`adr/0056-grow-natural-objects-in-slots-the-map-places-and-build-villages-from-what-people-gather.md`), proposed. ADR 0047's build tool places up to 64 object slots an area, each a cell and a kind chosen from the area's biomes by the kinds' affinities with the tool's fixed salt, 2 MiB in all, so the same map gives the same forests in every game.
- **Room for seats with more factions.** Every one of 200 seeds has room for the three seats with three factions. A fourth faction added as data, with the sects unchanged, leaves a seat no room on 5 of 200 seeds, so the test that adds one does so without sects. With villages kept `apart` 20, all 200 seeds still generate with four factions.
- **Seeds with no room.** Of seeds 0 to 999, 998 generate at `apart` 20: seed 480 leaves a seat no room and 394 a village. `main` at `8b4bb8d7`, before villages were kept apart, refused 5 of them.
- **Whether territory should follow terrain.** Territory ignores terrain, so a border can cross a mountain range, and a faction's territory can be mostly impassable. No seed tested lacks room for a village, but nothing guarantees it. ADR 0047, proposed, ends the question with the hand-made map: no faction territory is partitioned when a world is created, and territory is the claims of villages, as ADR 0023 gives it.
- **How large a region the game needs.** The region is 128 cells square, the smallest size tested at which every one of 200 seeds held room for its villages with four factions; it is not measured against play. The owner decided on 2026-10-09 on one hand-made world of 4,096 cells square (`docs/concept.md` §World and scale).
- **How the hand-made world is made.** ADR 0047 (`adr/0047-author-one-continuous-map-of-4096-cells-by-hand-at-a-li-a-cell.md`), proposed. Its format exists at ADR 0070's size, as described above; nothing else above changes until the pull requests that build the rest land. It supersedes ADR 0041's generated 512-cell world, which was never built.
  - **The map.** One map of 4,096 x 4,096 cells, the same in every game; no seed shapes its terrain, and the seed draws only the simulation's randomness. A cell is a li of 500 metres between places in the heartland, and the outer lands are compressed inside an impassable frame at least 128 cells deep.
  - **Authoring.** Elevation tiers, biomes and regions are painted at one value per 8 x 8 cells. Rivers, lakes, coast, roads, passes, fords, bridges, ferries and named places are data, and cities, seats and passes are patched cell by cell. A deterministic build-time tool compiles them into four bytes a cell by 32-cell area, with the minimap images and the bare map's coarse graph, reproducible bit-for-bit with its hash pinned by a test.
  - **Validation.** The tool and a test refuse a map whose named places do not all reach one another, whose regions join other than through their gates, or whose key routes fall outside their designed lengths.
  - **What the map places.** It places only the starting cities, major towns and sect seats, with their layouts, owners and households. Growth places every other village (#169, #187). Hwasan stands on Hua Shan, the Black Fang in the Taihang and the Heavenly Demon on Tianshan.
  - **Natural objects.** The tool places the first natural objects, so they come from the map and not the seed.
  - **Its size.** ADR 0070 (`adr/0070-double-the-map-to-8192-cells-of-half-a-li-so-every-journey-takes-twice-as-long.md`), proposed, would make the map 8,192 x 8,192 cells, a cell half a li between places in the heartland, so every route is twice as long. Layout patches keep their person-sized cells, and elevation, biome and region are painted per 16 x 16 cells. Inns are authored on the roads a day's walk apart, and caves in the wilderness. It would supersede the 4,096 cells, the li a cell and the painting per 8 x 8 above.
  - **What it would supersede.** The 128-cell region, terrain and territory by nearest site, and villages drawn among the corners that fit, all above. The saves it makes hold the map's identity, not its cells.
- **The region once the map is built.** ADR 0077 (`adr/0077-move-play-to-the-8192-cell-map-in-ordered-steps-keep-the-region-as-the-rules-fixture-and-hold-the-tiers-to-an-all-live-run.md`), proposed. `generate_region` stays as the rules' fixture, for the core's tests and `make sweep`, while play, the bench's bound and the view move to the map. The map's validator refuses authored settlements closer than ADR 0076's `apart`.
