# Village captures for #328

Villages laid out by ADR 0076's roles (#169), compared with `main`.

## In-game captures (after)

These are in `game/`.

- **How they were made.** `capture_villages.gd` ran in the real `play/play.tscn` and HUD, in Korean, at 1920 x 1080, on Godot 4.7.2 on macOS arm64, at `feat/village-forms` 6b50645d.
- **What each shot is.** Each plays its seed's region at 5 ticks a day, with the player born into the village named, lived the years named. The camera is on the player.
- **What the script printed.** `capture_log.txt` gives each village's square, residents, structures within 20 cells, fields, fields on hills, and nearest other village.

| Shot | What it shows |
|---|---|
| `seed1_year0_village1` | A hamlet whose fields terrace hill cells (2 of 2 on hills) |
| `seed1_year0_village4` | The village start list beside hills (1 field of 2 on hills) |
| `seed0_year0_village7` | A village beside water |
| `seed5_year0_village0` | A hamlet beside water |
| `seed0_year0_village2` | A village on flat ground |
| `seed0_year0_village0` | A hamlet on flat ground |
| `seed10_year60_village3` | A village after 60 years, which has grown more structures |
| `seed6_year60_village7` | A village after 60 years, with a terraced field |

The year-60 villages were picked from a core run with no player. Born into them, the player changes the world's history. So in the game seed 10's village 3 had no residents left at year 60, and seed 6's village 7 had 9, where the core run had 22 and 20. Neither shows the village tier.

The town tier is not reached on the 128-cell region, and walls wait for ADR 0062, so neither is shown.

## Top-down before and after

These are in `topdown/`.

- **How they were made.** `render.cpp` draws 45 x 45 cells round the village at 8 pixels a cell. The same seed, village and years were linked once against `main` 14ab6461 (`before_*`) and once against `feat/village-forms` 6b50645d (`after_*`).
- **Templates.** Each seed draws the same templates on both, as #328's generation order keeps them.
- **Sites.** The sites differ, because `main` places villages with no `apart`.
- **Colours:**
  - green: plain;
  - dark green: forest;
  - tan: hills;
  - blue: water;
  - grey: mountain;
  - brown: homes (houses, granaries, storehouses, barrels);
  - red: square buildings (hall, well, smithy, apothecary);
  - yellow: fields;
  - dark brown: fence.
- **The square** is not drawn. It is the clear block at the centre.

**Before.** `main` stamps each template's rows of houses with fields squeezed between them.

**After.** Houses ring the square with a lane a cell wide. The hall and well touch the square. Fields lie beyond the houses, and on hills they terrace them.
