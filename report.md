# Evidence for #324 (Part 2 of #209)

Head `52abddef`, stacked on #323's head `a44ac4e7`. Apple M1, macOS, Godot 4.7.2.

- `images/`: Korean captures at 1920x1080, seed 2026, zoom 3: `p1-*` (#323's head) against `p2-*` (this head) for morning, day, dusk, night, day-low, mountain-morning, mountain-day. Seed 2026's levels 4-5 lie only on the map's edge, so the mountain shots keep the camera half a screen in from it.
- `logs/bench2.log`: the counted bench, p1 against p2 over three rounds: M1 frame, village at midnight and at morning, high. The quiet file was held; other workers' builds raised the load to 9-19.
- `logs/mut-*.log`, `logs/rmut-*.log`: headless and renderer mutations (run at the pre-rebase head `1b688873`; the rebases changed no fog or cloud line).
- `logs/check-*.log`, `logs/repeat-*.log`: grade_check runs.
- `logs/gdtests*.log`: the GDScript suites.
- `harness/`: capture.gd, village.gd, bench2.sh, mut.sh, rmut.sh, levels.gd.
