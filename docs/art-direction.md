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
- `art/mockup/genealogy_panel.png` — the genealogy panel open over the target screen: a three-generation tree of cards (parents, the steered person and their spouse, their children) joined by gold lines of kin, and a column of the centred person's details, in Korean; `genealogy_panel_4x.png` at 4x. Its Hangul is Galmuri9, drawn from `hud/fonts/Galmuri9.bdf` by `art/mockup/bdf.lua`.
- `art/mockup/sect_panel.png` — the sect panel open over the target screen, in the genealogy panel's frame and cards: the steered person's sect with its seat village's gate and whether the seat is held, its faction, master, elders and members against its cap, the cards of its master and elders, and a column of a faction's leader, how the leader is chosen and its sects, in Korean; `sect_panel_4x.png` at 4x.
- `art/mockup/title_screen.png` — the title screen: `art/title/backdrop`'s ridges of mountains in mist under a low sun and the working title at 4x of the font, and a menu in the HUD's frame with a new game, a greyed continue that waits for saving, settings and quit, in Korean; `title_screen_4x.png` at 4x.
- `art/mockup/new_game_screen.png` — the new-game panel over the dimmed title screen: a seed typed into its field or drawn at random, and the button that starts the world, in Korean; `new_game_screen_4x.png` at 4x.
- `art/mockup/pause_screen.png` — the pause panel over the target screen: a menu to resume, open the settings, leave for the title or quit, the world's seed, and the controls guide of each key's cap beside what it does, in Korean; `pause_screen_4x.png` at 4x.
- `art/mockup/settings_screen.png` — the settings panel over the target screen: time speed, window mode, resolution in whole multiples of the native size, language, and the volume of all sound, the effects and the music in quarters, each a row of choices with the chosen one in jade, and a line saying what the row being changed sets, in Korean; `settings_screen_4x.png` at 4x.
- `art/mockup/mobile_hud_idle.png` — the touch HUD at rest over the target screen's world: the status, leader, rumour and map panels along the top, the genealogy, sect and duel buttons under the status and the pause button in the top-right corner; a virtual joystick at the bottom left and round skill buttons in an arc round the main one at the bottom right, one cooling down, in Korean; `mobile_hud_idle_4x.png` at 4x. Its panels, buttons and the words they hold, each checked for room in Korean and English, are drawn by `art/mockup/touch.lua`.
- `art/mockup/mobile_hud_aim.png` — a skill button dragged to aim: its pad with the thumb's knob, the skill's range ringed round the player, a dotted line to the aimed area in jade, and the cancel zone on the arc's upper arm; `mobile_hud_aim_4x.png` at 4x.
- `art/mockup/mobile_hud_cancel.png` — the same drag carried onto the cancel zone, lit crimson, with the aim greyed; `mobile_hud_cancel_4x.png` at 4x.
- `art/mockup/mobile_hud_self.png` — a long press on a skill button, its ring filled in jade under a slip saying it goes to the player, and a jade ring round the player; `mobile_hud_self_4x.png` at 4x.
- `art/mockup/mobile_hud_targets.png` — `mobile_hud_idle` over the dimmed world with every touch target's outline and size in native pixels, and a legend of a native pixel's size on a phone at the 4x export; `mobile_hud_targets_4x.png` at 4x. It is for review.

The production set. Each `.aseprite` file is drawn by the `.lua` script beside it and exported beside it as a `.png` sheet with a `.json` of its frames and tags, by the command in `docs/convention/stack-godot.md` §Asset export. `art/build.sh` redraws and exports all of it.

