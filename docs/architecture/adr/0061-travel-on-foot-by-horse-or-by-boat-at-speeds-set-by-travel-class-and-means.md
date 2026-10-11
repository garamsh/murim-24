# ADR 0061: Travel on foot, by horse or by boat, at speeds set by travel class and means

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #207, a follow-up named in #206: at 4,096 cells, a li a cell in the heartland (ADR 0047), walking alone makes long journeys tedious, and the jianghu travels by road, horse and river. The issue asks for these:
- **Travel classes and means as rules data.**
  - Classes: open ground, road, highway, river by boat, and a ferry crossing.
  - Means: foot, horse and boat, with speeds and costs.
  - One rule for the player and NPCs: caravans, armies and couriers use them too.
- **Horses:** owned, bought or kept by villages and sects; mounting and dismounting; and combat while mounted, stated or forbidden.
- **Boats:** river and canal routes, the Yangtze and the Grand Canal, with stops at towns and ferries at crossings. The Three Gorges are a fast route downstream and a slow one up.
- **ADR 0047's route-time table** recomputed with each means.

`concept.md` §World and scale agrees that roads are faster than open ground, and that horses and river boats are planned.

The owner decided on 2026-10-11, in #255, that the world becomes vaster, so travel takes at least twice as long as on ADR 0047's map. The PM ruled on this PR that the owner's "twice as long" applies to travel on foot: horses and boats keep their ratios to walking, which makes them worth more.

Facts from `main` at `1b18c01`:
- **Travel today** (ADR 0047).
  - Each ground kind has a travel class as data. A step costs 5 and 7 on open and rough ground, 4 and 6 on roads and bridges, and 10 and 14 on fords.
  - A person moves at `SPEED`, 16 units a tick, scaled by 1.25 on roads, 0.5 on fords and 1 elsewhere: 1.875 cells a second at 1x.
  - Roads are polylines with a class of highway, road or track. Rivers carry their direction of flow and whether boats use them.
  - A ferry is a pair of landings with a crossing time, by one rule for everyone.
  - As starting data, ADR 0047 gives a horse 2.0 on open ground and 2.5 on roads, and a boat 2.5 downstream and 0.8 upstream on rivers marked for boats.
- **Items and holders** (ADRs 0050 to 0052).
  - A person wears four slots and carries 8 stacks. Every item has one holder and moves by four verbs.
  - Markets sell what their pack lists. Merchant houses run caravans as armies along roads.
  - Money is copper.
- **Paths** (ADR 0043). A coarse graph of areas, with fields shared by area and target, and a ferry as an edge at its crossing time.
- **Tiers** (ADR 0042). Far armies move on the coarse graph, and #216's live tier holds 5,000 at a bound measured between 10,000 and 50,000 (ADR 0048).

Options weighed:

- **Classes and means, as data.** A travel class is a ground kind's, as ADR 0047 gave it. The pack gives each means a speed per mille on each class:

  | Class | Foot | Horse | Boat |
  |---|---|---|---|
  | Open | 1,000 | 2,000 | — |
  | Rough | 1,000 | 1,000 | — |
  | Track | 1,100 | 2,200 | — |
  | Road and bridge | 1,250 | 2,500 | — |
  | Highway | 1,250 | 2,750 | — |
  | Ford | 500 | 750 | — |
  | River, downstream | — | — | 2,500 |
  | River, upstream | — | — | 800 |
  | Canal | — | — | 1,500 |

  - A step's cost in a field is the class's cost over the means' speed, so fields per means prefer roads more strongly for horses than for walkers.
  - ADR 0037's diagonal speed scales the same way, so a step costs the same either way.
  - A highway is ADR 0047's road class of highway: no faster on foot, and faster for a horse. A track is its third class.
  - **Independent of the map's size.** Speeds are per mille of `SPEED` on a cell of the map as built. They do not change when #255's design changes the cells' count or the distance a cell stands for, so walking takes whatever time the built map's lengths give it, and a horse or a boat its ratio of that.
