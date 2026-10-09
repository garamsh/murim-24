# ADR 0052: Run the economy from households' needs through town markets, taxed by the power that holds the land

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #211 asks for merchant houses that carry goods between towns, with prices from supply and stock. The owner's decisions of 2026-10-09 in its comments are binding and widen it:
- **A market economy from needs.** People buy and sell tools and weapons as they need them, and the economy emerges from that.
- **People earn to eat and to keep their families.** The household is the unit, and it holds a purse:
  - Earners feed dependants. Each member eats a daily ration bought from the market or the village granary, and the household pays for housing and gear upkeep.
  - People earn by wages, selling harvests, game and crafts, escort or mercenary work (#172), sect stipends and loot.
  - Failing to earn brings, in turn, hunger, a debt of goods to kin, selling gear, migrating to a richer town and turning bandit (#172). These are outcomes of the same need thresholds, not scripted events.
- **Taxes, and treasuries that fund wars.**
  - The power holding a territory taxes its households and markets: a share of trade, a levy per household, and a toll on caravans at its gates. Sects take dues.
  - Taxes, loot sales and dues fill the faction's or sect's treasury, held in its warehouse.
  - War costs money. A war a faction cannot pay for weakens it, by desertion, unarmed recruits and a peace sought sooner, and ADR 0038's war rules read the treasury.
  - A player leading a faction sets the rate, and others set it by a need rule.
- **A tax rate that follows needs, and discontent that makes people flee.**
  - The rate moves up when the treasury runs short of what war or upkeep demands, and down on a surplus, within bounds and at a limited speed.
  - Each household accumulates discontent from high tax, hunger, kin lost in war, a leader of bad reputation and looting by its own side, and it decays when life is good.
  - Past a threshold a household flees to another faction's territory it knows of by rumour (ADR 0039), taking what it can carry. It may be stopped at gates or by patrols, and arriving, it joins that faction.
- **What the design states:**
  - how this sits on ADR 0022's village food store
  - what far tiers aggregate per village
  - the cost per household at the bound
  - the treasury's sources and sinks
  - what an NPC leader decides and when
  - the leader's screens
  - how discontent is stored
  - what each tier simulates
  - that `make sweep` keeps villages alive with a bounded money supply and treasuries, and no starvation spiral in peace
  - that no faction empties in peace at normal rates, and a faction raising its rate in war loses people over years, not days
- **PM direction.**
  - One market per town holds stock and a price per item kind, the price moving with stock against demand.
  - Merchant houses buy where it is cheap and sell where it is dear.
  - Needs are deterministic thresholds.
  - The money balance names its sources and sinks.
  - Loans, interest, banks, exchange rates and haggling are out of scope for now.
  - Far tiers trade in aggregate per town.

Decided by the PM on PR #215's review, 2026-10-09, on this ADR's recommendations, with the owner told and free to override:
- Caravans read the prices of the markets within reach at once.
- A gate held by a fleeing household's own faction stops it at one in two, as pack data.
- The food cap counts four fifths of the harvest, and the sweep is rerun.
- Money is counted in copper.

This is the third of #211's three ADRs, after ADR 0050's items and ADR 0051's holders and warehouses. Its data is ADR 0049's pack format.

Facts from `main` at `5047f29` (`core/region.cpp`, `simulation.md`):
- **Food.** A person eats a measure a day. A field cell yields 600 measures a harvest, once a year on the first day of winter, so a cell feeds 5 people. A year is 4 seasons of 30 days, 120 days.
  - A village feeds everyone from its stock or no one.
  - A person unfed for 10 days moves to a village within 48 cells with room and food, or dies.
  - A village's cap is the least of its housing, harvest and storage (ADR 0022).
- **Silver** comes from nowhere today. Everyone is born with 30 and earns 1 a day at home (ADR 0040), and nothing destroys silver except the dead keeping theirs, which ADR 0051 ends.
- **War.** ADR 0038's diplomacy declares war and makes peace by numbers alone. Growth's raids send 10 raiders.
- **Reputation**, #175, is not built.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and the daily rules are spread over the day's ticks by index.

Options weighed:

- **Money.** The core counts money in the pack's smallest coin, copper in the base pack, with 100 copper to a silver. ADR 0040's amounts are multiplied by 100, so prices move in steps finer than a silver. One coin only: exchange rates are out of scope.
- **Households.**
  - A household is a married couple, or the one who survives, with their unmarried children and anyone they take in. A marriage makes a new household. The person the world starts with or a settler who has none heads its own.
  - Members are derived from kinship and home. A household's record stores its head, its home, its purse, its discontent and one debt, 24 bytes. At 50,000 people, about 14,000 households hold 336 KB.
  - Earners are members from coming of age, 16, to 60, while their health is above half. The rest are dependants.
  - A household that loses its last member passes its purse and its accounts (ADR 0051) to the household of its nearest living kin in the same village. With none, they pass to the faction that owns the village.
  - Rejected: a purse per person. The owner made the household the unit, and a purse per person would leave children and elders unfed by the rule rather than by poverty.
- **Pocket and purse.** A person carries pocket silver, for what it picks up, loots or is paid away from home. Each day, a member at home puts its pocket above 5 silver into its household's purse. Earnings go to the purse, and an NPC's purchases are paid from it. The steered body's pocket is never pooled. The player gives the household silver by hand.
- **Food, on ADR 0022's store.** The village granary becomes a market of one kind, food, with ADR 0022's stock as its stock. Chosen: the granary sells.
  - Each day every household buys a ration for each member at home, at the granary's price, out of its purse.
  - At harvest, intact fields' yield fills the stock as before. Fields are worked by the village's earners who hold no other work. Each day the granary pays out its takings, its sales and exports, to its field workers' households by their number.
  - ADR 0022's cap counts four fifths of the harvest, in the base pack, in place of all of it. A village at its cap then sells a fifth of what it grows, to towns, soldiers and caravans or out of the world, which is a farming village's only income beyond feeding itself. Without that margin a village at its cap eats its whole harvest and earns nothing to pay its levy, upkeep and tools.
  - ADR 0022's hunger days and migration stand. Hunger now comes from an empty purse as well as an empty store.
  - Rejected: free rations paid by a village fund. Food would cost households nothing, so money would have no base need.
- **Earning.** Until #170 gives people trades, an earner earns by the first of these that applies:
  1. a soldier's wage, from its faction's treasury
  2. a sect member's stipend, from its sect's vault
  3. a caravan guard's wage, from its merchant house
  4. a workshop's wage, for each unit it makes, while a standing workshop of its village has room for workers
  5. field work, as above

  Selling loot, found gear and crafts adds to that. Hunting and gathering wait for natural objects (#187), and escort and mercenary contracts for #172.
- **The base need, and failing it.** Each day a household meets its needs in order, each a threshold:
  1. rations for its members
  2. a tool for each field or workshop worker without one (ADR 0050)
  3. house upkeep, its share of its village's standing houses' upkeep
  4. the levy, on a season's first day
  5. empty slots, then spares, for members whose role sets them (ADR 0050), while its purse stays above a reserve of 10 days of its rations

  Short of rations, a household does these in turn:
  1. It takes a debt of goods: the household of its nearest kin in the village with a purse above its own reserve pays the rations, and the debt is recorded, one creditor at a time, repaid first from later income.
  2. It sells members' spares, then valuables, then worn pieces to the market.
  3. It goes hungry. Hungry, it migrates as a household under ADR 0022's rule, now to the village within reach with the lowest food price, room and food, and dies otherwise.

  Turning bandit is #172's. When it lands, a hungry household's armed earners turning bandit is one more step in this order.
  - Rejected: planning, such as saving toward a goal or weighing needs against prices. Every step is a threshold on today's purse, cheap at the bound.
- **Markets.** A town is a village with a standing market, a structure kind of the pack's. The map's starting cities and towns have one, and growth builds one past 60 residents.
  - A market holds, for each item kind it trades, a stock of whole items, a pool of worn durability, a price, and the demand of the last 30 days. It also holds a till.
  - A village without a market buys food at its granary, and everything else at the nearest market within 64 cells, by the daily rule, without walking.
  - A market trades at most 64 item kinds, the pack's list for its kind of market, so its daily pass and a caravan's search are bounded.
  - The player buys and sells by talking to the market's keeper. ADR 0040's `Offer::TRADE` moves from a person whose best discipline is medicine to a market's keeper, and `sells` reads the market.
- **Price.** Each day, per kind, a market compares its stock against its target, its last 30 days' demand:
  - below the target, the price rises 5%
  - above twice the target, it falls 5%
  - it stays within a floor and a ceiling, half and double the kind's base price in the base pack
  - **Buying** takes the price from the buyer to the till, and the faction's share of trade from the till to its treasury.
  - **Selling** pays a quarter below the price, scaled by the durability left, from the till, refused when the till cannot pay. A worn item goes to the worn pool.
  - Rejected: a price set by matching each buyer to a seller. It is haggling, out of scope, and costs a search per trade.
- **Making.** A workshop's holding lists the kinds it makes and the materials each needs, as pack data. Each day each standing workshop makes up to its workers' output, adding to its market's stock and taking materials from it, and pays each worker's household a share of the price as wages.
  - A smithy also turns its worn pool into whole items at a material cost, which is ADR 0050's repair by sale.
  - An apothecary makes herbs and elixirs.
- **Imports and exports.** Each market trades with the world beyond the map at the kind's ceiling and floor:
  - A market whose stock of a kind falls below a tenth of its target imports up to the target at the ceiling, while its till pays. The silver leaves the world.
  - A market whose stock passes four times its target exports the surplus at the floor. The silver enters the world.
  - Valuables are made by no one inside the map, so every new one is an import.
- **Merchant houses.** A house is pack data: its name, its home town, its faction or none, its caravans, the lots a caravan carries (32) and the guards it takes (6). It holds a balance at its counting house (ADR 0051).
  - Each day, for each caravan at home and idle, the house chooses the kind and the market within 1,500 cells along its roads whose margin is largest. The margin is the price there less the price at home and the selling discount, times the cargo, less the tolls on the way and the caravan's wages for the days of the trip.
  - It buys that cargo with its balance and marches the caravan there as an army: a merchant and its guards from the home town's earners, paid daily, the objective the market.
  - It sells on arrival, buys the best cargo for home by the same rule, and returns. Nothing positive leaves the caravan home.
  - Prices are read from every market within reach at once, as the PM decided. Prices heard by rumour, some days old, are the alternative, if caravans crowd one route.
  - Each house's routes use a table of the cost along roads between its home town and each market within reach. The table is built from ADR 0047's coarse graph when the world is created or loaded, and one row is refreshed a day, so the daily search reads it and runs no pathfinding.
  - **The search's cost.** A caravan's search is at most 64 kinds over the markets within reach. At a pack bound of 256 caravans and 512 markets, the worst case is about 8.4 million margins a day. Spread over the day's 1,800 ticks with the other daily rules (ADR 0048), that is about 4,700 integer margins a tick, some hundredths of a millisecond, under 1% of the 4 ms p99, by this estimate.
  - A caravan destroyed scatters its cargo as ADR 0051's piles.
  - A house's balance above 60 days of its wages pays the excess to its members' households.
  - Rejected: caravans by fixed routes. Prices would not meet, and the owner asked for a market that emerges.
- **Taxes.** The faction owning a household's home village, or a market's village, taxes it at the faction's rate `r`, per mille, within the pack's bounds, 0 to 300 in the base pack. One rate sets three taxes:
  - trade: `r` per mille of each sale's price at its markets, paid from the till; a granary's sales of food are not taxed
  - a levy: `r` per mille of 10 days of a household's rations, on each season's first day
  - a toll: `r` per mille of 10 silver for each cargo lot a caravan carries past a gate (ADR 0047) in its territory
  - A sect takes its dues, per mille of its members' earnings, into its vault, from the pack.
  - Rejected: a rate per tax. Three levers for every NPC leader to set is three rules where one does.
- **The treasury.** A faction's account at its treasury (ADR 0051).

  | Into the treasury | Out of it |
  |---|---|
  | Trade, levies and tolls | Soldiers' wages, paid daily into their pockets |
  | Loot sold to its capital's market | Gear, food and elixirs bought for soldiers, from markets |
  | Estates of heirless households (ADR 0051) | Upkeep of the faction's own structures, which leaves the world |
  | Spoils of a village taken (ADR 0051) | The court: 1% a day of the balance above twice its needs, which leaves the world |

  A sect's vault is the same, filled by dues and paying stipends, and its court sink applies too.
- **What an NPC leader decides, each day.** A faction's needs are 60 days of its soldiers' wages and its structures' upkeep at today's count.
  1. **Rate.** Below its needs the rate rises 10 per mille, and above twice its needs it falls 10, at most once every 30 days, within the bounds.
  2. **War.** ADR 0038's war chance applies only while the treasury holds 120 days of the wages of the soldiers a war would raise. Its peace chance applies whenever the treasury falls below 30 days of its needs, whether outnumbered or not.
  3. **Soldiers.** A raid, or an army sent to war, takes no more soldiers than the treasury pays for 30 days. A soldier unpaid for 10 days deserts: it leaves its army and goes home. A recruit takes issue from the treasury (ADR 0051), and without it marches with empty slots.
  4. **Purchases.** While above its reserve, the treasury buys what its soldiers' slots and spares need from its capital's market.
  5. **Loot.** It sells loot it does not issue to its capital's market.

  A steered leader decides none of these. The player sets the rate by hand within the bounds, and orders purchases and sales.
- **Discontent.** Each household holds discontent from 0 to 1,000, two bytes in its record. Each day:
  - it rises by 1 for every 100 per mille the rate stands above a comfortable 100, in proportion
  - it rises by 20 for each member who went hungry
  - it rises by 200 once for each member or kin killed by a foe's wound
  - it falls by 2 a day when no member went hungry and the rate is at or below 100

  The other two of the owner's inputs wait for what they read. A leader's bad reputation is #175's. Looting by a household's own side cannot happen under ADR 0051, since a soldier loots only from foes, so it waits for a rule that lets a faction take from its own, such as requisition.
- **Flight.**
  - At 800, a household flees. It chooses, among the villages it knows, the one of another faction, at peace or allied by `relation`, with room and food, whose faction's rate is lowest, the lower index among equals. It knows a village within ADR 0022's migration reach, or one a rumour its members heard names (ADR 0039).
  - It leaves as an army, as settlers do, carrying what its members carry and its purse.
  - Passing a gate held by its own faction, where a village of that faction stands with living residents within its claim, it is stopped and sent home, with a chance of one in two, rolled from the seed, and keeps its discontent. Patrols wait for #172.
  - Arriving, its members become the village's residents and take its faction. Their discontent halves.
  - Rejected: individuals fleeing alone. The owner made the household the unit, and a household split by flight leaves children behind with no one to feed them.
- **The money balance.**
  - **Sources.** Exports at the floor, and the starting purses, tills, treasuries and balances. ADR 0040's purse at birth and wage from nowhere are retired: a newborn holds nothing, and every wage is paid by someone.
  - **Sinks.** Imports at the ceiling, valuables among them; the upkeep of houses and of factions' structures; the court of treasuries and vaults above twice their needs; silver piles that expire (ADR 0051); and the gap between floor and ceiling on everything sold out of the world and bought back.
  - **Comfort.** A household whose purse is above 30 days of its needs spends 1% a day of the excess on valuables, and a market's till above 60 days of its turnover pays the excess to its faction's treasury.
  - **Why it is bounded.** Every holder of silver has a level above which a share of the excess leaves, or moves to a holder whose excess leaves:
    - households by comfort, through imports of valuables
    - treasuries and vaults by the court
    - tills into treasuries
    - houses into households

    So the sinks grow with the silver above those levels. Exports are bounded by what the world makes, which is bounded by its people, and the population bound bounds them. The supply settles where exports meet the sinks, below the sum of the holders' levels plus exports over the least share. All of those scale with the bound.
- **The cost per household at the bound.** For a household of two field workers and two children in the base pack, with food at its base price of 50 copper a measure, a day:
  - rations, 200 copper
  - house upkeep, 5
  - tool wear, 6: two hoes of 300 copper lasting 100 working days
  - the levy at a rate of 100, about 7

  That is about 218 copper. The two workers' share of their granary's takings is about 200 for what the village eats, plus 25 to 50 for its fifth sold, at the floor or the base price. So about 225 to 250 comes in, and the household saves about 7 to 32 a day toward gear and comfort. At 50,000 people, about 14,000 households spend about 30,000 silver a day. A household's record is 24 bytes, 336 KB in all. Its daily rule, spread over the day's 1,800 ticks, is about 8 households a tick.
- **The daily market pass.** Each market updates the price, demand and trade with the world of at most 64 kinds, and runs its workshops. At 512 markets that is about 33,000 kind updates a day, about 18 a tick spread over the day. It is negligible beside the households.
- **Tiers.** Daily rules are untiered (ADR 0042), so households' needs, markets' prices, production, taxes, treasuries, discontent and flight run for every village wherever it is. What a far area aggregates per village:
  - its households' gear purchases, summed by kind into one order at their market and handed out in roster order
  - wear in its fights, applied as the expected hits from ADR 0042's far strike rule
  - caravans and fleeing households, which move as far armies do

  Live and stepped areas run all of these per person and per tick. Outcomes in far areas may differ by profile within ADR 0048's terms.
- **Out of scope for now.** Each is rejected with what would bring it in:
  - **Loans and interest.** A debt of goods to kin is the only debt. Lending at interest needs a lender's rule and a default's consequence, which wait for a need the kin debt cannot meet.
  - **Banks.** Warehouses keep balances but lend nothing.
  - **Exchange rates.** One coin, copper, everywhere, until a second theme or region needs another.
  - **Haggling.** Prices are the market's. The player pays what an NPC pays.
- **Saves, the hash and determinism.** Households, purses, debts, discontent, markets' stocks, pools, prices, demand and tills, caravans and treasuries are saved and hashed. Every amount is an integer, a percentage a per mille, and every choice breaks ties by the lower index.
- **The view.**
  - The **market window** lists each kind traded, with its price, its stock and whether its price rose or fell this week, for buying and selling. It replaces ADR 0040's wares screen.
  - The **household panel** shows the purse, its members, the days of rations it holds, its debt and its mood, from discontent in five words.
  - The **treasury window**, for a leader, shows the balance, last season's income and spending by source, the rate within its bounds, and the soldiers paid and unpaid, and orders purchases and sales.

The implementation of ADRs 0050 to 0052, in order, each an issue with its acceptance:
1. **Items and equipment** (ADR 0050). Acceptance: kinds, slots, the weapon's strike, bare hands, arts by family, wear and breaking, the gear rule and loadouts, with ADR 0046's targets measured again, `make bench` on loadouts and `make sweep` rerun.
2. **Inventories, the ground and death** (ADR 0051). Acceptance: drops, piles, expiry and the pick-up rule, with the inventory window.
3. **The view's gear layers and faction dye** (ADR 0050). Acceptance: a body draws its four layers, an empty slot draws bare, and a person changing faction changes its tint on the next frame, with the core's hash unchanged.
4. **Households and food** (this ADR). Acceptance: purses, pockets, the granary selling, field wages, the needs order, kin debt and migration as a household. ADR 0040's purse and wage are retired, and the sweep shows no starvation in peace.
5. **Markets** (this ADR). Acceptance: price, buy, sell, making, repair by sale, imports and exports, and the market window.
6. **Warehouses and loot** (ADR 0051). Acceptance: keepers, accounts, fees, vaults, treasuries, scattering, spoils, estates, loot routing, souls' accounts, caves for the steered body, and the warehouse window.
7. **Taxes and treasuries** (this ADR). Acceptance: the rate, the three taxes, dues, wages, desertion, issue, the diplomacy reading the treasury, and the treasury window.
8. **Discontent and flight** (this ADR). Acceptance: each input and the decay with a test and its control, a household fleeing at 800 to the lowest-rate known village, one stopped at its own faction's gate, and the sweep's flight targets.
9. **Merchant houses and caravans** (this ADR), with the tolls. Acceptance: a caravan carrying the largest-margin cargo and selling it, a toll paid at a gate, a destroyed caravan scattering its cargo, and the money supply target held in the sweep.
10. **Far-tier aggregation**, with ADR 0042's far tier. Acceptance: a far village's gear purchases settled as one order per kind, far fights' expected wear applied, and the sweep's targets held with tiers on the standard profile.

`make sweep` gains, per seed, the money supply by holder at each decade, each treasury, food's lowest and highest price, hungry households and households fled per faction a year. The steps that change money show, over seeds 0 to 49 for 100 years:
- the money supply staying within half and double its year-10 level from year 10 on
- every treasury below a bound the pull request states
- hungry households under 5% in every year without a war
- no faction losing more than a tenth of its households to flight in peace at its starting rate
- a faction held at its highest rate through a war losing households over two years or more, and less than a tenth within its first 100 days

## Decision

Money is copper, 100 to a silver. Households of a couple, their children and those they take in hold a purse and are the economy's unit. They earn by wages that someone pays: fields through the granary, workshops through their market, soldiers through their treasury, members through their sect, and guards through their house. They meet their needs in order, each a threshold: rations, tools, upkeep, levy, then gear by role. Short of rations, a household takes a debt of goods from kin, then sells what it carries, then goes hungry and migrates as a household or dies.

The village granary sells food, and a village's food cap counts four fifths of its harvest, so a village at its cap has a fifth to sell. A town's market holds stock, a worn pool, a price and a till for each kind. Its price moves 5% a day against 30 days' demand, between a floor and a ceiling at which the market exports and imports, and its workshops make goods and restore worn ones. Merchant houses send caravans for the largest margin within reach along roads.

The faction owning a village taxes trade, households and caravans at gates by one rate. An NPC leader moves the rate by its treasury against 60 days of needs, at most 10 per mille a month. War needs a war chest, peace comes sooner on an empty treasury, unpaid soldiers desert, and recruits without issue march with empty slots. A player leader sets the rate and orders purchases.

Households accumulate discontent from tax, hunger and kin killed, decaying in good times. At 800 a household flees to a known village of another faction with a lower rate, unless its own faction's gate stops it. Every holder of silver sheds a share of its excess, so the money supply is bounded by the population bound. Loans, interest, banks, exchange rates and haggling are out of scope.

This supersedes these:
- ADR 0040's sellers by best discipline, its fixed prices, its purse at birth, its wage from nowhere and its NPC daily purchase of each ware
- ADR 0022's free feeding from the village's stock, now rations bought from it; its cap from the whole harvest, now four fifths of it; and its migration by the person, now by the household
- ADR 0038's diplomacy by numbers alone, now gated by the treasury
- ADR 0023's raiding party of a fixed count, now no more than the treasury pays for 30 days

## Consequences

- Silver has a use and a cost, which ADR 0040's signs named as missing: prices move, and wealth is spent.
- Villages can starve with full granaries, if their households cannot pay. Field wages paid from the granary's own sales are what prevent it in peace, and the sweep must show it.
- Wars are paid for. A long war drains the treasury, raises the rate and drives households away over years, which ends wars that numbers alone would not.
- The player gains goals: trading between towns, paying a household, leading with a rate.
- Every economic number is pack data, so balance is tuned by the sweep without code.
- The save version rises with each step, and the pinned hashes change.
- The daily rules carry most of the cost. Spread over the day, they stay within ADR 0048's budget only if the measurement in step 4 shows it.

## Signs the decision was wrong

- The money supply climbs or collapses over a century in the sweep despite the sinks, so a holder's excess level or share is wrong, or a holder was missed.
- Food prices swing between floor and ceiling every season, from harvest to winter, so a year-long target is needed in place of 30 days' demand.
- Caravans crowd one route because every house reads the same prices at once, emptying one market and flooding another.
- Households flee in waves at the first war, or never, so discontent's inputs need weights other than these.
- The daily household rule, spread over the day, costs a tick more than its share of ADR 0048's budget at the bound.
