# ADR 0031: Let a person apply to a sect by the admission's thresholds

> Status: accepted
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0026 admits people to sects by a daily draw: a person meeting its faction's `Admission` thresholds joins, with the admission's chance a year, a sect drawn by the seed among those open to it. It rejected "joining as an order through the port, for the player's body", because a port to ask for entry waited for a UI that asks. Issue #129 is that UI: the player asks to join a sect.

Facts that shaped the choices, from `core/world.hpp` and `core/world.cpp` at `948918f`:
- `World::admit` checked the thresholds inline: a living person of no sect, with a home, of a faction with an `Admission`, old enough, with a talent limit and an attainment at the thresholds in one discipline, whose home lies within reach of a seat held by a sect of its faction with a living elder and room under its cap.
- A challenge is refused by one function, `challenge_refusal`, whose reasons `order` refuses on, so the rule is written once (ADR 0018).
- Membership is a field on the person, and nothing records who steers an army (ADR 0011, ADR 0026).
- Talent is hidden from the player (ADR 0019).

Options weighed:

- **An application through the port.** `apply(person, sect)` makes the person a member at once. `application_refusal(person, sect)` returns why it may not, an `ApplicationRefusal`, or `NONE`; `apply` refuses exactly when it names a reason. The daily admission calls the same assessment for every person and sect, so the thresholds are written once, with no branch for the player.
  - The chance and the draw among sects are not part of an application. They model how often a person happens to seek entry, and which sect it happens on; an applicant names its sect and is answered at once.
  - NPCs do not apply. The daily draw already admits them by the same thresholds.
- **The reasons**, the first that holds in this order: `DEAD`, `IN_A_SECT`, `NO_HOME`, `NOT_OF_FACTION` (of another faction, or one whose sects admit no one), `TOO_YOUNG`, `UNPROVEN`, `TOO_FAR`, `SEAT_LOST`, `NO_ELDER`, `FULL`.
  - `UNPROVEN` covers the talent and the attainment together: no discipline meets both. A refusal for talent alone would tell the player its hidden talent.
- Rejected: an application as an `Order`. An order is an army's march and strikes; membership is a person's, and an army may hold people of several sects.
- Rejected: an application answered at the next daily check. The player would wait up to a day, a minute of play, for an answer, and the check would still roll the chance.
- Rejected: an easier rule for the player, such as no thresholds. It is a player branch, which ADR 0019's one birth rule and ADR 0018's one challenge already avoid.

## Decision

A person applies to a sect through the port, the same for the player's body as for an NPC, and joins at once when the daily admission's thresholds hold for it and that sect. `application_refusal` names the first threshold that fails, talent and attainment as one reason, and `apply` refuses exactly then. This supersedes ADR 0026's rejection of joining through the port; its daily admission stands, and shares the assessment.

## Consequences

- A player who meets the thresholds joins the sect of its choice, where an NPC waits on the chance and the draw.
- A refusal tells the player which threshold failed but never whether its talent fell short.
- The thresholds are assessed in one function, so a new threshold, such as infamy or money (ADR 0026), refuses an application and the daily admission alike.
- An application changes the state between ticks, as an order does, so a log that replays a world's orders must carry its applications too, each with the tick it was made before.

## Signs the decision was wrong

- Players join a sect on their first day whenever their talent allows, so membership reads as free and the admission's chance, kept for NPCs, reads as a player advantage.
- Players read `UNPROVEN` as talent, so the merged reason hides nothing.
- A sect fills with applicants before NPCs are drawn, and NPC membership falls in play.
