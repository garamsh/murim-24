# ADR 0076: Keep villages a spacing apart, and grow each one's form from its square by the ground it stands on

> Status: accepted; corrected when built, as `simulation.md` records: a village's tier is saved and hashed, since a tier kept once reached needs its peak stored, so saves do gain it; and `apart` is 20, no worse than `main`'s seeds 0 to 999, since no value of 20 to 32 generates all of them and `main` itself fails 5; and the sweep bar `apart` was chosen by, 48 of seeds 0 to 49, is since 2026-10-15, by the owner, the share of seeds 0 to 199 over 100 years keeping every faction peopled, no more than one standard error below `main`'s on the same machine; its growth laying only the plan that most raises the scarcest holding amended by ADR-0082, under which a village first lays the square kinds its tier names
> Date: 2026-10-12

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #169. The owner's feedback on `main` at `0962bfe`: villages stand too close together, and a village does not look like a village. The owner's later comments on the issue are binding:
- **2026-10-08.** Forms are composed by rules from the site and the tier, not stamped from a few templates. Two villages of the same tier on different sites look different.
- **2026-10-09.** A village is a real settlement, with tens of buildings, lanes, a core and edges. Target sizes per tier are stated and measured.
- **2026-10-09, the direction.** Take villages from WorldBox:
  - a centre first;
  - houses placed outward into suitable ground, along the terrain;
  - lanes where people walk;
  - special buildings with size;
  - fields at the edges.

  This is one rule set with #187's growth, which ADR 0056 designs and which leaves layouts to this issue.
- **2026-10-10.** A walled town encloses at least about 40 × 30 cells, and villages stay unwalled.

