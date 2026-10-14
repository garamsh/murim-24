# Evidence for #323, review round 1

Head `a44ac4e7`, main `7f986689`. Apple M1, macOS, Godot 4.7.2. Round 0's evidence is in this branch's history (parent commit).

- `r1/images/`: Korean captures at 1920x1080, seed 2026, zoom 3: `main-*` (main) against `r1-*` (head) for day, dusk, night, night-low, smithy-night, smithy-night-low; `crop-night-head.png` (round 0) against `crop-night-r1.png` (pools after the fix); each tree's night-village bench frame.
- `r1/logs/bench.log`: the counted bench, main against head over three rounds: frame, night village high, low, and high with a hearth listed. Each run is headed by load and other builders; the quiet file was held, but other workers' builds raised the load to 23.
- `r1/logs/bench-contaminated-432bd2fe.log`: the bench at the pre-field head, contaminated the same way; it showed the light loop's cost.
- `r1/logs/nolights.log`: the night village at low with and without lights, with the loop emptied, with a data texture, and with the light field.
- `r1/logs/mut-*.log`, `r1/logs/rmut-*.log`: headless and renderer mutations.
- `r1/logs/check-*.log`: grade_check at each stage.
- `r1/logs/ci.log`: the local make ci run, killed from outside at ctest 68/279.
- `r1/harness/`: village.gd, bench.sh, capture-r1.gd, mut.sh, rmut.sh, rmut2.sh.
