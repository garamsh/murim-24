# ADR 0067: Let animals arise where food feeds them, hunt them for meat and materials, and let spirit beasts hold territories

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-10, in issue #171's first comment:
- **Animals arise only where they can live.** Grazers appear where grass or forage grows, on #220's natural slots, and predators where prey lives. No animal appears where its kind cannot live: no fixed spawn points, and none in deserts without forage or in towns. Grazers crop forage, which regrows by #220's rules, and predators hunt grazers. Their numbers follow food, as #229 bounds people by food: a balance of predators, prey and forage, not a cap.
- **Spirit beasts (영수).** Rare, strong animals hold a territory and defend it. They attack anyone entering it and do not roam beyond it. On death they leave a spirit core (영단), a cultivation item consumed or refined for qi or realm progress, and materials such as hide, bone, horn and claw, worth more than an ordinary animal's. A spirit beast is reborn or replaced only by the same food and territory rules, so killing one leaves its territory empty for a while.
- **Every animal leaves materials when killed,** by species as pack data (ADR 0049). Meat is food: it feeds the hunter's household and counts toward #229's food. Hide, bone and horn feed #245's crafting.
- **Hunters are a trade (#170),** and NPCs hunt for food and materials as the player does.

The owner decided on 2026-10-10, in the issue's second comment:
- **Animals arise naturally, with no breeding.** An animal appears on its own where its kind can live, at a rate set by the food there: forage for grazers, prey for predators. Animals do not mate or raise young.
- Numbers stay bounded by food: where forage is grazed down or prey is hunted out, fewer appear, and none appear where nothing feeds them.
- Spirit beasts arise the same way, in a suitable territory left empty.

The owner decided on 2026-10-11, in the issue's third comment:
- **Many kinds of creatures,** a wider bestiary as pack data: grazers, browsers, small game, predators of several sizes, birds and fish where cheap, and spirit beasts.
- **Herds.** Some kinds move and graze in groups, deer, boar, wild horses and wolves in packs. A group moves as one by simple flocking, cohesion, alignment and separation, behind a leader or centre that follows forage.
- **Prey flees predators.** Grazers that see a predator run away from it, toward their group or cover. Predators hunt, chase and give up after a while, on a chase budget.
- **An ecosystem that balances,** forage to grazers to predators, bounded by food (#229) with no caps, over 100 years in the sweep: no extinction everywhere and no explosion.
- **Some intelligence.** Animals avoid predators and avoid larger groups, giving way when outnumbered, by the same threat appraisal people use (#256).

The issue asks the design to state:
- the rules as core rules, with the slots and bytes they cost
- how animal counts stay bounded by food over 100 years, as a sweep measure
- what the spirit core does, for the player and for NPCs
- how a drop reaches crafting

What this builds on, from `main` at `c86b08d` and the open PRs it cites:
- **ADR 0056,** proposed, merged in #220. Natural objects are slots the map's build tool places, at most 64 an area and 1,048,576 in all, each a kind and a stage with a timer, 2 bytes at run time. Every 8 days each area's slots advance together, on the area's own tick of the cycle, about one area a tick. A stump regrows as a sapling, and a sapling grows through growing to grown in its kind's growing days. Spread draws from `slot_stream`, a counter-based stream of the world's seed, the area, the cycle and the slot. Villages assign idle earners to gather, nearest first, while fields keep at least half the earners. ADR 0056 left hunting to this issue, as "gathering from a slot that moves".
- **ADR 0063, proposed in the open PR #243** for #229. Population is bounded by the food that can be had. Births follow a household's provision. The living bound is a technical capacity, and reaching it is a failure that `make sweep` and `make bench` report. It leaves the form of game and fish to #171's design, and takes their rates and yields as levers.
- **ADR 0064, proposed in the open PR #245** for #231. Materials are stacking item kinds. Hide is a material whose source is game once #171 lands, tanned into leather at a leatherworks. Workshops make what their market lacks, by recipes in the pack.
- **ADRs 0050 and 0051.** Items are stacks of pack kinds in families, food and material among them. Every item has one holder: a person, an account's lot at a warehouse, or a pile on the ground, which expires after 30 days. People pick up piles they want.
- **ADR 0052.** A person eats a measure a day. The granary is a market of food, and fruit and berries are sold to it as measures (ADR 0056). Households have purses and accounts.
- **ADRs 0035, 0040 and 0055.** A person's qi store grows daily by its cultivation, 0 to 2 qi a day in the region, up to a limit its talent sets, 1,000 to 15,000. Realms are thresholds on the store, third-rate at 1,000. A ware restores health or qi when consumed, by the `consume` order.
- **ADR 0046.** A skill is a shape resolved toward a point. A person no driver steers engages a foe within sight. A newborn has 120 health.
- **ADRs 0042 and 0048.** People are simulated tick by tick in live areas, coarser in stepped and far ones, and daily rules are untiered and spread over the day's ticks.
- **ADR 0032.** The event ring records deaths by wound, age and hunger.
- **Not built.** Trades are #170's.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000.

Options weighed:

- **Animals as records, not slots.** An animal is a record in a store of its own, apart from people and from slots:

  | Field | Bytes |
  |---|---|
  | position, in sub-cell units as a person's | 8 |
  | home area | 2 |
  | herd, the index of its group, or none | 2 |
  | kind | 1 |
  | state: grazing, fleeing, chasing, fighting or home | 1 |
  | health | 2 |
  | hunger, in cycles unfed | 1 |
  | chase budget left, in tens of ticks | 1 |
  | the tick its strike is ready | 4 |
  | the person or animal it chases or fights, if any | 4 |
  | reserved | 2 |
  | **Total** | **28** |

  - Animals are kept by home area, in an index of 4 bytes an animal, so a rule reads one area's animals without a search.
  - Chosen. Animals move, flee and fight, which a slot fixed to a cell cannot, and they need none of a person's genealogy, purse or roster.
  - Rejected: game as slot kinds, as ADR 0056 first expected. A slot cannot be chased, cannot cross an area, and cannot defend a territory, so predators hunting grazers and spirit beasts attacking intruders would need a second form anyway.
  - Rejected: animals as people of a faction of beasts. Every person carries kin, talent, a household and a vote in its faction's rules, all meaningless for a deer, and every rule that reads people would need an exception.
- **The capacity.** The store holds at most 262,144 animals, 7 MiB, with its index 1 MiB more. It is a technical capacity, as #243's ADR 0063 makes the living bound: an appearance past it is refused and counted in a 4-byte counter, saved and hashed, and `make sweep` and `make bench` fail on any refusal.
  - **Its basis.** Grazers are bounded by forage, at most one for 3 forage slots (below). If the base pack's forage kinds take a quarter of ADR 0056's 1,048,576 slots, that is at most about 87,000 grazers, and predators at most a sixth of them, about 15,000, so about 102,000 in all. The base pack keeps the peak at three fifths of the capacity or less, about 157,000, which leaves room for a mod with richer forage.
  - Rejected: a capacity sized for every slot as forage, about 350,000 grazers and 58,000 predators. At three fifths of it that is about 680,000 records, 16 MiB, for a map no pack paints.
- **Forage, on ADR 0056's slots.** Grass is a new slot kind of the base pack. Its biomes are plain 6, hills 4 and forest 2, it grows in 16 days, and its stump regrows in 8. A kind is forage when the pack marks it so; bushes are not, since people gather their berries.
  - **Grazing is a slot-stage change.** At its area's cycle, each grazer crops one grown forage slot of its home area, the lowest index first, setting it to stump. A cropped slot is a sapling 8 days later and grown 16 days after that, so it feeds again after 3 cycles.
  - **So a grazer needs 3 forage slots.** It eats one a cycle, and each slot feeds once in 3 cycles. That is the `forage_per_grazer` of the base pack, pack data.
  - **Its share of the slots.** The build tool places grass and undergrowth among an area's 64 slots by the same affinity draw as every kind (ADR 0056). In the base pack:
    - a plain area weighs grass 6 against tree 2, fruit tree 3, bush 4 and covey 2, 17 in all, so about 23 of its slots are grass
    - a hills area weighs grass 4 and undergrowth 1 against tree 3, bush 2, rock 6 and herb 2, 18 in all: about 14 grass and 4 undergrowth
    - a forest area weighs grass 2 and undergrowth 6 against tree 10, fruit tree 2 and herb 3, 23 in all: about 6 grass and 17 undergrowth
  - Grass costs no storage beyond its slots, and the map is rebuilt for it, as ADR 0056 says any change to natural-object kinds requires.
- **Kinds.** Animal kinds are pack data: a name, a role (grazer, predator or spirit beast), the biomes it lives on with a weight each, its health, its speed, its strike, the kinds it preys on, and what it drops. The base pack's:

  | Kind | Role | Lives on | Health | Walk | Run | Sight, open / forest | Strike | Drops |
  |---|---|---|---|---|---|---|---|---|
  | Deer | grazer, herds of 3 to 8 | plain 4, forest 3, hills 3 | 60 | 16 | 32 | 10 / 4 | 3 | 30 meat, 1 hide, 1 horn |
  | Boar | grazer, herds of 2 to 6 | forest 4, hills 3 | 120 | 14 | 28 | 6 / 3 | 10 | 50 meat, 1 hide, 1 bone |
  | Wolf | predator of deer, roe and boar, packs of 2 to 6 | forest 3, hills 4 | 90 | 16 | 38 | 10 / 5 | 8 | 10 meat, 1 hide, 1 bone |
  | Tiger | predator of deer, roe, boar and wild horse, alone | forest 4 | 240 | 16 | 40 | 8 / 4 | 20 | 30 meat, 2 hide, 2 bone, 1 claw |
  | Hare | grazer, small game, alone | plain 4, hills 3 | 10 | 18 | 36 | 6 / 3 | 0 | 4 meat, 1 hide |
  | Wild horse | grazer, herds of 4 to 12 | plain 5 | 150 | 18 | 44 | 12 / 5 | 6 | 60 meat, 2 hide, 1 bone |
  | Roe | browser, herds of 2 to 6 | forest 5 | 50 | 16 | 34 | 8 / 4 | 2 | 20 meat, 1 hide, 1 horn |
  | Fox | predator of hare, alone | plain 3, forest 3 | 30 | 16 | 40 | 8 / 4 | 4 | 4 meat, 1 hide |
  | Spirit tiger | spirit beast, predator of deer and boar | forest 4, hills 2 | 4,000 | 16 | 44 | 12 / 6 | 40 | 1 spirit core, 4 spirit hide, 4 spirit bone, 2 spirit claw, 40 meat |

  Speeds are in sub-cell units a tick, a cell being `CELL_SIZE`, 256 units (`core/map.hpp`), beside a person's `SPEED` of 16, 1.875 cells a second. An animal walks at its walk speed and runs, fleeing or chasing, at its run speed: a deer at 32, 3.75 cells a second, and a wolf at 38. Sight is in cells, the shorter where the watcher stands on forest. Strike is the damage of the kind's strike, which with health gives its strength. Meat is a food kind, a measure a unit. Hide, horn, bone, claw, spirit hide, spirit bone and spirit claw are #245's material kinds.
  - **Browsers** feed on undergrowth, a second forage kind of the base pack: forest 6, hills 1, growing in 16 days, its stump regrowing in 8. A kind names the forage kinds it eats, so roe crop undergrowth and deer and wild horses grass. Undergrowth is placed by the build tool as grass is.
  - **Small game** is a grazer alone, light and fast, eaten by small predators: hares and foxes run the same rules at a smaller scale.
  - **Birds and fish** are not records. They are slot kinds of ADR 0056, a shoal on water cells and a covey on plain, weighing 2, gathered by the gathering order as fruit is, since chasing them would add cost and nothing a rule reads. This is the owner's "where cheap".
  - The kinds above are the base pack's. A pack adds kinds as rows, within the bounds below.
- **Appearance.** At each area's cycle, after grazing, one draw for each role, from `animal_stream`, a counter-based stream of the world's seed, the area's index, the cycle's number and the role. Like `slot_stream`, no other rule's draws shift it, and no order of visiting matters.
  - **Where nothing appears.** No animal appears in an area holding any structure of a village, so none appear in towns. An area whose biomes no kind lives on, or with no forage, has no grazers, and so no predators.
  - **Grazers, at a rate set by the spare forage.** The forage supports S grazers, the area's forage slots that are not empty divided by `forage_per_grazer`, and G graze there. One grazer appears with a chance of 250 × (S − G) / S per mille, rounded down, and none while G is at least S. So an area with all its forage to spare fills at 250 per mille a cycle, one half grazed at 125, and one at its support not at all. The kind is drawn by its weight on the area's biomes, at a forage slot's cell.
  - **Predators, at a rate set by the spare prey.** The area and its 8 neighbours hold P grazers of a predator kind's prey, which support Q = (P − 6) / 6 predators, rounded down: 6 grazers each, above a reserve of 6. With R predators there, one appears with a chance of 100 × (Q − R) / Q per mille, rounded down, and none while R is at least Q. The kind is drawn by its weight on the area's biomes.
  - **Spirit beasts.** Below.
  - So the rate follows the food: much to spare, many appear; little, few; at the support, none. A grazed-down or hunted-out area fills again only as fast as its food allows, and an area that feeds nothing stays empty, as the owner decided.
  - Rejected: a flat chance below a ceiling. Food would then be a gate and not a rate, and an area one grazer short of its support would fill as fast as an empty one.
  - Rejected: breeding, which the owner ruled out.
  - Rejected: spawn points placed by the map. The owner ruled out fixed spawn points, and they would refill a hunted area whatever its food.
- **Hunger.** A grazer that finds no grown forage of its kinds to crop at its cycle gains a cycle of hunger, and loses it when it eats. A predator must eat every second cycle, and gains a cycle of hunger when it has not killed since its last meal. A grazer dies at 2 cycles of hunger and a predator at 3, and leaves nothing.
- **Herds.** A kind may herd, with a size range as pack data. A herd is a row of a herd table, 8 bytes: its kind, its leader's index, its count and its home area.
  - **Joining.** A herding animal that appears joins the herd of its kind in its area with the fewest members below the kind's most, or founds one. A herd at its most splits when a new member would join, the newer half following a new leader.
  - **The leader** is the member of lowest index. It walks to the next grown forage slot of the herd's kinds, so the herd follows forage. When it dies the next lowest leads.
  - **Flocking,** in live areas, for each member each tick, in integers:
    - **cohesion:** a pull toward the herd's centre, the mean of the members' positions, when farther than 4 cells from it
    - **alignment:** the leader's heading, at half weight
    - **separation:** a push from any herd mate nearer than a cell
    - The sum is clamped to the kind's speed. Herds are at most 12, so a member reads at most 11 mates, and needs no other search.
  - Packs of predators herd by the same rule, behind a leader that hunts.
- **Flight and the chase.** In live areas, each tick:
  - **Prey sees its predators.** A predator stalks at its walk toward its prey. When it first comes within the prey's sight, a draw from `animal_stream` sets the distance at which the prey notices it, a whole number of cells from 1 to that sight, so a stalker sometimes comes close. A grazer that `sees` (ADR 0058) a predator of a kind that eats it within that distance flees: it runs away from the predator toward its herd's centre, or toward the nearest forest cell when alone, and its herd's leader flees with it, so the herd turns as one. It grazes again 60 ticks after it last saw one.
  - **A predator hunts** when hungry, from its cycle until it eats: it takes the nearest prey it sees and chases it at its run speed, the chase budget counting down from its kind's, 300 ticks, 10 seconds, in the base pack. Reaching its prey within an arm's length, it strikes by its kind's strike, and a kill is its meal. With the budget spent it gives up and rests 600 ticks, then hunts again. A pack chases the leader's target together.
  - **The kill** removes the prey and drops nothing to people: the predator has eaten it. A predator struck by a person fights back until the person or it is 8 cells away.
  - **Speed decides the chase.** In a budget of B ticks a predator gains C = (its run − the prey's run) × B / 256 cells. It catches its prey when C reaches the distance at which it was noticed, less the arm's length of a cell.
    - A wolf on a deer: (38 − 32) × 300 / 256 = 7.0 cells, so it catches a deer that noticed it 8 cells away or nearer: 8 of the 10 draws on open ground, 800 per mille, and every draw in forest, where the deer sees 4.
    - A tiger on a wild horse: 40 against 44 never gains, so it catches only a horse that noticed it within its arm's length, the pounce: 1 draw in 12 on open plain, 1 in 5 in forest. Tigers ambush in forest.
    - A fox on a hare: (40 − 36) × 300 / 256 = 4.7 cells, 5 of 6 draws on open ground.
- **Avoiding the stronger.** Animals give way by the threat appraisal people use, #256's, which the core worker designs as ADR 0071. This ADR gives it the animals' inputs:
  - **A side** is a herd, a pack or a lone animal.
  - **Strength** is the side's summed health × strike, by its kind's data. A grazer's strike is its kick or tusk, a small value in the kind's row.
  - **Temperament** is a kind's, pack data: timid for hares, deer and horses, wary for boar and wolves, bold for tigers and spirit beasts.
  - So a lone wolf gives way to a herd of six boar, a pack keeps clear of a band of armed people, and grazers keep away from a group of people larger than their herd. A spirit beast holds its territory whatever it faces, as the owner decided.
  - Until #256's appraisal is built, an animal flees only predators that eat it and people within 4 cells, as step 4 orders it, and the same step hooks the appraisal in once #256 lands.
- **In stepped and far areas** animals stand where they last were, herds included, and the cycle settles what the chase does live: a hungry predator kills one prey of its kinds in its area or the 8 around it, the lowest index first, with the chance the live chase gives: of the prey's sight S in the area's main cover, the share of draws from 1 to S that the chase reach C covers, min(S, ⌊C⌋ + 1) / S, or 1 / S when C is below 0, at most 900 per mille. A wolf on a deer on open ground kills at 800 per mille a cycle, in a forest area at 900; a tiger on a wild horse at 83 on plain.
  - The two forms are one rule at two grains: the far chance is the live chase's own odds over the same draw, so a live area's kills over many cycles match it, which step 2 measures.
  - Rejected: predation settled only at the cycle, in every tier. The owner asked for prey that flees and predators that chase and give up, which only a live chase shows.
- **Where the balance comes from.** Each grazer needs 3 forage slots, so grazers never outnumber their forage for long: past it they starve. A predator eats one grazer every 16 days at most, and grazers refill at most one an area every 32 days on average, so an area and its neighbours, 9 areas, refill about 4 grazers in the 16 days 1 predator eats 1. Predators past a sixth of the grazers around them find none to kill and starve. Fleeing and herding lower the predators' success, so their share settles lower than the cycle's arithmetic alone gives, which the sweep's bands bound. Hunting by people takes from the same refill.
- **Movement.** In live areas, animals move tick by tick, straight toward a cell and stopped by blocked footing, within their home area and its 8 neighbours. They walk on no flow field. A grazer a person approaches within 4 cells, or that a strike hits, flees as from a predator.
- **Strikes on animals.** A strike's shape hits the animals in it as it hits people (ADR 0046), from the area index. The steered body may take an animal as a cast's target. An animal's health falls by the same damage rule, and at 0 it dies.
- **Drops.** A killed animal drops its kind's stacks as piles at its cell (ADR 0051). The nearest person with room picks them up by ADR 0051's rule, as it picks up anything with a price.
- **Hunting, by NPCs as by the player.** Hunting is a livelihood, as ADR 0052 lists "selling game" among earnings. It is not paid by a treasury, as ADR 0056's gathering is.
  - **Who hunts.** Each day, earners of a village with no other work, after ADR 0056's gatherers and builders, are assigned to hunt while an animal of a kind they may hunt stands within 32 cells of the village's centre: at most a fifth of the earners, with gatherers, builders and hunters together at most half, so fields keep half.
  - **What they may hunt.** Any grazer or predator. A spirit beast only by a person whose realm's rank is at least its kind's rank, 5 in the base pack for the spirit tiger. Rank 5 is beyond peak (`core/region.cpp`'s realms), at a store of 15,000, which only a person of the highest talent reaches. So almost no NPC ever hunts one: the masters of the great sects, and the player once it is strong enough. Spirit beasts stay rare by that too.
  - **How.** A hunter takes the nearest such animal by index as its objective, closes on it, and strikes with its skills, as it would engage a foe. It picks up the drops and walks home.
  - **Meat.** Meat a hunter brings home goes into its household's account. Its household eats from it before it buys rations, a measure for a member's day. Meat above 30 days of its members' rations is sold to the village granary at its price, into the household's purse. So meat feeds the hunter's household first, as the owner decided, and the rest joins the village's stock, which #243's ADR 0063 counts as food.
  - **Materials.** Hide, horn, bone and claw are sold to the village's market, or the nearest within 48 cells, into the household's purse. A market buys anything with a price (ADR 0052).
  - **The player** hunts by striking. Its drops are its own, to eat, to sell or to keep.
- **How a drop reaches crafting.** A market's stock of hide feeds #245's leatherworks, which tans 2 hides into 1 leather. Bone, horn, claw and the spirit parts are material kinds #245's ADR 0064 defines, by the owner's decision on #231, with its recipes: bone-studded leather armour, a horn-hilted saber, a horn-capped staff, claw gauntlets, spirit lamellar and spirit claw gauntlets. Workshops make them when their market lacks them, by #245's rule, so spirit-beast gear appears only where spirit beasts have been killed.
- **Spirit beasts.**
  - **Territory.** A spirit beast's territory is the disc of its kind's radius, 12 cells in the base pack, around its home cell. Its home cell is the forage slot where it appeared.
  - **Where one appears.** At an area's cycle, one draw for each spirit-beast kind living on the area's biomes, when all of these hold:
    - the area and its 8 neighbours hold spare prey: P grazers of its prey, less 6 for each predator there, at least 12
    - no village structure stands within 48 cells of the area's centre
    - no spirit beast of any kind has its home within 128 cells
  - **Its chance follows the spare prey:** (P − 12) / 6 per mille, rounded down, at most 8. With 12 to spare it is none, with 36 it is 4, and from 60 it is 8.
  - At 4 per mille a cycle, a territory left empty waits on average 250 cycles, about 2,000 days or 17 years, so killing one leaves it empty for a while, as the owner decided. A territory rich in prey refills sooner, and a lean one later or never.
  - **Food.** It eats as a predator, every second cycle, and starves at 3 cycles of hunger, so a territory hunted bare of game loses its beast too.
  - **Defence.** It engages any person standing inside its territory, closing and striking with its kind's strike, an arc at arm's length for 40 every 30 ticks in the base pack. It never steps outside its territory. With no person inside, it walks back to its home cell. A newborn's 120 health falls to 0 on the third strike, so levies passing through die, and masters may stand.
  - **Paths.** ADR 0043's fields do not avoid territories. NPCs never choose an objective, a slot or an animal to hunt, inside one unless they may hunt its beast. So a road through a territory is dangerous, and villages gather and hunt elsewhere.
  - **Events.** A spirit beast killing a person is a death by a beast, a new cause beside wound, age and hunger (ADR 0032). A spirit beast killed is a new event, `beast slain`, with its kind and its killer, so rumours carry it.
- **The spirit core.** An item kind of the elixir family, stacking, worth 1,500 copper, with a qi value, 800 for the spirit tiger's.
  - **Consumed.** By ADR 0040's `consume`, it adds its value to the consumer's store, up to its talent's limit. So it buys at least 400 days of cultivation at the region's best rate of 2 qi a day, and more for anyone slower, and can carry a person across a realm's threshold. Talent still bounds how far anyone grows.
  - **Refined.** An apothecary refines 1 core and 2 herbs into 3 spirit pills, each of 320 qi, in 2 workers' days, by #245's recipe rule. The pills hold 960 qi to the core's 800, so refining is worth the labour, and a pill is a ware anyone can buy.
  - **For NPCs.** A person no driver steers who holds a core or a pill consumes it once a day when its store lies below its limit by at least the value. Otherwise it sells the core to its village's market. An apothecary's market buys cores, so refining happens where an apothecary stands.
  - **For the player,** it is an item like any other, to consume, sell, or have refined by buying the pills.
- **Tiers.** The cycle's rules, appearance, grazing, hunger and spirit beasts' arrival, run on the area's tick of ADR 0056's cycle in every tier. Movement, flocking, flight, chases and fights run only in live areas, and predation is the chase there and the cycle's chance elsewhere.
  - **Far hunting** is settled once a day: each assigned hunter of a far village kills the nearest animal it may hunt within 32 cells, by index, and its drops go into its inventory. A spirit beast is never killed this way; it needs a fight.
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Memory.** 7 MiB of records and 1 MiB of index at the capacity, plus a 4-byte refusal counter, and a herd table of 8 bytes a herd, at most 65,535 herds, 512 KiB. Grass costs no storage beyond ADR 0056's slots.
  - **The cycle.** About one area a tick, as ADR 0056's slots: at most 64 slots and the area's animals, typically under 30, and a few draws. The predator and spirit-beast tests read the 9 areas' grazer counts, kept per area.
  - **Live movement.** Live areas hold at most about 3,000 animals, at about 30 an area. Each takes one step a tick: flocking reads at most 11 herd mates, about 33,000 reads a tick at worst. A grazer's flight test asks #205's spatial index for predators and people within its 8 cells' sight, about 3,000 queries a tick, and `sees` walks a line only where levels differ (ADR 0058). A chasing predator reads its one target. No allocation is made per tick.
  - **At 500 living,** the region's few areas hold a few hundred animals, a tenth of these figures.
  - **Strikes.** A strike reads the animal index of the areas its shape touches, a few dozen records.
  - **Far hunting.** A scan of at most 9 areas' animals for each hunter, once a day, beside ADR 0056's far gathering.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 64 animal kinds
  - a herd of at most 12, and at most 65,535 herds
  - a chase budget of at most 2,550 ticks, its byte in tens of ticks
  - a sight of at most ADR 0046's reach
  - at most 8 prey kinds a predator
  - at most 8 stacks in a kind's drops
  - health of at most 65,535
  - a territory radius of at most 32 cells
  - a spirit core's qi value of at most 15,000, the region's highest talent limit
- **Saves, the hash and determinism.** The animal records, the herd table, the index and the refusal counter are saved and hashed. Every rule is integer. Areas run on their own tick of the cycle in index order, animals within an area in index order, and every draw comes from `animal_stream`.

The implementation, in order, each a pull request with its acceptance, after ADR 0056's slots and gathering are built:
1. **Grass, grazers and their store.**
   - Acceptance, each with a control: grazers appearing only in areas with forage; none in an area holding a village structure; grazing cropping slots that regrow in 3 cycles; a grazed-down area's grazers starving to its forage; an appearance past the capacity refused and counted.
   - Acceptance: `make sweep` with the animal bands below.
2. **Herds, predators, flight and the chase.**
   - Acceptance, each with a control: a herd following its leader to forage and keeping within 4 cells of its centre, where a lone animal wanders; a deer fleeing a wolf it sees toward its herd, where one behind a cliff it cannot see does not; a wolf giving up when its budget is spent and resting; a pack chasing one target; predators appearing only near enough prey and starving where prey is hunted out.
   - Acceptance: a live area's kills over 50 cycles within a fifth of the far chance's for the same animals, so the two grains agree.
   - Acceptance: `make bench` within budget with the animals of the live tier, and `make sweep` within the bands.
3. **Strikes on animals and hunting.**
   - Acceptance, each with a control: a strike killing a deer and its drops as piles; a hunter assigned only while game stands within 32 cells; meat eaten by its household before rations, the rest sold to the granary; hide sold to the market and tanned at a leatherworks.
   - Acceptance: `make bench` within budget with the animals of the live tier.
4. **Avoiding the stronger.** The interim rule, then #256's appraisal.
   - Acceptance, each with a control: before #256, a deer fleeing a person within 4 cells, where one 5 cells away grazes on.
   - Acceptance, once #256's appraisal lands, each with a control: a lone wolf giving way to a herd of six boar, where it holds against two; grazers keeping clear of a band of people larger than their herd, where they ignore a smaller one; a spirit beast holding its territory whatever it faces.
   - Acceptance: `make sweep` within the bands.
5. **Spirit beasts and spirit cores.** Acceptance, each with a control: a spirit beast appearing only in a territory meeting all three conditions; engaging a person inside and never stepping outside; its death recorded and its core consumed up to the talent's limit; an apothecary refining 3 pills from a core.
6. **The view.** Animals drawn by kind and state, a spirit beast's territory shown only once the player has seen its beast, and the new items' sprites in the pack's `view/`. Acceptance: captures (ko) of grazing deer, a hunt and a spirit beast defending.

**The sweep's measure.** `make sweep` prints, per seed, at each decade: grazers, predators and spirit beasts by kind, the share of forage slots grown, the grazers the forage supports and the predators the prey supports, summed over areas, animals hunted that decade, and wild meat per living person. Target bands, over seeds 0 to 49 for 100 years:

| Measure | Band |
|---|---|
| Grazers at year 100 against year 80 | within 4/5 to 5/4, on 45 seeds or more |
| Grazers against forage slots not empty | at most a third at every decade, on every seed |
| Grazers against what the forage supports, from year 20 | half to all of it at every decade, on 45 seeds or more: the counts track the food |
| Predators against what the prey supports, from year 20 | half to all of it at every decade, on 40 seeds or more |
| Predators against grazers, from year 20 | a twelfth to a quarter, on 40 seeds or more |
| A grazer kind and a predator kind alive at year 100 | on 40 seeds or more |
| No explosion: a kind's count in any area against what its food supports there | at most twice, at every decade, on every seed |
| No extinction everywhere: every base-pack kind alive somewhere | at every decade, on 45 seeds or more |
| Wild meat a living person has a year at year 100 | 5 to 40 measures: a supplement to fields, never the staple |
| Refusals at the capacity | none, on every seed |

Spirit beasts are reported with no band: the region's 16 areas and its villages leave few places 48 cells from every village.

## Decision

Animals are records of 28 bytes in a store of their own, kept by home area, with a technical capacity of 262,144, an appearance past it refused and counted as a failure. Kinds are pack data: grazers, predators and spirit beasts, with biomes, health, speed, strike, prey and drops.

Grass is a forage slot kind of ADR 0056. At each area's 8-day cycle, a grazer crops one grown forage slot, so it needs 3 of them, and starves at 2 cycles without one. A predator kills one grazer of its prey nearby every second cycle, and starves at 3 cycles without one. A grazer appears, with no breeding, at a chance of 250 per mille times the share of the area's forage support still unused, and a predator at 100 per mille times the share of the prey's support still unused, each by a counter-based draw, so the rate follows the spare food and is none at the support. None appear in an area holding a village structure.

The base pack's bestiary holds grazers, browsers and small game, predators of several sizes and a spirit beast, and birds and fish as slot kinds. Herding kinds move in herds of up to 12 behind a leader that follows forage, by cohesion, alignment and separation. In live areas a grazer that sees a predator of its kind flees toward its herd or cover, and a hungry predator chases the nearest prey on a budget and gives up when it is spent; elsewhere the cycle settles predation by a chance from the two speeds. Animals give way to a stronger side by #256's threat appraisal, with their kind's strength and temperament. Predators fight back when struck. Strikes hit animals as people, and a killed animal drops its kind's stacks as piles. Idle earners hunt while game stands within 32 cells, at most a fifth of them. Meat feeds the hunter's household first and the rest goes to the granary; hide, horn, bone and claw go to the market, where #245's workshops make leather and gear from them, spirit-beast gear among it.

A spirit beast appears, at a chance that rises with the spare prey from none at 12 to 8 per mille a cycle at 60, no village stands within 48 cells and no other spirit beast within 128, holds a territory of 12 cells, attacks anyone inside it and never leaves it. Its death drops a spirit core, which adds its qi to the consumer's store up to its talent's limit, or which an apothecary refines into pills.

This supersedes ADR 0056's hunting as gathering from a slot that moves, now hunting animals that are records of their own. It extends ADR 0056's slot kinds with forage, ADR 0046's strikes with animals as targets, ADR 0032's deaths with a death by a beast and a beast slain, and ADR 0052's earnings with hunting.

## Consequences

- Game follows the land: forests and hills hold deer and boar, predators follow them, and a region hunted hard empties of both.
- Villages near rich ground eat better, and leather and horn and bone gear come from where the game is.
- Spirit beasts make wild places dangerous and rewarding, and their cores are a rare path to qi that talent still bounds.
- Live areas carry a few thousand more movers, each simpler than a person.
- #243's ADR 0063 gains wild meat as a food source beside fields and fruit, tuned by forage and appearance rates in the pack.

## Signs the decision was wrong

- Grazers and predators swing from boom to bust every few decades, so appearance needs smoothing or predation a slower rate.
- Hunting empties every region in the first decades, so game stops mattering and the hunter's livelihood fails.
- Wild meat feeds more than fields in the sweep, so food is no longer farmed and villages stop growing fields.
- Spirit beasts never appear, because no place lies 48 cells from every village, or they kill so many travellers that roads empty.
- Spirit cores carry ordinary people across realms too easily, so realms stop marking talent.
- Live kills and the far chance drift apart, so an area changes its game as the player walks in or out of it.
- Herds clump at area borders or split and rejoin every tick, so flocking needs damping.
