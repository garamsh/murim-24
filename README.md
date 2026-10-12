# Map captures for #168

The player's map as built from ADR 0073: the real `play/play.tscn` and HUD, with nothing drawn by the script.

`map_capture.gd` ran at 22166a8 on `feat/chart`; the commits since change no drawing, with Godot 4.7.2 on macOS arm64.

The world is seed 2026's region at 5 ticks a day:
- the player, steered as a new game steers it, walks to the 3 villages nearest its home;
- it then stands while the world lives 40 more years.

The run printed: 2,928 of 16,384 cells seen, and 5 of 19 villages remembered. 9 villages were founded out of sight while it stood. They are not on the map, because the player has not seen them. The 2 founded within its sight are on it.

## Images

Each in `ko` and `en`:

- `minimap_pc_*` and `minimap_touch_*`: the minimap around the player, from the core's chart.
  - Unseen cells are fog.
  - Borders are drawn only against seen cells.
  - Each village is drawn as last seen.
- `window_pc_*`: the map window N opens, which the rail's lit "지도 / Map" row also shows.
  - At 1920 x 1080 it draws the 128-cell map at 3x in a HUD at 2x, 6 screen pixels a cell.
  - At 1280 x 720 it draws it at 4x in a HUD at 1x.
  - The legend names the factions as the HUD does.

On a touch screen no button opens the window, since touch is deferred (scope, 2026-10-09).

The mockup these follow, approved with changes, is on `evidence/map-mockup`.
