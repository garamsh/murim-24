# The revised look

The look PR #204's review asked for (PM comments 6076845914 and 6076862823): elevation levels with cliff faces and tiered mountains, shadows baked into the art, taller buildings, and a much larger village beside villages of other characters. Made by `angle.gd` here, the same file as `art/mockup/angle.gd` at 5ba6909, copied to the project root, with Godot 4.7.2 and Aseprite 1.3.18.6 on macOS arm64 (Apple M1), on AC power.

## Files

- `village_<locale>.png`, `town_<locale>.png`, `terraces_<locale>.png`, `riverside_<locale>.png`: 1920x1080 at the PC's 3x, under the HUD, ko and en. `valley_<locale>.png`: the range and the village pulled back to 2x. All from `godot --path . --script res://angle.gd -- <dir> stills valley`, after 90 ticks of the valley's people walking.
- `sequence.gif` (640x360) and `sequence_2x.gif` (1280x720): 180 frames, 100 ms apart, 18 s of the village's people walking its lanes, without the HUD; `strip.png`: every 15th frame; `frames/`: all of them. From `frames valley`, assembled by `../sequence.lua`.
- `depth_behind.png`: 4x, a villager behind a house's roof, drawn as an outline over it. `depth_front.png`: a person in front of a house, drawn over it.
- `bench.log`: every line the nine bench runs printed.

## What changed

- **Levels.** Ground stands on levels 16 pixels apart, from -1 (the river) to 5. Where a cell drops to its south, the drop is a face: rock on the range, stone under the terraces and the shrine's terrace, earth along the river's bank, stone steps where a stair climbs. Each raised row is a strip sorted with everything standing by its row's north edge, so people and buildings on a level are drawn raised and in front of the level behind, and anyone behind a terrace is hidden by it and outlined. The range rises in tiers, smoothed so no tier is a lone cell, grass and forest floor on the lower tops and rock above, pines on its slopes and three peak sprites capping the top tier.
- **Shadows.** Every building, tree and cliff casts a shadow to the lower right, baked into the ground two steps darker along each colour's palette ramp (`art/mockup/angle/shade.json`); nothing is lit at run time. Where two kinds of ground meet on a level, each takes a ragged few pixels of the other, so lanes, meadows and gardens have organic edges.
- **Buildings** (`art/mockup/angle/buildings.lua`): fronts 20 to 32 pixels high, hip roofs graded from ridge to eave with upturned corners, or rounded thatch. Tiled houses in two sizes, thatched houses in two, an ancestral hall, a two-storey inn, a shrine, a guardian pavilion and guardian posts, a village gate, market stalls in three colours, a watchtower, a stilt house, a pier and a boat, town walls and the town's gate tower, field-stone walls and brushwood fences, a well, and crocks, laundry, a cart, haystacks, firewood, lanterns, bushes and boulders.
- **The valley** (128 x 96 cells, 328 structures), composed by `Valley` with a seeded draw so it is the same each run:
  - the main village, entered from the south through its gate between guardian posts: winding lanes to a paved square with a well and stalls, the ancestral hall and the inn beside it, a shrine up stairs on a terrace at the foot of the range, tiled houses in the core and thatched ones toward the edges, a cell apart, with kitchen gardens, crocks, firewood, laundry and trees among them;
  - a walled market town to the west: its gate tower in the south wall, a street up to a market street lined with stalls, shops, an inn, watchtowers and lanterns;
  - a terraced village on the eastern hillside: four levels of paddies and gardens behind stone walls, a stair winding up through them, houses on dry yards along each terrace, bamboo;
  - a riverside village: stilt houses standing in the river off its bank, piers and boats between them, willows along the bank.
- **People.** 80, or 500 with `crowd`, walk the valley, every position the core's: `WorldAdapter.create` makes a world on the valley's open cells (cliff tops are closed except at stairs), and each army walks between lane cells of its place through the order port. Villagers wear the cloth body; a few armies are Orthodox and Demonic, at peace.
- **The HUD** is the real one over the valley, but it is the generated region's player's, so its status and minimap are not of this valley.

## Map build and streaming

- The valley's 12 chunks are baked once when the view is made, as a map build would bake them: the ground, the strips, the rough edges and the shadows. That took 1.39 to 1.52 s for all 12, 107 to 124 ms a chunk at p50 and up to 190 ms, in GDScript.
- Streaming a chunk in then only uploads its images and makes its sprites: 3.9 to 4.4 ms at p50 and 8.1 ms at most. Before the bake moved out of streaming, building a chunk with its shadows took 110 to 160 ms (one run at c83cd06).
- A chunk's images hold 1.8 to 1.9 MB with its strips; 9 are resident at 3x and at most 12 while panning, 17 MB.

## Frame cost

`bench valley [crowd|pan]` at 5ba6909: 1920x1080, vsync off, 1200 frames timed after 120 of warm-up, under the HUD, each run quitting on its own after at most 40 s of timing (and killed after 120 s). Three runs each, one after another.

| Scene | p50 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|
| the village, 80 people, 38 to 39 on screen | 4.814, 4.922, 4.901 | 7.112, 6.879, 7.218 | 9.530, 7.908, 8.494 |
| `crowd`: 500 people, 222 to 223 on screen | 11.190, 11.390, 11.362 | 13.340, 13.691, 13.648 | 14.365, 14.806, 15.079 |
| `pan`: the camera round the four places at 20 cells a second, 135 to 139 cells | 4.861, 4.836, 4.763 | 7.318, 7.312, 7.379 | 13.464, 12.077, 11.155 |

Every run is under the 16.7 ms budget. The mockup draws each person as its own sprite set from GDScript, where the game batches bodies in bands; with 500 people the p50 of 11 ms is mostly that.

## Not done, or for the owner to judge

- Buildings show their fronts and roofs, not their sides: the camera faces north without turning, so a side wall is edge-on. Gable and hip ends show as the roof's ends.
- The terraces run in near-straight bands, and the range's rock tops are busy at 3x.
- Damage stages are drawn only for the first round's structures, not this set.
- The region screens wait for the owner's view of this look.
