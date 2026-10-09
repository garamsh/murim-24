# Phase 2 captures of the shortcut rail

The shortcut rail as built in `feat/pc-button-bar`, captured in the real `play.tscn`. This branch is never merged.

`rail_capture.gd` made the images. It lives on this branch only. I ran it with Godot 4.7.2 on macOS arm64 after `make build`, at the head of `feat/pc-button-bar` named in the commit that added these images: copied to the project root, then `godot --path . --script res://rail_capture.gd -- <dir>`.

What the images show:
- Seed 2026's region, just started from the title's own `start`, with the touch controls off.
- Input reaches the scene as a player's does. The script pushes the cursor's motion and a left click into the window with `push_input`, at the centre of the row in window pixels. It does not call the scene's functions.

Names are `<view>_<window>_<locale>.png`. The windows:
- `1280x720`: the HUD at 1x in 1280x720.
- `1920x1080`: the HUD at 2x in 960x540.
- `2560x1440`: the HUD at 2x in 1280x720.

The views:
- `idle`: the rail under the map, its right edge on the map's, with nothing open.
- `hover`: the cursor on the factions row, marked in gold.
- `open`: a click on the genealogy row. The panel opens, and its row is lit in jade over the panel's dimmed room.
- `settings`: a click on the settings row. The settings open, and the settings row is lit under their checker.

What the script printed, for every image:

| Locale | Window | Rail (HUD px) |
|---|---|---|
| ko | 1280x720, 2560x1440 | (1115, 70) 161x102 |
| ko | 1920x1080 | (795, 70) 161x102 |
| en | 1280x720, 2560x1440 | (1117, 70) 159x102 |
| en | 1920x1080 | (797, 70) 159x102 |

In every `open` image it printed `lit "genealogy"` and `open genealogy.gd`. In every `settings` image it printed `lit "settings"` and `open settings.gd`. In every `hover` image it printed `hovered 2`, the factions row, and `open none`.
