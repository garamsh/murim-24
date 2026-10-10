# ADR 0058: Leap up and down cliffs by realm and stamina, and veil a cliff's top from those who cannot leap it

> Status: proposed
> Date: 2026-10-11

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-09, in issue #214, that terrain such as cliffs should be something to interact with:
- A person of a high realm climbs a cliff easily, and a low one less.
- A high cliff, not a hill or a low step, hides what is on top of it: looking up from below, one cannot see the ground above. Low steps stay walkable and visible.
- A cliff's height shows by its size, as the 2.5D mockup draws it.

The owner decided on 2026-10-10, in #223's review:
- Climbing spends a stamina of its own, not qi.
- No strikes or casts while climbing, and a hit takes stamina.
- Footwork arts' effect is climbing. Whether footwork also gives speed stays open, as ADR 0055 leaves it.

The owner decided on 2026-10-10, in #214's comments, superseding the hand climb and the fall that this ADR first proposed:
- **Every climb is a leap.** A cliff the person can climb is crossed in one leap, with a jump motion: no hand climb, no climbing state, and no fall from stamina running out.
- **Going down is a leap too,** with the same motion. Only a drop the person can descend may be jumped down. A deeper drop is refused, as a destination and as a path edge, so nobody leaps to their death.
- **A cliff the viewer can climb shows its top.** The veil stays only on a cliff higher than the viewer can climb. From above, the ground below is visible.
- **Clicking a cell on top of a climbable cliff moves there.** The path walks to the foot and leaps up. A cell on a cliff too high is not a destination.
- **A leap's duration is proportional to the height,** at a natural pace, the same for every realm, up and down alike.

The PM decided on 2026-10-10, delegated by the owner, in #214's comments:
- **The descent limit is twice the climb height.** Dropping is easier than leaping up.
- **A jump down costs no stamina.** The descent limit alone governs it.
- **A leap takes 6 ticks plus 6 a level,** at 30 ticks a second: 0.2 seconds of take-off and landing, and 0.2 seconds a level, so a 4-level cliff takes a second. These are the core's constants. If the capture looks wrong in review, the art pass may retune them with a stated reason.

The PM's reading in #214, kept unless the owner says otherwise:
- What a person can climb is still set by realm and footwork, and paid in climbing's own stamina, by the levels a leap rises.
- A person short of the stamina cannot leap. The leap is refused, and never fails in the air.
- No strikes or casts in the air, and a hit during a leap costs stamina.
- NPCs follow the same rule: path edges open to the classes able to leap them, and only when they have the stamina.

The PM's earlier direction on #214, which stands:
- **Height by size.** Keep it, with a compression above a few levels, so that a cliff of 5 to 10 levels, 80 to 160 pixels, does not cover half the screen.
- **Height alone cannot hide.** The camera looks down from above, so a cliff facing it shows its top. Unseen ground above a high cliff is drawn veiled: the terrain's outline only, with no people, items or structures. The veil comes from the core's sight rule, so NPCs share it.

Facts from `main` at `14360c4`:
- **Elevation.** ADR 0047 paints an elevation level from 0 to 7 per 8 × 8 cells. Its build tool makes the upper rim cell of a drop a cliff kind with blocked footing, and joins levels by slope kinds. Levels 6 and 7 are high mountains and peaks with blocked footing, crossed only at passes. ADR 0047 says the core never reads elevation.
  - The core has no elevation until ADR 0047's map lands.
  - The view derives levels for itself in the meantime. ADR 0059, accepted in #226, draws a cell 16 pixels wide and 12 deep and raises ground on level n by 16 × n pixels, at every height. Its derived relief puts mountains at levels 2, 4 and 5, and it draws a cliff only where the core blocks movement.
- **Elevation in the core.** The open PR #222's ADR 0057 proposes a level per cell in the core, beside ADR 0047's footing, for terrain that changes. It also proposes one line walk of cells, shared by strikes and by this issue's sight.
- **Sight.** The rules' sight is 6 cells in the region. A person no driver steers engages a foe within it (ADR 0046). ADR 0054's witnesses are those within it. #168 will fill in the player's map from what it sees.
- **Lightness arts** are ADR 0055's footwork family, merged in #218. That ADR leaves what footwork does beyond its skills as an open question. The owner's decision above answers it for climbing alone.
- **Moving the steered body.** The player's body moves by the `order` port (ADR 0011). Clicking a cell to walk there is the input the owner's click-to-climb extends.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000.

