# #364 temple and inn

Evidence for the content PR from `feat/temple-inn`. The head is `e42cb66f`, on main `d60c33d2`. Everything ran on the development Mac (Apple M1, macOS, Godot 4.7.2) on 2026-10-16.

- `images/day-ko.png` and `images/night-ko.png`: captures with the Korean HUD at 1920x1080, by `harness/capture.gd`.
  - The scene is the production play scene of seed 2026, with the player born in village 7 (index 6), a village-template village with a temple and an inn.
  - The camera stands between the two buildings, at zoom 3, clamped as the play scene clamps it.
  - Both are at high quality, summer noon and summer midnight.
  - `logs/capture.log` holds that run's output, copied from the terminal.
- `logs/bench.log`: the night-village bench, by `harness/bench.sh` and `harness/night-village.gd` (#323's harness, with the player born in village 7).
  - It runs at summer midnight at high, 1920x1080, vsync off, with 120 warm-up and 1,200 timed frames.
  - Three rounds alternate main, head, and head with no hearth passed to the grade, so bloom stays off over the same 27 lights.
  - Each run starts once the 1-minute load is below 4, in the PM's quiet window.
  - Main ran in a scratch worktree of `d60c33d2` on the same extension binary, since the branch changes no C++ outside tests.
- `logs/sweep-main-d60c33d2.md` and `logs/sweep-head-e42cb66f.md`: `make sweep` over seeds 0 to 199 for 100 years.
  - Main ran the commands behind `make sweep` in its scratch worktree.
  - The head ran `make sweep FIRST_SEED=0 LAST_SEED=199 YEARS=100`.
- `logs/sweep-main-fe0b2c96.md` and `logs/sweep-head-a7855632.md`: the same sweep, re-run on 2026-10-17 after the branch was rebased onto main `fe0b2c96` (#388, which changes how a raid band picks its target).
  - Both binaries are `murim_core_sweep`, built as `make sweep` builds it: CMake Release with the macOS SDK. Main was built in a scratch worktree of `fe0b2c96` and the head in the branch's worktree at `a7855632`.
  - Each ran `murim_core_sweep 0 199 100`. The two ran side by side; the seeds fix their results.
