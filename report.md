# #282 step 4: the captures (ADR 0068, "The view")

- Before: main `cbd37d2f`, the parent of `d16771b` (#331, step 2).
- After: main `b2556de` (#358, step 3).
- Each tree runs on its own extension (`make build`).
- Machine: Apple M1, macOS, Godot 4.7.2 (Compatibility renderer, OpenGL 4.1 on Metal).

The view needed no change: the waiting ring is drawn from the people's positions, as ADR 0068's step 4 says.

## How they were taken

- Scene: `world_view.tscn`, the M1 battle (seed 2026), at 1920x1080 with the Korean locale.
- Ticks: run with `--fixed-fps 30`, so every frame is exactly one tick. Both trees are shot at the same ticks.
- Determinism: each tree's world hash at a tick is the same in every run (in `logs/`).
- Settings: the rows the game exposes, applied the way `play.gd` applies them.
  - Damage numbers off.
  - Blood off.
  - Hit stop off, which also turns off its screen shake.
  - Rain off.
  - Screen quality left at its default, standard.
- What no setting removes: the swing shapes and the nameplates. `world_view` names the people round one army's lead.
- The art differs between the two trees: #354 (attack motion at the 2.5D angle) merged between them. Before draws #208's strike shapes; after draws #354's swings. Only the bodies' places are the rule's.

## The M1 battle: `m1/`

- Files: `m1/before-tickNNNN.png` and `m1/after-tickNNNN.png`, at ticks 90, 180, 270, 360, 450, 600, 750, 900, 1050, 1200, 1350, 1500, 1650 and 1800.
- Camera: on the battle's centre, as the scene places it.
- What to compare: ticks 900 and 1800.
  - Before: the two lines run into each other and stack into one band of overlapping bodies.
  - After: the front stays two lines. Behind each line, those without a token stand back in looser ranks; that is the waiting crowd.
  - After, by tick 900, small knots break off the east side of the front, with a few people round one foe and the rest at a distance (top right at 900, right of centre at 1800).

## One foe ringed by a crowd: `ring/`

M1 is a line battle, so a lone foe shows only where one is cut off. The two trees are not the same encounter: **before never cuts a foe off.**

### How the probe measured it

Every 30 ticks from 30 to 2,400, the probe took the body with the most hostile bodies within 3.2 cells, less four times its allies there (`capture.gd`). The counts it logged for that body:

- hostile bodies within 3.2 cells (820 sub-cell units);
- those of them in the striking band, within 576 units (the strike ring sits at 480);
- those standing back, from 576 to 820 units (the waiting ring sits at 672);
- allies within the same 3.2 cells.

Across the 80 probes in each tree:

| Tree | Probes with that body at 0 or 1 allies | Fewest allies |
|---|---|---|
| Before | 0 of 80 | 4 |
| After | 34 of 80 | 0 |

The full lists are in `logs/capture-*.log`, on the `probe:` and `capture:` lines.

### Same camera, same tick, both trees

The camera is held on the point where after's foe stands, at the same tick in both trees (`ring.gd`).

| Shot | Tree | Body nearest the camera | Hostile in 3.2 cells | Striking band | Standing back | Allies |
|---|---|---|---|---|---|---|
| `ring/*-p74-tick1440.png` | after | person 74, cut off | 13 | 6 | 7 | 0 |
| | before | person 144, inside its own line | 0 | 0 | 0 | 14 |
| `ring/*-p309-tick1050.png` | after | person 309, cut off | 25 | 13 | 12 | 0 |
| | before | person 292, inside its own line | 2 | 0 | 2 | 26 |

Before, at both points, the lines simply hold.

### Each tree's own most crowded foe, at the same ticks

`ring/*-crowded-tick1440.png` and `ring/*-crowded-tick1050.png`. The camera is on the body with the most hostile bodies within 3.2 cells in that tree, with no allowance for allies.

| Tick | Tree | Person | Hostile | Striking band | Standing back | Allies |
|---|---|---|---|---|---|---|
| 1440 | before | 374 | 31 | 16 | 15 | 9 |
| | after | 372 | 30 | 18 | 12 | 12 |
| 1050 | before | 225 | 29 | 18 | 11 | 14 |
| | after | 331 | 33 | 19 | 14 | 5 |

Inside the line, the counts look alike in both trees. Before's crop at 1440 shows bodies overlapping round the foe. After's is covered by swings, so it does not show the spacing either way.

### Files

- Each `ring/` shot is a 3x crop of the frame's middle 640x360. The whole frame is beside it as `*-frame.png`.
- `ring/<tree>-tickNNNN.png` are `capture.gd`'s own 3x crops round the top-scoring foe of each M1 shot, on the battle-centre camera.

### Why the crops are hard to read

The crops cannot carry the claim alone. #354's swings cover the bodies inside a crowd, and #355 (fewer phase-1 shapes in a crowd) is the follow-up already filed. The counts above are what show the ring.

### A recommendation

A staged-ring scene would be worth having. It would be one foe and about twenty attackers under the default tokens, on open ground, in both trees.

- It would show the ring without a battle round it.
- No scene reachable from GDScript gives one today:
  - `WorldAdapter.create()` sets neither tokens nor sight, so a staged world there has no waiting ring.
  - The region changed between the two trees (#328 and #362), so it is not the same scene.
- It needs a code change, for example `create()` taking `tokens` and `sight`, or a core scenario. That is the PM's call.

## Files

- `m1/`, `ring/`: the captures.
- `harness/`:
  - `capture.gd`: the M1 shots and the probe.
  - `ring.gd`: the held-camera shots.
  - `run.sh`: the exact runs, for one tree.
- `logs/`: one log per run, with the world hash at each shot.
