# ADR 0072: Gate arts on insights gained by manuals, nature and repetition, and let arts grant passive skills

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-11, in issue #257, extending ADR 0055:
- **Insight (깨달음) is separate from arts (무공).** An insight is gained from a manual (무공서); naturally, over time and conditions such as meditation, seasons or a place; or by repeating an action, such as sword strikes, swimming in rapids, chopping wood, leaping cliffs (ADR 0058) or surviving a near death.
- **Learning an art has requirements:** certain insights, a minimum realm (경지), or sometimes a lower art learned first. If any is missing, learning fails. The design states what the failure costs, and the attempt says what was missing.
- **An art grants several skills.** Some are active, the Q to T casts. Some are passive and always on, such as regeneration, damage by family, fall damage, sight and stamina. Passives are pack data, folded into one resolution like everything else, with no per-tick work for an unchanged passive set.
- **The lineage should be visible at a glance.** The window's layout is pending with the owner, among the PM's mockups. The data is designed so any of those layouts can read it.

The issue also asks for repetition counters that are deterministic and seeded, their cost at 500 and 5,000 living, and NPCs gaining insights by the same rules; passives' stacking and their fold into strike resolution (ADR 0046) and movement (ADR 0058); and `make sweep` 48/50 and `make bench` in each rule step.

What this builds on, from `main` at `3dfecbb`:
- **ADR 0035.** Qi is a current amount and a store. The store grows daily by cultivation, 0 to 2 qi a day, up to a talent limit of 1,000 to 15,000, and qi recovers a hundredth of the store a second. Realms are thresholds on the store, each with a might on body damage. A manual lying on a cell is read by the nearest person within reach who has the rank and the qi, spending a reading cost. Sect teaching gives a yearly chance. A person's nature is its first art's, and `Life.learning` gives an ease per mille that multiplies learning chances.
- **ADR 0055,** proposed, merged in #218. Arts have a family, naming a discipline and a weapon family, and a tier of five. Each known art carries a mastery to 10,000 in five stages, raised by hits, training and teaching and capped by talent. Forms open by stage and realm. Higher arts come by a daily synthesis chance over a lineage graph of pack data. The soul keeps a bit for each art it knows of, and the lineage window on L shows ??? for the rest. What footwork and heart-method arts do beyond their skills is left open.
- **ADR 0036.** The core notes each skill learned, with how, and each realm reached, in a store of the latest 32, which the HUD's notices read.
- **ADR 0046.** Every skill is a shape resolved by one function, and damage lands after everyone has struck.
- **ADR 0049.** Content is pack data, and the core names no theme word. A pack past a core bound is refused at load with a `ContentRefusal`.
- **ADR 0050.** A weapon family gives the basic strike, and an art's skill needs its discipline's weapon family.
- **ADR 0054.** Deeds and witnesses read sight.
- **ADR 0058,** proposed, merged in #223. Each level leapt up costs 40 stamina, scaled down by the person's best footwork stage, 1,000 per mille at none to 500 at the highest. There are no falls: a leap a person cannot make is refused.
- **Open PRs it cites:** #241's ADR 0067 (animals, whose kills a practice may count) and #248's ADR 0065.
- **Not built.** Swimming: deep water blocks people on foot (ADR 0047), so no rule counts swimming yet.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000.

Options weighed:

- **Insights as pack data.** An insight is a kind of the pack: an id, its display fields in `view/` and `text/` (ADR 0049), and its sources, any of the three below. The core knows an insight only by index.
  - **A person's insights** are a set of one bit an insight. A pack defines at most 256, so the set is 32 bytes.
  - **Known of.** The soul's set of arts it knows of (ADR 0055) gains a set of insights it knows of, 32 bytes more, so the window can show ??? for an unknown insight as for an unknown art.
  - Rejected: insights as mastery on an art. The owner made them separate, and one insight may open several arts of different families.
- **Source 1, a manual.** A manual's kind names the insights its reading gives, at most 4. A reading that succeeds gives them with the art. A manual of insight alone, naming no art, is allowed, so a pack can scatter treatises.
- **Source 2, nature.** An insight may name natural conditions, each pack data from a fixed set the core reads, all of which must hold at the daily pass:
  - a season, or any
  - the biome of the person's block, or any (ADR 0047)
  - a least elevation level of its cell, or any (ADR 0058)
  - a least realm rank
  - standing within its sect's seat (ADR 0026), the base pack's meditation

  Then, once a day, the person rolls the insight's chance per mille a year, scaled by its talent limit in the insight's discipline over that discipline's mean limit and by the ease, as ADR 0055's synthesis rolls. The integer roll is synthesis's: multiplied first, divided last, drawn from the world's seeded generator in person order, then insight order.
