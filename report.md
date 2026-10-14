# Evidence for #323 (Part 1 of #209)

Head `094ab8e2`, main `22f03a29`. Apple M1, macOS, Godot 4.7.2.

- `images/`: Korean captures at 1920x1080, seed 2026's birth village, zoom 3, `main-*` against `head-*` (day, dusk, night, night-low, smithy-night, smithy-night-low), crops `crop-night.png` and `crop-smithy.png`, and each tree's night-village bench warm-up frame.
- `logs/bench.log`: the counted bench, alternating main and head over three rounds, each run headed by load, power source and other builders.
- `logs/mut-*.log`: headless mutations, each failing its test.
- `logs/rmut-*.log`: renderer mutations, each failing its `grade_check` check.
- `logs/names-*.log`: the final grade_check harness, 10 runs (8 pass 6/6, 2 exit 2 "out of frames"). `top-*`, `drawn-*`, `flaky-*` and `dbg-*` come from earlier harness revisions.
- `logs/ci.log`, `logs/gcc.log`: local make ci and the GCC 16.2 Release -Werror ctest.
- `harness/`: capture.gd, crop.gd, night-village.gd, bench.sh, bench1.sh.
