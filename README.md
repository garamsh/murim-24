# Map mockup for #168

The mockup of the player's map, which ADR 0073 proposed, for the PM's approval before the view is built.

`map_mockup.gd` drew it over `play/play.tscn` at fa20c14: `main` 2cc1292 plus the chart unit's commit. It used Godot 4.7.2 on macOS arm64, with the HUD's own panel frame, font and palette.

The world is seed 2026's region, lived 10 years at 5 ticks a day. Then the player walks to the 3 villages nearest its home, and the world lives 10 more years out of sight.

## How the mockup decides what was seen

The mockup computes it in GDScript, as the ADR has the core do it:
- every tick, the cells within 15 cells of the body are seen;
- every village with a cell of its area within that disc is remembered as it stands then.

The run printed: 2,521 of 16,384 cells seen, 3 of 11 villages remembered. None of those 3 changed owner or emptied while out of sight. One village was founded out of sight, and the map does not show it, while `today_*` does.

## Images

Each in `ko` and `en`:

- `today_pc_*`: the HUD's map as `main` draws it, the live world, for comparison. PC only.
- `minimap_pc_*` and `minimap_touch_*`: the minimap around the player, drawn from what was seen.
  - Unseen cells are fog.
  - Seen cells show their terrain.
  - Borders are derived from the remembered villages.
  - A village whose residents were gone when seen is drawn in stone.
- `window_pc_*`: the map window N opens.
  - It draws the same image at the largest whole multiple that fits, 1x for the 128-cell region, in a frame fitted to the map and a legend.
  - The legend gives the season, each faction's colour, ruins, unseen land, and the player.
  - On a touch screen there is no window, since its button is deferred (scope, 2026-10-09).

Windows: 1920 x 1080 and 1280 x 720 on PC, and 1920 x 1080 with the touch controls.