Options weighed:

- **Levels in the core.** The core holds a level, 0 to 15, per cell: a nibble beside ADR 0047's footing nibble, a byte a cell and 16 MiB for the map. The same decision is in #222's ADR 0057, and whichever merges first records it. Until ADR 0047's map lands every cell is at level 0, so every rule below is dormant and the 128-cell region is unchanged.
- **Cliff classes.** A step between two cells is classed by the drop between their levels:

  | Drop | Class | Movement | Sight from below |
  |---|---|---|---|
  | 0 | flat | as today | open |
  | 1 | step | walked, at the travel cost of rough ground | open |
  | 2 to 15 | cliff | leapt, below | hidden from 3, from those who cannot leap it |
  | any, on a level 6 or 7 kind | range | blocked, except at passes | hidden |

  - The thresholds are pack data: the step's drop, the highest cliff, 15 in the base pack, and the hiding drop, 3 in the base pack.
  - A drop of 1 with no slope is a step the owner wants walkable, so this changes ADR 0047's rule that every drop makes a blocked cliff. A drop of 2 or more stays a cliff kind, blocked to walking and open to leaping.
- **The great ranges stay impassable,** except at passes. That is already decided: `concept.md` §World and scale [agreed] has regions join only through its gates, and ADR 0047's gate table and validation rest on it. Leapable ranges would let any army bypass Tong Pass.
- **Stamina.** Stamina is a new resource on each person: a current amount and a maximum, 2 bytes each.
  - The maximum is 100 plus 25 for each realm rank, so 100 to 300 in the base pack.
  - Leaping up spends it, and a hit during a leap takes 10. It recovers 5 a second, every 30 ticks, while the person stands or walks on flat ground.
  - Rejected, and decided so by the owner: climbing paid in qi. Qi pays for skills (ADR 0035), so a climber would arrive unable to fight, and a person of no art, with little qi, could not climb at all.
- **The climb height and the descent limit.**
  - **The cost.** Each level leapt up costs 40 stamina, scaled down by the footwork art's stage the person knows best: 1,000 per mille at none, down to 500 at the highest stage. That is pack data on the stage, and the core reads the stage's effect, not the family's name (ADR 0049).
  - **The climb height** is the most levels a person can leap up on full stamina: its maximum over its cost a level, rounded down. With no footwork, rank 0 leaps 2 levels, ranks 1 and 2 leap 3, rank 3 leaps 4, ranks 4 and 5 leap 5, ranks 6 and 7 leap 6, and rank 8 leaps 7. The highest stage of footwork doubles each: rank 0 leaps 5, rank 8 15.
  - **The descent limit** is twice the climb height: rank 0 jumps down 4 levels, rank 4 jumps down 10.
  - **On the base map.** ADR 0047 paints levels 0 to 7, and 6 and 7 are ranges, so the cliffs there are drops of 2 to 5. A rank 0 person with no footwork leaps up only the 2-level ones and jumps down any but a 5-level one. Rank 4 leaps up every cliff the base map has. Drops of 6 to 15 come only from ADR 0057's terrain delta, raised earth and spoil, if #222 lands.
- **The leap.** A person moving onto a cliff edge leaps instead of stepping, up or down, when it may:
  - **Up** only when the drop is within its climb height and its current stamina pays for the whole drop. The stamina is spent at take-off.
  - **Down** only when the drop is within its descent limit. It costs nothing and harms no one.
  - **Otherwise it is refused,** and the person stays at the edge, as a step into a wall is. A leap never fails once taken: there is no fall.
  - **Its duration** is 6 ticks plus 6 a level, the same for every realm and in both directions: a 2-level leap takes 18 ticks, and a 5-level one 36.
  - **In the air.** A leap in flight holds 3 bytes: the direction from the cell it left, with a bit for down; the levels; and its tick, at most 96. Its position stays the cell it left until it lands on the cell beyond the edge, so no cell index is stored. It is not the climbing state of the hand climb this ADR first proposed: nothing can stop it or make it fail.
  - **While leaping** a person neither strikes, casts nor engages. It is struck as anyone is, and each hit takes 10 stamina besides its damage. It lands as planned.
  - Rejected: a leap's speed by realm. The owner decided it is the same for every realm.
