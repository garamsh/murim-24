# ADR 0070: Double the map to 8,192 cells of half a li, so every journey takes twice as long

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-11, in issue #255:
- **The world is vaster,** so travel takes at least twice as long as on ADR 0047's map. There, edge to edge is about 36 minutes on foot at 1x, and Luoyang to Chang'an about 6.
- The owner chose "bigger" over "the same size, emptier" and over "build the 4,096 map sooner".

The issue asks the design to compare the ways to double travel time or more, with numbers, and recommend one:
- (a) 8,192 × 8,192 cells at a li a cell, with the outer lands less compressed
- (b) 4,096 × 4,096 cells, each larger, with what that does to the patch scale of #227 and #242
- (c) other options

For each, it asks for the map's bytes on disk and in memory, the authoring cost by hand with ADR 0047's tools, streaming and the tiers, the live tier's budget, the far tier's cost, save size and load time, and the effect on travel. It also asks what fills the space so it does not feel empty, and for the steps to build it. The bigger map replaces the 4,096 target of #173's steps; it does not add a second map. The mobile quality profiles of ADR 0048 must still fit.

What this builds on, from `main` at `2462921` and the open PRs it cites:
- **ADR 0047,** proposed. One hand-made map of 4,096 × 4,096 cells, 16,777,216 cells in 16,384 areas of 32 cells.
  - A cell is a li, 500 metres, between places in the heartland, and the outer lands are compressed toward an impassable frame at least 128 cells deep.
  - Elevation, biome and region are painted at one value per 8 × 8 cells, and a deterministic build tool compiles four bytes a cell by area, with the bare map's coarse graph. The compiled map was projected at 2.8 to 5.2 MiB on disk, 64 MiB raw.
  - People walk at `SPEED` 16 units a tick over a `CELL_SIZE` of 256, at 30 ticks a second: 1.875 cells a second. Its route table gives Luoyang to Chang'an 650 cells, 5.8 minutes on foot over open ground, and edge to edge 36.4 minutes.
  - A day is 1,800 ticks, 60 seconds at 1x.
  - It names one sign of its own failure that this ADR meets: a compiled map over 16 MiB on disk.
- **ADR 0057,** proposed, merged in #222. The core holds a level nibble beside the footing nibble, a byte a cell, and a delta layer of at most 4,096 areas' blocks, 16 MiB.
- **ADR 0058,** proposed, merged in #223. Levels per cell for leaps and sight.
- **ADR 0056,** proposed, merged in #220. 64 natural-object slots an area, 1,048,576 in all, 2 bytes each at run time, and an 8-day cycle that visits about one area a tick.
- **ADR 0042.** Tiers by distance from the steered, in radii counted in areas: live 1 to 3 areas, stepped 3 to 6, by ADR 0048's profiles.
- **ADR 0043.** A coarse graph of areas' components and entrances, estimated by ADR 0047 at about 100,000 nodes at 4,096 cells. ADR 0047 adds a level of regions above the areas if one path costs more than a frame.
- **ADR 0044.** Saves hold the tiers' state, and ADR 0047 makes a save hold the map's identity, not its cells.
- **ADR 0048.** The population bound is measured on the low profile, on ADR 0047's map, from 10,000 to 50,000.
- **ADR 0059.** The view draws a cell 16 pixels wide and 12 deep and bakes each area's ground once.
- **Open PRs.**
  - #254's ADR 0061 gives travel by foot, horse and boat at speeds per mille of walking, and recomputes ADR 0047's route table.
  - #242, merged as 2462921, records realistic scale: a person fills a cell, a walled town is at least 40 × 30 cells, and its "two scales" bullet says the li measures distance between places while a place's layout patch carries the size of what stands there.
  - ADR 0063, merged in #243, bounds the population by the food that can be had.
  - #241's ADR 0067 puts animals and herds in the wilderness, with a capacity sized on ADR 0056's slots.

Options weighed. Every figure is for the whole map:

