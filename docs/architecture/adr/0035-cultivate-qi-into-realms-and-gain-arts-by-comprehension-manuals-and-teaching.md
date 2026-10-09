# ADR 0035: Cultivate qi into realms, and gain arts by comprehension, manuals and teaching

> Status: accepted; its realms, qi, cultivation and heart method named in the core, and its fixed orientations and polarities, superseded by ADR-0049, which gives them general names and makes the natures' axes pack data; its draw among all of a person's skills whatever it holds superseded by ADR-0050, under which an art skill needs its discipline's weapon family; its comprehension on a realm gained, its manual teaching every skill the rank allows, its teaching of the first skill lacked and its heart method as the first art learned superseded by ADR-0055, which opens forms by mastery stage, gains higher arts by synthesis and takes the heart method from its family
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept agrees qi and martial arts (`docs/concept.md` §Qi and martial arts):
- every person holds a store of qi, grown by cultivation and bounded by hidden talent
- thresholds of qi make realms, whose names and thresholds are data
- qi has a nature of orientation and polarity, and an art of another nature is harder to learn or weaker to use
- skills need a qi or realm to learn and use, and cost qi to cast
- arts are gained three ways: comprehension on a realm gained, secret manuals, and sect teaching

Issue #155 asks for the model as data under one rule for NPCs and the player. Its PM comment adds per-skill cooldowns: one `ready_at` per person makes every skill cool together, unlike Albion's per-skill cooldowns (found in #151).

Facts that shaped the choices, from `core/` at `59b6aae`:
- Attainment per discipline grows by practice up to its talent limit, and every mend that restores a structure is practice, so attainment mirrors talent (ADR 0028).
- An army's skills come from its faction's data. A strike draws among its order's skills by the seed, and a cast names a skill by index among its army's skills (ADR 0034).
- A manual is a discipline and a place, which a fallen sect scatters to a cell or loses (ADR 0026). Nothing reads one yet.
- ADR 0034 left a resource check for one more reason in `CastRefusal` and one more step in the resolution every strike shares.

Options weighed:

- **What qi is.** A quantity of its own on the person: `qi`, the current amount, and `qi_capacity`, the store. Birth draws a cultivation growth a day and a limit from `Life.cultivation` by the one birth function. Once a day the store grows by its growth up to its limit. Every `recovery_ticks` ticks qi recovers `recovery` per mille of the store, rounded up. The people a world starts with begin cultivated for their founding age, and full.
  - Rejected: qi as a reading of summed attainment. Attainment mirrors talent, and mending is practice, so builders would be masters, and nothing would be spent on a cast.
  - Rejected: growth by practice per hit. Factions that fight more would outgrow the rest, and mends would cultivate; time gives a distribution shaped by age.
- **Realms.** `Life.realms`, ascending by qi, each with a name in Korean and romanised and a `might` per mille. A person's rank is how many of them its store reaches, 0 for none, derived on read. A realm gives a comprehension roll when gained, access to the skills of its rank, and its might on body damage.
  - Rejected: a rank stored on the person, which can disagree with the store, as ADR 0022 rejected for a village's cap.
  - Rejected: no might. A first-rate master would differ from a third-rate fighter only by the skills it knows, which reads wrong in wuxia. Might is data, so a region can set it to 1,000.
- **Arts and known skills.** `Life.arts`: each a name, the discipline its skills practise, a nature, and its skills, each a `Skill` with the rank needed to learn it and whether comprehension can unlock it. `Skill` gains `qi_cost`, 0 for the factions' skills. A person's `known` is the art skills it learned, in order, append-only, each with a ready tick of its own. The hash folds them, and a save holds them.
- **Per-skill cooldowns.** Each known art skill keeps its own ready tick. The army's skills keep the person's shared `ready_at`, as the basic attack's timer.
  - Rejected: a ready tick for every skill, the factions' included. An Orthodox would guard and mend on separate timers, raising every village's action rate and shifting every battle and the sweep. Army skills also change on conscription and rebirth, so ticks kept by army index would need re-keying.
  - Rejected: one shared tick for all, which #151 found wrong.
- **One rule, no per-person branch.** A person's skills are its army's followed by its known skills, and `cast` indexes that list, so an army's indices are unchanged. The automatic strike draws by the seed among its order's skills once `ready_at` has passed, and its known skills that are ready and that its qi pays for. A person who knows no art draws exactly as before. Qi is spent when the use resolves. `CastRefusal` gains `NO_QI`, checked after `ON_COOLDOWN`.
  - Rejected: copying the factions' skills into each person's known skills. That is a second copy of what the army gives, which conscription and rebirth would have to edit.
