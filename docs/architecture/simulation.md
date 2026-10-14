# Simulation

The simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

### The world

- The simulation core is a C++ library in `core/`, with no engine dependency.
- Its data and the checks on them sit in one file pair per unit, flat in `core/` (ADR 0010), each named for what it holds:
  - `limits` the bounds a world accepts;
  - `calendar` the days, seasons and years;
  - `skill` skills, strikes, casts and their refusals;
  - `attack` each person's attack slot and token against the foe it engages, each foe's slots and tokens taken, and the token table's rules (ADR 0068), saved and hashed only when set or held;
  - `talent` talent and its draws, and souls;
  - `chart` the cells and villages a soul remembers, whether a village is in view of a cell, and the territory a soul's chart remembers;
  - `names` names;
  - `kinship` marriage and births;
  - `village` villages, their structures and what they hold, and the villages a world keeps: the one place for each village's record and each structure's village, deciding what a village holds, its cap, what its workshops serve, how its stock fills and feeds, which village takes in the hungry, and who ruins and who retakes it; and how villages' claims divide the land into territory, which the live territory and a chart's remembered territory both read;
  - `growth` how villages grow;
  - `arts` arts, realms, cultivation, learning and manuals;
  - `cultivation` the rules of cultivation and the martial arts: a person's realm rank, nature and heart method, whether a skill lies within its realm, how far an art agrees with it, the skill it may learn next, how its qi capacity grows and its qi recovers, and the chances it comprehends, reads a manual or is taught;
  - `diplomacy` relations and their refusals, and the declarations standing between factions, one place for each two, deciding how two factions stand by the declaration between them or else their stances, which declarations a faction may make and when, what a leader no driver steers seeks, and which factions a duel's winner's allies turn hostile;
  - `trade` wares and trade;
  - `talk` talk and what a person offers;
  - `leadership` a faction's lead, duels, admission and elections, and the leaders a world keeps: the one place for each faction's leadership and the duels being fought, deciding who may challenge a leader, how a duel ends and whom it leaves leading, and when a faction elects;
  - `sect` sects as created, and applying to one;
  - `event` the world's events, whether an event names only records the world holds, who is a person's kin, and who hears of an event as a rumour, by where it stands, its kin and its faction;
  - `chronicle` the bounded, numbered record that keeps the latest events and steps of progress, and checks a saved one;
  - `progress` people's steps in the martial arts, and whether a step names only a person, art, skill and realm the world holds;
  - `person` a person;
  - `life` the rules of life that gather the rest.

  `world` holds the World, which keeps the rest of the state and runs the rest of the rules on each tick, and the rules data it reads. It schedules the villages' rules and carries what they decide to people and armies: who makes a village home and the rosters they serve on stay with it, as do a village's seat and its growth by building, founding and raiding. For the leaders it chooses the sect master a faction elects and records each change of leader, and it applies to the rules' stances the hostility a duel's winner's allies turn to. For diplomacy it refuses a declaration from the dead or from one who leads no faction, counts each faction's living and draws whether a leader acts on what it seeks. For cultivation it goes over the living, the manuals and the sects, finds the nearest reader of a manual, draws whether a person comprehends or is taught, and notes each realm and skill gained.
- A world is created from a seed, a map of buildable, rough and blocked cells, armies, structures, rules, villages, each faction's leadership and its rules of life. It advances one `tick()` at a time.
- The rules are data: the separation people keep, the hostility between factions by their stances, the floors of the structure stages, the ticks in a day, the sight within which people close on a foe, and the core's capacity for the living.
- The capacity is a storage and performance limit, not a rule of play: ADR 0017's bound, `MAX_PEOPLE`, unless the rules set another, up to `PEOPLE_CEILING`. Only a scenario measuring a larger world sets it, as a measurement aid; every region keeps the default until an ADR raises it with the measurements. It is a hard upper bound, not a target.
- All simulation state is integers: positions are fixed-point sub-cell units.
- The world ticks at a fixed rate, `TICKS_PER_SECOND`, so a rate given per second, such as a skill's, becomes work per tick.
- The calendar is the core's: a day is the rules' `ticks_per_day` ticks, with seasons and years over days. `date()` derives the date from the tick count, which `tick_count()` reads, and `date_at` the same of any tick.
- Every person moves and strikes on every tick; there are no levels of detail. At the population bound the whole world costs less than the tick budget (below).

### People and armies

- Every NPC is a person, with a position, health, a cooldown, a home village or none, its qi and known art skills, its marriages and children, its silver and wares, and what its birth gave it.
- A person's identity is its index among the world's people, kept for as long as the world runs, through every army it serves in and after it dies.
- `age_in_years` reads a person's age in whole years, and `faction_of` the faction of the army it serves in, none once dead.
- An army is a roster of the living people serving in it, never a count, with an objective, a faction and skills. Every living person stands on exactly one roster. A person who dies leaves its roster but keeps its record, so the dead cost no work on a tick and stay out of the spatial index.
- `conscript()` moves one living person to another army's roster, and `disband()` returns every member with a home to that village's residents. Neither creates nor removes a person.
- Every army follows an order: an objective cell, the skills it strikes with, and an attack intent's target or none. `order()` gives an army another order, which it follows from the next tick. That is the port through which a driver steers a body, Godot input or a test, and the same type and resolution serve an NPC and the player alike.
- A driver marks the armies it steers through `steer`, saved and hashed; any driver may still order any army. The rules that decide for people on their own decide nothing for a steered army's members.
- The player's body is an army of one person, born into a village (`world-generation.md`). An army may name a village as its members' home without being its residents; a home that is no village is refused.

### Leadership and diplomacy

- A faction may have a leader, a person, and a succession rule, as data. The only rule is the duel; a faction without one keeps its leader.
- An order may name a faction to challenge, the same order for the player's body as for an NPC:
  - the challenger is the ordered army's lead, and a duel taking members only refuses one from another faction
  - the challenger and the leader become hostile to each other alone and fight by the one skill rule until one is down to the duel's yield
  - a challenger who wins leads the faction, and if the duel says so, every faction at peace with it by stance turns hostile to it
  - `challenge_refusal` gives the reason a challenge may not be made, and `order` refuses a challenge exactly when it names one, so the rule is written once
  - `duel` reads the duel being fought for a faction's lead
- A faction with an `Election` is led by the master of its sects with the most elders when its leader is dead or missing, or when its term ends. It has no duel, so every challenge on it is refused. `elects` reads whether a faction has an election.
- Two factions stand by their stances unless a declaration, war or peace, stands between them. `hostile` reads the declaration first and the stances otherwise, and every rule that asks whether one faction attacks another asks `hostile`.
- `relation` is the one read of how a faction stands toward another: own, ally, neutral, hostile or at war. `declaration` reads the declaration standing between two.
- Only a faction's leader declares, through `declare`, at once and for both factions. `declaration_refusal` gives the first reason it may not, and `declare` refuses exactly when it names one; a declaration holds for the diplomacy's hold before the pair may declare again.
- Diplomacy is data, `Diplomacy` in `Life`, checked once a day, one rule for every leader no driver steers: war on a faction allied or neutral while its own people outnumber the other's by a margin, peace at a declared war while they are fewer, each by a chance a year.

### Life, kinship and souls

