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

Facts from `core/` at `c4e24a7` and the proposals this builds on:
- **Engagement (ADR 0046).** A person no driver steers engages the nearest foe within the rules' sight, 6 cells in the region: at home only a foe inside its home village's claim, on the march any. It stands to fight while a foe is within its reach, and otherwise closes on it. Nothing makes anyone give way to a stronger foe, so a band of ten walks into a battle of a hundred and dies to the last man.
- **The attack intent (ADR 0066).** It is an order's target that a driver sets, and its army's members chase it. The steered body never engages on its own without one.
- **Attack slots and tokens (ADR 0068, #251, open).** People engaging a foe close on slots around it. Only a holder of one of its tokens strikes it, and the rest circle a waiting ring outside reach.
- **Animals (ADR 0067, #241, open).** Animals are records of their own, in herds behind a leader. A grazer that sees a predator of its kind flees toward its herd or cover. Animals give way to a stronger side by this appraisal, with their kind's strength and temperament.
- **Cliffs (ADR 0058, proposed).** A person leaps cliffs within its climb height, and `sees` hides the ground above a cliff from those below who cannot leap it.
- **Strength.**
  - A realm's `might`, 1,000 per mille for none and up to 1,800 for the eighth, multiplies the body damage its people strike with.
  - Health is a count, 120 for a newborn in the region.
  - Gear (ADR 0050, proposed) will give the weapon's strike and the armour's guard.
- **The spatial hash (ADR 0046, #224).** Positions are marked with their army's faction as a kind bit. `holds` answers whether any block under a span may hold a kind, and `visit_within` and `nearest` read only cells holding a kind sought.
- **Wars (ADR 0038).** A faction's leader declares war and makes peace by its numbers against the other's, and raids by a village's pressed residents take a party to a hostile neighbour. `make sweep` keeps every faction peopled on 48 of 50 seeds, with 54 wars and 27 peaces.

### Options weighed for what is counted

- **The strength in sight on each side, summed.** Chosen.
  - A person's strength is its health times its realm's might over 1,000, times its gear's factor once ADR 0050 lands, 1,000 per mille until then. An animal's is its kind's strength from pack data times its health share.
  - Its own side is itself and everyone within the rules' sight, by `sees` once ADR 0058 lands, who is not hostile to it and serves in its army or its faction, or for an animal its herd.
  - The threat is everyone in sight who would strike it: `foes` for people, and for an animal the kinds its kind's data names, predators and people for a grazer.
  - The appraisal is the ratio of threat to own strength, per mille.
  - This is Lanchester's square law without the square: damage dealt scales with might and damage taken with health, so their product is how long a side lasts and how hard it hits. Numbers, realms and health all count, as the owner asked.
- **Numbers alone.** Rejected: a peak master would flee three novices.
- **Realms alone.** Rejected: ten novices would walk into a master's ten.
- **Squared numbers, Lanchester's law in full.** Rejected for now: it makes two against one four times as strong. It tunes battles that the sweep already shows sensitive to small changes, and the plain sum is the first thing to try. The Signs say when to square.

### Options weighed for temperament

- **A temperament per person, drawn at birth, and per animal kind, each naming three thresholds as pack data.** Chosen.
  - The people's table draws bold, cautious or cowardly by a share, by default 20%, 60% and 20%, by a counter-based draw from the birth, so no seeded stream moves.
  - A kind of animal names one temperament, a spirit beast bold, a deer cowardly.
  - People already alive when a world loads under this rule, and worlds from before it, read cautious.

  The thresholds, as the ratio of threat to own strength per mille, at which each outcome begins:

  | Temperament | Back off above | Flee above |
  |---|---|---|
  | Bold | 2,500 | 5,000 |
  | Cautious | 1,500 | 3,000 |
  | Cowardly | 1,000 | 1,800 |

- **One threshold for everyone.** Rejected: the owner named temperament, and a uniform crowd breaks all at once, which reads as a switch rather than a mind.
- **Temperament from realm.** Rejected: courage is not cultivation; a master may be cautious.

### Options weighed for the outcomes

Five outcomes, from the appraisal and whether the person has its group in sight:

- **Close**, the ratio at or below the back-off threshold: ADR 0046's engagement and ADR 0068's slots, unchanged.
- **Hold**, the ratio above the back-off threshold, for a person inside its home village's claim, or within two cells of a wall or gate of its faction's once walls exist: it does not close. It stands to fight whoever comes within its reach, and it does not take ADR 0068's waiting ring. Defenders hold their ground; they do not run from it.
- **Back off**, the ratio above the back-off threshold and not holding:
  - it neither closes nor takes a slot;
  - it steps away from the threat's centroid, a strength-weighted mean of the foes' positions as the appraisal read them, by the open step nearest that line, until the ratio falls under the threshold;
  - it still strikes a foe within its reach with a token;
  - a waiter on ADR 0068's ring that backs off leaves the ring.
- **Flee**, the ratio above the flee threshold, holding or not: it walks toward its refuge with its army's flow field, never toward the threat. The refuge is:
  - its army's lead while the army has one in sight;
  - otherwise its home village's centre;
  - an animal's herd leader, or for a leader its cover, as ADR 0067 names it.

  If a cliff it can leap stands between it and the threat, the leap is the escape (ADR 0058). It strikes nothing while fleeing.
- **Regroup**, alone, with no one of its side within sight, and the ratio above half the back-off threshold: it walks toward its army's lead, or its herd, before it engages, so a straggler joins its band rather than meeting the foe alone.

### Options weighed for hysteresis

- **A stance kept until the ratio falls under three quarters of the threshold that set it, and for at least 30 ticks.** Chosen.
  - A person backing off at 1,600 against a threshold of 1,500 closes again only under 1,125.
  - A stance changes at most once a second.
  - Combined, they keep a person on the boundary from dithering, and a fleeing crowd does not turn back the moment it has gained a step.
- **A timer alone.** Rejected: a ratio hovering at the threshold would still flip every 30 ticks.

### Options weighed for bands and wars

- **An army on a march order appraises as one, through its lead, and its members follow the lead's stance.** Chosen.
  - The lead counts its army and allies in sight against the foes in sight of any member, read by the same rule.
  - An army whose stance is back off or flee turns back to its home village: a raid fails and the party comes home.
  - An army defending inside its claim holds instead.
  - A member separated from its army appraises alone and regroups.
  - Keep to your group, the owner's third point, falls out of it: bands move as one.
- **Every soldier on its own.** Rejected: a band would shed its cowards one by one into the enemy's slots and still die piecemeal.
- **What it changes in wars.** Fewer battles end with a side wiped out. Raids of a small party against a strong village turn back on seeing it. Wars last longer in deaths per war and shorter in raids pressed home. ADR 0038's diplomacy reads numbers, so more survivors means peace comes later for a side that would have been bled out. The sweep reports wars and peaces in every rule step, and the token table and the thresholds are the levers if it falls below 48 of 50.

### How it fits

- **ADR 0046.** Engagement runs only for a person whose stance is close. Hold keeps the stand-to-fight rule and drops the closing. Back off, flee and regroup replace the engage pull with their own.
- **ADR 0066.** The steered body is never appraised and never overridden, intent or no intent; the player decides. An intent's target may back off or flee, and the intent chases it until it leaves sight, as now.
- **ADR 0068.** Appraisal runs before slot assignment on the tick. A person backing off or fleeing takes no slot and no token, and frees those it held. People waiting on the ring stop circling and back off when outmatched.
- **ADR 0067.** An animal's strength and temperament are its kind's data, and its side is its herd. Its flight is the same rule, which replaces 0067's flee toward herd or cover for grazers that see a predator. A spirit beast is bold and holds its territory: a beast inside its territory holds rather than backing off.
- **ADR 0058.** `sees` decides who is counted, so a force hidden above a cliff does not frighten anyone below it, and a leap down is a flight a pursuer without the height cannot follow.

### Cost

**State.** A person keeps, in structure-of-arrays form sized to the capacity, never resized on a tick:
- its temperament, 1 byte, drawn at birth and saved;
- its stance, 1 byte;
- the tick the stance began, 4 bytes.

That is 6 bytes a person: 3 KB at 500 and 30 KB at 5,000. An army keeps its stance in its order, 5 bytes.

**Work.** Each person not steered appraises every 15 ticks, at a phase of its index modulo 15, so a fifteenth appraise on any tick.
- An appraisal first asks the hash's `holds` whether any block in sight may hold a threat's kinds.
- An appraisal that may meet a threat sums strengths through `visit_within` over the sight radius, reading only the cells holding a kind of either side. That is 169 cells at 6 cells of sight.
- An army's members are skipped: the lead appraises for them.
- In the live scenario of 5,000:
  - The 2,000 villagers appraise on their own, about 133 a tick. Their villages lie beyond any hostile faction's sight, so `holds` ends each of those appraisals.
  - The 68 armies, the battles' lines and the road bands, appraise through their leads, about 5 a tick. In battle each reads up to 169 cells holding about one person each.
  - That comes to under 2,000 positions read a tick, well below 0.05 ms.
- At 500, a tenth of that.

Nothing allocates: the sums are integers on the stack.

**Determinism.** The appraisal reads positions as the tick began, uses integer arithmetic, and runs in person index order. Temperament is drawn by a counter-based draw that no other draw shares.

## Decision

Every person no driver steers, and every animal, appraises the danger it sees with one rule.
- **The appraisal.** It is the ratio of the strength of those in sight who would strike it to its own side's strength, per mille. A person's strength is its health times its realm's might over 1,000, times its gear's factor; an animal's is its kind's. Its side is those in sight of its army or faction not hostile to it, or its herd. Sight is the rules' sight, by ADR 0058's `sees` once that lands.
- **Temperament.** A person's temperament is drawn at birth, bold, cautious or cowardly by shares in pack data, and an animal's comes from its kind. It names the ratios above which the person backs off and flees: by default 2,500 and 5,000 for bold, 1,500 and 3,000 for cautious, and 1,000 and 1,800 for cowardly.
- **The outcomes.**
  - At or below the back-off ratio it closes as ADR 0046 and ADR 0068 say.
  - Above it, a person inside its home claim or by its faction's wall holds; anyone else backs off from the threat's centroid.
  - Above the flee ratio it flees to its refuge: its army's lead, its home village, or its herd.
  - Alone with any threat above half the back-off ratio, it regroups toward its group first.
- **Hysteresis.** A stance holds until the ratio falls under three quarters of the threshold that set it, and for at least 30 ticks.
- **Who appraises, and how often.** An army on a march order appraises through its lead, and its members follow its stance: a band that backs off or flees turns home. Each person appraises every 15 ticks at its own phase.
- **What is exempt.** The steered body is never appraised. Nothing overrides the player.

This supersedes ADR 0046's engagement of the nearest foe in sight regardless of the odds. It amends ADR 0068 so that a person backing off or fleeing takes no slot or token, and ADR 0067's flight of a grazer from a predator, which becomes this rule.

**Implementation steps.** Each step is its own pull request, with `make ci`, GCC, `make sweep` keeping 48 of 50 seeds or better with the war and peace counts reported, and `make bench` passing on AC power:
1. **Temperament and the appraisal, no behaviour.**
   - Temperament drawn at birth, saved and hashed when not cautious.
   - The pure appraisal function, reading the hash and returning the ratio.
   - Tests:
     - the ratio for set layouts: one against one equal; three against one; a peak master against three novices;
     - a hidden foe not counted, once ADR 0058 lands;
     - the draw's shares over 10,000 births within a tolerance;
     - the save round trip.
   - Sweep identical.
2. **Individuals: back off, flee, regroup and hold.**
   - Tests:
     - a cautious villager outside its claim backs off from three foes and closes on one; control, the same villager bold closes on three;
     - a cowardly one flees to its village;
     - one inside its claim holds against three;
     - a straggler regroups to its army before engaging;
     - hysteresis: a ratio set to oscillate across the threshold keeps the stance for 30 ticks and to three quarters of it;
     - the steered body never changes stance, whatever it faces.
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
- Each person carries 6 more bytes, and the tick gains well under a tenth of a millisecond at 5,000.

## Signs the decision was wrong

- Battles never resolve: both sides back off at the edge of sight and stand there. The thresholds need a bias toward closing for a side on its own ground, or the square law.
- The sweep falls below 48 of 50 because raids never press home and pressed villages starve, at any threshold.
- Players read a band's retreat as a bug: they see it turn without seeing what it saw.
- A band at the edge of sight steps back and forth across it, which hysteresis was meant to stop.
