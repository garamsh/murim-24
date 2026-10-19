# ADR 0085: Draw the gear people are born with at each family's lowest tier

> Status: proposed
> Date: 2026-10-19

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0050's starting loadouts give each calling its own tier: villagers cloth, sect members leather, sect founders steel, and soldiers lamellar and a helmet. Building it (#295, step 1(a)) and re-measuring ADR 0046's targets on its rules showed what those tiers do to raids (#295):
- A band of raiders is soldiers. In lamellar and a helmet they guard against 325 per mille of each blow, and the hamlet's villagers in cloth against 50.
- Raiders who outlast the villagers they fall on shorten a hamlet raid below its one-minute floor (`world-generation.md`) on 10 of seeds 0 to 19, the fastest 1,713 ticks, about 57 seconds. On `main` at `fb3954d3` none falls below it; the fastest is 1,868.
- With the armour gap closed, by dressing either side as the other for the measurement, the misses fall to 2 of 18.

A tier handed out at birth is also gear made from nothing. Better gear should come from production: smiths making it from materials (ADR 0052 step 5, ADR 0064). `simulation.md` §The principle rules out a hand-listed table standing in for that. The owner decided on 2026-10-19 that gear drawn at birth is the lowest tier of each family.

## Decision

Every item kind has a family and a tier within it, as pack data. A loadout names, for each slot, families with weights, or the art's weapon (`simulation.md` §Gear), and never a kind. A person born draws a family by weight and wears a kind of that family's lowest tier, drawing among several by the same counter-based draw. No calling is given a higher tier, so the starting tier is derived from the item data rather than listed per faction or role.

This supersedes ADR 0050's starting loadouts' tiers: sect founders' steel, and soldiers' lamellar where a lower armour is listed. The rest of ADR 0050 stands, its rule that sect members carry their art's weapon included.

## Consequences

- Soldiers start in leather and a helmet, sect founders with their art's weapon at its lowest tier, and villagers in cloth. Better tiers enter the world only once something makes them.
- A pack adds a better kind by giving it a higher tier, with no change to any loadout.
- Founders lose steel's fifth more damage and its edge of 1,200 until smiths make steel.
- Until production exists, every armed person in the base pack carries an iron weapon or darts, so fights differ by family and art, not by tier.

## Signs the decision was wrong

- Re-measured on these rules, a raid or duel still falls outside ADR 0046's targets, so the armour gap was not what moved them.
- A pack needs a calling to start above the lowest tier, such as a sect whose founders must wield its relic, and can say so only by adding a family of one kind.
