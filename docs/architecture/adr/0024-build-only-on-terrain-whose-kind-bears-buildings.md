# ADR 0024: Build only on terrain whose kind bears buildings

> Status: accepted; its rough cells bearing nothing superseded by ADR-0076 for fields, which a village's edge role may lay on them as terraces at a higher labour cost
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Villages were placed on any passable cell (ADR 0009), and growth laid structures on any cell clear of a structure or an impassable cell (ADR 0023). Forest and hills are passable, so growth laid a rice field inside a forest. The view also drew a hill landform's bushes over houses and a square (issue #96, `evidence/pr-90/growth_built_raised_4x.png`). Landforms stand on hills and mountains, and on nothing else.

Options weighed:

- **Where the rule lives.** Each terrain kind carries a footing: buildable, rough (bodies cross it but nothing is built on it) or blocked. The region writes each cell's footing into the map rows as `.`, `^` or `#`. `Map::buildable` is the one test that both generation's room for a village and growth's free cells read.
  - Rejected: a growth-only check against the terrain. Generation and growth would then disagree about the same cell, and the world, which holds only the map, would need the region's terrain.
  - Rejected: a `buildable` flag beside `passable`. It would admit a kind that is buildable but blocked.
- **Landforms over structures.** The data keeps every landform kind off buildable ground: hills are rough and mountains blocked. A landform then never shares a cell with a structure, and the view draws as it did.
  - Rejected: `world_art` clearing landforms under footprints. That would hide the placement, leave a structure standing in a hill range, and put a placement rule in GDScript, which holds none.
- **Which kinds bear buildings.** Seeds 0 to 199 for which generation found no room for a village, with three factions and then with four, by the generator's existing rule:

  | Buildable | Plain weight | 3 factions | 4 factions |
  |---|---|---|---|
  | plain | 4 | 26 | 44 |
  | plain | 16 | 0 | 1 |
  | plain, forest | 4 | 2 | 4 |
  | plain, forest | 10 | 0 | 0 |

  - Plain and forest bear buildings, hills are rough, and the plain's weight rises from 4 to 10. Then every seed has room, and nothing in the generator changes. Plain becomes 58% of the cells over those seeds, from 36%, and forest 18%, from 27%.
  - Rejected: the plain alone. It leaves a seed without room even at a weight of 16, so generation would need a new rule for a faction with no room.

## Decision

Each terrain kind's footing, buildable, rough or blocked, is data. The region writes it into the map, and one test, `Map::buildable`, decides where generation places villages and where growth lays structures. Plain and forest are buildable, hills rough, and water and mountains blocked. This supersedes ADR 0009's villages on free passable cells and ADR 0023's laying clear of an impassable cell.

## Consequences

- No structure stands on hills or mountains, so no landform is drawn over one, and `world_art` needs no rule about it.
- The plain's weight of 10 re-rolls every region. Region tests that depend on one village's surroundings now use seeds where those surroundings hold.
- A rice field can still be laid in a forest, since forest is buildable.
- A new terrain kind states its footing, and a new landform kind needs a kind that is not buildable.

## Signs the decision was wrong

- Villages look as though they stand in woodland, and forest has to be rough too, which needs a generator that can place a village without room for every faction.
- Hills cover the land between villages, and settlers find no buildable land within reach.
- A region reads as mostly plain in play, and the plain's weight needs to fall again.
