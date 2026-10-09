# Evidence for PR #208 (#184 phase A: combat feedback)

Captured on the development Mac (Apple M1, macOS, Godot 4.7.2, Compatibility renderer), on branch `feat/combat-feedback`. Every Godot run exited on its own within a frame cap.

- `fight_ko_strip.png`: 8 frames, 640x360 each, cropped round the screen's middle from the play scene at 1920x1080 in Korean. The player was born in the Cult village of seed 2026 and ordered on the largest hostile army. Read left to right, top to bottom, the frames show the player's sweep in jade, the struck drawn in the impact colour, the damage numbers rising with an ink shadow, and crimson brackets round the target, under the HUD's nameplate. Script: `zz_capture.gd`. Its log of what `combat.gd` showed each frame is `fight_log.txt`; the strip is frames 41 to 48.
- `m1_strip.png`: 8 frames of the M1 battle through `world_view.tscn`, with damage numbers hidden by the settings' flag, from tick 863, while people die. It shows the arcs and the casters' discs in each faction's accent and the struck in the impact colour. The dying fall and thin out under the dither; one to three were dying in every frame captured. Script: `zz_capture_m1.gd` with a second argument. Its log is `m1_log.txt`.
- `zz_strip.gd` lays the frames four to a row.

Run each from the repository root as `godot --path . --script res://zz_capture.gd -- <out dir>`, after copying it there.
