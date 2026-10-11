# ADR 0047: Author one continuous map of 4,096 cells by hand, at a li a cell, and stream it by area

> Status: proposed; its map identity of pack, version and compiled hash, with a refusal of its own, superseded by ADR-0049, which folds it into the providing pack's identity and `PACKS_DIFFER`; its bound of 256 natural objects an area superseded by ADR-0056, which places 64 slots an area; its every drop a blocked cliff and its core reading no elevation superseded by ADR-0058, under which a one-level drop is a step walked, higher cliffs are leapt, and the core holds a level per cell; its terrain unchanged in play, its core reading no elevation, its save holding only the map's identity and its 8 MiB core layer superseded by ADR-0057, which adds elevation and a delta layer; its single speed for every person on a travel class, and its horses and boats left to a follow-up, superseded by ADR-0061, which sets speeds per means and designs horses, boats and fares
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner revised #173's world on 2026-10-09, in issue #201 and its comments:
- One continuous map, with no transitions between regional maps. Ranges and rivers block travel, and passes, fords and ferries are places one walks through. Distances are physical.
- An offline game.
- A hand-made map, the same in every game. No seed shapes the terrain. The simulation's randomness stays seeded, so runs still reproduce.
- 4,096 x 4,096 cells. At 1x it should take about 36 minutes to walk straight from edge to edge, about 12 minutes to cross a region, and about 6 minutes from Luoyang to Chang'an.
- Map data stored by 32-cell area, so render chunks (#200) and tiers (ADR 0042) load per area. The minimap and the map window draw from prebuilt downscaled images.
- A quality profile per device, with the same content everywhere (ADR 0048).
- Villages grow WorldBox-style (#169, #187), so the map places only the starting cities and major towns.
- The Demonic Cult's seat on Tianshan in the far west, as Korean wuxia places it.

ADR 0041, proposed and never built, generated a 512-cell world from a table of features moved by the seed. It rejected one hand-drawn map because every world would be the same, and the owner now asks for exactly that. ADRs 0042 to 0045 assumed 512 cells and 256 areas. At 4,096 cells there are 16,777,216 cells and 16,384 areas.

Figures this ADR rests on:
- **Walking speed.** `SPEED` is 16 units a tick (`core/world.hpp`), `CELL_SIZE` is 256 units (`core/map.hpp`), and there are 30 ticks a second (`core/m1_scenario.hpp`). That is 1.875 cells a second at 1x. A day is 1,800 ticks (`world-generation.md`), 60 seconds at 1x. Walking 4,096 cells takes 36.4 minutes, as the owner said.
- **Real distances.** `routes.py` on branch `evidence/hand-made-map` holds a draft placement of the anchor places in cells, with each place's real longitude and latitude. It prints each key route's length in cells and its real great-circle length in kilometres.
- **On-disk size.** `proxy.py` on the same branch builds a synthetic compiled map of four bytes a cell, from macro layers at one value per 8 x 8 cells with dithered borders, rivers and roads. Compressed per area with deflate, the whole map came to 2.75 MiB. A noisier variant came to 5.18 MiB. Raw, it is 64 MiB.
- **Render memory.** PR #204, round 2 at 6a879ae, streamed the 2.5D mockup by 32-cell chunk. A chunk's ground image is 802,816 bytes, and at most 12 chunks were resident while panning at 3x.
- **Geography.** The PM's map research for #201 of 2026-10-09 is the source for the gates and seats below. Its real-world sources are the Wikipedia articles on Tong Pass, Hangu Pass, Jianmen Pass, Jiayu Pass, Shanhai Pass, Yanmen Pass, Juyong Pass, Mei Pass, the Three Gorges, the Yellow River, the Qinling–Huaihe Line and the Hexi Corridor.

Options weighed:

- **How the map is made.** Macro layers are authored at a coarse resolution. A deterministic build-time tool fills in fine detail. Cell-level patches cover set pieces such as cities, seats and passes. Chosen.
  - Rejected: painting all 16.8 million cells in a tile editor. No one can place that many by hand, and every revision of a range would mean repainting its cells.
  - Rejected: ADR 0041's generation from a seed. The owner decided against it.
  - Rejected: running the same detail tool when the game starts, with a fixed salt and no seed. Every player would still get the same map, but every start would pay the build's time, slowest on the weakest device, and the core would carry the tool. As the owner noted on #201, generating at runtime lowers no runtime cost.
- **Whether the compiled map is committed.** The authored sources are committed. The compiled map is built from them, reproducible bit-for-bit, and a test pins its hash, so any change to a source or to the tool shows in review as a changed hash. Chosen.
  - Rejected: committing the compiled map. Each edit would add megabytes of binary that no reviewer can read, and the repository would keep every version.
- **The distance scale.** A cell is a li of 500 metres between places in the heartland: the Central Plains, Guanzhong, the south of Hebei and Shanxi, the middle Yangtze, the Sichuan basin and Jiangnan. The outer lands are compressed toward the frame. Chosen. In `routes.py` the heartland's routes come to 0.49 to 0.52 km a cell. Liaodong, the far south-west and Lingnan come to 0.66 to 0.74, and the far west to 0.97.
  - Rejected: one scale for the whole map. Tianshan to Suzhou is 3,165 km. Fitting it inside the frame takes about 0.88 km a cell, which puts Luoyang and Chang'an 370 cells apart, 3.3 minutes instead of the owner's 6.
  - Rejected: a li a cell everywhere. Tianshan to Suzhou alone would need 6,330 cells.
  - The scale measures distance between places, not the size of what stands on a cell. A cell is drawn as a 16-pixel tile a person fills, and a village keeps its footprint in cells. The li is how far apart places are and how long the walk between them takes.
- **Layers.** Each layer below says where it is authored and what the compiled map holds.
  - Elevation: a tier from 0 to 7, painted at one value per 8 x 8 cells. The tool adds small relief, and cliffs and slopes where tiers meet.
  - Ground kind: one byte a cell, such as plain, loess, paddy, sand, snow, cliff, slope, road, bridge, ford, river, lake or sea. Each kind's footing and travel class are data, as ADR 0024 gives footing.
  - Biome: painted at 8 x 8 cells. The compiled map holds a per-cell blend for drawing.
  - Rivers, lakes and coast: rivers are polylines from source to mouth, with a width and whether boats use them. Lakes and coast are polygons. Rivers carry their direction of flow.
  - Roads: polylines between named places, with a class of highway, road or track.
  - Passes, fords, bridges and ferries: points on a range or a river with a width, as data the tool carves.
  - Regions: painted at 8 x 8 cells, each a named land with its real basis.
  - Named places: cities, major towns, sect seats and landmarks. Each is a point with a name, and a city, town or seat also has a layout patch, an owning faction and its starting households.
- **Elevation and the 2.5D look.** Tiers give the 3/4 view its height. #204 drew raised hills 8 pixels high with a step face. The core never reads elevation:
  - Where a tier drops, the tool makes the upper rim cell a cliff kind with blocked footing.
  - A slope kind joins two tiers and is passable.

  So movement keeps one passability per cell, ADR 0037's corner rule holds over cliffs as over walls, and tiers can change in art without changing a rule.
- **Impassable ranges.** Tiers 6 and 7 are high-mountain and peak kinds with blocked footing, and deep water blocks people on foot. A range is a band of those cells, and it is crossed only where a pass carves a corridor of passable kinds through it, at the pass's authored width. Examples are Jianmen at 2 to 3 cells, and Tong Pass at 6 to 10 cells between the river and the cliff. Rejected: blocking travel at region borders with an invisible edge. The owner wants the ranges themselves to block.
- **Crossing water.**
  - A ford is a band of shallow-water kind across a river, passable at a slower travel class.
  - A bridge is a road kind over water.
  - A ferry is a pair of landings with a crossing time, both data. The water between them stays blocked to people on foot. A person reaching one landing bound across waits out the crossing time, then stands on the open cell nearest the other landing. That is one rule for every person, steered or not. In the coarse graph, a ferry is an edge between the landings' components at the crossing time's cost.
- **Regions and gates.** There are ten regions, each joined to its neighbours only through the gates in this table, which is data the validator reads:

  | Gate | Joins | Kind |
  |---|---|---|
  | Tong Pass, with Hangu Pass behind it | 중원 (Central Plains) – 관중 (Guanzhong) | corridor between the river and the cliff |
  | Mengjin ferry | 중원 – 하북·산서 (Hebei–Shanxi) | ferry |
  | Pujin ferry | 관중 – 하북·산서 | ferry |
  | Nanyang gap | 중원 – 호광 (Huguang) | low saddle |
  | Huai fords and the Grand Canal | 중원 – 강남 (Jiangnan) | fords, and canal bridges |
  | Jianmen Pass, on the Shu road | 관중 – 사천 (Sichuan) | cleft with a plank road |
  | Wu Pass | 관중 – 호광 | mountain pass |
  | Longshan road | 관중 – 서역 (Western Regions) | hill road |
  | Three Gorges | 사천 – 호광 | river gorge with a plank trail |
  | Southern road | 사천 – 운남·남만 (Yunnan and the southern barbarians) | mountain road |
  | Yangtze banks | 호광 – 강남 | open river valley |
  | Meiling Pass | 호광 and 강남 – 운남·남만 | mountain pass |
  | Yanmen Pass and Juyong Pass | 하북·산서 – 새북 (beyond the northern wall) | wall gates |
  | Shanhai Pass | 하북·산서 – 요동 (Liaodong) | wall gate on the coast |
  | Oasis route | 서역 – 새북 | desert crossing |
  | Jiayu Pass | within 서역, between the Hexi Corridor and the Cult's lands | wall gate |

  The Taihang passes lie within 하북·산서, with Jingxing the cart road.
- **Seats and places.** The first map seats today's three sects:
  - Hwasan on Hua Shan in 관중, at the Tong gate
  - the Black Fang in the Taihang in 하북·산서
  - the Heavenly Demon on Tianshan in 서역, behind Jiayu

  Other famous mountains, such as Song Shan, Wudang, Emei and Kunlun, stand as landmarks until their sects exist as data. The map places only the starting cities, major towns and seats, about 3 to 6 in each heartland region and 1 to 3 in each frontier. Growth places every other village (#169, #187).
- **Validation.** The build tool refuses a map unless all of these hold, and a test runs the same checks on the compiled map:
  - Every named place stands on passable cells and reaches every other through the coarse graph, ferries included.
  - For each pair of regions, taking away the cells of the gates that join them disconnects them, and with the gates in place they connect. So no range leaks, and no gate is shut.
  - Every pass and ford is open at its authored width.
  - Every river runs unbroken from source to mouth. People on foot cross it only at fords, bridges and ferries.
  - Every tier change is a cliff or a slope.
  - The frame is impassable at least 128 cells deep, sea, ranges or desert, so the camera never meets the edge.
  - Each key route's shortest path is no shorter than its designed length, and at most 20% longer, so the scale still holds after an edit.
- **What the core holds, and what streams.** The core holds, for the whole map:
  - a nibble a cell of footing and travel class, 8 MiB
  - the biome and region of each 8 x 8 block, 256 KiB each
  - the coarse graph of ADR 0043

  Daily rules are untiered (ADR 0042), so growth, founding and the spread of natural objects read cells and biomes anywhere, and the far tier walks the graph. The detail layers serve only drawing, so the view streams them by area, 4,096 bytes an area raw, with the render chunks around the camera. The view never reads them from the core.
  - Rejected: streaming the core's layer too. That saves 8 MiB, but a daily rule touching a far area would then load it from disk in the middle of a tick, through a port on the hot path.
  - Rejected: a byte a cell for every layer in the core. That is 64 MiB, mostly for drawing.
- **The coarse graph is built with the map.** The map is fixed, so the tool builds the bare map's coarse graph (ADR 0043) and ships it. A world, when created or loaded, rebuilds only the areas its structures touch. Rejected: building the graph over 16.8 million cells at every start, as ADR 0043 did at 512 cells.
- **Saves.** Terrain never changes in play: structures, rubble and natural objects are separate state. So a save holds the map's identity, its pack and version and the compiled map's hash, in place of ADR 0044's runs. A save whose map hash differs from the loaded map's is refused with a typed error, as ADR 0030 refuses another version.
  - Rejected: saving the map in runs. Nothing in a save could differ from the content.
- **Travel.** Each ground kind has a travel class as data. A step's cost in ADR 0037's 8-connected field is the class's cost: 5 and 7 on open and rough ground as today, 4 and 6 on roads and bridges, and 10 and 14 on fords. A person's speed on a cell is `SPEED` scaled by the class: 1.25 on roads, 0.5 on fords and 1 elsewhere. So people prefer roads, as travellers do. Ferries are above.
  - Horses and river boats are named here, and their rules wait for a follow-up issue: who owns a mount, boarding at a landing, fares. As starting data for that issue, a horse travels at 2.0 on open ground and 2.5 on roads. A boat travels at 2.5 downstream and 0.8 upstream on rivers marked for boats, the Yangtze and the Grand Canal among them.
- **Content packs (#202).** The authored sources and the compiled map are one entry of the base content pack, loaded through the pack mechanism #202 designs. The core receives the map's layers in its own terms, as it receives a region's inputs today, so it reads no file.

Key routes. Length is the sum of the route's legs through its gates, in cells, and so in li in the heartland. Real is the great-circle length of the same legs. Times are minutes at 1x, and a minute at 1x is a game day:

| Route | Length (cells) | Real (km) | On foot, open ground | On foot, by road | Mounted, by road | By boat |
|---|---|---|---|---|---|---|
| Luoyang–Chang'an, by Hangu and Tong | 650 | 320 | 5.8 | 4.6 | 2.3 | — |
| Luoyang–Kaifeng | 340 | 170 | 3.0 | 2.4 | 1.2 | — |
| Luoyang–Song Shan (Shaolin) | 90 | 50 | 0.8 | 0.6 | 0.3 | — |
| Chang'an–Hua Shan | 210 | 110 | 1.9 | 1.5 | 0.7 | — |
| Chang'an–Chengdu, by Jianmen | 1,220 | 620 | 10.8 | 8.7 | 4.3 | — |
| Luoyang–Taiyuan, by Mengjin and a Taihang pass | 720 | 360 | 6.4 | 5.1 | 2.6 | — |
| Taiyuan–Yanjing, by Jingxing | 870 | 420 | 7.7 | 6.2 | 3.1 | — |
| Yanjing–Liaoyang, by Shanhai | 820 | 610 | 7.3 | 5.8 | 2.9 | — |
| Luoyang–Wuchang, by Nanyang | 1,100 | 560 | 9.8 | 7.8 | 3.9 | — |
| Xiangyang–Wudang | 220 | 110 | 2.0 | 1.6 | 0.8 | — |
| Chongqing–Wuchang, by the Three Gorges | 1,500 | 790 | 13.3 | 10.7 | 5.3 | 5.3 down, 16.7 up |
| Wuchang–Nanjing, along the Yangtze | 880 | 460 | 7.8 | 6.3 | 3.1 | 3.1 down, 9.8 up |
| Kaifeng–Yangzhou, by the Huai fords | 1,070 | 550 | 9.5 | 7.6 | 3.8 | — |
| Nanjing–Suzhou–Hangzhou | 620 | 310 | 5.5 | 4.4 | 2.2 | — |
| Chang'an–Tianshan, by Kongtong, Liangzhou and Jiayu | 2,160 | 2,080 | 19.2 | 15.4 | 7.7 | — |
| Chengdu–Dali | 1,000 | 680 | 8.9 | 7.1 | 3.6 | — |
| Wuchang–Guangzhou, by Changsha and Meiling | 1,370 | 910 | 12.2 | 9.7 | 4.9 | — |

Across the frame, 3,840 cells, takes 34 minutes on foot over open ground.

## Decision

The world is one hand-made map of 4,096 x 4,096 cells, the same in every game, with no seed shaping its terrain. A cell is a li of 500 metres between places in the heartland, and the outer lands are compressed toward an impassable frame at least 128 cells deep.
- **Authoring.** Elevation tiers, biomes and regions are painted at one value per 8 x 8 cells. Rivers, lakes, coast, roads, passes, fords, bridges, ferries and named places are authored as data, and set pieces as cell-level patches. A deterministic build-time tool compiles them into four bytes a cell, stored by 32-cell area, with minimap images and the bare map's coarse graph. The compiled map is reproducible bit-for-bit and its hash is pinned.
- **Validation.** It refuses any map whose places do not reach one another, whose regions join other than through their gate table, or whose key routes fall outside their designed lengths.
- **What the core holds.** Footing and travel class at a nibble a cell for the whole map, with biome and region by 8 x 8 block. The view streams the detail layers by area.
- **Movement.** Ranges are bands of blocked kinds crossed at carved passes. Rivers are crossed at fords, bridges and ferries, a ferry by one rule for everyone. Roads are faster by their travel class.
- **Saves.** A save holds the map's identity, not its cells.
- **Supersessions.** This supersedes ADR 0041 whole. It also supersedes ADR 0043's coarse graph built at the world's creation, ADR 0044's map saved in runs, ADR 0037's single step cost for cells of another travel class, and ADR 0030's region regenerated from its seed on load. ADR 0013's 128-cell region and ADR 0009's terrain from sites stay until the map lands, and the pull request that builds it supersedes them.

## Consequences

- **Content work.** Painting the macro layers, writing the place, gate and route data, and patching cities, seats and passes is art and design work. It goes in a pull request of its own, with the draft placement in `routes.py` as its start.
- **The build tool** is a C++ program built by CMake beside the core. It shares the core's integer map code, so the footing it writes is the footing the core reads.
- **The game's export builds the map.** A test pins the compiled map's hash, so an unintended change to the tool or a source fails it.
- **Memory and disk.**
  - The core holds about 8.5 MiB of map layers and the coarse graph, which its implementing pull request measures.
  - The view holds the streamed chunks: 12 or fewer at 3x, about 9.6 MB of ground images by #204's figure, and 4 KiB of detail data each.
  - On disk, the compiled map is projected at 2.8 to 5.2 MiB, and the minimap images add a few more.
- **ADR 0042's tiers** cover 16,384 areas. ADR 0048 makes their radii a per-device profile.
- **ADR 0043's A\*** runs over a graph estimated at about 100,000 nodes at this size. Its implementing pull request measures a path across the map. If one path costs more than a frame, it adds a level of regions above the areas.
- **Natural objects (#187).** ADR 0041's bound of 256 objects an area allows 4.2 million objects at 4,096 cells. That is about 32 MiB in a save at ADR 0044's 8 bytes each. #187 lowers the bound or stores far areas' objects as counts, and keeps saved object state within 8 MiB. The build tool places the initial objects, so a new world's objects come from the map, not the seed.
- **Territory** is the claims of villages, as ADR 0023 gives it and #169 and #187 grow it. No faction territory is partitioned at creation, which retires ADR 0041's territory by path cost.
- **Saves of a 128-cell region** are refused when the map lands, as ADR 0044 planned.
- **Plan of the implementation pull requests (#173).**
  - The coarse graph and area fields come first, as planned.
  - Step 3, "Generate the 512-cell Central Plains", becomes three steps:
    - the map format, compiler and validator, on a small test map
    - the travel classes and the ferry rule
    - the first map's content
  - Step 3's acceptance is replaced by the validator's checks, the pinned hash, and loading within one second on the development Mac.
  - The tiers, the far tier and the dead's compaction follow unchanged, at 16,384 areas.
  - The bench and the bound (step 7) move to ADR 0048's procedure.
- **Dependent issues.**
  - #168's explored map is 16,777,216 bits, 2 MiB, at a bit a cell, or 32 KiB at a bit per 8 x 8 block. That issue chooses between them.
  - #172's roads are authored, and their speed is decided here.
  - A follow-up issue is needed for horses and river boats.

## Signs the decision was wrong

- Painting at 8 x 8 cells cannot draw a feature the owner asks for, so patches spread until most of the map is patched by hand.
- The build tool's output differs between hosts, so the pinned hash fails on a machine that changed nothing.
- The compiled map exceeds 16 MiB on disk, or loading it misses a second on the development Mac.
- Players name the compressed outer lands as too short to cross, or the heartland as too long to cross with nothing in it.
- A rule needs to change terrain in play, such as floods or digging a canal, so the map must be saved after all.
- People queue at a ferry, or at a pass a few cells wide, enough that a war's armies jam there in a way the far tier does not show.