- **Roads are read, not decided.** This ADR reads each cell's road class and decides nothing about how roads come to be. They are the map's (ADR 0047), and they appear where people walk as villages grow, by the owner's decision ADR 0056 records and #169's village growth. #248's ADR 0065 leaves road spending out of a faction's priorities, so any rule that builds roads will be its own, and this ADR reads whatever it lays.
- **Horses.**
  - **An item.** A horse is an item kind of the family "mount" (ADR 0050), held in a fifth slot, the mount, beside the four. Its durability is its condition. It loses 1 for every 100 cells ridden, a horse lasting 10,000 in the base pack, and a horse at 0 dies and is gone.
  - **Getting one.** Horses are bought at a market whose pack lists them, at a town with a stable (ADR 0052), for 30 silver in the base pack. Villages and sects keep horses in their accounts (ADR 0051) and issue them, as gear is issued, to the soldiers and members whose roles need one: soldiers sent to war, caravan guards and couriers.
  - **Upkeep.** A horse costs its holder's household fodder, a ration a day at the granary's price (ADR 0052). That is a money sink that grows with horses. A household that cannot pay sells the horse before it goes hungry, in ADR 0052's order of selling.
  - **Mounting.** `mount` and `dismount` are orders through the port for anyone, each taking 30 ticks. A person mounts only a horse in its own mount slot. A rider's speed is its horse's on each class, and its body is drawn on the horse.
  - **Combat while mounted.** A rider strikes with its weapon's basic strike (ADR 0050), and casts no art skill. A cast order dismounts it first, taking the 30 ticks. A rider is struck as anyone is. Mounted charges and horse archery wait for an art that needs them.
  - **A rider's death.** The horse drops where it falls, as a pile (ADR 0051), and anyone may pick it up.
  - **Riders and cliffs.** A horse cannot leap (ADR 0058), so the horse's field has no leap edges, and a rider goes round by road wherever a way round exists. A boat is not a means on cliffs.
    - Where its objective lies only across a leap edge, a rider dismounts at the edge, in the 30 ticks of `dismount`, and tethers its horse there. The horse stays in its mount slot, marked tethered at that cell, 4 bytes, and the rider leaps and walks on.
    - Back within a cell of the tether, `mount` rides it again. A tethered horse is struck and killed as anyone's, and dropped as a pile if its holder dies.
    - A walker's path is unchanged.
  - **NPCs.** An army whose members all ride moves at a horse's speed. Otherwise it moves at its slowest member's, so a mixed band walks. People no driver steers mount when given an objective over 64 cells away, and dismount within 8 cells of it or when a foe is in sight.
  - Rejected: horses as a person's property outside the item model. A horse would then need its own owner, sale and loss rules beside items'.