- Life is data, `Life`, given with the world. One function bears every person, the player's body and every NPC alike, at creation and on rebirth.
- Talent is a growth and a limit per discipline, and a lifespan is drawn with it. A strike that hits practises a discipline, raising its attainment toward its limit. A person dies once its age passes its lifespan, lengthened by its attainment.
- Everyone the world starts with has no parents. A village's households start as married couples, each heading a family of its own; everyone else the world starts with belongs to the family of its army. A family branches only when one of its people founds a village.
- Kinship is data, `Kinship` in `Life`, checked once a day:
  - a person of marrying age with a home and no living spouse marries, by a chance, someone of the other sex and another family from its village or one nearby
  - a wife of bearing age whose husband lives bears a child by a chance, through the one birth function, into her husband's family, or hers if the data says so
  - where the data balances sexes, a child takes the sex fewer among its village's unmarried; otherwise its sex is drawn at even odds
  - no child is born while as many live as the rules' capacity
- Every person bears its family's surname and a given name for its sex, each in Korean and romanised, from `Names` in `Life`. `name()` reads it.
- A soul is the history a person may carry from body to body: the lives it has lived and the disciplines it practised. `ensoul()` gives a living person one.
  - a past life widens a person's talent draw, the less as lives repeat
  - a person carrying a soul who dies is born again on the next tick, into a village with a living resident, in the army its body served in; with no such village it is not born again
- A soul keeps a chart of what its bodies have seen, `chart()`. No rule reads it: it is the steered body's remembered view.
  - at the end of every tick, each living person carrying a soul in a steered army sees the cells within the rules' `map_sight`, 15 in the region, and 0 charts nothing
  - seen cells are kept a bit a cell, in blocks of 128 bytes for each 32-cell area with a seen cell; runtime storage includes each block's area index and optional village slots, as ADR 0078 records
  - a village with a structure in view is remembered as it stands then: its owner, whether it has living residents, its claim centre, and whether its structures stand, are damaged or lie in ruins; one never in view is not remembered
  - `remembered_territory()` derives borders as `territory()` does, from the villages remembered and their remembered claim centres, and gives no holder to a cell not seen
  - a body born again keeps the soul's chart; the chart is saved, and the hash folds it only while it holds something

### Movement

- People move toward their order's objective along their army's flow field, rebuilt when an order changes the objective. An objective must be reachable from where each member stands.
- The field is 8-connected. A diagonal step is open only where both cells beside it are passable, so no step cuts or grazes a blocked corner, and a diagonal step moves at about the straight speed.
- A cell's step is the one, of those beginning a shortest path, whose cell lies nearest the straight line to the objective, so a diagonal over open ground is walked on its line.
- People keep the separation from neighbours found through a spatial hash rebuilt each tick:
  - a person feels no pull within its arrival ring, so an arrived army spreads out to the separation
  - a standing crowd parts for someone walking out of it
  - a person on the move does not give way to anyone behind it, so a band queues through a gap one cell wide
  - two people closing on attack slots round the same foe do not give way to each other, so people crossing round it to their slots do not lock; one standing on its slot counts as standing, so they pass it, and at rest the slots keep the separation
  - a person on the move holds while a foe on the move lies ahead of it within its reach, and walks on once that foe falls, stands or leaves; a foe behind or standing holds no one
- A person no driver steers engages a foe within the rules' sight: at home only a foe inside its village's claim, on the march any. It stands to fight while a foe lies within its reach, and otherwise closes on the nearest.
- While the rules set attack tokens, a person engaging a foe within four cells' reach closes on an attack slot round it instead, and stands to fight on its slot, within a quarter cell, with the foe in reach. A foe has a ring for arm's length, a reach up to 2 cells, and one for long reach, up to 4, each at the shortest reach on it less two steps, with as many slots up to 16 as lie the separation apart. A slot is offered only on a passable cell where nobody but the foe and those engaging it stands within the separation, so a foe inside its own line draws no ring there. Each tick the people engaging a foe keep the slot they hold while it is offered, and the rest take the free offered slot nearest them in order of distance from the foe then index, the lower slot among equals. A seeker off the ring's band, its radius and half a cell, closes on the slot nearest it, and on the band walks round the ring a slot at a time toward its own, the shorter way. A slot frees when its holder dies, stops engaging that foe on that ring, or the slot stops being offered. A longer reach, and anyone offered no free slot, closes on the foe itself as above. Every region sets ADR 0068's default table; tokens gate no strike yet.
- An army holding an attack intent closes on its target and strikes it, whoever steers it, stepping round a blocked corner along one axis. The intent ends when its target dies, stops being a foe or leaves every member's sight, or by `end_intent`; the army then holds where its lead stands. An order without a target replaces the intent. `objective` reads an army's objective.
- `order` refuses a target that `attack_refusal` names a reason for. `target` reads the intent, and `foes_in_sight` a person's foes within sight, nearest first. A save holds the intent.
- Positions are read as the tick began, and every query either adds up what it finds or breaks ties by index, so the order people are read in decides nothing.
- The hash notes the factions each cell and block holds, so the search for the nearest foe walks outward and passes over cells holding no hostile faction, with the same result as checking everyone in sight.

### Skills and casting

- A skill is data: range, radius, damage to people, damage to structures, cooldown, qi cost and shape. A person's skills are its army's, which share a cooldown, followed by the art skills it knows, each with its own.
- A skill's shape, `Shape`, is the ground one use covers, aimed toward a point: `AREA`, a disc at the point within range; `ARC`, a frontal sweep; `LINE`, a line from the user; `SELF`, a disc around the user; and `TARGETED`, one person alone. The region's basic strike is targeted (ADR 0066).
- One rule resolves every skill, for every person alike, with one branch by shape and none by skill:
  - a person given no cast draws by the seed among its ready skills its qi pays for
  - it faces its attack intent's target while that lies in range, and otherwise, holding no intent, the nearest hostile person in range; the core keeps no facing
  - every hostile person the shape covers takes the damage once, raised by its home's workshop edge and its realm's might, and for an art skill by its nature's potency; every structure the shape reaches takes the structure damage once
  - damage to structures is signed: a negative amount restores durability up to the intact floor. Rebuilding is this rule, not a second one
  - damage to people lands after everyone has struck, and a person whose health reaches 0 dies
- Each use on a tick is recorded, by `strikes()`, for the view to draw. The record holds the last tick's uses and is neither hashed nor saved.
- A person may cast through the port, the player's body as any NPC: `cast` has it use one of its skills at a point, a person or itself on the next tick, in place of its automatic strike and through the same rule. A point beyond range is brought within it, and its cooldown starts even when it hits no one.
- `cast_refusal` gives the first reason a cast may not be made, and `cast` refuses exactly when it names one. `aim` reads, with no change to the world, the hostile person a skill would aim at, favouring a facing it is given.

### Qi and arts

- Qi is data, `Cultivation` in `Life`, one rule for every person: a store that grows daily toward a limit drawn at birth, and qi that recovers toward the store. Without cultivation no one holds qi.
- Realms are data, `Life.realms`, ascending by qi, each with a might that multiplies the body damage a person strikes with. A person's rank is how many its store reaches, derived on read.
- Arts are data, `Life.arts`: each a discipline, a nature of orientation and polarity, and skills with the rank each needs. A person's known skills are an append-only list in the order learned, and its nature is that of the art it learned first, its heart method.
- `Learning` in `Life` gives, for each axis on which an art differs from the heart, an ease on learning and a potency on damage.
- A person learns a skill of an art three ways:
  - comprehension, on each realm gained, by a chance scaled by its talent
  - a manual lying on a cell, read by the nearest person in reach with the rank and the qi; the manual stays
  - teaching, once a day, by its sect's elders, of an art its sect holds a manual of
