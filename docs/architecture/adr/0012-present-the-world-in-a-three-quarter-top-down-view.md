# ADR 0012: Present the world in a three-quarter top-down view on the 16-pixel grid, with Korean-first UI text

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0008 chose an orthogonal top-down view, one core cell to one 16-pixel tile, real-time combat and pixel art from Aseprite. The target mockup made to it (`art/mockup/target_screen.png`, #35) was approved by the user. It draws more than a straight-down view: ground and roofs are seen from above, but the fronts of buildings and the bodies standing on the ground are drawn upright, and they rise into the cells above them. The village house in that mockup covers 3 x 3 cells, while the core gives every structure a single cell (`core/region.cpp`, `region_data()`). The game's UI text had no language set. Options weighed:

- **Three-quarter top-down.** Upright facades and bodies on the square grid, drawn back to front. The grid the core works on stays the grid the view draws, and a body or a building reads by its front, as in the approved mockup.
- **Straight-down orthogonal, as ADR 0008 states it.** Rejected: the approved mockup is not drawn that way, and roofs and heads seen only from above hide what tells a faction or a building's stage apart.
- **Isometric.** Rejected for the reasons ADR 0008 gives: a diamond grid the core does not have, and every asset drawn for that projection.
- **Structure footprints of several cells, set by data.** The approved house is kept at 3 x 3 cells, and the core gains the footprint. Rejected: drawing every structure inside one cell, which would undo the approved house; and keeping 3 x 3 art over one-cell structures, which would draw buildings over their neighbours.
- **Korean-first UI text, English supported**, as the user set it. Rejected: English only or Korean only, each of which leaves out the players of the other language.

## Decision

The world is shown in a three-quarter top-down view on the 16-pixel grid, one core cell to one tile. Ground and roofs are seen from above; facades and standing bodies are drawn upright, rise above their footprint and are drawn in order of the footprint's bottom edge. A structure occupies a footprint of width x height cells given by data, 1 x 1 being the smallest case. Combat runs in real time on the core's tick, and art is pixel art on that grid, made in Aseprite and exported by its CLI. UI text is written in Korean first, and English is supported.

## Consequences

- The view maps cells to tiles by a scale alone, as before, and adds one rule: everything standing is sorted by the bottom edge of its footprint before it is drawn.
- A sprite may be taller than its footprint. A body's frame is 16 x 24 over a 16 x 16 cell; the house's frame holds its 48 x 48 footprint and the eaves past it (`docs/art-direction.md`).
- The core does not have footprints yet. Until it does, a structure is one cell in the simulation and 3 x 3 in the art, and wiring the house into the view waits on that change.
- Every piece of UI text needs a Korean string and an English one, and the HUD needs a bitmap font that carries Hangul. The mockup's 5-pixel capital font draws English and numerals only.
- Straight-down art, isometric or side views, turn-based combat, and painted or 3D art are ruled out until this is superseded.

## Signs the decision was wrong

- Upright facades hide the bodies behind them, and players lose sight of fights in villages unless buildings are cut away or faded.
- Structures of several cells make the core's area skills, passability or flow fields costly enough to break the tick budget, and buildings are drawn over single cells to avoid it.
- Korean text does not fit the HUD at the native resolution, and panels keep being widened or text abbreviated to make room.
