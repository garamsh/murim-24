# ADR 0032: Keep a bounded ring of world events and read rumours from it

> Status: accepted
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The HUD's rumour log always says no rumour has come (`docs/architecture/godot-adapter.md`, open questions). Issue #129 asks the core to record the world's events, a village founded, raided or ruined, a sect fallen, a leader elected or won by duel, a birth or a death in the player's family, and the HUD to show the latest few by distance or relevance.

Facts that shaped the choices, from `core/world.hpp` and `core/world.cpp` at `948918f`:
- The core records no history. A village keeps who ruined it last (ADR 0015); nothing else keeps what happened.
- Nothing records who steers an army (ADR 0011), so the core does not know who the player is or which family is the player's.
- A sect stands while it has a living member, with no flag stored, and a fallen sect has no elder, so it admits no one and never stands again (ADR 0026).
- Every death, by a wound, by age or by hunger, ends with the dead leaving their rosters in `tick()`.
- GDScript holds no game rule (`docs/architecture/structure.md`).

Measured with this rule, over 100 years with days of five ticks, the generated region records 365, 423 and 452 events on seeds 24, 2026 and 7, about four a year: 23 to 29 villages founded, 3 to 7 raids sent, 13 to 54 ruins, at most one sect fallen, 1 to 3 leaders, 198 to 270 births and 95 to 103 deaths. At the end of each year, the lead of the first army with a living member heard 14.8, 15.1 and 16.3 of the 64 kept, on average.

Options weighed:

- **What is recorded.** An event is its sequence, counted over every event recorded, its tick, its cell, and what happened, one of: a village founded, by a faction; a party sent against a village, with its owner and the raiders' faction; a village ruined, with its owner and its destroyer; a sect fallen, with its faction; a faction's lead changing hands, by election or by duel; a person born, a child or a soul born again; a person dying.
  - The cell is the village's centre, the seat's centre, or where the person stood.
  - A lead renewed, an election choosing the leader there was, is not a change and is not recorded.
  - A sect's fall is noted when its last member leaves, where the dead leave their rosters and where the daily check removes members serving another faction. Nothing new is stored, since a fallen sect never stands again.
  - Rejected: a stored flag per sect for its fall. ADR 0026 stores none, and the moment its last member leaves is already a point the core passes.
  - Rejected: recording only births and deaths in the player's family. The core does not know the player, and the reader decides whose family matters.
- **How many are kept.** The latest `EVENT_CAPACITY`, 64, oldest first; the oldest gives way. About fifteen years of the region at the rate measured. The sequence tells a reader which entries are new since it last read.
  - Rejected: an unbounded log. It grows with the world, and with every save of it.
- **Who hears what.** `rumours(person)` returns the events kept that reach the person, newest first, derived on read:
  - those within `rumour_reach` cells of where it stands, along each axis, world data in `Rules`
  - wherever they happened: the births and deaths of its kin, its family, spouses, parents and children; and what befell its faction, a village of it founded, raided or ruined, or one raided or ruined by it, a sect of it fallen, or its lead changing hands
  - Rejected: the selection in GDScript. Who hears a rumour is a rule, and a rule written there exists outside the application.
  - Rejected: rumours spreading person to person by a rule of their own (`docs/concept.md` §Reputation and rumors). It needs reputation, which the core does not hold; this selection is what such a rule would replace.
- `events()` reads the ring itself. Both are part of the hash, and of a save.

## Decision

The core records each event that befalls a village, a sect, a faction's lead or a person's life, with its tick and cell, in a ring of the latest 64. `rumours` derives which of them reach a person: those within the world's rumour reach of where it stands, and those about its kin or its faction wherever they happened. The region's rumour reach is 32 cells.

## Consequences

- The HUD's rumour log reads `rumours` for the steered person and words each kind; it decides nothing about who hears what.
- An event older than the 64 latest is gone, so a rumour cannot be told years later.
- The determinism hash changes, since it folds the ring and the rumour reach, and a save must hold both.
- Each death and each change of hands costs one record; the daily check costs nothing more.
- A rule that needs history, such as reputation or a sect's grudge, has a record of the latest events to read, but no more than the ring holds.

## Signs the decision was wrong

- Births and deaths crowd the ring, so a village's ruin or a leader's fall is gone before the player hears of it, and the capacity or the kinds kept need revising.
- Players hear nothing for long stretches of play, at about four events a year of two hours, and the feed needs events of smaller grain or a wider reach.
- A rule comes to read history further back than the ring, and events need a store of their own.