- `skill_count`, `skill_of` and `ready_at` read a person's skills, and `realms` and `arts` the data.

### Structures and villages

- A structure is a footprint of width x height cells and a durability. Its stage, intact, damaged, destroyed or rubble, follows from durability by the rules' floors. Structures are found through a spatial hash of their footprints.
- A structure's footprint blocks movement for as long as the world runs, rubble included, unless its kind does not block, as a field does not. Footprints lie on passable cells, cover no other structure and leave every person's start uncovered.
- A cell is buildable, rough or blocked: bodies cross a rough cell, and nothing is built on one. `Map::buildable` is the one test of where something may be built.
- A village is data: its structures and its residents, whose members make it their home. Its owner is its residents' faction.
  - while its residents live, it takes damage only from factions hostile to its owner and repair only from the others
  - once none lives, the first army to restore one of its structures becomes its residents, and its owner follows
- A village's state is derived on each read and never stored: ruins, raided, prosperous or rebuilding. It records who ruined it, which outlives rebuilding and is hashed.
- What each structure holds is data, `Holding`: the people it houses, the food it stores, and the harvest it yields, which makes it a field. With husbandry, `Husbandry` in `Life`, once a day:
  - a village adds its intact fields' harvest to its stock on the first day of winter, up to its storage
  - it feeds everyone whose home it is, or none of them when it cannot feed them all
  - a person unfed too long moves to a village nearby with room and food, or dies
- A village's cap is derived on each read: the least of its standing housing, its intact fields' yearly harvest and its standing storage, each in people. A birth's chance is scaled by the headroom under the child's village's cap.
- A field shows its season, and is burnt whenever it is below its intact stage.
- A workshop is a structure whose `Holding` arms or tends people: an edge on body damage, or healing a day. While a village's arms and tends cover its people, they strike with its best edge and heal by its best healing. A ruined workshop does neither. All of it is derived on read.
- Villages grow by one rule, `Growth` in `Life`, with no branch per faction. A village pressing against its cap takes the first expansion its owner's stance, per-faction data, allows:
  - build: lay, as rubble, the plan that most raises its scarcest holding, placed by its role within its tier's vicinity, which its residents raise by the repair rule
  - found: lay the growth's core as rubble, placed by role round a new centre drawn among the open sites nearby, and send settlers there, led by a founder whose children take a new branch of its family. A site is open where the first tier's square lies on free buildable land held by no other faction and no village's centre lies within `apart` cells, the test generation uses. `settlement()` reads the square a village was founded round
  - raid: send raiders against the nearest hostile village within reach, until the party is gone, the target is empty or no longer hostile
- A village's clearings, such as a seat's court, are land growth keeps clear as though a structure covered it, and so is the square its tier keeps round its centre.
- A village's tier is the last of the growth's tiers its residents reach: a hamlet, a village from 20 and a town from 150 in the base pack. It is kept once reached, saved and hashed, and read by `tier()`. Each tier sets the square kept clear and the vicinity the village builds in (ADR 0076).
- Each plan and structure carries a role, square, home or edge (`core/layout.cpp`). A plan is placed nearest the village's centre by path over the cells bodies pass, ties drawn by a counter-based hash of the world's seed, the village and the plan's index among its structures: a square plan touching the square, a home a cell clear of anything built, an edge at least two steps farther than the farthest home. So a village strings along a bank its houses are nearest by path, and its fields lie beyond its houses.
- An edge plan may lie on rough cells as a terraced field, which repair raises at the growth's `terrace` per mille of the labour, 2,000 in the base pack. Every other role builds on buildable cells (ADR 0024).
- Settlers keep their old home until a harvest finds their new village able to hold them.
- Growth lays a structure only where it covers no objective and no living person and leaves every objective reachable. Laying one that blocks rebuilds every army's flow field. `plan_of` reads the plan a structure was laid from, and `army_count`, `structure_count` and `village_count` count what growth added.
- A cell lies in the territory of the owner of the nearest peopled village whose claim reaches it, derived on each read. `territory()` reads it.

### Sects

- A sect is data given with the world, `SectSpec`: a name, a faction, a seat village and founders. Membership is the person's `sect`; everything else about a sect is derived on read:
  - its members are its living people, and it stands while it has one
  - its elders are the share of its members of the highest attainment, at least one, and its master the highest of them
  - its seat is held while the village is not in ruins and its owner is the sect's faction, and its cap is the pupils of the seat's standing structures, 0 while the seat is lost
- Once a day, after kinship, sects are kept:
  - a member serving in an army of another faction leaves its sect
  - a fallen sect's manuals scatter near its seat or are lost
  - a person who meets its faction's `Admission` thresholds joins a sect of its faction within reach with room, by a chance a year
- A person may apply to a sect through the port, the player's body as any NPC: `apply` makes it a member at once when the admission's thresholds hold, without the chance. `application_refusal` gives the first that fails, talent and attainment as one reason so a refusal never tells hidden talent, and the daily admission assesses every person by the same function.
- `sect_count`, `sect_of`, the other `sect_*` reads, `seat_held` and `manuals` read sects and manuals.

### Events, progress and rumours

- The world records events in a bounded ring of the latest, `EVENT_CAPACITY`, read oldest first by `events()`: a village founded, raided or ruined, a sect fallen, a lead changing hands, a person born and a person dying.
- It notes each step of progress in the martial arts, a skill learned or a realm reached, in a store of its own, `PROGRESS_CAPACITY`, read by `progress()`. No rumour is derived from it. The store is hashed and saved.
- `rumours` derives the events that reach a person, newest first: those within the rules' `rumour_reach`, and wherever they happened, what befell its kin and its faction. The `event` unit decides who hears; the World gives it the events kept, the people, the person's faction and the reach, and records each event where it happens.

### Talking and trade

- One person may talk to another within the rules' `talk_reach`. `talk_refusal` names the first reason it may not: either is dead, the two are one person, the other is beyond reach, or their factions are hostile or at war as `relation` reads them.
- `talk_target` names whom a person would talk to: the nearest it may talk to, or would but for hostility, the first in army and roster order among equals.
- `offers` lists what a person offers one who talks to it: its news while it lives, then its wares while it sells any. The player hears rumours by talking.
- Silver and wares go round by one rule, `Trade` in `Life`, for every person alike:
  - a ware is data: its name, its price, the health and qi it restores, and the discipline whose people sell it
  - `sells` is the one rule of who sells what: a living person whose best discipline is the ware's
  - each birth holds the trade's purse, and every living person with a home earns a wage a day. Silver is a plain count on the person; the dead keep theirs
  - `buy` and `consume` act at once, refused by `buy_refusal` and `consume_refusal`
  - every person no driver steers buys the wares it lacks once a day, and consumes one when its health or qi runs low

### Content packs

