# Evidence for #354 (#184 phase B: attack motion at the 2.5D angle)

Head `32af0d80` on main `dc4306bc`. Apple M1, macOS, Godot 4.7.2, Aseprite 1.3.18.6. The approved mockup is on `evidence/mockup-184`.

- `seq/strip-sweep-ko.png`, `seq/strip-dart-ko.png`, `seq/strip-area-ko.png`: staged runs, Korean locale, at 3x in daylight, frame by frame from the hit tick; `seq/sweep*`, `seq/dart*`, `seq/area*` the whole frames (16 each).
- `seq/frame*-ko.png`, `seq/crop*-ko.png`: eight consecutive frames of the M1 battle at night, and 4x crops round its busiest use.
- `logs/bench.log`: the PM-cleared counted bench, make bench's M1 frame benchmark, main on its own extension against the head.
- `logs/mut-*.log`: the mutations (C++ rebuilt each way).
- `logs/gdtests-4.log`: the GDScript suite at the head; `logs/lint.log`: make lint before the clang-tidy fixes.
- `harness/`: duel.gd, sequence.gd, strip.gd, bench.sh, mut.sh.
