# ADR 0080: Offer only uncrowded attack slots, and reach them round the ring

> Status: proposed
> Date: 2026-10-14

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0068 surrounds a foe with attack slots and has each person engaging it close on a slot. Built as written in its second step (#331, Part 2 of #282, at `ec7fd839`), the slots fail in two ways, both measured.

- **A crowd from one side locks.** Eight attackers from one side of a lone foe are given slots by distance, and the farthest get the far side. Neighbouring slots on the basic strike's ring lie 269 units apart under a separation of 256, so people crossing to the far side meet people closing on the next slot head-on. Under ADR 0020 each is ahead of the other, so both push back. From tick 80 the crowd sways with a period of two ticks and nobody stands on a slot.
- **A line battle stacks.** Letting two people seeking slots pass each other ends the lock, but in the M1 battle most slots of a front-line foe lie behind it, inside its own line. People then walk into the enemy line, and the two lines become one thin band of bodies on top of one another.

The PM had the options below prototyped on #331's branch. Each was measured three ways:
- on the M1 battle over ticks 90 to 600, the mean share of living people with a neighbour inside half the separation, and strikes a tick;
- on the eight attackers, how many stand within a quarter cell of their slot at tick 300.

The measurements used GCC 16 at `-O2` with a driver over the core library. Each option ran with no pass between seekers, with seekers of the same foe passing each other, and with any two seekers passing.

The options measured:
- **(a) Only the near half of the ring.** A newcomer takes only a slot on the side of the foe facing it.
- **(b) Round the ring.** A seeker first walks to the ring at its own bearing, then round it one slot at a time to its slot.
- **(c) Only uncrowded slots.** A slot is offered only while no one other than the foe or a person engaging it stands within the separation of it; within half the separation was also tried.

| Option | No pass | Same foe passes | Any pass |
|---|---|---|---|
| No tokens, as on `main` | 4.2%, 18.0 | — | — |
| Slots as ADR 0068 | 6.2%, 11.7, 0 of 8 | 64.6%, 27.5, 8 of 8 | 79.1%, 29.3, 8 of 8 |
| (a) near half | 5.4%, 11.2, 1 of 8 | 52.8%, 22.8, 4 of 8 | 71.1%, 26.6, 4 of 8 |
| (b) round the ring | 3.5%, 10.9, 0 of 8 | 61.6%, 25.4, 8 of 8 | 80.5%, 29.0, 8 of 8 |
| (c) uncrowded, half the separation | 1.8%, 10.5, 0 of 8 | 19.4%, 15.7, 8 of 8 | 23.3%, 18.1, 8 of 8 |
| (c) uncrowded, the separation | 4.1%, 15.5, 0 of 8 | 5.3%, 15.8, 8 of 8 | 6.3%, 16.0, 8 of 8 |
| (c) at the separation with (b) | 4.3%, 16.6, 0 of 8 | 4.8%, 16.1, 8 of 8 | 6.6%, 16.1, 8 of 8 |

Each cell gives the stacked share, then strikes a tick, then the attackers on their slots. ADR 0068 counts strikes a tick in the M1 bench falling below 16 as a sign it was wrong.

What the table shows:
- Without a pass between seekers, no option forms the ring.
- Without (c), every option that forms the ring stacks the line battle.
- (a) leaves half the attackers off a ring.
- A shrinking slot count was also asked for, and (c) is that: a ring offers fewer slots the more of its points are crowded.

## Decision

A slot is offered only on a passable cell where nobody stands within the separation, other than the foe and the people engaging it. A held slot that becomes crowded is freed, like one that becomes blocked, and its holder takes the nearest free slot still offered on the same tick.

A person engaging a foe that offers it no free slot does as on `main` (ADR 0046):
- it closes on the foe itself and stands to fight once the foe is within its reach;
- it is no seeker, so it gives way as anyone does;
- it keeps the same foe, neither waiting on a ring nor turning to another;
- it takes a slot on a later tick, once one is offered.

This is how the measured prototype behaved.

A seeker off the ring's band, its radius plus half a cell, closes on the slot nearest its own bearing. Once on the band, it walks round the ring one slot at a time toward its own slot, the shorter way.

Two people seeking slots round the same foe do not give way to each other. People seeking slots round different foes, and anyone else, keep ADR 0020's give-way.

This amends ADR 0068's offer of every passable slot and its straight approach to a slot. It replaces #331's pass between any two seekers with a pass between seekers of the same foe, amending ADR 0020 for them alone.

## Consequences

- **Line battles:** the lines meet as they did before slots. Slots open only on a foe's free side, so a ring forms round a foe in the open and not round one inside its own line; those engaging a foe with no slot to offer fight it from where they reach it, as on `main`.
- **Who strikes:** strikes a tick in the M1 battle fall from 18.0 to about 16. The measured 16.1 sits on ADR 0068's floor, so #331 runs the M1 bench and reports it against the floor, and step 3's token table is tuned with that margin in view.
- **Cost:** offering a slot reads the spatial hash within the separation round it, at most 16 reads for each person engaging, each tick. #331's bench measures the cost against the live scenario's tick.
- **Stability:** a slot can be freed by a passer-by standing on it, so people near a crowded foe change slots more often. Nothing remembers the change, and the slot is retaken by distance as ADR 0068 assigns it.
- **What stays:**
  - ADR 0068's ring geometry and its ordering by distance then index;
  - its tokens, waiting ring, freeing and lease;
  - its steps 3 and 4.

## Signs the decision was wrong

- The M1 bench, run with ADR 0068's default token table set in the scenario as #331 sets it, measures fewer than 16 strikes a tick while tokens still gate no strike, that is, before step 3 lands.
- Foes inside a line never draw a ring however long a fight lasts, because their own allies keep every slot crowded.
- People round one foe change slots often enough to show as dithering on the captures.
- The stacked share in a line battle rises above about twice `main`'s once tokens and the waiting ring are added.
