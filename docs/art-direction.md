# Art direction

How Murim24 looks: the screen, the palette, bodies, structures, effects and the HUD, written so that an artist can make matching art from this file and the mockups it names.

This is the draft taken from the target mockup (issue #33, phase 1). The production set (phase 2) completes it once the user has approved the mockup.

## Reference images

- `art/mockup/target_screen.png` — one finished-game screen at native size; `target_screen_4x.png` is the same at 4x.
- `art/mockup/lineup_sheet.png` — the bodies of each faction, villagers and the player, the village house in its four damage stages and the aggregate-army marker; `lineup_sheet_4x.png` at 4x.
- Each image is drawn by the `.lua` script of the same name beside it, saved as `.aseprite`, and exported by `art/mockup/build.sh`. Running the script again redraws the same pixels.

## Screen and grid

| Property | Value |
|---|---|
| Native resolution | 480 x 270 pixels (16:9) |
| Scale | Whole multiples only: 4x fills 1920 x 1080 |
| Grid | One core cell to one 16 x 16 tile (ADR 0008); 30 cells across, 16.875 down |
| View | Orthogonal top-down: ground and roofs seen from above, the front face of bodies and buildings drawn upright, no isometric diamond and no depth projection |
| Light | From the upper left; every standing thing casts a shadow to the lower right |

A body stands about 10 x 15 pixels inside its cell and may overlap the cell above. A house covers 3 x 3 cells, a hall 5 x 3.

## Mood and references

The world is the gangho of wuxia fiction in daylight: river villages, tiled roofs, rice and millet fields, roads where sects meet. What happens on it is violent and the ground stays calm, so that the violence reads.

References, described in words:

- Ink-wash landscape painting: muted greens, slate roofs, mist and smoke in grey rather than colour.
- Wuxia film fight staging: bright, short-lived arcs of sword qi and palm force over still ground.
- Top-down 16-pixel console RPGs: one outline colour, three-to-five-step ramps, readable at a glance.

## Palette

Every pixel comes from the master palette, defined in `art/mockup/pixel.lua` as ramps that run dark to light. Shading moves along a ramp, never off it, and nothing is blended by transparency: smoke and glows are dithered.

| Ramp | Hex, dark to light | Use |
|---|---|---|
| ink | `14101a` `2a2433` `3d3647` `524a5e` | Outlines, hair, HUD panels |
| stone | `3b4250` `4a5361` `66707d` `8a94a0` `b4bcc4` `dfe3e6` | Roofs, paving, rock, smoke |
| grass | `1f3a2b` `2c4a32` `3b6138` `4f7a3f` `6b9447` `8fae5a` | Ground cover, rice |
| leaf | `142a22` `1e3328` `284536` `35593d` `4a7044` | Tree canopies |
| earth | `3a2c24` `4a3a2c` `6b523a` `8c6d4a` `b08d60` `d1b787` | Roads, banks, straw hats |
| wood | `2e1f1a` `3b2a22` `5c3d2a` `7d5535` `9c7048` | Doors, beams, bridge, weapons' hafts |
| water | `1a2f40` `1f3a4d` `2b5770` `3d7a8f` `5fa3a8` `a6d4cf` | River, foam |
| crimson | `3d0d14` `5c1418` `9c1f24` `d63a2f` `f06a3c` | Demonic Cult accent, HP |
| azure | `152a55` `1f3d7a` `2f63b8` `5b9ae0` `a8d0f5` | Orthodox accent, QI |
| amber | `4a2e0c` `6b4310` `b07418` `e8a830` `f5d27a` | Unorthodox accent, millet |
| skin | `5c3a28` `8a5a3c` `c48a62` `e8b98e` | Skin |
| cloth | `6b6052` `8c7f68` `b4a68a` `d9cdb0` | Villagers' clothes |
| robe | `8a94a0` `c8ccd0` `f4f1e6` | Orthodox white robes |
| fire | `9c1f24` `f06a3c` `f5a33c` `fbe37a` `fff8dc` | Flames, impact cores |
| jade | `1f5c4a` `2f8c6e` `4fc49a` `a6f0cf` | The player's marker, bamboo, selection |
| gold | `5c4418` `8c6a2a` `c99a3c` `f0d080` | HUD trim, the player's chevron |
| paper | `a8946c` `d9c9a3` `efe3c2` | Plaster walls, HUD text |
| lacquer | `3d1418` `7a2a24` `a83a2c` | Pillars, HUD corners |

Faction accents, chosen to stay apart at one cell and against grass, road and water:

| Faction | Accent | Body value |
|---|---|---|
| Orthodox | azure `2f63b8` | Light: white robes |
| Demonic Cult | crimson `d63a2f` | Dark: charcoal clothes |
| Unorthodox | amber `e8a830` | Middle: brown clothes under straw |
| Player | jade `4fc49a` | Its own faction's, with a jade sash |

## Bodies

- One body template for every faction; a faction changes the palette slots, the headgear and the weapon. A faction is told apart by three cues at once, so that any one of them is enough: accent colour, body value (light, dark, middle) and silhouette.
- Silhouettes: Orthodox topknot or bun and a long robe; Demonic Cult spiky hair under a crimson headband, or a hood; Unorthodox the wide conical straw hat.
- Villagers wear the cloth ramp and carry no faction accent.
- Every body has a one-pixel ink outline around its whole silhouette, weapon included, and a two-step contact shadow.
- A knocked-down body is the same sprite lying on its side, without a shadow.
- The player is a body like any other (`docs/concept.md` §Identity) and is told apart only by markers drawn around it: a jade ring on the ground and a gold chevron above the head.

An individual agent and an aggregate army read differently:

| | Individual agent | Aggregate army |
|---|---|---|
| Drawn as | One full body per agent | One block standing for the whole army |
| Detail | Face, weapon, pose | Rows of heads in the faction's colours, no faces |
| Frame | None | Accent-coloured bracket corners, the faction's banner |
| Number | None | A plate with the army's name and count |

## Structures and damage stages

The village house in its four stages, shown side by side on `art/mockup/lineup_sheet.png`:

| Stage | What shows |
|---|---|
| Intact | Slate roof with ridge and upturned eaves, plaster wall between lacquered pillars, door and lattice windows, stone plinth |
| Damaged | Holes in the roof showing rafters, scorch on the wall, the door broken in, fallen tiles in front |
| Destroyed | Roof fallen in with one slab left, charred beams, wall broken to stumps, debris at the door; flames and smoke while it burns |
| Rubble | A low heap of tile, stone and burnt beams on the plinth, weeds returning, a short shadow |

The silhouette shrinks with each stage, and so does the cast shadow, so a stage reads from its outline before its detail.

## Effects and HUD

- Effects are brief and bright over calm ground: an area skill is a ring of force in its user's accent ramp with a white-hot core, cracks on the ground and sparks; sword qi is a crescent in the azure ramp; damage numbers rise in the colour of the technique that dealt them, with an ink shadow.
- Fire uses the fire ramp; smoke is grey puffs lit from the upper left that thin with height.
- HUD panels are ink with a gold trim and lacquer corner studs, so that they sit apart from the world's greens and earths. Text is a 5-pixel capital font in paper; bars are crimson for HP and azure for QI.
- The HUD holds the player (portrait, name, sect, age, HP, QI), the map and season, the rumour that last reached the player, the technique bar with cooldowns, and the current target.

## Do and don't

| Do | Don't |
|---|---|
| Draw every pixel from the master palette | Blend with transparency or add colours outside it |
| Keep each body inside its cell, front face upright | Draw isometric diamonds or a perspective view |
| Give each faction its accent, value and silhouette together | Rely on colour alone to tell factions apart |
| Outline bodies and structures in ink | Outline terrain tiles |
| Light from the upper left, shadow to the lower right | Mix light directions in one scene |
| Keep the player a normal body with markers around it | Give the player a larger or unique sprite |
| Scale by whole multiples | Scale by fractions, which smears pixels |
