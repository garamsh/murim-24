# ADR 0036: Keep people's progress in the martial arts in a store of its own

> Status: accepted; its three ways a skill is learned superseded by ADR-0055, which adds synthesis
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #159 asks the HUD to announce each skill the player learns, by comprehension, a manual or its sect's teaching, and each realm it gains. It prefers events from the core to reads diffed by the HUD. ADR 0035 learns arts and gains realms in the core, and records neither.

Facts that shaped the choices, from `core/` at `0962bfe`:
- ADR 0032 keeps the latest 64 events of a village, a sect, a faction's lead or a person's life in one ring, sized at about fifteen years of the region, and derives `rumours` from it. Its first sign the decision was wrong is births and deaths crowding the ring so that a ruin or a leader's fall is gone before the player hears of it.
- How a skill was learned is known only where the core teaches it, in `comprehend`, `read_manuals` and `teach`. A reader of `person_skills` sees a skill appear, not how.
- The owner decided on 2026-10-08 that the on-screen rumour log goes away and rumours are heard by talking to NPCs, in the next milestone, so `rumours(person)` stays the read that matters.

Measured over seeds 24, 2026 and 7, each region lived 100 years with days of five ticks, from year 30 on, once the ring is full (`core/` at `335fcdc`, where lessons and realms shared the ring):
- the region records 310, 911 and 1,193 events of ADR 0032's kinds, and 368, 473 and 629 skills learned and realms gained
- the ring of 64 holding ADR 0032's kinds alone spans 21.5, 8.9 and 11.1 years on average
- shared with lessons and realms, the 64 span 8.8, 4.4 and 4.4 years. A shared ring spans as long as ADR 0032's kinds alone only at 128 or more places: 19.2, 9.3 and 9.7 years at 128, and 24.4, 11.9 and 12.7 at 160
- at most 2 lessons and realms are recorded in one tick, and at most 4 in any 16 ticks, the most one frame of the play scene runs at its fastest speed

Options weighed:

- **(a) A store of their own.** `progress()` keeps the latest `PROGRESS_CAPACITY` steps, oldest first, each its sequence, its tick, and what it was: a skill learned, with its person, art, the skill's index in the art and how, or a realm reached, with its person and rank. The event ring, `rumours` and `heard_by` stay as ADR 0032 has them.
  - Capacity 32: eight times the most measured in the 16 ticks between two of the HUD's reads, while a save holds at most 32 more records.
  - No cell: nothing reads where a person learned.
- **(b) The kinds in the shared ring, its capacity raised.** Rejected. Keeping ADR 0032's history takes 128 places or more at the rates measured, twice the records in every save and in every `rumours` read, which walks the whole ring. And `heard_by` would still need a branch keeping lessons and realms out of rumours, a special case on the one rule of who hears what.
- **(c) No record: the HUD diffs `person_skills` and `person_realm` each frame.** Rejected. A diff sees a skill appear but not how. Telling comprehension from a manual or teaching would need the HUD to know the rules: that a reading spends qi and teaches every skill of the art the realm allows, that comprehension rolls on a realm gained. GDScript holds no game rule.
- Steps are noted where the core already learns and cultivates: one for each skill learned, one for each realm reached. Recording one consumes nothing of the seed, so no rule's outcome changes.
- The store is part of the hash, folded only while it holds a step, and of a save, refused when a step names a person, art, skill or realm the world lacks.

## Decision

The core notes each skill a person learns, with how it learned it, and each realm it reaches, in a store of the latest 32, apart from ADR 0032's ring of events. The HUD announces the player's from it. Rumours are derived from the event ring alone, as before.

## Consequences

- ADR 0032's ring keeps its history: lessons and realms take none of its places.
- The HUD's notices follow the store's sequence, as the sound scene follows the events'. A reader that falls more than 32 steps behind misses the oldest.
- A save holds one more list, so `SAVE_VERSION` rises.
- A rule that wants a person's history of learning, such as a master's lineage, has the latest 32 steps of the region, not a person's whole record.

## Signs the decision was wrong

- Players miss notices because more than 32 steps are noted between two reads, as a region grows, and the capacity needs raising.
- Rumours come to need lessons and realms, a famed master's breakthrough heard across the region, and the two stores need a reader that merges them, or the kinds need to move into the event ring after all.
- A rule comes to read a person's whole history of learning, and progress needs to live on the person instead.