- **NPCs, and paths.**
  - Each person has a climb class: its climb height rounded down to one of four floors, 0, 2, 5 and 10. Every member of a class can leap up its floor and jump down twice it. An army's class is its weakest member's, so a band never leaves one of its own at a wall.
    - On the base pack's numbers, ranks 0 to 3 are class 2, ranks 4 to 8 class 5, and rank 4 and up with the highest stage of footwork class 10.
    - Rounding down routes some people round a cliff they could leap, such as a rank 3 person round a 4-level one. That errs toward a path that never fails.
  - ADR 0043's coarse graph gains leap edges across cliffs, one each way, each with its drop and a cost of the leap's ticks. An edge is open only to the classes whose floor covers it:

    | Drop | Up, open to classes | Down, open to classes |
    |---|---|---|
    | 2 | 2, 5 and 10 | 2, 5 and 10 |
    | 3 to 4 | 5 and 10 | 2, 5 and 10 |
    | 5 | 5 and 10 | 5 and 10 |
    | 6 to 10 | 10 | 5 and 10 |
    | 11 to 15 | none: no NPC path leaps up it, and only a steered body does | 10 |

  - **Starting a leap up.** A person no driver steers starts one only when its current stamina pays for the whole drop. Short of that, one following a path waits at the foot, recovering, and a chaser goes round.
  - Area fields are built per class only in areas holding a cliff, at most 4 fields where there was one, within ADR 0043's per-tick limit.
  - A person no driver steers closes on a foe across a cliff only when its class opens the edge and its stamina pays for it. A chase uses the same step, so a chaser of a high realm leaps after its prey and a low one goes round.
- **The steered body.**
  - **Walking into an edge** leaps by the same rule, or is refused.
  - **Clicking a cell** on top of a cliff the body can leap up, or below one it can jump down, is a destination. The path walks to the foot or the edge and leaps. A body short of the stamina waits there, recovering, as an NPC does.
  - **A cell beyond reach is not a destination:** on a cliff above the body's climb height, or below a drop past its descent limit. The click is refused with its reason as data, and the view shows it.
  - The path is the body's own: it uses the body's climb height and descent limit, not its class's floor.
- **Sight blocked by height.** `sees(a, b)` is the one rule: b lies within the sight reach of a, and no cliff hides b from a.
  - **What hides.** A cliff hides b when b's level is at least the hiding drop above a's, more than a's climb height above a's level, and the line walk from a's cell to b's crosses a cell that high or more. So a cliff a can leap up hides nothing from a.
  - **Its climb height, not its stamina.** `sees` reads the climb height on full stamina, so what a person sees does not flicker as it spends and recovers stamina.
  - **The walk.** It is the line walk #222's ADR 0057 proposes, or this ADR's own if it lands first. When the levels differ by less than the hiding drop, or by no more than a's climb height, it is skipped, which is nearly always.
  - **Looking down.** A person on top sees down, since nothing between rises above it.
  - **Who reads it.** `sees` replaces "within sight" in every rule that reads sight:
    - ADR 0046's engagement
    - ADR 0054's witnesses
    - #168's explored map, whose cells are revealed only where the player's body sees them
