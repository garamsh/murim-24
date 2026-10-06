# Art direction

How Murim24 looks: the screen, the palette, terrain, bodies, structures, effects and the HUD, written so that an artist can make matching art from this file and the images it names.

## Contents
- Sources
- Screen and grid
- Mood and references
- Palette
- Terrain
- Bodies and markers
- Structures and damage stages
- Effects and HUD
- Do and don't

## Sources

The approved target, which the production set is taken from:

- `art/mockup/target_screen.png` — one finished-game screen at native size; `target_screen_4x.png` at 4x.
- `art/mockup/lineup_sheet.png` — the bodies of each faction, villagers and the player, the village house in its four stages and the aggregate-army marker; `lineup_sheet_4x.png` at 4x.

The production set. Each `.aseprite` file is drawn by the `.lua` script beside it and exported beside it as a `.png` sheet with a `.json` of its frames and tags, by the command in `docs/convention/stack-godot.md` §Asset export. `art/build.sh` redraws and exports all of it.

| Asset | Frame | Tags |
|---|---|---|
| `art/terrain/terrain` | 16 x 16 | `plain`, `forest`, `hills`, `water`, `mountain`, four variants each |
| `art/bodies/orthodox`, `demonic`, `unorthodox` | 16 x 24 | `idle` (one frame), `walk` (four frames) |
| `art/structures/house` | 64 x 60 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/markers/army_marker` | 64 x 48 | `orthodox`, `demonic`, `unorthodox` |
| `art/markers/player_marker` | 16 x 24 | `ring` (one frame), `chevron` (two frames) |

`art/contact_sheet/contact_sheet.png` shows every frame of the set, read back from the sources, and a scene assembled from them on the grid; `contact_sheet_4x.png` at 4x. It is for review, and Godot does not import it.

## Screen and grid

| Property | Value |
|---|---|
| Native resolution | 480 x 270 pixels (16:9) |
| Scale | Whole multiples only: 4x fills 1920 x 1080 |
| Grid | One core cell to one 16 x 16 tile; 30 cells across, 16.875 down |
| View | Three-quarter top-down (ADR 0012): ground and roofs seen from above, facades and standing bodies drawn upright; no isometric diamond and no perspective |
| Draw order | Everything standing is drawn in order of its footprint's bottom edge, back to front |
| Light | From the upper left; shadows fall to the lower right |

- A sprite covers its footprint and may rise above it. A body's footprint is its cell, the bottom 16 rows of its 16 x 24 frame. The house's footprint is 3 x 3 cells, the 48 x 48 at (8, 8) in its frame; the margin round it holds the upturned eaves and spilled debris.
- Shadows are not in the sprites. The view draws them on the ground by darkening it two steps along its own ramp: an ellipse under a body's feet, a band past a structure's right and bottom edges that shortens as the structure falls, a soft patch under an army. `art/sprites.lua` draws these shapes, and the contact sheet's scene shows them in place.

## Mood and references

The world is the gangho of wuxia fiction in daylight: river villages, tiled roofs, fields, roads where sects meet. What happens on it is violent and the ground stays calm, so that the violence reads.

References, described in words:

- Ink-wash landscape painting: muted greens, slate roofs, mist and smoke in grey rather than colour.
- Wuxia film fight staging: bright, short-lived arcs of sword qi and palm force over still ground.
- 16-pixel console RPGs seen from above: one outline colour, three-to-six-step ramps, readable at a glance.

## Palette

Every pixel comes from the master palette, defined in `art/pixel.lua` as ramps that run dark to light. Shading moves along a ramp, never off it, and nothing is blended by transparency: smoke and glows are dithered. `art/contact_sheet/contact_sheet.lua` refuses a source holding a colour outside the palette.

| Ramp | Hex, dark to light | Use |
|---|---|---|
| ink | `14101a` `2a2433` `3d3647` `524a5e` | Outlines, hair, HUD panels |
| stone | `3b4250` `4a5361` `66707d` `8a94a0` `b4bcc4` `dfe3e6` | Roofs, rock, mountains, smoke |
| grass | `1f3a2b` `2c4a32` `3b6138` `4f7a3f` `6b9447` `8fae5a` | Plains, hills |
| leaf | `142a22` `1e3328` `284536` `35593d` `4a7044` | Forest canopy |
| earth | `3a2c24` `4a3a2c` `6b523a` `8c6d4a` `b08d60` `d1b787` | Roads, banks, straw hats |
| wood | `2e1f1a` `3b2a22` `5c3d2a` `7d5535` `9c7048` | Doors, beams, hafts |
| water | `1a2f40` `1f3a4d` `2b5770` `3d7a8f` `5fa3a8` `a6d4cf` | Water, ripples |
| crimson | `3d0d14` `5c1418` `9c1f24` `d63a2f` `f06a3c` | Demonic Cult accent, HP |
| azure | `152a55` `1f3d7a` `2f63b8` `5b9ae0` `a8d0f5` | Orthodox accent, QI |
| amber | `4a2e0c` `6b4310` `b07418` `e8a830` `f5d27a` | Unorthodox accent |
| skin | `5c3a28` `8a5a3c` `c48a62` `e8b98e` | Skin |
| cloth | `6b6052` `8c7f68` `b4a68a` `d9cdb0` | Villagers' clothes |
| robe | `8a94a0` `c8ccd0` `f4f1e6` | Orthodox white robes |
| fire | `9c1f24` `f06a3c` `f5a33c` `fbe37a` `fff8dc` | Flames, impact cores |
| jade | `1f5c4a` `2f8c6e` `4fc49a` `a6f0cf` | The player's ring, selection |
| gold | `5c4418` `8c6a2a` `c99a3c` `f0d080` | HUD trim, the player's chevron |
| paper | `a8946c` `d9c9a3` `efe3c2` | Plaster walls, HUD text |
| lacquer | `3d1418` `7a2a24` `a83a2c` | Pillars, HUD corners |

Faction accents, chosen to stay apart at one cell and against every terrain kind:

| Faction | Accent | Body value |
|---|---|---|
| Orthodox | azure `2f63b8` | Light: white robes |
| Demonic Cult | crimson `d63a2f` | Dark: charcoal clothes |
| Unorthodox | amber `e8a830` | Middle: brown clothes under straw |
| Player | jade `4fc49a`, in its markers only | Its faction's body, unchanged |

## Terrain

One tile set covers the terrain kinds the region generator defines (`core/region.cpp`, `region_data()`): plain, forest, hills, water and mountain.

| Kind | Passable | Look |
|---|---|---|
| Plain | yes | Grass in three tones with tufts and a rare flower |
| Forest | yes | Canopy from above: leaf lumps lit from the upper left over a dark understorey |
| Hills | yes | Wavy terraced ridges, two to a tile, lit on the upper edge and shaded below |
| Water | no | Deep and shallow blue with short light ripples; no grass anywhere |
| Mountain | no | Bare rock: a lit crest, a creased shaded face, scree at the foot |

- A tile carries no outline, so tiles of one kind join into one surface.
- Every variant of a kind meets every other without a seam: each is drawn from noise and shapes that repeat every 16 pixels, and hills and mountain ridges cross the tile edge at the same height in every variant.
- Water and mountain, the two impassable kinds, carry no green, so passability reads from colour family alone.
- Where two kinds meet, the cell edge shows. Transition tiles are not drawn yet; the mockup's ragged river bank is the look they are to give.

## Bodies and markers

- One body template for every faction; a faction changes the palette slots, the headgear and the weapon. A faction is told apart by three cues at once, so that any one of them is enough: accent colour, body value (light, dark, middle) and silhouette.
- Silhouettes: Orthodox topknot and a long robe; Demonic Cult spiky hair under a crimson headband; Unorthodox the wide conical straw hat.
- Every body has a one-pixel ink outline around its whole silhouette, weapon included.
- The body faces the viewer, turned to the right; the view mirrors it to face left.
- Walk: a step, a passing frame a pixel higher, the other step, a passing frame, at 150 ms a frame. Idle is the standing frame.
- Villagers wear the cloth ramp and carry no faction accent (approved line-up; not yet in the production set).
- A knocked-down body is the same sprite lying on its side, without a shadow (approved line-up; not yet in the production set).
- The player is a body like any other (`docs/concept.md` §Identity), drawn with its faction's sprite. Two markers tell it apart: the jade `ring`, drawn under the body round its feet, and the gold `chevron`, drawn over it above the head and bobbing a pixel every 400 ms.

An individual agent and an aggregate army read differently:

| | Individual agent | Aggregate army |
|---|---|---|
| Drawn as | One full body per agent | One 4 x 3-cell marker standing for the whole army |
| Detail | Face, weapon, pose | Rows of heads in the faction's colours, no faces |
| Frame | None | Accent-coloured bracket corners round the block, the faction's banner above it |
| Number | None | A plate under it with the army's name and count, drawn by the HUD as text |

## Structures and damage stages

The village house covers 3 x 3 cells, in four stages that follow the core's structure stages:

| Stage | What shows |
|---|---|
| Intact | Slate roof with ridge and upturned eaves, plaster wall between lacquered pillars, door and lattice windows, stone plinth |
| Damaged | Holes in the roof showing rafters, scorch on the wall, the door broken in, fallen tiles in front |
| Destroyed | Roof fallen in with one slab left, charred beams, wall broken to stumps, debris at the door |
| Rubble | A low heap of tile, stone and burnt beams on the plinth, weeds returning |

- The silhouette shrinks with each stage, and so does the cast shadow, so a stage reads from its outline before its detail.
- The plinth stays in every stage, so the footprint stays readable after the building is gone.
- Fire and smoke over a burning building are effects drawn over the `destroyed` frame, not part of it.

## Effects and HUD

- Effects are brief and bright over calm ground: an area skill is a ring of force in its user's accent ramp with a white-hot core, cracks on the ground and sparks; sword qi is a crescent in the azure ramp; damage numbers rise in the colour of the technique that dealt them, with an ink shadow.
- Fire uses the fire ramp; smoke is grey puffs lit from the upper left that thin with height.
- HUD panels are ink with a gold trim and lacquer corner studs, so that they sit apart from the world's greens and earths. Bars are crimson for HP and azure for QI.
- The HUD holds the player (portrait, name, sect, age, HP, QI), the map and season, the rumour that last reached the player, the technique bar with cooldowns, and the current target.
- UI text is Korean first, with English supported (ADR 0012). Text is drawn in paper on ink. The mockup's 5-pixel capital font draws English and numerals only; Korean text needs a bitmap font that carries Hangul, which is not chosen yet. Every panel is laid out for the longer of the two strings.

## Do and don't

| Do | Don't |
|---|---|
| Draw every pixel from the master palette | Blend with transparency or add colours outside it |
| Stand bodies and facades upright on the square grid | Draw isometric diamonds or a perspective view |
| Draw a structure over its footprint and let it rise above | Draw a structure larger than its footprint to fit in more detail |
| Give each faction its accent, value and silhouette together | Rely on colour alone to tell factions apart |
| Outline bodies and structures in ink | Outline terrain tiles |
| Light from the upper left, shadow to the lower right | Bake shadows into sprites, or mix light directions in one scene |
| Keep the player its faction's body with markers around it | Give the player a larger or unique sprite |
| Scale by whole multiples | Scale by fractions, which smears pixels |
