# Ground off the cell grid

Round 3 of the revised look: #212's natural terrain, approved by the owner step by step, prototyped in the valley. Made by `angle.gd` and `ground.gdshader` here, the same files as `art/mockup/angle.gd` and `art/mockup/angle/ground.gdshader` at 8440a71. The captures were made at 53d76c1; 8440a71 changes only when the baker is freed, and its stills are byte-identical to them. Run with Godot 4.7.2 on macOS arm64 (Apple M1), on AC power.

## Files

- `valley_<locale>.png`: the valley at 2x under the HUD. `village_`, `town_`, `terraces_` and `riverside_<locale>.png`: the four places at 3x. From `stills valley`.
- `sequence.gif`, `sequence_2x.gif`, `strip.png`, `frames/`, `depth_behind.png`, `depth_front.png`: the village's people walking, from `frames valley`.
- 2x crops at the same spots, in three folders:
  - `crops/before/`: `../valley-fixes/` (739d086);
  - `crops/round3/`: this round as first pushed (25e4fe2);
  - `crops/after/`: this round after the PM's review (53d76c1).
- The crops:
  - `cliffs_left` and `cliffs_mid`: the range's tiers;
  - `forest_edge`: the forest below the hill, from the valley still;
  - `forest_edge_river`: the forest by the river, from the riverside still;
  - `rock_ground` and `slope`, in `round3/` and `after/` only: the grey highland, and the mountainside where it rises without a cliff.

## Step 1: the four uncounted runs

The four runs `../valley-fixes/` did not count, rerun on 739d086's `angle.gd`, which now brings its window to the front on its first frame (`bench_739d086_rerun.log`). Conditions: on AC power, load average 1.7 to 2.1, no other test, lint or Godot process; the other workers held their runs for the window. None was paced: no p50 is near 6.94 ms.

All 9 runs of 739d086, the 5 counted in `../valley-fixes/` and the 4 reruns:

| Scene | p50 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|
| the village, 80 people (runs 1, 2; rerun) | 4.950, 4.934; 5.054 | 7.214, 7.101; 10.195 | 9.519, 9.454; 10.737 |
| `crowd`: 500 people (runs 1, 2; rerun) | 12.073, 12.248; 12.003 | 14.101, 15.407; 17.347 | 15.332, 40.942; 18.393 |
| `pan` (run 1; reruns 1, 2) | 5.349; 5.351, 5.361 | 8.020; 10.315, 10.525 | 11.921; 12.185, 12.054 |

- p50 agrees across the runs, to within 0.25 ms.
- p99 is about 2 to 3 ms higher in the reruns than in the counted runs: 10.2 to 10.5 against 7.1 to 8.0 ms, and 17.3 against 14.1 and 15.4 ms with the crowd.
- The window was in front for the reruns, and its place is not recorded for the counted runs. I have not established the cause.
- The crowd's rerun p99, 17.3 ms, is over the 16.7 ms budget.

## Step 2: the ground off the grid

What the shader does, per pixel, from the valley's cells (`ground.gdshader`):

- **Cell data**: a texture of one texel a cell, holding the ground kind, the level, and the material of the face the cell drops by: rock, earth bank, masonry or stair.
- **Sub-cell edges**: the point is warped by noise, up to 7 px. Its four cells' levels are interpolated, and the result is taken to the nearest level one of those cells has, so contours curve without a false ledge between levels. The kind is the one whose cells weigh most at the point.
- **Elevation**: the shader looks down the pixel's column from the highest ground that could show there. The first point that shows at or above the pixel is what is seen: that point's ground, or the face dropping from its edge. Faces follow the contour wherever it runs, ending where it turns.
- **Dithered transitions**: a little per-pixel noise in the kinds' weights dithers the narrow band where two kinds are nearly even.
- **Rims**:
  - foam and shallows within 1 and 3 px of the shore;
  - a trodden edge where grass meets a lane, and a darker edge on the lane;
  - shade within 2 px of a forest's edge;
  - a lit lip where ground turns down into a face;
  - a dithered shadow over the 4 px at a face's foot.
- **Faces**: on rock, strata 2.5 to 5.5 px thick that tilt and wander along the face, each its own tone with a lit top and a dark underside, split by fissures. Plants hang over the lip in tufts, and scree piles at the foot to a height that varies along the run. Masonry and stairs, which are built, keep courses and treads. A face above the first level is rock along its whole run.
- **Slopes**: a cell can be flagged as a slope. Toward such cells the height is interpolated rather than snapped to a level, so the ground climbs without a cliff, and a cliff fades out where it meets a slope.
- **Texture**: varied per pixel of the image in clusters, with patches of colour 48 px across, and moss on rock in clusters.
- **Ramps**: four steps a kind, shifted toward blue in the dark and yellow in the light, built by #212's rule from a hue, saturation and lightness per kind. Paddy and garden take four colours of the master palette.

