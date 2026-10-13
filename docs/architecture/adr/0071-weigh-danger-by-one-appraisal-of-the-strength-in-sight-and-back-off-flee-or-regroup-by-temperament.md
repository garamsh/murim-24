# ADR 0071: Weigh danger by one appraisal of the strength in sight, and back off, flee or regroup by temperament

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #256. The owner decided on 2026-10-11 that people and animals should have some intelligence about danger:
- **Flee what is stronger.** A person or an animal that sees a predator, a spirit beast or a force clearly stronger than its own side moves away instead of closing. For people, stronger means more numbers, higher realms or better gear; animals use their kind's data.
- **Avoid crowds of foes.** Do not walk into a group that outnumbers your side: wait, go around or gather first.
- **Safety in numbers.** Seek your group: herd mates, fellow villagers or the army.
- **One simple rule for both**, the owner's causal-world principle (#229). The appraisal reads what one sees and compares the two sides' strength, with thresholds by temperament (bold, cautious or cowardly) as pack data. Nothing is scripted per kind.

Facts from `core/` at `1c4bb7f` and the proposals this builds on:
- **Engagement (ADR 0046).** A person no driver steers engages the nearest foe within the rules' sight, 6 cells in the region: at home only a foe inside its home village's claim, on the march any. It stands to fight while a foe is within its reach, and otherwise closes on it. Nothing makes anyone give way to a stronger foe, so a band of ten walks into a battle of a hundred and dies to the last man.
- **The attack intent (ADR 0066).** It is an order's target, and its army's members chase it. Only `World::order` sets a target, which a driver calls; the core never gives an army one of its own. The steered body never engages on its own without one.
- **Steering.** `World::steer` marks a whole army as steered by a driver, and the rules that decide for people on their own decide nothing for its members.
- **Attack slots and tokens (ADR 0068, proposed, merged in #251).** People engaging a foe close on slots around it. Only a holder of one of its tokens strikes it, and the rest circle a waiting ring outside reach.
- **Animals (ADR 0067, proposed, merged in #241).** Animals are records of their own, in herds behind a leader. A grazer that sees a predator of its kind flees toward its herd or cover. Animals give way to a stronger side by this appraisal, with their kind's strength and temperament.
- **Cliffs (ADR 0058, proposed).** A person leaps cliffs within its climb height, and `sees` hides the ground above a cliff from those below who cannot leap it.
- **Strength.**
  - A realm's `might`, 1,000 per mille for none and 1,100 to 1,800 for the eight realms, a tenth more each, multiplies the body damage its people strike with. Peak, the fourth, is 1,400.
  - A realm's `qi` is the capacity that reaches it: 1,000 for third-rate, 10,000 for peak and 36,000 for life and death. Arts' skills are paid from qi.
  - Health is a count, 120 for a newborn in the region, and no realm raises it.
  - Gear (ADR 0050, proposed) will give the weapon's strike and the armour's guard.
- **The spatial hash (ADR 0046, #224), `core/spatial_hash.hpp`.**
  - Positions are marked with their army's faction as a kind bit.
  - `nearest` takes the kinds sought. It first asks the private `holds` whether any block under its span may hold one, then walks rings of cells, passing over cells holding none of them.
  - `visit_within` takes no kinds: it visits every cell of its span, 169 at 6 cells of sight, and every position in them, keeping those within the radius.
  - No public query visits only the cells holding a kind sought.
- **The live scenario (`core/live_scenario.cpp`).** 5,000 people: two battles of two lines of ten armies of 50, 40 armies and 2,000 people; eight villages of 250; and 20 bands of 50 on the roads. Its p99 tick at `make bench` on 2026-10-11 was 3.856 ms against an informational target of 4 ms (#269).
- **Identity and clock (re-read on main `585311ec`).** `core/world.cpp` keeps every recorded person, dead included; births and rebirths append new identities while the capacity bounds only the living. A live person's identity may therefore exceed the capacity. The spatial hash indexes compact living slots; the existing crowd maps those slots to persistent people and back. The world clock, a person's ready tick and a declaration's tick are 64-bit (`core/world.hpp`, `core/person.hpp`, `core/diplomacy.hpp`).
- **Wars (ADR 0038).** A faction's leader declares war and makes peace by its numbers against the other's, and raids by a village's pressed residents take a party to a hostile neighbour. `make sweep` keeps every faction peopled on 48 of 50 seeds, with 54 wars and 27 peaces.

### Options weighed for what is counted

- **The strength in sight on each side, summed.** Chosen.
  - A person's strength is its health, times its realm's might over 1,000, times its realm's weight, times its gear's factor once ADR 0050 lands, 1,000 per mille until then. An animal's is its kind's strength from pack data times its health share.
  - A realm's weight is a field of its realm in pack data, by default doubling with each realm: 1 for none, 2 for third-rate, 16 for peak and 256 for life and death.
  - Its own side is itself and everyone within the rules' sight, by `sees` once ADR 0058 lands, who is not hostile to it and serves in its army or its faction, or for an animal its herd.
  - The threat is everyone in sight who would strike it: `foes` for people, and for an animal the kinds its kind's data names, predators and people for a grazer.
  - The appraisal is the ratio of threat to own strength, per mille.
  - This is Lanchester's square law without the square: damage dealt scales with might and damage taken with health, so their product is how long a side lasts and how hard it hits.
  - The weight counts what might and health leave out of a realm: the qi that pays for its arts' skills, which grows 36-fold across the realms while might grows less than 2-fold, and the arts of higher rank a realm unlocks. Numbers, realms, health and gear all count, as the owner asked.
  - The owner's example, a peak master against three novices with no realm, all at full health:
    - the master's strength is 120 × 1,400 / 1,000 × 16 = 2,688;
    - the novices' is 3 × 120 = 360;
    - the master's ratio is 360 / 2,688 = 134 per mille, so it closes whatever its temperament.
    - Each novice's ratio is 2,688 / 360 = 7,467 per mille, above every flee threshold.
    - A cautious peak master backs off only from 34 novices and flees from 68.
    - A cautious third-rate fighter, 264, closes on 3 novices (1,364 per mille) and backs off from 4 (1,818).
- **Numbers alone.** Rejected: a peak master would flee three novices.
- **Might as the realm's only measure, with no weight.** Rejected: might spans 1,000 to 1,800 and health is 120 for everyone, so realm hardly counts. The peak master's ratio against three novices would be 3 × 120 / 168 = 2,143 per mille: above the cautious back-off threshold, so the master would back off from them, which numbers alone was rejected for.
- **The weight as the realm's qi over 1,000.** Rejected: it makes third-rate weigh the same as no realm, and it ties the appraisal to the cultivation ladder's tuning. A weight of its own is one field to tune when battles read wrong.
- **Realms alone.** Rejected: ten novices would walk into a master's ten.
- **Squared numbers, Lanchester's law in full.** Rejected for now: it makes two against one four times as strong. It tunes battles that the sweep already shows sensitive to small changes, and the plain sum is the first thing to try. The Signs say when to square.

### Options weighed for temperament

- **A temperament per person, drawn at birth, and per animal kind, each naming three thresholds as pack data.** Chosen.
  - The people's table draws bold, cautious or cowardly by a share, by default 20%, 60% and 20%. The draw is counter-based and seeded per person: a hash of the world's seed, the person's index and its birth tick. No seeded stream moves, and two people born on one tick draw independently.
  - A kind of animal names one temperament, a spirit beast bold, a deer cowardly.
  - New person-state storage is initialized to cautious before birth or restoration fills it. Every birth, including founders and explicit scenario recruits through the existing birth rule, replaces that initial value with the seeded draw. Loading restores the saved temperament; it never draws or defaults a living person again. Saves with a version unequal to `SAVE_VERSION` remain refused, including saves from before this rule; no backward-save migration is proposed.

  The thresholds, as the ratio of threat to own strength per mille, at which each outcome begins:

  | Temperament | Back off above | Flee above |
  |---|---|---|
  | Bold | 2,500 | 5,000 |
  | Cautious | 1,500 | 3,000 |
  | Cowardly | 1,000 | 1,800 |

- **One threshold for everyone.** Rejected: the owner named temperament, and a uniform crowd breaks all at once, which reads as a switch rather than a mind.
- **Temperament from realm.** Rejected: courage is not cultivation; a master may be cautious.

### Options weighed for the outcomes

Five outcomes, from the appraisal and whether the person has its group in sight. At each appraisal, choose flee first when above its threshold, otherwise regroup when alone above its threshold, otherwise hold or back off above the back-off threshold, otherwise close. The hysteresis below decides whether a held stance may yield to that choice:

- **Close**, the ratio at or below the back-off threshold: ADR 0046's engagement and ADR 0068's slots, unchanged.
- **Hold**, the ratio above the back-off threshold, for a person inside its home village's claim, or within two cells of a wall or gate of its faction's once walls exist: it does not close. It stands to fight whoever comes within its reach, and it does not take ADR 0068's waiting ring. Defenders hold their ground up to the flee threshold; above it they flee.
- **Back off**, the ratio above the back-off threshold and not holding:
  - it neither closes nor strikes, takes no slot or token, and frees those it held before slot assignment;
  - it steps away from the threat's centroid, a strength-weighted mean of the foes' positions as the appraisal read them, by the open step nearest that line, until the release conditions below hold, unless worsening danger requires flight first;
  - a waiter on ADR 0068's ring that backs off leaves the ring.
- **Flee**, the ratio above the flee threshold, holding or not: it walks toward its refuge with its army's flow field, never toward the threat. The refuge is:
  - its army's lead while the army has one in sight;
  - otherwise its home village's centre;
  - an animal's herd leader, or for a leader its cover, as ADR 0067 names it.

  If a cliff it can leap stands between it and the threat, the leap is the escape (ADR 0058). It strikes nothing while fleeing.
- **Regroup**, alone, with no one of its side within sight, and the ratio above half the back-off threshold: it walks toward its army's lead, or its herd, before it engages, so a straggler joins its band rather than meeting the foe alone.

The owner's "go around" is no outcome of its own. A person backing off takes the open step nearest the line away from the threat, so it moves around a crowd rather than into it, and resumes its way once the ratio falls. Routing around an area known to be dangerous before it comes into sight would need costs on the flow field, and that is left to the Signs.

### Options weighed for hysteresis

- **Immediate flight on worsening danger, with a release ratio and minimum hold time for recovery.** Chosen.
  - At each appraisal, a ratio above the flee threshold changes any non-flee stance to flee, even before 30 ticks or without satisfying a release ratio. Flight takes precedence over holding ground and regrouping. The new stance tick is that appraisal tick; staying in the same stance does not reset it.
  - Close enters the selected non-close outcome immediately. A non-close stance otherwise yields only after at least 30 ticks in that stance and a ratio strictly below three quarters of its entry threshold: the flee threshold for flee, the back-off threshold for hold or back off, and half the back-off threshold for regroup. On release, choose again from the current appraisal and group/ground conditions.
  - A cautious person backing off at 1,600 at tick 0 flees at tick 15 if the ratio becomes 4,000. Its flight releases only at an appraisal at or after tick 45 with ratio below 2,250; 2,000 selects back off again, whereas 1,000 with allies in sight selects close.
  - Without escalation, 1,600 at tick 0 followed by 1,000 at tick 15 remains back off until tick 30. Oscillating between 1,400 and 1,600 remains back off even past 30 ticks; at 1,125 it still holds, and only below 1,125 may it release. Recovery is limited by both conditions, while crossing the flee threshold is not delayed.
- **A timer alone.** Rejected: a ratio hovering at the threshold would still flip every 30 ticks.

### Options weighed for who is exempt

- **The player's armies are not appraised.** Chosen, the PM's ruling on 2026-10-11 in #262.
  - An army a driver steers, by `World::steer`, is not appraised: neither its lead nor its members back off, flee, regroup or hold on their own. The steered body is never overridden, and the members follow the player's orders.
  - An army holding an attack intent is not appraised while the intent holds. Only a driver's order gives one, so it is the player's command. Once the intent ends, by the core or the driver, the army appraises at its next phase.
- **Exempting the steered body alone.** Rejected: the player's own band would back off around it while it charges, against its orders.
- **Appraising an army under the player's intent.** Rejected: an NPC army the player sends at a strong foe would turn back on sight of it, and the order would mean nothing.

### Options weighed for bands and wars

- **An army on a march order, not steered and holding no attack intent, appraises as one, through its lead, and its members follow the lead's stance.** Chosen.
  - The lead counts its army and allies in sight against the foes in sight of any member, read by the same rule. A person in sight of several members counts once.
  - An army whose stance is back off or flee turns back to its home village: a raid fails and the party comes home.
  - An army defending inside its claim holds instead.
  - A member separated from its army appraises alone and regroups.
  - Keep to your group, the owner's third point, falls out of it: bands move as one.
- **Every soldier on its own.** Rejected: a band would shed its cowards one by one into the enemy's slots and still die piecemeal.
- **What it changes in wars.** Fewer battles end with a side wiped out. Raids of a small party against a strong village turn back on seeing it. Wars last longer in deaths per war and shorter in raids pressed home. ADR 0038's diplomacy reads numbers, so more survivors means peace comes later for a side that would have been bled out. The sweep reports wars and peaces in every rule step, and the token table and the thresholds are the levers if it falls below 48 of 50.

### How it fits

- **ADR 0046.** Engagement runs only for a person whose stance is close. Hold keeps the stand-to-fight rule and drops the closing. Back off, flee and regroup replace the engage pull with their own.
- **ADR 0066.** A steered army is never appraised, intent or no intent, and an army holding an intent is not appraised while it holds; the player decides. An intent's target may back off or flee, and the intent chases it until it leaves sight, as now.
- **ADR 0068.** Appraisal runs before slot assignment on the tick. A person backing off or fleeing takes no slot and no token, and frees those it held. People waiting on the ring stop circling and back off when outmatched.
- **ADR 0067.** An animal's strength and temperament are its kind's data, and its side is its herd. Its flight is the same rule, which replaces 0067's flee toward herd or cover for grazers that see a predator. A spirit beast is bold and holds its territory: a beast inside its territory holds rather than backing off.
- **ADR 0058.** `sees` decides who is counted, so a force hidden above a cliff does not frighten anyone below it, and a leap down is a flight a pursuer without the height cannot follow.

### Cost

**State.** Keep persistent appraisal state with the existing person record, indexed by its lifetime identity, not in arrays bounded by the living capacity:
- its temperament, 1 byte, drawn at birth;
- its stance, 1 byte;
- the tick the stance began, 8 bytes, on the same 64-bit clock as the world.

That is 10 bytes of field payload per recorded person, including the dead: 5 KB for 500 records and 50 KB for 5,000 records, not bounds at those living counts. An army keeps its stance and its 64-bit stance tick in its existing order, 9 bytes of field payload. These counts exclude record padding; step 1 reports the actual change in record size. Birth and restoration use the existing record-allocation boundaries; appraisal adds no allocation or new identity mapping. Death, a changed roster and crowd compaction do not transfer a person's saved state to another body, and rebirth gives the new body its own birth draw.

**Saves and the hash.** The stance and the tick it began carry the hysteresis, so they are world state, as ADR 0068 keeps its slots:
- a person's temperament, stance and full 64-bit stance tick, and an army's stance and full stance tick, are saved, and `SAVE_VERSION` rises;
- the hash folds a person's temperament only when it is not cautious, and a stance with its tick only when it is not close, so a world in which nobody has left close hashes as before;
- a world loaded from a matching-version save restores temperament, stance and stance tick unchanged, including a bold living NPC; unequal versions remain refused, with no migration.

**The query.** The hash gains one public query: visit the positions of some kinds within a radius. It asks `holds` over its span first, and passes over every cell holding none of the kinds, as `nearest` does. Nothing else in the hash changes, and it allocates nothing.

**Merged sight.** An army appraising through its lead reads the foes and allies in sight of any member:
- each member's sight is visited in turn with the new query, whose results are compact live crowd slots; read persistent state through the existing crowd's slot-to-person map, and locate a member through its person-to-slot map;
- a person found by several members must count once, so the world keeps one 4-byte stamp per compact live slot, sized to the living capacity and allocated with the world, never indexed by a persistent person identity;
- each appraisal takes the next serial and stamps each slot it counts, skipping one already stamped with it; the crowd is fixed for that appraisal, and every later appraisal uses a new serial, so a changed crowd cannot inherit a previous occupant's membership;
- armies appraise one at a time, so one stamp array serves every army, and no army keeps scratch space of its own;
- when the serial wraps, the array is cleared once. Stamps are scratch space, neither saved nor hashed.

At a living capacity of 6,250 the stamps are 25 KB. With P recorded people, C living capacity and A recorded armies, the added field payload and stamp storage are 10P + 4C + 9A bytes, before record padding and the serial. Persistent memory grows with recorded history; the living capacity bounds only the scratch array.

**Work.** Each person and each army not exempt appraises every 15 ticks, at a phase of its index modulo 15, so a fifteenth appraise on any tick.
- An appraisal first asks, through the new query, whether any block in sight may hold a threat's kinds; where none may, it ends there.
- One that may meet a threat walks the 169 cells of each sight once for the kinds of either side, and reads the positions of those kinds.
- In the live scenario of 5,000:
  - The 2,000 villagers appraise on their own, about 133 a tick. Their villages lie beyond any hostile faction's sight, so each ends at its first block test.
  - The 20 road bands appraise through their leads, about 1.3 a tick. Each asks the block test for its 50 members' sights, and none sees a foe, so about 70 block tests a tick.
  - The 40 battle armies appraise through their leads, about 2.7 a tick. Each walks its 50 members' sights: 8,450 cell tests. Every member stands within 6 cells of the foe's line, so each reads the positions in its disc of 113 cells over the two lines, about 100, some 5,000 an appraisal.
  - That comes to about 23,000 cell tests and 13,000 positions a tick. At a few nanoseconds each that is about 0.1 ms a tick, 2–3% of the 4 ms target, with no allocation. Step 1 measures it against the bench.
- At 500, a tenth of that.

**Determinism.** The appraisal reads positions as the tick began, uses integer arithmetic, and runs in person index order, then army index order. Temperament is drawn by a counter-based draw that no other draw shares.

## Decision

Every person outside the player's armies, and every animal, appraises the danger it sees with one rule.
- **The appraisal.** It is the ratio of the strength of those in sight who would strike it to its own side's strength, per mille. A person's strength is its health, times its realm's might over 1,000, times its realm's weight, by default doubling from 1 with no realm to 256, times its gear's factor; an animal's is its kind's. Its side is those in sight of its army or faction not hostile to it, or its herd. Sight is the rules' sight, by ADR 0058's `sees` once that lands.
- **Temperament.** A person's temperament is drawn at birth, bold, cautious or cowardly by shares in pack data, and an animal's comes from its kind. New person-state storage starts cautious; every birth, including founders and recruits, replaces it with the draw, and restoration replaces it with the saved temperament and refuses unequal save versions. It names the ratios above which the person backs off and flees: by default 2,500 and 5,000 for bold, 1,500 and 3,000 for cautious, and 1,000 and 1,800 for cowardly.
- **The outcomes.**
  - At or below the back-off ratio it closes as ADR 0046 and ADR 0068 say.
  - Above it, a person inside its home claim or by its faction's wall holds; anyone else backs off from the threat's centroid. Back off and flee free all slots and tokens before assignment, take none, and strike nothing, even with a foe in reach.
  - Above the flee ratio it flees to its refuge: its army's lead, its home village, or its herd.
  - Alone with any threat above half the back-off ratio, it regroups toward its group first.
- **Transition precedence and hysteresis.** At each appraisal, choose flee above its threshold first, then regroup when alone above half the back-off threshold, then hold or back off above the back-off threshold, then close. Flight overrides any non-flee stance immediately, regardless of release ratio or elapsed time. Close enters the chosen outcome immediately; other transitions require both 30 ticks in the current stance and a ratio strictly below three quarters of its entry threshold. Each change records a new stance tick; unchanged stances keep theirs. A cautious back-off at 1,600 therefore becomes flight at 4,000 at the next appraisal, while declining or oscillating risk obeys the recovery conditions above.
- **Who appraises, and how often.** An army on a march order appraises through its lead, counting what any member sees, and its members follow its stance: a band that backs off or flees turns home. Each person appraises every 15 ticks at its own phase.
- **What is exempt.** An army a driver steers is never appraised, its lead and members alike, and an army holding an attack intent is not appraised while the intent holds. Nothing overrides the player.
- **State.** Temperament, stance and the full 64-bit tick a stance began stay on the persistent person record, including after a crowd is compacted, and are saved and hashed when they differ from cautious and close. Rebirth creates a new record with its own draw. Only the scratch stamps key compact live slots and are bounded by the living capacity; recorded history is not bounded by it.

This supersedes ADR 0046's engagement of the nearest foe in sight regardless of the odds. It amends ADR 0068 so that a person backing off or fleeing takes no slot or token, and ADR 0067's flight of a grazer from a predator, which becomes this rule.

**Implementation steps.** Each step is its own pull request, with `make ci`, GCC, `make sweep` keeping 48 of 50 seeds or better with the war and peace counts reported, and `make bench` passing on AC power:
1. **Temperament and the appraisal, no behaviour.**
   - Temperament drawn at birth, saved and hashed when not cautious; persistent fields on the existing person record, army fields on its order, and stance ticks on the 64-bit world clock. Report field payload separately from the measured change in record size, and persistent memory by recorded population, dead included.
   - The realms' weights in the realm table.
   - The hash's query of some kinds within a radius, and the world's stamps.
   - The pure appraisal function, reading the hash and returning the ratio, for a person and for an army's merged sight.
   - Tests:
     - the ratio for set layouts: one against one equal; three against one; a peak master against three novices at 134 per mille, with a control: the same layout with every weight 1 reads 2,143;
     - an army's merged sight counts a foe seen by two members once; control, two foes, each seen by one member, count twice;
     - repeat that layout after deaths and births or rebirths give a living person an identity beyond the living capacity, and after roster changes remap compact crowd slots; assert those identities, mappings and living count in the setup; control, the same visible strengths with identities below the capacity give the same ratio;
     - the kinds query visits only positions of the kinds sought; control, the same layout through `visit_within` visits all;
     - a hidden foe not counted, once ADR 0058 lands;
     - the draw's shares over 10,000 births within a tolerance;
     - a saved bold living NPC restores as bold, with the same hash and appraisal thresholds; control, an otherwise identical cautious NPC at ratio 2,000 reads above its back-off threshold while the restored bold NPC reads below its own;
     - an unequal-version save is refused; control, the matching-version save restores, with no migration.
   - Sweep identical.
2. **Individuals: back off, flee, regroup and hold.**
   - Unless testing isolation, keep an ally in sight and set the two sides' strengths to the named ratio; assert that setup before checking the outcome.
   - Tests:
     - a cautious villager outside its claim backs off at ratio 2,000 and closes at 1,000; control, the same layout with the villager bold closes at 2,000;
     - a cowardly one flees to its village;
     - one inside its claim holds at ratio 2,000;
     - a straggler regroups to its army before engaging;
     - a backing-off token holder with a foe still in reach releases all slots and tokens before assignment and deals no damage, taking none on later ticks; control, the same layout with a closing token holder strikes; repeat for flee;
     - worsening risk: cautious back off at ratio 1,600 at tick 0 becomes flee at 4,000 at tick 15, with stance tick 15, repeat from hold on home ground and from regroup while alone, each entering flee at 4,000; control, 1,600 then 2,000 with allies in sight stays back off (hold on home ground), with its original tick;
     - declining risk: back off at 1,600 at tick 0 and 1,000 at tick 15 remains back off until tick 30, then closes with allies in sight; control, 1,125 at tick 30 does not release;
     - oscillating risk: 1,400 and 1,600 at successive appraisals keep back off past 30 ticks; control, 1,000 after that releases it;
     - after the escalation at tick 15, flight at 2,000 at tick 30 persists until tick 45, then becomes back off; controls, 2,250 at tick 45 retains flight and 1,000 with allies in sight releases to close;
     - a steered army facing three foes keeps closing, its lead and members alike; control, the same army not steered backs off;
     - an army ordered with an attack intent at a stronger foe keeps chasing it; control, the same army after `end_intent` backs off at its next phase;
     - a world containing a bold living NPC saved mid-retreat restores its temperament, stance and stance tick and hashes the same, 60 ticks on, as the world continued; repeat after a death/birth or rebirth history puts its identity beyond the living capacity and roster compaction changes its crowd slot; controls, the same sequence without compaction retains its own state, and loading with stances reset to close diverges;
     - a cautious NPC at phase 1 begins back off at T = UINT32_MAX − 14, escalates to flee at T + 15, and is saved there with its full stance tick above UINT32_MAX; at ratio 2,000 it still flees at T + 30 and releases to back off at T + 45, so a truncated tick cannot release it early; continuation after load matches the unsaved world; nearby control, start at UINT32_MAX − 59 and repeat the same ratios and hold ages before the boundary; assert the fixture's world clock and stance tick before checking the outcomes.
3. **Bands.**
   - Army stances through the lead, raids turning home, defenders holding.
   - Tests:
     - a raid of five against a village of thirty turns home on sight of it; control, a raid of thirty presses on;
     - a band outnumbered in a war backs off together, none of its members left behind.
   - The sweep's war and peace counts reported against the step before.
4. **Animals and cliffs**, once ADRs 0067 and 0058 land: a grazer flees a predator by the same rule, and a leap down is taken as the escape.
5. **The view.** Nothing new to decide; captures of a band backing off and a village holding.

## Consequences

- Battles end with survivors, raids fail on sight of a strong village, and a lone traveller avoids a hostile band, so the world loses fewer people to fights the losing side could see coming.
- The player can rout a band by outnumbering it, and is never made to run.
- Wars cost fewer lives and may last longer before ADR 0038's diplomacy makes peace; the sweep's war and peace counts will move.
- Animals and people share the rule, so a tiger in a herd of deer reads the same as a master among novices.
- Each recorded person adds 10 bytes of field payload and each recorded army 9, plus measured record padding; scratch stamps cost 4 bytes per live-capacity slot. Dead history grows the persistent storage without raising the living bound. Appraisal adds no allocation; its estimated tick cost is about 0.1 ms at 5,000, to be measured in step 1.

## Signs the decision was wrong

- Battles never resolve: both sides back off at the edge of sight and stand there. The thresholds need a bias toward closing for a side on its own ground, or the square law.
- The sweep falls below 48 of 50 because raids never press home and pressed villages starve, at any threshold.
- Players read a band's retreat as a bug: they see it turn without seeing what it saw.
- A band at the edge of sight steps back and forth across it, which hysteresis was meant to stop.
- People back off from a crowd only to walk into it again by another way, because nothing remembers where danger was: going around needs costs on the flow field.
- Step 1's measurement puts the appraisal above 0.2 ms a tick at 5,000: the merged sight then needs sampling, such as the members at the army's edge only.
