# Evidence for the Central Plains design

The measurements behind ADRs 0041 to 0045 (issue #173, phase 1), so a reader can repeat them. Nothing here is part of the game.

## What was measured

- Machine: the development Mac, Apple M1, 8 GB, macOS, on AC power. Other workers' test runs shared it at a load average of 7 to 9, so every timing carries that noise and the ADRs quote ranges over runs.
- Source: `core/` of murim-24 at `origin/main` 1d943f0, with `raise-bounds.patch` making `MAX_PEOPLE` and `REGION_SIDE` settable. Built with `-O3 -DNDEBUG`, as `make bench`'s Release build is, at `MAX_PEOPLE` 5,000 and `REGION_SIDE` 128 or 512.
- `make bench` at 1d943f0 on the same machine: p50 0.471 to 0.593 ms, p99 0.797 to 1.130 ms. `measure battle 500 128` reproduces that scenario: 498 alive at the end, p50 0.52 to 0.62 ms.

## Files

- `measure/measure.cpp`: the program. Its modes:
  - `battle <people> <side>`: the M1 battle with one more column of ten 5 x 5 armies a side per 500 people, on an open map, 60 ticks of warm-up then 600 timed
  - `field <side>`: 20 flow-field builds toward the centre of an open map
  - `terrain <seed>`: the cells of each terrain kind in the generated region
  - `region <people> <seed>`: the generated region, its villages' people raised by an army homed in each village on the open cells nearest its centre, 600 ticks of warm-up then 900 timed; then the save's bytes through an in-memory store; then the same world with 60-tick days, its day ticks timed apart
- `measure/build.sh <repo>`: builds `measure_128` and `measure_512` from a checkout.
- `measure/raise-bounds.patch`: the only change to `core/`.
- `measure/battle128.txt`, `battle512.txt`: three runs at 500 to 5,000 people at 128 cells; one at 512.
- `measure/region.txt`: seeds 2026 and 7 at 128 cells and seed 2026 at 512, at 500, 2,000 and 5,000 people. It was taken before two print-only additions to `measure.cpp`, each day's tick and the map's row runs, which `days128.txt` and `map512.txt` hold.
- `measure/field.txt`, `field-earlier.txt`: flow-field builds at 32 to 1,024 cells, over six runs.
- `measure/terrain512.txt`: the cells of each terrain kind at 512 cells on seeds 2026, 7, 24, 0 and 1, which bound the natural objects of #187 the forest and hills would carry.
