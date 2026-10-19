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

## Review round 1 (head `0e2b1022`)

- `logs/review/tool-0e2b1022.log`: 12 of 12 pass. The trodden check also reads the row past the map's bottom edge below cells built on the last rows (`AT_BOTTOM`). The face check's control compares warped with unwarped feet beside the built cells.
- `logs/review/tool-without-s1.log`: with the shader's old early return off the map, 43 of 96 pixels of that row are trodden, and the check fails.
- `harness/probe_lip.gd`: built cells just north of water, and below the face, at `10966d19` (`captures/review/pre_*`) and at head (`captures/review/fix_*`). By row:
  - the lip: only the built cells' last row changes, 64 px across, which no longer takes the lit lip;
  - below the face: only ground rows 432 to 435 change, which now carry one solid shadow row and nothing dithered below it.

## Pan bench (S4)

`harness/bench4.sh`, `pan_bench_tool`, 3 rounds interleaved in the PM's window: base `af3dbfc6` (this branch's base, whose C++ matches the build) against head `0e2b1022`. Each start waits until the CPU summed over every other process, except those under `/System/` and `/usr/libexec/`, is at most 50% and the 1-minute load is under 4, and logs what was running. Log: `logs/review/pan-bench.log`.

| | p50 | p99 |
|---|---|---|
| base | 3.465 / 3.836 / 3.510 ms | 8.954 / 9.904 / 9.282 ms |
| head | 3.634 / 3.888 / 3.672 ms | 9.638 / 9.702 / 9.847 ms |
