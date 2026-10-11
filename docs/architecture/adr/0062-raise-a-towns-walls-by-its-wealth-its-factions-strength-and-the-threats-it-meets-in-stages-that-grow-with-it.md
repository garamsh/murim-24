# ADR 0062: Raise a town's walls by its wealth, its faction's strength and the threats it meets, in stages that grow with it

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-10, in issue #228 and its comment:
- A village becomes a walled town as it and its faction grow. Whether it has walls, and how great they are, depends on the faction's strength and the village's wealth.
- Building walls, like building the village itself, takes materials and labour.
- **Walls are part of the village, and they grow with it.** When the village or its faction expands past its walls, the town builds outward:
  - a new, larger circuit is planned around the grown town
  - the old wall stays as an inner wall around the old core, or is taken down and its stone reused, with mass and materials conserved
  - suburbs outside the walls come first, and the new wall follows once wealth and labour allow

The issue asks the design to decide these:
- when a village plans walls, by its population and size, its wealth, its faction's strength and treasury, and the threats it has met, as deterministic rules, with the faction's leader deciding when a treasury pays
- what walls cost in materials and labour, and whether a faction can levy corvée, with discontent
- how long they take, in stages, with partial walls counting where they stand
- what a wall does in the rules: blocking except at gates, absorbing strikes, guards at gates, besiegers breaching
- upkeep, decay and repair, and what happens when a town shrinks or its faction falls
- far tiers in aggregate, and the memory and tick cost at the bound

It also asks how a wall's plan follows the village's footprint, and what is kept or reused.

What this builds on, from `main` at `c4e24a7` and the open PRs it cites:
- **ADR 0056,** proposed, merged in #220. Structures cost materials as pack data, gathered into a village's account and carried to the site. The repair rule raises a structure only as far as its materials are delivered. Gatherers and builders are paid from the owning faction's treasury, at most a third of earners each and half together.
- **ADR 0052.**
  - The NPC leader's daily rule weighs the treasury against 60 days of its needs.
  - Discontent per household rises with tax, hunger and kin killed, and households flee past 800.
  - Upkeep of a faction's own structures is a sink.
- **ADR 0023.** Growth lays a plan as rubble when a village presses its cap, by its faction's stance. A blocking structure triggers path rebuilds (ADR 0043).
- **The open PR #242, for realistic scale (#227).** Its reference sizes, with the PM's decisions on it:
  - a walled town of at least 40 × 30 cells inside the wall, and a city of at least 80 × 60
  - a wall 2 cells thick and 2 levels high for a town, and 3 and 3 for a city
  - corner towers of 3 × 3, and 2 to 4 gates, each 2 to 3 cells wide under a 5 × 2 gate tower over a road of 3 cells, as #239 draws it
  - #239 has drawn the walled town's pieces as art, in the view only: wall runs, corner towers, gate towers and steps. Its wall is one level high for now, an exception the art will raise to #242's 2 levels.
- **ADR 0057,** proposed, merged in #222. Materials absorb strikes, rammed earth and stone wall among them, and breaches.
- **ADR 0058,** proposed, merged in #223. Every cliff is crossed by a leap: a person leaps up a cliff within its climb height, set by realm and footwork and paid in stamina, and jumps down one within twice it. Rank 0 leaps up 2 levels.
- **Bandits** are #172's, not yet designed.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000.

Options weighed:

- **Walls as structures.** A wall is laid as segments: structures of 4 cells along it by its thickness, so a town's circuit is about 40 segments. Corner towers and gate towers are structures of their own, and a gate is a gate tower's passage. Every segment is laid as rubble and raised by ADR 0056's rule, so a partial wall stands and blocks where its segments stand.
  - Rejected: a wall as one structure for the whole circuit. A breach anywhere would bring it all down, and none of it would count until all of it stood.
