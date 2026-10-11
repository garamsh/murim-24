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
- A sect's seat is a village laid out from the seat template in its faction's territory, after the other villages. Its founders are the seat's residents, the first of them its master. The seat's court, its path to the gate and the ground before the gate are its clearings, which growth keeps clear.

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

### Villages

- A village template is a name, its area, and its buildings, each a structure with a footprint, an offset, a kind, and the households the village starts with in it. A template's buildings lie inside its area, do not overlap, leave its centre free and house at least one household.
- A village has a name, its template's, an owner, its area, its structures, its households and a resident army of two people for each household, standing nearest its centre and bound for it; their home is the village.
- The game's two templates, the village and the hamlet, follow the approved target mockup (`art/mockup/target_screen.lua`).
- Wells, barrels and the fence are structures, so they block movement and break like the houses. The mockup's fields are structures of the `field` kind, which block no movement and burn under a torch.
- The paving, road and banners never block or break; they are presentation data in `world_art/villages.json`, keyed by template name.
- Each faction owns villages in turn, and each sect's seat is laid out after them. Each takes a template drawn by the seed among those with room, and a corner drawn among every corner where its area lies on free buildable cells of its owner's territory, as `Map::buildable` reads the map rows. No template with room is a failure the generator reports.
- `born_in` makes an army of one person born into a village, of its faction and with its residents' skills and health, making the village its home from creation as a rebirth does. The player's body is born this way.

## Rationale

- Templates of footprints from the approved mockup, a template drawn among those with room, and the region at 128 cells: ADR 0013 (`adr/0013-give-structures-footprints-that-block-movement-in-every-stage.md`).
- Each terrain kind's footing as data, read by one test for villages and growth alike, and the plain's weight of 10: ADR 0024 (`adr/0024-build-only-on-terrain-whose-kind-bears-buildings.md`). Over seeds 0 to 199, plain and forest buildable at that weight leave every seed room for its villages with three factions and with four; the plain alone at its old weight of 4 left 26 and 44 seeds without, and plain and forest at 4 left 2 and 4.
- Content as data tables, nearest-site partitions, capitals spread by best candidate, and villages placed among every corner that fits: ADR 0009 (`adr/0009-generate-a-region-from-data-tables-by-nearest-site-partitions.md`).
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
- **Room for seats with more factions.** Every one of 200 seeds has room for the three seats with three factions. A fourth faction added as data, with the sects unchanged, leaves a seat no room on 5 of 200 seeds, so the test that adds one does so without sects.
- **Whether territory should follow terrain.** Territory ignores terrain, so a border can cross a mountain range, and a faction's territory can be mostly impassable. No seed tested lacks room for a village, but nothing guarantees it. ADR 0047, proposed, ends the question with the hand-made map: no faction territory is partitioned when a world is created, and territory is the claims of villages, as ADR 0023 gives it.
- **How large a region the game needs.** The region is 128 cells square, the smallest size tested at which every one of 200 seeds held room for its villages with four factions; it is not measured against play. The owner decided on 2026-10-09 on one hand-made world of 4,096 cells square (`docs/concept.md` §World and scale).
- **Village spacing and forms.** ADR 0076 (`adr/0076-keep-villages-a-spacing-apart-and-grow-each-ones-form-from-its-square-by-the-ground-it-stands-on.md`), proposed; nothing above changes until the pull request that builds it lands. Generation draws each starting village and seat only among corners whose centre lies `apart` cells from every village placed, and lays each village's core, then places what it starts with by the growth's roles. A template becomes the list of what a village starts with, not where each building stands. Measured on seeds 0 to 49 at `006d15d`, the median nearest village is 21.2 cells at year 0 and 15.2 at year 100, below the 16-cell claim.
- **How the hand-made world is made.** ADR 0047 (`adr/0047-author-one-continuous-map-of-4096-cells-by-hand-at-a-li-a-cell.md`), proposed; nothing above changes until the pull requests that build it land. It supersedes ADR 0041's generated 512-cell world, which was never built.
- **The region once the map is built.** ADR 0077 (`adr/0077-move-play-to-the-8192-cell-map-in-ordered-steps-keep-the-region-as-the-rules-fixture-and-hold-the-tiers-to-an-all-live-run.md`), proposed. `generate_region` stays as the rules' fixture, for the core's tests and `make sweep`, while play, the bench's bound and the view move to the map. The map's validator refuses authored settlements closer than ADR 0076's `apart`.
  - **The map.** One map of 4,096 x 4,096 cells, the same in every game; no seed shapes its terrain, and the seed draws only the simulation's randomness. A cell is a li of 500 metres between places in the heartland, and the outer lands are compressed inside an impassable frame at least 128 cells deep.
  - **Authoring.** Elevation tiers, biomes and regions are painted at one value per 8 x 8 cells. Rivers, lakes, coast, roads, passes, fords, bridges, ferries and named places are data, and cities, seats and passes are patched cell by cell. A deterministic build-time tool compiles them into four bytes a cell by 32-cell area, with the minimap images and the bare map's coarse graph, reproducible bit-for-bit with its hash pinned by a test.
  - **Validation.** The tool and a test refuse a map whose named places do not all reach one another, whose regions join other than through their gates, or whose key routes fall outside their designed lengths.
  - **What the map places.** It places only the starting cities, major towns and sect seats, with their layouts, owners and households. Growth places every other village (#169, #187). Hwasan stands on Hua Shan, the Black Fang in the Taihang and the Heavenly Demon on Tianshan.
  - **Natural objects.** The tool places the first natural objects, so they come from the map and not the seed.
  - **Its size.** ADR 0070 (`adr/0070-double-the-map-to-8192-cells-of-half-a-li-so-every-journey-takes-twice-as-long.md`), proposed, would make the map 8,192 x 8,192 cells, a cell half a li between places in the heartland, so every route is twice as long. Layout patches keep their person-sized cells, and elevation, biome and region are painted per 16 x 16 cells. Inns are authored on the roads a day's walk apart, and caves in the wilderness. It would supersede the 4,096 cells, the li a cell and the painting per 8 x 8 above.
  - **What it would supersede.** The 128-cell region, terrain and territory by nearest site, and villages drawn among the corners that fit, all above. The saves it makes hold the map's identity, not its cells.
