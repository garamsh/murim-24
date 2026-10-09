# Evidence for the 2.5D angle mockup

Phase 1 of #200: the world drawn at Albion's 3/4 angle in 2D pixel art, shown to the owner before anything is rebuilt (PR #204). This branch is never merged.

## How it was made

On macOS arm64 (Apple M1), Godot 4.7.2 and Aseprite 1.3.18.6, at ba32a9f, the head of `mockup/two-point-five-d`, after `make build`:

- Art: `art/mockup/build.sh` draws the sheets under `art/mockup/angle/` from `terrain.lua`, `structures.lua` and `bodies.lua`, and exports each as a `.png` sheet with a `.json` of its frames and tags.
- Scene: `angle.gd` here is the same file as `art/mockup/angle.gd` at ba32a9f. Copied to the project root, it ran as `godot --path . --script res://angle.gd -- <dir> frames`, then `stills`, then `bench` (see Frame cost).
- Animation: `sequence.lua` here assembled `frames/` into the GIFs and the strip: `aseprite --batch --script-param dir=<dir> --script sequence.lua`.

## What the images show

- The world is seed 2026's region from `WorldAdapter.create_region`, lived 10 years with days of 5 ticks, as evidence/pr-176 and the faction window mockup do. The camera centres on cell (26, 46): village 5 at the forest's edge, with its houses, hall, well and paved square, granary, storehouse, fence, fields and road; the seat of the Heavenly Demon with its gate and training ground above it; the southern houses of village 2; hills to the north-east and the forest to the east.
- Every position is the core's. The mockup only gives orders through `WorldAdapter.order`, after `steer` marks the armies it drives:
  - Demonic people walk set routes: one army along the row behind village 5's southern houses and back, one out of the seat's gate to village 5 and back, one down from village 2.
  - Five Orthodox armies (the player's among them, 20 people) gather south of the view, then march on village 5. When one comes within 12 cells of the village's road, the Demonic people go to meet the nearest of them. Both sides strike by the core's automatic strike, and people die and vanish as the core says.
- `sequence.gif` (640x360, the native size) and `sequence_2x.gif` (1280x720): 180 frames, one each 3 ticks (100 ms), 18 s, without the HUD. `strip.png`: every 15th frame, 1.5 s apart, left to right and top to bottom. `frames/` holds all 180 at 640x360.
- `village_<locale>.png` (frame 40) and `skirmish_<locale>.png` (frame 130), 1920x1080: the same moments under `play.tscn`'s HUD as it is on `main` (#176, #197), ko and en. The world draws at 3x and the HUD at 2x, as `view_scale.gd` gives a PC window of that size. The HUD's map and minimap are today's, top-down.
- `depth_behind.png` and `depth_front.png`: 4x crops of the sequence, the first frame where a house covers at least 12 rows of a person, and the first where a person stands over a house's front.

## Projection

- A cell is 16 pixels wide and 12 deep: the ground is foreshortened to three quarters, as from a camera pitched about 49 degrees down and facing north. There is no yaw, so cell rows stay horizontal and a building shows its south front and its roof, never its sides.
- A point in the core's sub-cell units (256 a cell) is drawn at x * 16 / 256, y * 12 / 256. Height is drawn straight up: a house's wall is 14 pixels and its ridge 12 above that; a person is the production 16x24 frame, feet on row 22.
- Hills are raised 8 pixels, with a step face along their front edge and a lit lip on their back and west edges, and the people on them are drawn raised too.
- Everything standing (structures, trees, people) is one y-sorted node, sorted by the bottom edge of its footprint: a structure by its front edge, a tree by its trunk's foot, a person by its feet. A frame holds its footprint's bottom-left at (8, height - 8).
- At 1920x1080 the world draws at 3x, 640x360, so 40 cells across and 30 down, against 22.5 down today.

## Depth

- A person sorted before a structure or a tree whose opaque pixels overlap its frame is drawn behind it, and its outline is drawn again over the occluder in the faction's light accent (azure `5b9ae0`, crimson `d63a2f`). The outline is a child of the occluder with `clip_children`, so it shows only on the occluder's pixels: the visible part of the person is drawn once, the hidden part as an outline.
- A person sorted after a structure is drawn over it, whole.

## Frame cost

`angle.gd`'s `bench` mode, in a 1920x1080 window with vsync off on an Apple M1 (macOS, on AC power): 1200 frames timed after 120 of warm-up, ticking at the core's 30 a second and drawing bodies between ticks on every frame.

- `village`: the sequence above in real time, from the raiders' march, under the HUD (about 50 people in the world).
- `m1`: `WorldAdapter.create_m1()`, 500 people round the M1 battle, without the HUD.
- `flat` turns the y-sort off (`y_sort_enabled` false on the standing node); `solid` turns the outlines off.

`bench.log` holds the printed lines of every run. Runs 1 to 3 were at b8532e5 and run 4 at ba32a9f, which adds only the depth crops' row count. Today's frame bench is `godot --path . --script res://world_view/frame_bench.gd` on the same build, the frame half of `make bench`, run between them.

| Scene | p50, runs 1 to 4 (ms) | p99, runs 1 to 4 (ms) |
|---|---|---|
| village | 3.544, 3.623, 3.634, 3.439 | 10.499, 13.186, 13.059, 10.514 |
| village flat | 3.378, 3.401, 3.479, 3.374 | 10.344, 10.919, 10.663, 11.013 |
| village solid | 3.206, 3.265, 3.232, 3.154 | 11.025, 10.747, 11.111, 10.869 |
| m1 | 6.982, 7.282, 8.154, 7.593 | 10.838, 10.844, 10.963, 10.960 |
| m1 flat | 6.752, 7.075, 7.083, 6.826 | 10.597, 12.564, 10.784, 10.296 |
| m1 solid | 5.230, 5.336, 5.367, 5.301 | 8.104, 8.532, 7.730, 7.802 |
| today's frame bench, top-down, MultiMesh bands, about 304 of 500 on screen | 2.471, 2.528, 2.475 | 5.910, 7.290, 5.872 |

What they say:

- **The y-sort**: sorted against `flat`, the p50 is 0.07 to 0.23 ms higher in the village and 0.23 to 1.07 ms with 500 people; the p99s overlap within their run-to-run spread. The game already sorts everything standing by y (`world_art.tscn`'s `Standing` has `y_sort_enabled`, and bodies are batched in one band per row), so the angle adds no sort; its rows are 12 pixels apart instead of 16.
- **The outlines**: against `solid`, the p50 is 0.29 to 0.40 ms higher in the village and 1.75 to 2.82 ms with 500 people (about 30 of them hidden), and the M1 p99 2.7 to 3.2 ms higher. In the mockup that cost is GDScript finding each body's occluders every frame, and a clipped pass per occluder that hides someone.
- **Against today**: the mockup draws each person as its own `Sprite2D` set from GDScript on every frame and culls none, where the game batches bodies in MultiMesh bands and culls to the screen. `m1 solid` at p50 5.3 ms against today's 2.5 ms is that difference, not the angle. Every run is under the 16.7 ms budget at p99.
- The village's p99 (10.3 to 13.2 ms) does not move with either switch; it is set by frames outside the drawing.

## If the angle is adopted

What the mockup shows would change in the game (later phases of #200), with where each lives now:

- `world_art/world_art.gd`: `CELL` (16) splits into 16 wide and 12 deep wherever a position becomes pixels (`body_at`, the bands' rows, structures, landforms, decorations, fields, markers). The bands stay one MultiMesh per row of cells under the y-sorted `Standing` node, as today: rows are 12 pixels apart instead of 16, and sorting does not change. The see-through outline is new: per occluder, an outline band or a clipped child as here.
- Terrain (`docs/convention/stack-godot.md` §Terrain layers): `TileMapLayer`s with 16x12 tiles, the dual-grid transitions redrawn at that size, and the hills as a layer raised 8 pixels with its face tiles. Landforms become standing sprites at the angle.
- Art (`art/`): every sheet redrawn at the angle; the bodies keep their template, with a back view added.
- `play/play.gd` (picking and camera): `PIXELS_PER_CELL` is used for the camera's limits, a click's cell (`follow`, the touch tap at `_view_origin`), a cast's point, the aim's range and radius, and the talk prompt's head. Each becomes 16 by 12, and picking on hills checks the raised ground first.
- `play/aim.gd`: the range ring and the area become ellipses, 12/16 as deep as wide.
- `sound/sound.gd`: its `PIXELS_PER_CELL` places sounds; the y axis becomes 12.
- `world_view/world_view.gd`: its `PIXELS_PER_CELL` places the camera on the M1 centre; `frame_bench.gd` then measures the angle.
- The core does not change: positions, footprints and cells are the same.

## Left out of the mockup

- Only the intact stage of each structure is drawn; damage stages, fire and smoke are not.
- Water and mountain are not drawn at the angle (none is in view); they fall back to plain grass. Banners and territory borders are not drawn.
- Fields are laid in the state they hold when the view is built; with 5-tick days they would change several times in the sequence.
- No strike effects: a person who strikes shows its strike frame for 200 ms.
