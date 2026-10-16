# ADR 0082: Lay a tier's square buildings once a village reaches it, ahead of its holdings' plans

> Status: proposed; built by #390, as `simulation.md` records; its no-site reading corrected in the Errata below
> Date: 2026-10-17

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #377. A village that grows into a tier gains none of the square buildings that hold no housing, food, storage, arms or tends, such as a temple or an inn.

How growth chooses what to lay, at `main` `b74e361c`:
- **The growth check.** Every `interval` days, each village records the tier its residents have reached (`World::grow`, ADR 0076). Only a village whose people reach `pressure` per mille of its cap goes on, and it takes the first expansion of its owner's stance it can (ADR 0023).
- **Building.** `World::lay_building` builds only while every structure of the village is intact. It lays the plan of `builds` that most raises the village's scarcest holding (housing, food, storage, arms or tends), placed by its role (ADR 0076). A plan that raises none is never chosen, so a temple or an inn is never laid.
- **Founding** lays the growth's `core`: a square, a hall or a first house, and a well (ADR 0076).
- **A lost structure** stays on the map as rubble with its footprint and its kind. Residents raise it again by the repair rule (ADR 0023). Nothing removes a structure.
- **What is saved and hashed.** Each structure's kind is kept with its village (`VillageSpec::kinds`, saved and hashed by #368), and so is a village's tier (ADR 0076, as corrected when built). Growth data is saved with the life, `Growth::tiers` included, and the hash does not fold it.

ADR 0081 gives people trades from their village's buildings. The monk works at a temple and the innkeeper at an inn. #364 adds a temple (5 × 4) and an inn (5 × 3) to the base pack. By the PM's ruling on #364, they join the village start list as data only, and generation places them by role round the square. Hamlets, foundings and villages that grow into the village tier get neither. So no monk or innkeeper works in them, though a generated village of the same tier has both.

### Options weighed for how a tier names its square buildings

- **Each tier names square kinds in growth data**, as `square_kinds`, structure kinds of the square role. A village holds the kinds of its tier and of every tier below it. Chosen. The tier is where ADR 0076 already keeps what a village of that size looks like: its square and its vicinity.
- **A list beside the tiers, each kind with the residents it needs.** Rejected. It would be a second threshold beside the tier's, and the two could disagree about when a village is a village.
- **New holdings for worship and lodging,** read by the scarcity rule. Rejected. Nothing in the simulation reads a holding of worship or lodging, so the holdings would exist only to make the scarcity rule pick these plans.

### Options weighed for when a village lays them

- **At the growth check, whether or not the village presses its cap and whatever its stance,** while every structure of its own is intact. Chosen.
  - It is the only option under which every village of a tier gains the tier's buildings, as every generated village of that tier starts with them.
  - The Demonic Cult's stance only raids, so under a pressed build no Cult village would ever lay a temple. ADR 0065 lets a leader's policy change the stance, which would change which villages gain them.
- **As a build expansion, ahead of the scarcity rule, only when the village presses its cap.** Rejected. It keeps ADR 0023's frame, but villages that never press, and every village whose stance does not build, would never gain the tier's buildings.
- **By the scarcity rule's turn, as the holdings' plans are chosen.** Rejected. These buildings raise no holding, so the rule would never pick them, which is the defect itself.

### Options weighed for their order against the holdings' plans

- **First, one a check.** A village lacking a kind its tier names lays the first such kind, in the order the tiers list them, and builds nothing else that check. Chosen. These buildings are what makes a village of its tier, and a village lacks at most a few. The scarcity rule takes its turn again once they stand.
- **After the holdings, only when no holding is scarce.** Rejected. A growing village is nearly always short of some holding, so this order would rarely lay them.
- **Beside the holdings' plan, both laid in one check.** Rejected. Growth lays one structure a check today, and two pieces of rubble at once would hold building back for twice as long.

### Options weighed for no site

- **The kind is skipped for that check, the next kind the tier names is tried, and every lacked kind is tried again at the next check.** Chosen. No state is kept, so a village whose square's edge later has a free corner lays the kind then, as when it reaches a larger tier whose square reaches past the structures round the old one.
- **Placed by the home role when the square's edge has no room.** Rejected. A temple away from the square is no longer a square building, and ADR 0076's roles would mean less.
- **The village records that it gave up.** Rejected. It is new saved state for a case that may come right on its own.

### Options weighed for a lost one

- **Rubble counts as held.** A village holds a kind while any structure of its own is of that kind, at any stage. Rubble is raised again by the repair rule, as any structure is. Chosen. It is the rule every other structure follows, and it lays no second temple beside a fallen one.
- **A destroyed one replaced by a new plan.** Rejected. It duplicates what repair already does, and a village under raid would lay rubble on rubble.