| Asset | Frame | Tags |
|---|---|---|
| `art/terrain/terrain` | 16 x 16 | `plain`, `forest`, `hills`, `water`, `mountain`, four full tiles each |
| `art/terrain/transitions` | 16 x 16 | `forest`, `hills`, `water`, `mountain`, 15 tiles each, by corner mask 0 to 14 |
| `art/terrain/landforms` | 64 x 64 | `mountain_2x2`, `mountain_3x2`, `mountain_3x3`, `mountain_4x3`, `hills_2x1`, `hills_2x2`, `hills_3x2`, `hills_3x3` |
| `art/bodies/orthodox`, `demonic`, `unorthodox` | 16 x 24 | `idle` (one frame), `walk` (four frames) |
| `art/structures/house` | 64 x 60 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/hall` | 96 x 60 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/well` | 48 x 48 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/barrels` | 32 x 32 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/fence` | 96 x 32 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/granary` | 48 x 48 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/storehouse` | 64 x 48 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/training_ground` | 112 x 80 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/manual_hall` | 80 x 64 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/seat_gate` | 64 x 64 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/smithy` | 64 x 48 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/apothecary` | 80 x 48 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/decorations/fields` | 16 x 16 | `sown`, `growing`, `ripe`, `fallow`, `burnt` and each with `_winter`, two variants each |
| `art/decorations/ground` | 16 x 16 | `paving` and `road` (four each), `millet` and `rice` (two each) |
| `art/decorations/banner` | 16 x 24 | `orthodox`, `demonic`, `unorthodox` |
| `art/decorations/sect_banner` | 16 x 32 | `orthodox`, `demonic`, `unorthodox` |
| `art/icons/skill_icons` | 16 x 16 | `strike`, `burst` |
| `art/icons/economy_icons` | 16 x 16 | `food`, `stock`, `housing` |
| `art/markers/army_marker` | 64 x 48 | `orthodox`, `demonic`, `unorthodox` |
| `art/markers/player_marker` | 16 x 24 | `ring` (one frame), `chevron` (two frames) |
| `art/title/backdrop` | 480 x 270 | none: one frame, the title screen's mountains and working title |

`art/contact_sheet/contact_sheet.png` shows every frame of the set but the title backdrop, which `art/mockup/title_screen.png` shows, read back from the sources, a scene assembled from them on the grid, a seeded 24 x 12-cell patch of mixed terrain laid as described under Terrain, a farming village, a sect's seat and a grown village with its workshops assembled from them; `contact_sheet_4x.png` at 4x. It is for review, and Godot does not import it.

`art/icon/icon.aseprite`, drawn by `icon.lua` and exported as a 32 x 32 `icon.png` tagged `icon`, is the application icon: a jian planted before the title screen's ridges and low sun, in the HUD's frame. The PC exports scale it by nearest neighbour, and the contact sheet leaves it out.

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

| Kind | Passable | Full tile | Where it meets the plain | Landforms |
|---|---|---|---|---|
| Plain | yes | Grass in three tones with tufts and a rare flower | (the ground under every other kind) | none |
| Forest | yes | Separate tree crowns from above, lit from the upper left with a dark rim, the understorey and the odd trunk between them | A dark canopy edge and a shadow on the grass | none |
| Hills | yes | Upland grass a step lighter than the plain, with a faint contour arc | A contour, dark on the lower-right side and lit on the upper-left | Clusters of low mounds, 2 x 1 to 3 x 3 cells |
| Water | no | Blue with a little deep-water grain and short light ripples | Foam, then a bank of wet earth | none |
| Mountain | no | Fractured stone and scree | A lit lip above, a dark cliff foot below, a shadow on the grass | Massifs of sharp peaks, 2 x 2 to 4 x 3 cells |

- A tile carries no outline, so tiles of one kind join into one surface.
- Full tiles carry no feature larger than a tuft, a crown or a ripple, and near their edge all four variants are the same grain, so a stretch of one kind shows neither seams nor a feature per cell.
- Features larger than a cell are landforms: mountain massifs and hill clusters, each over a footprint of several cells, standing on a stretch of their kind and drawn with everything standing, by the bottom edge of the footprint. A landform's frame is 64 x 64 with the footprint at its bottom-left; what rises above the footprint is drawn above it in the frame.
- Water and mountain, the two impassable kinds, carry no green, so passability reads from colour family alone.

Transitions:

- Every kind but the plain is laid over the plain on a dual grid, half a cell up and left of the cells, so each tile's four corners sit on four cells' centres.
- A tile's corner mask adds top-left 1, top-right 2, bottom-left 4 and bottom-right 8 for each of those four cells that is of the kind. Masks 1 to 14 are the transition tiles, frame mask of the kind's tag in `art/terrain/transitions`; mask 15 is one of the kind's full tiles; mask 0 is empty. The 16 tiles are the set Godot's Match Corners terrain mode describes.
- Within a transition tile, the kind's area is its corners blended across the tile with noise that repeats every 16 pixels, so neighbouring tiles agree all along their shared edge. The rest is transparent, so the plain shows.
- Where two kinds other than the plain meet, each meets the plain, and a strip of plain shows between them, read as a bank or a clearing.
- How Godot lays them is in `docs/convention/stack-godot.md` §Terrain layers.

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

