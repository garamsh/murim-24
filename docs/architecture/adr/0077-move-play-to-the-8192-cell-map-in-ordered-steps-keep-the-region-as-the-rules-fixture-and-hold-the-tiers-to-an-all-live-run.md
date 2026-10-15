# ADR 0077: Move play to the 8,192-cell map in ordered steps, keep the region as the rules' fixture, and hold the tiers to an all-live run

> Status: proposed; its step 3 built, as `simulation.md` records: until step 4 a far army's anchor is its lead's area and cell with no coarse node, and its progress stays 0 until step 5's far rules advance it
> Date: 2026-10-12

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #173 asks for one large, continuous world with distance LOD. Its first phase is design:
- the world's generation and its natural borders;
- the tiers: what each simulates, how a far village and the same village near give consistent results, and how determinism holds across them;
- hierarchical paths;
- the save's growth;
- how the population bound is chosen;
- measurements that justify the tiers;
- a worked plan of the implementation pull requests.

The owner's decisions on the issue moved its target in three steps. ADRs already decide most of it at the target now in force:
- **512 cells, generated (2026-10-08).** Designed by ADR 0041, superseded before it was built.
- **4,096 cells, by hand, with regions joined by gates and tiers by quality profile (2026-10-09).** Designed by ADRs 0042, 0043, 0044, 0047 and 0048. The owner asked for the bound measured from 10,000 to 50,000 on the lowest profile.
- **A larger world, with travel taking at least twice as long as on the 4,096 map (2026-10-11, #255).** That intent is the owner's, recorded in `concept.md` §World and scale. How to reach it is proposed, not owner-confirmed: ADR 0070, merged as proposed in #267, proposes 8,192 cells at half a li a cell, with the map's format, its memory, a level of regions on the coarse graph, the slots and the routes, in five steps. `concept.md` lists those details as **[proposed]**.

The PM directed on 2026-10-12 that this ADR designs the world and its LOD at ADR 0070's proposed 8,192 cells, reusing:
- ADR 0047's map tools;
- #216's live-tier budget;
- ADR 0076's spacing.

What exists on `main` at `1e3179c`:
- **The world** is the generated region of 128 × 128 cells, its villages placed by `generate_region` from `region_data()`. Every person runs every rule on every tick.
- **None of the large-world pieces is built:** no map compiler, areas, tiers, coarse graph or quality profile.
- **Content packs.** The base pack's port is open in #269. ADR 0049 puts the map in a pack, so the compiled map arrives through it.
- **`make sweep`** lives the region for seeds 0 to 49 over 100 years at 5 ticks a day, and holds 48 of 50 seeds with every faction peopled.
- **`make bench`** times the M1 battle of 500 and #216's live scenario of 5,000 people on 256 × 256 cells. The live target is 4 ms p99, not yet in force.
- **ADR 0053,** which ADR 0070 cites for #216's live tier, does not exist. #216 is open, and its target is the issue's: 5,000 people live within ADR 0045's 4 ms p99.

Measured on `1e3179c`, by a scratch program over the core's public types and save port:
- a person's record, `sizeof(Person)`, is 352 bytes;
- the live scenario's save, after 60 ticks, is 801,594 bytes for 5,000 people, 160 bytes a person, map rows included.

`make bench` at `1e3179c` on the development Mac, at a load average of 3.3 over 8 cores:
- the M1 battle of 500: p99 0.616 ms;
- #216's live scenario of 5,000: p50 3.719 ms and p99 3.962 ms, within the 4 ms target by 0.04 ms, about 0.79 µs a person a tick at p99;
- the frame: p99 14.3 ms against 16.7.

An earlier run at a load average of 73 gave the M1 battle a p99 of 47 ms, so it measured the machine and is not used.

Options weighed for the generated region once the map is built:
- **Keep it as the rules' fixture.** Chosen.
  - It is a small, seeded world on which `make sweep` lives 50 seeds for 100 years in minutes, and the core's tests build worlds of tens of cells.
  - The map is one world, and a sweep has no steered army, so it runs every area live (ADR 0042). A projection, not a measurement:
    - At the measured 0.79 µs a person a tick, 50,000 live cost on the order of 40 ms a tick.
    - A 100-year sweep is 60,000 ticks a seed at 5 ticks a day: about 40 minutes a seed, and more than a day for 50 seeds.
  - Play, the bench's bound and the view move to the map.
- **Retire it.** Rejected. The rules would lose their sweep, and every rule test would load a map of 67 million cells.
- **Keep playing on it beside the map.** Rejected. The owner chose one world.

Options weighed for consistency across tiers:
- **The tiered world against the same world run all-live.** ADR 0042 runs a world with no steered army live everywhere, so the same seed and map run both ways. They compare per village after 10 years: residents, standing structures, the owner, and whether it was founded or lost. Chosen. Their hashes differ by design, as ADR 0048 accepts. The check is on outcomes in bands.
- **Hash identity across tiers.** Rejected: the far tier resolves fights by expected damage, so no tiered world can hash as a live one.
- **No check.** Rejected: issue #173 asks how a far village and a near one stay consistent, and only a measured band answers it.

Options weighed for the spacing on the map:
- **ADR 0076's `apart`, read by the map's validator for the authored places as well as by founding.** Chosen. The map is authored, so its build is where generation's half of ADR 0076's rule runs. A city, town, seat or inn closer than `apart` to another refuses the map's build.
- **Spacing for founding only.** Rejected. Authored places would be free of a rule founding obeys, which is the special case ADR 0076 removes.

## Decision

Play moves from the generated region to ADR 0070's map in the order below. The map's 8,192 cells at half a li a cell are ADR 0070's proposal, not an owner decision; the owner's decision is the larger world with travel at least twice as long, and the plan holds whatever size ADR 0070 is accepted at. The region stays as the rules' fixture: the core's tests and `make sweep` keep it, and every rule runs the same on both worlds, with no branch by world.

**Tiers.** ADR 0042's tiers, with ADR 0048's profiles, run on the map's 65,536 areas.
- The live tier's budget is #216's: 5,000 people within 4 ms p99 on the standard profile. The low profile's cap is measured as ADR 0048 measures it.
- Consistency is a test, not a hash. The same map and seed run tiered and all-live for 10 years. Per village, the tiered run must keep:
  - residents within 15% of the all-live run's, averaged over 10 seeds;
  - the same owner in at least 9 villages in 10;
  - foundings and losses within 20% of the all-live count.

  The far tier is retuned until it does.
- Determinism holds within each run, and the hash folds tier state as ADR 0044 has it.
- **What a tier change hands over.** It reads only saved state, so a loaded world hands over as the saved one would:
  - **Live to far.** Every person keeps its whole record, as ADR 0042 has it: health, qi, cooldowns, home and household. Its army gains ADR 0044's far anchor, the area and coarse node its lead stands in, with far progress 0 along its coarse path to its objective. The area's fields are dropped.
  - **Far to live.** The lead is placed on the entrance cell its far progress reached on its coarse path. The other members are placed in roster order on the nearest free cells by the separation rule, read in cell index order. Every record carries over unchanged. The area's fields are rebuilt, and a stepped area's phase is the tick modulo its stride.

**Spacing.** ADR 0076's `apart` is checked by the map's validator for every authored settlement, inns included, and by founding in play.
- The base pack's value on the map is measured in the step that compiles the first map's content.
- It starts at 48 cells: a walled town's 40 and a margin of 8.

**The bound.** The population bound is ADR 0048's measurement on the low profile, on this map, as ADR 0070 has it.

**The worked plan.** These are the pull requests, in order. Each lands with tests and controls, `make ci`, and `make sweep` 48/50 on the region and `make bench` where it changes a rule:
1. **The map format at 8,192, through the base pack:** ADR 0070's step 1, after #269. Acceptance:
   - a test map compiled with its hash pinned;
   - a key route within its length, refused when shortened;
   - two settlements closer than `apart` refused, with a control at `apart` 0.
2. **A world on the compiled map:** `World` created from the map's footing, levels and authored places, with starting villages laid by ADR 0076's roles. Acceptance:
   - a world on the test map hashing the same twice, against a control of another map;
   - its villages placed where the map names them.
3. **Areas, tiers and profiles,** ADRs 0042 and 0048, with the tiers' state saved as ADR 0044 has it. Acceptance:
   - tiers recomputed with hysteresis, with a control of no steered army running live;
   - a save round-trip with the tiers' state;
   - **the handover keeps every person's record.** A world whose people hold non-default values in every saved field of their record is snapshotted per person before the steered army walks away. The fields are those `Person` holds on `main` today, inner energy (qi and its capacity), health, family, parents, spouses and children, home, sect, talent and attainment, the arts known, silver, items held, hunger, lifespan and soul, and any a later ADR adds before this step is built. Its other army is then handed live to far, then far to live, with no tick run between the handovers, so no rule can change a record. After each handover, every person's record equals its snapshot field by field, except the fields the handover names: position and the army's far anchor and progress. Its control corrupts the handover so it drops one preserved field, qi set to its default, and the same assertion is shown failing on that field;
   - a run that goes live, far and live again, by the steered army walking away and back, hashing the same twice. Two controls: another seed gives another hash, and the same run with the far phase skipped, tiers off, gives another hash than the tiered one, so the far phase is seen to act. This is reproducibility only: the record test above, not the hash, is what shows the handover preserves each person. Step 5 runs both tests again with the far tier's rules;
   - #216's live scenario within 4 ms p99 at an idle machine.
4. **The coarse graph with its level of regions:** ADR 0043 and ADR 0070's step 2. Acceptance: a path across the test map within a frame, 16.7 ms, on the development Mac at an idle machine, with a control of the graph of areas alone missing it.
5. **The far tier and the consistency check:** ADR 0042's far rules, and the test above. Acceptance: the bands, with a control in which a far tier that skips fights falls outside them.
6. **The first map's content at 8,192:** ADR 0070's step 4, with `apart` measured and set. Acceptance:
   - the validator's checks on the first map, and Luoyang to Chang'an within ADR 0070's bounds;
   - `apart` set to the largest of 32, 40, 48 and 56 at which every authored settlement passes the validator and `make sweep` keeps 48 of 50. Its control is the next larger value, shown refusing an authored pair or failing the sweep;
   - a test map without inns still compiling and validating, as ADR 0070 has it.
7. **The bound measured:** ADR 0048 on this map, on the low profile, from 10,000 to 50,000. Acceptance:
   - the bound is the largest N, in steps of 5,000, at which the median p99 of three runs is within 4 ms and the tick running the daily rules within a frame, 16.7 ms, at an idle machine;
   - its control is the next N above it, measured and shown failing, unless 50,000 itself passes;
   - the save and the memory at the bound, against this ADR's 160 and 352 bytes a person.
8. **The view at the map's scale.** Acceptance: captures in Korean of each.
   - ADR 0070's step 5;
   - #168's chart read by area, ADR 0073;
   - streaming by area, ADR 0059.

## Consequences

- **The region outlives the move as a fixture.** Every rule is tested and swept on it, and its numbers in `make sweep` stay the bands rules are judged by.
- **A rule that behaves differently at the map's scale** shows only from step 5's consistency check and step 7's bound. The region's sweep cannot catch it.
- **Memory per person.** At 352 bytes, 50,000 people hold about 16.8 MiB of person records, beside ADR 0070's 79 to 95 MiB of map. Steps 3 and 7 measure what tiers add.
- **Saves.** At 160 bytes a person, 50,000 people's state is about 7.6 MiB before the map's changed state, which ADR 0070 puts at its own size. Steps 3 and 7 measure the whole.
- **The far tier's rules are tuned against the live ones,** not by eye, and a change to a live rule can now fail step 5's bands.
- **ADR 0070's steps are folded into this plan as steps 1, 4, 6 and 8.** Its region level and slot cycle stand as it decided them.

## Signs the decision was wrong

- The region's sweep passes while the map's run fails a band. Then the fixture no longer stands for the world, and a reduced map, a few hundred areas, should replace it as the sweep's world.
- The far tier cannot reach the bands without the live tier's cost. Then tiers by distance save less than the cap they cost, and the live radius should grow instead.
- `apart` at 48 cells refuses settlements the map's history places closer, such as twin cities across a river. Then authored places need their own spacing, smaller than founding's.
- #216's live scenario, at 3.962 ms p99 today, cannot stay within 4 ms once tiers and the far tier's bookkeeping are added. Then the live cap of 5,000 is the wrong standard, and ADR 0048's profiles set it lower.
