# Evidence for #185 (in progress)

Branch `feat/sect-compounds` on main `330aaa7f`.

- Refused seeds, 0-199, by `harness/refusals.gd`: head 1 of 200 (191, no room for a seat); main 0 of 200.
- `play_test` move pass: with no input, a new game's person born in village 0 is pushed 0.55 cells over its first 20 ticks, then stands still to tick 120.
- `captures/`: the first seat capture (Hwasan's, seed 2026, tick 900, 3x) and the new sheets at 4x.
- `logs/tests-before-fixes.log`: the GDScript run before the test fixes.
- Sweeps: `harness/sweeps.sh`, main and head over seeds 0-190 and 192-199, pending art's turn.
