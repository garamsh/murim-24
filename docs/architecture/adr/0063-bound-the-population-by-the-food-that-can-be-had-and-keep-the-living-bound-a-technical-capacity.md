# ADR 0063: Bound the population by the food that can be had, and keep the living bound a technical capacity

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-10, in issue #229, as the world's design principle:
- **Simple rules whose consequences combine into complex cause and effect,** the way money shapes villages' forms, their growth and each person's life. Our job is to tune the balance, not to script outcomes.
- **Population has no separate limit: it is bounded by the food that can be had.** Fruit, game and fish regrow, fields yield, and how fast they regrow is what governs how many people the world feeds.

The issue asks the design to decide these:
- births depending on a household's food and wealth, and deaths on hunger, with a village's housing and food caps kept only as consequences of what is built and harvested
- the core's people storage as a technical capacity set by memory and the performance budget, well above what food supports, whose reaching the bench reports as a failure
- regrowth rates as the levers: fruit, game, fish and field yields, by biome and season, as pack data
- the balance loop: `make sweep` reporting population, food, money, villages founded and starved over 100 years per seed, with target bands, tuned by pack data
- which fixed caps it supersedes, and how they become consequences

What this builds on, from `main` at `c86b08d` and the open PRs it cites:
- **Births today** (ADR 0021, ADR 0022, `core/region.cpp`).
  - A wife of bearing age, 18 to 40 in the region, whose husband lives, bears with a chance of 400 per mille a year, no sooner than 2 years after her last child.
  - The chance is scaled by her village's headroom, (cap − population) / cap, so births slow near the cap and stop at it.
  - No child is born while `MAX_PEOPLE`, 500, live (ADR 0017).