- **Source 3, repetition.** The pack declares at most 16 practices, each a core action kind with a filter, and a person keeps a 2-byte counter for each, saturating at 65,535:

  | Action kind | Filter in the pack | Counted when |
  |---|---|---|
  | a hit made | a weapon family, or any | a use of the person's lands on anyone or anything (ADR 0046) |
  | a hit taken | none | a use lands on the person and it lives |
  | a leap up | a least height in levels | a leap lands (ADR 0058) |
  | a gathering | a slot kind (ADR 0056) | a gathering completes |
  | a kill of an animal | an animal kind, or any | an animal the person strikes dies (#241's ADR 0067) |
  | a mend | none | a structure the person mends gains durability |
  | a near death survived | none | the person's health fell to a tenth of a newborn's or below, and it lives at the next daily pass |

  An insight with a repetition source names a practice and a threshold. Once a day, a person whose counter stands at or past the threshold, and who meets the insight's natural conditions if any, rolls its chance as above. So repetition makes the insight likely, never certain on the day, and two people with the same counts differ by talent and the seed.
  - **Swimming in rapids** is left out. No rule lets a person enter water, so nothing can count it. A pack adds the practice once a swimming rule exists.
  - Rejected: a counter per insight. A person would carry up to 256 counters, 512 bytes, most of them for insights it will never near. Practices shared by insights keep it at 32 bytes.
  - Rejected: an insight gained on the threshold's crossing alone. The owner asked for something seeded, and a deterministic count would let every soldier of one battle gain the same insight on the same tick.
- **NPCs** gain insights by the same rules: their hits, leaps and gatherings count, and they roll the same daily chances. No rule reads whether a driver steers the person.
- **Requirements on arts.** Each art carries pack data of what learning it needs, all of which must hold:
  - at most 4 insights
  - a least realm rank
  - at most 2 prerequisite arts, each known at a least mastery stage (ADR 0055)
- **The learning attempt.** Every way to gain an art is an attempt that checks the requirements first:
  - **A manual** (ADR 0035). A person that meets the manual's rank and qi reads it, spending the reading cost. If a requirement fails, the reading fails: the cost is spent, the art is not learned, and the person may not try that manual again for 30 days. The 30 days stop a reader standing on a manual from failing every tick.
  - **Sect teaching.** The yearly chance is rolled only for an art whose requirements the member meets, so teaching never fails; the master teaches what the pupil can take.
  - **Synthesis** (ADR 0055). An edge's roll is made only when the higher art's requirements hold beside the edge's own, so synthesis never fails either. Nothing is spent on a roll not made.
  - **What was missing.** A failed attempt is noted in ADR 0036's store with what was missing: the insights lacked, the realm short, and the prerequisite arts or stages lacked. The HUD announces the player's from it, as it announces a skill learned.
  - **The cost of failing,** for a manual: its reading cost in qi, and 30 days before that person may try that manual again. The last failed attempt's day is 2 bytes on the person, and the manual it was is read from the store's entry.
  - **Qi deviation** is not designed here. Forcing an art above one's realm or of the wrong nature is a [proposed] risk in `concept.md`, and the owner's question below.
  - Rejected: a failure that consumes the manual. A manual lies on a cell and stays where it lies (ADR 0035), so consuming it would be a second rule for manuals.
- **The lineage graph, extended.** ADR 0055's graph is pack data of art nodes and synthesis edges. It gains:
  - insight nodes
  - requirement edges, each from an insight or a prerequisite art to an art, with the stage needed for an art
  - a realm requirement on each art node, not an edge, since every art has one

  The core exposes, for a person, each node's state, known, known of or unknown, and each edge's whether met. Any layout of the window, a tree, a grid by family and tier, or a radial map, reads the same nodes and edges and decides nothing itself.
- **Skills granted, active and passive.** An art's skills are ADR 0035's, each opening at a mastery stage and a realm rank (ADR 0055). A skill is now one of two kinds:
  - **Active,** as today: a shape, a cooldown and a qi cost, cast from the loadout's Q to T or drawn by the automatic strike.
  - **Passive:** at most 4 effects, each an effect kind the core reads and a value. A passive is on from the moment its stage and rank open it, for as long as the art is known, whatever the loadout holds. It costs no qi and has no cooldown.
- **Effect kinds** the core reads, named in the pack:

  | Effect | Value | Read by |
  |---|---|---|
  | health regeneration | health every 30 ticks | the recovery pass that restores qi (ADR 0035) |
  | qi recovery | per mille added to the recovery rate | the same pass |
  | damage by weapon family | per mille on body damage, for strikes of one family | strike resolution (ADR 0046) |
  | sight | cells added, within ADR 0046's reach | `sees` (ADR 0058) |
  | stamina maximum | stamina added | ADR 0058's stamina |
  | climb cost | per mille on the stamina a level costs | ADR 0058's leap |
  | descent | levels added to the descent limit | ADR 0058's leap |

  - **Fall damage** has nothing to act on: under ADR 0058 nobody falls, since a leap a person cannot make is refused. The owner's "fall damage" is served by the descent effect, which lets a person jump down further.
  - **Footwork's climbing effect** becomes a climb-cost passive on each footwork art's stages: 1,000 per mille at none to 500 at the highest, the base pack's numbers unchanged. This replaces ADR 0058's rule that reads the best footwork stage.
- **Stacking.** A person's passives fold into one record of totals:
  - Additive effects, regeneration, sight, stamina and descent, add across passives.
  - Per-mille effects take the strongest per kind and per family, not the product, so two arts' passives cannot compound past either. A climb cost takes the lowest.
  - Each total is clamped to a core bound below.
- **No per-tick work for an unchanged set.** The totals are derived and cached on the person, 16 bytes: recomputed only when an art is learned, a stage is reached, or a realm rank changes, and on load. They are not saved, since they follow from what is. The rules read the cache:
  - Strike resolution multiplies body damage by the striker's family total, beside might and potency (ADR 0035), in the order the resolution already multiplies, per mille and rounded down.
  - The recovery pass, every `recovery_ticks`, adds regeneration to health, up to a newborn's health, for those whose total is above 0, beside the qi it restores.
  - `sees` adds sight to the reader's reach. ADR 0058's leap reads stamina, climb cost and descent.
- **The base pack.** Ten insights, and requirements on the starting graph's arts:

  | Insight | Sources |
  |---|---|
  | Sword Intent (검의) | Three Talents Sword's manual; or 1,000 sword hits, 200 per mille a year |
  | Saber Intent (도의) | Five Tigers Saber's manual; or 1,000 saber hits, 200 per mille a year |
  | Iron Body (철신) | 2,000 hits taken, 100 per mille a year |
  | Weightless Body (경신) | 200 leaps up of 2 levels or more, 200 per mille a year |
  | Woodcutter's Rhythm (벌목의 율) | 500 trees felled, 100 per mille a year |
  | Brink of Death (생사경계) | 3 near deaths survived, 300 per mille a year |
  | Breath of Heaven (천기) | standing at elevation 5 or more, realm rank 2, 50 per mille a year |
  | Flowing Water (유수) | spring, by a river's biome, realm rank 1, 50 per mille a year |
  | Hunter's Patience (사냥꾼의 인내) | 100 animals killed, 100 per mille a year |
  | Stillness (정심) | within its sect's seat, 30 per mille a year |

  - Plum Blossom Sword needs Sword Intent, rank 2 and Three Talents Sword at minor. Heavenly Demon Saber needs Saber Intent and rank 2. Cloud Ladder needs Weightless Body. Purple Mist Divine Art needs Breath of Heaven and rank 3. Hundred Paces Divine Fist needs Iron Body and Brink of Death. The rest need only their realm rank.
  - **How long it takes.** A fighter making 20 sword hits a skirmish reaches 1,000 in 50 skirmishes, and then gains Sword Intent within about 5 years at 200 per mille a year, faster with talent. A disciple standing in its seat rolls Stillness at 30 per mille a year, about 33 years on average, so most never gain it.
  - **Passives.** Each footwork art carries the climb-cost passive as ADR 0058 sets it, and Cloud Ladder adds 2 to descent at consummate. Each heart-method art adds 100 to 300 per mille to qi recovery by stage, and Iron Body's arts, the fist family's, add regeneration of 1 to 3 health every 30 ticks. A sword art at major adds 100 per mille to sword damage.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - at most 256 insights
  - at most 16 practices
  - at most 4 insights a manual gives
  - at most 4 insights, 2 prerequisite arts and one realm rank an art requires
  - at most 4 effects a passive skill holds
  - totals clamped to: regeneration 50 health every 30 ticks, qi recovery 1,000 per mille, damage 1,000 per mille added, sight 4 cells, stamina 500, climb cost no lower than 250 per mille, descent 8 levels
- **Memory, saves and the hash.** A person gains its insights, 32 bytes; its practice counters, 32; the day of its last failed attempt, 2; and a near-death flag in a byte with room to spare, 1; 67 bytes in all, saved and hashed. The soul gains 32 bytes of insights known of. The passive totals, 16 bytes, are derived and not saved.
  - At 500 people that is about 34 KB, at #216's live tier of 5,000 about 335 KB, and at 50,000 about 3.4 MB.
  - `SAVE_VERSION` rises. The pinned hash changes, since the new state is folded.
- **Cost.**
  - **Counting.** A counter rises where the event already happens: a hit, a leap, a gathering, a kill, a mend. Each event finds its practices in a list built at load for each action kind, at most 16. In a battle of the live tier, about 300 hits a tick count for strikers and struck, about 600 additions with at most 16 filters each, under 10,000 checks a tick.
  - **The daily pass.** For each person, the insights not yet known whose practice stands past its threshold or whose sources are natural. A base pack of 10 insights is 10 checks a person. At 50,000 people spread over 1,800 ticks, about 28 people and 280 checks a tick, and a pack of 256 insights at most about 7,200, a few hundredths of a millisecond.
  - **Attempts.** A manual's check reads at most 4 insights, a rank and 2 arts.
  - **Passives.** Recomputed on change, at most 16 arts × 3 skills × 4 effects, 192 reads, a few times in a person's life. Strikes and leaps read the cached totals, one read each.
- **Tiers.** Counting happens wherever the event does. The daily pass is untiered (ADR 0042). A far fight's hits count by the same rule as its far resolution.

The implementation, in order, each a pull request with its acceptance, after ADR 0055's mastery and graph are built (its steps 1 and 2):
1. **Insights, practices and the daily roll.**
   - Acceptance, each with a control: a practice's counter rising on its action and not on another's; an insight rolled only once its threshold is passed, where one short never rolls; a natural insight rolled only under its conditions; the same roll for a person no driver steers as for the player; the counters and sets saved and hashed.
   - Acceptance: `make bench` within budget with the live tier's battle counting, and `make sweep` 48/50 or better.
2. **Requirements and the learning attempt.**
   - Acceptance, each with a control: a manual read by one who meets the requirements, failing for one missing an insight, with its cost spent, the 30 days kept, and the store noting what was missing; teaching and synthesis never rolled for an art whose requirements fail.
   - Acceptance: `make bench` within budget, and `make sweep` 48/50 or better, with the sweep printing insights gained by source and failed attempts by what was missing.
3. **Passive skills and their fold.**
   - Acceptance, each with a control: a sword passive raising sword strikes' damage and not saber strikes'; two per-mille passives taking the strongest; regeneration restoring health on the recovery pass, and none for a person without it; a footwork passive giving ADR 0058's climb cost with the same heights as today; totals recomputed on a stage reached and not on a tick without change.
   - Acceptance: `make bench` within budget, and `make sweep` 48/50 or better.
4. **The window and the loadout,** after the owner chooses a layout from the mockups. The lineage window draws insight nodes and requirement edges, met or not, and ??? for an unknown insight. The K panel lists an art's passives as always on, apart from the five slots. Acceptance: captures (ko) of each.

## Decision

Insights are pack data, apart from arts. A person holds a bit for each of up to 256, gained from a manual, from natural conditions of season, biome, height, realm or its sect's seat, or by repetition: up to 16 practices, each a core action kind with a filter, counted on the person. Once a day a person whose conditions hold, or whose practice stands past an insight's threshold, rolls the insight's chance, scaled by talent and ease as synthesis is. NPCs gain insights by the same rules.

An art requires up to 4 insights, a realm rank and up to 2 prerequisite arts at a stage. A manual's reading fails without them, spending its cost and barring that manual for 30 days, and ADR 0036's store notes what was missing. Teaching and synthesis are rolled only for arts whose requirements hold. The lineage graph gains insight nodes and requirement edges, and the core exposes each node's state and each edge's met, for any layout.

An art's skills are active or passive. A passive holds up to 4 effects: regeneration, qi recovery, damage by weapon family, sight, stamina, climb cost and descent. Additive effects add and per-mille effects take the strongest, clamped, into a cached record of totals recomputed only when the passive set changes. Strike resolution, the recovery pass, `sees` and the leap read it.

This supersedes ADR 0055's gaining an art by manual, teaching or synthesis without requirements, and answers its open question of what footwork and heart-method arts do beyond their skills, as passives. It supersedes ADR 0058's climb cost read from the best footwork stage, now a footwork passive with the same numbers. It extends ADR 0035's skills with passives and ADR 0036's store with insights gained and attempts failed.

## Consequences

- A person's path in the arts now runs through what it has done: a woodcutter, a cliff-leaper and a veteran of near deaths open different arts.
- Manuals become worth more than their art: a treatise of insight alone is a prize.
- Players see why an art will not come, in the store's note, rather than waiting on a hidden roll.
- Passives give heart methods and footwork a use without a slot, and a person's strength reads partly in what it carries, not only what it casts.
- Saves grow by about 3.4 MB at 50,000 people, and the pinned hash changes.

## Signs the decision was wrong

- Insights from repetition reach every soldier of a long war, so the arts gated on them stop being rare, and thresholds or chances need raising.
- Natural insights never happen in the sweep, because their conditions rarely hold together.
- Players stand on a manual and fail every 30 days without learning what to do, so the note needs to point at a source.
- Passives' strongest-per-kind rule makes a second heart method worthless, so stacking needs a diminishing sum instead.
- Counting on every hit shows in the battle's tick at the live tier, so practices need a coarser grain.
