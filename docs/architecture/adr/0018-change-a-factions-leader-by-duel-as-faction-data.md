# ADR 0018: Change a faction's leader by duel, as faction data

> Status: accepted
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept gives each faction its own way to change leader (`docs/concept.md` §Factions and the rules for gaining power):
- The Demonic Cult changes leader by challenge and duel. Only a member may challenge, the winner leads, and the old leader's allies become the winner's enemies.
- The Orthodox elect theirs.
- The Unorthodox buy or kill their way in.

Issue #41 asks for the duel only, as data on the faction, with no interface for the other two rules until a second rule arrives (`simplicity.md`). Its other requirements:
- The player challenges through the same order port as an NPC (ADR 0011).
- Combat is the one skill rule, per person (ADR 0017).

Options weighed:

- **The leader.** A person, by index, given per faction with the world (`Leadership`), so it survives as a person does. Rejected: a leader army, which leads as a count and loses the person ADR 0017 keeps.
- **The rule as data.** `Duel {members_only, yield_health, allies_turn_hostile}`, optional per faction: who may challenge, when the fight is decided, and what the win changes. A faction without one keeps its leader. Rejected:
  - A succession interface or a kind enum with an empty case per faction, which builds the election and the takeover before either is designed.
  - Fixed code for the Cult, which makes the next faction to duel a code change.
- **The challenge.** An optional third argument to `World::order`: the same call, for any army, whoever issues it. The challenger is the army's lead. Rejected:
  - A field in `Order`. Every brace-initialised order in the code would have to name it, since the build warns on a missing field.
  - A separate `challenge()` call beside `order()`, which is a second port for one conversation.
- **The fight.** The two duellists are hostile to each other alone, so the one skill rule resolves the duel, and nobody else joins in. It ends when either has no more than the yield health, 0 for the Cult, a fight to the death. The leader keeps the lead when both fall on one tick. Rejected: a dice roll or a comparison of health, which bypasses the combat the player sees.
- **The consequence.** Every faction at peace with the leader's faction, hostile in neither direction, turns hostile to it, which is to the new leader's faction. Hostility is per faction, so the concept's "enemies of the challenger" lands on the faction the challenger now leads. Rejected: hostility per person, which the world has no other use for yet.
- **Where it is described.** In `simulation.md`, since the tick and the combat rule resolve it. A responsibility document of its own waits for a second succession rule.

## Decision

A faction's leader is a person, and how the lead passes is optional data on the faction, the duel being the only rule. A challenge is an order naming the faction, the same for the player's body as for an NPC. The duel is fought by the one skill rule between the two alone until one yields. A winning challenger leads, and the leader's allies then turn hostile to the challenger's faction.

## Consequences

- The Cult's leader can change in play. A test does so from the player's body and from an NPC member, and refuses a non-member, with a lost duel as the control.
- Hostility can now change while the world runs, and the hash covers it, the leaders and the duels in progress.
- The election and the takeover each need their own data when they come; nothing in the core anticipates them.
- A leader dying outside a duel leaves the faction led by a dead person. No rule yet names an heir.

## Signs the decision was wrong

- The second succession rule cannot be written as data beside the duel, and a branch per faction appears.
- Duels need others to take part, seconds or guards, and the two-person hostility has to grow into something else.
- Faction-wide hostility after a duel reads wrong in play, because the old leader's allies would turn on the challenger alone, and hostility per person is needed.
