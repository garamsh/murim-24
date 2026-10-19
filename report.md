# Evidence for #411

Head `02aff1cc` on `fix/detail-off-built`; main `6e1ad822`. The ground shader is #410's in both; only `world_art/ground.gd` and `relief.gd` differ.

## The tool's map

`harness/probe.gd` bakes ground_check's map with three blocks of built cells: on the plain across a forest edge (`flat`), on a slope (`slope`) and below a face (`drop`).
- `captures/probe_{main,head}_<block>_warped.png`: 4x crops round each block, before and after.
- `captures/probe_marked_<block>.png`: the head crop with every pixel that differs from main's in magenta.

Every marked pixel is a tuft, flower or bit of litter that main laid inside a built block or on the row along it. Pixels differing at 4x: `flat` 960, `slope` 736, `drop` 432.

## The game

`harness/cellshot.gd`: seed 2026 at tick 900, the camera at 3x on cell (87, 124), village 0's square, and on (124, 71), village 2's hamlet. Frames and crops are `captures/{main,head}_<x>_<y>*.png`. Most of what is built there lies under paving or structure sprites, so whole frames differ by 81 and 45 screen pixels. The `_seam4x` crops are the 360x200 windows where those differences are densest.