- The world's tables come from content packs (ADR 0049, its first step) through a port, `PackSource`, that serves a pack's manifest and its content documents, one per kind of content, each a list of entries with an id, as the core's value tree, `ContentTree`.
- `load_content` takes the base pack and the mods, orders them by what each requires and names to load after, checks every document against its kind, refuses a pack with any error whole, with every pack requiring it, merges the rest by id field by field, the later winning, and builds the tables, refusing a merged set that fails the checks `generate_region` makes.
- Every error is collected with its pack, document, entry, field path and a `ContentRefusal`.
- The base pack, `packs/wuxia/`, holds exactly the tables the core defined before. `packs_json/` is the JSON adapter of the port, reading pack directories for the sweep and the tests, and documents handed in as bytes for the adapter.
- Loading the base pack takes about 1 ms on the development Mac in the Release build, within ADR 0049's start-up budget of a second.

### Determinism, saving and budgets

- A world exposes a hash of its whole state, and determinism is tested by comparing hashes. A test pins the hash, so it must be the same on every compiler and platform. State added since is folded only while it is held, so a world without it keeps the pinned hash.
- A log of orders and casts, each with its tick, replays to the same hash.
- A world is saved through the save port, `SaveStore`, which keeps bytes by `Slot` and names no file or technology. `save` writes the world's whole state, its data included, and for a generated region its `Origin`, the region's seed and the player's birth village. `load` reads it back and rebuilds what the world derives. `save_parts` decodes a save as `load` does and reports where three of its parts end, so a test that changes a field in place finds it by its part, not by its place from the payload's end.
- The encoding is the core's own, versioned and checksummed. `load` refuses with a `LoadError` an empty slot, bytes that are not a save, another version, a truncated or corrupted save, and any state no world can be in. Any change to what a save holds raises `SAVE_VERSION`.
- The tick budget is a p99 tick time of at most 4 ms on the development Mac (Apple Silicon), on the M1 benchmark scenario in `core/m1_scenario.cpp`, the worst case: the population bound in one battle with structures broken by casters. The benchmark fails its setup when the battle strikes too rarely, so a scenario that stops bounding a battle in play does not pass.
- `make bench` measures it, and also measures the live scenario in `core/live_scenario.cpp`, issue #216's target for the live tier, against the same budget. That target is not yet in force: it is printed as met or missed every run and does not enter the exit status. ADR 0053 makes it binding once #216 meets it.
- Each benchmark scenario sets its capacity above the people it holds, and fails its setup if the living ever reach it, since a world held back by its capacity no longer measures the people it was built for.
- Timing depends on the machine, so `make ci` does not run the benchmark.
- `make sweep` lives the generated region of a range of seeds for a number of years and prints, per seed, what the region's data produces. It tunes data and gates nothing, so `make ci` does not run it.
- The core is built, tested, linted and formatted with the toolchain ADR 0003 chose. Lint parses it on every host against one pinned Ubuntu 24.04 sysroot, unpacked by `lint-sysroot.sh`.

## Rationale

- Remembering each claim centre with the village so live orders do not move remembered borders: ADR 0078 (`adr/0078-remember-each-villages-claim-centre-with-its-last-seen-state.md`).

