# Evidence for #341 (Part 3a of #209: rain)

Head `ebd55c4d` on main `8dc3d774`. Apple M1, macOS, Godot 4.7.2.

- `images/`: Korean captures at 1920x1080, seed 2026, zoom 3 (pre-rebase head `06ebd398`): noon dry, rain at noon, dusk and midnight, noon raining at low, and a 2x crop of the streaks.
- `logs/bench.log`: the PM-cleared counted bench; the rain rows are `sub noon-dry` and `sub noon-rain` (3b's head, which holds this branch unchanged).
- `logs/mut-*.log`, `logs/rmut-*.log`: headless and renderer mutations (pre-rebase head).
- `logs/check-head.log`, `logs/gdtests-head.log`: grade_check 12/12 and the GDScript suite at this head.
- `harness/`: capture.gd, village.gd, bench.sh, mut.sh, rmut.sh.
