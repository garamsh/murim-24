# ADR 0022: Cap each village by its housing, food and storage

> Status: accepted; its free feeding from the village's stock, its cap from the whole harvest and its migration by the person superseded by ADR-0052, which sells rations from the stock, counts four fifths of the harvest and migrates households; its births scaled by headroom and stopping at the cap, and its single field yield, superseded by ADR-0063, under which births follow a household's provision and crowding, no rule stops at the cap, and yields are pack data by biome and season
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept caps population by housing, food and storage (`docs/concept.md` §Growing civilization). It proposes the cap as the smallest of housing, food supply divided by consumption per person, and storage. Growth should slow near the cap, and a shortage should bring starvation or migration.

Issue #76 asks for this as data, one rule for every village:
- The cap derived on read.
- Births (ADR 0021) reading it.
- Starvation and migration as rules on persons.
- The 500 bound kept.

PR #78 added granary and storehouse art and fields drawn in five states, for `world_art` to draw from the core.

Two facts shaped the choices:
- The templates' founders were exactly as many as their buildings housed. A cap of housing would have stopped every birth from the first day.
- Fields were presentation only, ground tiles in `world_art/villages.json`, so the core could neither harvest nor burn them.

Options weighed:

- **What a structure holds.** Per kind, in a table the region data gives: the people it houses, the food it stores, and the food each cell yields a harvest. The region turns it into a `Holding` per structure, which a `VillageSpec` carries.
  - Region values: a house 8, a hall 12, a granary 2,400 measures, a storehouse 3,600, and a field 600 a cell.
  - A building's founders stay as they were, 4 a house and 6 a hall, so a village starts at about half its housing.
  - Rejected: holdings on `Structure`, which every structure in the code would carry for no rule that reads it.
- **Fields.** A field is a structure of the `field` kind, with a footprint that does not block movement. A torch burns it through the one structure-damage path, and the burnt state is any stage below intact. Mending restores it as it restores a house. This supersedes ADR 0013's rule that every footprint blocks: whether one does is now per-kind data.
  - Rejected: a second disc check for fields beside the structure one, which would be a second rule.
  - Rejected: fields as blocking structures, which would wall villages in further than #70 already found.
- **The cap.** Derived on read, never stored, in people: the least of three terms.
  - The housing of the village's structures still standing, intact or damaged.
  - The yearly harvest of its intact fields over a person's yearly consumption.
  - The storage of its standing structures over the same.

  Without husbandry the cap is MAX_PEOPLE. Rejected: a cap stored and updated, which can disagree with the structures it came from.
- **Food.** `Husbandry` in `Life`: consumption a person a day, hunger days, and migration reach. Once a day:
  - On the first day of winter a village adds its intact fields' harvest to its stock, never above its storage.
  - It feeds everyone whose home it is, or, when it cannot feed them all, none of them.
  - A person unfed for the hunger days moves, by the seed, to a village within reach with room under its cap and food in store. With none, it dies.
  - A village starts with its storage full.
  - Rejected: feeding some and not others, which needs an order of precedence no rule gives yet.
- **Births.** The chance of a birth is scaled by the headroom of the child's village, (cap − population) / cap, so births slow near the cap and stop at it. The 500 bound stays.

## Decision

Each village holds food from its fields' harvests up to its storage and feeds its people from it daily. Its population cap is derived on read as the least of its standing housing, its intact fields' yearly harvest per person, and its storage per person, all from per-kind data. Births slow with a village's headroom and stop at its cap. A person unfed too long moves to a village with room and food, or dies. Fields are structures that block no movement and burn by the one damage rule, superseding ADR 0013's rule that every footprint blocks.

## Consequences

- With the game's data a village is capped at 50 and a hamlet at 20, so a region of six to eight villages holds 120 to 400 people under its caps, below the 500 bound.
- Under the game's marriage and birth odds, villages grow toward their caps over 30 years and level off below them. With every wife bearing as she may, they fill to their caps and never pass them; without husbandry the same run passes them.
- A burnt field yields nothing at harvest. A village whose stock then runs out sends its people to a neighbour with room, or they die.
- Raids now ruin fields, a granary and a storehouse with the houses. The #67 targets still hold, and `make bench`'s M1 scenario has no villages, so its cost is unchanged.
- The pinned determinism hash changed: every structure folds whether it blocks, and the hash folds each village's holdings and stock and each person's hunger.

## Signs the decision was wrong

- Feeding all or none makes whole villages flee at once in play, and a ration shared by some order is needed.
- Villages never reach their caps under the game's odds, so the cap never binds in play and the economy reads as decoration.
- A field that blocks no movement but burns like a building reads wrong, and fields need a state of their own beside the structure stages.
- Migration to whichever village within reach has room scatters families, and moving should follow kin.