- **The cap** (ADR 0022). Derived on read, the least of standing housing, intact fields' yearly harvest per person and storage per person. A house houses 8 and a hall 12. A granary stores 2,400 measures and a storehouse 3,600. A field cell yields 600 measures once a year, on the first day of winter.
- **Food and hunger** (ADR 0022, ADR 0052). A person eats a measure a day, 120 a year, so a field cell feeds 5. ADR 0052 makes the granary sell rations to households from their purses, and counts four fifths of the harvest in the cap. A person unfed for 10 days moves to a village within 48 cells with room and food, or dies, and ADR 0052 moves the household instead.
- **Growth** (ADR 0023, ADR 0025). Every 30 days a village whose people reach a fifth of its cap lays the plan that most raises its scarcest holding, within 8 cells of its centre.
- **Wild food** (ADR 0056, merged in #220). A fruit tree yields 6 fruit each season but winter, and a bush 3 berries, both sold to the granary as measures. Slot kinds, their biomes, growth and regrowth are pack data.
- **Game and fish** wait for animals, #171, which ADR 0056 says will be gathering from a slot that moves, with its yield as pack data.
- **The bound.** ADR 0045 and ADR 0048 measure it on the low profile between 10,000 and 50,000, and it stays 500 until then. ADR 0048 estimates 20,000 to 40,000 living once growth has filled ADR 0047's map. The open PR #233, for #216, makes the bound rules data, `people_capacity`, `MAX_PEOPLE`'s 500 by default and at most `PEOPLE_CEILING`, 2^20. The PM ruled there that it is a storage and performance limit, not a rule of play. Each bench scenario sets its capacity a quarter above the people it holds, 625 for M1 and 6,250 for the live scenario, and the bench fails a run whose living ever reach it.
- **The yield, decided by the PM on #242 (2026-10-11).** Fields stay at a realistic 4 to 8 cells a household, and the base pack's yield is lowered to about 1 person a cell, so food binds the population. This ADR sets it as 120 measures a cell on plain and 60 on hills.
- **The sweep.** `make sweep` lives seeds 0 to 49 for 100 years with days of five ticks, and every faction stays peopled on 48 of them. ADR 0052 adds the money supply, treasuries, food prices, hungry households and flight to it.

Options weighed:

- **Births follow food and wealth.** A wife's chance is 400 per mille a year, as now, scaled by two factors, each per mille and multiplied, rounded down:
  1. **Provision.** The days of rations her household can have: the lesser of the days its purse buys at the granary's price (ADR 0052) and the days the village's stock feeds everyone whose home it is. The factor is those days × 1,000 / 30, at most 1,000. So a household with a month's food bears at the full chance, one with a fortnight's at half, and one with none never.
  2. **Crowding.** While the village's people exceed its standing housing, the factor is housing × 1,000 / people. Otherwise it is 1,000.

  The 30 days are ADR 0052's comfort threshold, and both are pack data. Until ADR 0052's households are built, the provision is the village's stock alone.
  - Rejected: the headroom factor kept beside them. It reads the cap, so births would still stop at a number rather than at the food.
  - Rejected: births by the village's food alone. The owner named wealth, and a poor household in a fed village is the case ADR 0052's purses exist for.
- **Deaths follow hunger,** as ADR 0022 and ADR 0052 decide, unchanged: a household unfed for 10 days migrates to a village with food, or its members die. That is the only check from above. A village that outgrows its harvest runs out before the next one, and the hungry leave or die.
- **Caps as consequences.** ADR 0022's cap stays derived on read, but no rule stops at it.
  - Growth reads it, as ADR 0023 does, to see what is scarcest and what to build.
  - Migration and flight read it as room: a village whose people are below its cap.
  - The granary's fifth for sale (ADR 0052) is still read from it.
  - Storage still bounds the stock, and a harvest above it is sold or lost (ADR 0052).
  So housing and food limit people only through what they cause: crowding slows births, and an empty granary brings hunger.
- **A worked example,** at the base pack's 120 measures a cell. ADR 0056's starting village, with its fields at 4 cells a household, as the PM's decision on #242 has them: 4 households, 14 people, 2 houses housing 16, and 16 field cells yielding 1,920 measures a year. ADR 0056's own example gives it 4 cells at the old yield; its field counts scale with the yield, which the implementation reads from pack data.
  - **Its food.** 1,920 measures feed 16 people for 120 days. About 10 grown fruit trees within its 32 cells add 180 a year, a person and a half. So its food bounds it near 17.
  - **At 20 people.** The 2,100 measures last 105 days from the harvest. Births run at the full chance until day 75, fall to none by day 105, and for the last 15 days households go unfed. The shortfall is 300 measures, about 3 people's need, so about 3 go hungry before the harvest and leave or die.
  - **Crowding.** At 20 people in housing for 16, the crowding factor is 800, so births slow before food runs short.
  - **What growth does.** Food is scarcest, so growth lays 4 more field cells at no material cost, and the next harvest feeds 20, 21 with the fruit. The village's people follow its fields, and its fields are bounded by the buildable cells within its 8.
  - Under ADR 0022 the same village would bear no child at all: its cap is 12, four fifths of its harvest, below its 14 people, with food for 16 in store.
- **The living bound becomes a technical capacity.** The core's capacity for living people is the bound ADR 0048 measures: the most the tick and frame budgets and memory hold on the low profile, 500 until measured. It is not a rule of the world.
  - A birth at capacity is still refused, since nothing may be stored past it.
  - The world counts each such refusal, 4 bytes, saved and hashed.
  - Reaching it is a failure of the balance, never gameplay. `make sweep` fails a seed with any refusal. `make bench` fails a run whose living ever reach its capacity, as the PM ruled on #233: each scenario sets its capacity a quarter above the people it holds, 625 and 6,250, so a run that reaches it was decided by the capacity and not by the world.
  - **Well above what food supports.** The base pack is tuned so that the sweep's peak living stays at most three fifths of capacity, a margin of two fifths for wars' and harvests' swings.
  - Rejected: removing the capacity. Memory and the tick budget are finite, and ADR 0048's bound is what they hold.
  - Rejected: a capacity far above the bound, such as #233's ceiling of 2^20. People past the bound would miss the tick budget, so the world would fail on time instead of on a count.
- **Yields as the levers.** Every food yield is pack data (ADR 0049), by biome and by season:
  - **Fields.** A field kind's yield per cell for each biome, and the seasons it is harvested. The base pack's: 120 measures on plain and 60 on hills, at the first day of winter, about 1 person a cell on plain, as the PM decided on #242. It replaces today's 600.
  - **Wild food.** ADR 0056's slot kinds already give biomes, growing and regrowing days and yields. Each food kind gains its yield for each season, so the base pack's fruit tree yields 6 in spring, summer and autumn and none in winter, as ADR 0056 has it.
  - **Game and fish,** when #171 lands, are levers by the same rule: their rates of appearance and their yields are pack data, by biome and season, whatever form #171's design gives animals.

  The sweep tunes these and nothing else to reach the bands below.
  - Rejected: a multiplier on the whole world's food. One number cannot make a river valley richer than a hill, and the owner's levers are each kind's regrowth.
- **The balance loop.** `make sweep` prints, per seed, at each decade:
  - the living, births and deaths, and deaths by hunger
  - food: the harvest, the wild food gathered, and the stock at the end of autumn, each per living person
  - money, as ADR 0052 adds it
  - the peopled villages and the villages founded
  - births refused at capacity

  **Target bands,** over seeds 0 to 49 for 100 years. They are the sweep's own figures, so a change is measured against them:

  | Measure | Band |
  |---|---|
  | Factions peopled at every year's end | on at least 48 seeds, as today |
  | Living at year 100 against the start | at least 3 times, as ADR 0025 sets |
  | Peopled villages at year 100 against the start | at least 2 times, as ADR 0025 sets |
  | Living at year 100 against year 80 | within four fifths and five fourths, on at least 45 seeds, so the population has met its food |
  | Food a living person has a year at year 100, harvest and wild | 120 to 200 measures: fed, and not far past its needs |
  | Deaths by hunger, in years without a war | at most 5% of deaths |
  | Hungry households, in years without a war | under 5%, as ADR 0052 sets |
  | Money supply from year 10 | within half and double its year-10 level, as ADR 0052 sets |
  | Peak living | at most three fifths of capacity |
  | Births refused at capacity | none, on every seed |

  - Rejected: bands checked by `make ci`. The sweep takes minutes per seed and tunes data, so it gates nothing, as today. A pull request that changes a rule or the pack's food shows the sweep's figures against the bands.
- **Supersessions.**
  - ADR 0017's bound of 500 living people as a rule of the world, now a technical capacity.
  - ADR 0021's births while fewer than 500 live, now births refused only at capacity, counted as a failure.
  - ADR 0022's births scaled by headroom and stopping at the cap, now births by provision and crowding, the cap read only by growth, migration and the granary.
  - ADR 0022's single field yield, now yields by biome and season in the pack, and ADR 0052's field cell of 600 measures a harvest, now 120 on plain in the base pack.
  ADR 0045 and ADR 0048 stand: they measure the capacity.
- **Tiers.** Births, feeding and hunger are daily rules, untiered (ADR 0042), so they run the same in every area. Far villages' gathering is ADR 0056's aggregate.
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Births.** The daily kinship scan already visits every wife. Each reads her household's purse and her village's stock, two reads more, within ADR 0048's spread of the daily rules.
  - **Memory.** The refusal counter, 4 bytes. The yield tables, at most 64 kinds of 16 biomes and 4 seasons at 2 bytes, 8 KiB.
  - **The sweep** adds counts it already passes over.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - a yield of at most 65,535 measures a cell or a slot a season
  - provision days of 1 to 360
  - at most 16 biomes in a field kind's yields, as ADR 0056 bounds a slot kind's affinities
- **Saves, the hash and determinism.** The refusal counter is saved and hashed, and `SAVE_VERSION` rises for it. The yields are pack data, in the pack's identity (ADR 0049). Every factor is an integer per mille rounded down, and births are rolled from the seed in the order the kinship scan visits, as today. The pinned hash changes, since births change.

The implementation, in order, each a pull request with its acceptance, after ADR 0052's households and food (its step 4) and ADR 0056's building from materials (its step 3):
1. **The balance loop.** The sweep's new columns and the bands, measured on today's rules first, so the change after it is seen against a baseline.
   - Acceptance: the columns printed for seeds 0 to 49, and a run with a capacity set too low failing, where the default passes.
2. **Births by food and crowding, and the capacity counted.**
   - Acceptance, each with a control: a household with 30 days' provision bearing at the full chance, one with none never; crowding past housing slowing births; a village growing past its old cap while food allows, and shrinking by hunger after its fields burn; a birth refused at capacity counted.
   - Acceptance: `make bench` failing a run whose living reach the scenario's capacity, where the default headroom passes.
   - Acceptance: `make sweep` within every band.
3. **Yields by biome and season.**
   - Acceptance, each with a control: a hill field yielding half a plain's; a fruit tree yielding nothing in winter; a pack past a yield bound refused.
   - Acceptance: `make sweep` within every band.
4. **Game and fish,** with #171, as #171's design gives them.
   - Acceptance, with a control: wild meat entering the food column where an area without animals adds none.
   - Acceptance: `make sweep` within every band.

## Decision

Population has no limit of its own. A wife bears at the pack's chance scaled by her household's provision, the days of food its purse and its village's stock give, full at 30, and by crowding, housing over people once people exceed housing. Hunger, as ADR 0022 and ADR 0052 decide it, is the only check from above. ADR 0022's cap stays derived, read by growth, migration and the granary, and no rule stops at it.

The core's capacity for the living is ADR 0048's measured bound, a technical limit and not a rule. A birth at it is refused and counted, and the sweep and the bench report any as a failure. The base pack keeps the peak at three fifths of it.

Food yields, of fields and of wild kinds, are pack data by biome and season, and they are the levers. `make sweep` reports population, food, money, villages and hunger by decade against target bands over 50 seeds and 100 years, and tuning changes only pack data.

This supersedes ADR 0017's bound as a rule, ADR 0021's births while fewer than 500 live, ADR 0022's births by headroom stopping at the cap and its single field yield, and ADR 0052's field cell of 600 measures a harvest. The base pack's fields yield 120 measures a cell on plain and 60 on hills, as the PM decided on #242.

## Consequences

- Villages grow until their food runs short, then hold there, so their size reads their land, their fields and the wild food around them.
- A harvest burnt in war brings hunger and flight the next winter, and births fall, without a rule for either.
- Hunger becomes common at the edge of growth, a few people a year in a growing village. The bands keep it under 5% of deaths in peace.
- Tuning moves from caps to yields, so a change to one food kind shows in the sweep's population.
- Reaching capacity becomes a bug to fix in the balance, not a quiet ceiling.

## Signs the decision was wrong

- On ADR 0047's map, land within reach is so plentiful that births alone limit growth, and the living reach capacity before food binds. A labour lever, such as a field worker tending a bounded number of cells, would then be needed.
- Villages overshoot and starve in waves every winter, so the provision factor needs a longer horizon than 30 days.
- Crowding never binds, because growth houses people faster than they are born, and it can go.
- The sweep's bands cannot all be met by yields alone, so a lever other than food is needed, against the owner's principle.
- The poor stop bearing in fed villages, so whole classes of household die out, and provision should read the village's food more than the purse.
