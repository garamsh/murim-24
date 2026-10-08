# ADR 0039: Let a person talk to another within a reach and hear the news it has heard

> Status: accepted
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #166 asks for the player to talk to the people of the world instead of reading a rumour log. The owner decided on 2026-10-08 that the on-screen rumour window goes away, and that rumours are heard only by talking to an NPC, who tells what it has heard. The conversation is the frame that buying from a merchant (#167) and the deeds of #175 plug into. The PM approved the mockup and six proposals on PR #180 (evidence/pr-180).

Facts that shaped the choices, from `core/` at `598e028`:
- `rumours(person)` derives the events that reach a person from ADR 0032's ring, so each person already knows different news. GDScript decides nothing about who heard what.
- `relation` is the one read of how two factions stand (ADR 0038), and #175's standing is to reach people through it.
- `Rules` holds the region's reaches as data: `separation` and `rumour_reach` (32 cells). Both are saved and hashed.
- People stand at least a cell apart (`separation`), so a person beside another is one or two cells from it along an axis.
- The pinned hash of the M1 crossing (`HashAfterTenThousandTicksIsTheSameOnEveryPlatform`) moves with any new fold, and ADR 0036 folds its store only while it holds a step, so a world without one keeps its hash.

Options weighed:

- **(a) The core decides who may talk and what is offered.** Chosen.
  - `Rules::talk_reach` holds the cells, along each axis, within which one person may talk to another. The region gives 2. It is validated like the other reaches, saved, and folded into the hash only when set.
  - `talk_refusal(person, other)` names the first reason the talk is refused, in order: either is dead, they are the same person, the other stands beyond the reach, or their factions are hostile or at war as `relation` reads them.
  - `offers(person)` lists what a person offers one who talks to it: today its news, `Offer::NEWS`, for every living person, and nothing once dead.
- **(b) The reach as a constant of the play scene, hostility read in GDScript.** Rejected. Who may talk to whom is a game rule, and the adapter and GDScript hold none. A deed of #175 that makes a person refuse would also have to land in GDScript.
- **(c) A menu per kind of NPC in the view.** Rejected by the issue. The view would branch for merchants, masters and villagers, and each new kind would add a branch. With `offers`, #167 adds `TRADE` for merchants and #175 its deeds in the core alone, and the view draws one row for each offer it is given.
- **(d) Hostility by `hostile` in either direction.** Rejected for `relation`. `relation` is HOSTILE when either faction is hostile to the other by stance, and AT_WAR at a war declared, which is `hostile` both ways, so the two agree today. `relation` stays the one read, as ADR 0038 asks of the dialogue, and a person-level standing from #175 is to change it in one place.

The view, approved on PR #180:
- Space talks to the person a prompt names. That is the nearest living person the core lets the player talk to or refuses only for hostility. A hostile one gets the refusal on its prompt and no key.
- The window stands at the foot of the room and the world runs on. It closes when `talk_refusal` stops being `NONE`: the person walks out of reach, dies, or its faction turns hostile.
- The HUD's rumour log goes in both layouts. Talking has no touch button while touch is deferred (scope, 2026-10-09).

## Decision

A person may talk to another living person within the rules' talk reach, 2 cells in the region, unless their factions are hostile or at war as `relation` reads them. The core names the refusal through `talk_refusal`, and lists what a person offers through `offers`, its news today. The player hears rumours only by talking, from the person talked to's own `rumours`. The HUD keeps no rumour log.

## Consequences

- Each person tells its own news: a person far from another, of another faction or kin, tells other rumours.
- A save holds the talk reach, so `SAVE_VERSION` rises to 7. A world with no talk reach keeps its hash, the M1 crossing's among them.
- An offer is added to the core and given a word and an action in the view. No view branches on the kind of person.
- Rumours no longer reach the player unasked. The news of a raid on its village waits until it talks to someone who heard it.

## Signs the decision was wrong

- People refuse or accept talk for reasons that are a person's rather than a faction's, such as a grudge or a debt from #175, and `relation` cannot carry them, so the refusal needs a standing between people.
- Players miss news they need, such as a raid on their own village, because no one near them is talked to, and some news needs to reach the player unasked again.
- Offers come to depend on the speaker as well as the listener, as a price that depends on standing would, and `offers(person)` needs a second person.
