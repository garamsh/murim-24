# Evidence for #342 (Part 3b of #209: sub-pixel camera)

Head `ad9926a9`, stacked on #341. Apple M1, macOS, Godot 4.7.2.

- `images/`: Korean captures at 1920x1080, seed 2026, zoom 3, dusk, the camera at 0, 1/3 and 2/3 of a world pixel, and 4x crops round the steered person (pre-rebase head `b84a14e6`).
- `logs/bench.log`: the PM-cleared counted bench, main `8dc3d774` on its own extension against this head. `logs/bench-aborted-shared-extension.log`: the aborted attempt that ran main on this head's library.
- `logs/mut-*.log`, `logs/rmut-*.log`: headless (one C++ mutation rebuilt each way) and renderer mutations.
- `logs/check-main.log`, `logs/gdtests-main.log`: grade_check 14/14 and the GDScript suite at this head; `logs/lint.log`: make lint (pre-rebase head).
- `harness/`: capture.gd, bench.sh, mut.sh, rmut.sh.
