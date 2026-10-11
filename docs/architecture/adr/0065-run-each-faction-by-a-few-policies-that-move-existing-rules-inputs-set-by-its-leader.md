# ADR 0065: Run each faction by a few policies that move existing rules' inputs, set by its leader

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-10, in issue #230:
- Factions run **policies**, through a faction management system, so that a faction adapts to the world's causes and effects and strengthens and expands itself.
- A player who leads a faction sets them. An NPC leader chooses them by rules.

The issue asks the design to decide these:
- the policies, few, each a lever on a rule that already exists: the tax rate and its kinds, the levy of soldiers, corvée for building, the treasury's spending priorities, the trade stance, the diplomacy stance, settlement and expansion, and law
- each policy's effect as a change to an existing rule's input, never a hidden bonus, with costs the causal chain shows: discontent, flight, wealth and reputation
- how an NPC leader adapts: deterministic rules reading food per head, the treasury, threats, discontent and war, with a minimum term, differing by faction character
- the player's faction management window
- far tiers in aggregate, the memory and tick cost at the bound, and determinism
- the balance: the sweep reports faction outcomes per policy mix, and no single policy dominates over 100 years

What this builds on, from `main` at `c4e24a7` and the open PRs it cites:
- **ADR 0052** (proposed). The core rule this ADR widens.
  - One faction rate `r`, 0 to 300 per mille, sets three taxes: a share of trade, a levy on households each season, and a toll on caravans at gates. A rate per tax was rejected there as three levers where one does.
  - The NPC leader's daily rule: the rate rises 10 per mille below 60 days of needs and falls 10 above twice them, at most once every 30 days; war and peace gated by the treasury; soldiers capped at what the treasury pays for 30 days; purchases for soldiers while above a reserve, which that ADR does not give a value.
  - Discontent per household, 0 to 1,000: 1 a day for each 100 per mille of rate above 100, 20 for each hungry member, 200 for each member or kin killed by a foe, falling 2 a day in good times. A household flees at 800 to the known village that scores best on wealth, safety and rate.
  - Sects take dues into their vaults and pay stipends.
- **ADR 0038.** The NPC diplomacy rule declares war with `war_chance` 100 per mille a year while its people number `war_margin` 1,500 per mille of the other's, makes peace with `peace_chance` 200 while outnumbered, and holds a declaration `hold_days` 600. A faction's stance toward each other faction is the rules' matrix.
- **ADR 0023.** A faction's growth stance is pack data, an ordered list of `BUILD`, `FOUND` and `RAID`, and a village pressing its cap takes the first it can.
- **ADR 0054** (proposed). Deeds change groups' standing by a table of base values and weights per tier, pack data. A tax rate above 100 lowers each of the faction's villages' standing toward its leader by 3 a day for each 100 above. Standing tiers decide hostility, sales, admission, teaching and prices.
- **ADR 0056,** proposed, merged in #220. Gatherers and builders are paid from the owning faction's treasury, and a faction that cannot pay builds nothing that day.
- **ADR 0062, proposed in the open PR #246** for #228. A village's threat counter; wall circuits funded from the treasury; corvée while a village's threat is at least 400, one earner a household one day in ten, 3 discontent a corvée day; guards in gates paid as soldiers.
- **ADR 0063,** proposed, merged in #243 for #229. The world runs by simple rules whose consequences combine, and population is bounded by food. Policies here are levers on those rules, never outcomes scripted.
- **ADR 0064, proposed in the open PR #245** for #231. Gear is made from materials by workshops, and a faction's arms spending orders gear.
- **ADR 0061, proposed in the open PR #254** for #207. Roads are map data, polylines with a class, and nothing builds them.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000. About 14,000 households at 50,000 people (ADR 0052).

Options weighed:

- **Policies as inputs, never as modifiers.** Each policy is a field in a faction's policy record, and each is read by a rule that exists already, in place of a constant that rule reads today. No policy adds a percentage to an outcome.
  - Rejected: policies as bonuses, such as "+10% harvest". A bonus has no cost in the world, so the best policy would be the one with the largest number, and the owner's cause and effect would not show.
  - Rejected: many policies, one per rule. Each needs an NPC rule and a place in the window, and the issue asks for few.
