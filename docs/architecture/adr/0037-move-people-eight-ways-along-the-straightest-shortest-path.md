# ADR 0037: Move people eight ways along the straightest shortest path

> Status: accepted
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #162: playing `main` at 0962bfe, the owner saw people move only north, south, east and west, and not smoothly. Measured on that commit:
- The flow field (ADR 0005) is a breadth-first search over four directions. Its step on a cell is the first of the four it was reached from, so a diagonal objective is reached by an L: every step along one axis, then every step along the other.
- `play.tscn`'s joystick orders the person `STRIDE` cells the way it points, again from each new cell. Pushed down and right from village 7 of seed 2026, the person walked 120 pixels down and none right in 4 seconds, since each order's first step is the same axis.
- A step is the sign of twice the push plus the pull on each axis, at `SPEED`, 16 units a tick, on each. A push across the pull already gave diagonal steps, about 1.41 times as fast as straight ones.

Options weighed, each to keep ADR 0020's queue through a one-cell gap, ADR 0017's spacing and every objective reachable as ADR 0013 requires:

- **Eight ways, along the straightest shortest path.** The field searches eight directions at a cost of 5 straight and 7 diagonal, 7/5 within 1% of the square root of two. A diagonal step is taken only where both cells beside it are passable. A cell's step is, of those starting a shortest path, the one whose cell lies nearest the line to the objective, a diagonal first among equals. Every diagonal step moves `DIAGONAL_SPEED`, 11 units, along each axis. Chosen.
- **Free direction (any-angle).** A person steers toward the objective's centre while it can see it, and along the field otherwise. Every walk over open ground would be straight. It needs a line of sight per cell for every field, rebuilt for every army whenever growth lays a blocking structure; a pull that is a vector, not a step; and the push, the step aside and "behind along its way" of ADRs 0017 and 0020 restated for a vector. Rejected as more than the issue needs, since it accepts eight ways.
- **Eight ways with Chebyshev distance.** Every step costs one, so going straight and going diagonal tie. A walk along a row may zigzag, and the arrival ring becomes a square. Rejected.
- **Diagonals past one blocked cell.** Paths round a single corner get shorter. A step moves one axis at a time and enters only cells the field reaches, so a body crossing that corner is refused on one axis and slides along the other, a hitch where the field promised a straight diagonal. Rejected.

## Decision

The flow field is 8-connected. A straight step costs 5 and a diagonal step 7, and a diagonal step is open only where both cells beside it are passable. Each cell's step is the one, of those beginning a shortest path, whose cell lies nearest the straight line to the objective, a diagonal first among equals. Its distance is the path's cost over 5, in cells rounded down. A diagonal step moves `DIAGONAL_SPEED`, 11 units, along each axis, about `SPEED` over the square root of two, so a person moves at about `SPEED` whichever way it goes. This supersedes ADR 0005's breadth-first search over 4-connected cells; its flow field per army stays, as ADR 0017 keeps it, and so do ADR 0017's and ADR 0020's rules of who gives way.

## Consequences

- A person ordered along a diagonal over open ground walks it on the line between the cells' centres. Toward a cell on no diagonal, it walks within a cell of the line, for 12 cells along one axis and 5 along the other. Tests show both, and that no step passes beside a blocked cell, with open ground as a control.
- Straight walks keep `SPEED`. A diagonal covers 15.6 units a tick, 2.8% short of it, so 9.5 cells each way take 222 ticks where 9.5 straight take 152.
- Which cells a person can reach is unchanged: a diagonal step is open only where its two straight steps are.
- The joystick's eight ways and the right button's cell are walked as given. `play_test` shows both walking a diagonal, along each axis to within a cell.
- People on the move pass round one another more often. Two lines of skirmishers bound for each other's starts now slip past and stand out of range. The region's skirmish test binds both lines for the cell between them, and they fall within its target time. The crossing scenario's army of two, which now passes the others unharmed, has three.
- People closing on an objective from either side of it come within half a cell of each other, which straight paths never did, until they stand and spread out.
- Building a field costs a priority queue in place of a queue, and a second pass to choose steps. `make bench` holds the tick within its budget.
- The pinned determinism hash changed.

## Signs the decision was wrong

- People closing on an objective stay overlapped after they stand, or a band queuing through a gap jams where it used to queue.
- A walk on open ground to a cell on no diagonal visibly bends, enough that the owner names it as a staircase again.
- A field rebuilt for every army when growth lays a blocking structure costs a tick past its budget.