- **The veil.** The core exposes, for a viewer and an area, the area's cells it cannot see, as 1,024 bits, computed when the viewer's cell, level or climb height changes.
  - The view draws veiled cells' terrain outline only, and no people, items, piles or structures on them.
  - A cliff the viewer can leap up is never veiled, so its top shows. A rank 0 viewer sees veiled tops on the base map's 3- to 5-level cliffs, and a rank 4 viewer on none of them.
  - The veil lifts as the viewer leaps up, since its level rises, and as its climb height grows with its realm or footwork.
  - The view asks for the areas on screen, at most about 12 at 3x.
  - **While it lags.** Recomputing every area on screen takes up to about 40 ticks, 1.3 seconds, at a few hundred walks a tick, nearest area first. Until an area's new veil is complete the view keeps drawing its last one, so for that long ground come into sight stays veiled, and ground gone out of sight stays drawn with its people. No rule reads the veil: engagement, witnesses and the explored map read `sees` at once.
- **Height by size, compressed.** The view lifts each level by 16 pixels for the first 3 levels, then by 6 pixels a level beyond.
  - A 5-level cliff draws 60 pixels high, not 80, and a 10-level one 90, not 160. That is under a third of the 270-pixel HUD's height.
  - The step stays the mockup's 16 pixels.
  - The lift per level, the levels drawn at full lift and the lift beyond are the pack's `view/` data, 16, 3 and 6 in the base pack, so a pack can retune them.
  - **Cost.** Compression changes only the view's lift per level, so it costs nothing in the core.
- **The jump in the view.** A leap is drawn as a jump: a crouch at take-off, an arc over the face to the landing cell, and a landing, over the leap's ticks, the same motion up and down. A stamina bar shows under the body while its stamina is below full.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - 15 levels
  - a hiding drop of at least 2
  - a highest cliff of at most 15
  - stamina's maximum below 2^15
  - the per-level cost at least 1 and at most 1,000
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Memory.** Stamina adds 4 bytes a person and a leap in flight 3, 7 bytes, 350 KB. The level nibble is 8 MiB beside ADR 0047's, unless #222 already counted it.
  - **Leaping** is a counter per leaper per tick.
  - **Sight.** Engagement reads `sees` for each candidate within 6 cells. Levels differing by less than the hiding drop or within the reader's climb height skip the walk.
    - **The worst case** is a battle on a terraced slope where every pair differs by 3 and every fighter is of rank 0. #205's index gives at most about 5 candidates a person, so the live tier's 5,000 make about 25,000 walks of at most 7 cells, 175,000 level reads a tick. By this estimate that is about 0.3 ms, under 8% of ADR 0048's 4 ms p99.
    - **A battle on flat ground** reads no level beyond the first.
  - **The veil** is at most 12 areas × 1,024 walks of at most about 30 cells when the viewer moves a cell. That is spread over the next ticks, a few hundred walks a tick.
  - **Paths.** At most four classes of field in areas holding a cliff, and two leap edges for each cliff on the coarse graph.
- **Tiers.** In live and stepped areas people leap tick by tick. A far army crosses a leap edge on the coarse graph at its cost, and a member whose class does not open it makes the army take another edge. Stamina is untracked in far areas and is full on entering a stepped one.
- **Saves, the hash and determinism.** Stamina, leaps in flight and the levels' deltas, if #222 lands, are saved and hashed. Every value is an integer, and the walk visits cells in a fixed order.

The implementation, in order, each a pull request with its acceptance, after ADR 0047's map has levels in the core (#173's steps) and after #205's engagement index:
1. **Levels, classes and steps.**
   - Acceptance: a step walked and a 2-level drop blocked to walking, each with a control.
   - Acceptance: the region unchanged at level 0.
   - Acceptance: `make sweep` 48/50 or better.
2. **Stamina and leaps.**
   - Acceptance, each with a control:
     - a rank 0 person leaping up a 2-level cliff and refused at a 3-level one, where a rank 4 person leaps up a 4-level one, which the base map has
     - a person short of the stamina refused, then leaping once it has recovered
     - a rank 0 person jumping down 4 levels and refused at 5
     - a 4-level leap taking 30 ticks for rank 0 and for rank 8
     - a hit in the air taking 10 stamina, and the leap landing
   - Acceptance: `make sweep` 48/50 or better.
