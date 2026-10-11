# ADR 0057: Resolve world-scale arts as waves over area indexes, absorbed by materials, and reshape terrain in a conserving delta layer

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner asked on 2026-10-09, in issue #203, for a single move at the top of the arts ladder to cover an area so vast that the player zooms out to aim it. Everything inside is affected: people, structures, natural objects and the terrain itself. The cost must scale with what lies inside the area, never with the map's 4,096 cells. The PM's direction in the issue:
- area queries through per-area indexes
- the effect propagated as a wave, ring by ring over several ticks, deterministically
- the same in every tier
- terrain changed through a delta layer over the authored map
- zoomed-out drawing from lower-detail representations

The owner's comments of the same day are binding:
- **Attacks reshape terrain as power grows, at every scale.** An art can cut or level a cliff, blast a crater, fell a forest, or breach a wall or gate, each gated by realm and the art's power.
  - Every change goes to the delta layer, saved, hashed and deterministic.
  - Movement, paths and sight follow the changed cells on the next tick, and ADR 0043's graph is rebuilt for the touched areas only.
  - Villages and structures inside take damage by the rule strikes use.
  - The ground shader (#212) draws the map plus the delta, so a change needs no new art.
  - The design states the worst case, whether the delta heals, and why ADR 0047's validation need not hold after a change.
- **Mass is conserved.** Earth removed is deposited: thrown in the strike's direction, piled at a crater's rim, or dropped as scree at a cliff's foot.
  - A felled forest leaves logs, and a breached wall stones.
  - A change sums to zero in elevation units, in fixed point, and a test asserts it.
  - Material landing in water fills it, and material thrown off the map is clamped onto the frame.
  - Debris becomes items people may gather.
  - The cost stays bounded by the art's area.
- **Obstacles absorb an attack by their material.**
  - Each terrain kind and structure has a material, pack data, with a toughness and a breaking threshold. Below the threshold it absorbs and is damaged; above, it breaks and passes the rest on.
  - An attack carries energy along its shape, each cell or structure on the path subtracting what it absorbs, so people behind a wall that held take nothing.
  - This holds at every scale, ordinary strikes included.
  - The design states whether sight (#214) and strikes share one line walk, the cost bound, how ADR 0013's structure damage maps onto toughness, and the tests.

Facts from `main` at `aff1434`:
- **Shapes** (ADR 0046): area, arc, line, self and targeted. A use covers its shape and hits every hostile person in it once, with nothing between the user and a target considered. A structure takes a skill's separate structure damage where the shape reaches its footprint (ADR 0013, ADR 0015). Structures have 10,000 durability, with stage floors.
- **The map** (ADR 0047). It is hand-made, in 16,384 areas of 32 cells.
  - Elevation tiers 0 to 7 are painted per 8 × 8 cells, and the build tool makes a cliff, with blocked footing, where a tier drops, and a slope between tiers.
  - The core holds a nibble a cell of footing and travel class, 8 MiB, and "never reads elevation". It says terrain never changes in play, so a save holds only the map's identity.
  - Its validation guarantees that the named places reach one another.
- **Paths** (ADR 0043): a coarse graph of areas, rebuilt only where a blocking structure changes an area.
- **Natural objects** are slots the map places, 64 an area, with a stage and a timer each, under ADR 0056 in the open PR #220 for #187.
- **Tiers.** Every person keeps its record in every tier (ADR 0042), and far armies move on the coarse graph.
- **Pending.** Sight blocked by height is #214's ADR 0058, the number the PM assigned it, in its own PR. The arts' tiers are ADR 0055 (#194), and the ground shader is #212.

Decided by the PM on PR #222's review, 2026-10-10:
- **Absorption on every strike** is the owner's own rule, "the same rule at every scale", and stays. Its effect on raids and melee against structures is stated under Consequences.
- **Crater spoil** is thrown in the strike's direction where the art has one, with a symmetric rim for a centred blast.
- **The core holding elevation** goes beyond ADR 0047's "the core never reads elevation". It is accepted, as #214's climbing and sight need it too, and recorded as a partial supersession of that point of ADR 0047.
- **Terrain below its threshold** is damaged, as the owner said, through a damage counter per cell (below).
- **Raised elevation never healing** is a proposal, marked as such in `concept.md`.
- **Proposals in pack data,** which the sweep checks: the material table, 120 days of healing, 4 cells a tick, 256 sectors, 4,096 blocks, 16,384 cells a cast, and zoom to a sixteenth.

Options weighed:

- **One line walk for strikes and sight.** A line of effect is walked over the cells between a point and a target: Bresenham's line in fixed point, deterministic, each cell once. Each cell's material is read once, for the energy it absorbs and for whether it blocks sight. #214's ADR decides sight's rules, and is to read them from this same walk. Chosen, so one cost serves both.
  - Rejected: separate walks for sight and for strikes. That is two passes over the same cells for every engagement check and every strike.
- **Materials.** A material is pack data (ADR 0049): a toughness, the energy it absorbs, and a threshold at which it breaks.

  | Material | Toughness | Threshold | Of |
  |---|---|---|---|
  | Earth | 20 | 200 | plain, hills, slopes |
  | Rock | 80 | 800 | cliffs, mountains |
  | Timber | 40 | 300 | houses, fences, trees |
  | Rammed earth | 60 | 500 | village walls |
  | Brick | 80 | 700 | halls |
  | Stone wall | 150 | 1,500 | city walls, gates |
  | Water | 10 | never breaks | rivers, lakes |
  | Ice | 50 | 400 | frozen water |

  Each ground kind and structure kind names its material. A cell absorbs only where it stands in the line's way: a structure's footprint, a cell of blocked footing such as a cliff or a mountain, or a cell whose level is above the level the line is walked at, the origin's. So the earth of a plain or a hill at the line's own level absorbs nothing, and earth absorbs where a hill or a terrace rises above the line.
- **Energy and absorption.** A use's energy is its body damage after might, edge and potency, the number ADR 0046's rule lands today.
  - For each target its shape covers, the walk runs from the shape's origin, the user or a disc's centre, to the target's cell. At each cell or structure with a material:
    - energy below the threshold is absorbed: the structure takes the energy as damage, or the cell is unchanged, and the walk stops
    - energy at or above the threshold breaks a cell: the cell is changed (below) if the art may change terrain, and the energy goes on less the toughness
    - energy at or above a structure's threshold damages it by the threshold. The energy goes on, less the toughness, only if that damage brings the structure to its destroyed stage (ADR 0013). A wall that still stands stops the blow, so a breach is the wall's fall and not a blow passing through it
  - The target takes the body damage scaled by the energy that reaches it over the energy at the start, rounded down.
  - So a sword cut of 10 against a timber wall is absorbed and harms no one behind it, and a palm strike of 400 breaks the wall and lands at 360.
  - **Structures.** A structure still takes a skill's own structure damage, the torch's and the mend's, as ADR 0015 decided, where the shape reaches it. It also takes the energy it absorbs from blows that meet it. That is how ADR 0013's structure damage maps onto toughness, and its stages are unchanged.
  - **Cost.** The walk is per covered target. An arc at 1.5 cells walks at most 2 cells and a line of 8 at most 9, so ordinary strikes add a few reads.
  - Rejected: absorption only for world-scale arts. The owner made it one rule at every scale.
- **The wave shape.** A new shape, `WAVE`, for world-scale arts: a disc of radius R about its point, resolved ring by ring from the centre.
  - The ring at distance r resolves on the tick its speed reaches it: 4 cells a tick, slowed so that no tick's ring exceeds 8,192 cells. R = 200 takes 50 ticks, under 2 seconds.
  - Energy travels outward by angular sector: 256 sectors, each carrying what its cells have not absorbed, so a hill or a wall casts a shadow behind it.
  - **A cell's sector** is an integer angle from the centre, in 256ths of a turn. It is found from the cell's offset by its octant and a table of 32 ratios in 1,024ths, with no floating point, so every host assigns the same sector.
  - **The order within a ring.** Cells are taken by sector, then by distance, then by index. People, structures and slots in a ring are hit in index order.
  - A ring reads only the areas it overlaps, through per-area indexes:
    - people, from the spatial hash
    - structures, from their hash
    - slots, from ADR 0056's area table
  - Each person, structure and slot in a ring is hit once, by its sector's energy at that ring, scaled by the wave's falloff per ring, pack data.
  - **One budget for all waves.** The 8,192 cells a tick are shared by every wave in the world, not given to each.
    - Waves advance in the order they were cast. One that finds the tick's budget spent waits for the next tick, its rings delayed and not dropped.
    - At most 8 waves are in flight at once, a core bound. A cast of a wave past it is refused with a new `CastRefusal`, `TOO_MANY_WAVES`.
    - So the per-tick figures below hold however many casters there are.
  - Rejected: resolving the whole disc on one tick. R = 200 covers 125,600 cells, a whole frame's work.
- **Gating.** A skill's terrain power and its realm rank are pack data. Only a skill whose `terrain` field names a change, and whose user's rank reaches its rank, changes cells, and only cells its energy breaks. The changes:
  - **Crater.** Lower a disc's elevation, depositing the volume beyond it. An art with a direction, an arc or a line, throws the spoil ahead in that direction, on the far side of the crater. A centred blast, a wave or a self shape, piles it on a symmetric rim. A grown slot in the disc drops its yield as piles, and every slot in it is set empty, to regrow only by spread (ADR 0056).
  - **Cut.** Lower a cliff's top cells, depositing scree at its foot on the side away from the user. Slots on the cut cells are treated as a crater's.
  - **Level.** Set a disc's cells to their mean elevation, the units divided evenly, and the remainder placed a unit a cell in the fixed order, so nothing is dropped.
  - **Fell.** Set the grown slots in the shape to stumps, and drop their wood as piles (ADR 0051).
  - **Breach.** Break a structure, by the damage rule, and drop stones as piles.

  The base pack gives no art a terrain change until a peerless art is designed (ADR 0055's tiers). Tests use a pack of their own.
- **Elevation in the core.** The core holds an elevation tier, a nibble a cell beside ADR 0047's nibble of footing and travel class: a byte a cell, 16 MiB in all.
  - Terrain changes, and #214's sight by height, read and change it.
  - After a change, the build tool's rule runs on the touched cells and their neighbours: a drop of a tier or more is a cliff, with blocked footing, and a one-tier step is a slope. So movement follows the next tick.
  - Rejected: loading elevation from the map through a port when an art lands. A wave crossing 120 areas would read from storage in the middle of a tick, which ADR 0047 rejected for daily rules.
- **Damage below the threshold.** As the owner said, a cell a blow fails to break is damaged. Each cell of a block keeps a damage counter of 1 byte, in eighths of its material's toughness. A blow absorbed by a cell adds its energy to the counter. A counter reaching the threshold breaks the cell as a blow at the threshold would. Counters return to 0 with the ground's healing, after 120 days.
- **The delta layer.** A changed area holds a delta block of 1,024 cells: a fine elevation of 2 bytes, in 64ths of a tier, a ground-kind override of 1 byte and the damage counter of 1 byte, 4 KiB in all. Unchanged areas hold none.
  - At most 4,096 areas hold a block, 16 MiB. A change that would need a new block past the bound changes no terrain, though it still deals its damage.
  - Blocks are saved and hashed, so `SAVE_VERSION` rises and earlier saves are refused. ADR 0047's map identity still names the authored map, and the save holds the deltas over it.
- **Mass conservation.** Every change moves elevation units and never makes or destroys them:
  - **The units.** A cell's elevation is its level × 64 plus its block's fine delta, in 64ths of a level: from a floor of 0 to a ceiling of 15 × 64 + 63, 1,023. The level nibble is that over 64, rounded down, and footing follows it.
  - **Removing.** A cell is never lowered below the floor, so a change takes from each cell only what it holds above 0.
  - **Depositing, with spill.** Spoil is placed on its target cells up to the ceiling. What a cell cannot take spills outward, ring by ring from the deposit, at most 64 rings beyond it.
  - **The remainder.** A share that does not divide evenly is placed a unit a cell in the fixed order of the cells, by sector, then distance, then index.
  - **Atomic refusal.** A change is computed whole in a scratch buffer before anything is written. If it needs a delta block past the bound of 4,096, or spoil that 64 rings cannot hold, the whole change is refused: no cell changes, and the blow's damage still lands. No change is ever half made.
  - **The sum test.** A test asserts that the units over every touched cell, plus the units in its stone piles (below), are the same before and after each kind of change, including a crater whose rim overflows.
  - A crater's removed volume goes on its rim or ahead of it, and a cut's onto its scree cells.
  - Units landing on water raise its bed, turning deep water to shallows, and shallows to land past a tier.
  - Units thrown past the map's playable edge land on the nearest frame cell, which absorbs any amount.
  - Debris that is a material, wood from a fell and stone from a breach or a crater's rock, is dropped as piles for anyone to gather (ADR 0051).
    - A crater's stone is counted inside the conservation: each stone stack takes 16 units out of the spoil it would have been, so no material is made beyond the earth removed.
    - A cast drops at most 256 stone piles, the rest staying as spoil, so its piles stay a small share of ADR 0051's bound of 1,048,576 and its worst day.
    - Wood from felled slots is the slots' yield, as ADR 0056 gives it, and is not elevation. A village's gatherers take such piles first, as ADR 0056 decides, so debris feeds building.
- **Paths.** Movement follows a change on the next tick, as the owner asked, and the graph follows within a bound. This is a decision:
  - **The next tick.** A step reads its target cell's footing directly (ADR 0037), so no one walks into a cell made impassable, or is kept out of one made open, on the tick after a change, whatever the graph says.
  - **The graph.** It is rebuilt area by area from a queue, at most 8 areas a tick, with the queue shared by every change. That is about 0.4 ms at about 50 µs an area, by this estimate. First come the areas people stand in, then those on an army's current coarse path, then the rest by index.
  - **The fields** crossing a rebuilt area are rebuilt within ADR 0043's per-tick limit.
  - An army whose objective a change has made unreachable holds the reachable cell nearest it until its order changes, since ADR 0013's reachability is checked when an order is given.
  - **The true worst case.** A wave of R = 256 covers about 206,000 cells, about 201 areas. Its 16,384 changed cells may fall across all of them, and their neighbours for the graph's borders bring it to about 260 areas. At 8 a tick the queue empties in about 33 ticks, about a second, while movement has followed the cells from the first tick.
  - **Eight waves at once.** With the cap of 8 waves in flight, each of R = 256 changing its 16,384 cells across about 260 areas, the shared queue holds at most about 2,080 areas. At 8 a tick it empties in about 260 ticks, under 9 seconds, while movement follows the cells from the first tick.
- **Validation after play.** ADR 0047's validation holds for the authored map and is not run on deltas. Cutting a pass shut or walling a city off is the kind of feat the owner asked for, and the world's rules already handle what is unreachable: orders are refused and armies hold. The map's guarantee is about its design, not about what players do to it.
- **Healing.** Ground-kind overrides, scorched or churned earth, revert to the biome's ground after 120 days. Felled slots regrow by ADR 0056's rule, and villages rebuild by growth. Elevation changes stay. No rule moves earth back, and conserving mass while healing would be a second change of its own.
- **Tiers.** A wave is the same in every tier.
  - People are hit by their record wherever they are. A far person's position is its army's anchor on the coarse graph.
  - Structures and slots exist everywhere.
  - The delta changes the map for every tier.
- **The view, with #200 and #212.**
  - **Zoom levels.** Beyond the 1x to 3x of today, the camera zooms out to a half, a quarter, an eighth and a sixteenth.
  - **Ground.** Zoomed out, the ground is drawn from prebuilt downscaled images per area, from the map's build, shared with the minimap. They are streamed by area as chunks are today. An area with a delta block redraws its images from the ground shader once its change settles.
  - **People and objects.** People below a quarter are drawn as dots in their faction's colour, and below an eighth as one cluster mark per area with a count. Objects are drawn as clusters below a quarter.
  - **Aiming and casting.** Aiming a wave at any zoom shows its full disc. A cast pulls the camera back to frame the disc, then returns.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 64 materials
  - a toughness and a threshold each at most 2^20
  - a threshold at most 255 eighths of its toughness, about 32 times it, so the 1-byte damage counter in eighths of toughness cannot overflow
  - a wave's radius at most 256 cells
  - a skill's terrain change at most 16,384 cells a cast
  - a wave's falloff per ring at most 1,000 per mille

  The 256 sectors, 4 cells a tick and 8,192 cells a tick are core values, and the delta bound of 4,096 areas is the core's.
- **At the bound and the live tier.** At 50,000 people with #216's live tier of 5,000:
  - **Ordinary strikes.** The M1 battle makes 18 strikes a tick among 500 people (ADR 0046), so about 180 a tick at 5,000, each covering about 1.7 targets with a walk of at most 2 cells for an arc. That is about 600 cell reads a tick.
  - **A wave.** It reads at most 8,192 cells a tick. In the densest town, about a person each 4 cells, that is at most about 2,000 people, plus the structures and slots in the areas the ring overlaps.
  - **Delta blocks.** At most 16 MiB. The graph rebuilds at most 8 areas a tick, shared by every change.
  - All of these are estimates, which the measurements below replace.
- **Measurements the implementing pull requests make,** on the low profile (ADR 0048), with the expected density at the bound:
  - a tick's p99 while a wave of R = 50, 100 and 200 crosses the busiest town, with every slot grown
  - the cells, people, structures and slots each ring reads
  - the delta layer's size after a scripted war of 100 casts of R = 100 with terrain changes
  - the graph rebuilds a cast causes

The implementation, in order, each a pull request with its acceptance:
1. **The line walk and materials.**
   - Acceptance: one strike against open ground, a timber wall and a stone wall giving decreasing damage behind each, and a strike over the threshold breaching and going on weakened.
   - Acceptance: `make bench` within ADR 0048's budget.
2. **The wave shape.**
   - Acceptance: rings resolved on their ticks, shadows behind a wall, a per-tick cell bound held, and the R = 50, 100 and 200 measurements.
3. **Elevation and the delta layer.**
   - Acceptance: crater, cut, level, fell and breach, each conserving mass by a summing test.
   - Acceptance: footing following the next tick, the graph rebuilt for touched areas only, and saves with their deltas.
4. **Zoom levels and aiming.** After #200's chunks and #212's ground shader. Acceptance: the frame bench at each zoom.

## Decision

Every strike walks a line of effect, shared with #214's sight, from its shape's origin to each target. Materials on the way, pack data with a toughness and a threshold, absorb its energy or break and pass the rest, and structures take what they absorb as damage beside their skill's own.

World-scale arts use a new `WAVE` shape: a disc resolved ring by ring at up to 4 cells a tick, carrying energy by 256 sectors so obstacles cast shadows. Every wave in the world shares one budget of 8,192 cells a tick, in cast order, and at most 8 waves fly at once, a ninth refused with `TOO_MANY_WAVES`. It reads people, structures and slots through per-area indexes, the same in every tier.

The core holds an elevation tier per cell, and changed areas hold delta blocks of fine elevation and ground overrides, at most 4,096, saved and hashed. Skills gated by realm and power may crater, cut, level, fell or breach. Every change conserves elevation units, filling water and clamping onto the frame, and drops its debris as piles. Footing is recomputed by the build tool's rule, so movement follows the next tick, and the graph is rebuilt for the touched areas from one shared queue at 8 areas a tick, at most 16,384 changed cells a cast. A cell a blow fails to break keeps a damage counter toward its threshold.

ADR 0047's validation is not run on deltas. Ground overrides heal in 120 days and elevation stays. Zooming out draws downscaled area images, dots and clusters.

This supersedes these:
- ADR 0046's use hitting every target its shape covers whatever lies between
- ADR 0013's and ADR 0015's structure damage from the skill's structure damage alone
- ADR 0047's terrain that never changes in play, its core that never reads elevation, its save holding only the map's identity, and its 8 MiB core layer
- ADR 0043's graph rebuilt only for structures

## Consequences

- Walls protect. A village's palisade absorbs ordinary blows, and only strong arts reach those behind it. Raiders must break a wall before they can strike those within, so raids on walled villages last longer and need stronger raiders, while open hamlets fall as before. The sweep is rerun in step 1.
- Melee breaks structures faster, since blows meeting a wall damage it.
- The core's map grows from 8 to 16 MiB, and saves by up to 16 MiB of deltas after a long war.
- Terrain that players reshape stays reshaped. A blocked pass is a fact of the world until someone cuts it open again.
- The view gains four zoom levels and their downscaled images, built with the map.

## Signs the decision was wrong

- Every strike's line walk costs the battle's tick more than ADR 0048's budget allows, so ordinary strikes need a cheaper rule than world-scale arts.
- Waves of R = 200 cannot keep 8,192 cells a tick within budget, so the wave must slow so much it reads as a crawl.
- The delta bound of 4,096 areas fills in ordinary play.
- Players use craters to cut every route to a rival's seat, and the world stalls because nothing heals elevation.
- #214's sight needs a walk the strike walk cannot share, such as one over elevation that strikes ignore.