- **The eight policies.** The base pack's values and bounds are pack data (ADR 0049).

  | Policy | Setting | Input it moves | Its cost, seen in the world |
  |---|---|---|---|
  | Tax | the rate `r`, and which of trade, levy and toll are on | ADR 0052's three taxes | discontent, standing toward the leader, flight |
  | Levy of soldiers | a share of earners, per mille | the soldiers a raid or war army may draw | wages, earners lost to the fields, discontent past the voluntary share |
  | Corvée | off, walls under threat, or all works | ADR 0062's corvée, and who raises ADR 0056's plans | discontent, a day's field work |
  | Spending | an order over arms, granaries, walls and sect support, and a reserve | what the treasury pays after wages | the treasury itself |
  | Trade | free, tolled or closed to rivals | ADR 0052's toll and which caravans may sell | the till's takings, prices |
  | Diplomacy | peaceful, measured or aggressive | ADR 0038's chances and margin | war's dead, wages and flight |
  | Settlement | the growth order of `BUILD`, `FOUND` and `RAID` | ADR 0023's stance | settlers, materials, raids' deeds |
  | Law | lenient, ordinary or harsh | ADR 0054's changes for deeds against its people, and its guards | guards' wages |

- **Tax.** The rate stays ADR 0052's, with its bounds and its NPC rule. The policy adds which kinds are on, three bits. A kind that is off is not collected.
  - Discontent's rate input and ADR 0054's standing toward the leader read `r` while the levy or the trade share is on, since households pay those. A toll alone falls on caravans, so a faction taxing tolls alone takes no discontent from its rate, and ADR 0052's decay, which reads the rate, reads it as 0 then.
  - So a leader can trade revenue for mood. The levy at `r` 200 is 200 per mille of 10 days of a household's 200 copper of rations, 400 copper a season. A faction of 1,000 households with the levy off forgoes 4,000 silver a season, and its households still take 1 discontent a day from the trade share.
  - This narrows ADR 0052's rejection of a rate per tax: still one rate, so one NPC rule moves it, with on and off for each kind.
- **Levy of soldiers.** A share `s` of the faction's earners, 0 to 250 per mille, base 50, may serve as soldiers.
  - A raid or army sent to war draws no more soldiers than `s` of the faction's earners, beside ADR 0052's cap of what the treasury pays for 30 days. The lower cap holds.
  - Soldiers are paid ADR 0052's wage and do not work the fields, so a faction of 1,000 earners at 100 per mille has 100 soldiers and a tenth less field work, which ADR 0022's harvest shows.
  - Above the voluntary 50 per mille, each household with a member serving takes 2 discontent a day while it serves.
  - **Decay is suspended on such a day.** ADR 0052 lowers a household's discontent by 2 a day while nobody in it is hungry and the rate is at most 100. On a day a household is pressed, or owes corvée, that decay does not apply, so the press's 2 and corvée's 3 are net. Otherwise a fed, pressed household would net 0 and the levy would cost nothing.
  - So at a pressed household's net 2 a day, one at 500 reaches ADR 0052's flight at 800 in 150 days unless the levy falls.
- **Corvée.**
  - **Off.** No corvée, even under threat.
  - **Walls under threat.** ADR 0062's rule as it stands: corvée for wall works while the village's threat is at least 400. The base setting.
  - **All works.** Corvée raises any plan of ADR 0056 and ADR 0062 in the faction's villages, whatever the threat, at the same one earner a household one day in ten and 3 discontent a corvée day, net, since decay is suspended that day (above). Corvée workers take no wage, so the treasury pays for fewer builders.
