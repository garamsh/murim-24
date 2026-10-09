# A skill that levels a village

One high-realm skill hitting a whole village at the 2.5D angle, asked for on #200 (owner, 2026-10-09): the effect, the structures through their damage stages, and a camera pull-back from 3x to 2x with shake. Made by `angle.gd` here, the same file as `art/mockup/angle.gd` at 6a879ae, copied to the project root: `godot --path . --script res://angle.gd -- <dir> level`, then `bench level`.

## What the core resolves

- The world is made with `WorldAdapter.create` from seed 2026's region: its map, one structure of durability 100 on every cell of each building's footprint, 60 Demonic people of health 100 in 6 armies on the free cells round village 5, and one Orthodox caster at cell (30, 64), 15 cells south of the village's centre.
- The region's skills hit no structure (`core/region.cpp` gives every art skill a structure damage of 0), so the caster's skill is the mockup's data: range 24 cells, radius 11 cells, 40 body damage and 30 structure damage. Stage floors are 80, 50 and 20, so each cast takes a cell one stage down.
- It casts through `WorldAdapter.cast` at the village's centre at 2.3, 2.9 and 3.5 s; the core resolves each on the next tick. At the end all 60 villagers were reached and are dead, and village 5's 12 buildings are rubble; the region's other 69 structures, out of reach, stay intact.
- A building is drawn at the stage most of its cells are in, the worse on a tie.

## What the view adds

- **Camera**: 3x until 1.0 s, eased to 2x by 1.8 s round a centre that takes in the caster, held, and eased back to 3x from 5.0 to 5.8 s. Between the two it passes through fractional scales, so pixels shimmer while it moves; a cut from 3x to 2x would avoid that.
- **Pillar**: a column of qi rises over the village from 1.2 s, stands, and sinks from 4.4 s.
- **Sword qi**: 40 blades per cast fall 140 pixels in 0.3 s onto points spread over the skill's reach, landing with the cast.
- **Impact**: when a tick takes buildings down a stage, a ring of azure pixels grows to the reach on the ground, each such building throws dust and tile shards, and the camera shakes 4 pixels, dying away over 0.35 s.
- **Stages**: each building swaps to the core's stage at once, and its shadow with it, shorter destroyed and gone as rubble.

## Files

- `level.gif`: 121 frames, 15 a second, 8 s, 1920x1080 without the HUD (17 MB). `level_half.gif`: the same at 960x540 by nearest neighbour, which smears the 3x part (6.3 MB). `level_strip.png`: every 15th frame, 1 s apart. `frames/`: all 121.
- `bench.log`: every line the benches printed, with the streaming ones (`../streaming/`).

## Frame cost

`bench level`: the sequence in real time, 1920x1080, vsync off, Apple M1, Godot 4.7.2, three runs at 6a879ae.

| Run | Whole sequence p50 / p99 / max (ms) | Pull-back to return, 1.0 to 6.0 s: p50 / p99 / max (ms) |
|---|---|---|
| 1 | 3.310 / 9.609 / 37.971 | 3.872 / 9.246 / 17.195 |
| 2 | 6.902 / 11.253 / 49.507 | 6.898 / 8.953 / 11.253 |
| 3 | 3.031 / 10.055 / 38.404 | 3.230 / 9.102 / 11.907 |

- Run 2 is capped at the 144 Hz display's 6.9 ms, as in `../streaming/`.
- The whole sequence's maximum, 38 to 50 ms, falls in its first second, where the first frame builds all 6 chunks on screen at once; from the pull-back on, the worst frame was 17.2 ms (run 1, at 2.15 s) and 11.3 and 11.9 ms in runs 2 and 3, with 60 people and up to 9 chunks resident at 2x.
- Before structure shadows were sprites, each stage change built its chunk again: at 6ddf73a, with lines added to time the worst frame, that frame took 52.8 ms at the first impact, 37.6 ms of it building chunks. 6a879ae made shadows sprites.
