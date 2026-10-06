# ADR 0006: Resolve every skill through one rule scaled by the agents its user stands for

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Attacks break objects and hit several targets at once, and NPCs attack exactly as the player does (`docs/concept.md`). ADR 0005 made an army a list of bodies, each one agent or an aggregate holding count and strength, with tiers that differ only in data. It left open how per-agent state survives a collapse. A battle away from the focus has to resolve by the same rule as one beside it, at the aggregate's coarser grain, so that the player arriving later finds what that rule produced. Destruction is per cell or object, through the stages intact, damaged, destroyed and rubble; debris and effects are presentation and are never stored. Options weighed:

- **What a skill is.** A record of data: range, radius, damage to bodies, damage to structures and cooldown. Its shape is the disc of that radius around its target, so a single-target strike is a disc of radius 0. Rejected: a skill kind per behaviour (melee, ranged, area), each with its own code, which is a branch per kind wherever skills are resolved.
- **Who resolves a skill.** One function for every body. Its user's count multiplies both damages, so an aggregate of five strikes as hard as five agents, and the player's body is an agent of an army of one. Rejected: separate rules for agents and aggregates, such as damage drawn from the aggregate's strength or a statistical exchange between far armies. Each is a second rule that drifts from the first, and the player would find a far battle's outcome made by rules they never meet.
- **Choosing and aiming.** A body whose army is due this tick and whose cooldown has passed draws one of its army's skills from the world's random sequence. It aims at the nearest living hostile body within the skill's range, with the lowest index breaking ties. Every hostile body in the disc around that target takes the damage, and so does every structure whose cell centre lies in it. Damage is gathered from where bodies stood when the tick began and applied after every body has struck, so the order bodies strike in does not decide who strikes. Rejected: applying damage as each body strikes, which lets the army listed first kill before it is struck.
- **Hostility.** Factions are indices, and whether faction `a` attacks faction `b` is a matrix held as data. Rejected: hostility fixed in code per faction pair, and a player-only flag.
- **Per-agent state through a collapse.** Damage folds into count and strength. Strength falls by the damage and count falls in proportion, rounded up: a body with strength left keeps at least one agent, never holds more agents than units of strength, and has none once its strength is gone. A collapse sums both exactly, and an expansion divides them evenly, so wounds are shared out rather than kept per agent. The one other per-agent value, a cooldown, merges to the latest among the merged bodies. Dead bodies are dropped when the army regroups; an army with none left keeps one empty body as its lead. Rejected: keeping each agent's health through a collapse, which makes a far army as costly as a near one, as ADR 0005 already weighed for its totals.
- **Structures.** A structure is a cell and a durability, and its stage follows from durability by three floors held as data: the lowest durability of the intact, damaged and destroyed stages, below which it is rubble. Durability never falls below 0, so damage to a structure gives the same result in any order. Rejected: storing the stage beside durability, which can disagree with it, and a structure type per stage.

## Decision

A skill is data, and one rule resolves it for every body: draw a skill by the seed, aim at the nearest living hostile body in range, and damage every hostile body and structure in the disc around it. Damage is multiplied by the agents the user stands for, and applied after every body has struck. Hostility between factions and the floors of the structure stages are data. Damage folds into count and strength, which is the whole of the per-agent state a collapse keeps, so a battle resolves through the same rule in every tier and its totals carry through expansion and collapse exactly.

## Consequences

- Adding a skill or faction, or tuning stages, changes data and no code.
- The player's body fights under the rule every NPC does.
- A far battle is coarser than a near one: an aggregate is one target, so an area skill that would have hit several of its agents hits it once. Its body acts only on its tier's ticks, so a cooldown shorter than the tier's interval is not reached between them.
- An expansion heals wounds by spreading strength evenly; the totals are exact, but no agent keeps its own health.
- Each strike checks every structure, so the cost of a tick grows with structures times strikes.
- A dead army keeps one empty body, which still moves and is still in neighbour queries.
- Destroyed structures do not yet change passability.

## Signs the decision was wrong

- A skill needs behaviour that its data cannot express, and a branch on what kind of skill it is appears in the rule.
- The same battle, started near the focus and far from it, ends with a different winner often enough for players to notice.
- Combat rules start needing a single agent's state, such as a wound or a status, that count and strength cannot carry.
- Profiles show the scan over structures dominating ticks with many strikes.
- Empty bodies of dead armies crowd the living out of cells or queries.