- **When a village plans walls.** Every 30 days, with growth (ADR 0023), a village with no circuit plans one when all four of these hold. The thresholds are pack data:
  1. **Scale.** It has at least 150 residents, and its built cells span at least a town's 40 × 30 (#242).
  2. **Wealth.** Its households' mean purse is above 30 days of their needs (ADR 0052), and its account holds the first stage's materials for a tenth of the circuit.
  3. **Its faction's strength.** The owning faction's treasury holds 60 days of its needs plus the first stage's wages, and the faction has at least 3 villages.
  4. **Threat.** Its threat is at least 200.
     - A village's threat is a counter, 2 bytes.
     - It rises 100 for each raid on it, 50 when its faction is at a declared war, and 20 for each day a hostile army stands within 32 cells. Bandits nearby (#172) will add to it once designed.
     - It falls 1 a day.

  A leader no driver steers funds the works, by ADR 0052's rule, while the treasury stays above its reserve. A player who leads orders a circuit by hand, under the scale condition alone.
- **The plan follows the footprint.**
  - The circuit is the smallest octagon, sides along the eight directions, enclosing the village's built cells with a margin of 4 cells, its corners cut by the map's blocked cells and water.
  - A gate is placed where each road or track leaving the village crosses the circuit, at least 2 and at most 4: the busiest by the roads' class, then by index.
  - The circuit's segments, corner towers and gate towers are laid as a growth plan, segment by segment around from the gate on the main road. The village's houses stay where they are.
- **Stages.** Each segment is raised in up to three stages, each a structure kind's upgrade laid over the last:

  | Stage | Material | Cost a segment | Durability | Raised by a crew of 6, the whole circuit |
  |---|---|---|---|---|
  | Earthwork | rammed earth | 16 wood for the forms | 8,000 | about a year and a half |
  | Stone | stone wall | 120 stone, 8 wood | 40,000 | about 7 years more |
  | Towers and gate towers | stone wall | a tower 200 stone and 60 wood, a gate tower 300 and 100 | 20,000 | about a year more |

  - **The crew.** At most 6 builders work on a village's walls at once, pack data, beside its builders for other plans.
  - **Where the figures come from.** A circuit of a 40 × 30 town is about 40 segments: about 4,800 stone and 4 to 8 towers. At the repair rule's 5 durability every 30 ticks a builder (ADR 0023), which ADR 0056 keeps, a day of 1,800 ticks raises 300.
    - Earthwork: 40 segments of 8,000 are about 1,070 builder-days, about 180 days for 6, a year and a half at 120 days a year.
    - Stone: 40 of 40,000 are about 5,330 builder-days, about 890 days for 6, over 7 years.
    - Towers: about 10 of 20,000 are about 670 builder-days, about 110 days for 6.

    So a full circuit takes about 10 years, as the owner's "years" asks, and corvée shortens it.
  - **Ordering.** Each stage starts only when the circuit's previous stage stands whole, so a town is walled in earth before it is walled in stone.
  - The costs and durabilities are pack data.
- **Labour, and corvée.** Builders are ADR 0056's, paid from the treasury, at most a third of the village's earners. A faction may also levy corvée while the village's threat is at least 400, pack data:
  - each household sends one earner unpaid one day in ten
  - each day of corvée raises that household's discontent by 3 (ADR 0052). Against ADR 0052's decay of 2 on a good day that alone nets 1, so #248's ADR 0065 suspends the decay on a corvée day, and the 3 is net

  So a threatened town walls itself faster at the cost of its people's mood. Whether corvée is allowed at all is listed for the owner.
- **What a wall does.**
  - **Blocking.** Segments and towers block movement (ADR 0013).
  - **Gates.** A gate tower's passage stays open, except that it closes while a hostile army, one whose faction `hostile` reads, stands within 16 cells of it. A closed gate blocks like a segment. The town's residents assigned as guards stand in its passage, paid as soldiers (ADR 0052), 2 a gate.
  - **Absorbing.** A segment absorbs strikes by its material, rammed earth or stone wall, as ADR 0057 decides.
  - **Breaching.** Besiegers breach a segment by bringing it to its destroyed stage by the strike rule. A destroyed segment no longer blocks, so the breach is a way in.
  - **Climbing.** Walls are never climbed. A segment is a structure, not a level of the ground, so ADR 0058 does not see it as a cliff. Under that ADR's numbers every realm leaps up a 2-level drop, so a wall leapt as a cliff would stop no one. Climbing walls is listed for the owner.
    - Rejected: a wall of 2 or 3 levels as a cliff of that height. Levies would leap a town's wall as easily as masters.
- **Upkeep, decay and repair.**
  - Each standing segment and tower costs its owning faction 2 copper a day in upkeep, a sink under ADR 0052. A treasury that cannot pay lets its walls lose 2 durability a day.
  - Builders repair them by ADR 0056's rule when the village's account holds the materials.
  - **When the town shrinks** below the scale threshold, it keeps its walls. They are only upkept while the treasury can pay, so an emptied town's walls slowly fall to ruin.
  - **When the faction falls,** or the village changes owner, the walls pass with the village, as every structure does (ADR 0015).
- **Growing outward.**
  - **Suburbs first.** Growth lays houses outside the circuit when the free buildable land inside is used up, by ADR 0023's and ADR 0056's rules unchanged.
  - **A new circuit.** Once a fifth of the village's houses stand outside the circuit, and the four conditions above hold for the larger footprint, a new circuit is planned around it.
  - **The old wall.** It stays as an inner wall when the faction's treasury holds 120 days of needs. Otherwise it is taken down: builders lower each segment by the repair rule run backward. Its materials go back to the village's account in full, every unit, so the new circuit reuses its stone and mass is conserved (ADR 0057).
  - A gate in the old wall on a road stays open as an inner gate.
- **Tiers.** Planning, threat and upkeep are daily rules, untiered.
  - Building in a far village is aggregated as ADR 0056 aggregates it: a day of its builders' repair is applied to its sites.
  - Gates close by the same test in every tier, a far hostile army's anchor counting as where it stands.
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Memory.** About 100 towns of about 50 wall structures each are about 5,000 structures at ADR 0013's record, about 160 KB. Each village adds a 2-byte threat counter.
  - **Building.** A laid segment blocks, so its area joins ADR 0043's rebuild queue, #222's 8 areas a tick shared. A circuit's 50 structures touch about 12 areas over years, so this cost is negligible.
  - **Gates.** About 400 gates are checked once a second, each a spatial query of 16 cells, about 14 queries a tick.
  - **The daily plan check** is one pass over each village.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 3 wall stages
  - at most 4 gates a circuit
  - at most 2 circuits a village, an outer and an inner
- **Saves, the hash and determinism.** Wall structures are structures, already saved and hashed. The threat counter and each circuit's plan, its octagon, gates and stage, are saved and hashed too. Every rule is integer, and segments are laid and raised in index order.

The implementation, in order, each a pull request with its acceptance, after ADR 0056's building from materials is built:
1. **Threat, the plan and the stages.**
   - Acceptance, each with a control: a village planning a circuit only when all four conditions hold; the octagon and gates following its footprint and roads; earthwork, then stone, then towers, each waiting for the last.
   - Acceptance: `make sweep` showing towns walling over decades and not years.
2. **Gates, guards, breaches, upkeep and decay.** Acceptance, each with a control: a gate closing before a hostile army within 16 cells; a breached segment letting people through; walls decaying unpaid.
3. **Corvée and growing outward.** Acceptance, each with a control: corvée raising its households' discontent where a town without it raises none; a new circuit planned once a fifth of the houses stand outside, and not before; an old wall kept or taken down with its materials returned, by a sum test.
4. **Far aggregation and the view's walls**, drawn at their height in levels (#242, ADR 0058's compression), from #239's pieces.
   - Acceptance, with a control: a far town's circuit rising by a day of its builders' repair, the same stages in the same order as a live town of the same seed.
   - Acceptance: captures (ko) of a town's wall in each stage, a closed gate and a breach.