The PM's rulings:
- **2026-10-11, from #254.** Roads come only from use, as ADR 0056 has them.
- **2026-10-12, on this issue.**
  - Design #169 now, not after #173.
  - The spacing is cells in pack data, shared by generation and founding, so it holds at any map size. It is applied to today's region and reused by #173.
  - The walled form cites ADR 0062 (#246) instead of being redesigned.

Facts from `core/` at `006d15d`:
- **Generation** draws each starting village's corner among every corner where its template fits on its faction's free buildable cells. Nothing keeps it from another village. Sect seats are placed the same way.
- **Founding** draws a site within `reach`, 40 cells, of the mother village. The site must be `spacing`, 3 cells, clear of any structure, and on land no other faction holds. Nothing keeps it from another village's centre.
- **Layouts are stamped.**
  - Generation lays one of two templates, a hamlet of 7 × 9 cells and 6 structures, or a village of 17 × 9 cells and 14 structures.
  - Founding always lays the hamlet's template.
- **Building.** `lay_building` puts each building on the free corner within `vicinity`, 8 cells, nearest the centre in a straight line, with ties going to the first corner in scan order. So every village on flat ground grows the same shape, with fields squeezed among the houses.
- **Territory** is the land within `claim`, 16 cells, of the nearest peopled village.
- **The region** is 128 × 128 cells.

Measured on `main` at `006d15d`: the generated region of seeds 0 to 49, lived 100 years at 5 ticks a day as `make sweep` lives it, by a scratch program over the core's public reads. Distances are between peopled villages' centres.

| | Year 0 | Year 100 |
|---|---|---|
| Peopled villages | 504 | 1,024 |
| Nearest village, minimum | 8.1 cells | 8.1 cells |
| Nearest village, 10th percentile | 12.0 | 11.2 |
| Nearest village, median | 21.2 | 15.2 |
| Nearest village, 90th percentile | 42.0 | 26.0 |
| Closer than 16 cells, the claim | 127, 25% | 567, 55% |
| Closer than 24 cells | 286, 57% | 889, 87% |
| Territory touching another village's | 421, 84% | 989, 97% |
| Territory touching another faction's village's | 120, 24% | 304, 30% |
| Residents a village, median and largest | 2 and 2 | 7 and 47 |

The median nearest village falls below the claim as growth founds, so most villages' claims overlap. No village has more than 47 residents, so none reaches a town's scale.

`docs/art-direction.md` §Scale gives the sizes:
- a village 15 × 10 to 30 × 20 cells, with 5 to 30 households;
- a walled town at least 40 × 30 inside its wall;
- a market square 10 × 8 in a town.

ADR 0062, merged as proposed in #246, walls a village once it reaches a town's scale: 150 residents and 40 × 30 cells of building.

ADR 0024 makes hills rough: bodies cross them, and nothing is built on them (`world-generation.md`). So no village can terrace a hillside today.

Roads are authored on ADR 0047's map and, by ADR 0056, appear where people walk. Neither exists in the code at `006d15d`, so no village yet stands where roads meet.

Options weighed for spacing:
- **One `apart` in cells, the least distance between any two villages' centres, read by one test that generation and founding both call.** Chosen. It holds at any map size because only the pack's value changes.
- **A separate rule for each.** Rejected: the issue asks for no special case, and the two would drift.
- **Spacing by territory, refusing a site inside another's claim.** Rejected. It ties the spacing to `claim`, which ADR 0062 and ADR 0052 read for other ends.

Options weighed for forms:
- **Roles and ground.** Each plan carries a role as pack data, square, home or edge. Growth places it by its role and by path distance from the village's square, breaking ties by the seed. Chosen. The ground gives the form: a village strings along a river bank or up a valley because those cells are nearer by path. The seed separates villages on like ground.
- **More templates, drawn by tier and terrain.** Rejected by the owner's direction: villages are not stamped.
- **A generator per form, riverside, terraced or crossroads.** Rejected. It is code per form, and the owner asked for one rule set on different ground.

How the owner's four example forms arise under the chosen rule:
- **Riverside.** From path distance: a bank's buildable cells are the nearest to a square laid beside water, so houses string along it.
- **Walled.** From ADR 0062, at the town tier.
- **Terraced.** It needs ADR 0024 changed. Options weighed:
  - **Fields on rough cells at a higher labour cost, houses still on buildable cells.** Chosen, as the PM proposed. A village beside hills lays fields up the slope when hill cells are nearer by path than free flat land, so it terraces only where the ground pushes it to.
  - **Houses on rough cells.** Rejected. Every rule that reads footing for building would change with it, ADR 0062's walls included, for a look the fields alone give.
  - **A later issue.** Rejected. The owner names terraces among this issue's forms, and the change is one footing test for one role.
- **Crossroads market.** From a market plan of the square role, laid by a village of the village tier or above whose square lies within 3 cells of where two roads meet. It waits for two things:
  - roads in the core: ADR 0047's authored roads, once a world is built on its map, or ADR 0056's trodden roads;
  - a market structure kind: ADR 0052's town market.

  Until both land, no village takes the form. That is listed under what this leaves to other work, not designed twice.

## Decision

**Spacing.** Growth data gains `apart`, cells between two villages' centres. It is held in `growth` in `packs/wuxia/data/life.json`, beside `spacing` and `claim`, as are the tiers' thresholds, squares and vicinities and `terrace`. The templates in `packs/wuxia/data/villages.json` become lists of what a village starts with. A site is open to a village only where no village's centre lies within `apart` of its own. Every village counts: sect seats, villages in ruins, and villages still being settled.
- **Generation** draws each starting village's site, seats included, among the open corners.
- **Founding** draws among the open sites within `reach`.
- **No room.** A region that leaves a starting village no room is refused, as today.
- **Authored places.** On ADR 0047's map, every named settlement is a centre in the test: cities, towns, seats and ADR 0070's inns. ADR 0047's validator refuses two closer than `apart`, as it refuses a map whose places fail its other checks.
- **The value.** The implementing pull request measures 20, 24, 28 and 32 on today's region. It chooses the largest at which `make sweep` keeps at least 48 of 50 seeds and seeds 0 to 999 all generate. The expected value is 24, one and a half claims.

**Tiers.** A village's tier is read from its residents against thresholds in pack data: a hamlet below 20, a village from 20, a town from 150, ADR 0062's scale. A tier, once reached, is kept, so a village near a threshold does not flip between forms as people are born and die. Each tier gives:
- a square, kept clear as a seat's court is: 3 × 3 for a hamlet, 6 × 4 for a village, and 10 × 8 for a town;
- a vicinity within which growth builds, in cells from the square's centre. On #173's map: 8 for a hamlet, 15 for a village's 30 × 20, and 20 for a town's 40 × 30 around its 10 × 8 square.

**Roles.** Each plan carries a role in pack data:
- **Square:** the hall, the well, workshops and later the market and temples. Placed on a free corner touching the square's edge.
- **Home:** houses, granaries and storehouses. Placed on the free corner nearest the square by path, one cell clear of any structure, so lanes run between houses.
- **Edge:** fields and pens. Placed on the free corner nearest the square by path among those at least 2 cells, by path, beyond the farthest home.
  - An edge plan may also lie on rough cells, as a terraced field.
  - It costs `terrace` per mille of its labour, a pack value, 2,000 in the base pack, in ADR 0056's labour once that lands. Until then it is raised by repair from that share of a field's durability.
  - Every other role keeps ADR 0024's buildable cells.

**Placement.** Path distance is a flow field from the square's centre over the vicinity. Among corners equally near, one is drawn from a counter-based stream hashed from the world's seed, the village and the plan's index among the village's structures. So a draw does not depend on what else drew from the world's random stream that tick. The square stays where it was laid when a village crosses into a larger tier, and grows by its centre, keeping clear only cells no structure holds.

**Laying a village.**
- **Founding** lays a core alone: the square, the hall or a first house, and a well. Growth builds the rest, from ADR 0056's materials once they land.
- **Generation** lays each starting village's core on its site, then places the rest of what it starts with by the same roles. A template becomes a list of what a village starts with, not where each building stands.
- **Authored patches.** On ADR 0047's map, a city's or a town's layout patch is laid as authored and names its square. Growth extends it from that square by the same roles, so roles govern only what grows after the patch.
- **Far villages.** In ADR 0042's far tier, a village builds in aggregate as ADR 0056 has it. Its placements run in plan order when its area next becomes stepped or live, through the same draw and within the catch-up bound under Consequences, so the layout does not depend on when it was seen.

**What this leaves to other work.**
- **Walls.** A town's walls are ADR 0062's, planned around this ADR's built cells once the town tier is reached. Its square is the town's market.
- **Roads.** Roads come from use, as ADR 0056 has them. Until they are built, the view draws the square's paving and lanes, as it draws a village's paving today.
- **The crossroads market.** It waits for roads in the core and for ADR 0052's market kind, as weighed above.

This supersedes:
- ADR 0009's villages placed among every corner that fits;
- ADR 0013's village templates laid out from the mockup;
- ADR 0023's founding laying the settlement's layout, and its building nearest the centre within one vicinity;
- ADR 0024's rough cells bearing nothing, for terraced fields, which the edge role may lay on them.

## Consequences

Targets by tier. The base pack sets them for #173's map; today's region holds what its 128 cells allow:

| Tier | Residents | Households | Structures | Extent on #173's map | On today's region |
|---|---|---|---|---|---|
| Hamlet | under 20 | 1 to 5 | 3 to 10 | up to 15 × 10 | vicinity 8, as now |
| Village | 20 to 149 | 6 to 42 | 10 to 60 | 15 × 10 to 30 × 20 | vicinity 12 |
| Town | 150 and more | 43 and more | 60 and more | at least 40 × 30, walled by ADR 0062 | not reached, at most 47 residents measured |

Households count about 3.5 residents each, the PM's estimate on #276, which the implementing pull request measures. So the village tier's 20 to 149 residents is 6 to 42 households. `art-direction.md` §Scale's 5 to 30 is reconciled to it in this pull request, since ADR 0062's town of 150 residents fixes the upper end.

- **Today's region cannot hold villages at full size.** It cannot keep villages of 30 × 20 cells 24 cells apart, so its base values stop short of the realistic sizes. #173's map reuses the same rule with larger values. The pull request says this to the owner rather than hiding it.
- **Growth slows.** At year 100, 87% of the villages measured lie within 24 cells of another, so a spacing of 24 refuses most of the sites today's foundings take. Peopled villages per faction may fall, and the sweep's 48 of 50 is the bound.
- **Variety comes from ground and seed.** Two villages of one tier on different sites differ, and the same site and seed give the same layout.
- **Cost per placement.** A placement builds a flow field over the vicinity: at most 25 × 25 cells on today's region, and 41 × 41, 1,681 cells, for a town on #173's map. It then scans the vicinity's corners. This runs once per growth check of a pressing village, every 30 days, and generation does the same for each starting building. Nothing runs per tick.
  - **At 500 living**, today's region, the measured year-100 average is about 20 villages a seed. At most 20 placements fall due in 30 days.
  - **At 5,000 live,** at the measured median of 7 residents a village at year 100, about 714 villages, so about 24 placements a day, spread over the day's ticks as ADR 0048 spreads daily rules.
  - **Far villages on the 8,192 map** place nothing until their area is stepped or live. A village returning to view places at most one plan a tick, and at most 16 placements run in any tick world-wide, nearest area first. So a backlog of B plans settles within B ticks of its area coming into view, and no tick pays for more than 16 fields over a vicinity.
- **Saves.** Saves gain nothing, since the square derives from the seat and the tier from residents.
- **Art.** House materials by tier, such as thatch, tile and stone, and regional styles wait for #200's look to be approved, as the owner decided on 2026-10-09. The view will choose a house's look from its village's tier.
- **Building it is one pull request:**
  - `apart`, the tiers and the roles as data;
  - generation and founding through the one open-site test;
  - placement by role and path;
  - the mockup of three tiers first, on an evidence branch;
  - tests: a site within `apart` refused by generation and by founding, with a control at `apart` 0; two same-tier villages on different sites with different layouts, with a control of the same site and seed giving the same one;
  - fields lying beyond the houses, with a control in which the edge role is placed as a home and fields fall among the houses;
  - a terraced field laid on a hill cell beside a village, with a control of the same village with no hills near, which lays none;
  - a tier kept as residents fall back below its threshold;
  - the distribution measured again after;
  - captures of at least six villages over seeds 0 to 49, three tiers before and after;
  - `make ci`, `make sweep` and `make bench`.

## Signs the decision was wrong

- Villages still read as alike on flat ground. Then the seed's tie-break is too weak, and a form needs a second axis, such as a lane direction drawn per village.
- The sweep cannot keep 48 of 50 at any spacing that leaves claims apart on today's region. Then the region is too small for the rule, and the measurement waits for #173's map.
- Fields placed beyond the houses fall outside the claim, or onto another village's land, as villages grow. Then the edge role needs the claim as its bound.
- Towns under ADR 0062 wall cells this ADR's growth keeps building past. Then the town tier's vicinity should be the wall's circuit.
- Terraced fields climb hills until villages beside a range claim most of its slopes, crowding out ADR 0067's grazers or blocking ADR 0058's leap edges. Then terraces need a bound per village, or the labour cost should rise with the climb.
