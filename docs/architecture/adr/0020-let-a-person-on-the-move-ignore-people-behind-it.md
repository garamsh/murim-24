# ADR 0020: Let a person on the move ignore people behind it

> Status: accepted; its rejection of a pull toward hostiles superseded by ADR-0046, under which people no driver steers close on a foe in sight; its give-way between two people seeking attack slots round the same foe amended by ADR-0080, under which they do not give way to each other
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #70: a Demonic band ordered to the centre of the 26-resident village never reached its defenders, so no strike landed and the raid never ended. Under ADR 0017's movement rule:
- A person steps away from anyone closer than the separation, and otherwise along its pull. A step goes by the sign of twice the push plus the pull, so any push outweighs the one-step pull.
- People on the move give way to one another, and to nobody standing still.

The village is walled by houses, barrels and a fence, with gaps one or two cells wide. The issue suspected the arrival ring. Tracing seed 24 showed instead that the band never got through the west gap:
- Each raider moved every tick, but the band cycled through the same cells for thousands of ticks.
- The raiders behind pushed the front raider sideways into the walls (a push of (23, 11) against a pull of (1, 0)), so nobody went through.
- This is ADR 0017's sign of a wrong decision: crowds deadlocking in play.

Options weighed, each to hold the marching and standing spacing (#52), the crowd exit (#47) and the hamlet raids (#67):

- **Ignore the people behind.** A person on the move gives no way to anyone behind it along its own way: anyone the pull points away from. The front of a queue walks on. Everyone behind still keeps the separation from the person ahead, so a column keeps its spacing and does not collapse. It is the rule movers already follow toward people standing still, extended to a direction. Chosen.
- **Weight the pull against the pushes.** ADR 0017 rejected this at every weight it tried, 1, 32, 64 and 256: a column closed up, or a person stuck at a crowd's edge.
- **Shrink the arrival ring, or close to skill range on hostiles.** The issue suggested these. They would not help: the band stalls at the gap, short of its ring. A pull toward hostiles would also be a second steering rule for raids.
- **Widen the village's gaps.** Data, but every walled template meets the same queue, and the mockup sets the layout.

## Decision

A person on the move gives way to no one standing still and to no one behind it along its way. It steps away from everyone else closer than the separation, and otherwise along its pull. This supersedes ADR 0017's rule that people on the move give way to one another; the rest of its movement rule stands.

## Consequences

- A band queues through a one-cell gap and reaches the defenders. A test shows the 26-resident village, raided by a band scaled from the hamlet's, in ruins within a target twice the hamlet's on three seeds. A band the village outnumbers now strikes its residents down. Kept where it stands, as a control, the same band strikes no one.
- The pinned determinism hash changed, since every marching crowd steps differently.
- People pushed from behind no longer move ahead of their pull, so a crowd squeezed from behind holds its place rather than spilling forward.

## Signs the decision was wrong

- A column on the move closes up, or its rear overruns its front.
- Two groups crossing at an angle hold each other up, because each counts the other as ahead.
- A crowd pushed from behind into a wall stays overlapped, because nobody in it gives way to the people pressing it.