## Decision

Walls are laid as segments of 4 cells, corner towers and gate towers, each a structure raised by ADR 0056's rule, so partial walls stand and block where they stand.

A village plans a circuit, every 30 days with growth, when four things hold: it has reached a town's scale, 150 residents and 40 × 30 cells of building; its households and account are wealthy enough; its faction's treasury and size are strong enough; and its threat counter is at least 200. The counter rises with raids, war and hostile armies near, and falls 1 a day. An NPC leader funds the works while the treasury stays above its reserve, and a player leader orders them.

The circuit is an octagon around the built cells with a margin of 4, with 2 to 4 gates on the roads. It rises in stages: earthwork, stone, then towers. That takes years with ADR 0056's builders, and faster with corvée, which a threatened faction may levy for discontent.

Gates close while a hostile army is within 16 cells, and guards stand in them. Segments absorb strikes by their material, and a destroyed segment is a breach. Walls cost daily upkeep and decay unpaid.

Suburbs grow outside first. When a fifth of the houses stand outside, a new circuit follows, and the old wall stays as an inner wall or is taken down with its materials returned in full. Far towns build in aggregate.

This extends ADR 0023's growth with a wall plan, ADR 0056's building with stages, and ADR 0052's discontent with corvée.

## Consequences

- Towns visibly become walled as they prosper under threat, and the frontiers of wars wall themselves first.
- Wall building is a long, steady demand for stone and builders' wages, which ADR 0052's and ADR 0056's economies carry.
- Sieges become possible. Raids on a walled town need a breach or an open gate, so raids target open villages more.
- Paths are rebuilt as segments rise, a few areas over years per town.

## Signs the decision was wrong

- Every town walls itself in the first decade of the sweep, because threat accrues faster than it decays, so walls stop marking importance.
- No town ever walls, because the four conditions rarely hold together.
- Corvée drives households to flee faster than walls are built, so threatened towns empty.
- Closed gates trap a town's own people outside during long sieges, enough that the owner names it.
- Taking down an old wall leaves a town open for years while the new one rises.
