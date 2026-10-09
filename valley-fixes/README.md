# Valley fixes

The fixes the PM asked for on the revised look (PR #204 comment 6078753845). Made by `angle.gd` here, the same file as `art/mockup/angle.gd` at 739d086, with Godot 4.7.2 and Aseprite 1.3.18.6 on macOS arm64 (Apple M1), on AC power. Everything else is as `../revised/` describes it.

## Files

- `valley_<locale>.png`: the valley at 2x under the HUD, framed as `../revised/valley_<locale>.png`. `village_`, `town_`, `terraces_` and `riverside_<locale>.png`: the four places at 3x. From `stills valley`.
- `sequence.gif`, `sequence_2x.gif`, `strip.png`, `frames/`, `depth_behind.png`, `depth_front.png`: the village's people walking, from `frames valley`, assembled by `../sequence.lua`.
- `crops/before/` and `crops/after/`: 2x crops of `valley_ko.png` at 5ba6909 (`../revised/`) and at 739d086, at the comment's spots. Each is named for its spot: `seam_670`, `seam_1150`, `seam_1665`, `lime_top`, `cliff_left`, `cliff_mid`, `cliff_low` and `peak`; the number is the spot's x in the capture.

## What drew each defect, and the fix

1. **Vertical seams and 1 px lime lines.**
   - **What drew them:** a raised cell drew a 1 px lit line on its west side wherever its west neighbour was lower, and a dark one on its east. Along a long vertical change of level, these made a straight line.
   - **Why the boundaries were long and straight:** the heightfield had straight cuts. The range was lowered by a level between x 30 and 70 (the seams near x≈670 and x≈1150). The village's and the town's rectangles were forced flat, and the terraces were a rectangle (the field edge near x≈1665). Below those, the ground was chosen by level, so its shade changed along the same lines.
   - **Fix:**
     - No vertical lip or edge is drawn. A side wall is edge-on to this camera, so it shows nothing.
     - The cuts are replaced by a fall of the ground over several cells round an oval for the village and one for the town, so no level ends on a straight line.
     - The terraced hill's edges wander by noise.
     - The ground kind is chosen by noise, not by level (item 3).
     - Crops: `seam_670`, `seam_1150`, `seam_1665`, `lime_top`.
2. **Cliff faces as loose blocks.**
   - **What drew them:** a face was drawn under each cell whose south neighbour is lower, cut square at both ends, with nothing marking the top edge or the foot.
   - **Fix:**
     - Each face has a lit 1 px lip along its top edge (grass, or stone on rock).
     - Where a run of face ends against a cell that drops less, its last 2 px are shaded two steps darker and its top corner is cut, so the face turns away.
     - A 3 px band at its foot is in shadow, baked into the lower ground, on top of the cliff's swept shadow.
     - Crops: `cliff_left`, `cliff_mid`, `cliff_low`. `cliff_left` is mostly under the HUD's status panel in both.
3. **Straight biome edges.**
   - **What drew them:** each level had a fixed ground kind (grass, forest floor, rock), so kinds met along level boundaries and the cut lines of item 1.
   - **Fix:** the kind is chosen by a noise field mixed with the level (rock high up, forest floor and meadow lower), so kinds meet in blobs across levels. Where two kinds meet on one level, the existing ragged edge blends them. The river is one kind of water along its whole length; it changed kind at x 20.
4. **Peaks pasted on a flat base, and busy rock.**
   - **What drew it:** each peak sprite's foot was one straight row of pixels the full width of the mountain, standing on bare rock ground.
   - **Fix:**
     - The foot wanders by 6 px.
     - Scree spills a few pixels from it, thinning by dithering.
     - 16 pines straddle it.
     - Pines are planted round each peak's footing on the ground.
   - **Rock ground:** the tile is now broad patches of two greys with a rare darker crack, in place of three greys and light speckles.
   - Crop: `peak`.

## Frame cost

To follow. The bench is rerun on a quiet machine. A series run at 739d086 while two core test suites were each holding a CPU at 100% (load average 17) read about 20 to 25% slower than the quiet series in `../revised/`, and is not counted.
