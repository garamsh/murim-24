# ADR 0008: Present the world in an orthogonal top-down view, in real time, as pixel art on a 16-pixel tile grid

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

`docs/concept.md` left the camera perspective, the combat style and the art direction open. Each changes what contributors build: how the view maps the core's cells to the screen, whether input waits for turns, and what form art assets take. The core places bodies on a cell grid (ADR 0005) and advances at a fixed tick rate (`godot-adapter.md`), and `world_view/world_view.tscn` draws every body through one `MultiMesh`, at 16 pixels per cell (`world_view/world_view.gd`). Options weighed:

- **Orthogonal top-down.** A cell maps one-to-one to a square tile, so the view adds no projection to the grid the core works on, and thousands of bodies on screen stay apart and readable.
- **Isometric.** Rejected: a cell becomes a diamond, bodies overlap by depth and need sorting, and each asset is drawn for a projection the core does not have.
- **Side-scrolling.** Rejected: it shows one row of the map, while battles in the core spread over two dimensions with thousands of bodies.
- **Real-time combat.** The core advances on a fixed tick whether or not the player acts, and the world changes without the player (`docs/concept.md` §Identity).
- **Turn-based combat.** Rejected: it would stop a world whose factions and armies keep acting, or it would need a second rule for the player that NPCs do not follow.
- **Pixel art on a 16-pixel tile grid**, from Aseprite sources exported by its CLI (`docs/convention/stack-godot.md` §Asset export). One cell is one tile, the size the view already draws a cell at.
- **Hand-painted art.** Rejected: it does not reduce to the small, repeated sprites thousands of bodies need, and it has no export path the project already sets.
- **3D art.** Rejected: it adds a 3D pipeline to a 2D game (`docs/concept.md` §Identity) for a view that looks straight down.

## Decision

The world is shown in an orthogonal top-down view, with one core cell to one tile, and combat runs in real time on the core's tick. Art is pixel art on a 16-pixel tile grid, made in Aseprite and exported by its CLI.

## Consequences

- The view maps cells to tiles by a scale alone; there is no projection or depth sorting to build.
- Combat input acts on a world that keeps ticking; nothing waits for the player.
- Every sprite and tile is drawn to the 16-pixel grid, and its `.aseprite` source is committed with its export.
- Isometric or side views, turn-based combat, and painted or 3D art are ruled out until this is superseded.

## Signs the decision was wrong

- Players cannot tell bodies, structures or terrain apart in a top-down view, and a perspective with height would fix it.
- Real-time combat moves too fast for one life to be steered through it, and players ask to pause or take turns.
- 16 pixels per tile cannot show what a body or structure must show, such as its faction or stage, and art keeps being drawn at another size.
