# Evidence: built things crisp (#200)

Head `10966d19` on `fix/built-crisp-on-slopes`; main `af3dbfc6`.

## Tool

`world_art/ground_check_tool.gd` on a display:
- `logs/tool-head.log`: 12 of 12 pass at head;
- `logs/tool-main.log`: the same tool on main's `ground.gdshader` and `relief.gd`, 9 of 12. The slope, face and trodden checks fail.

## The tool's map, before and after

`harness/probe.gd` bakes three blocks of built cells, warped (`*_warped`) and with no warp (`*_unwarped`), and crops them at 4x. Each crop is 24 px left and 64 px above the block's cells:
- `flat`: across the forest's edge on the plain;
- `slope`: across the hills' west edge;
- `drop`: below a mountain's south face.

`captures/main_<block>_warped.png` is main and `captures/head_<block>_warped.png` is head.

## The game, before and after

`harness/cellshot.gd` draws seed 2026's region at tick 900 in `world_art.tscn` alone, the camera at 3x on a cell. `captures/{main,head}_<x>_<y>[_crop4x].png`:
- `73_104` and `19_8`: road cells on a slope;
- `124_73`: village 2's hamlet. Here the frames barely differ: the audit's "face fraying" was a structure's sprite standing over rock.

`harness/slopes.gd` and `harness/split.gd` count the built cells main drew bent: 31, 19 by a slope and 12 by a drop only.

## Bake time

`harness/baketime.gd`: seed 2026's 16 areas, interleaved with main, at a 1-minute load of about 20 (`logs/baketime.log`):

| Round | Main | Head |
|---|---|---|
| 1 | 3093.8 ms | 3378.2 ms |
| 2 | 3314.0 ms | 3332.0 ms |
| 3 | 3252.1 ms | 3402.2 ms |
