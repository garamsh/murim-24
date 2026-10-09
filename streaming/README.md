# Streaming by chunk

What the 2.5D view holds and builds when it renders only round the camera, asked for on #200 (owner, 2026-10-09), measured in `angle.gd` at 6a879ae (`levelling/angle.gd` here is that file).

## What streams

- **Chunk**: 32 x 32 cells, ADR 0042's area. Its ground is one 512 x 392 image (32 cells of 16 x 12 pixels, and 8 rows above for raised hills), 802,816 bytes as RGBA8.
- **Radius**: a chunk is built once it comes within 64 pixels of the screen, and freed once it is a whole chunk past that. At most one is built a frame, nearest the screen first; the first frame builds every chunk it needs at once.
- **Per chunk**: the ground image (terrain, roads, paving, fields, hills with their step, tree shadows), and the sprites of its structures, their shadows and its trees. A structure's shadow is a sprite of its own, so a structure changing stage swaps two textures and rebuilds nothing.
- **Bodies**: only those within 32 pixels of the screen get a sprite; the y-sort and the see-through outlines run on those alone, and a body's sprite is freed when it leaves.

## Measured

`godot --path . --script res://angle.gd -- <dir> bench [pan|m1]`, 1920x1080, vsync off, Apple M1, Godot 4.7.2, three runs at 6a879ae; `bench.log` holds every printed line.

| Scene | Frame p50 (ms) | Frame p99 (ms) | Frame max (ms) | Chunks resident, most | Chunk build p50 / max (ms) |
|---|---|---|---|---|---|
| village under the HUD, camera still | 3.959, 6.910, 6.911 | 13.673, 8.557, 8.613 | 16.174, 8.842, 9.718 | 6 | 4.0 to 4.4 / 8.9 to 9.2 |
| `pan`: the camera round the region at 20 cells a second, 104 to 184 cells | 3.148, 6.906, 6.876 | 12.198, 8.353, 8.863 | 15.030, 16.025, 17.644 | 12 | 4.0 to 5.3 / 8.8 to 9.2 |
| `m1`: 500 people, about 354 within the screen | 6.894, 6.903, 6.895 | 9.908, 14.519, 9.815 | 27.875, 44.781, 24.445 | 4 | 2.7 / 2.8 to 3.3 |

- Run 2's p50 is 6.9 ms in every scene, the period of a 144 Hz display, where runs 1 and 3 run the same scenes at 3.0 to 4.5 ms; vsync was asked off in all three. I take run 2 as capped and read runs 1 and 3.
- Panning, chunks build one a frame at 4 to 5 ms each in GDScript, and the p99 stays at 12.2 ms (run 1) and 8.9 ms (run 3).
- With 500 people the maxima are 24 to 45 ms against 15.2 to 18.9 ms unstreamed (`../bench.log`, at b8532e5 and ba32a9f), and the p99 is 9.8 to 9.9 ms in runs 1 and 3. I have not traced those maxima. One candidate: the mockup frees a body's sprite as it leaves the screen's margin and makes a new one as it comes back, where pooling them would not.

## At 4,096 x 4,096 cells

- 16,384 chunks. Resident chunks depend on the screen, not the world: at most 12 at 3x while panning, 9 at the 2x pull-back (`levelling/`). Their ground images hold at most 12 x 802,816 bytes, 9.6 MB, at any world size.
- What the mockup holds for the whole map and the game would not: a terrain kind per cell and an index of the cells roads, paving and fields cover, read once from the core's 128-cell region. At 4,096 that is 16.8M cells, so the game reads them per area as the chunk loads, which is the per-area map format #201 asks for.
- Not in the mockup: the minimap and map window from prebuilt images. The HUD's minimap in the stills is today's.