- The core is C++ and loads no engine, so its rules run headless: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The build, test framework, formatter and linter, and how CI installs them: ADR 0003 (`adr/0003-build-the-core-with-cmake-googletest-and-llvm-tools.md`).
- Lint against one pinned Linux sysroot, because clang-tidy's findings depend on the standard library it parses: ADR 0016 (`adr/0016-lint-against-one-pinned-linux-sysroot-on-every-host.md`).
- An 8-connected flow field that never passes beside a blocked cell, each cell's step the straightest of a shortest path, diagonal steps at about the straight speed, and a person on the move holding before a foe on the move within its reach: ADR 0037 (`adr/0037-move-people-eight-ways-and-hold-them-before-a-foe-in-reach.md`), which supersedes ADR 0005's 4-connected breadth-first search.
- A person on the move ignoring people behind it, so a crowd passes a gap: ADR 0020 (`adr/0020-let-a-person-on-the-move-ignore-people-behind-it.md`), which supersedes ADR 0017's rule that people on the move give way to one another.
- Attack slots round a foe, assigned by distance and kept while held: ADR 0068 (`adr/0068-surround-a-foe-from-attack-slots-and-let-only-its-token-holders-strike-it.md`). Offered only where uncrowded, reached round the ring, and passed between seekers of the same foe: ADR 0080 (`adr/0080-offer-only-uncrowded-attack-slots-and-reach-them-round-the-ring.md`), after slots as first written locked a crowd from one side and, given a pass between any seekers, stacked a line battle.
- Every NPC a person with an identity, armies as rosters, conscription and disbanding, no levels of detail, the population bound, and the movement rule: ADR 0017 (`adr/0017-make-every-npc-an-individual-and-every-army-a-roster.md`), which supersedes ADRs 0005 and 0007 and ADR 0006's multiplicities.
- Flow fields, the spatial hash, and fixed-point numbers for determinism across builds: ADR 0005 (`adr/0005-simulate-armies-at-three-levels-of-detail-in-fixed-point.md`), whose tiers and aggregates ADR 0017 removes.
- Skills as data, the single rule that resolves them, and structures and their stages: ADR 0006 (`adr/0006-resolve-every-skill-through-one-rule-scaled-by-count.md`), whose scaling by count ADR 0017 removes.
- Villages, their derived state, repair as a signed structure effect, owners following residents, and the recorded destroyer: ADR 0015 (`adr/0015-run-the-village-life-cycle-from-structures-and-residents.md`).
- Footprints, the hit rule they share, footprints that block in every stage, and objectives that must be reachable: ADR 0013 (`adr/0013-give-structures-footprints-that-block-movement-in-every-stage.md`).
- Leaders as people, the duel as faction data fought by the one skill rule, and challenges through the order port: ADR 0018 (`adr/0018-change-a-factions-leader-by-duel-as-faction-data.md`).
- Growth by one rule of stances and pressure, building and founding by laying rubble raised by repair, family branches, settlers moving home at a harvest, derived territory, and flow fields rebuilt when growth lays a blocking structure: ADR 0023 (`adr/0023-let-villages-build-found-and-raid-as-they-press-their-caps.md`), which supersedes ADR 0013's rule that footprints block only from the world's creation.
- Buildable, rough and blocked cells, and one test of where something may be built: ADR 0024 (`adr/0024-build-only-on-terrain-whose-kind-bears-buildings.md`), which supersedes ADR 0023's laying clear of an impassable cell.
- A village's clearings extend ADR 0023's spacing as template data, counted as a structure's footprint is, so growth builds around a seat's court rather than into it (issue #120); ADR 0023's rule stands as stated.
- Villages kept `apart`, tiers kept once reached, plans placed by role round a square by path distance, and terraced fields: ADR 0076 (`adr/0076-keep-villages-a-spacing-apart-and-grow-each-ones-form-from-its-square-by-the-ground-it-stands-on.md`). Corrected when built (#169):
  - **The tier is saved.** ADR 0076 holds that saves gain nothing, but a tier kept once reached needs its peak stored, so each village's tier is saved and hashed. So is the centre its square stays round, since its residents' objective moves when another army takes the village over. A plan's and a structure's role are saved too, and `SAVE_VERSION` is 13.
  - **`apart` is 20.** ADR 0076 asks for the largest of 20 to 32 at which seeds 0 to 999 all generate and `make sweep` keeps 48 of 50. No value generates all 1,000:
    - `main` at `8b4bb8d7` itself generates 995, refusing seeds 320, 394, 416 and 864 for a seat and 480 for a village;
    - at 20, 998 generate, refusing seed 480 for a seat and 533 for a village;
    - at 24, 983; at 28, 933; at 32, 771;
    - so the criterion is read as no worse than `main`, as the PM decided.
  - **The sweep at 20.** `make sweep` keeps every faction peopled on 49 of 50 seeds over 100 years. At 24, seed 25, inside the sweep's seeds, does not generate.
  - **The distribution at 20.** Over seeds 0 to 49, lived 100 years at 5 ticks a day:
    - the nearest peopled village is at least 20.0 cells away at both ends, with a median of 38.0 at year 0 and 22.2 at year 100, against ADR 0076's 21.2 and 15.2 at `006d15d`;
    - 62% of villages lie within 24 cells of another at year 100;
    - the median village has 9 residents and the largest 44, so 103 of 814 reach the village tier and none the town;
    - the living number 3.65 for each married woman living with her husband, near the 3.5 a household ADR 0076 assumed.
- Village caps from housing, food and storage, the daily feeding, hunger, migration and starvation, births slowed by headroom, and fields as structures that block nothing: ADR 0022 (`adr/0022-cap-each-village-by-its-housing-food-and-storage.md`), which supersedes ADR 0013's rule that every footprint blocks.
- Workshops as per-kind capacities and qualities on `Holding`, an edge on the strike rule and daily healing while their capacity covers a village's people, and arms and tends among the holdings growth's scarcity rule compares, rather than shorter cooldowns, healing as a skill, or a trigger of its own: ADR 0029 (`adr/0029-let-villages-build-workshops-that-arm-and-tend-their-people.md`).
- Sects as data with membership on the person and everything else derived, seats as villages, admission and leaving, the cap from a seat's pupils, falling and manuals, and the election by elders' favour: ADR 0026 (`adr/0026-keep-sect-membership-on-the-person-and-seat-each-sect-in-a-village.md`).
- Applying to a sect through the port by the daily admission's thresholds, assessed once for both, with talent and attainment one reason: ADR 0031 (`adr/0031-let-a-person-apply-to-a-sect-by-the-admissions-thresholds.md`), which supersedes ADR 0026's rejection of joining through the port.
- The ring of the latest events, a sect's fall noted when its last member leaves, and rumours derived by reach, kin and faction: ADR 0032 (`adr/0032-keep-a-bounded-ring-of-world-events-and-read-rumours-from-it.md`).
- Progress in the martial arts noted in a store of its own, apart from the ring of events, so rumours keep their history and who hears what keeps one rule: ADR 0036 (`adr/0036-keep-progress-in-the-martial-arts-in-a-store-of-its-own.md`).
- Elders as a share of a sect's members by attainment, at least one, since members' attainment rises to their talent limit and no threshold kept elders a minority: ADR 0028 (`adr/0028-make-a-sects-elders-the-share-of-its-strongest-members.md`), which supersedes ADR 0026's elders by an attainment threshold.
- Households of married couples, each heading a family of its own, as a village's founders: ADR 0025 (`adr/0025-start-each-village-from-households-of-married-couples.md`), which supersedes ADR 0021's one family per starting army for the families of households.
- A child of the sex its village's unmarried lack, so a faction's line does not end on a run of one sex, and the alternatives measured against it: ADR 0027 (`adr/0027-bear-each-child-of-the-sex-its-village-lacks.md`), which supersedes ADR 0021's even-odds sex for those children.
- Marriage, children and names as data under one rule for everyone, births capped by the living bound, founding ages, and the day's ticks as world data: ADR 0021 (`adr/0021-let-people-marry-have-children-and-bear-family-names.md`), which supersedes ADR 0019's day fixed in the core.
- A chart per soul of what its steered bodies saw, kept by area, villages as last seen with borders derived from them, saved, hashed only while held, and read by no rule: ADR 0073 (`adr/0073-chart-the-land-a-soul-has-seen-and-show-each-village-as-it-was-last-seen.md`).
- Talent and lifespan drawn at birth by one rule, ageing, souls born again into a family of the same world with a widened draw, and the calendar: ADR 0019 (`adr/0019-draw-talent-and-lifespan-at-birth-and-bear-a-soul-again.md`).
- Saving through a save port of bytes by slot, the whole state with its data and the region's origin in the core's own encoding, versions refused rather than misread, and every state no world can be in refused at the boundary: ADR 0030 (`adr/0030-save-a-world-through-a-save-port-in-the-cores-own-versioned-encoding.md`).
- Orders as the one shape of every army's intent, and the player's body as an army of one: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`), whose record of no driver ADR 0038 supersedes.
- Declarations of war and peace held per pair over the stances, `relation` as the one read of how factions stand, only a leader declaring, the diplomacy deciding for leaders no driver steers, and the steered mark, with the alternatives measured: ADR 0038 (`adr/0038-let-faction-leaders-declare-war-and-make-peace-over-their-stances.md`), which supersedes ADR 0011's record of no driver.
- Talking within a reach given as data, refused for hostility as `relation` reads it, and what a person offers listed by the core, so the view draws a menu with no branch per kind of person: ADR 0039 (`adr/0039-let-a-person-talk-to-another-within-a-reach-and-hear-the-news-it-has-heard.md`).
- Silver a plain count on each person, earned by a wage and spent on wares as data, sellers decided by one rule of best discipline, buying and consuming as orders refused for their reasons, and people no driver steers buying and consuming by the same rule: ADR 0040 (`adr/0040-buy-elixirs-and-herbs-from-merchants-with-silver.md`).
- Skills shaped as data and resolved by one rule with a branch by shape, the automatic strike facing the nearest foe in range and sweeping its shape toward it, melee at arm's length as data, the per-tick record of strikes, and people no driver steers closing on a foe in sight: ADR 0046 (`adr/0046-strike-by-shape-at-arms-length-and-close-on-a-foe-in-sight.md`), which supersedes ADR 0006's disc around the nearest target for every skill, ADR 0034's automatic strike landing on the nearest, ADR 0037's rejection of closing on a foe and its standing foe holding no one for people no driver steers, and ADR 0020's rejection of a pull toward hostiles.
- The basic strike targeted, an attack intent as an order's target that its army chases and strikes, steered or not, the intent's refusals and its end, and the guard strike retuned with the sweep's measurements: ADR 0066 (`adr/0066-strike-one-chosen-foe-as-the-basic-attack-and-chase-it-on-an-attack-intent.md`), which supersedes ADR 0046's basic arc, its region with no targeted skill, and a steered body never engaging, for a body holding an intent.
- Casting a skill at a point, a person or itself through the port, resolved on the next tick in place of the automatic strike, which stays for everyone, with no facing in the core: ADR 0034 (`adr/0034-let-a-person-cast-a-skill-at-a-point-a-person-or-itself.md`), which supersedes ADR 0011's rejection of a one-shot strike.
- Qi as a store cultivated by a talent drawn at birth, realms derived from it with a might, arts as data learned by comprehension, manuals and teaching into a list of skills each with its own ready tick, one strike and cast rule over a person's army's and known skills that spends qi, and nature from the first art learned: ADR 0035 (`adr/0035-cultivate-qi-into-realms-and-gain-arts-by-comprehension-manuals-and-teaching.md`), which supersedes ADR 0026's manual of a discipline, ADR 0034's cast of its army's skills only and ADR 0019's best-discipline practice for art skills.

## Open questions

- **Whether a soul widens cultivation.** A soul's past lives widen its talent by discipline, and not its cultivation, so the realms past beyond peak are reached by no one under the region's data.
- **Qi deviation.** Forcing an art of another nature or above one's realm carries no risk (`docs/concept.md` §Qi and martial arts lists it as proposed).
- **News of a war.** No event records a declaration, so no rumour carries it, and the HUD shows it only in the faction window.
- **Standing feeding relations.** ADR 0054 (`adr/0054-keep-each-groups-standing-toward-a-person-changed-by-deeds-seen-or-heard.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Standing.** Villages, sects and factions hold standing toward people, changed by deeds as pack data that witnesses within sight teach their groups at once, and that a rumour carries at 64 cells a day to the victim's groups. Unseen deeds stay hidden, and standing decays toward zero.
  - **Its effects.** Hostile standing makes `hostile` true between a group's members and the person, beside factions, duels and feuds. Unfriendly standing refuses sales, admission and teaching.
  - **Faction relations.** Standing feeds faction relations only through ADR 0038's diplomacy, which weighs standing toward the other faction's leader. A deed never turns a faction hostile by itself.
- **Where a soul goes with no village to be born in.** A soul whose body dies while no village has a living resident is not born again.

- **When a settlement fails.** A founded village that raids burn back before its settlers raise it keeps them, and keeps its mother village from founding again, for as long as they live.

- **Whether rubble should free its cells.** Rubble blocks as an intact structure does. Freeing it would mean rebuilding the flow fields that cross it when a structure falls, on the tick it falls.
- **Whether two people on the move can block each other.** A person on the move gives way to another on the move as to anyone, so two at peace marching head-on into each other hold each other up. Two on crossing diagonals that meet side by side do so too, on `main` as now. Foes hold before each other on purpose (ADR 0037).
- **Whether people closing on an objective should keep apart before they stand.** People closing on one objective from either side count one another as behind and come within half a cell before they stand and spread out. Walking straight from one side, a column never did; a column marching more than ten cells short of its objective keeps nearly the separation either way.
- **How the world is simulated at the hand-made map's scale.** Eight ADRs, proposed, decide it; nothing above changes until the pull requests that build them land:
  - ADR 0042 (`adr/0042-simulate-the-world-in-three-tiers-by-distance-from-the-steered.md`): 32-cell areas, 16,384 of them, in live, stepped and far tiers by distance from the people a driver steers, with hysteresis, a live cap, the same rates in every tier, a far strike rule by expected damage on individuals in roster order, daily rules untiered, and full detail everywhere for a world with no steered army or no tiers. It would supersede the "no levels of detail" above.
  - ADR 0043 (`adr/0043-find-paths-over-a-coarse-graph-of-areas-and-fields-within-them.md`): a coarse graph of areas' components and entrances, paths by A* over it, and fields over an area and a margin that keep ADR 0037's straightest step toward the objective, in place of a flow field per army over the whole map; reachability read from the graph.
  - ADR 0044 (`adr/0044-encode-the-map-in-runs-and-save-the-tiers-state-with-the-world.md`): the dead saved without the living's state, the tiers' state saved and hashed, and the save version raised.
  - ADR 0047 (`adr/0047-author-one-continuous-map-of-4096-cells-by-hand-at-a-li-a-cell.md`): the hand-made map. The world holds a nibble a cell of footing and travel class, and the biome and region of each 8 x 8 block. The coarse graph comes built with the map and is rebuilt only where structures touch it. A step costs 5 and 7 on open and rough ground, 4 and 6 on roads and bridges, and 10 and 14 on fords, and a person moves at `SPEED` scaled by 1.25 on roads and 0.5 on fords. That would supersede ADR 0037's single cost above for cells of another class. A ferry carries anyone bound across from one landing to the other after its crossing time. A save holds the map's identity in place of its cells and is refused for another map. Horses and river boats wait for an issue of their own.
  - ADR 0045 (`adr/0045-set-the-population-bound-by-measuring-a-central-plains-scenario.md`): `MAX_PEOPLE` set by measuring a Central Plains scenario on PC, at a median p99 within 4 ms and a day's tick within a frame; 500 until then.
  - ADR 0070 (`adr/0070-double-the-map-to-8192-cells-of-half-a-li-so-every-journey-takes-twice-as-long.md`): the map at 8,192 cells, 65,536 areas, with a level of regions of 8 x 8 areas above ADR 0043's graph, up to 4,194,304 slots for ADR 0056's cycle at about 4.6 areas a tick, and the tiers' radii unchanged in areas. It would supersede the 4,096-cell figures in this list.
  - ADR 0048 (`adr/0048-scale-each-device-by-a-quality-profile-and-measure-the-bound-on-the-lowest.md`): the tiers' radii, live cap and stride come from a quality profile kept with the player's settings, low, standard or high, neither saved nor hashed. Tests run on standard, and far outcomes may differ between profiles. The bound is measured on the low profile, on the 4,096 map, from 10,000 to 50,000, with the daily rules spread over the day. This replaces ADR 0045's range of 2,000 to 5,000.
  - ADR 0077 (`adr/0077-move-play-to-the-8192-cell-map-in-ordered-steps-keep-the-region-as-the-rules-fixture-and-hold-the-tiers-to-an-all-live-run.md`): play moves to the map in eight ordered steps, while the generated region stays the rules' fixture for the tests and `make sweep`. The live tier's budget is #216's 5,000 people within 4 ms p99. A tiered run is held to bands against the same world run all-live: residents within 15%, owners kept in 9 villages in 10, and foundings and losses within 20%. A tier change hands over only saved state: an army's far anchor and progress, and every person's whole record. Across live, far and live again, each person's record is asserted unchanged field by field, with a control that drops qi, and the run hashes the same twice. ADR 0076's `apart` is also checked by the map's validator, starting at 48 cells.
- **Content from packs, and a theme-neutral core.** ADR 0049 (`adr/0049-load-the-games-content-from-data-packs-and-let-mods-be-packs-over-the-base.md`); its first step, the port and the base pack, is above, and the rest waits for the pull requests that build steps 2 to 7.
  - **What stays code.** The rules, the set of skill shapes, the refusal reasons, the kinds of event, the calendar's seasons and years, the save's encoding, the hash and the limits such as `MAX_PEOPLE` stay code.
  - **Saves and the hash.** A save records the ordered identities, id and data hash, of the packs with simulation data, and is refused with `PACKS_DIFFER` under others. The hash folds them, and every pinned hash changes once. The save keeps the world's data as above.
  - **Theme words.** `Orientation` and `Polarity` become two axes of values the pack names. Qi becomes energy, realms levels, cultivation training and the heart method the first art. Names become text keys, so the core holds no display text.
- **Gear, holders and the economy.** ADRs 0050, 0051 and 0052, proposed; nothing above changes until the pull requests that build them land.
  - **Gear.** ADR 0050 (`adr/0050-carry-and-wear-items-as-pack-data-in-four-slots-that-break-to-empty.md`): items as pack data in four slots. The weapon gives the basic strike in place of the army's guard strike, an art needs its discipline's weapon family, and an empty weapon slot strikes bare-handed. Gear wears by hits, a broken piece empties its slot, and one rule wears the best piece held. It would supersede ADR 0046's guard strike as an army skill, ADR 0029's edge, ADR 0040's wares as counts and ADR 0035's draw among all skills whatever a person holds.
  - **Holders.** ADR 0051 (`adr/0051-give-every-item-one-holder-and-scatter-a-destroyed-warehouse-on-the-ground.md`): every item held by a person, a warehouse lot owned by an account, or a pile on the ground. The dead drop everything, piles expire, and soldiers at war carry loot home to their faction. A destroyed warehouse scatters, and caves open only to a steered body. It would supersede ADR 0040's rule that the dead keep their silver.
  - **The economy.** ADR 0052 (`adr/0052-run-the-economy-from-households-needs-through-town-markets-taxed-by-the-power-that-holds-the-land.md`): money in copper, and households with purses meeting needs by thresholds. The granary sells rations and pays its takings to field workers. Town markets move prices against demand between import and export prices, and merchant houses run caravans. One faction rate taxes trade, households and gates, and treasuries pay soldiers and gate the diplomacy. Discontent drives households to flee, and every holder sheds its excess so money stays bounded. It would supersede ADR 0040's sellers, prices, purse and wage, ADR 0022's free feeding, whole-harvest cap and migration by the person, ADR 0038's diplomacy by numbers alone, and ADR 0023's raiding party of a fixed count.
- **Arts' families, tiers, mastery and lineage.** ADR 0055 (`adr/0055-give-arts-a-family-a-tier-and-a-mastery-and-gain-higher-arts-by-synthesis-over-a-lineage-graph.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Mastery.** Each known art carries a mastery raised by hits, training and teaching, capped by talent, whose stages open forms with the realm's rank. Manuals and teaching give an art's first forms.
  - **Synthesis.** Higher arts come by a daily chance for anyone meeting an edge of a lineage graph in the pack, replacing the comprehension above.
  - **Left open by ADR 0055.** What heart-method and footwork arts do beyond their skills and ADR 0072's passives, such as cultivation's rate or footwork's speed, which would touch ADRs 0037 and 0046. ADR 0079 selects heart-method-driven nature; how the method is selected or changed remains open. Whether talk or rumour reveals a person's arts, which would change ADR 0039. Whether people draw an art at birth.
- **Nature and building from materials.** ADR 0056 (`adr/0056-grow-natural-objects-in-slots-the-map-places-and-build-villages-from-what-people-gather.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Slots.** Natural objects are slots the map places, 64 an area, that grow, spread and regrow every 8 days and block nothing.
  - **Gathering.** People gather from them by one order. Villages assign their idle earners to gather what their next plan lacks, paid from the treasury.
  - **Building.** A structure rises only as far as its materials are delivered, which supersedes ADR 0023's raising by repair alone above, and settlers carry a first house's cost.
- **World-scale arts, materials and terrain change.** ADR 0057 (`adr/0057-resolve-world-scale-arts-as-waves-over-area-indexes-absorbed-by-materials-and-reshape-terrain-in-a-conserving-delta-layer.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Materials.** Every strike walks a line of effect through materials that absorb or break, which supersedes the shape hitting every target it covers above.
  - **Waves.** A `WAVE` shape resolves ring by ring through per-area indexes.
  - **Terrain.** Skills gated by realm and power change terrain in a delta layer that conserves elevation units, with footing and paths following the next tick.
- **Cliffs, leaps and sight by height.** ADR 0058 (`adr/0058-leap-up-and-down-cliffs-by-realm-and-stamina-and-veil-a-cliffs-top-from-those-who-cannot-leap-it.md`), proposed; nothing above changes until the map's levels reach the core.
  - **Levels and cliffs.** The core holds a level per cell. A one-level drop is a step, and higher drops are cliffs, crossed by a leap. A person leaps up a cliff within its climb height, set by realm and footwork and paid in stamina, and jumps down one within twice it. A leap it cannot make is refused, so nobody falls.
  - **Paths.** Climb classes on the coarse graph let NPCs leap only drops their weakest member's class covers, up and down, and leap up only with the stamina for the whole drop.
  - **Sight.** `sees` hides the ground above a cliff of three levels from those below who cannot leap it, for engagement, witnesses and the explored map.
- **Population bounded by food.** ADR 0063 (`adr/0063-bound-the-population-by-the-food-that-can-be-had-and-keep-the-living-bound-a-technical-capacity.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Births.** A wife's chance is scaled by her household's provision, the days of food its purse and its village's stock give, full at 30, and by crowding past housing, in place of the headroom above. No birth stops at the cap, which growth, migration and the granary still read.
  - **The capacity.** `MAX_PEOPLE`, or the bound ADR 0048 measures, is a technical capacity. A birth at it is refused and counted, and the sweep and the bench fail on any.
  - **Levers and the loop.** Field and wild food yields are pack data by biome and season. `make sweep` reports population, food, money, villages and hunger by decade against target bands.
- **Making gear from materials, and recycling.** ADR 0064 (`adr/0064-make-gear-from-materials-by-recipe-and-reforge-broken-metal-at-a-loss.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Recipes.** Every made kind carries its workshop, materials and labour as pack data. Each day a workshop makes the kind its market lacks most among those it has materials for, and treasuries order gear at their capital's market.
  - **Sources.** Ore comes from ore veins at the map's mine sites, gathered as ADR 0056 gathers, and a village gathers for its market's shortfall.
  - **Scrap.** A broken item becomes scrap of its main metal, which supersedes ADR 0050's item gone above, and organic parts leave nothing. A smith reforges 2 scrap into 1 metal, so metal is made only from ore and halves with each round.
- **Animals and spirit beasts.** ADR 0067 (`adr/0067-let-animals-arise-where-food-feeds-them-hunt-them-for-meat-and-materials-and-let-spirit-beasts-hold-territories.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Animals.** Animals are records of their own, kept by home area. Grazers crop grass slots and predators kill grazers at each area's 8-day cycle, and each appears, with no breeding, at a chance scaled by the spare food around it, none where its food is all used. Starving ones die. Herding kinds flock behind a leader, prey flees the predators it sees, and predators chase on a budget where people are simulated in full and kill by a speed chance elsewhere. Animals give way to the stronger by the threat appraisal of #256.
  - **Hunting.** Strikes hit animals, whose drops fall as piles. Idle earners hunt game within 32 cells. Meat feeds their households first, and hide, horn and bone go to the market for the workshops.
  - **Spirit beasts.** They appear rarely in wild places, defend a territory, and drop a spirit core that adds qi up to the talent's limit.
- **Travel means.** ADR 0061 (`adr/0061-travel-on-foot-by-horse-or-by-boat-at-speeds-set-by-travel-class-and-means.md`), proposed; nothing above changes until the map and items it needs are built. Speeds are per means and travel class: foot, horse and boat. Horses are items in a mount slot, and boats are vehicles sailing routes of landings for a fare. Armies move at their slowest member's means. A horse cannot leap, so a rider goes round by road or tethers its horse at a leap edge, and speeds hold whatever size the map is built at.
- **Insights, requirements and passive skills.** ADR 0072 (`adr/0072-gate-arts-on-insights-gained-by-manuals-nature-and-repetition-and-let-arts-grant-passive-skills.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Insights.** Each person holds a set of insights, gained from manuals, from natural conditions, or by repeating actions counted in up to 16 practices, each rolled daily once its conditions or threshold hold, the same for NPCs.
  - **Requirements.** An art needs insights, a realm rank and prerequisite arts. A manual's reading fails without them, for its cost and 30 days, and the store notes what was missing.
  - **Passives.** An art's passive skills give regeneration, qi recovery, damage by family, sight, stamina, climb cost and descent, folded into cached totals that strikes, recovery, sight and leaps read. Footwork's climbing effect is one of them.
  - **The soul and scrolls.** The player's soul holds its insights, known arts and scrolls, so a reborn body keeps them, its arts sealed until its realm and, under the selected model B once implemented, its qi nature fit. Scroll kinds are pack data, held per soul and hung when first earned.
- **Qi nature direction selected; runtime pending.** The owner selected model B on 2026-10-13 in [#293](https://github.com/garamsh/murim-24/issues/293): righteous-to-demonic and yin-to-yang nature moves gradually according to the heart method, under shared NPC/player rules. ADR 0079 (`adr/0079-move-qi-nature-gradually-on-two-axes-by-the-heart-method.md`) resolves ADR 0072's pending model choice alone. The first-art derivation above remains implemented until replacement lands.
  - **Decision gates.** Before implementation, settle axis bounds, rates and cadence, heart-method selection/change, art compatibility and path bands, and reborn-body initialization. Five elements, forced use and deviation penalties remain undecided; ADR 0072's numbers are proposals.
  - **Bounded follow-up.** Under #293, ADR 0079 splits drift with save/hash, learning compatibility, and rebirth sealing into dependent changes with controls. Learning needs the art/insight requirement work; sealing also needs soul-held knowledge and passive availability. Design approval completes none of these.
- **Fortification.** ADR 0062 (`adr/0062-raise-a-towns-walls-by-its-wealth-its-factions-strength-and-the-threats-it-meets-in-stages-that-grow-with-it.md`), proposed; nothing above changes until it is built. A village plans a circuit of walls by its scale, wealth, faction's strength and threat. Segments are raised by ADR 0056's rule in stages over years. Gates close before a hostile army, a destroyed segment is a breach, and a grown town plans a new circuit and keeps or reuses the old one.
- **Faction policies.** ADR 0065 (`adr/0065-run-each-faction-by-a-few-policies-that-move-existing-rules-inputs-set-by-its-leader.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Policies.** Each faction holds eight: tax with its kinds, a levy share, corvée, spending priorities above a reserve, trade, diplomacy, its growth order and law. Each is read by an existing rule in place of a constant, so it costs what that rule costs.
  - **NPC leaders.** One daily rule moves a policy a step at most after a year's term, from food per head, the treasury, threat, discontent and war, around defaults its character sets.
  - **Open for the PM.** Roads have no rule that builds or keeps them, so no spending priority funds them.
- **Personal favor.** ADR 0060 (`adr/0060-keep-each-persons-favor-toward-the-few-it-deals-with-often-beside-its-groups-standing.md`), proposed; nothing above changes until the pull requests that build it land. Each person holds favor toward at most 8 others, kin and those it deals with often, changed by deeds felt first-hand. A person's own conduct follows its favor where it holds one, and its groups' standing otherwise.
- **Attack slots and tokens.** ADR 0068 (`adr/0068-surround-a-foe-from-attack-slots-and-let-only-its-token-holders-strike-it.md`), proposed. Its first two steps, the state and the rules data in the `attack` unit and the slots, are in force above as ADR 0080 amends them; the tokens, the waiting ring and the view wait for the pull requests that build steps 3 and 4.
  - **State.** The per-person arrays grow with the people list, as it grows on a birth, rather than being sized to the capacity at construction and on load as the ADR's cost section has them; its status line records the correction.
  - **Tokens.** Only a holder of one of the foe's tokens, by its realm rank as rules data, may aim a use at it, which limits the automatic strike above. The steered person attacking is exempt and takes a token ahead of any queue. The rest circle a waiting ring outside reach, and people engaging on their own prefer a foe with a free token.
  - **Freeing.** Slots and tokens free on death, a new foe, an intent's end, a leap or flight, and after a lease while others wait.
  - **Reaching slots.** ADR 0080 (`adr/0080-offer-only-uncrowded-attack-slots-and-reach-them-round-the-ring.md`), proposed, amends the slots: one is offered only where nobody but the foe and those engaging it stands within the separation, so a front-line foe draws no ring inside its own line and those engaging it fight it from where they reach it, as above; a seeker reaches the ring at its own bearing and walks round it to its slot; and two people seeking slots round the same foe do not give way to each other.
- **Weighing danger.** ADR 0071 (`adr/0071-weigh-danger-by-one-appraisal-of-the-strength-in-sight-and-back-off-flee-or-regroup-by-temperament.md`), proposed; nothing above changes until the pull requests that build it land.
  - **The appraisal.** Everyone outside the player's armies compares the strength in sight that would strike it with its own side's: health times realm might times the realm's weight, which doubles with each realm, and gear once ADR 0050 lands. It does so every 15 ticks. An army a driver steers is never appraised, nor one holding an attack intent while it holds.
  - **Temperament.** Drawn at birth, bold, cautious or cowardly, naming the ratios at which a person backs off and flees. New person-state storage starts cautious before every birth (founders and recruits included) installs its draw or loading restores saved temperament, including bold living NPCs; unequal save versions remain refused without migration.
  - **Outcomes.** Closing as above at or below the back-off ratio; holding inside the home claim; backing off from the threat; fleeing to the army's lead or home; regrouping when alone. At each appraisal, flight above its threshold takes precedence over regrouping and holding ground and overrides any non-flee stance immediately. Otherwise close enters the chosen outcome immediately; a non-close stance yields only after 30 ticks and a ratio strictly below three quarters of its entry threshold (the entry threshold for regroup is half the back-off threshold), then chooses again from the current conditions. A cautious back-off at 1,600 becomes flight at 4,000 at the next appraisal; declining or oscillating risk waits for both recovery conditions. Every change starts a new stance tick. Back off and flee free all slots and tokens before assignment, take none, and strike nothing even with a foe in reach. This replaces the engagement above, which closes whatever the odds.
  - **Bands.** An army on a march order appraises through its lead, counting what any member sees, and turns home when outmatched, so a raid can fail on sight of a strong village.
  - **State.** Temperament, stance and a full 64-bit stance tick stay with the persistent person record, through deaths, roster changes and crowd compaction; a reborn body gets its own birth draw. They are saved, so a loaded bold living NPC continues as saved even with an identity beyond the living capacity. The existing crowd maps compact query slots to persistent people; only scratch stamps key those live slots and are bounded by the living capacity. Payload is 10 bytes per recorded person (dead included), 9 per recorded army and 4 per live-capacity slot, excluding record padding; persistent history is not capped by the living bound. Appraisal adds no allocation or new mapping.
- **Who sells, once people have trades.** `sells` reads a person's best discipline. When issue #170 gives people trades, the trade replaces it there, in that one function (ADR 0040).
- **Silver taken by force.** Issue #175's deeds may let extortion move silver. Silver stays a plain count on the person so a deed can take it.
- **Whether full health is the person's own.** A village's healing restores anyone up to `newborn_health`, the same for every person. A per-person maximum waits for a rule that needs one.
