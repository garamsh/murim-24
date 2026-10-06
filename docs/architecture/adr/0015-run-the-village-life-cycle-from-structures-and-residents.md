# ADR 0015: Run the village life cycle from structures and residents

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept's village life cycle runs from prosperity through a raid to ruins, then survivors return or another faction moves in, and the village is rebuilt (`docs/concept.md` §Village life cycle). The owner can change with who rebuilds, and the memory of the destroyer remains. Issue #37 asks for:

- a state derived from structures and residents, never stored beside them, as ADR 0006 derives a structure's stage from its durability
- rebuilding through the same kind of rule that breaks
- rebuilders chosen by data, with no branch per faction

ADR 0013 keeps rubble impassable. Options weighed:

- **What a village is.** Data the world is created with: its structures and its resident army. The owner is the residents' faction. Rejected: an owner stored beside the residents, which could disagree with them.
- **The state.**
  - Ruins while every structure is rubble.
  - Otherwise raided while a living body hostile to the owner could damage one of its structures from where it stands.
  - Otherwise prosperous if every structure is intact, and rebuilding if one is not.
  - Rejected: raided while a hostile body stands inside the village's area. A raider burning houses from outside the area read as rebuilding.
  - Rejected: raided only while a structure is damaged. Residents mending between strikes made a besieged village read as prosperous.
  - Rejected: a stored state or a "fallen since" flag, which is the drift the issue rules out.
- **Rebuilding.** The strike rule with a signed structure effect: a negative amount restores durability up to the intact floor and never lowers a structure above it. A skill of range 0 lands on its user, so a mend works where the mender stands, with no hostile body to aim at. Rejected: a separate repair rule, and a target kind stored on the skill, each a second path beside the strike.
- **Whose effects a village takes.** While its residents live, damage only from factions hostile to the owner and repair only from the others. A Demonic village's own torches therefore spare it, and an empty village takes either. Rejected: every structure taking every effect, which made residents with a damaging skill of range 0 burn their own village.
- **Who rebuilds.** Each faction's skills are data: the Orthodox and Unorthodox guard and mend, and the Demonic Cult guards and torches. Residents take their faction's skills. The first army to restore a structure of a village with no living resident becomes its residents. Rejected: a rebuilder flag on factions, which the skills already express.
- **The destroyer.** The army, and its faction, whose strike brought the last standing structure to rubble, recorded then and kept through rebuilding. This record is history, not state, since no read of the structures could recover it.

## Decision

A village is its structures and resident army, given to the world as data. Its owner is its residents' faction, and its state is derived on each read from its structures' stages and from who could damage it. Rebuilding is the strike rule with a negative structure effect, a range-0 skill lands on its user, and a village with living residents takes damage only from its owner's enemies and repair only from the others. Factions' skills decide who rebuilds. An empty village becomes the rebuilder's, and the destroyer is recorded when the last structure falls.

## Consequences

- No state can disagree with the structures. A test compares the state with one worked out from the public API on every tick of a full cycle.
- A skill of range 0 now resolves on its user every cooldown. The M1 benchmark's idle armies do that work: on the development Mac, tick p99 was 1.920 ms before, and 2.421, 2.004 and 1.989 ms in three runs after, within the 4 ms budget.
- Structures outside any village take every effect, as before.
- Rubble stays impassable (ADR 0013): rebuilding changes durability and nothing about movement.
- Survivors rebuild their own village through their faction's mend. Another faction moves in only once no resident lives.

## Signs the decision was wrong

- A village reads as raided long after the fighting, because hostile armies idle within their torches' reach of it.
- Players expect a faction to take a village it has not rebuilt, by conquest alone, and ownership has to follow something other than residents.
- Range-0 skills multiply until resolving them on every user shows in the tick budget.