What the map build adds to each 32-cell chunk (ADR 0042's area):

- The shader draws each chunk twice into an offscreen viewport, once in colour and once in what each pixel is: its level, kind and the row it sorts with.
- Detail is scattered by Poisson-disc spacing, 4.2 px apart, from a seed the chunk's index gives, so it is the same on every build. Grass gets tufts and flowers, the forest floor litter, lanes and paving pebbles, and the water's edge reeds. Each is drawn only on its own ground, and more of them near an edge between kinds.
- Decorative trees are scattered the same way, 8 px apart. They are dense inside forests and thin toward their edges by noise; now and then one stands alone in the open, on a cell open to it. Each has a contact shadow. The valley's own trees (willows, bamboo, those by houses and round the peaks) are kept as placed.
- Structures cast shadows on their own level, their edges wandering a couple of pixels.
- The image is cut into the ground (level 0 and below) and a strip per raised row and level, as before, so anyone behind a terrace is still hidden and outlined.

## The PM's review of 25e4fe2

The review on #204 asked for three fixes before the benches count:

1. **Cliffs read as masonry.** Rock faces now have the uneven strata, overhanging plants and scree described under Faces. They end where the contour turns or where a slope begins. Built walls keep their courses.
2. **Too many parallel ridges.** The range's height is now taken to levels 0, 2, 4 and 5, so the mountainside climbs in three tall steps where it had five or six. About a sixth of its raised cells, in patches, rise as slopes with no cliff.
3. **Rock ground dithering.** Moss on rock is drawn in clusters where it was a Bayer checker. The per-pixel jitter between kinds follows the image, so a steep slope no longer stretches it into streaks.

`crops/round3/` and `crops/after/` show each fix at the same spots.

## Step 3: captures and frame cost

- The captures above are at 53d76c1.
- The valley's forest floor has no rectangular patches: the woods and slopes the valley used to set tree by tree are forest ground now, and their trees are scattered.
- Straight cell-edged runs left on screen:
  - the shrine's terrace and the town's walls, which are built straight;
  - short vertical joins where a face's material changes between tiers, earth to rock;
  - the HUD.
- Frame cost: `bench valley [crowd|pan]` at 8440a71, three runs each, 1920x1080, vsync off, under the HUD, window in front, 1200 timed frames, each run quitting on its own. On AC power, load average 2.2 to 2.7, no other test, lint or Godot process, none paced. `bench.log` holds every printed line.

| Scene | p50 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|
| the village, 80 people | 5.072, 5.052, 5.061 | 9.953, 10.044, 9.915 | 10.253, 10.451, 10.635 |
| `crowd`: 500 people | 12.048, 12.030, 12.015 | 16.933, 17.279, 17.113 | 17.862, 18.441, 18.479 |
| `pan` | 5.559, 5.530, 5.505 | 10.271, 10.853, 10.370 | 11.742, 15.290, 11.186 |

- Against 739d086's reruns under the same conditions, the shaded ground costs nothing measurable per frame: the village 5.05 to 5.07 against 5.05 ms p50, the crowd 12.02 to 12.05 against 12.00 ms. A frame draws the baked images, as before.
- `pan` is 0.15 to 0.2 ms higher at p50 (5.51 to 5.56 against 5.35 to 5.36 ms).
- The crowd's p99, 16.9 to 17.3 ms, is over the 16.7 ms budget in all three runs, as in 739d086's rerun. The village and `pan` are under it.
- The map build bakes the 12 chunks in 3.31 to 3.34 s, 267 to 274 ms a chunk at p50 and 485 ms at most, against 1.34 s at 739d086. I have not timed how this splits between the shader's draw, read back to the CPU, and the CPU's work: the detail, the trees, the shadows and cutting the image into strips. None of it is paid while the game runs.
- Less ground image is resident: 1.16 MB a chunk against 1.90 MB at 739d086, because the strips are cut to their content.
- A first measurement, at 53d76c1, gave a p50 of about 22.6 ms in every scene (`bench_53d76c1.log`, not counted). The viewport that bakes the ground drew its shader again every frame after the bake. 8440a71 frees it once every chunk is baked.

## For the owner and the PM

- The ground's colours come from #212's ramps, not from the master palette in `docs/art-direction.md`, whose rule is that every pixel is from it. #212 asks the art direction to be rewritten for materials and ramps in a later phase.
- Not done here, as #212 places them later:
  - the art direction's materials per region;
  - quality profiles;
  - the tests of identical pixels and placement;
  - the port into the game's view.
