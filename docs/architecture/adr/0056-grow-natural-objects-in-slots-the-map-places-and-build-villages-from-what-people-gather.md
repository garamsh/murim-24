# ADR 0056: Grow natural objects in slots the map places, and build villages from what people gather

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-09, in issue #187, that the world feeds and builds itself, WorldBox-style:
- Villages are larger by default (also #169).
- NPCs move, gather and hunt on their own, and can found new villages.
- **Trees and stones are world objects,** and NPCs build villages from them, automatically. Construction stays NPC-only.
- **Terrain is a biome,** and the trees and fruit on it grow naturally. Today's forest is ground on which tree objects grow, spread and can be felled.
- **Villages grow like WorldBox,** with people and resources: they expand and found new villages.

The issue's comments add two inputs:
- **The PM, from #181.** Silver piles up with no sinks, so the economy should give it some.
- **The owner, 2026-10-09.** Villages grow centre first, with houses placed outward into suitable ground, roads where people walk, building tiers upgraded with materials, and fields, docks and quarries around them. #169 owns the layout, and the growth rules here are the ones that give villages their shapes.

Facts from `main` at `aff1434`, and from the open PR #215, on which this branch is stacked:
- **Growth today** (ADR 0023). Every 30 days, a village at a fifth of its cap lays a plan as rubble within 8 cells of its centre. Residents raise it by the repair rule, 5 durability every 30 ticks against 10,000, so one builder raises a structure in 60,000 ticks, about 33 days. Building costs nothing else.
- **Food** (ADR 0022). Food comes from fields into the village's stock. #215's ADR 0052 makes the granary sell it, paying its takings to field workers, and counts four fifths of the harvest in the cap.
- **The map** (ADR 0047). One hand-made map of 4,096 cells, 16,384 areas of 32 cells. Its build tool places the initial natural objects, so they come from the map and not the seed. #187 is asked to lower ADR 0041's bound of 256 objects an area, which would allow 4.2 million, or to store far areas' objects as counts, keeping saved object state within 8 MiB.
- **Daily rules** are untiered (ADR 0042), and are spread over the day's ticks (ADR 0048).
- **Items and holders** (#215).
  - ADR 0050 defines materials as item kinds that stack, and tools that lose one durability a working day.
  - ADR 0051 holds every item with one holder, accounts at warehouses among them, and a market's stock is ADR 0052's.
  - Earners earn by wages someone pays.
- **What is not built.** There are no animals yet (#171), and no trades (#170).

Decided by the PM on PR #220's review, 2026-10-10:
- **Extending, not superseding.** ADRs 0051 and 0052 are unmerged, and ADR 0052 already defers gathering to #187, so this ADR extends them.
- **The village's account** is accepted as a new kind of account, extending ADR 0051.
- **Slot kinds compiled into the map** from the pack's affinities are accepted.
- **Proposals in pack data**, which the sweep checks: the gatherers' reach of 32 cells, the cap of a third of earners, building before field work, and fruit and berries sold to the granary and herbs to the apothecary.
- **The player gathers but never builds,** as the issue gives it.

Options weighed:

- **Natural objects as slots.** The map's build tool gives each area up to 64 object slots, as part of the compiled map:
  - each slot is a cell within the area and a kind, 2 bytes, chosen from the area's biomes by the kinds' affinities with the tool's fixed salt
  - the map's 16,384 areas hold at most 1,048,576 slots, 2 MiB on disk
  - at run time each slot holds 2 bytes, 2 MiB in all, saved and hashed:
    - a stage of 3 bits: empty, sapling, growing, grown, stump or harvested
    - a timer of 13 bits: the days the slot has spent in its stage, up to 8,191

  Since the kinds' affinities are compiled into the map, a mod that changes natural-object kinds needs the map rebuilt, which changes the map's identity and so the pack's (ADR 0049).

  Chosen. Positions cost nothing at run time, the same map gives the same forests in every game, and ADR 0047's "objects come from the map" holds. Objects block nothing, so felling one rebuilds no path.
  - Rejected: objects as records with positions, at 8 bytes, which is 8 MiB at this count and grows with spread.
  - Rejected: counts per area for far areas, re-instantiated near the player. A stump felled far away would come back when the player arrives, and two forms of one object need two rules.
- **Kinds.** Kinds are pack data (ADR 0049): a name, the biomes it grows on with a weight each, the days from sapling to grown, a chance a day of spreading, the days a stump takes to regrow, and what gathering yields. The base pack's:

  | Kind | Biomes | Grows in | Yield | Stump regrows in |
  |---|---|---|---|---|
  | Tree | forest 10, plain 2, hills 3 | 240 days, 2 years | 4 wood | 120 days, a year |
  | Fruit tree | plain 3, forest 2 | 360 days, 3 years | 6 fruit, each season but winter, tree kept | never felled |
  | Bush | plain 4, hills 2 | 120 days, a year | 3 berries, bush kept | never felled |
  | Rock | hills 6 | at the map's build | 3 stone | 240 days, 2 years |
  | Herb patch | forest 3, hills 2 | 30 days, a season | 1 herb, ADR 0040's ware | 30 days, a season |

  A year is 4 seasons of 30 days, 120 days (ADR 0019). Rocks lie on hills only. The map's mountains are blocked footing (ADR 0047), so no slot lies on them.

  Fruit and berries are food, measures of it, sold to the granary. Herbs go to the apothecary as its material. Herbs were "later" in the issue. They are kept because a herb patch is one more row of kind data under the same rules, and ADR 0040's herb is already a ware.
- **Growth and spread.** Every 8 days, each area's slots advance by 8 days at once, on the area's own tick of the 8-day cycle, by area index, so a tick touches about 1 area of 64 slots. Each slot's timer rises by 8, then:
  - a sapling past half its kind's growing days becomes growing, and a growing slot past them becomes grown
  - a stump past its kind's regrowing days becomes a sapling
  - a harvested slot becomes grown again at the next season's first day
  - an empty slot becomes a sapling with its kind's chance for every 8 days, per mille, for each grown slot of the same kind in its area

  A stage change resets the timer, so growth is deterministic.
  - **The draw.** Spread is the only draw. It comes from `slot_stream`, a stream of its own, counter-based: each draw is a hash of the world's seed, the area's index, the cycle's number and the slot's index. So no other rule's draws shift it, and no order of visiting matters.

  So forests creep back into cleared ground near forest and stay cleared far from it.
  - Rejected: spreading to any cell. Slots are fixed, so spread is only refilling, which keeps the bound and needs no search for free ground.
- **Gathering.** `gather` is an order through the port for a person within a cell of a grown slot, the player's body included. It takes 15 seconds, 450 ticks, then yields the slot's stacks into the gatherer's inventory (ADR 0051) and sets the slot to stump, or for fruit, to harvested until next season. It is refused by its first reason as data: dead, not near, not grown, no room. The player may gather; only NPCs build.
- **Who gathers, and why.** A village keeps a stock of materials in its own account. It is a new kind of account, beside ADR 0051's, at the structure the pack marks as its village's store: a warehouse kind with lots, such as the base pack's storehouse. Each kind of material has a target, which is the cost of the next plan its growth would lay, plus a reserve.
  - Each day, earners of the village with no other work (ADR 0052's earning order) are assigned to gather, nearest first by index, while a material is short of its target: one gatherer for each 10 of the shortfall, at most a third of the earners.
  - Builders are at most a third of the earners too, and gatherers and builders together at most half, so field work never falls below half the earners.
  - A gatherer walks to the nearest grown slot of the kind within 32 cells of the village's centre, gathers, and walks back to deposit its stacks into the village's account.
  - **Pay.** Gatherers and builders are paid a day's wage, the mean daily share of the village's field workers, by the faction owning the village, from its treasury, as works (ADR 0052). A faction that cannot pay builds nothing that day, and its villagers work the fields.
  - Material above twice its target is sold to the village's market, or exported at the floor (ADR 0052), into the treasury.
  - **Unlike the shared store ADR 0051 rejected.** ADR 0051 rejected one store per village with no accounts, because every deposit would belong to everyone. This account holds only what the village itself owns: materials gathered for its building, paid for by its faction. Households' and others' accounts are untouched. Deposits to it pay no keeper's fee.
  - **Before the village has a store.** A village without a standing store structure keeps its account at its first structure that houses people, its hall or first house, as a yard of 16 lots. The account moves to the store once one stands. Destroying the structure that holds it scatters it as ADR 0051 scatters a warehouse.
  - Rejected: gathered materials as the gatherer's own, sold to a market for silver. Building would then wait on a village buying from its own people, two trades for one task.
- **Arts and debris (#203).** ADR 0057, designed after this ADR, may change slots by an art:
  - felling sets a grown slot to stump
  - a crater or cut over a slot's cell sets it to empty, to regrow only by spread

  Either drops the slot's yield, logs as wood and rock as stone, as piles (ADR 0051). Debris of a breached structure drops as stone piles too. Such piles are gatherable input: a gatherer assigned for a material first picks up piles of it within its 32 cells, by ADR 0051's pick-up rule, before it gathers from a slot, and deposits them into its village's account. So the owner's logs and stones from #203 feed building.
- **Hunting** waits for animals (#171). It will be gathering from a slot that moves, and the yield, meat and hide, is pack data like any other.
- **Building from materials.** Each structure kind's cost in materials is pack data: in the base pack, a house 40 wood and 10 stone, a hall 80 and 30, a granary 60 and 10, a storehouse 50 and 20, a smithy 40 and 40, a fence 4 wood a cell, and a field nothing.
  - Growth's plan (ADR 0023) is laid as rubble as before, once the village's account holds its cost. Builders carry the materials from the storehouse to the site, where they are spent.
  - The repair rule raises a structure only as far as the share of its cost delivered: half the materials allow half its durability.
  - A village lacking its plan's materials gathers instead of building. The next plan the scarcity rule picks waits until the account holds it.
  - Rejected: building that spends from the account at once, with nothing carried. People would never be seen at work, and the owner asked for villages built from what they bring.
- **Shapes, with #169.** Growth lays a plan on the free buildable cell nearest the village's centre, as ADR 0023 does, so villages grow centre first and outward. #169 decides layouts, roads where people walk, tiers upgraded with materials, and docks and quarries. Those use the costs and the gathering decided here.
- **Founding.** A village founding another, by ADR 0023's rule, sends its settlers with the first house's cost carried in their inventories, taken from its account. The new village gathers for itself from then on. A village without that cost does not found.
- **Fields and granaries** stay. Farming feeds a village, and foraging adds fruit, berries and herbs to the granary and the apothecary.
- **Tiers.** Daily rules are untiered, so the slots grow and assignments are made everywhere.
  - In live and stepped areas, gatherers and builders walk and work, tick by tick.
  - In a far area, a village's gathering is settled in aggregate once a day: each assigned gatherer yields one gathering's stacks from the nearest grown slots of the kind within 32 cells, in slot order. Its building raises each site by a day of its builders' repair. Wages and accounts are the same.
- **Cost.**
  - **Slots:** about 1 area of 64 slots a tick.
  - **Gathering:** a walk on ADR 0043's area fields toward the slot's cell. Fields are shared by area and target, so gatherers of one village going to one grove share one field.
  - **Far aggregation:** a slot scan within 32 cells for each gatherer, once a day.
  - **The daily assignment:** for each village, a pass over its earners.

  Each is a share of ADR 0048's daily spread. The implementing pull request measures a tick's p99 on the low profile with every village gathering.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 64 slots an area, placed by the build tool
  - at most 64 kinds of natural object
  - at most 16 biomes in a kind's affinities
  - at most 8 stacks in a kind's yield
  - at most 8 materials in a structure kind's cost
- **At the bound and the live tier.** At 50,000 people with #216's live tier of 5,000:
  - Gatherers are at most a third of a village's earners, so at most about 1,000 of the live tier walk to slots at once. A walk costs what any mover's step costs, on fields shared by area and target.
  - A gathering is one order and one slot write.
  - The daily assignment passes over every village's earners once a day, about 28 people a tick spread over 1,800 ticks.
  - A far village's aggregate scans at most 9 areas of 64 slots for each gatherer, once a day.
- **Memory and saves.** 2 MiB of slots on disk with the map, 2 MiB of stages and timers in each save, and a few lots for each village's account: 4 MiB, within the 8 MiB ADR 0047 set.
- **Money.** Gathering and building wages move silver from treasuries to households, and surplus materials sold or exported bring it back. Imports of materials a village cannot gather, such as stone on a plain, are a sink under ADR 0052's balance.
- **A worked example.** One base-pack village's first 10 years: a starting town of 4 households, 14 people, with 2 houses and 4 field cells, on plain beside a forest and a rocky hill within 32 cells.
  1. **Year 0.** The cap is the least of housing, 16, and four fifths of 2,400 measures over 120 a person, also 16. Five earners work the fields. The account starts with a granary's cost, as the map's starting places do.
  2. **Year 1.** Births bring 15 people, past a fifth of the cap, so growth chooses a house, housing being scarcest among equals. It needs 40 wood and 10 stone. Two gatherers fell about 2 trees a day each, 8 wood a day between them, and one breaks 1 rock a day, 3 stone. The cost is in hand in 5 days. Three builders carry it and raise the house in about 11 days, so it stands 16 days after it was planned. The cap rises to 24 on housing, and food holds it at 16.
  3. **Year 2.** Food is now scarcest, so growth lays 4 field cells, at no material cost, and the cap reaches 24 at the next harvest.
  4. **Years 3 to 6.** A house, then a granary (60 wood), then a storehouse (50 wood, 20 stone). About 50 trees are felled within 16 cells and stumps regrow in a year, so the forest's edge moves out about 10 cells and stays.
  5. **Year 8.** 28 people. A smithy is laid past self-reliance (ADR 0029). It needs 40 stone, 14 days of one rock-breaker, since the hill's rocks regrow in two years.
  6. **Year 10.** 32 people press a cap of 32. The account holds a house's cost twice over, so the village founds: 6 settlers carry 40 wood and 10 stone to land 30 cells away, and three of them raise its first house there in about 11 days.
- **Supersessions.**
  - ADR 0023's plans raised by the repair rule alone, which now raise only as far as their materials are delivered, and its founding with nothing carried.
  - ADR 0047's bound of 256 objects an area, as ADR 0041 set it, now 64 slots an area placed by the build tool.
- **Extensions of the stacked #215.** ADR 0051's kinds of account gain the village's, and ADR 0052's earning order gains gathering and building paid from the treasury, which ADR 0052 defers to #187.

The implementation, in order, each a pull request with its acceptance:
1. **Slots and kinds.** The build tool places slots on the test map, with stages, growth, spread and regrowth.
   - Acceptance: the same map giving the same slots, spread refilling only near grown slots with the same draws whatever the visiting order, stumps regrowing on their days, and 2 MiB of stages and timers saved and hashed.
2. **Gathering.** The `gather` order and its refusals, and the village's account and its targets.
   - Acceptance: an NPC gathering a short material and depositing it, and a player gathering.
   - Acceptance: after #215's economy, wages paid from the treasury.
3. **Building from materials.** Costs, carrying and raising only as far as delivered, and founding with materials.
   - Acceptance: a test of each.
   - Acceptance: `make sweep` showing villages still growing over 100 years, with the worked example's first house within 30 days of its plan on seed 2026.
4. **Far aggregation.**
   - Acceptance: a far village growing at the rate a live one does within ADR 0042's margins.
   - Acceptance: the low profile's p99 measured.

## Decision

Natural objects are slots the map's build tool places, up to 64 an area, each a cell and a kind from the area's biomes. At run time each holds 2 bytes: a stage, empty, sapling, growing, grown, stump or harvested, and a timer of days in it. Kinds are pack data with biomes, growth, spread, regrowth and yields. Slots advance by 8 days at once, an area on each tick of an 8-day cycle, and refill from grown neighbours of their kind by draws from a counter-based stream of their own. Objects block nothing.

`gather` is an order for anyone within a cell of a grown slot, taking 15 seconds and yielding stacks to the inventory. Villages keep a materials account at their storehouse with targets from their next plan. Earners with no other work are assigned to gather what is short, and are paid with builders from the owning faction's treasury. Surplus is sold.

Structures cost materials as pack data, carried to the site, and the repair rule raises a structure only as far as its materials are delivered. Founding carries a first house's cost. Far villages gather and build in aggregate by the same accounts. Hunting waits for #171, and layouts for #169.

This supersedes ADR 0023's plans raised by repair alone and its founding with nothing carried, and ADR 0047's bound of 256 objects an area. In #215 it extends ADR 0051's accounts with the village's, and ADR 0052's earning order with gathering and building.

## Consequences

- Villages visibly work: people walk to the forest and back, and building waits on wood and stone, so growth slows where materials are far and quickens beside a forest.
- Forests recede around old villages and creep back around ruins.
- Treasuries pay for growth as well as war, so a faction at war builds less.
- Saves grow by 2 MiB of stages and timers, and the map by 2 MiB of slots.
- The sweep's growth figures change, since building now waits on materials, and growth data is retuned in step 3.

## Signs the decision was wrong

- 64 slots an area leaves forests visibly sparse, so the owner asks for more, which costs slots everywhere.
- Villages on open plain never build, for want of stone within 32 cells, so a market import of stone becomes every plain village's only way.
- Gatherers walking to far groves fill the tick in live areas, so gathering needs a shorter reach or a field shared more widely.
- Treasuries at war pay no builders, and villages stop growing for decades, which reads as stagnation, not hardship.