The village house covers 3 x 3 cells and the hall 5 x 3, each in four stages that follow the core's structure stages:

| Stage | What shows |
|---|---|
| Intact | Slate roof with ridge and upturned eaves, plaster wall between lacquered pillars, door and lattice windows, stone plinth |
| Damaged | Holes in the roof showing rafters, scorch on the wall, the door broken in, fallen tiles in front |
| Destroyed | Roof fallen in with one slab left, charred beams, wall broken to stumps, debris at the door |
| Rubble | A low heap of tile, stone and burnt beams on the plinth, weeds returning |

- The silhouette shrinks with each stage, and so does the cast shadow, so a stage reads from its outline before its detail.
- The plinth stays in every stage, so the footprint stays readable after the building is gone.
- Fire and smoke over a burning building are effects drawn over the `destroyed` frame, not part of it.

The village is furnished as the approved mockup's is. What blocks movement or breaks is a structure, drawn in the same four stages; what does neither is decoration, which never changes:

| Prop | Kind | Footprint | Look |
|---|---|---|---|
| Well | Structure | 2 x 2 | A stone ring round dark water under a small tiled roof on two posts, a bucket on its rope; damaged loses roof tiles and stones, destroyed drops the roof and snaps a post, rubble is a ring of stones |
| Barrels | Structure | 1 x 1 | Two barrels with one on top; damaged breaks one, destroyed splits them and spills grain, rubble is staves on the ground |
| Fence | Structure | 5 x 1 | Posts every 8 pixels with two rails; damaged and destroyed lose rails and posts, rubble is posts lying down |
| Paving | Decoration | the square | Slabs of 8 pixels in offset courses, a light slab here and there |
| Road | Decoration | a row through the village | Packed earth with two wheel ruts |
| Fields | Decoration | plots south of the square | Millet in amber rows or rice in green rows over water, an earth border at each plot's edge |
| Banner | Decoration | one cell | The owner's accent on a pole, with a white mark |
| Granary | Structure | 2 x 2 | A plank store raised on stilts over stone footings, a ladder to its door, under a hipped straw roof with a dark ridge and a ragged fringe; damaged holes the straw and scorches the planks, destroyed burns the roof through to the beams and breaks the walls, rubble is a heap of straw, planks and stone |
| Storehouse | Structure | 3 x 2 | A long plastered store on a stone plinth, iron-studded double doors and vents, no windows, a crate by the door, under a low slate roof; its stages break it as the house's break the house |
| Field | Decoration | a plot | Rows of tilled soil four pixels apart, an earth border at the plot's edge, in five states: sown (dark soil with seeds showing), growing (green shoots in rows), ripe (amber grain), fallow (grass and weeds over old rows), burnt (black soil, ash, charred stubble and embers); in winter each lies under snow gathered in its furrows |

- Every frame holds its footprint at (8, 8), as the house's does, except the seat gate's (see below).
- Ground decorations are tiles laid over the terrain and under everything standing; the banner stands and is sorted with bodies and buildings.

A sect's seat is a village of its own (ADR 0026), laid out with the village's buildings and these of its own, so that a seat reads apart from a farming village at a glance:

| Prop | Kind | Footprint | Look |
|---|---|---|---|
| Training ground (연무장) | Structure | 6 x 4 | An open yard of raked sand in a stone curb, a ring trodden where pupils spar; two weapon racks of spears, glaives and staves and three wooden dummies along its back row, plum-blossom poles and stone weights on the sand; damaged scorches the sand, knocks a rack flat and snaps a dummy, destroyed burns the racks to their bare frames and the dummies and poles to stumps, rubble is grass and ash inside the curb with a heap of burnt wood at the back |
| Manual hall (장경각) | Structure | 4 x 3 | A two-storey library on a high stone platform with steps: latticed shutters over paper and a double door between lacquered pillars below, a gold-framed name plaque under the upper roof, two slate roofs with upturned eaves; damaged holes both roofs, scorches the wall and blows pages onto the steps, destroyed takes the upper storey and drops the lower roof in, rubble is a heap on the platform |
| Seat gate | Structure | 3 x 1 | A one-bay paifang: two lacquered pillars on stone bases, a painted bracket band, a gold-framed name board between the lintels, a slate roof with upturned eaves; damaged holes the roof and cracks the board, destroyed drops the roof, breaks one pillar and lays the board in the dust, rubble is the two bases and a burnt beam |
| Sect banner | Decoration | one cell | A tall pole on a stone foot with a crossbar, a long paper-white streamer edged in the owner's accent, two ink characters down it and a swallowtail with tassels; the faction's banner is a short pennant of the accent itself |

