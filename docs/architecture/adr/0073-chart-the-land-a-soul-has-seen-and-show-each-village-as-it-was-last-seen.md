# ADR 0073: Chart the land a soul has seen, and show each village as it was last seen

> Status: accepted
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #168. The owner decided on 2026-10-08: the map completes and updates as the player moves, and it does not update a place until the player has looked at it again. The owner's scope of 2026-10-09, in the issue's comment, defers new touch buttons and layouts. The existing touch layout must keep working.

Facts from `main` at `1c4bb7f`:
- `hud/hud.gd`'s minimap draws the live world at a pixel a cell: every cell's terrain kind, the borders of `territory`, and every village in its owner's accent. It is redrawn whenever `world_art` signals a change to the territory or the villages.
- The map is the generated region of 128 x 128 cells (`REGION_SIDE`), with 6 to 8 villages at its start. ADR 0047's map of 4,096 x 4,096 cells is not built. ADR 0070, merged as proposed in #267, doubles it to 8,192 x 8,192, after the owner's revision of 2026-10-11 (#255).
- `territory` is derived on each read: a cell is held by the owner of the nearest village with living residents whose seat lies within the growth's `claim`. Its inputs per village are its owner, whether it has living residents, and its seat.
- `village_state` reads every army to tell whether a village is raided. Ruins and rebuilding are read from the village's own structures alone.
- Villages are only appended, so a village's index is stable.
- Only the player's body carries a soul (`ensoul`). A soul is born again into another body of the same world. The soul is saved; the steered mark is saved and hashed (ADR 0038).
- The rules' sight, 6 cells in the region, is how near a foe must be for a person no driver steers to close on it (ADR 0046). ADR 0058, proposed, makes `sees` the one sight rule and names #168's explored map among its readers.
- At the PC's least view, 640 x 360 world pixels, the screen shows 40 x 30 cells of 16 x 12 pixels (ADR 0059).
- The save holds the world's whole state in the core's own encoding at `SAVE_VERSION` 11, and refuses any other version (ADR 0030).
- The loadout is the player's choice and lives in the view, beside the save as `loadout_0.cfg`.
- `concept.md` §Reputation and rumors: a reborn body starts with no standing, while warehouses and caves stay with the soul.
- `concept.md` §Identity [agreed]: there are no player-only rules, and the one exception is caves. The PM ruled on PR #270 (2026-10-11) that the chart is not a rule: it is the steered body's remembered view, as the camera and the HUD are. Nothing in the simulation reads it, and it changes no outcome for anyone, so it needs no second exception.
- The owner decided on #257 (2026-10-11) that on rebirth what the soul knew carries over to the new body, while standing (ADR 0054) belongs to the body and resets.

Options weighed for where the record lives:
- **In the core, one chart per soul, saved and hashed with the world.** Chosen.
  - Sight belongs to the core. ADR 0058's `sees` hides cliff tops, and the core holds the levels it reads.
  - The soul carries knowledge already: caves found and warehouse accounts (ADR 0051).
  - The save already versions and checksums the world, and the core's tests can show the record round-trips.
  - No rule an NPC acts on reads it.
- **In the view, a file beside the save as the loadout is.** Rejected.
  - The view would repeat the core's sight, cliffs included, as a second rule.
  - A file written beside the slot is not covered by the save's checksum or version. It can be out of step with the world it is loaded with.

Options weighed for what is remembered of the land:
- **Whether each cell has been seen, a bit a cell, kept in blocks of 1,024 bits for each 32-cell area once any of its cells is seen.** Chosen. The minimap draws a pixel a cell, so the edge of what was seen is drawn where it lies. The cost follows the land explored, not the map.
- **A bit a cell over the whole map.** Rejected. It costs the full amount from the first tick, in memory and in every save: 8 MiB at 8,192 cells.
- **A bit per 8 x 8 block, ADR 0047's other choice.** Rejected. A sight 15 cells in radius would be drawn as steps of 8 pixels on the minimap.
- Terrain itself is not copied. It does not change until ADR 0057's terrain deltas land. Remembering those deltas is left to that work.

Options weighed for what is remembered of villages and borders:
- **Per village, as it was last seen: its owner, whether it has living residents, and its stage, standing, damaged or ruins.** Chosen. Borders are derived from these as `territory` derives them now, over the remembered villages alone, and shown only on seen cells.
- **Per seen cell, its holder.** Rejected. It costs a byte a cell, 16 MiB at 4,096 and 64 MiB at 8,192 once all of it is seen.
- **`village_state` itself.** Rejected. Raided reads every army each time, and it lasts only while the raiders stand there. The stage is read from the village's structures alone.

Options weighed for how far the player sees:
- **The rules' sight, 6 cells.** Rejected. The player would see villages on screen 7 to 20 cells away that the map keeps blank.
- **A map sight of its own, rules data, 15 cells in the region.** Chosen. That is half the 30 rows the PC's least view shows. Once ADR 0058 lands, a cell within it is seen only where `sees`'s cliff test does not hide it.

Options weighed for a body born again:
- **The soul keeps its chart.** Chosen. Knowledge of the land is the soul's, as the owner decided for what it knew on #257, and as caves and warehouses stay with the soul. A body born far from the last one finds the map as its last life left it, stale where the world moved on.
- **A blank chart.** Rejected. Every life would explore the same land again.
- **The family's chart.** Rejected. No NPC keeps a chart, so a family has none to pass on, and keeping one for every household would cost memory for a record no rule reads.

## Decision

Each soul keeps a chart of the land its bodies have seen. At the end of every tick, each living person carrying a soul whose army a driver steers charts what lies within the rules' `map_sight` of its cell:
- **Cells.** Every cell within `map_sight` is marked seen: 709 cells at 15. A soul's chart keeps an index of 4 bytes an area, and a block of 128 bytes for each area with a seen cell, made when its first cell is seen.
- **Villages.** A village is in view while `map_sight` reaches a cell under one of its structures. Its memory is overwritten with its owner, whether it has living residents, and its stage: ruins when every structure is rubble, standing when every structure is intact, damaged otherwise. A village never in view has no memory, so one founded while the player was away does not show.
- **Borders.** `remembered_territory` derives each cell's holder as `territory` does, from the remembered villages alone, and gives no holder to an unseen cell.
- **Sight.** Once ADR 0058's `sees` lands, a cell or a structure's cell is in view only where its cliff test does not hide it from the charting person. This supersedes ADR 0058's explored map read at the rules' sight: its cliff test is kept, at `map_sight`.
- **Every tick.** Charting has no cache and no state beyond the chart. A loaded world charts as the saved one did.

The chart is the soul's. A body born again keeps it, and a new game starts blank. No rule an NPC acts on reads it.

It is saved and hashed with its soul:
- **Saved.** For each seen area, its index and its 128 bytes, in order of index. For each remembered village, its index, owner and flags. `SAVE_VERSION` rises to 12, and older saves are refused as before.
- **Refused as `CORRUPTED`.** An area outside the map, areas out of order or repeated, a block with no seen cell, a village index past the villages, an owner past the factions, or a stage that does not exist.
- **Hashed.** The hash folds the same values in the same order, and folds a soul's chart only while it holds a seen cell or a remembered village, as ADR 0038 folds steered marks and declarations only while held. A world with no chart keeps its hash, so the pinned determinism hash is unchanged until a chart is held.

The core keeps a revision per chart that rises when a cell is first seen or a memory changes. It is for the view, and it is neither saved nor hashed.

The core-to-view boundary is four reads of the chart of the soul the player carries:
- `explored`: a byte per cell, row by row, 1 where seen.
- `remembered_villages`: what `villages` gives, with the remembered owner and stage, and only villages remembered.
- `remembered_territory`: what `territory` gives, from the remembered villages.
- `chart_revision`.

These are whole-map reads, as `terrain` and `territory` are. At ADR 0047's size they move to reads by area with the view's chunks, together with those.

The view draws only the chart:
- **The minimap** draws the map around the player, as it does today: seen cells from the remembered borders and villages, a ruined village in stone, and unseen cells in fog. It is redrawn when `chart_revision` changes, in place of `world_art`'s change signal.
- **The map window** is `hud/map_window.gd`. N opens and closes it, as the panel keys do, and the shortcut rail and the pause guide list it. It draws the minimap's own image, at the largest whole multiple at which the whole map fits the HUD's room inside the panels' frame, with the player's position and the season. At ADR 0047's size it draws from that ADR's prebuilt downscaled images instead, with unseen land in fog. Its touch button waits with the other deferred touch controls.
- **The mockup** of both, at PC and touch layouts, goes to the PM before the view is built.

## Consequences

Costs, at the region of 128 cells built today, ADR 0047's 4,096 and ADR 0070's 8,192:

| | 128 | 4,096 | 8,192 |
|---|---|---|---|
| Areas | 16 | 16,384 | 65,536 |
| Index, held from the start | 64 B | 64 KiB | 256 KiB |
| A walk along one axis from edge to edge, 2 areas wide | 1 KiB, 8 areas | 32 KiB, 256 areas | 64 KiB, 512 areas |
| Every cell seen | 2 KiB | 2 MiB | 8 MiB |
| In the save, every cell seen | 2.06 KiB | 2.06 MiB | 8.25 MiB |
| Village memory | 4 B a village | 39.06 KiB per 10,000 villages | the same |

- **Memory.** Only souls of steered armies chart, one today. The index is saved only as the list of seen areas, so it costs nothing on disk.
- **Per tick, for each charting person:**
  - 709 bit marks.
  - Today a distance test against every village's structures, 6 to 8 villages in the region. At ADR 0047's size the villages are found by area instead: a sight of 15 cells, 31 across, touches at most 4 areas.
  - For the villages in view, a read of their structures' stages and residents.
  - With ADR 0058, one line walk of at most 15 cells for each of the 709 cells, skipped where the levels differ by less than the hiding drop, which ADR 0058 finds nearly always.
  - Nothing that grows with the population. `make bench` measures it in the implementing pull request.
- **No rule changes.** No NPC rule reads the chart, so `make sweep`'s outcomes do not move. The implementing pull request runs it to show that.
- **The steered body charts.** A world with no steered soul charts nothing, so the M1 scenario's bench and `world_hash`'s comparisons are untouched.
- **The view no longer draws the live world's borders and villages on the map.** A village burned out of sight shows standing until the player returns. The same burning in sight shows at the end of that tick.
- **Building it is one pull request:**
  - the chart in `core/` with its save and hash
  - the adapter's four reads
  - the minimap's fog and the map window, after the mockup
  - tests of a remembered village against the same change in sight as its control, of a save round-trip, of rebirth keeping the chart, and of each refusal
  - a test that a world run with an empty chart and the same world with a full one stay hash-identical in everything but the chart, so no outcome reads it
  - captures, `make ci`, `make sweep` and `make bench`

## Signs the decision was wrong

- Saves of long games grow by megabytes from the chart, at ADR 0070's size, toward its 8 MiB. Then a bit per 2 x 2 cells, a quarter of the size, is worth its coarser edge.
- Borders on the map disagree visibly with the land, where an unseen village's claim reaches seen cells. Then the holder per seen cell, at its cost, is the better record.
- Players find a reborn body's stale map misleading more than useful. Then the blank chart is the better choice.
- A rule comes to need what an NPC knows of the land. Then a chart per soul is the wrong unit, and the record belongs to people.
- 15 cells reveals too little or too much of what the screen shows, once ADR 0057's zoom lets the player see farther. Then the map's sight should follow the view, not the rules.
