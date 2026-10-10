# ADR 0066: Strike one chosen foe as the basic attack, and chase it on an attack intent

> Status: accepted
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #232. The owner decided on 2026-10-10, in the issue's comments, which are binding and replace its body:
- The basic attack becomes targeted, and the player gets target selection. This reverses #183 and ADR 0046's frontal arc as the basic strike, and its rule that targeting is kept for special skills.
- Area damage moves to the skills on Q to T, the arts.
- One rule holds for everyone: an NPC's basic attack is targeted too, at the nearest foe, as today.
- Selecting a target only marks it. Space sets an attack intent on the selected target, or on the nearest hostile person in sight when none is selected. The person then chases the target and strikes it when it is in reach and the strike is ready.
- The intent holds until the target dies or leaves sight, the player gives a move order, presses Esc or selects another target.
- The intent is input: the view passes it to the core as the steered person's order, and the core resolves the chase and the strikes.

Facts from `core/` at `641ea23`:
- The region's basic strike, the army's guard strike, swept an arc at arm's length, 1.5 cells, for 10 damage every 15 ticks. ADR 0046 measured 1.717 people hit per melee strike.
- `TARGETED` was a shape already, resolved by the one strike rule: it hits the person locked on alone, and only when that person is a foe.
- An order was an objective and the skills an army strikes with (ADR 0011).
- A steered body never engaged a foe in sight (ADR 0046).
- The view kept no target, and the core kept no facing (ADR 0034).

Options weighed for the basic strike:
- **The army's basic strike of shape `TARGETED`, aimed like any automatic strike: at the army's target while it is in range, else at the nearest foe in range.** Chosen. The arts keep their shapes, the blade arts' first skill its arc.
- **A targeted strike for the player and the arc for NPCs.** Rejected: the owner asked for one rule for everyone.

Options weighed for the intent:
- **The intent as the order's target, `Order::target`, saved and hashed with the order.** The order is already how a driver and the rules tell an army what to do (ADR 0011). A move order is an order without a target, so it ends the intent with no rule of its own. Chosen.
- **The intent kept in the view, re-sending a cast at the target each tick.** Rejected: the view would decide the chase, and a save would lose the intent.

Options weighed for when the intent ends:
- **The core ends it on the tick its target dies, stops being a foe of the army's lead, or lies beyond the rules' sight of every member, positions read as the tick began.** Chosen.
- **Only the view ends it.** Rejected: the core would chase a target the view no longer sees.

Options weighed for what an ended intent leaves the army doing:
- **It holds where its lead stands: the core sets its objective to that cell.** Chosen. The core decides it, as it decides the chase.
- **It walks back to the objective it chased from.** Rejected: after a kill the body would turn and walk away on its own.
- **The view orders a stop when it sees the intent end.** Rejected on PR #237's review: it is a rule in the view, and it overwrote a move order the player gave during the intent a tick later.

Retuning, with the basic strike hitting one person instead of 1.7. `make sweep`, seeds 0 to 49 for 100 years, every faction peopled, against `main` at `d89a52f`, which kept 48 (failing seeds 35 and 48), with 55 wars and 28 peaces:

| Guard strike | Every faction peopled | Seeds failing | Wars, peaces |
|---|---|---|---|
| 10 damage every 15 ticks | 47 | 11, 27, 48 | 54, 26 |
| 15 damage every 15 ticks | 48 | 35, 48 | 54, 27 |
| 17 damage every 15 ticks | 47 | 11, 29, 48 | 56, 27 |
| 10 damage every 9 ticks | 48 | 27, 48 | 49, 23 |

15 damage every 15 ticks was chosen. It keeps the seeds `main` keeps, and nearly its wars and peaces, at 1.5 times the arc's damage to one person against its 1.7 people.

## Decision

The basic strike, the army's skill, is targeted. An army may hold an attack intent, a target given with its order. Its members close on the target and aim every automatic strike at it, whoever steers the army, until the target dies, stops being a foe, or leaves every member's sight, or an order without a target replaces it. An intent the core ends leaves the army holding where its lead stands, its objective that cell; the view issues no stop of its own.

`attack_refusal` names why a person may not be the target: the army has no member, there is no such living person, the person is not a foe of the army's lead, or no member has it in sight. `order` refuses a target for the same reasons, and `foes_in_sight` lists a person's foes in sight, nearest first, for the view to cycle and to take when nothing is selected.

The region's guard strike does 15 damage. This supersedes the following parts of ADR 0046:
- the region's basic strikes sweeping a frontal arc, for the army's strike;
- no skill of the region's being targeted;
- a steered body never engaging, for a body holding an attack intent.

## Consequences

- Without an intent, the automatic strike stays for everyone, the steered body included. The owner's comments add the intent and remove nothing. With an intent, the strike aims only at the target, and when the target is out of range it does not swing at anyone else.
- `World.HashAfterTenThousandTicksIsTheSameOnEveryPlatform` is unchanged: its crossing strikes with areas and holds no intent, and a target is folded only when set. The region's worlds are not pinned, and they now fight otherwise.
- A save holds the order's target, so `SAVE_VERSION` rises to 10 and every earlier save is refused.
- The M1 scenario's lines strike one foe each too. `make bench` still finds 18.0 strikes a tick, above its floor of 16.
- A targeted hit finds its person through the crowd's slot table, where it had scanned the whole crowd. The scan would have cost every basic strike a pass over every living person.
- The view passes the intent as input: Space or the touch attack button sets it on the selection, or on the nearest foe in sight with none, and the view decides nothing about the chase. Selection by click, Tab or tap, the target frame, and talk moving to a click and C land in the same pull request.
- An intent's chase whose diagonal step a blocked corner shuts takes the open axis of that diagonal, the longer offset's first. Without it, a body inside a village stood still behind a house corner with its intent held. People engaging on their own keep ADR 0046's step unchanged, so `make sweep` is not moved by it.

## Signs the decision was wrong

- Battles in play read as people jostling to reach one foe each, where the arc let a line press a line, enough that the owner names it.
- Fights end so much later that a region's raids and duels miss their target times, or `make sweep` falls below 48 under data that kept it before.
- The player asks the steered body to stop striking on its own when it holds no intent, as in Albion, and the automatic strike for the steered body has to go.
- A chase on an intent jams against walls, where a field toward the target would have found the way (ADR 0046's rejected field per chaser).
