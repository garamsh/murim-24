# ADR 0075: Read the world once a tick and draw bodies and the fighting natively

> Status: accepted
> Date: 2026-10-12

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-09 to stay on Godot and make the frame cheaper rather than write an engine (issue #210). The cost that grows is the view's GDScript, not Godot's renderer. Every tick, `world_art.gd`, `combat.gd` and `sound.gd` each read every army's roster, positions and health through the adapter, one call per army, and compared every person with the tick before. Every frame, `world_art.gd` placed, tagged, tested for what hides it and packed every body on screen, and `combat.gd` filtered and drew the uses of skills and the numbers, one Dictionary each.

Options weighed:

- **One packed read a tick, and the per-person view work in native code in the adapter.** The adapter gives in one call every living body's person, position, health and ready tick, each army's faction, each structure's stage and the tick's uses as packed arrays. A class in the GDExtension, beside `WorldAdapter`, keeps by person what the view compares and holds, packs the bodies' MultiMesh buffers each frame and draws the uses, the target's brackets and the numbers through the rendering server in one call. GDScript keeps the scene: the ground, structures, borders, blood rings, sound and the HUD.
- **The same work in typed GDScript.** Rejected unmeasured: the loops stay interpreted, and each body still costs a call per vector operation; the packed read alone removes the calls per army, not the work per person.
- **The view's work in the core.** Rejected: the core is engine-free and holds the game's rules (ADR 0001). Flashes, hit stops and atlas frames are the view's, and would put the view's decisions and the renderer's buffer layout inside the application.
- **A Godot engine module, or another engine.** Rejected by the owner's decision of 2026-10-09; a module also binds the build to a custom editor and export templates.

## Decision

- `WorldAdapter.snapshot()` reads, in one call a tick, what a view reads after the tick, as packed arrays. The world art and the sound read one snapshot a tick between them.
- `BodyView` (`extension/body_view.cpp`), registered with Godot and reached through `world_art/combat.gd`, keeps the bodies and what the fighting did to them by person, packs every body on screen into the rows' buffers each frame and draws the uses of skills, the target's brackets and the damage numbers in one call. It holds no game rule: it reads the snapshot and decides only how the view shows it.
- The view's numbers that GDScript and the native code both need, its timings and the buffer's layout, are `BodyView`'s integer constants, so neither language repeats them.
- `make bench` prints where a frame's time goes: the core's tick, the adapter's snapshot, the view's scripts with the native code they call, and rendering.

## Consequences

- Measured on the development Mac (Apple M1, on AC power, each run started at a 1-minute load under 4) on 2026-10-12, three runs of each, alternating: in the M1 battle, `make bench`'s frame p50 fell from 8.26 to 10.95 ms on main at `5de863f` to 4.49 to 5.14 ms, and its p99 from 13.69 to 16.10 ms to 10.85 to 11.35 ms. In a generated region's village, through `world_view/village_bench.gd`, p99 stayed within noise, 8.27 to 8.76 ms on main and 7.98 to 8.48 ms here.
- Main before the combat feedback of issue #208, at `aff1434`, gave a p50 of 3.48 to 4.48 ms and a p99 of 8.41 to 12.38 ms in the M1 battle. It drew about 307 bodies on screen against about 400 here, with no outlines and no baked ground: #226, #235 and #239 added those after it, and they are not the view's scripts.
- A frame of the M1 battle spent, on average, about 0.1 ms in the core's tick, under 0.01 ms in the adapter's snapshot, 0.8 to 1.0 ms in the view's scripts and the native code they call, and 1.3 to 1.5 ms in the renderer's CPU, of 5.2 to 5.8 ms; the rest, about 3 ms, lies outside all four, and did not change with the outlines' pass turned off. With the names' people packed too, one more round, on main at `2cc1292`, gave the view's scripts 0.52 ms of a 4.92 ms mean frame, and an M1 p50 and p99 of 4.00 and 10.03 ms, against main's 9.01 and 13.45 ms and `aff1434`'s 3.57 and 9.68 ms.
- Changing how bodies or the fighting are drawn means changing C++ and rebuilding the library, where a GDScript change needed no build.
- The native code repeats the GDScript arithmetic it replaced, single-precision points and double-precision floats, and `relief.gd`'s height of the ground, so bodies stand where they stood; `relief.gd` and `ground.gdshader` remain the ground's other two readings of it.
- A load shows the world from a clean start: `BodyView.begin()` drops every hit stop and held body, and `combat.gd` its blood (issue #268, folded into #210).

## Signs the decision was wrong

- The view script's share of a frame, as `make bench` prints it, does not fall below what rendering takes, so the native code bought nothing the renderer does not spend again.
- Drawing changes stall on rebuilding the library often enough that the view's look is changed less than it is asked to be.
- A body or a use is drawn somewhere the GDScript would not have drawn it, because the two readings of the ground or the frame drift apart.
