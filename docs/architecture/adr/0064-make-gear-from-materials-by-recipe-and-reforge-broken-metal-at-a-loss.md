# ADR 0064: Make gear from materials by recipe, and reforge broken metal at a loss

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-10, in issue #231:
- Weapons and armour take resources to make: iron, steel, leather, wood and cloth.
- **Broken gear can be recycled.** ADR 0050 says an item at no durability is gone, and this replaces that point.
- Mass and materials stay conserved, as ADR 0051 and ADR 0057 keep them for items and the ground.
- **Animal parts are materials,** in the issue's later comment: hide, bone, horn and claw come from every animal, and rarer parts from spirit beasts (#171).

The PM's direction of the same day, in the issue's comment, after the owner asked whether recycling is realistic:
- **Metals recycle, but not for free.** Scrap iron and bronze were melted or reforged; iron was too precious to throw away. So iron, steel and bronze recycle. Reforging takes charcoal, a smith's labour and time, and loses part of the metal, proposed at half, as slag.
- **Organic materials do not.** Leather, wood and cloth cannot be remelted. A broken organic part is lost, or at most becomes fuel or rags.
- **What follows.** Battlefield scrap becomes valuable loot, and a faction short of iron arms itself by gathering scrap.
- The owner may still remove recycling. If so, ADR 0050's "gone" stands.

The issue asks the design to decide these:
- recipes as pack data: each made item's materials and labour by trade (#170), from material kinds ore, iron, steel, leather, timber and cloth, and where they come from: mines on the map, game for hides (#171) and forests (ADR 0056)
- who makes, and how: workshops making what their market lacks, deterministically, and a faction's arms spending ordering gear (#230)
- breaking yielding scrap below a yield of 1, held or dropped like any item, and reforged by a smith
- battlefield scrap and broken gear as loot
- materials and money staying bounded, and scrap never letting gear grow from nothing
- the supersessions, and the memory and cost at the bound

What this builds on, from `main` at `c86b08d` and the open PRs it cites:
- **ADR 0050.** Items are pack data: kinds with a family, tier, material, worth, durability or a stack size, price and effects. Materials are a family of kinds that stack. Weapons wear by hits made, armour by hits taken, tools by days worked. A broken piece leaves its slot empty, and the item is gone. Repair is a sale: a worn item sold goes to the market's worn pool, and a smithy restores it from materials.
- **ADR 0051.** Every item has one holder, a person, a warehouse lot or a pile on the ground, and moves by four verbs. A cell holds 16 piles, and the world 1,048,576, above the worst day's drops of 985,872, every person dying with 13 stacks among them. A soldier at war picks up drops of its foes as loot for its faction.
- **ADR 0052.** A market holds stock, a worn pool, a price, a 30-day demand and a till for each of at most 64 kinds. Its price moves 5% a day against its target. A workshop makes up to its workers' output each day from materials in its market's stock, and pays its workers' households a share of the price. A market imports below a tenth of its target at the ceiling and exports above four times it at the floor. The treasury buys its soldiers' slots and spares at its capital's market and sells its loot there. A household of two field workers earns about 225 to 250 copper a day, so a worker's day is about 110 copper.
- **ADR 0056,** proposed, merged in #220. Natural objects are slots the map's build tool places, with kinds as pack data. A village's account holds the materials its next plan needs, gathered by its idle earners and paid from the treasury, and sells what passes twice its target to the market. Trees yield 4 wood, rocks 3 stone, and a stump regrows on its kind's days.
- **ADR 0063, proposed in the open PR #243** for #229. The world runs by simple rules whose consequences combine, balanced by tuning pack data against the sweep's bands, not by scripted outcomes.
- **ADR 0057,** proposed, merged in #222. Changes to the ground conserve mass, and a sum test counts every unit before and after.
- **ADR 0065, proposed in the open PR #248** for #230. It sets a faction's spending priorities, arms among them.
- **ADR 0067, proposed in the open PR #241** for #171. Animals drop hide, bone, horn and claw by species, and spirit beasts rarer parts.
- **Not built.** Trades are #170's.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000.

Options weighed:

- **Material kinds.** Materials are ADR 0050's material family, stacking item kinds of the pack, up to 50 a stack in the base pack. The base pack's:

  | Material | Base price (copper) | Comes from |
  |---|---|---|
  | Ore | 20 | ore veins at the map's mine sites (below) |
  | Wood, the issue's timber | 10 | trees (ADR 0056) |
  | Charcoal | 35 | wood, burnt at a smithy |
  | Iron | 150 | ore and charcoal, smelted at a smithy, or iron scrap reforged |
  | Steel | 500 | iron and charcoal, at a smithy, or steel scrap reforged |
  | Bronze | 250 | bronze scrap reforged, or imports |
  | Hide | 30 | animals (#241's ADR 0067); imports until then |
  | Bone | 20 | animals |
  | Horn | 30 | animals |
  | Claw | 30 | predators |
  | Spirit hide, spirit bone, spirit claw | 400, 300, 500 | spirit beasts only |
  | Leather | 115 | hides, tanned at a leatherworks |
  | Cloth | 40 | imports, until a fibre crop is designed |
  | Iron scrap, steel scrap, bronze scrap | 30, 200, 80 | broken gear (below) |

  - Each material kind may name a metal: iron for iron, steel and their scrap, since steel is refined iron, bronze for bronze and its scrap, and none for the rest. The core reads the metal to check recipes and to count conservation, and names no metal itself.
  - The base pack makes no bronze piece, since its map has no copper or tin. Bronze enters as imported and looted pieces, and recycles like the other metals.
- **Mines on the map.** Ore comes from a natural-object kind of ADR 0056's, the ore vein: yield 2 ore, present at the map's build, its stump reopening in 360 days, 3 years. The map's authoring marks mine sites, each a point and a radius, beside its named places (ADR 0047). The build tool gives ore veins affinity only within them, on hills. Gathering a vein is ADR 0056's `gather`.
  - Chosen, because it is one row of kind data under ADR 0056's rules, and the same map gives the same mines in every game.
  - Rejected: a mine as a structure with its own workforce. It would be a second gathering rule beside ADR 0056's, for the same walk and the same yield.
- **Recipes.** Each made kind, gear or material, carries a recipe in the pack's `items` document:
  - the workshop kind that makes it
  - its materials, each a kind and a count, at most 8
  - its labour, in tenths of a worker's day
  - its output count, for kinds that stack

  A worker gives 10 tenths a day. The base pack's recipes:

  | Made | Workshop | Materials | Labour, days | Scrap when it breaks |
  |---|---|---|---|---|
  | 4 charcoal | smithy | 8 wood | half | none, burnt |
  | 2 iron | smithy | 3 ore, 4 charcoal | 1 | none |
  | 1 steel | smithy | 2 iron, 2 charcoal | 1 | none |
  | 1 iron, reforged | smithy | 2 iron scrap, 1 charcoal | half | none |
  | 1 steel, reforged | smithy | 2 steel scrap, 1 charcoal | half | none |
  | 1 bronze, reforged | smithy | 2 bronze scrap, 1 charcoal | half | none |
  | 1 leather | leatherworks | 2 hides | half | none |
  | Hoe | smithy | 1 iron, 2 wood, 1 charcoal | 1 | 1 iron scrap |
  | Iron sword | smithy | 3 iron, 1 wood, 1 leather, 2 charcoal | 2 | 3 iron scrap |
  | Steel sword | smithy | 3 steel, 1 wood, 1 leather, 2 charcoal | 3 | 3 steel scrap |
  | Iron saber | smithy | 4 iron, 1 wood, 1 leather, 2 charcoal | 2 | 4 iron scrap |
  | Spear | smithy | 2 iron, 3 wood, 1 charcoal | 1 | 2 iron scrap |
  | 20 darts | smithy | 1 iron, 1 charcoal | 1 | none, each spent when thrown (ADR 0050) |
  | Iron gauntlets | smithy | 2 iron, 1 leather, 1 charcoal | 1 | 2 iron scrap |
  | Helmet | smithy | 2 iron, 1 leather, 1 charcoal | 1 | 2 iron scrap |
  | Lamellar | smithy | 6 iron, 3 leather, 3 charcoal | 4 | 6 iron scrap |
  | Leather armour | leatherworks | 4 leather | 2 | none |
  | Bone-studded leather armour | leatherworks | 4 leather, 4 bone | 3 | none |
  | Horn-hilted saber | smithy | 4 iron, 1 horn, 1 leather, 2 charcoal | 2 | 4 iron scrap |
  | Horn-capped staff | leatherworks | 3 wood, 2 horn, 1 bone | 1 | none |
  | Claw gauntlets | smithy | 2 iron, 2 claw, 1 leather, 1 charcoal | 2 | 2 iron scrap |
  | Spirit lamellar | smithy | 4 iron, 3 spirit hide, 2 spirit bone, 3 charcoal | 6 | 4 iron scrap |
  | Spirit claw gauntlets | smithy | 2 iron, 2 spirit claw, 1 leather, 1 charcoal | 3 | 2 iron scrap |
  | Cloth armour, hat, headband | tailor | 4, 1 and 1 cloth | 1, half, half | none |

  - **Where the prices come from.** A recipe's base price is about its materials at their base prices plus its labour at 110 copper a day. Iron: 3 ore and 4 charcoal, 200, and a day, 110, make 2, about 155 each, so 150. A hoe is 150 + 20 + 35 + 110 = 315, near ADR 0052's 300. An iron sword is 450 + 10 + 115 + 70 + 220 = 865, so 850. Leather is 2 hides, 60, and half a day, 55: 115.
  - **Scrap's price** is what a smith can pay for it and still sell the metal: reforged iron costs 2 scrap, 35 of charcoal and 55 of labour, so iron scrap is worth at most (150 − 90) / 2 = 30. Steel scrap by the same sum is at most 205, so 200, and bronze scrap (250 − 90) / 2 = 80.
  - **Animal parts.** Bone, horn and claw are fittings and studs on gear of ADR 0050's families: a horn hilt, a capped staff, claws on gauntlets, bone studs on leather. Spirit parts make spirit-beast gear, so it appears only where spirit beasts have been killed. They break into the scrap of their metal and nothing of their parts, which are organic.
    - A bow is left out: ADR 0050 has no bow family, and adding one is a weapon family's decision of its own, with its shape and art discipline.
  - **Carpenters,** whom #231 lists, are left out of the base pack. No made kind is wood alone, and every wooden part is an input the smithy or the leatherworks fits, so a carpenter's workshop would make nothing the market buys. With #170's trades a carpenter's shop can make the staff and the shafts, as a pack row.
  - The leatherworks and the tailor are new workshop kinds of the base pack, laid by growth's scarcity rule as ADR 0029 lays the smithy, at 40 wood and 10 stone each (ADR 0056).
  - **Trades.** Until #170 gives people trades, a workshop's workers are ADR 0052's: earners its village assigns while the workshop has room. With #170, a smithy employs smiths, a leatherworks leatherworkers and a tailor tailors, each workshop kind naming its trade.
  - Rejected: a recipe on the workshop kind, as ADR 0052's holding lists it. A kind made in two workshops would carry two recipes, and the check below would read two places. The workshop kind keeps only the list of kinds it makes.
- **Who makes, and how.** Each day, on its market's tick, each standing workshop spends its labour, its workers × 10 tenths plus what it carried over:
  1. Of the kinds it makes, it takes the one whose market stock stands lowest against its target, in per mille, among those whose materials the market's stock holds now. Ties go to the earlier row of the workshop's list, then the lower kind index.
  2. It takes the materials from the stock at once, and spends the recipe's labour. A batch whose labour exceeds what is left is carried to the next day, its kind and labour done held by the workshop, 4 bytes.
  3. A finished batch adds to the stock, and the workshop pays its workers' households a share of the price, as ADR 0052 has it.
  4. It stops when the labour is spent, or when no kind is below its target with materials in stock.

  So a smithy whose market lacks iron smelts or reforges before it makes swords, and one with iron in hand and no swords makes swords. When a kind has two recipes, iron from ore and iron from scrap, the workshop list's order decides which is tried first, and the base pack puts the reforge first, since it costs half the labour.
  - Rejected: making whatever has the largest margin. Margins move with every price, so every smithy would chase the same kind; the shortfall against a target settles on what the market lacks.
- **Where the materials come from.** A market buys materials as it buys anything. Two inflows feed it:
  - **The village's account.** A village with a market adds to each material's target, in its account (ADR 0056), its market's shortfall of that material below its target. So idle earners gather ore and wood for the smithy as they gather for the next plan, paid from the treasury the same way. What the account holds past its own building target is sold to the market at its price, into the treasury.
  - **Sellers.** Anyone may sell scrap and materials to a market that lists them, at ADR 0052's discount.
  - **Imports.** A market short of a material imports it at the ceiling by ADR 0052's rule, so a plain town with no ore buys iron dear.
- **Orders.** A faction's treasury may order gear at its capital's market: a kind and a count, at most 8 orders a market, 8 bytes each.
  - An order adds its count to the kind's target until it is filled, so the workshops make it before kinds short by less.
  - A finished piece goes to the order, and the treasury pays the price into the till on delivery, from which the faction's account receives it (ADR 0051).
  - ADR 0052's purchase rule places an order for what its soldiers need and the stock cannot supply. #248's ADR 0065 spending priorities set how much the treasury orders for arms beyond that.
  - Rejected: buying from stock alone. A faction arming for war would wait on the market's 30-day demand to catch up, a month behind every decision.
- **Breaking yields scrap.** When an item breaks, it becomes a stack of its kind's scrap, as its recipe's table names it, in the same place: the slot it was worn in empties, as ADR 0050 decided, and the scrap goes into the holder's inventory, or onto the ground as a pile on its cell when the inventory has no room (ADR 0051).
  - **Metals.** A kind's scrap is its main metal, one unit of scrap for each unit of that metal in its recipe, at most one scrap kind an item. Lamellar's 6 iron and 3 leather leave 6 iron scrap.
  - **Organics.** Leather, wood and cloth leave nothing. A broken leather coat is gone, as ADR 0050 had it.
  - **Reforging** turns 2 scrap into 1 metal, with a charcoal and half a day. The other half is slag, which leaves the world. So the metal comes back at half, the PM's proposal, lost once, at the fire.
  - Rejected: scrap at half the metal, then reforged at half again. Two losses would leave a quarter, and a broken sword would weigh less than the iron in it before anyone fired it.
  - Rejected: organic parts turned to rags or fuel. A rag kind would need a use to be anything but a pile, and a sword's wooden grip as fuel is under a unit of charcoal.
- **Loot.** Scrap is an item with a price, so ADR 0051's pick-up rule wants it, and a soldier at war picks it up as loot. Its faction sells it to its capital's market by ADR 0052's rule, where the smithy reforges it, and the treasury buys back gear made from it. So a battlefield's broken swords become a faction's next swords, and a faction short of iron arms itself from the scrap its wars leave.
- **Repair.** ADR 0050's repair by sale stands. A smithy restoring a worn piece from its market's worn pool spends a share of the piece's recipe in proportion to the durability restored, rounded up, of its metal and charcoal only, and labour likewise. Restoring an iron sword from half its durability costs 2 iron, 1 charcoal and 1 day.
- **The balance.**
  - **Metal is never made except from ore.** Every metal unit comes from a vein's ore or an import. Making a piece moves metal into it, breaking moves it into scrap one for one, and reforging returns half. So each round of break and reforge halves what is left, and one unit of new iron yields gear of at most 1 + 1/2 + 1/4 + ... = 2 units over every use, and never more. A thrown dart, slag, an expired pile and an export are the sinks.
  - **The core checks it.** A recipe with no ore among its materials may not output more units of a metal than its materials hold, counting a scrap unit and a material unit as one and a piece as its scrap count. Steel's 2 iron for 1 steel passes, its other unit lost as slag. A pack breaking this is refused at load.
  - **Money.** Making adds no money: wages are a share of the price a buyer paid. Scrap and materials move silver between holders through tills, taxed as any trade. Imports of iron and leather are a sink, and exports of surplus a source bounded by what the world makes, both under ADR 0052's balance.
  - **The sum test.** A test asserts, for each metal, that the units in materials, scrap and pieces held anywhere, plus those spent by darts, lost as slag, expired on the ground and exported, less those smelted from ore and imported, are the same before and after every making, breaking, reforging, repair and sale.
- **Tiers.** Making, orders and the account's targets are daily rules, untiered (ADR 0042).
  - In live and stepped areas, items break per hit and scrap lands where its holder stands.
  - In a far area, ADR 0052 applies a fight's expected wear in aggregate; each piece that breaks there leaves its scrap with its holder in the same way, and ADR 0051's far looting gives the field's piles to the side holding it.
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Breaking** is one stack written where ADR 0050's break already writes, at most the tick's hits, about 30 in the M1 battle.
  - **The ground's bound holds.** A break replaces one stack with at most one stack, so no person ever holds or drops more than ADR 0051's 13 stacks, and the worst day stays 985,872 piles under 1,048,576.
  - **Making.** About 1,500 workshops each run at most a batch a worker a day over at most 16 kinds, about 100,000 comparisons a day, about 60 a tick spread over 1,800 ticks.
  - **Memory.** A workshop's carried batch is 4 bytes, about 6 KB. Orders are at most 8 of 8 bytes for each of 512 markets, 32 KB. Recipes are pack data, read at load.
  - **Markets' kinds.** The base pack's materials and scrap are 18 kinds, within a market's 64.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 8 materials in a recipe
  - at most one scrap kind an item, its count at most the units of its metal in the item's recipe
  - no recipe without ore outputting more units of a metal than its materials hold
  - at most 16 kinds a workshop makes
  - a recipe's labour at most 1,000 tenths
  - at most 8 orders a market
- **Saves, the hash and determinism.** Workshops' carried batches, orders and the slag and spent tallies the sum test reads are saved and hashed. Every amount is an integer, and every choice breaks ties by the earlier row, then the lower index.
- **The view.** Each material and scrap kind has a sprite in the pack's `view/` (ADR 0049), shown in the inventory, on the ground and in the market window like any kind. The treasury window lists its orders.

The implementation, in order, each a pull request with its acceptance, after ADR 0052's markets (its step 5) and ADR 0056's gathering (its step 2):
1. **Recipes and making.**
   - Acceptance, each with a control: a workshop making the kind lowest against its target with materials in stock; a batch carried over to the next day; reforging tried before smelting by the list's order.
   - Acceptance: a pack with a recipe yielding more metal than it holds refused with its `ContentRefusal`.
2. **Scrap and reforging.**
   - Acceptance, each with a control: an iron sword breaking into 3 iron scrap in the inventory, and onto the ground when the inventory is full; leather armour breaking into nothing; 2 scrap reforged into 1 iron.
   - Acceptance: the sum test for each metal over making, breaking, reforging, repair and sale.
3. **Sources.** Ore veins at the test map's mine sites, the account's targets raised by the market's shortfall, and hides into leather once #241's animals land.
   - Acceptance, each with a control: a village with a smithy gathering ore it never builds with, where one without a smithy gathers none; hides tanned into leather only where a leatherworks stands.
   - Acceptance: `make sweep` over seeds 0 to 49 for 100 years holding ADR 0052's money bands, with each metal's units in the world bounded at every decade.
4. **Orders and loot.**
   - Acceptance, each with a control: an order made before kinds short by less, where with no order the shortest kind is made first; the order delivered to the faction's account.
   - Acceptance, with a control: scrap looted, sold and reforged into gear the treasury buys, where scrap left on the ground expires.
5. **Far aggregation and the view.**
   - Acceptance, with a control: a far fight's breaks leaving scrap with the holders, the same metal's units as the live fight of the same seed.
   - Acceptance: sprites for each material and scrap kind, in captures (ko).

## Decision

Materials are stacking item kinds of the pack, each naming its metal or none: ore, wood, charcoal, iron, steel, bronze, hide, bone, horn, claw, the spirit beasts' hide, bone and claw, leather, cloth, and the scrap of each metal. Animal and spirit-beast parts are fittings on gear and break into nothing of their own, as the owner decided they are materials. Ore comes from ore veins, a natural-object kind the build tool places at the map's mine sites, gathered by ADR 0056's rule.

Every made kind carries a recipe: its workshop, its materials, its labour in tenths of a worker's day, and its output. Each day a workshop makes, of the kinds it lists, the one lowest against its market's target whose materials are in stock, carrying an unfinished batch over. A village with a market gathers for its market's shortfall as for its own building. A faction's treasury orders gear at its capital's market, which raises the kind's target until the order is filled.

A broken item leaves its slot empty, as before, and becomes scrap of its main metal, one unit for each unit of that metal in its recipe, in the holder's inventory or on the ground. Organic materials leave nothing. A smith reforges 2 scrap into 1 metal with charcoal and labour, and the other half is slag that leaves the world. Scrap is loot like any priced item. Metal is made only from ore, so scrap can never grow gear beyond twice the metal mined, and the core refuses a recipe that would.

This supersedes ADR 0050's item at no durability being gone, now scrap of its metal. It extends ADR 0052's making with recipes on item kinds, the choice by shortfall and orders, ADR 0056's natural-object kinds with the ore vein and its account's targets with its market's shortfall, and ADR 0047's map data with mine sites.

## Consequences

- Gear has a chain behind it: a village's ore and wood, a smithy's labour, a market's price. A town with no mine pays the ceiling for iron, and its soldiers go less well armed.
- Battlefields are worth walking back to. Scrap piles from a battle feed the winner's smithies.
- Smithies, leatherworks and tailors become steady employers, so a town's workshops grow with its wars.
- Leather and cloth come only by import until #171's game and a fibre crop exist, a cost to every town that the sweep shows.
- The sum test and the core's recipe check keep metal bounded, whatever a pack's recipes say.
- The market window lists materials and scrap, which adds 18 rows to a smithy town's market.

## Signs the decision was wrong

- Iron runs out across the map within decades, because veins reopen too slowly for the sweep's wars, and every faction's soldiers fight bare-handed.
- Scrap piles outnumber everything else on the ground after battles, so ADR 0051's piles fill toward their bound.
- Smithies make only charcoal and iron and never gear, because the shortfall rule always ranks materials lowest, so a market's targets for materials need to be lower.
- Recycling makes iron so cheap that mines are never gathered, so the slag share needs to rise.
- Players find the chain invisible, so recycling adds rules without anything they notice, and the owner removes it.