- The training ground is laid with the ground, over the terrain and under everything standing, so people stand and spar on it. What on it rises more than a few pixels, the racks and the dummies, stands along its back row, where anyone in front of it is drawn over it.
- The seat gate rises 32 rows above its one-row footprint, so its frame holds the footprint at (8, 40), its bottom margin 8 rows as every frame's. Its pillars stand on its two end cells and the middle cell is the opening.
- The stone curb, the platform and the gate's bases stay in every stage, as the plinth does.
- The court before the halls is paved, and a road runs down from it to the gate's opening; both are ground decorations, laid as a village's paving and road are.

As a village grows, its people build workshops among its houses (`docs/concept.md` §Growing civilization). What each does is not yet designed; their art is drawn ahead of the rule:

| Prop | Kind | Footprint | Look |
|---|---|---|---|
| Smithy (대장간) | Structure | 3 x 2 | A forge shed on a stone plinth under a low slate roof, its left bay open on a stone hearth of white-hot coals whose glow is dithered over the dark inside and the plinth, bellows beside it, a stone chimney rising from the hearth through the roof; a plank wall hung with tongs and hammers in its right bay; an anvil on a stump, a quench tub of water and a heap of charcoal before it; damaged holes the roof, knocks the chimney's crown off, scorches the planks and dims the coals, destroyed drops the roof into the shed, breaks the chimney half down and burns the plank wall to stumps with a few embers left, rubble is a heap with the hearth and the chimney's stub standing out of it |
| Apothecary (약방) | Structure | 4 x 2 | A plastered shop on a stone plinth under a slate roof over its left three cells: a counter window on a cabinet of small drawers with gold pulls, jars and a mortar on the sill, herbs hung under the eave, a door under a split cloth curtain and a name board; in the yard at its right a herb rack of two posts and two bars hung with bundles, a drying tray of woven bamboo on trestles and a round tray on the ground, herbs spread on both; damaged holes the roof, scorches the wall, pulls drawers out and snaps the rack's lower bar onto the ground, destroyed drops the roof in and burns the walls to stumps, the rack to two posts and the tray where it fell, rubble is a heap on the plinth and ash in the yard |

- The anvil outlasts the smithy: it stands on its stump intact and damaged and lies on its side, destroyed and rubble.

## Effects and HUD

- Effects are brief and bright over calm ground: an area skill is a ring of force in its user's accent ramp with a white-hot core, cracks on the ground and sparks; sword qi is a crescent in the azure ramp; damage numbers rise in the colour of the technique that dealt them, with an ink shadow.
- Fire uses the fire ramp; smoke is grey puffs lit from the upper left that thin with height.
- HUD panels are ink with a gold trim and lacquer corner studs, so that they sit apart from the world's greens and earths. Bars are crimson for HP and azure for QI, each under its label and its value.
- The HUD holds the player (portrait, name, sect, age, HP, QI), the map and season, the rumour that last reached the player, the technique bar with cooldowns, and the current target.
- The economy icons, at the technique icons' scale: food is a lacquered bowl of rice with chopsticks, stock a tied sack of grain, housing a house under its slate roof.
- A technique slot shows its number key in its top-left corner over an icon of the skill's shape: a blade with an arc of sword qi for a strike at one body, an open palm in a ring of force for a burst that reaches the bodies round its target.
- UI text is Korean first, with English supported (ADR 0012). Text is drawn in paper on ink. The game's text is Galmuri9 at 10 pixels, a pixel font that carries Hangul and Latin (`hud/fonts/`); the mockup's 5-pixel capital font draws English and numerals only. Every panel is laid out for the longer of the two strings.

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
