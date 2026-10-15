# ADR 0081: Give people trades from their village's buildings, and move them between home, work and the square by the hour

> Status: proposed
> Date: 2026-10-15

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-08, in #170, that people should live on their own: go about a day's work, move without being ordered, and be peopled by trades, not only fighters. Seven trades were named: merchant, physician, smith, farmer, innkeeper, beggar and monk.

Today a person moves only by its army's order. A village's residents are one army bound for the village's centre, so once they arrive they stand. In the prototype below, with no schedule, 0% of a village's residents are moving at any hour.

What this design builds on:
- **Places.** ADR 0076 gives every structure a role: square (hall, well, workshops), home (houses, granaries, storehouses) or edge (fields, fences). Each village keeps a square clear round its centre, sized by its tier.
- **Buildings and needs.** ADR 0022 and ADR 0029 give each structure kind a `Holding`: housing, storage, a field's yield, pupils, and the people a smithy arms or an apothecary tends.
- **Who acts on their own.** A driver marks the armies it steers (ADR 0038), and the rules that decide for people on their own, among them the routines #170 adds by ADR 0038's own words, decide nothing for a steered army's members. The PM's note on #170, from #165, makes that mark, and not "an army of one", the test for daily routines.
- **What overrides a walk.** Engagement (ADR 0046), attack slots (ADR 0068, amended by ADR 0080) and, once built, the appraisal of danger (ADR 0071) each give a person a pull of its own.
- **Who reads a trade.** `sells` reads a person's best discipline until people have trades (ADR 0040; `simulation.md` §Open questions). Merchants (#167) and dialogue (#166) will read the trade too.
- **The looks.** The PM approved the trade looks on #170 on 2026-10-15: `art/trades-mockup` at `65b7bed0`, with the merchant's shoulder pole at `746d0157`. A trade is palette and prop over the shared body. Each has an idle frame, a four-frame walk and a two-frame work motion, and the monk alone changes the head.

The base pack's structure kinds (`packs/wuxia/data/structure_kinds.json` at `64a61d5`) hold a smithy, an apothecary, fields, houses and a hall. They hold no inn, temple or market, and no sect in `sects.json` is a monastery.

### Options weighed for what a trade is

- **(a) A code enum of the seven trades.** Rejected: the core holds no theme words (ADR 0049), and a pack must be able to add or rename trades.
- **(b) Pack data: a table of trades.** Each names where it works, how many openings a workplace or a village's residents give, and its schedule by hour. Chosen.

### Options weighed for assigning trades

- **(a) Drawn at birth.** Rejected: needs change as buildings rise and fall, and a child's trade would be fixed before its village has the workplace.
- **(b) Derived on every read, in roster order.** Rejected: one death shifts everyone after it, so a smith becomes a farmer mid-day and the view's looks flicker.
- **(c) A daily rule that fills openings and keeps held trades.** Chosen. A person keeps its trade while its workplace stands and its village is its home, so trades change only when the village changes.

### Options weighed for moving people about the village

- **(a) A flow field per person, over the whole map.** Rejected: on the live scenario's 256 x 256 map each field is 65,536 cells, for each of 2,000 villagers.
- **(b) An order per resident.** Rejected: an order belongs to an army, and the residents are one army; splitting them into armies of one breaks the rules that take a village's residents as one army: feeding, migration, raids and who keeps the village.
- **(c) A path field per place, windowed round the village and kept until its layout changes.** Chosen and prototyped below. A village has few places: its houses, its workplaces and its square. Their fields are shared by everyone going there.
- **(d) A straight step toward the place, with no field.** Rejected: houses stand in rows with lanes between, so a straight step walks into a wall.

### The prototype

The prototype is a scratch copy of `main` at `64a61d5`, not in the repository; its source is posted on the pull request that proposes this ADR. It has five parts:
- **Trades.** The prototype hard-codes six trades and assigns them daily by its village's workplaces. A smithy or an apothecary opens 2 places for its trade and a field 1 for a farmer. In a village of 20 or more, a square structure that is no workshop opens 1 for an innkeeper, and every 30 residents open 1 for a merchant. Every adult left over becomes a beggar.
- **Schedule.** By hour, a day being 1,800 ticks: home overnight; farmers at their field from 06 to 18, smiths and physicians at work from 07 to 19, merchants and beggars on the square through the day; then the square in the evening, and home.
- **Places and spots.** Each person keeps its own spot: within a cell of its house's or workplace's door, or anywhere on the square. Awake, it wanders within a cell of that spot, moving every 90 ticks.
- **Paths.** Each place has a path field over 49 x 49 cells round it, built when first needed and kept until a structure is laid. A person follows the field until it is within 3 cells, then steps straight to its spot.
- **Moving.** A person counts as moving at a tick when it stands at least half a cell from where it stood 30 ticks earlier, so jostling in place does not count.

