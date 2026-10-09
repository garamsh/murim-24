# ADR 0046: Strike by shape at arm's length, and close on a foe in sight

> Status: accepted
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #183: playing `main` at `1d943f0`, the owner saw people attack from too far away. The owner then decided on 2026-10-09, in the issue's scope comment, that the basic attack becomes a frontal area attack in the way of Albion Online, that every skill declares its shape as data, and that attacks locking on one person are kept for rare special skills.

Facts that shaped the choices, from `core/` at `ca54aca`:
- Near arts and the army's guard strike reached 3 cells; the dart art 8. A skill was a disc of its radius around the nearest hostile person in range (ADR 0006), or around its user at range 0, and a cast landed the same disc at its target (ADR 0034).
- People keep a separation of one cell, a body's width. ADR 0037 holds a mover before a foe on the move within its reach, its longest range.
- The core keeps no facing (ADR 0034). The view draws each body facing the way it last moved.

Options weighed for the strike:

- **A shape per skill, resolved by one function with a branch by shape.** `Skill::shape` is `AREA`, `ARC`, `LINE`, `SELF` or `TARGETED`. A use goes toward a point: the nearest hostile person in range for the automatic strike, and the cast's target for a cast. The arc covers the points within the range ahead of the way to that point, within a quarter turn either side, the same "ahead" `aim` reads. The line covers the points within the radius of a segment the range long. `SELF` is a disc around the user. `TARGETED` hits the one person aimed at, and a cast of it at a point or itself is refused with `NO_TARGET`. Area is the default, so a skill given without a shape resolves as before. Chosen.
- **An arc's width as data.** No skill needs a width other than a half turn, and a half turn is the core's existing "ahead". Rejected until one does.
- **Facing as state of the core**, such as each person's last step. A person standing still and struck from behind would sweep the air, and the state would be hashed and saved for a value the strike can choose on its tick. Rejected: a strike faces the nearest foe in range, and a cast faces its target. ADR 0034's sign that a cone needs facing as state did not hold for a cone that faces its foe.
- **A line that stops at the first person hit.** A projectile's flight is a later shape if a skill needs one. Rejected for now: the dart's line pierces.

Options weighed for the reach, as data in `region.cpp`:
- **A cell and a half, `MELEE`.** A foe at the separation, straight or diagonal (1.41 cells), lies within it. The guard strike and each blade art's first skill sweep an arc this long; the blade arts' wider skills land at a point within it; the dart art keeps 8 cells. Chosen.
- **One cell.** A foe pushed just past the separation, or diagonal, falls out of reach. Rejected.

Short melee left fights unfinished. With the arc at a cell and a half and nothing else changed, `Region.ASkirmishOfTwentyASide` never ended on seed 5, nor `Region.ADuelEnds` on seeds 24 and 5, probed to 100,000 ticks. People walk to a fixed objective cell: the skirmish's survivors stood at their arrival rings 4 to 5 cells apart, and a duel's leader, standing, stepped aside for the challenger walking to its old cell. At 3 cells this was masked. At 2 cells a skirmish still stalled on seed 2026. Options weighed, decided by the PM on 2026-10-09:

- **Engage a foe in sight.** A person no driver steers, with a living foe within the rules' `sight` cells, stands to fight while a foe lies within its reach, and otherwise closes on the nearest on the one of the eight steps nearest the line to it, open only as ADR 0037's field opens a step: into a passable cell, and on a diagonal only where both cells beside it are passable. It keeps its objective when no foe is in sight or the step is not open. A steered body never engages: the player keeps its movement, and its automatic strike still swings at foes in reach. Chosen.
- **A field per chaser toward its foe.** It finds a way round a wall, at a field per person per tick. Rejected: a foe behind a wall within sight is left to the objective's field.
- **Keep melee at 3 cells.** Rejected by the issue.
- **Re-order the scenarios' sides at each other.** It hides the behaviour rather than fixing it. Rejected.