| Option | Cells | Luoyang–Chang'an on foot | Edge to edge on foot | Core byte a cell | Compiled raw, and on disk | Slots | Coarse graph |
|---|---|---|---|---|---|---|---|
| ADR 0047 | 4,096², 16.8 M | 650 cells, 5.8 min | 36.4 min | 16 MiB | 64 MiB, 2.8 to 5.2 MiB | 1,048,576, 2 MiB | about 100,000 nodes |
| (a) 8,192² at a li, outer lands less compressed | 8,192², 67.1 M | 650 cells, 5.8 min | 72.8 min | 64 MiB | 256 MiB, 11 to 21 MiB | 4,194,304, 8 MiB | about 400,000 nodes |
| (b) 4,096², larger cells | 4,096² | 650 cells, 11.6 min | 72.8 min | 16 MiB | 64 MiB, 2.8 to 5.2 MiB | 1,048,576, 2 MiB | about 100,000 nodes |
| (c) 8,192² at half a li | 8,192² | 1,300 cells, 11.6 min | 72.8 min | 64 MiB | 256 MiB, 11 to 21 MiB | 4,194,304, 8 MiB | about 400,000 nodes, and a level of regions |
| (d) 6,144² at two thirds of a li | 6,144², 37.7 M | 975 cells, 8.7 min | 54.6 min | 36 MiB | 144 MiB, 6 to 12 MiB | 2,359,296, 4.5 MiB | about 225,000 nodes |

The on-disk figures scale ADR 0047's projection of 2.75 to 5.18 MiB by the cells, and the graph by the areas.

- **(a) A li a cell, the outer lands less compressed.** Rejected. It doubles only what was compressed: the frontier routes lengthen toward their real distances, Chang'an to Tianshan's 2,080 km toward about 4,160 cells, but the heartland, already a li a cell, keeps Luoyang to Chang'an at 5.8 minutes. The owner asked for travel twice as long, and most travel is in the heartland.
- **(b) 4,096 cells, each covering more ground.** Rejected. A cell that covers twice the ground has only two readings, and both fail:
  - **The patch scale shrinks.** If cells really are larger, a person fills half a cell, and #242's merged sizes, a person filling a cell and a walled town of 40 × 30 cells inside its wall, no longer hold at a person's scale.
  - **Or it is walking halved.** If a person still fills a cell, a cell's ground is unchanged, and only travel slows: that is halving `SPEED`. Halving it doubles every journey, at #242's 2-metre cell a realistic 1.9 metres a second, but it halves all movement in live play as well. Crossing a 40 × 30 town takes 43 seconds instead of 21, every chase and closing rule of ADR 0046 and ADR 0066 is retuned, and every pinned hash and sweep changes, for a goal that is only about distances between places.
- **(c) 8,192 cells at half a li.** Chosen. A cell between places becomes half a li, 250 metres, in the heartland, and the outer lands keep ADR 0047's compression relative to it. Every route's length in cells doubles, so every journey takes twice as long at the same walking speed. Live play is unchanged: a person still fills a cell, and a town is still 40 × 30 cells.
- **(d) 6,144 cells at two thirds of a li.** Rejected. It makes travel only 1.5 times as long, short of the owner's "at least twice".
- **Slower travel on open ground only,** by a travel class of 500 per mille for open and rough ground. Rejected. Villages and towns stand on open ground, so it is (b) inside every settlement.
- **The same size, emptier, or the 4,096 map sooner.** The owner ruled both out.

The chosen design, in detail:
- **Two scales, kept.** #242's two scales hold unchanged. The li measures the distance between places, now half a li a cell in the heartland. A layout patch carries the size of what stands there at a person's scale, so a walled town of 40 × 30 cells, a field of 4 to 8 cells and a road of 2 cells keep #242's sizes. Only the land between places doubles.
- **Authoring by hand costs the same.** Elevation, biome and region are painted at one value per 16 × 16 cells, so each layer is still 512 × 512 painted values, as at ADR 0047's 8 × 8. Rivers, roads, passes and places are polylines and points whose coordinates double. Set pieces are cell patches of the same size as before. The build tool fills twice the detail between them, by the same deterministic rules.
  - Rejected: painting at 8 × 8 cells, 1,024 × 1,024 values a layer. It quadruples the hand work for land the tool fills well.
- **The frame** stays impassable at least 128 cells deep: it keeps the camera off the edge, which is a matter of the screen in cells.
- **Validation.** Every key route's designed length doubles, and the check that each shortest path is no shorter and at most 20% longer holds as before.
- **What the core holds.** For the whole map:
  - footing and level, a byte a cell, 64 MiB
  - biome and region per 16 × 16 block, 512 × 512, 256 KiB each, as before
  - the coarse graph with a level of regions, below
  - ADR 0057's delta layer, its bound of 4,096 areas unchanged, now 6% of the areas rather than 25%