It ran three scenarios. Each lived one day to settle, then was measured over the next 1,800 ticks:
- **500 in two villages.** Two villages of 250 residents on a 128 x 128 map. Each was laid out by the driver with 44 houses, a smithy, an apothecary, a hall and 12 fields: 118 structures in all.
- **The live scenario.** `core/live_scenario.cpp`'s 5,000 people, its eight villages of 250 laid out the same way: 460 structures. Its battles kill about 1,200 people over the day the measurement first lives.
- **The generated region.** Seeds 0 to 2, at their first year: 20 to 22 living.

| Scenario | Moving, day mean, before | After | After at 08 | After at 18 | After, 00 to 05 |
|---|---|---|---|---|---|
| 500 in two villages | 0.0% | 7.6% | 28.2% | 19.4% | 3.6–6.0% |
| live scenario | 5.9% | 9.5% | 19.7% | 15.2% | 6.1–8.5% |
| region, seeds 0–2 | 0.0% | 15.1–17.7% | — | — | — |

- **Before** is `main`'s movement: in the live scenario, the battles and the road bands.
- **The night's share** is people jostling at their doors. A house's residents share a door, and enough of them are pushed half a cell in a second to count.
- **Crowding cost the most.** The prototype first put the square's visitors within 2 cells of the centre. At noon over a hundred people stood on about 15 cells, and each visited more neighbours for the separation. Spreading them over the whole square cut the tick's median at 500 from 0.69 ms to 0.31 ms, against 0.30 ms with no schedule, at a load of 20 to 40 from other work, so the figures are a ratio and not the cost. Where people stand is therefore part of the cost, and the decision below spreads them.

### How it fits

- **ADR 0076.** A trade's workplace is a structure kind; it carries the role 0076 gave that kind, so a smith walks to the square's edge and a farmer to the village's edge. Home is a home-role structure that houses people, and the square is the square 0076 keeps clear. A village growing into a larger tier keeps its square where it was laid, so the square's spots grow with it.
- **ADR 0071.** The schedule is the weakest pull. A person whose stance is not close follows its stance: backing off, fleeing home or to its lead, regrouping or holding. A villager appraises as before, whether at home or at work.
- **ADR 0068 and ADR 0080.** A person engaging a foe closes on it or on its slot, as now; the schedule resumes once it stops engaging. A person walking an errand is on the move under ADR 0020, so it gives way to no one standing or behind it. Once at its spot it stands, so movers pass it.
- **ADR 0046.** Engagement at home stays bounded by the village's claim. Someone at work on a field beyond the claim engages as anyone at home does, only a foe inside the claim, so a farmer does not chase a passing band.
- **ADR 0040.** `sells` reads the trade in place of the best discipline. Which wares a trade sells is pack data on the trade.

### Cost

Pending: the tick cost at 500 and at 5,000 waits for a quiet window on the development Mac. The shares above do not depend on timing. The decision names the budget the measurement must meet.

## Decision

**Trades are pack data.** A pack lists trades. Each names:
- **its workplace:** a structure kind, or the square;
- **its openings:** how many people each standing structure of that kind takes, or one for every so many residents for a trade that works on the square;
- **its schedule:** bands of hours, each naming a place: home, work or the square;
- **the wares it sells**, read by `sells`.

The pack also names:
- the working age;
- the trade given to an adult for whom no opening is left;
- a schedule for those without a trade, such as children.

The base pack takes the seven trades the owner named:
- the smith works at the smithy and the physician at the apothecary;
- the farmer works at a field;
- the merchant and the beggar work on the square, until ADR 0052's market kind gives the merchant a stall;
- the innkeeper works at the hall, until a pack adds an inn;
- the monk opens no place until a pack adds a kind it works at: the base pack has no temple and no monastery sect.

**Assignment is one daily rule.** Once a day, each village fills its openings from its residents of working age, in trade order as the pack lists them, then by residents' index:
- a person keeps its trade while the village is its home and a standing structure of its workplace kind remains;
- the rest take the first open trade, and an adult left over takes the pack's default.

The rule is the same for every resident: the player's kin, NPCs and founders alike. It reads no driver and no faction.

