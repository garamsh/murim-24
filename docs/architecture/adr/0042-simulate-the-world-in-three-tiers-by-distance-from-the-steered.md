# ADR 0042: Simulate the world in three tiers by distance from the people a driver steers

> Status: proposed; its one set of tier values for every device superseded by ADR-0048
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The user decided on 2026-10-08 (issue #173, `docs/concept.md` §World and scale):
- far places are simulated coarsely
- the population bound is raised to the largest between 2,000 and 5,000 that keeps a tick's p99 within 4 ms

ADR 0017 removed levels of detail because 500 people cost 0.487 ms a tick. Every person still acts on every tick, and every person is an individual whose record genealogy, talent and succession read.

Measured on the development Mac (Apple M1, 8 GB, macOS, on AC power) by the `measure` program on branch `evidence/central-plains`. The program links a copy of `core/` at `origin/main` 1d943f0 with `MAX_PEOPLE` raised to 5,000. Each figure is 600 timed ticks after a warm-up. Other workers' test runs shared the machine, at a load average of 7 to 9, so the figures are ranges over runs and the p99s carry that noise. `make bench` on the same machine measured p50 0.471 to 0.593 ms and p99 0.797 to 1.130 ms.

Full detail, a battle. The M1 scenario with one more column of ten 5 x 5 armies a side per 500 people, on a 128-cell map, three runs each:

| People | p50 (ms) | p99 (ms) | p50 per person (µs) |
|---|---|---|---|
| 500 | 0.556 to 0.611 | 0.758 to 1.155 | 1.1 to 1.2 |
| 1,000 | 2.357 to 2.499 | 3.037 to 3.837 | 2.4 to 2.5 |
| 2,000 | 5.808 to 5.963 | 7.963 to 9.932 | 2.9 to 3.0 |
| 3,000 | 8.164 to 8.325 | 10.466 to 21.958 | 2.7 to 2.8 |
| 5,000 | 13.209 to 13.607 | 14.962 to 16.630 | 2.6 to 2.7 |

Full detail, a peopled world. The generated region of seeds 2026 and 7 at 128 cells, its villages' people raised by armies homed there on the open cells nearest each centre, with 1,800-tick days:

| People | p50 (ms) | p99 (ms) | p50 per person (µs) |
|---|---|---|---|
| 500 | 0.403 to 0.410 | 0.665 to 0.743 | 0.8 |
| 2,000 | 1.947 to 1.965 | 3.070 to 3.347 | 1.0 |
| 5,000 | 4.720 to 5.919 | 8.195 to 11.805 | 0.9 to 1.2 |

The same at 512 cells, seed 2026, gave p99 0.787, 4.084 and 7.369 ms at 500, 2,000 and 5,000 people. The daily rules, timed apart with 60-tick days at 5,000 people, cost 35 to 47 ms on the first day, 12 to 13 ms on the second, and 2.0 to 6.2 ms on each day after, over two runs.

So full detail everywhere holds the budget up to about 1,000 people fighting or about 2,000 living in villages, and the bound the user asked for needs a level of detail. Options weighed:

- **What a tier is.** The world is divided into areas of 32 x 32 cells, 256 at 512 cells, the same areas the coarse graph of ADR 0043 is built on. Each area has a tier, and an army acts at the nearest tier among the areas its living members stand in, so an army is never split across tiers. Chosen.
  - Rejected: a tier per person by its own distance. It costs a distance per person per tick, and an army strung along a boundary would act at two rates.
  - Rejected: ADR 0005's aggregates of count and strength. They erase the individuals ADR 0017 made.
- **The focus.** Distance is counted, in areas along each axis, from the nearest area holding a living member of an army a driver steers (ADR 0038's steered mark, which is saved and hashed). A world with no steered army, or whose rules give no tiers, is live everywhere, as every world is today. Chosen.
  - Rejected: a focus point set by the driver apart from what it steers. That is a second input beside the steered mark, which ADR 0017 removed once.
- **How many tiers.** Three. Chosen.
  - Live: areas within the live radius, 2 areas to start with, 5 x 5 areas or 160 cells across. That is several times the view: on a PC it shows at least 640 x 360 world pixels (`view_scale.gd`), 40 x 23 cells at 16 pixels a cell, and a window only shows more where it is not a whole multiple of them. Rings of areas are added nearest first while the people in them stay within the live cap, 1,000 to start with, the most measured fighting within the budget. The ring holding the focus is always live.
  - Stepped: areas within the stepped radius, 4 areas to start with.
  - Far: the rest.
  - Rejected: live and far only. A battle just past the screen would be resolved as numbers and then appear placed. People walking in from far would appear at their army's anchor rather than where they were walking.
  - Rejected: live and stepped only. It fits the M1 Mac by the figures above. But every person far away would keep a position, a share of the spatial hash and a flow field. ADR 0043 measures a flow field over the whole map at 3 MB, and phones are slower.
- **The live tier.** Everything as today, on every tick.
- **The stepped tier.** The same per-tick rules, run by each person on one tick in every `stride` ticks, 8 to start with, its phase its index modulo the stride.
  - A move covers the stride's ticks: 8 x `SPEED` is 128 units, half a cell, so it crosses at most one cell edge and is checked as a step is today.
  - A strike is taken on the first acting tick at or after the person's ready tick. The next ready tick counts from the due tick, not the acting tick, so a stepped person strikes as often over time as a live one.
  - Rules counted in ticks from the tick count, such as qi recovery every `recovery_ticks`, are unchanged.
  - Rejected: a stepped tier at every tick with fewer neighbours read. It saves less, and changes the separation rule rather than its rate.
- **The far tier.** A far army has no position work.
  - It has an anchor: its village for residents at home, otherwise a cell on its coarse path. It advances along the path once a second by 30 x `SPEED` of path cost, the speed a live person walks.
  - Where a far army meets a hostile one or a village it raids, the far strike rule resolves them once a second. Each living person deals its order's skills' expected body damage over the second, the strike rule's might, edge and potency included. It lands on the other side's living people in roster order, each taking damage until it dies, then the next. Expected structure damage lands the same way on the target's structures in index order, and practice is counted as a strike's would be. Reach is not modelled: every far person is engaged.
  - A manual lying on a cell is read only in a live or stepped area.
  - Rejected: no fighting far away, with raids resolved when the player arrives. The world would then not change without the player, against the concept's first pillar.
- **Rules that are not tiered.** The daily rules read no position, or only a village's centre and a person's home: kinship, husbandry, hunger and migration, growth, sects, the election, diplomacy, ageing and cultivation. They run for every person in every tier, so a far village bears, starves and grows as a near one does. Rumours and talk read a far person's position as its army's anchor. Growth lays a structure clear of live and stepped people as today. Far people are placed afterwards on the cells left open.
- **Natural objects (#187).** Objects do not move, so each keeps its cell and stage in every tier, and no tier materialises them. Their growth, spread and regrowth are daily rules, untiered, bounded per area (ADR 0041). Gathering follows the rule of who gathers: a live or stepped person walks to an object and takes it by the per-tick rules. A far village's gatherers take, once a day, what the same people would gather over a day. The objects are taken within their reach of its centre, in index order. So the counts agree in expectation, as fights do. #187 decides the rates.
- **Moving between tiers.** Tiers are computed once a second, on ticks the second divides. With hysteresis, an area moves nearer at once and farther only once its distance passes the radius by one area, so walking along a boundary does not change tiers.
  - Into live or stepped from far, an army's members are placed in roster order on the open cells nearest its anchor. A village's residents are placed nearest its centre, as the region stands them today.
  - Health, qi, cooldowns, casts pending and everything else on the person carry over unchanged.
  - Into far, an army's anchor becomes its village, if all its members are at home in it, or else its lead's cell, and its far progress starts from that cell's place on its coarse path.
  - Between live and stepped, only the rate changes.
- **Near and far agree.** Some quantities are conserved exactly in every tier:
  - people on rosters
  - births and deaths, by the same rules
  - food, durability and attainment

  Some rates are equal: a person's walking speed, its strikes over time, and every daily rule. Outcomes of a fight agree in expectation, not tick for tick. Tests measure it: the same raid resolved live and far over many seeds must end with the same side holding the village on most seeds, and with survivors within a margin. The implementing pull request sets both thresholds from its measurement.
  - Rejected: requiring identical outcomes. Only one tier could then exist.
- **Determinism and the hash.** Tiers follow from hashed state: the steered marks, the positions of the steered and the areas' hysteresis. The hysteresis, the anchors and the far progress are world state, saved and hashed. Like the declarations they are folded only while tiers are in the rules, so a world without tiers keeps the pinned hash. Phases and roster order are fixed by index, so the order people are read in still decides nothing. Replaying the same orders, casts and steers gives the same hash.

## Decision

The world is divided into areas of 32 x 32 cells, each at one of three tiers by its distance from the nearest area holding a person in a steered army. With hysteresis, tiers are recomputed once a second.
- Live areas, within a radius and a cap on the people in them, run every rule on every tick.
- Stepped areas run the same rules on one tick in every stride, at the same rates.
- Far areas move armies along the coarse graph at walking speed and resolve fights by an expected-damage rule landing on individuals in roster order.

The daily rules are not tiered. Every person keeps its record in every tier, and the radii, cap and stride are rules data. A world with no steered army or no tiers runs live everywhere. This would supersede ADR 0017's "no levels of detail" and its focus removed; its individuals, rosters and bound stay until ADR 0045's measurement sets the bound.

## Consequences

- Every rule written from here on states what it does in each tier, or that it is a daily rule and untiered. A rule that reads positions every tick has a far form or does not run far. Issues #170, #171 and #172 are written that way.
- Every existing test keeps full detail: none steers an army in a world with tiers.
- The pinned hash is unchanged until a test steers an army in a world with tiers.
- A fight far away is less exact than one on screen. A raid the player walks into may stand differently from how the far rule left it, within the consistency test's margin.
- The daily rules at 5,000 people cost 2.0 to 6.2 ms on a day's tick after the second day, measured. The day's tick then runs past 4 ms by that much. ADR 0045 measures it against one frame, and spreads the daily rules over the day's ticks if it does not fit.
- The live cap is a count of people, so a crowd near the player narrows the live tier rather than missing the budget.

## Signs the decision was wrong

- The consistency test cannot be made to pass with a far rule that stays this simple, and the far rule grows a case per kind of fight.
- Armies near a tier boundary change tiers every few seconds despite the hysteresis, and placing them shows in play as people jumping.
- The player sees a battle's outcome change when walking toward it, often enough to name it.
- The stepped tier's half-cell moves read as stutter at the edge of a zoomed-out view.
- A rule needs a person's position far away every tick, and the far tier has to keep positions after all.