- **The coarse graph gains a level of regions.** A region is 8 × 8 areas, 256 cells: 1,024 regions. A path is found by A* over the regions first, then over the areas in the regions it crosses, then by ADR 0043's fields. ADR 0047 named this level as the remedy if one path cost more than a frame. At about 400,000 area nodes and a crossing of 256 areas, it is no longer optional.
- **Areas and slots.** 65,536 areas of 32 cells. ADR 0056's 64 slots an area stay, since trees and rocks are person-scale, so the map holds at most 4,194,304 slots, 8 MiB at run time. ADR 0056's 8-day cycle now visits about 4.6 areas a tick: 65,536 areas over 14,400 ticks, about 290 slots a tick.
- **The tiers** keep their radii in areas (ADR 0042, ADR 0048, and #216, whose ADR 0053 sets the streaming and the live tier), so the live and stepped tiers cost what they did: the live tier's 5,000 and its budget are untouched. Only the far tier holds more areas, and the far tier's daily rules are per village and per army, not per area, beside the slot cycle above.
- **What fills the space.**
  - **Villages.** Growth founds villages between the starting places (#169, #187). Under ADR 0063, four times the land is more forage and more fields, so more people live, up to the measured bound.
  - **Wilderness.** #241's ADR 0067 fills it with herds, predators and spirit beasts' territories, which need ground far from villages.
  - **Inns and outposts.** A new kind of named place, extending ADR 0047's table of named places in step 4, authored on the map's highways and roads about a day's walk apart: 140 cells on a road, a day of 60 seconds at 2.34 cells a second. Luoyang to Chang'an holds about 9. An inn is a small layout patch with a household, so growth may grow it into a village.
  - **Caves.** ADR 0051's caves, which only a steered body opens, are placed in the wilderness by the map's authoring, in ranges and hills far from roads.
- **Travel matters more.** Every route doubles, so #254's horses, at twice walking's speed on open ground and 2.5 times on roads, and its boats, at 2.5 times downstream, halve the longest journeys. Ferries and passes become the places armies and caravans meet.
- **The key routes, doubled.** Minutes at 1x, a minute a game day:

  | Route | Length (cells) | On foot, open ground | On foot, by road | Mounted, by road | By boat |
  |---|---|---|---|---|---|
  | Luoyang–Chang'an, by Hangu and Tong | 1,300 | 11.6 | 9.2 | 4.6 | — |
  | Luoyang–Kaifeng | 680 | 6.0 | 4.8 | 2.4 | — |
  | Chang'an–Chengdu, by Jianmen | 2,440 | 21.7 | 17.4 | 8.7 | — |
  | Chongqing–Wuchang, by the Three Gorges | 3,000 | 26.7 | 21.3 | 10.7 | 10.7 down, 33.3 up |
  | Chang'an–Tianshan, by Kongtong, Liangzhou and Jiayu | 4,320 | 38.4 | 30.7 | 15.4 | — |

  Edge to edge, 8,192 cells, takes 72.8 minutes on foot, and across the frame, 7,936 cells, 70.5. Luoyang to Chang'an on foot is about 12 days, about 27 km a day on its real 320 km, a traveller's pace.
- **The view.** Chunks, bakes and the streaming radius are per area and unchanged (ADR 0059). The minimap and the map window draw from prebuilt images four times as many pixels, built with the map, and ADR 0057's zoomed-out images are four times as many areas.
- **Bounds, with refusals.** These are core bounds. A pack whose map is past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - a map side of at most 8,192 cells, a power of two
  - at most 65,536 areas, so an area's index fits 2 bytes
  - a painted block of 8 or 16 cells
  - at most 1,024 regions
- **Cost, at 500, 5,000 live and 50,000 living.**
  - **Memory.** The core's map is 64 MiB for footing and level, 512 KiB for biome and region, about 6 MiB for a graph of 400,000 nodes at 16 bytes, this ADR's estimate of a node's area, component, entrance range and region, padded, which step 2 measures, and 8 MiB of slots, about 79 MiB, against about 20 MiB at 4,096 cells: 16 MiB, 512 KiB, 1.6 MiB for 100,000 nodes and 2 MiB. With ADR 0057's delta layer full, up to 16 MiB, it is about 95 MiB at worst. This ADR proposes a core budget of 128 MiB on the low profile, measured in step 1, and on mobile under the M-platform milestone (#27).
  - **500 living,** the region's scale, holds the same map, so the map's cost is fixed and the people's cost is ADR 0045's.
  - **5,000 live.** Unchanged, since the live tier is counted in areas around the steered.
  - **50,000 living.** The far tier's armies walk paths twice as long in cells, so a far army's coarse path holds about twice the nodes, found once per objective. The slot cycle adds about 220 slot updates a tick over ADR 0056's 73.
  - **Saves.** A save holds the map's identity and its changed state. Slot states are 8 MiB raw, stored compressed per area, mostly grown and so small. #168's explored map at a bit per 8 × 8 block is 128 KiB.
  - **Load.** Four times ADR 0047's map data, which ADR 0047 set to load within a second. This ADR sets 2 seconds on the development Mac, superseding that target. Load on mobile is measured under the M-platform milestone (#27).
- **Saves, the hash and determinism.** The map's identity, its pack and its compiled hash, is saved and hashed as ADR 0047 has it. No 4,096 map has shipped, so no save is migrated. Every rule stays integer, and the region level is searched in index order.

The implementation, in order, each a pull request with its acceptance, replacing the 4,096 target of #173's steps:
1. **The map format at 8,192.** The compiler at a side of 8,192, painting per 16 × 16, and the validator's doubled route lengths, on a small test map scaled up.
   - Acceptance, each with a control: a test map of 8,192 compiled with its hash pinned; a key route within its doubled length, where one shortened below it is refused; a map past a bound refused with its `ContentRefusal`.
   - Acceptance: the core's map memory, the delta layer full, measured within the proposed 128 MiB on the low profile, and loading within 2 seconds on the development Mac.
2. **The level of regions on the coarse graph.**
   - Acceptance, with a control: a path across the test map found within a frame, 16 ms, where the graph of areas alone takes longer.
   - Acceptance: `make bench` within budget, and `make sweep` 48/50 or better.
3. **Slots and daily rules at 65,536 areas.**
   - Acceptance, with a control: ADR 0056's cycle visiting every area once in 8 days at about 4.6 areas a tick, where a test map of 16,384 areas visits about 1.1.
   - Acceptance: `make bench` p99 within 4 ms with every area's daily rules, and `make sweep` 48/50 or better.
4. **The first map's content at 8,192,** with inns on the roads and caves in the wilderness.
   - Acceptance: the validator's checks, and Luoyang to Chang'an on foot over open ground no shorter than twice ADR 0047's 5.8 minutes and at most 20% longer, the validator's wording.
   - Acceptance, with a control: inns compiled as named places of the new kind on the roads, about 140 cells apart, where a test map without inns still compiles and validates.
   - Acceptance: ADR 0048's bound measured on this map.
5. **The view's map images.** The minimap, the map window and the zoomed-out images at 8,192. Acceptance: captures (ko) of each.

## Decision

The world is one hand-made map of 8,192 × 8,192 cells. A cell between places is half a li, 250 metres, in the heartland, and the outer lands keep ADR 0047's compression relative to it, so every route in cells, and every journey, is twice as long: Luoyang to Chang'an 1,300 cells, 11.6 minutes on foot, and edge to edge 72.8.

A cell is still drawn as a tile a person fills, and layout patches keep #242's sizes, so live play is unchanged. Elevation, biome and region are painted per 16 × 16 cells, so the hand work stays at 512 × 512 values a layer. The core holds 64 MiB of footing and level, 65,536 areas with up to 4,194,304 slots, and a coarse graph with a level of regions of 8 × 8 areas above the areas. The tiers keep their radii in areas.

Inns and outposts are authored on the roads a day's walk apart, as a new kind of named place, and caves in the wilderness, beside the villages growth founds and the animals of #241.

The core's map, about 95 MiB at worst with ADR 0057's delta layer full, is held to a proposed budget of 128 MiB on the low profile, and the map loads within 2 seconds on the development Mac.

This supersedes ADR 0047's 4,096 × 4,096 cells, its cell of a li between places, its layers painted per 8 × 8 cells, its key routes' lengths and times, its sign of a map over 16 MiB on disk, and its load within one second, now 2 seconds. It supersedes ADR 0043's graph of areas alone, ADR 0056's 1,048,576 slots in all, and ADR 0048's bound measured on the 4,096 map.

## Consequences

- Every journey takes twice as long, and horses, boats, ferries and passes matter twice as much.
- The map's core memory rises from about 20 MiB to about 79 MiB, 95 MiB at worst with the delta layer full, and the compiled map on disk to about 11 to 21 MiB.
- The hand work of painting stays the same, and the build tool's run and the map's hash cover four times the cells.
- The far tier holds four times the areas, and paths need the region level.
- Four times the land feeds more people under ADR 0063's food bound, so the measured bound may be met sooner.
- #241's capacity for animals, sized on 1,048,576 slots, scales with the slots.

## Signs the decision was wrong

- Players find the land between places empty, so inns, villages and wilderness do not fill it, and the owner asks for density over size.
- The core's map memory misses the low profile's budget on a mobile device.
- Loading the map misses 2 seconds on the development Mac, or the compiled map passes 32 MiB on disk.
- A path across the map misses a frame even with the region level.
- Painting at 16 × 16 cells cannot draw a feature the owner asks for, so patches spread until much of the map is patched by hand.
- The population food supports on four times the land passes three fifths of the measured bound, so the bound or the yields must change.
