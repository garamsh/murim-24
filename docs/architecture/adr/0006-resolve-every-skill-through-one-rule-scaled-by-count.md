# ADR 0006: Resolve every skill through one rule scaled by the agents its user stands for

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Attacks break objects and hit several targets at once, and NPCs attack exactly as the player does (`docs/concept.md`). ADR 0005 made an army a list of bodies, each one agent or an aggregate holding count and strength, with tiers that differ only in data. It left open how per-agent state survives a collapse. A battle away from the focus has to resolve by the same rule as one beside it, at the aggregate's coarser grain, so that the player arriving later finds what that rule produced. Destruction is per cell or object, through the stages intact, damaged, destroyed and rubble; debris and effects are presentation and are never stored. Options weighed:

- **What a skill is.** A record of data: range, radius, damage to bodies, damage to structures and cooldown. Its shape is the disc of that radius around its target, so a single-target strike is a disc of radius 0. Rejected: a skill kind per behaviour (melee, ranged, area), each with its own code, which is a branch per kind wherever skills are resolved.
- **Who resolves a skill.** One function for every body, so the player's body is an agent of an army of one. Rejected: separate rules for agents and aggregates, such as damage drawn from the aggregate's strength or a statistical exchange between far armies. Each is a second rule that drifts from the first, and the player would find a far battle's outcome made by rules they never meet.
- **Keeping the outcome the same in every tier.** A battle must have the same winner wherever the player stands, so the rule counts three things that differ between tiers, and the tier's data decides each:
  - Agents striking: the user's count multiplies the damage, so an aggregate of five strikes as hard as five agents.
  - Agents struck: a body takes the damage once for each of its agents the disc covers, the smaller of its count and the number of agents the disc's area holds at one agent per separation square. An agent is struck once, and an aggregate as often as a disc over its agents would strike them.
  - Uses per step: a body acting once per tier interval makes every use its cooldown allows in that step, counted from the later of its readiness and the step's start. A body acting every 16 ticks then uses a skill as often as one acting every tick.

  Rejected: one hit per body and one use per step, which made an area skill weaker and a quick skill slower far from the focus than near it, so the same battle could change winner with the focus. Also rejected: simulating a far aggregate's agents for combat, which gives up the aggregate's cost.
- **Choosing and aiming.** A body whose army is due this tick and whose cooldown has passed draws one of its army's skills from the world's random sequence. It aims at the nearest living hostile body within the skill's range, with the lowest index breaking ties. Every hostile body in the disc around that target takes the damage, and so does every structure whose cell centre lies in it, found through a spatial hash over structures as bodies are found through one over bodies. Damage is gathered from where bodies stood when the tick began and applied after every body has struck, so the order bodies strike in does not decide who strikes. Rejected: applying damage as each body strikes, which lets the army listed first kill before it is struck.
- **Hostility.** Factions are indices, and whether faction `a` attacks faction `b` is a matrix held as data. Rejected: hostility fixed in code per faction pair, and a player-only flag.
- **Per-agent state through a collapse.** Damage folds into count and strength. Strength falls by the damage and count falls in proportion, rounded up: a body with strength left keeps at least one agent, never holds more agents than units of strength, and has none once its strength is gone. A collapse sums both exactly, and an expansion divides them evenly, so wounds are shared out rather than kept per agent. The one other per-agent value, a cooldown, merges to the latest among the merged bodies. Dead bodies are dropped when the army regroups; an army with none left keeps one empty body as its lead. Rejected: keeping each agent's health through a collapse, which makes a far army as costly as a near one, as ADR 0005 already weighed for its totals.
- **Structures.** A structure is a cell and a durability, and its stage follows from durability by three floors held as data: the lowest durability of the intact, damaged and destroyed stages, below which it is rubble. Durability never falls below 0, so damage to a structure gives the same result in any order. Rejected: storing the stage beside durability, which can disagree with it, and a structure type per stage.

## Decision

A skill is data, and one rule resolves it for every body: draw a skill by the seed, aim at the nearest living hostile body in range, and damage every hostile body and structure in the disc around it. Damage is multiplied by the agents the user stands for, by the uses its cooldown allows in its tier's step, and, for each body struck, by that body's agents the disc covers. It is applied after every body has struck. Hostility between factions, the separation that sets an agent's footprint, and the floors of the structure stages are data. Damage folds into count and strength, which is the whole of the per-agent state a collapse keeps. A battle therefore resolves through the same rule in every tier, with the same winner near the focus and far from it, and its totals carry through expansion and collapse exactly.

## Consequences

- Adding a skill or faction, or tuning stages, changes data and no code.
- The player's body fights under the rule every NPC does.
- A far battle keeps a near battle's winner but not its exact totals. Near the focus, damage lands on the agents actually hit, so some die early and stop striking. Far from it, damage spreads over the aggregate, and its count falls in proportion. The tests hold the strength the winner keeps near and far to within a quarter of the start.
- An aggregate struck by a disc is treated as if all its agents stood packed around the target at the separation, whether or not the disc lies over them.
- An expansion heals wounds by spreading strength evenly; the totals are exact, but no agent keeps its own health.
- A dead army keeps one empty body, which still moves and is still in neighbour queries.
- Destroyed structures do not yet change passability.

## Signs the decision was wrong

- A skill needs behaviour that its data cannot express, and a branch on what kind of skill it is appears in the rule.
- A matchup the tests do not cover, started near the focus and far from it, ends with a different winner, or with the winner's strength apart by more than the tolerance the tests state.
- Packing every agent of an aggregate at the separation overstates how many an area skill hits, and wide area skills win far battles they lose near the focus.
- Combat rules start needing a single agent's state, such as a wound or a status, that count and strength cannot carry.
- Empty bodies of dead armies crowd the living out of cells or queries.