- **Boats.**
  - **Routes.** A boat route is a pack list of landings along a river or canal marked for boats: the towns on it, and ferries' landings where they lie on it.
  - **A boat.** A boat is a vehicle owned by a merchant house or a village (ADR 0051's accounts). It has a capacity of 20 passengers and 32 cargo lots, and a route it sails back and forth.
  - **Sailing.** It moves along the river's cells at the boat's speed for its direction, waits a quarter of a day at each landing, and carries its passengers and cargo as an army carries a roster. Passengers neither strike nor are struck while aboard.
  - **Boarding** is an order through the port for anyone at a landing, bound for a later landing on that route. It pays the owner a fare of 1 copper for each cell to that landing, refused when the boat is full, or the purse short. The passenger steps off at its landing.
  - **NPCs.** Paths take a boat's legs as edges of the coarse graph between landings, at the sailing time plus half the wait. People no driver steers board when that path is shorter. Caravans load cargo onto boats on their routes, paying a fare by lot.
  - **The Three Gorges** are a river leg like any other: downstream at 2,500 and upstream at 800, so the gorges take about a third of walking's time down and three times a boat's down time up, on the map as built. On ADR 0047's 4,096 cells that was 5.3 minutes down and 16.7 up.
  - **Ferries** stay ADR 0047's rule, with a fare of 20 copper a crossing paid to the village holding the landing. A person who cannot pay waits, so it does not cross. NPCs' paths count the fare as a cost only for those who can pay.
  - Rejected: boats as a faster ground class on water cells for anyone. Every person would then swim at a boat's speed, and nothing would own or pay for a boat.
- **Couriers** do not exist yet, and are named so their means is decided: a courier is a person riding a horse on an objective, under these rules.
- **The route-time table, recomputed** from ADR 0047's lengths at its 1.875 cells a second for walking, minutes at 1x. It is a reference on ADR 0047's 4,096 cells. On the map #255 builds, walking takes at least twice these, and the other columns keep their ratios to walking. Steps 3 and 4 measure each route as length over speed on the map as built:

  | Route | Length (cells) | Foot, open | Foot, road | Horse, open | Horse, road | Boat |
  |---|---|---|---|---|---|---|
  | Luoyang–Chang'an, by Hangu and Tong | 650 | 5.8 | 4.6 | 2.9 | 2.3 | — |
  | Luoyang–Kaifeng | 340 | 3.0 | 2.4 | 1.5 | 1.2 | — |
  | Luoyang–Song Shan (Shaolin) | 90 | 0.8 | 0.6 | 0.4 | 0.3 | — |
  | Chang'an–Hua Shan | 210 | 1.9 | 1.5 | 0.9 | 0.7 | — |
  | Chang'an–Chengdu, by Jianmen | 1,220 | 10.8 | 8.7 | 5.4 | 4.3 | — |
  | Luoyang–Taiyuan, by Mengjin and a Taihang pass | 720 | 6.4 | 5.1 | 3.2 | 2.6 | — |
  | Taiyuan–Yanjing, by Jingxing | 870 | 7.7 | 6.2 | 3.9 | 3.1 | — |
  | Yanjing–Liaoyang, by Shanhai | 820 | 7.3 | 5.8 | 3.6 | 2.9 | — |
  | Luoyang–Wuchang, by Nanyang | 1,100 | 9.8 | 7.8 | 4.9 | 3.9 | — |
  | Xiangyang–Wudang | 220 | 2.0 | 1.6 | 1.0 | 0.8 | — |
  | Chongqing–Wuchang, by the Three Gorges | 1,500 | 13.3 | 10.7 | 6.7 | 5.3 | 5.3 down, 16.7 up |
  | Wuchang–Nanjing, along the Yangtze | 880 | 7.8 | 6.3 | 3.9 | 3.1 | 3.1 down, 9.8 up |
  | Kaifeng–Yangzhou, by the Huai fords | 1,070 | 9.5 | 7.6 | 4.8 | 3.8 | — |
  | Nanjing–Suzhou–Hangzhou | 620 | 5.5 | 4.4 | 2.8 | 2.2 | — |
  | Chang'an–Tianshan, by Kongtong, Liangzhou and Jiayu | 2,160 | 19.2 | 15.4 | 9.6 | 7.7 | — |
  | Chengdu–Dali | 1,000 | 8.9 | 7.1 | 4.4 | 3.6 | — |
  | Wuchang–Guangzhou, by Changsha and Meiling | 1,370 | 12.2 | 9.7 | 6.1 | 4.9 | — |

  - The highway's 2,750 for a horse shortens the "horse, road" column by a tenth where a route is highway. The map's content decides which legs are.
  - Waits at landings and ferries are not in the table.
  - A test on the built map measures each route by each means, as the issue asks.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 8 means
  - a speed of at most 4,000 per mille
  - at most 1,024 boats
  - at most 64 landings on a route
  - at most 512 landings in all
  - a fare of at most 2^20 copper
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Memory.**
    - A person gains its mount slot, 8 bytes, its means, a byte, and a tether cell, 4 bytes: 650 KB at 50,000, 65 KB at the live tier's 5,000, and 6.5 KB at 500.
    - A boat is about 128 bytes: route, position, direction, wait and the roster's head. Its passengers are on its roster and its cargo in lots, so 1,024 boats are 128 KB plus their lots.
    - The landings' graph edges are at most 512 landings' worth.
  - **Fields.** A field is built per means as well as per area and target. Only foot and horse walk the ground, so at most twice ADR 0043's fields, and only where riders go. A terrain change rebuilds both kinds of field for its areas from ADR 0057's one shared queue, at 8 areas a tick, so the per-means fields add no queue of their own.
  - **At 500 and at 5,000 living,** the boats and landings are the same, and riders are a share of the living: a few dozen at 500, a few hundred in the live tier.
  - **Movement.** A rider's step reads one speed from a table instead of one constant.
  - **Boats.** A boat's advance is one counter a tick. At 1,024 boats that is 1,024 counters a tick, a few microseconds.
  - **Decisions.** NPCs' mount decisions are made when an order is given, not each tick.
- **Tiers.** Far armies and boats move on the coarse graph at their means' costs, a boat's edge at its sailing time. Mounting is instant in far areas.
- **Saves, the hash and determinism.** Mount slots, means, boats, routes and fares are saved and hashed. Speeds are integers per mille, and boats advance in index order.

The implementation, in order, each a pull request with its acceptance, after ADR 0047's map and ADR 0050's items are built:
1. **Means and speeds.** The table as data, and fields per means.
   - Acceptance, each with a control: a horse on a road at 2.5 times walking pace, where a walker is at 1.25, and on rough ground at 1; a diagonal costing the same either way.
   - Acceptance: `make bench` within budget with fields per means.
2. **Horses.** The mount slot, buying, issue, upkeep, `mount` and `dismount`, mounted strikes, tethering at a leap edge, and NPCs mounting for long objectives.
   - Acceptance, each with a control: a cast dismounting; a dead rider's horse dropped; a mixed band at its slowest; a rider's path over a leap edge dismounting and tethering, where a walker's path is unchanged; a rider going round by road where a way exists.
   - Acceptance: `make sweep` within ADR 0052's bands, since fodder is a money sink.
3. **Boats and ferries' fares.** Routes, boats, boarding and fares, boats as graph edges, and caravans by boat.
   - Acceptance, each with a control: a short purse refused at a ferry where a full one crosses; a full boat refusing a passenger where one with room boards; the Three Gorges taking length over speed down and up on the map as built, downstream at 2,500 and upstream at 800.
   - Acceptance: `make bench` within budget with the boats.
4. **The route-time test.** Every route measured by each means on the map as built, within a tenth of its length over its speed, with walking at least twice ADR 0047's time.
   - Control: the same route on ADR 0047's lengths giving the reference table.

## Decision

Speeds are pack data per means and travel class: foot, horse and boat, with ADR 0047's starting values, a highway and a track, and fields per means. A horse is an item in a fifth slot, the mount. It is bought at towns with a stable, issued by villages and sects to soldiers, guards and couriers, wears with distance, and costs a ration of fodder a day. Mounting and dismounting take 30 ticks. A rider strikes with its weapon's basic strike only, and a cast dismounts it.

Boats are vehicles owned by houses or villages, sailing pack routes of landings at 2,500 downstream, 800 upstream and 1,500 on canals. Anyone at a landing boards for a fare of a copper a cell, and boats' legs are coarse-graph edges. Ferries keep ADR 0047's rule with a fare of 20 copper. Caravans, armies and couriers use the same rules. The route-time table is recomputed for every means.

Speeds are per mille on a cell of the map as built, so they hold whatever size #255 gives the map; walking takes at least twice ADR 0047's time there and the other means keep their ratios. Roads are read, not decided. A horse cannot leap: a rider goes round by road, or dismounts and tethers its horse at a leap edge.

This supersedes ADR 0047's single speed for every person on a class, now a speed per means, and its horses and boats left to a follow-up. It also supersedes ADR 0050's four slots, now five with the mount.

## Consequences

- Long journeys take a third to half the time by horse, and rivers become highways for those who can pay, so wealth buys distance.
- Horses are a steady expense, a sink ADR 0052's balance gains.
- Cavalry moves armies fast but fights only with basic strikes, so a mixed force must choose between speed and arts.
- Fields double where riders go.
- Saves grow by about 650 KB at the bound, plus boats.

## Signs the decision was wrong

- Riders cross the map so fast that the map feels small, which the owner names.
- Bands of mixed walkers and riders crawl at their slowest so often that horses never speed an army.
- Boats crowd one landing, and their passengers jam the town around it.
- Fodder empties poor households' purses, so horses are kept only by the rich and soldiers, and the roads stay empty of riders.