**Who follows a schedule.** A person follows its trade's schedule when all of these hold:
- it is a member of a village's residents;
- its army is not steered;
- its army's order is bound for the village's centre and holds no attack intent.

A raiding party, settlers, any army on the march and the player's body follow their orders.

**Where a person goes.** The hour is the tick within the day, scaled to 24. In each band:
- **home** is a house of the village, chosen for its household: households fill the village's housing structures in index order, up to each one's housing;
- **work** is a standing structure of its trade's workplace kind, the farmer's field in the same index order;
- **the square** is the village's square, as its tier keeps it.

Each person keeps its own spot at the place:
- within a cell of the structure's door, the passable cell beside it nearest the village's centre;
- for the square, anywhere on its cells, drawn from a counter-based stream of the world's seed and the person. Visitors are spread over the whole square, never gathered on its centre.

Awake, a person wanders within a cell of its spot, to a new one every 90 ticks, drawn from the same kind of stream with the period added. In a band naming home outside the day's waking hours it stands at its spot, and is at home.

**How it walks.** Each place has a path field over its village's window, the village's vicinity and a margin. It is built when first needed and kept until the village's layout changes. A person follows the field until it is within 3 cells of its spot, then steps straight to the spot, and stands there.

**What wins.** The schedule's pull is the weakest. Engaging a foe, closing on an attack slot and any stance but close, once ADR 0071 is built, each replace it. While walking an errand a person is on the move under ADR 0020; standing at its spot, it is standing.

**Reads.** `trade_of(person)` reads a person's trade. `activity(person)` reads whether it is walking, working at its workplace in a work band, at home, or idle. The adapter carries both, so the view draws the trade's look and its work motion.

**The view.** The look of each trade is palette and prop over the shared body, drawn from the approved mockup by data, with the monk's shaved head.

**Determinism, saving and the hash.**
- The hour comes from the tick count, every spot from a counter-based stream, and assignment runs in village and then residents' index order, so nothing draws from the world's shared random stream.
- A person's trade is saved, and `SAVE_VERSION` rises. A load refuses a trade the pack does not list.
- The hash folds a person's trade only while it has one, so a world with no trades data keeps its pinned hashes.
- Homes, places, spots and path fields are derived from what is saved, and neither saved nor hashed.

**The budget.** The schedule must hold:
- the M1 scenario's 4 ms p99, which has no village residents and so must be unchanged;
- the live scenario's target of 4 ms p99 at 5,000, issue #216's, as ADR 0053 makes it binding.

Within those, its own cost at 5,000 must stay within 0.2 ms at the median, 5% of the budget, measured in the bench as the live scenario with its villages laid out.

**Steps.**
1. Trades as pack data, the daily assignment, `trade_of`, saving and the hash.
2. The schedule, the path fields, `activity`, and the live scenario's villages laid out so the bench measures it.
3. The adapter's reads and the view's looks from the mockup, with captures of a village at morning and evening.

## Consequences

- **People live on their own.** A village's people walk to work in the morning and to the square in the evening. In the prototype at 500, 28% are moving at 08, against none today.
- **A rule change.** Every pinned hash of a world with trades data changes, and `make sweep` must keep ci.md's 200-seed bar. The sweep lives the region at 5 ticks a day, so a band of hours spans a tick or less there. Its movement is a few steps a tick, not a day's walk.
- **`sells` reads the trade.** That closes `simulation.md`'s open question on who sells once people have trades.
- **Memory.** One byte a person for its trade, saved. The path fields are kept per place; their size and the window's are pending with the cost.
- **Monks wait for content.** The base pack opens no place for a monk until a pack adds a temple kind or a monastery sect. The looks exist, but no monk appears in the base pack's villages.
- **The tiers.** ADR 0042, proposed, runs the daily rules in every tier. The schedule moves people only where they are simulated in full, and a person whose area becomes live starts at its spot for the hour.

## Signs the decision was wrong

- Doors jam: a household cannot reach its house through a crowd. Spots then need spreading along the house's whole side, not one door.
- Trades churn: people change trade more than a few times a year in a village that is not growing or shrinking.
- The sweep falls below ci.md's bar because people at work on the fields are caught by raids that a crowd at the centre would have fought.
- The tick cost at 5,000 exceeds 0.2 ms at the median: the path fields or the per-tick lookup then need a cheaper form, such as the next step stored per person and refreshed only when its place changes.
- Players read the evening crowd on the square as a bug: an idle person who wanders too little looks frozen, and one who wanders too much looks lost.
