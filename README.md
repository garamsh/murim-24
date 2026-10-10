# Evidence for the 2.5D port's ground (#200, PR A)

Captures of the game at 1920 x 1080 in Korean, through `play/play.tscn` and `world_view/world_view.tscn`, before (main d89a52f) and after (branch `feat/ground-25d` at b49a1d9; a51f7a1 changes no drawing). Taken on the development Mac (Apple M1, Godot 4.7.2, Compatibility renderer).

- `before_player_3x_ko.png`, `after_player_3x_ko.png`: seed 2026's region round the player's birth village, at the PC's 3x, the play scene's own camera.
- `before_forest_lake_2x_ko.png`, `after_forest_lake_2x_ko.png`: the same region's north-west, its lakes and forests, at 2x.
- `before_mountain_2x_ko.png`, `after_mountain_2x_ko.png`: the region's east, its hills and the mountains along its edge, at 2x.
- `before_m1_3x.png`, `after_m1_3x.png`: the M1 battle `make bench` renders, at 3x.
- `probe.log`: `world_art/ground_probe.gd` at b49a1d9, the pixel checks: the viewport freed, the same pixels twice, the forest's edge off the cells' side against the blocky control, and streaming.

The 2x views are centred on the same cells before and after: the angle draws a cell 12 pixels deep where main drew it 16, so their pixel centres differ by the ratio. Bodies and structures are still the 16-pixel grid's art, drawn at the angle's positions; PRs B and C redraw them.

## Round 2 (e0cf847)

After the review: the region code removed, the warp at 32 px over 48 px noise, the ground's ramps and detail as sheets. `round2/` holds the same views retaken at e0cf847 (`after_*`, with main's `before_*` copied beside them), `ground_check.log`, and a 3x crop of the rock edge at (1600, 700) before (`before_fix_mountain_rock_edge_crop.png`, from b49a1d9, with the island of hills inside the rock) and after (`after_mountain_rock_edge_crop.png`).