- **Practice.** A use of an art skill practises its art's discipline. A faction's skill practises the striker's best discipline, as before. This answers ADR 0019's open question of which discipline a strike practises.
- **Nature.** A person's nature is its heart method's, the art it learned first, derived. `Life.learning` gives, per axis, an `ease` per mille, which multiplies learning chances and divides a reading's cost, and a `potency` per mille, which multiplies an art skill's body damage, for each axis on which the art differs from the heart.
  - Rejected: a nature drawn at birth or set by faction. The concept ties it to the heart method, and deriving it adds no state.
- **Comprehension.** On each realm gained, for each art it knows, a person rolls `comprehension` per mille, scaled by its talent limit in the art's discipline over that discipline's mean limit and by the ease, to learn the art's first comprehensible skill it lacks and has the rank for.
- **Manuals.** A manual names an art, not a discipline. A manual lying on a cell is read, each tick, by the nearest living person within `reading_reach` who has the rank for a skill of its art it lacks and the qi: it spends `reading_cost`, divided by the ease, and learns every such skill. The manual stays where it lies.
  - Rejected: a port to read a manual, and a manual carried by its reader. A carried manual needs a rule for dropping it on death and a new place. Walking onto it is how the player reads, as an NPC does.
- **Sect teaching.** Once a day, a member of a sect whose seat is held learns, with `teaching` per mille a year for each rank of its highest-ranked elder, and the ease, the first skill it lacks of an art its sect holds a manual of, within both their ranks.
  - Rejected: a rate set by the master's attainment. Realm is the concept's measure; attainment is practice.
- **The region's data.** Eight realms on rising thresholds, third-rate at 1,000 qi to life-and-death at 36,000, each a tenth mightier than the last. Cultivation of 0 to 2 qi a day up to a limit of 1,000 to 15,000, recovering a hundredth of the store a second. Three arts, one for each sect's manual: Hwasan's sword, orthodox and yang; the Heavenly Demon's saber, demonic and yang; the Black Fang's hidden weapons, unorthodox and yin. Each has a strike at third-rate, taught or read only, and wider forms at first-rate and beyond peak, which comprehension also unlocks.

## Decision

Every person cultivates qi, a store grown daily by a talent drawn at birth and a current amount that recovers and is spent on skills. Realms are thresholds on the store, derived, each with a might on body damage. Arts are data whose skills a person learns into a list of its own, each with its own ready tick, by comprehending on a realm gained, by reading a manual lying on a cell, or by its sect's teaching. A person's skills are its army's followed by those it knows, through one strike and cast rule that spends qi and refuses a cast it cannot pay for. A person's nature is its first art's, and an art of another nature is learned at less ease and strikes with less potency, as data. This supersedes ADR 0026's manual of a discipline, which now names an art; ADR 0034's cast of one of its army's skills only, which now takes the skills a person knows too; and ADR 0019's practice of the best discipline for a strike of an art skill, which practises its art's.

## Consequences

- `make sweep` over seeds 0 to 49 for 100 years of five-tick days keeps every faction peopled on 50 of 50 seeds, with the living at least doubling on all 50. The living at the end, 9,506 over all seeds, stand by rank: 4,154 at none, 2,108 third-rate, 1,948 second-rate, 1,014 first-rate, 282 peak and none higher. 1,555 know an art.
- Over those 50 centuries, 3 people learned from a manual lying on a cell, so a manual that stays where it lies teaches few, and no reading cooldown is added.
- `make bench` on the M1 scenario measured a tick p99 of 0.591 ms on the development Mac, against the 4 ms budget; its people cultivate nothing and know no art, so it times the rule's added checks only.
- The pinned determinism hash is unchanged: a person's qi is folded only in a world that cultivates, its known skills only when it knows one, and a skill's qi cost only when it has one.
- What a save holds changes, so `SAVE_VERSION` rises to 4, and every earlier save is refused.
- Realms past beyond peak are reached by no one under the region's data: no limit reaches 15,000 with time to spare. A soul's widening does not reach cultivation.
- A manual lying on a cell is read again as its reader's realm grows, and by anyone who walks onto it.
- The HUD's 내공 bar is wired to `person_qi` and `person_qi_capacity` in a later pull request, after the controls of #156 and #157.

## Signs the decision was wrong

- Sect members win every fight their faction does, so raids and the sweep turn on who holds a manual rather than on the villages' growth. That would show art skills need to be rarer or costlier.
- Players read their art skills firing on their own as the game playing for them, and ask for art skills to wait for a cast. That would show the automatic strike should draw among the order's skills only.
- A scattered manual becomes a pilgrimage everyone in reach walks to, and arts spread through a region within years. That would show reading needs a cooldown or a cost per manual.
- Realms read as age alone, with every old person peak, because time is the only growth. That would show cultivation needs practice or teaching as well.
- Art skills need behaviour a `Skill` cannot express, such as healing an ally or a buff, and a branch on what kind of art skill it is appears in the strike rule.
