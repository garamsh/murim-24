# Evidence for #239 (PR C of #200's phase 2)

Captures at 1920 x 1080, ko, from the head against #235's head a64a7d7 (`before_*`):

- `after_player_3x_ko.png`, `before_player_3x_ko.png`: the player's village of seed 2026 at 3x under the HUD.
- `after_seat_2x_ko.png`, `before_seat_2x_ko.png`: a sect seat at 2x.
- `walled_town_2x.png`: the hand-laid walled town, 44 x 34 cells, 40 x 30 inside the wall, at 2x, laid by `town_layout.gd`. The game lays no town yet: #169's template.
- `walled_town_gate_3x.png`: the player at its south gate at 3x, for scale.
- `walled_town_steps_crop.png`: a 4x crop of the steps up to the north wall's walkway.

Scripts: `town_layout.gd` (the patch), `town_capture.gd <out dir>` (the two town captures), `town_bench.gd` (`frame_bench.gd` with the town laid round the M1 camera). Run them from the repository with `godot --path . --script <path>`.
