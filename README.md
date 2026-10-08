# Evidence for PR #174

Diagonal walks in `play.tscn`, in a window, recorded by `motion_probe.gd` over the region of seed 2026 with the player born into village 7. Each run measures 4 s at 60 frames a second, after 15 ticks of walking. The probe holds the camera where the walk began, so the walk crosses the screen. It saves every 30th frame, and the last frame at twice the size with every frame's drawn body centre in red. This branch is never merged.

- `joystick_main_*.png`: the touch joystick held down and to the right, with main's `core/flow_field.*`, `core/world.*`, `world_art/world_art.gd`, `play/play.gd` and `world_view/world_view.gd` at 0962bfe. The person walks straight down: logged `walked (0.0, 120.0) px`.
- `joystick_fixed_*.png`: the same with fix/smooth-movement at dd1f46b. The person walks the diagonal: logged `walked (82.0, 82.0) px`, `furthest off the straight line: 0.00 px`.
- `click_main_*.png`: the right button pressed and let go on the cell 14 cells down the same diagonal, at 0962bfe. The person walks straight down, the first leg of an L: `walked (0.0, 120.0) px`.
- `click_fixed_*.png`: the same at dd1f46b: `walked (82.0, 82.0) px`, `furthest off the straight line: 0.00 px`.
- `*_sheet.png`: frames 0, 30, ..., 210, four to a row. `*_trail.png`: frame 238 with the drawn path.

Commands, from the project root after `make build` with the code under test in place, the probe copied into `play/`:

    godot --path . --script res://play/motion_probe.gd -- diagonal 60 7 <dir>
    godot --path . --script res://play/motion_probe.gd -- click 60 7 <dir>

The numbers in the PR's table come from the same probe run headless without a capture directory: `godot --headless --path . --script res://play/motion_probe.gd -- <straight|diagonal|click> <60|144> 7`.