- **Spending.** Each day, the treasury first pays what is committed: soldiers' wages, guards, and ADR 0056's gatherers and builders. Then, while it holds more than its reserve, it pays the four priorities in the order the policy sets, each up to its daily target before the next:
  - **Arms:** ADR 0052's purchases for soldiers' slots and spares, and #245's ADR 0064 orders for gear its workshops make.
  - **Granaries:** food bought at the village's market into its granary, until each village holds 30 days of its people's rations. A village of 32 at a measure a person a day holds 960 measures, 480 silver at the base price of 50 copper. This feeds ADR 0022's food stock, so a village with a granary filled weathers a bad harvest without hunger or migration.
  - **Walls:** ADR 0062's wall works, their wages and materials.
  - **Sect support:** a transfer to the faction's sect's vault, up to 30 days of its stipends.
  - **The reserve** is days of the faction's ADR 0052 needs, 0 to 120, base 60. This gives ADR 0052's reserve its value.
  - **Roads are not a priority.** Roads are map data (ADR 0047, #254's ADR 0061), and no rule builds or keeps them, so spending on them would move no input. It is listed for the PM.
- **Trade.**
  - **Free.** No toll at the faction's gates, so ADR 0052's caravans choose its roads more, and its markets' tills take more trade.
  - **Tolled.** ADR 0052's toll. The base setting.
  - **Closed to rivals.** The toll, and a caravan whose merchant house's town belongs to a faction `hostile` reads is refused at the faction's markets, so its margin there is none. The faction's prices move only by its own stock and the caravans of others.
- **Diplomacy.** The stance sets ADR 0038's region data for the faction as an actor:

  | Stance | `war_chance` | `war_margin` | `peace_chance` |
  |---|---|---|---|
  | Peaceful | 0 | — | 400 |
  | Measured | 100 | 1,500 | 200 |
  | Aggressive | 200 | 1,200 | 100 |

  Measured is ADR 0038's data unchanged. ADR 0052's treasury gates still apply to every stance, so an aggressive faction with an empty treasury still makes no war.
- **Settlement.** The policy is the faction's growth order, a permutation of `BUILD`, `FOUND` and `RAID`, with `RAID` optionally left out. ADR 0023's rule reads it in place of the fixed stance. Founding spends settlers and a first house's materials (ADR 0056), and raiding makes deeds (ADR 0054) and may draw war.
- **Law.**
  - **Weights.** For deeds whose victim is of the faction's people, ADR 0054's change to the victim's village is scaled by 500 per mille when lenient, 1,000 when ordinary and 2,000 when harsh, within standing's bounds. So under harsh law a theft from a village's warehouse, −200 at that village's weight of 1,000, costs the thief −400 of its standing there, where ordinary law costs −200.
  - **Guards.** Each of the faction's villages of at least 100 residents keeps guards paid as soldiers: none when lenient, one for each 100 residents when ordinary, one for each 50 when harsh. Guards are people no driver steers who engage whoever their village stands hostile toward, by ADR 0054's tiers and ADR 0046's engagement. Under ordinary and harsh law, ADR 0062's gate guards are counted within these numbers.
  - **A lenient village with gate guards.** Lenient law keeps no guards of its own, but ADR 0062's gate guards, 2 a gate, stand all the same: they are the wall's, paid as soldiers by ADR 0062, and they close the gate before a hostile army. They engage only whom `hostile` reads, as any guard does, so lenient law changes how much a deed costs, not whether a walled gate is held.
  - Rejected: punishment as a scripted penalty, such as a fine. A fine would move silver by no rule a household reads, and the owner asked for consequences of rules.
- **Faction character.** Each faction has a character, pack data, defaulting from its growth stance's first entry so that today's factions diverge without new data. Its growth stance, not ADR 0038's stances, gives the default: ADR 0038's stances are the relation matrix's per-pair defaults, how two factions stand toward each other before any declaration, set in `core/region.cpp` with the factions themselves. A stance belongs to a pair of factions, not to one, so it says nothing of how a faction runs itself. ADR 0023's growth stance is already each faction's own choice of what to do first:

  | Character | From | Default diplomacy | Most levy | Default trade | Default law | Default spending order |
  |---|---|---|---|---|---|---|
  | Martial | `RAID` first | aggressive | 200 | tolled | harsh | arms, walls, granaries, sect |
  | Expansive | `FOUND` first | measured | 100 | free | ordinary | granaries, arms, walls, sect |
  | Settled | `BUILD` first | peaceful | 100 | tolled | ordinary | granaries, walls, arms, sect |

  The character's defaults are where an NPC leader starts and where it returns, and its most levy bounds the levy rule below.
- **How an NPC leader adapts.** A daily rule, `Policy` in `Life`, for each faction whose leader no driver steers, reads five sums the household and village passes already make:
  - food per head: the days of rations in its villages' granaries over their people
  - the treasury in days of its needs
  - its highest village threat (ADR 0062)
  - its households' mean discontent
  - whether it is at a declared war

  Each policy moves at most one step at a review, and is reviewed once its minimum term has passed: 120 days, a year, for every policy but the rate, which keeps ADR 0052's 30. A declared war, or food per head below 15 days, ends every term at once.

  | Condition | Steps taken |
  |---|---|
  | Food per head below 15 days | granaries first; levy −10 per mille; corvée off |
  | Mean discontent at least 400 | levy −10; corvée no more than walls under threat; law one step more lenient; the rate does not rise |
  | Mean discontent at least 600 | the rate falls 10, before ADR 0052's rule |
  | Highest threat at least 200 | walls ahead of arms |
  | At a declared war | arms first; levy +20, up to the character's most; trade closed to rivals |
  | Treasury below 30 days of needs | sect support last; the reserve down to 30 |
  | None of the above for a full term | each policy one step back toward the character's default |

  The conditions are read in the table's order, and a later one never undoes a step an earlier one took that day. Ties break by the lower faction index.
  - Rejected: a leader that scores every policy mix and picks the best. It would search thousands of mixes a day per faction, and it would find the dominant mix the balance must not have.
  - A steered leader's faction changes policy only by the player's orders, as ADR 0038 has it for declarations.
- **The player's screen.** The faction management window, for a leader, takes in ADR 0052's treasury window. It shows:
  - **Policies:** each setting, the term left before it may change again, and the input it moves, in words.
  - **Treasury:** ADR 0052's balance, income and spending by source, with spending by priority.
  - **Villages:** each village's people, food days, mood in ADR 0052's five words, threat and walls' stage.
  - **Armies:** soldiers paid and unpaid, the levy against its share, and guards.
  - **Effects seen:** last season against the one before, households fled out and in, deaths by hunger and by wounds, and the villages' standing tier toward the leader.

  A player's change is refused while its term runs, with the days left as the reason. Its mockup goes to the PM before it is built.
- **Tiers.** `Policy` and every rule it feeds are daily rules, untiered (ADR 0042). A far village's guards and soldiers are counted in its roster, and its granary purchases go through its market as ADR 0052 aggregates them.
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Memory.** A faction's policy record is about 16 bytes, its eight policies' last change days 32, and its last two seasons' effects seen 128: about 180 bytes a faction, a few kilobytes for every faction a pack can hold.
  - **The daily rule** reads sums ADR 0052's household pass and ADR 0062's village pass already accumulate, so it is one pass over the factions and a granary purchase per village at most, beside ADR 0052's 14,000 households a day. That is negligible.
  - **Guards** are people already living. Harsh law at 50,000 people keeps at most one in 50, 1,000 guards, paid from treasuries.
  - **At 500 and at 5,000 living.** At ADR 0017's 500, in a region of a few factions, the records are under a kilobyte and harsh law keeps at most 10 guards. At #216's live tier of 5,000, at most 100 guards stand in live areas, people the tick already moves, and the daily rule's pass is the same one over the factions.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - a levy share of at most 500 per mille, and a character's most levy within it
  - a reserve of at most 255 days
  - at most 8 spending priorities
  - a minimum term from 1 to 65,535 days
  - a law weight scale of at most 4,000 per mille
- **Saves, the hash and determinism.** Policy records, last change days, characters and effects seen are saved and hashed. Every rule is integer, read in the table's fixed order, ties by the lower index.

The balance, in `make sweep`: per seed and faction, each decade's policy settings, the share of the century each setting was held, people, villages, the treasury, households fled and wars. A mode pins a policy setting for every faction through a pack, to compare mixes. Over seeds 0 to 49 for 100 years:
- every setting of every policy is chosen by some NPC leader on at least 10 seeds
- no pinned setting makes the pinning faction the most populous at year 100 on more than 35 of 50 seeds
- among the most populous faction of each seed at year 100, no setting of any policy is the one held longest on more than 30 of 50 seeds
- ADR 0052's flight targets still hold at each character's default policies

The implementation, in order, each a pull request with its acceptance, after ADR 0052's taxes, treasuries and discontent are built (its steps 7 and 8), and ADR 0062's corvée:
1. **The policy record, tax kinds, levy, corvée, spending and the reserve.**
   - Acceptance, each with a control: a kind off collecting nothing; a toll alone adding no discontent; a raid capped by the levy share; pressed households taking discontent above 50 per mille; corvée on all works raising a house without wages; granaries filled to 30 days before walls when ordered first.
   - Acceptance: `make sweep` 48/50 or better.
2. **Trade, diplomacy, settlement and law.**
   - Acceptance, each with a control: a free faction drawing more caravans; a rival's caravan refused when closed; an aggressive faction's war chance and margin; a growth order without `RAID` sending no raid; harsh law doubling a theft's change to the village and keeping guards.
   - Acceptance: `make sweep` 48/50 or better, since these change rules.
3. **Character and the NPC rule.** Acceptance, each with a control: each table row's steps; the minimum term; war ending terms; a return to the default after a quiet term. Acceptance: the sweep's balance report and its bands.
4. **The faction management window**, after its mockup is approved. Acceptance: captures (ko) of each panel, and a change refused while its term runs.

## Decision

Each faction holds eight policies: the tax rate with which of its kinds are on, a levy share of earners, corvée, an order of spending priorities with a reserve, a trade stance, a diplomacy stance, a growth order and law. Each is read by an existing rule in place of a constant: ADR 0052's taxes and soldiers, ADR 0062's corvée, the treasury's purchases, ADR 0052's toll and markets, ADR 0038's chances, ADR 0023's stance and ADR 0054's deeds. None adds a bonus, and each costs what that rule already costs: discontent, wages, field work, standing or war.

An NPC leader adapts by one daily rule reading food per head, the treasury, threat, discontent and war, one step a policy at most, each after a year's term, the rate after ADR 0052's 30 days. Its character, defaulting from its growth stance, sets where it starts and returns. A player leader sets policies in the faction management window, which takes in ADR 0052's treasury window.

The sweep reports policy mixes and outcomes, and no setting may dominate across seeds over 100 years.

This supersedes ADR 0038's war and peace chances the same for every faction, now set by its diplomacy stance; ADR 0023's growth stance as fixed data, now a policy; ADR 0052's taxes always all collected and its purchases as the treasury's one spending, now kinds and priorities; ADR 0054's deeds' changes the same under every law, now scaled by the victim's faction's law; and ADR 0052's daily decay of discontent on every good day, now suspended on a day a household is pressed or owes corvée.

## Consequences

- Factions diverge: a martial faction levies and arms, a settled one fills granaries and walls, and the sweep shows which fares better on which seed.
- Every policy has a visible price, so a player leading a faction weighs mood, food and money rather than picking the largest number.
- NPC leaders react to famine, war and unrest within a season, and drift back in quiet years.
- The treasury window grows into a faction window with five panels.

## Signs the decision was wrong

- One mix wins on most seeds, so the costs are mistuned and the policy is a bonus in effect.
- NPC leaders oscillate between two settings each term, so the conditions need a margin between entering and leaving.
- Discontent from the levy and corvée drives flight faster than wars do, so factions empty in their first war.
- Players never change a policy, because the effects seen are too slow or too small to read.
- Harsh law's guards engage so many people that villages fight their own visitors.
