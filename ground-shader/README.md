# Ground off the cell grid

Round 3 of the revised look: #212's natural terrain, approved by the owner step by step, prototyped in the valley. Made by `angle.gd` and `ground.gdshader` here, the same files as `art/mockup/angle.gd` and `art/mockup/angle/ground.gdshader` at 53d76c1. Run with Godot 4.7.2 on macOS arm64 (Apple M1), on AC power.

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

To follow. The rerun waits for a quiet machine. Each bench now brings its window to the front on its first frame. It also prints `PACED` when its p50 is a 144 Hz display's period, so a paced run says so itself.

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
- Frame cost to follow, all 9 runs counted, on a quiet machine.

## For the owner and the PM

- The ground's colours come from #212's ramps, not from the master palette in `docs/art-direction.md`, whose rule is that every pixel is from it. #212 asks the art direction to be rewritten for materials and ramps in a later phase.
- Not done here, as #212 places them later:
  - the art direction's materials per region;
  - quality profiles;
  - the tests of identical pixels and placement;
  - the port into the game's view.
