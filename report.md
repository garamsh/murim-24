# Evidence for #400

Head: `c1b02f6041cd8b218285e48beb0574f5d80695c8` on `feat/ground-past-edge`. Main: `79eda20b13a0b32bfad43ff5027999248475968f`.

## Captures

Seed 2026, village 0, Korean locale, by `harness/capture.gd` (`<out> <W>x<H> <touch 0|1> <ticks...>`).

- `captures/{main,fix}_1920x1080_t{0,2000}.png`: PC.
- `captures/{main,fix}_2048x1536_touch_t2000.png`: touch, at zoom 4, with the HUD's bars covering 87 world pixels.
- `*_bare.png`: the same frame with the grade and the HUD hidden.
- `captures/{main,fix}_touch_crop4x.png`: 360x200 round (700, 1300) of the touch frame, at 4x, by `harness/crop.gd`.

## Void count

`harness/void.gd` counts each frame's pixels of the void's colour, `#20243e`, probed at (0, 1079) of `main_1920x1080_t0.png`. Log: `logs/void.log`.

| Frame | Main | Head |
|---|---|---|
| 1920x1080, tick 0 | 23157, rows 1014-1079 | 0 |
| 1920x1080, tick 2000 | 22092, rows 1020-1079 | 0 |
| 2048x1536 touch, tick 2000 | 296688, rows 1188-1535 | 0 |

## Tool

`godot --path . --script res://world_art/ground_check_tool.gd` on a display, at head: 9 of 9 checks pass. Log: `logs/tool.log`.

## make ci

Exit 0 at head: 34 of 34 GDScript tests passed. Log: `logs/ci.log`.

## Frame bench (partial)

`harness/bench4.sh`. Before each start it waits until the CPU summed over every other process, except those under `/System/` and `/usr/libexec/`, is at most 50% and the 1-minute load is under 4, then logs the reading it passed on and the busiest processes. Log: `logs/bench4-gated-partial.log`. The PM closed the window after `frame_bench_tool`'s three rounds and `pan_bench_tool`'s first round.