The region's sight is 8 cells. Measured with `make sweep`'s 50 seeds of 100 years, keeping every faction peopled, against `main`'s 48:
- 6 cells, the skirmish's starting gap: 46. At 6 cells the duel ended in 511, 834 and 497 ticks on seeds 24, 2026 and 5 (target 300 to 900) and the skirmish in 800, 522 and 753 (target 300 to 3,600).
- 4 cells: 48, but short of the 5 cells the skirmish's stalled survivors stood apart.
- 8 cells: 48, covering the skirmish's starting gap and every stall measured. Chosen. The seeds that fail move between the three values (11, 16, 29 and 48 at 6; 29 and 48 at 4; 27 and 48 at 8), so the gap reads as the sweep's noise rather than a trend in the sight.

## Decision

Every skill declares its shape as data, and one function resolves a use by its shape toward a point: the nearest hostile person in range for the automatic strike, the target for a cast. The region's basic strikes sweep a frontal arc at arm's length, a cell and a half; the dart's strike flies along a line; mending and torching are self areas; no skill of the region's is targeted. Each use on a tick is recorded with its user, skill, shape, origin and the point it went toward, for the view, neither hashed nor saved. A person no driver steers engages a foe within the rules' sight: it stands to fight one within its reach, and otherwise closes on the nearest by an open step. This supersedes ADR 0006's disc around the nearest target for every skill, ADR 0034's automatic strike landing on the nearest hostile person, ADR 0037's rejection of closing on the nearest foe and its rule that a standing foe holds no one, for people no driver steers, and ADR 0020's rejection of a pull toward hostiles.

## Consequences

Measured against `main` at `ca54aca` with the same scratch instrumentation in both trees, counting each strike of radius 0 and range at most 4 cells as melee and of range over 4 cells as far:

| Measure | `main` | This change |
|---|---|---|
| Skirmish of 20 a side, seeds 24, 2026, 5, 0 to 4, 6 and 7: mean ticks | 991.6 | 658.9 |
| Hits per melee strike in it | 1.000 | 1.717 (1,263 single, 1,149 several) |
| Striker to struck at a melee hit | 2.04 cells | 1.21 cells |
| The same skirmish with the Cult throwing darts: mean ticks | 1,270.6 | 271.0 |
| Hits per dart; striker to struck at a dart's hit | 1.000; 2.81 cells | 2.373; 4.18 cells |
| M1 scenario, 54,000 ticks: alive at the end; half its 500 dead by | 81; tick 8,339 | 342; never |
| `make sweep`, 50 seeds of 100 years: every faction peopled | 48 | 48 |
| Wars declared, peaces made | 51, 27 | 50, 25 |
| Deaths by a wound over the sweep | 2,629 | 2,845 |

- The M1 scenario's lines stand two cells apart across their barricade, out of arm's length, until the casters break it, so its battle no longer ends within half an hour of play. Its scenario is unchanged apart from the jab's range and shape; it gives no sight. `make bench` on the development Mac on 2026-10-09 timed a tick p99 of 0.427 ms against `main`'s 0.632 ms the same day, at 6.6 strikes a tick against 16.0, so the scenario no longer strikes as often as the worst case it is meant to be.
- A dart now pierces everyone along its line, so a band throwing darts ends a skirmish in a quarter of the time. That is a balance question for the arts' data, not the rule.
- `run_crossing`'s pinned hash is unchanged: its skills are areas and its rules give no sight, and both are folded only when set. The region's hashes change.
- A save holds each skill's shape and the rules' sight, so `SAVE_VERSION` rises to 8 and every earlier save is refused.
- The test of a band outnumbered at the walled village kept its band where it stands as the control that no one is struck; with people engaging, the residents now close on that band, so the control runs with no sight.
- The view (#184) draws each strike from `strikes()`; the adapter exposes neither the shape nor the record yet.

## Signs the decision was wrong

- Fights near villages draw in people who should have kept to their work, such as settlers on the march or residents chasing a lone passer-by away from their homes, enough that the owner names it.
- A chaser jams against a wall with its foe in sight beyond it, where a field would have found the way, often enough to show in play.
- The M1 scenario, striking less, stops bounding the tick: a battle in play costs more than it.
- A skill needs an arc narrower or wider than a half turn, or a projectile that stops at the first body, and the shapes grow a parameter per case.
- The player asks for the automatic strike to face where the body faces rather than the nearest foe.