## Decision

**Data.** Each of growth's `tiers` names `square_kinds`: structure kinds of the square role, in order. They live in `growth.tiers` in `packs/wuxia/data/life.json`. Growth's plans gain one for each kind a tier names, from its structure kind, as they gain one for each building of the core today (`growth_for`). The base pack's village tier names the temple and then the inn, the kinds #364 adds. Its hamlet and town tiers name none, so the town takes the village tier's two. The content check refuses a tier naming a kind the pack does not have, or one whose role is not the square.

**The rule.** At every growth check, before the pressure test, each village with living residents and every structure of its own intact looks at the kinds named by its tier and by every tier below it. It lacks a kind when no structure of its own is of that kind, at any stage. It lays the first lacked kind that has a site, placed as any square plan is: on a free corner touching its tier's square, by path, with ties drawn by the seed (ADR 0076). The structure is laid as rubble for its residents to raise by the repair rule. A village that lays one this way takes no expansion that check.

**No site.** A kind with no free corner on the square's edge is passed over that check. The next lacked kind is tried, and every lacked kind is tried again at the next check.

**A lost one** stays held while it is rubble, and is raised again by repair.

**What is not changed.** The scarcity rule, the stances, founding's core, generation's start lists, the square and home and edge roles, and placement are all as before. A founded village gains its tier's buildings by this rule once it reaches the tier, as a grown one does.

**Save and hash.**
- Saves gain each tier's `square_kinds` with the growth data, so `SAVE_VERSION` rises by one.
- The hash folds no growth data today and gains nothing: whether a village lacks a kind is derived from its structures' kinds and its tier, which are saved and hashed already.
- Pinned hashes move only where a pinned world's villages reach a tier that names kinds. The implementing pull request re-pins those once.

This amends ADR 0076's growth, which lays only the plan that most raises the scarcest holding. It also amends ADR 0023's rule that only a village pressing its cap grows, for the tier's square kinds alone.

## Consequences

- **Every village of the village tier holds a temple and an inn,** generated or grown, so monks and innkeepers (ADR 0081) work wherever the tier has been reached, the Cult's villages included.
- **Building pauses while they rise.** One more piece of rubble per kind holds the scarcity rule's building back until residents raise it, at most twice for a village crossing into the village tier.
- **Growth outside pressure.** A village that never presses its cap now lays these kinds, and only these. Its residents raise them by repair as for any structure, so the labour of a village at peace rises slightly.
- **Cost.** At each growth check, every village with living residents compares its structures' kinds with at most a few named kinds. A placement builds the same flow field over the vicinity that building does today, once per kind laid, and so at most twice per village over its life in the base pack. Nothing runs per tick.
- **Acceptance, in the implementing pull request:**
  - a hamlet growing into the village tier lays a temple and then an inn, with a control in which the tier names no kinds and it lays neither;
  - a village with no free corner on its square's edge lays none, and lays the kind once a corner frees, with a control in which a corner is free from the start;
  - a temple in rubble is raised by repair and no second is laid;
  - a Cult village, whose stance only raids, gains both;
  - `make sweep` at the owner's bar, seeds 0 to 199 against `main`, with the villages of the village tier holding each kind, and the monks and innkeepers, counted at year 100;
  - `make bench`: no per-tick change is expected, and the growth check's tick is reported;
  - `make ci` and `make gcc`.
- **It lands after #364,** which adds the two kinds and their plans to the base pack.

## Signs the decision was wrong

- Villages of the village tier at year 100 often lack a temple or an inn because their square's edge is full. Then square kinds need a placement that may grow the square, or a reserved place in the tier's square.
- Building stalls behind rubble: villages pressing their cap fall behind `main` in housing at the sweep, because the square kinds' rubble holds the scarcity rule's turn. Then they should be laid only when no holding plan is due, or beside it.
- Villages that never press their cap are raided down while raising a temple, so their losses rise against `main`. Then laying outside pressure needs a peace or a size bound.

## Errata

- **2026-10-17, implementing #390 at `main` `49903a04`.** The Decision's "No site" and the first of the Signs take a square plan to be placed only on a free corner touching the square's edge. Since #328 (`fbf08c8b`), `place` in `core/layout.cpp` places a square plan, when no corner touching the square is free, as near as any free corner within the vicinity, and smithies and apothecaries are placed so. The decision stands: a square kind is placed as any square plan is, by that one rule. So "no site" means no free corner for the kind in the whole vicinity, and a full square edge puts the kind at the nearest free corner rather than passing it over.
