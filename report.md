# Evidence for #402: trade looks and work motion

- Save: seed 2026 aged 20 years (`harness/age.gd`), 58 living. Village 6, Korean locale, 1920x1080.
- `captures/v6_08.png`, `captures/v6_20.png`: 08:00 and 20:00, taken with `harness/capture.gd`, and their 4x crops by `harness/crop.gd`.
- `captures/lineup_4x.png`: every base-pack trade's idle, walk and work frames on the three faction bodies (`harness/lineup.gd`).
- `logs/bench.log`: three alternating rounds of `frame_bench_tool` and `village_bench_tool`, main `ce580309` against head `7d1d9d17`, by `harness/bench.sh`. The head commit was later amended to `9998e846` for one variable rename (clang-tidy), with no change in behaviour.
- `harness/cmp.gd`: checks that the regenerated sheets keep the old 34 frames pixel for pixel and that each trade frame equals the mockup's.