3. **Climb classes in the graph and fields.**
   - Acceptance, each with a control: a class 2 band routed round a 3-level drop where a class 5 band leaps it; a band jumping down within its class's limit; a person short of the stamina waiting at a foot; no NPC path up a drop of 11; a chase up a cliff.
   - Acceptance: `make bench` within budget with the pathfinding cost at the population bound.
   - Acceptance: `make sweep` 48/50 or better.
4. **Sight by height and the veil.**
   - Acceptance, each with a control: a rank 0 person below a 3-level cliff not seeing one on top, where a rank 4 person does; on top, seeing down.
   - Acceptance: engagement and witnesses following `sees`.
   - Acceptance: the veil's bits for an area, by the viewer's climb height.
   - Acceptance: `make bench` within budget with the sight cost at the population bound.
   - Acceptance: `make sweep` 48/50 or better.
5. **The view and the input:** compressed height, the veil drawn, the jump up and down and the stamina bar, read from the core; clicking a cell on top of a leapable cliff leaping there, and a cell beyond reach refused. Acceptance: captures (ko) of a leap up, a leap down, a refused click and the veil.

## Decision

The core holds a level per cell, 0 to 15. A drop of 1 is a step, walked. A drop of 2 to 15 is a cliff, blocked to walking and crossed by a leap. Levels 6 and 7 stay impassable except at passes.

Stamina is a new resource: 100 plus 25 a rank, recovering 5 a second on flat ground. A person leaps up a cliff within its climb height when its stamina pays 40 a level, less by its best footwork stage, and jumps down a drop within twice its climb height for nothing. A leap takes 6 ticks plus 6 a level for every realm, never fails, and allows no strike, cast or engagement; a hit in the air takes 10 stamina. A leap it cannot make is refused, so nobody falls.

Climb classes on ADR 0043's graph and fields open leap edges only to the classes whose floor covers them, up and down, and an NPC starts a leap up only with the stamina for it, a band moving at its weakest. No NPC path leaps up a drop of 11 or more. The steered body leaps where its player walks or clicks, by its own climb height, and a cell beyond reach is not a destination.

`sees` is the one sight rule: a cliff of 3 levels or more, and above the reader's climb height, hides the ground above it from below, on the walk #222 proposes. Engagement, witnesses and the explored map read it, and the core exposes a veil per area for the view, so a viewer sees the top of every cliff it can leap.

The view lifts a cliff 16 pixels a level for 3 levels, then 6 pixels a level, numbers that are the pack's `view/` data, and draws the veil, the jump and the stamina bar.

This supersedes ADR 0047's every drop a blocked cliff, now a 1-level step walked and higher cliffs leapt, and its core that never reads elevation. It also supersedes ADR 0046's and ADR 0054's sight as a reach alone, now `sees`, and ADR 0043's graph without leap edges. It supersedes ADR 0059's lift of 16 pixels for every level, now 16 pixels for the first 3 levels and 6 a level beyond.

## Consequences

- Cliffs become routes for the strong and walls for the weak, so a sect on a mountain is reachable by masters and not by levies.
- A high realm gains a use outside combat, and sees more of the world from below.
- No one dies by a cliff, so cliffs shape travel and sight but never kill.
- Battles on terraced ground hide fighters of low realm from those above them, so ambush from above is possible, and low witnesses below a cliff see nothing of a deed above it.
- Paths cost up to four fields where cliffs are, and the sight rule costs a walk only where levels differ by 3 and more than the reader can leap.
- The view's compressed height reads less steep than the cliff is, a trade the PM directed.
- Nothing changes until the map's levels reach the core.

## Signs the decision was wrong

- Players leap everywhere, so cliffs stop shaping travel, and the per-level cost needs to rise.
- NPC bands stall at cliffs because their weakest member's class is too low, and wars stall with them.
- Fights on slopes cost the tick more than the estimate because levels differ by 3 almost everywhere, so the hiding drop or the walk needs a cache.
- Players find the veil confusing when standing just below a cliff's edge, so it needs a margin.
- Players find it odd that a cliff's top shows to a master and hides from a novice at the same spot.
- The compressed lift makes 4-level and 9-level cliffs look alike, and players misjudge leaps.
- A leap of 6 ticks plus 6 a level looks wrong in the captures, and the art pass retunes it.
