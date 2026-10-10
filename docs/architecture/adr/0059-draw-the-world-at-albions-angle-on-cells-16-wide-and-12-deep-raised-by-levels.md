# ADR 0059: Draw the world at Albion's angle, on cells 16 pixels wide and 12 deep raised by levels, its ground baked off the cell grid

> Status: accepted; its lift of 16 pixels a level at every height superseded by ADR-0058, under which the lift is 16 pixels for the first 3 levels and 6 a level beyond
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0012 shows the world three-quarter top-down on the 16-pixel grid: one cell to one 16 x 16 tile, facades and bodies upright, and the ground laid as tiles on that grid, as `docs/convention/stack-godot.md` §Terrain layers had it until #221 replaced it with §Ground. The owner decided on 2026-10-09 to go 2.5D at Albion's angle, in 2D pixel art and not full 3D (#200). The mockup of #204 (merged as d89a52f, `art/mockup/angle.gd`) drew that angle from the core, and the PM approved its round-4 look on 2026-10-10 for the owner, with acceptance for the port in #200:

- The ground has no straight or right-angled patch edges, a clean forest-edge dither, and no patch of one kind floating on rock.
- It is baked once when the map is built, and no viewport draws again after the bake.
- Rock faces with uneven strata, scree and plants; three tall steps with slopes between; Poisson-disc detail and clustered trees; contact shadows.
- Depth sorting with outlines, and bodies batched within the 16.7 ms frame budget at 500 people.

The owner approved #212's natural terrain step by step: the ground drawn off the cell grid by a shader reading the map's cells, with sub-cell edges, dithered transitions, rims and hue-shifted ramps.

The core gives each cell a terrain kind and nothing more. ADR 0047's map of four bytes a cell, which would carry levels, is proposed and not built (#173). The PM decided on 2026-10-10 that the view may derive levels from the terrain kinds until then, on one condition. What it draws must never contradict what the core lets people do: a cliff or rock face only where the core blocks movement, and passable ground rising as a slope a person visibly walks over.

## Considered

- **Cells 16 wide and 12 deep, raised 16 pixels a level, the ground baked per area from a shader.** The mockup's projection, which the PM approved. Bodies and buildings keep their 16-pixel width; depth is foreshortened by a quarter, so the ground reads as seen at an angle. Baking once keeps the frame's cost at sprites alone.
- **Keep ADR 0012's 16 x 16 grid with tile layers, and draw height in the tiles.** Rejected: the owner judged the round-1 mockup, which did that, not 2.5D, and tile layers on the cell grid keep the straight edges #212 removes.
- **Draw the shader every frame instead of baking it.** Rejected: the mockup measured p50 22.6 ms when its baking viewport drew every frame (#204, round 5), against a 16.7 ms budget.
- **Put levels in the core now.** Rejected for this change: the core's map format is ADR 0047's, which #173 builds, and #214 (climbing and sight) needs them there. Until then a view-derived relief draws nothing a rule could contradict.
- **Full 3D, or isometric.** Rejected by the owner's decision (#200) and by ADR 0012's reasons for isometric.

## Decision

The world is drawn at Albion's angle in 2D pixel art.

- **Cells.** A cell is drawn 16 pixels wide and 12 deep, and ground on level `n` is raised 16 pixels times `n`. Facades and standing bodies stay upright and are drawn in order of the row their footprint ends on; the raised rows of the ground are sorted among them, so whoever stands behind raised ground is drawn behind it.
- **Relief.** Each cell has a kind of ground, a level, a face material and whether it rises as a slope. Until the core's map carries levels, the view derives them from the core's terrain kinds:
  - water lies a level below the land, behind a bank;
  - plain and forest are flat at level 0;
  - hills are slopes rising to levels 1 and 2;
  - mountains are rock in three tall steps, levels 2, 4 and 5 by how far a cell lies inside them.

  A cliff is drawn only where the core blocks movement.
- **Ground.** A shader draws the ground off the cell grid from one texel a cell: edges warped by noise and interpolated between cells, rims, dithered transitions, rock faces in strata, and ramps shifted in hue. It is baked once per 32-cell area when the map is built, into a viewport freed once the last area is drawn. Detail and decorative trees are scattered per area by Poisson-disc spacing from a seed the area's index gives. The baked areas are shown only within a margin of the screen.
- **Decisions stay in the core.** The relief, the ground and its trees decide nothing; a tree the rules know comes from the core.

This supersedes ADR 0012's 16-pixel grid and its tiles. ADR 0012's upright facades, footprints set by data, real-time combat, Aseprite art and Korean-first text stand.

## Consequences

- Every place the view turns a cell into pixels, or a pixel into a cell, goes through the one projection: bodies, structures, banners, borders, the player's markers, the effects of combat, the aim, picking and the sound's hearing.
- A click picks the ground drawn under it, found down the pixel's column as the shader finds what it shows.
- The core's levels, when they come, replace the derived relief and keep the projection; the derived relief is deleted then.
- Ground art is no longer 16 x 16 tiles laid with corner masks. The ground is drawn as `docs/convention/stack-godot.md` §Ground sets out, and its art is each kind's ramp, detail and trees (`docs/art-direction.md`). What people build in straight lines, a village's paving and road among them, is laid over it as sprites.
- Building the map bakes every area before the first frame of the world shows. The time this takes grows with the map: for a 128-cell region it is about two seconds on the development Mac.
- A headless run has no renderer, so it bakes nothing. Tests check what the view gives the shader and where it places what it scatters, with a stand-in for the shader; the pixels it draws are shown by captures and checked by a tool run by hand on a display (`world_art/ground_check.gd`).
- Art drawn for the 16 x 16 cell is redrawn at the angle as each part is ported: bodies with their sorting and outlines, then structures.

## Signs the decision was wrong

- Building the map takes long enough at 4,096 cells (ADR 0047) that areas must be baked as they stream in, and streaming then stalls frames.
- People walking hills and banks are drawn floating over or sunk into the ground often enough that the CPU's height and the shader's have to be reconciled by hand.
- The derived relief draws a cliff a person walks through, or the warped edges put people visibly in water or on rock they cannot reach.
