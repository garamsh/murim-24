# Art direction

How Murim24 looks: the screen, the palette, terrain, bodies, structures, effects and the HUD, written so that an artist can make matching art from this file and the images it names.

## Contents
- Sources
- Screen and grid
- Mood and references
- Palette
- Terrain
- Bodies and markers
- Scale
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
- `art/mockup/arts_panel.png` — the arts panel open over the target screen on PC: the realm on a ladder of the eight realms, the qi left against the store, the heart method and its nature; a list of each art known by its nature, every skill with its qi cost, the realm it needs, its cooldown and the key it is on, greyed when not yet learned and its realm in crimson when not reached; and the loadout of five slots on Q W E R T, in Korean; `arts_panel_4x.png` at 4x. Its data and words, each checked for room in Korean and English, are drawn by `art/mockup/arts.lua`.
- `art/mockup/arts_panel_touch.png` — the arts panel on a phone: each art's skills as 42-pixel tiles of their icons, the realm and qi, and the tapped skill's details beside them, with the skill arc left clear and ringed in jade, waiting for the button the tapped skill goes on; `arts_panel_touch_4x.png` at 4x.
- `art/mockup/angle/`, drawn by `art/mockup/angle.gd` (#204, evidence/pr-204) — the world at Albion's angle (ADR 0059), the reference the game's view is ported to: ground off the cell grid (`angle/ground.gdshader`), levels with rock faces, trees, the buildings and bodies redrawn at the angle, and the four region screens.
- `art/mockup/mobile_hud_arts.png` — `mobile_hud_idle` with the qi left against the store and the realm in the status, an arts button beside the genealogy, sect and duel buttons, and the loadout on the arc; `mobile_hud_arts_4x.png` at 4x.

The production set. Each `.aseprite` file is drawn by the `.lua` script beside it and exported beside it as a `.png` sheet with a `.json` of its frames and tags, by the command in `docs/convention/stack-godot.md` §Asset export. `art/build.sh` redraws and exports all of it.

| Asset | Frame | Tags |
|---|---|---|
| `art/terrain/ramps` | 7 x 1 | One per kind of ground, by its name in `world_art/relief.gd`'s order, then `bank`, the earth of a bank's face: its seven colours, dark to light |
| `art/terrain/detail` | 5 x 5 | `tuft`, `flower` (three: violet, yellow and white), `litter`, `reed`; root at (2, 3), the ink ramp standing for the ground's ramp's steps |
| `art/terrain/trees` | 32 x 40 | `tree` (three), `pine`, `bamboo`, `willow`, `poplar`, `snow_pine` (two each), each foot at (16, 36) |
| `art/bodies/orthodox`, `demonic`, `unorthodox` | 16 x 24 | `idle` (one frame), `walk` and `back` (four frames each), `strike` (two), `idle_hurt` and `walk_hurt` (one and four), `idle_maimed` and `walk_maimed` (one and four), and each again with `_xray`, the silhouette's outline in the colour §Bodies and markers gives it; feet on row 22 |
| `art/structures/house` | 64 x 92 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/hall` | 96 x 98 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/well` | 48 x 66 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/barrels` | 32 x 50 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/fence` | 96 x 34 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/granary` | 48 x 74 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/storehouse` | 64 x 72 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/training_ground` | 112 x 82 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/manual_hall` | 80 x 116 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/seat_gate` | 64 x 68 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_wall_south`, `town_wall_north` | 32 x 64 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_wall_west`, `town_wall_east` | 48 x 48 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_corner_tower` | 64 x 92 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_gate_tower` | 96 x 88 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_steps` | 32 x 58 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_temple` | 80 x 96 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_market_stall` | 48 x 50 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/town_courtyard_house` | 96 x 106 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/smithy` | 64 x 58 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/structures/apothecary` | 80 x 52 | `intact`, `damaged`, `destroyed`, `rubble` |
| `art/decorations/fields` | 16 x 12 | `sown`, `growing`, `ripe`, `fallow`, `burnt` and each with `_winter`, two variants each |
| `art/decorations/ground` | 16 x 12 | `paving` and `road` (four each), `millet` and `rice` (two each) |
| `art/decorations/banner` | 16 x 24 | `orthodox`, `demonic`, `unorthodox` |
| `art/decorations/sect_banner` | 16 x 32 | `orthodox`, `demonic`, `unorthodox` |
| `art/icons/skill_icons` | 16 x 16 | `strike`, `burst` |
| `art/icons/economy_icons` | 16 x 16 | `food`, `stock`, `housing` |
| `art/icons/ware_icons` | 16 x 16 | `herb`, `elixir`, `silver` |
| `art/markers/army_marker` | 64 x 48 | `orthodox`, `demonic`, `unorthodox` |
| `art/markers/player_marker` | 16 x 24 | `ring` (one frame), `chevron` (two frames) |
| `art/markers/shadow` | 16 x 8 | `shadow`: an ink ellipse by the ordered dither, densest at its middle and thinning to its rim, centred on (8, 4) |
| `art/title/backdrop` | 480 x 270 | none: one frame, the title screen's mountains and working title |

`art/contact_sheet/contact_sheet.png` shows every frame of the set, read back from the sources under each sheet's name and tags, but the title backdrop, which `art/mockup/title_screen.png` shows, the icon and the ground's ramps, which are outside the master palette; `contact_sheet_4x.png` at 4x. It is for review, and Godot does not import it. What the frames make assembled is shown by the game's own captures in each pull request's evidence.

`art/icon/icon.aseprite`, drawn by `icon.lua` and exported as a 32 x 32 `icon.png` tagged `icon`, is the application icon: a jian planted before the title screen's ridges and low sun, in the HUD's frame. The PC exports scale it by nearest neighbour, and the contact sheet leaves it out.

## Screen and grid

| Property | Value |
|---|---|
| Native resolution | 480 x 270 pixels (16:9) |
| Scale | Whole multiples only, filling the window: on a PC, the largest showing at least 640 x 360 (3x at 1920 x 1080); with the touch controls, at least 480 x 270 (4x at 1920 x 1080). The HUD on a PC is drawn at half the world's multiple, rounded up |
| Grid | One core cell drawn 16 pixels wide and 12 deep (ADR 0059); at least 40 cells across and 30 down on a PC, 30 and 22.5 with the touch controls |
| Levels | Ground on level `n` is raised 16 pixels times `n`: water at -1, plain and forest at 0, hills rising as slopes to 1 and 2, mountains in steps of 2, 4 and 5. A face drops where a level ends, and only where the ground cannot be crossed |
| View | Albion's angle (ADR 0059): ground and roofs seen from above, foreshortened down the screen; facades and standing bodies drawn upright; no isometric diamond and no perspective |
| Draw order | Everything standing is drawn in order of the row its footprint ends on, back to front, the raised rows of the ground among it |
| Light | From the upper left; shadows fall to the lower right |

- A body stands with its feet, row 22 of its frame, on the point the core gives it.
- A structure's sprite covers its footprint and may rise above it; a body, standing on a point, has none. The house's footprint is 3 x 3 cells, 48 x 36 at the angle, its bottom-left corner 8 pixels in from its frame's left and bottom edges; the rows above hold its front and roof, and the margin round it the upturned eaves and spilled debris.
- No shadow is in a body's or a structure's sprite. The view draws them on the ground: `art/markers/shadow` under a body's feet; and past a standing structure's right and bottom edges, a band 6 pixels wide, 3 once it is destroyed and none from rubble. Both are soft contact shadows, ink by the ordered dither: densest at the body's middle or against the wall, at most four pixels in five, and thinning outward to about one in five.

## Mood and references

The world is the gangho of wuxia fiction in daylight: river villages, tiled roofs, fields, roads where sects meet. What happens on it is violent and the ground stays calm, so that the violence reads.

References, described in words:

- Ink-wash landscape painting: muted greens, slate roofs, mist and smoke in grey rather than colour.
- Wuxia film fight staging: bright, short-lived arcs of sword qi and palm force over still ground.
- 16-pixel console RPGs seen from above, readable at a glance, drawn with modern pixel art's sel-out outlines and hue-shifted ramps (ADR 0069).

## Palette

The ground is the exception: it takes the ramps of Terrain below, and its shadows lean toward a dark blue (`141828`). Everything else is drawn from the master palette.

Every other pixel comes from the master palette, defined in `art/pixel.lua` as ramps that run dark to light. Shading moves along a ramp, never off it, and nothing is blended by transparency: smoke and glows are dithered. `art/contact_sheet/contact_sheet.lua` refuses a source holding a colour outside the palette.

Each material's ramp is built from its base colour, its middle step, by `art/pixel.lua`'s `build_ramp` (ADR 0069), in seven steps. A step `u` from the middle runs from -6/7 to 6/7:

| What changes | In shadow (`u` < 0) | In light (`u` > 0) |
|---|---|---|
| Lightness | falls toward black by 0.62 `u` | rises toward white by 0.8 `u` |
| Hue | turns toward 250, blue-violet | turns toward 55, yellow |
| How far the hue turns | at most half the way at 0.43 `u`, and at most 30° `u` | the same |
| Saturation | the base's times 1.06, raised by 0.285 `u` | the base's times 1.06, lowered by 0.2375 `u` |

A grey base takes saturation 0.114 `u` of the target hue. Then, at the step's own lightness, a shadow is mixed toward a blue-violet, `3a2a7a`, by 0.55 `u`, and a light toward a warm yellow, `ffe7a0`, by 0.35 `u`. So even a grey material's shadows read cool and its lights warm. Fire is an emitter's ramp and is drawn by hand.

| Ramp | Base | Hex, dark to light | Use |
|---|---|---|---|
| ink | `3d3647` | `171229` `221b35` `2f273f` `3d3548` `716273` `a28b93` `d2c0b3` | Hair, HUD panels, the HUD's outlines |
| stone | `8a94a0` | `36315b` `4d4d73` `69708d` `8994a1` `aab1b0` `caccbf` `e9e5d3` | Roofs, rock, mountains, smoke |
| grass | `4f7a3f` | `1f2838` `2d4a43` `39603d` `4e7c3d` `7eab58` `b2c489` `dedfb9` | Plains, hills |
| leaf | `284536` | `111122` `181f2e` `213538` `274636` `517856` `83ae76` `c8d5ab` | Forest canopy |
| earth | `8c6d4a` | `3a2242` `563545` `6d4b43` `8e6d48` `b39467` `cdb991` `e7daba` | Roads, banks, straw hats |
| wood | `5c3d2a` | `22132c` `391d32` `46282b` `5e3d29` `946c48` `c39f6f` `e1cea7` | Doors, beams, hafts |
| water | `3d7a8f` | `1c1e44` `25355f` `2f5579` `3b7b91` `61aeb1` `91c7b4` `cbddc0` | Water, ripples |
| crimson | `9c1f24` | `3d1344` `631648` `831738` `a01b21` `d6422e` `e1906e` `efcca9` | Demonic Cult accent, HP |
| azure | `2f63b8` | `1a1a53` `1e2b77` `23439b` `2b62bc` `5c99ca` `9ac4cd` `c9decb` | Orthodox accent, QI |
| amber | `b07418` | `441933` `5d2625` `89461c` `b57513` `e2a32c` `e9c36d` `f3dfaa` | Unorthodox accent |
| skin | `c48a62` | `622854` `85394b` `ab5747` `c7895f` `d4ac84` `e3cba6` `f2e5c8` | Skin |
| cloth | `b4a68a` | `54395c` `765763` `987d6e` `b5a689` `c9bda1` `ddd4b9` `f0e9d1` | Villagers' clothes |
| robe | `c8ccd0` | `4d4878` `717197` `9ea0b2` `c8ccd0` `d9dad5` `eae8dc` `f7f4e6` | Orthodox white robes |
| fire | by hand | `9c1f24` `f06a3c` `f5a33c` `fbe37a` `fff8dc` | Flames, impact cores |
| jade | `4fc49a` | `213e60` `2b7287` `37ab9f` `4bc89b` `7acf9b` `a8d9a5` `deebc9` | The player's ring, selection |
| gold | `c99a3c` | `532741` `723c37` `a36734` `cd9b38` `d7b667` `e3ce93` `f2e5be` | HUD trim, the player's chevron |
| paper | `d9c9a3` | `733f5a` `9f6357` `bc9a7d` `dbcaa1` `e4d8b5` `efe5c9` `f8f2dd` | Plaster walls, HUD text |
| lacquer | `7a2a24` | `2e1337` `4d193c` `661c2e` `7d2821` `b2563d` `d4926b` `e9cba6` | Pillars, HUD corners |
| violet | `6a3290` | `241546` `371c61` `4f247b` `6b2f93` `a652b9` `cb88bd` `e4b6bf` | Demonic Cult body outline |

- **Outlines are sel-out.** A structure, a banner, a tree or the army marker has a one-pixel ring round its silhouette. Each of its pixels is the darkest step of the ramp of the pixel it borders. Where the ring lies left of that pixel or above it, toward the light, it is the second step instead. A body's whole ring is the darkest step of its garment's ramp, robe, ink or earth by faction, so no edge takes a step of its skin or hair and the figure stays dark against the ground. The HUD's pieces, the icons, the player's markers, the title and the application icon keep an outline in ink, `171229`, so they read against anything. `art/selout_test.gd` reads every world sheet exported and refuses an edge in that ink where it borders no ink, such as hair.
- **The former palette.** The former ramps, of three to six steps with a single ink outline, were mapped to these by `art/palette_map.lua`: each former step went to the new step nearest it in lightness, keeping the steps in order. The view's and the HUD's colours moved by the same map.
- **The mockups.** They draw from `art/mockup/pixel.lua` and `art/mockup/sprites.lua`, frozen copies of the former palette and body template. The production sheets five of them read, the bodies, the seat gate, the skill icons and the title backdrop, are frozen whole under `art/mockup/frozen/`, copied from main at 1aa9509.

Faction accents, chosen to stay apart at one cell and against every terrain kind:

| Faction | Accent | Body value |
|---|---|---|
| Orthodox | azure `2b62bc` | Light: white robes |
| Demonic Cult | crimson `d6422e` | Dark: charcoal clothes |
| Unorthodox | amber `e2a32c` | Middle: brown clothes under straw |
| Player | jade `4bc89b`, in its markers only | Its faction's body, unchanged |

## Terrain

The ground is not drawn as tiles. `world_art/ground.gdshader` draws it off the cell grid from each cell's kind of ground, level, face material and slope (ADR 0059), and the view bakes it once per 32-cell area. What an artist draws for it is not whole tiles but each kind's materials in `art/terrain/`: its ramp (`ramps`), the detail scattered over it (`detail`) and the trees (`trees`). The shader makes the texture's grain and the faces' strata from the kind's ramp.

| Kind | Where | Ramp, dark to light, hue in degrees, saturation, lightness | Surface | Rim |
|---|---|---|---|---|
| Plain | Plain cells | hue 100, 0.32, 0.42 | Grass in clustered texture and large colour patches | None of its own |
| Hills | Hills cells, as slopes | hue 70, 0.30, 0.47 | As the plain, a step lighter and yellower | As the plain |
| Forest | Forest floor | hue 130, 0.38, 0.27 | Leaf litter under clustered trees | Darkest step along its edge, a narrow dither where it meets other ground |
| Water | Water cells, a level down | hue 192, 0.42, 0.42 | Deep-water grain | Shallows and foam at the shore, reeds |
| Rock | Mountain cells | hue 220, 0.12, 0.46 | Two greys with moss in clusters | A lit lip at each drop |

- Each ramp has seven steps, built by §Palette's rule (ADR 0069) from the colour of the hue, saturation and lightness in the table above, and an earth bank's from hue 28, 0.35, 0.36. It differs from a material's in two ways, so the ground stays calm under what stands on it:
  - its steps span only 0.13 of lightness below that colour and 0.12 above;
  - its shadows and lights are mixed toward the cool and warm tints by a third as much, 0.18 and 0.12.
  `art/terrain/ground.lua` draws them.
- The shader's grain takes four adjacent steps about the ramp's middle, the second to the fifth, so the plain reads as one surface under what stands on it. Water's grain takes three tones of the dark half, and its shallows and foam the light steps. Rock's strata take every step from the third up.
- An edge between kinds is the cells' values interpolated through a point warped by noise, so it wanders over the cells' sides and never runs along one. The noise varies slowly enough that the warp never folds the ground over, so no island of one kind lies inside another. Where two kinds meet, a dither a few pixels wide joins them.
- A face drops where the ground drops a level and the ground cannot be crossed. Rock faces have strata of uneven thickness that tilt along the face, split by fissures, with plants over the lip and scree piled at the foot. Water's banks are earth. Each face has a lit lip above and a shadow at its foot.
- Detail is scattered per area by Poisson-disc spacing, denser near an edge between kinds: tufts and flowers on grass, litter on the forest floor and reeds at the water's edge. A detail frame's ink pixels take the ramp of the ground it lies on, or the forest's for litter and reeds.
- Decorative trees (`art/terrain/trees`) cluster inside forests and thin toward their edges, with a lone tree now and then in the open. A shadow lies on the ground round each one's foot, and none stands on a village or a structure.
- Fields are tiles a cell each over the ground (`art/decorations/fields`), since their state changes. What people build in straight lines is drawn over the ground as well, not in it: a village's paving and road are tiles a cell each (`art/decorations/ground`).

## Bodies and markers

- One body template for every faction; a faction changes the palette slots, the headgear and the weapon. A faction is told apart by three cues at once, so that any one of them is enough: accent colour, body value (light, dark, middle) and silhouette.
- Silhouettes: Orthodox topknot and a long robe; Demonic Cult spiky hair under a crimson headband; Unorthodox the wide conical straw hat.
- Every body has a one-pixel sel-out outline round its whole silhouette, weapon included (§Palette).
- The body faces the viewer, turned to the right; the view mirrors it to face left.
- Walk: a step, a passing frame a pixel higher, the other step, a passing frame, at 150 ms a frame. Idle is the standing frame. A body walking more away from the viewer than across shows its back, the head seen from behind; one that has just struck shows the weapon swung, then the standing frame, for 200 ms.
- Every standing body casts `art/markers/shadow` on the ground under its feet; a falling body casts none.
- A body something standing in front of it hides, a building, a tree or a raised row of the ground, is drawn behind it, and its outline frame is drawn over what hides it, only where the body itself does not show, even while it flashes struck. The outline is the faction's light accent, azure `5c99ca` and amber `e2a32c`, and for the Demonic Cult violet `a652b9`: crimson stays for hostility, so an outline never reads as the hostile ring.
- Villagers wear the cloth ramp and carry no faction accent (approved line-up; not yet in the production set).
- A knocked-down body is the same sprite lying on its side, without a shadow (approved line-up; not yet in the production set).
- The player is a body like any other (`docs/concept.md` §Identity), drawn with its faction's sprite. Two markers tell it apart: the jade `ring`, drawn under the body round its feet, and the gold `chevron`, drawn over it above the head and bobbing a pixel every 400 ms.
- A body whose faction is hostile to the player's, or at war with it, is not ringed: its red name tells it (§Effects and HUD), as the owner asked in #236. The only mark round another person is the target's.
- A wounded body shows it. Below half of the most health it has had, it stands and walks stooping a pixel forward and down, a blood stain on its chest (`_hurt`). Below a quarter the stain runs down its side and leg, the last step of its walk drags as it limps, and it drips now and then (`_maimed`). The stain is the crimson ramp's darker steps, on solid pixels only, so the outline stays whole. Back and strike frames show no wound. Wounds clear as health recovers.

An individual agent and an aggregate army read differently:

| | Individual agent | Aggregate army |
|---|---|---|
| Drawn as | One full body per agent | One 4 x 3-cell marker standing for the whole army |
| Detail | Face, weapon, pose | Rows of heads in the faction's colours, no faces |
| Frame | None | Accent-coloured bracket corners round the block, the faction's banner above it |
| Number | None | A plate under it with the army's name and count, drawn by the HUD as text |

## Scale

Places, buildings and land follow real proportions, measured against a person, who fills a cell (`docs/concept.md` §Identity). These are the base pack's proposed sizes, data a pack may change (ADR 0049). Each layout patch, art round and growth rule is checked against them.

The li of ADR 0047 measures the distance between places, and a place's layout patch carries the size of what stands there. A walled town of 40 × 30 cells keeps its neighbours where the map puts them.

| Thing | Size in cells | Note |
|---|---|---|
| House, one household | 4 x 3 | A rural house of three bays; if the owner grows today's house (`docs/concept.md` §Open questions), the base pack's 3 x 3 house grows to this |
| Courtyard house | 10 x 10 | Rooms on three or four sides round a court, a well-off household's. #239 draws it 5 x 5 for now, an exception the art will follow |
| Village hall | 6 x 4 | Ancestral or meeting hall, twice a house's floor |
| Temple compound | 12 x 10 | Gate, court and main hall; #239's temple of 4 x 3 is the main hall standing inside it |
| Yamen or great hall | 16 x 12 | A town's seat of rule, with its court |
| Market square | 10 x 8 in a town, 16 x 12 in a city | Open paving with stalls on its edges |
| Wall, thickness | 2 for a town, 3 for a city | Owner: at least 2 |
| Wall, height | 2 levels for a town, 3 for a city | Lifted like a cliff level, with a walkway and crenellations. #239 draws a town's wall one level high for now, an exception the art will follow |
| Gate | 2 to 3 cells wide, under a 5 x 2 gate tower over a road of 3 cells, as #239 draws it | 2 to 4 gates a town, on its roads |
| Corner tower | 3 x 3 | On every corner of a town's wall |
| Village | 15 x 10 to 30 x 20, unwalled | 6 to 42 households, 20 to 149 residents at about 3.5 a household, below ADR 0062's town of 150 (ADR 0076, proposed) |
| Walled town | at least 40 x 30 inside the wall | Owner: streets, wards, market, hall, temples, courtyard houses |
| Major city | at least 80 x 60 inside the wall | Owner: with inner wards |
| Sect seat | 24 x 16 to 40 x 30 | A compound of gate, halls, training grounds and quarters, not a single hall |
| Field, one household's | 4 to 8 cells | Exaggerated small: a real household farmed several thousand cells' worth. The base pack's yield is about 1 person a cell, below |
| Track | 1 cell wide | |
| Road | 2 cells wide | |
| Highway | 3 to 4 cells wide | |
| Stream | 1 to 2 cells wide | |
| River | 4 to 8 cells wide | |
| Great river, the Yellow River or the Yangtze | 16 to 40 cells wide | Exaggerated narrow: at a person's scale a real width would be hundreds of cells |
| Bridge | the river's width plus 2, 3 cells wide | Arched over streams and rivers. Great rivers are crossed by ferries and fords (ADR 0047) |

**[proposed]** ADR 0076 gives a village its look by growth rather than by template:
- a paved square at its centre, with the hall, the well and the workshops on its edges;
- houses around it along lanes a cell wide, following the ground nearest the square;
- fields and pens beyond the houses, terraced up the slope where hills lie nearer than flat land.

A house's material by tier, such as thatch, tile and stone, and regional styles are drawn after #200's look is approved.

- **The field's yield.** Decided by the PM on #242, 2026-10-11: fields stay at 4 to 8 cells a household, and the base pack's yield is lowered from ADR 0022's 600 measures a cell, about 5 people, to about 1 person a cell: 120 measures a cell on plain and 60 on hills, as ADR 0063 sets them. So a household's fields feed about a household, and food, not a field's size, binds the population (#229).
  - It is a pack-data change. The pull request that applies it shows `make sweep` 48/50 or better, since births follow food.
  - ADR 0056's worked example assumed today's yield. Its implementation reads the yield from pack data, so the example's field counts scale with it.
- **#239's pieces.** The courtyard house and the town wall's height above are this table's sizes, by the owner's realistic-scale principle. #239's smaller drawings are the current exceptions, and the art follows this table in a later round.

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

- Every frame holds its footprint's bottom-left corner 8 pixels in from its left and bottom edges, as the house's does, the seat gate's and the training ground's included.
- Ground decorations are tiles laid over the terrain and under everything standing; the banner stands and is sorted with bodies and buildings.

A sect's seat is a village of its own (ADR 0026), laid out with the village's buildings and these of its own, so that a seat reads apart from a farming village at a glance:

| Prop | Kind | Footprint | Look |
|---|---|---|---|
| Training ground (연무장) | Structure | 6 x 4 | An open yard of raked sand in a stone curb, a ring trodden where pupils spar; two weapon racks of spears, glaives and staves and three wooden dummies along its back row, plum-blossom poles and stone weights on the sand; damaged scorches the sand, knocks a rack flat and snaps a dummy, destroyed burns the racks to their bare frames and the dummies and poles to stumps, rubble is grass and ash inside the curb with a heap of burnt wood at the back |
| Manual hall (장경각) | Structure | 4 x 3 | A two-storey library on a high stone platform with steps: latticed shutters over paper and a double door between lacquered pillars below, a gold-framed name plaque under the upper roof, two slate roofs with upturned eaves; damaged holes both roofs, scorches the wall and blows pages onto the steps, destroyed takes the upper storey and drops the lower roof in, rubble is a heap on the platform |
| Seat gate | Structure | 3 x 1 | A one-bay paifang: two lacquered pillars on stone bases, a painted bracket band, a gold-framed name board between the lintels, a slate roof with upturned eaves; damaged holes the roof and cracks the board, destroyed drops the roof, breaks one pillar and lays the board in the dust, rubble is the two bases and a burnt beam |
| Sect banner | Decoration | one cell | A tall pole on a stone foot with a crossbar, a long paper-white streamer edged in the owner's accent, two ink characters down it and a swallowtail with tassels; the faction's banner is a short pennant of the accent itself |

- The training ground is laid with the ground, over the terrain and under everything standing, so people stand and spar on it. What on it rises more than a few pixels, the racks and the dummies, stands along its back row, where anyone in front of it is drawn over it.
- The seat gate's pillars stand on its two end cells and the middle cell is the opening.
- The stone curb, the platform and the gate's bases stay in every stage, as the plinth does.
- The court before the halls is paved, and a road runs down from it to the gate's opening; both are ground decorations, laid as a village's paving and road are.

As a village grows, its people build workshops among its houses (`docs/concept.md` §Growing civilization). What each does is not yet designed; their art is drawn ahead of the rule:

| Prop | Kind | Footprint | Look |
|---|---|---|---|
| Smithy (대장간) | Structure | 3 x 2 | A forge shed on a stone plinth at the back of its apron, under a slate roof running back over its footprint, its left bay open on a stone hearth of white-hot coals whose glow is dithered over the dark inside and the plinth, bellows beside it, a stone chimney rising from the hearth through the roof; a plank wall hung with tongs and hammers in its right bay; an anvil on a stump, a quench tub of water and a heap of charcoal before it; damaged holes the roof, knocks the chimney's crown off, scorches the planks and dims the coals, destroyed drops the roof into the shed, breaks the chimney half down and burns the plank wall to stumps with a few embers left, rubble is a heap with the hearth and the chimney's stub standing out of it |
| Apothecary (약방) | Structure | 4 x 2 | A plastered shop on a stone plinth under a slate roof over its left three cells: a counter window on a cabinet of small drawers with gold pulls, jars and a mortar on the sill, herbs hung under the eave, a door under a split cloth curtain and a name board; in the yard at its right a herb rack of two posts and two bars hung with bundles, a drying tray of woven bamboo on trestles and a round tray on the ground, herbs spread on both; damaged holes the roof, scorches the wall, pulls drawers out and snaps the rack's lower bar onto the ground, destroyed drops the roof in and burns the walls to stumps, the rack to two posts and the tray where it fell, rubble is a heap on the plinth and ash in the yard |

- The anvil outlasts the smithy: it stands on its stump intact and damaged and lies on its side, destroyed and rubble.

A walled town (#200) encloses a real town, at least about 40 x 30 cells inside its wall, with streets, wards, a market square, a hall, temples and courtyard houses; a village stays unwalled. The game does not lay one yet: the town's template and its growth are #169's. Its pieces, each in the four stages:

| Piece | Footprint | Look |
|---|---|---|
| Wall, south and north | 1 x 2 | A face of stone courses a level high under a paved walkway two cells deep, merlons along its outer edge and a low parapet along its inner one; laid a piece a cell, its sides outlined only at a run's ends, so a run reads as one wall |
| Wall, west and east | 2 x 1 | The walkway two cells wide seen from above, merlons down its outer edge, over the face that shows where its run ends |
| Corner tower | 3 x 3 | Two levels of stone courses, crenellated all round, a small watch pavilion on its top |
| Gate tower | 5 x 2 | The wall raised over a road, the passage through its middle cell dark under its arch with its doors open, a gold name board over it, a gate house of plaster and lacquered pillars on its top under a broad slate roof |
| Steps | 1 x 2 | Eight treads, each three rows deep over a riser two rows high, between plain cheeks, up to the walkway behind them |
| Temple | 4 x 3 | Lacquered walls on a high plinth with steps, under a skirt roof and an upper roof, a gold plaque between them |
| Market stall | 2 x 1 | A striped awning of paper and crimson on four posts over a table of pots, greens and bolts of cloth |
| Courtyard house | 5 x 5 | A grey wall round a paved court with a tree, the main hall across its back, a wing down each side, a gate in its front wall under a small roof |

- What is built is drawn crisp: its edges run along the pixel grid, and where the ground round it is flat, the ground under its footprint is drawn without warp or dither up to the cells' sides, so the ground meets it in a straight seam; on a slope's edge the ground is drawn as any other, so no cliff stands where nothing blocks the way.

## Effects and HUD

- The screen is graded by the hour and the season (#209), over the world and under the HUD: night, morning, noon and dusk, blended at their edges, each a gain and lift per channel with its shadows tinted cool and its lights warm, then the season's own tint (`grade/grades.json`). Noon is the art as drawn, but for its season's tint. The darkest shadows lean further blue-violet and the lights warmer, and at standard and high a light vignette darkens the corners by the ordered dither, in whole art pixels. The settings' screen effects row sets low, standard or high, low dropping the costly effects.
- A hit on or from the player holds the struck and the striker still for 2 to 4 frames by the damage, a hit stop, and shakes the screen by 1 to 3 whole screen pixels for 150 ms, unless the settings turn both off; the world's time runs on.
- A hit draws blood, unless the settings turn it off, in the crimson ramp's second and fourth steps, `631648` and `a01b21`, dithered as it fades: its first is too near black to read on the ground, and `d6422e` stays for hostility.
  - A spray of eight pixels, in `a01b21`, flies from the struck body's side facing the blow along it, 12 to 22 pixels out, about a cell, in its first 120 ms, falling a little, and fades by 400 ms.
  - Five drops fall, three beyond its feet in the blow's direction and two pooled just below them, and fade over 3 s. A drop is a cluster of two or three pixels, a `631648` core with a `a01b21` edge.
  - Nothing else on the ground is red, so red there means blood: the grass's flowers are violet, yellow and white.
- Effects are brief and bright over calm ground: an area skill is a ring of force in its user's accent ramp with a white-hot core, cracks on the ground and sparks; sword qi is a crescent in the azure ramp; damage numbers rise in the colour of the technique that dealt them, with an ink shadow.
- Fire uses the fire ramp; smoke is grey puffs lit from the upper left that thin with height.
- HUD panels are ink with a gold trim and lacquer corner studs, so that they sit apart from the world's greens and earths. Bars are crimson for HP and azure for QI, each under its label and its value.
- The HUD holds the player (portrait, name, sect, age, HP, QI), the map and season, the technique bar with cooldowns, the player's silver and, on PC, a belt of the wares it holds, and the current target. Rumours are heard by talking to people, not shown on the HUD.
- The economy icons, at the technique icons' scale: food is a lacquered bowl of rice with chopsticks, stock a tied sack of grain, housing a house under its slate roof.
- The ware icons, at the same scale: herb is a sprig of three jade leaves on a pale root, elixir a round crimson pill lit at its top left, silver an ingot with its two ends raised.
- A technique slot shows its key, Q, W, E, R or T, in a column of its own left of an icon of the skill's shape: a blade with an arc of sword qi for a strike at one body, an open palm in a ring of force for a burst that reaches the bodies round its target.
- The people near the player are named over their heads in the HUD's font, outlined in ink: own and allied in jade `2b7287`, neutral in paper `bc9a7d`, and hostile or at war in crimson `d6422e`, the loudest. A few names show at once, the hostile first, never over one another or a panel.
- UI text is Korean first, with English supported (ADR 0012). Text is drawn in paper on ink. The game's text is Galmuri9 at 10 pixels, a pixel font that carries Hangul and Latin (`hud/fonts/`); the mockup's 5-pixel capital font draws English and numerals only. Every panel is laid out for the longer of the two strings.

## Do and don't

| Do | Don't |
|---|---|
| Draw every pixel from the master palette | Blend with transparency or add colours outside it |
| Stand bodies and facades upright at the angle, on cells 16 wide and 12 deep | Draw isometric diamonds or a perspective view |
| Draw a structure over its footprint and let it rise above | Draw a structure larger than its footprint to fit in more detail |
| Give each faction its accent, value and silhouette together | Rely on colour alone to tell factions apart |
| Outline bodies and structures sel-out, in the darkest step of what they border | Outline the world in black, the ground at all, or let it follow the cells' sides |
| Light from the upper left, shadow to the lower right | Bake shadows into sprites, or mix light directions in one scene |
| Keep the player its faction's body with markers around it | Give the player a larger or unique sprite |
| Scale by whole multiples | Scale by fractions, which smears pixels |
