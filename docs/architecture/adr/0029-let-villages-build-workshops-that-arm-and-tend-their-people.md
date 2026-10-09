# ADR 0029: Let villages build workshops that arm and tend their people

> Status: accepted; its edge on the strike rule superseded by ADR-0050, under which a smithy makes and restores gear and weapons give the edge
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept says villages later build workshops such as a smithy (`docs/concept.md` §Growing civilization). It proposes that the smithy makes and mends weapons, and that the apothecary makes medicine and heals the wounded. Issue #118 asks for both workshops, whose art PR #109 drew. Its design bar has three parts:
- Each workshop's effect is per-kind data on `Holding` or on the strike rule, read by existing rules, with no workshop-specific branch where data can do it.
- Growth decides when to build one by the same scarcity rule, extended by data, in villages past a data-given size.
- At least two alternatives are weighed.

Facts that shaped the choices, from `core/` at `e21d2dd`:
- The core has no items or equipment.
- Health only falls: nothing in the core heals a wound.
- Growth lays the plan that most raises a village's scarcest holding among housing, food and storage, counted in people (ADR 0023). What a structure holds is per-kind data, `Holding`, summed over the structures still standing, intact or damaged (ADR 0022).
- The cooldown paces every skill: strikes, mends and torches. Every use that hits something practises the striker's best discipline (ADR 0019), and a sect's elders are its members of the highest attainment (ADR 0028).

Options weighed:

- **What a workshop holds.** `Holding` gains four terms. Two are capacities in people: `arms`, the people a smithy arms, and `tends`, the people an apothecary tends. Two are qualities: `edge`, per mille added to the body damage of a strike, and `healing`, health restored a day.
  - Capacities add up over a village's standing structures, as housing does. Qualities take the greatest among them, so a second smithy arms more people without sharpening further.
  - Rejected: summing qualities as well. Two smithies would double a village's edge, and bounding the sum would need a clamp or a limit on how many structures a village holds.
- **When a workshop serves.** A village's edge and healing reach the people whose home it is only while what it arms, or tends, covers them all. That is `self_reliance`, the people a village serves with no workshop, plus the arms, or tends, of its standing workshops. Everything is derived on read, with no new state.
  - Rejected: an effect whatever the capacity, as first proposed. The PM's review found that a second smithy, which growth builds once a village outgrows its arms, would then change nothing.
- **The smithy's effect.** A strike's body damage is multiplied by (1,000 + edge) / 1,000, the edge of the striker's home. A skill of no body damage still does none, so a mend never wounds. Structure damage is unchanged. A person with no home strikes as before.
  - Rejected: shorter cooldowns. A smithy would then speed mending and burning as well as fighting, and with more uses, practice, attainment and who become elders.
  - Rejected: lowering the damage residents take. That is armour, and the concept says weapons.
- **The apothecary's effect.** Once a day, each living person whose home tends it recovers its home's healing, up to `newborn_health`, the health a newborn starts with, which is the region's full health. No one's health is lowered.
  - Rejected: healing as a skill through the strike rule, a negative body damage on people not hostile, as repair is on structures. The effect would ride the residents' faction skills, not the building, so a standing apothecary would change nothing. The strike rule's hostility test would also have to invert for bodies.
  - Rejected: lowering damage taken, or lengthening lifespans. Neither heals the wounded, and without healing no wound in the core ever closes.
- **When growth builds one.** Building's holdings in people grow from housing, food and storage to those three, then arms, then tends. Arms is `self_reliance` plus what the standing smithies arm, and tends the same for apothecaries. The scarcest holding still decides, the earlier among equals. So a village whose other holdings all exceed `self_reliance` builds a smithy, then an apothecary, and one at or below it builds neither. `self_reliance` is a term of `Growth`, by default as many as a `std::uint32_t` holds, so growth that names no workshop builds as before.
  - Rejected: a trigger of its own, "a village of at least so many people with no standing smithy builds one". It is a second building rule beside scarcity, with a test for each kind.
  - Rejected: counting workshops in the cap. A burnt smithy starves no one, so capping population by it reads wrong.
- **What changes in data.**
  - `StructureKind` gains the same four terms.
  - The region adds a smithy of 3 x 2 cells that arms 40 people at an edge of 250, a quarter harder, and an apothecary of 4 x 2 cells that tends 40 and heals 12 a day, a tenth of a person's 120.
  - `self_reliance` is 20, a hamlet's cap, so hamlets build neither and villages, capped at 50, build both. Growth builds both after a house, a field and a granary.
  - `world_art` draws both kinds from their sheets, and the adapter needs no new port, since `structure_kind` already names a structure's kind.

## Decision

A workshop is a structure whose per-kind `Holding` gives capacities, the people it arms or tends, and qualities, an edge per mille on body damage and healing a day. While a village's self-reliance and its standing workshops' capacity cover the people whose home it is, those people strike with the greatest edge among its standing structures and recover the greatest healing daily, up to a newborn's health. Growth counts arms and tends among the holdings its scarcity rule compares, after housing, food and storage, so a village whose other holdings exceed its self-reliance builds a smithy and then an apothecary, raised by repair like any building.

## Consequences

- Over 100 years with days of five ticks, read at the end of each decade, seeds 2026, 24 and 7 had laid their first smithy by years 40, 40 and 30, and raised their first apothecary by year 80 each. Raising is slow: a laid workshop stood destroyed or damaged for a decade or more first.
- `make sweep` over seeds 0 to 49 for 100 years gives every faction peopled on 45 seeds, failing on seeds 18, 20, 23, 28 and 42, and the living at least doubled on all 50. That matches `e21d2dd` seed for seed. The mean living at the end goes from 179.2 to 183.6, and the mean villages from 18.1 to 18.5.
- A raid that leaves a village's workshops destroyed or rubble takes away its edge and its healing until they are raised again.
- Wounds now close in a village with a standing apothecary that tends all its people. Elsewhere they still never close.
- A village growing past its workshops' capacity loses their effect until growth builds another, which it does when arms or tends is next its scarcest holding.
- Full health is `newborn_health` for everyone. A per-person maximum waits for a rule that needs one.
- The pinned determinism hash is unchanged. Its world has no villages, and the hash folds the four new terms of each village's holdings.

## Signs the decision was wrong

- Villages lose their workshops' effect for decades as they outgrow them, because growth builds for housing, food and storage first. The edge then reads as decoration, and coverage needs to be a share instead of all or nothing.
- Players expect a person's strength to show its weapon, and an edge from its home reads wrong when a raider far from home strikes as hard as one beside the smithy.
- Healing up to a newborn's health makes a strong person no harder to wound than a child, and a per-person maximum is needed.
- Workshops delay the houses, fields and granaries a village needs to grow, so growth over a century slows measurably.
