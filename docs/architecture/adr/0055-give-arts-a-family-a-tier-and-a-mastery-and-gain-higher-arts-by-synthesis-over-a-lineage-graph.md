# ADR 0055: Give arts a family, a tier and a mastery, and gain higher arts by synthesis over a lineage graph

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner's feedback and decisions of 2026-10-09, in issue #194, are binding. The arts panel lists only what is known, with no relations and no way upward, and needs a full redesign, with a window of its own like Albion's Destiny Board:
- **An art keeps its skills, its forms.** ADR 0035's shape stays.
- **Arts gain a family and a tier as data.**
  - Families: sword, saber, fist or palm, hidden weapons, footwork, heart method and more.
  - Tiers run from a third-rate art up to a peerless one.
- **Growth is the realm plus a mastery per art.** The realm, the qi store, stays. Each art also has a mastery that rises with use, by casting its skills, fighting and training, as Albion's fame rises per weapon. Forms open by mastery stage, and mastery feeds the comprehension of higher arts.
- **Higher arts by synthesis.** Knowing two or more arts that an edge of the lineage graph names, each at enough mastery, gives a chance to comprehend the higher art. The graph is data, and the rule is the same for NPCs.
- **The lineage window** shows the graph's shape and edges. An unknown higher art's name and content stay hidden, as ???, until it is comprehended, or learned of by rumour or manual.

Decided on PR #218's review, 2026-10-10:
- **The owner's earlier decisions bind here.** An art's skill needs its discipline's weapon family, as the owner decided on #211, and darts are used up, as #215 records.
- **By the PM.**
  - The mastery cap and rates, the five stages, manuals giving only the first stage's forms, no manual for tier 3, and the rarity of 1 in 200 are proposals in pack data, which the sweep checks.
  - The window's key, L, and its mockup go to the PM in step 4.
  - **Teaching** here is ADR 0035's existing sect teaching, which this ADR lets raise mastery. It is not the feature the owner holds, a player asking a master for a lesson, which this ADR does not design.

Facts from `main` at `aff1434`:
- **Arts today** (ADR 0035, `core/region.cpp`). There are three arts: Plum Blossom Sword (sword), Heavenly Demon Saber (saber) and Black Fang Darts (hidden weapons), each with three skills of ranks 1, 3 and 5.
  - Skills are learned three ways: comprehension, a roll on each realm gained for the art's first comprehensible skill lacked; reading a manual, every skill the rank allows; and sect teaching, a yearly chance for the first skill lacked.
  - Known skills are an append-only list.
  - A person's nature is that of the first art it learned, its heart method.
- **Attainment.** Each discipline grows by practice up to its talent limit (ADR 0019). Every person draws a talent growth of 5 to 15 and a limit of 300 to 700 per discipline (`world-generation.md`).
- **Progress.** ADR 0036 notes each skill learned, with how, and each realm reached, in a store of the latest 32.
- **The arts panel** (#161) opens with K. The keys the HUD and play scene bind are C, E, F, G, K, M, Q, R, T, W, X and Z, so L is free.
- **Pending designs.**
  - ADR 0049 renames realms, qi and the heart method to general words.
  - An art's skill needs its discipline's weapon family, as the owner decided on #211. The open PR #215's ADR 0050 designs it.
  - ADR 0046 gave each skill a shape. Issue #183 is done.

Options weighed:

- **Family and tier.** Each art gains a `family` and a `tier`, pack data (ADR 0049).
  - A family names its discipline, the talent axis it practises. It also names the weapon family its skills need, or none, as the owner decided on #211.
  - The base pack's families:

    | Family | Discipline | Weapon |
    |---|---|---|
    | Sword | sword | sword |
    | Saber | saber | saber |
    | Fist and palm | fist | bare hands or gauntlets |
    | Hidden weapons | hidden weapons | darts |
    | Footwork | fist | none |
    | Heart method | medicine | none |

    Footwork and heart method share the talent of fist and medicine, so no new talent axis is drawn at birth. What their arts do beyond mastery and synthesis is out of this ADR's scope, below.
  - A tier is one of five, from 1 to 5: third-rate (삼류), second-rate (이류), first-rate (일류), peak (절정) and peerless (절세). It sets how slowly mastery rises in the art, and where the window places the art.
  - Rejected: families as a fixed list in the core. A re-theme names its own (ADR 0049).
  - **No theme word in the core.** The core knows a family only as a pack id with its discipline and weapon, and a stage only as an index, 0 to 4, with its threshold. The names above, initiate, major, heart method, footwork and the rest, are the base pack's text. Wherever this ADR says a stage's name, the core reads the stage's index.
- **Mastery.** Each art a person knows carries a mastery from 0 to 10,000 beside it, in the list of known arts.
  - **What raises it.**
    - Each use of one of the art's skills that hits anyone or anything adds the person's talent growth in the art's discipline, 5 to 15.
    - Each day, a sect member whose seat holds a standing training ground (ADR 0026) adds half its growth to the art it has used least recently, while the ground's pupils cover it.
    - A day of sect teaching (below) adds a tenth of the teacher's mastery in the art, at most 50.
  - **How fast.** Each gain is multiplied by the art's tier's rate per mille: 1,000, 800, 600, 400 and 250 from third-rate to peerless.
  - **How far.** Mastery never passes the person's talent limit in the discipline × 10,000 / 700. A limit of 700 reaches 10,000, and one of 300 stops at 4,285, so talent matters and stays hidden.
  - **Stages.** These are pack data:
    - initiate (입문), from 0
    - minor (소성), from 1,000
    - major (대성), from 3,000
    - consummate (원숙), from 6,000
    - ultimate (극의), at 10,000
  - **How long it takes.** Growth of 10 on a second-rate art is 8 a hit. About 375 hits reach major: about twenty skirmishes for a fighter, or 750 days, about six years, of daily training at 4 a day for a disciple.
  - Rejected: mastery as a share of attainment in the discipline. Two arts of one discipline would then rise together, and an unused art would master itself.
- **Forms open by stage.** Each skill of an art names the stage that opens it, beside ADR 0035's rank. A person learns a skill of an art it knows once its mastery reaches the skill's stage and its realm the skill's rank, checked when either rises. The base pack opens an art's three forms at initiate, major and consummate, with ADR 0035's ranks 1, 3 and 5 kept.
- **Gaining an art.**
  - **A manual** teaches the art's forms of stage 0, the base pack's initiate, that the reader's rank allows. Its later forms open by mastery.
  - **Sect teaching** now teaches an art the sect holds a manual of, its forms of stage 0, with ADR 0035's yearly chance. It then raises mastery daily, as above.
  - **Synthesis** comprehends a higher art (below).
  - ADR 0035's comprehension on a realm gained is replaced. Forms open by stage with no roll, and comprehension now means synthesis, which mastery feeds as the owner decided.
- **The lineage graph.** The graph is pack data:
  - its nodes are the arts
  - its edges each name two or three arts with the stage each must reach, the higher art they lead to, and a chance per mille a year
- **The synthesis rule.**
  - Once a day, with the other daily rules spread over the day (ADR 0048), a person who meets an edge's every condition and lacks its higher art rolls the edge's chance for a day.
  - The roll is scaled by its talent limit in the higher art's discipline over that discipline's mean limit, as ADR 0035 scaled comprehension, and by the nature's ease (ADR 0035).
  - **The integer roll.** The day's chance is the edge's per mille a year, times the talent scale per mille, times the ease per mille, multiplied first and divided last, rounded down. It succeeds when a draw from the world's one seeded generator, in [0, 1,000 × 120 × 1,000 × 1,000), falls below it. People are taken in index order, and each person's edges in the order of the arts it knows, then by edge index, so the draws come in one order on every host.
  - On success it learns the higher art's forms of stage 0, and the progress store notes it, with how: by synthesis.
  - It is one rule for every person, steered or not. A person meets few edges, so the daily check reads at most the edges whose first art it knows.
  - Rejected: synthesis by a quest or an order. The owner made it a chance, the same for NPCs.
- **The base pack's starting graph.** Six families of three tiers. The existing three arts keep their skills, and the others are new data with three forms each:

  | Family | Tier 1 | Tier 2 | Tier 3 |
  |---|---|---|---|
  | Sword | Three Talents Sword (삼재검법) | Plum Blossom Sword (매화검법) | Plum Blossom Divine Sword (매화신검) |
  | Saber | Five Tigers Saber (오호단문도) | Heavenly Demon Saber (천마도법) | Heavenly Demon Sovereign Saber (천마군림도) |
  | Fist and palm | Long Fist (장권) | Arhat Fist (나한권) | Hundred Paces Divine Fist (백보신권) |
  | Hidden weapons | Flying Stones (비황석) | Black Fang Darts (흑아비도술) | Heavenly Maiden Scatters Flowers (산화천녀수) |
  | Footwork | Grass Floating (초상비) | Dark Fragrance Steps (암향표) | Cloud Ladder (제운종) |
  | Heart method | Breathing Method (토납법) | Purple Mist Divine Art (자하신공) | Heavenly Demon Divine Art (천마신공) |

  Six edges, with the stages named:

  | From | To | Chance a year |
  |---|---|---|
  | Three Talents Sword at major, Grass Floating at minor | Plum Blossom Sword | 50 per mille |
  | Long Fist at major, Breathing Method at major | Arhat Fist | 50 per mille |
  | Plum Blossom Sword at consummate, Dark Fragrance Steps at major, Purple Mist Divine Art at major | Plum Blossom Divine Sword | 10 per mille |
  | Heavenly Demon Saber at consummate, Heavenly Demon Divine Art at major | Heavenly Demon Sovereign Saber | 10 per mille |
  | Arhat Fist at consummate, Cloud Ladder at minor | Hundred Paces Divine Fist | 10 per mille |
  | Black Fang Darts at consummate, Dark Fragrance Steps at major | Heavenly Maiden Scatters Flowers | 10 per mille |

  - Sects hold the manuals of their tier 2 arts as today. Tier 1 arts' manuals are placed in the world by the pack, as a fallen sect's are. No tier 3 art has a manual at the start, so synthesis is the only way to one.
- **Out of scope, as open questions.** #194 did not ask for these, so they are left to follow-ups and listed in `simulation.md`:
  - what footwork and heart-method arts do beyond their skills, such as speed or cultivation, which would touch ADRs 0037 and 0046
  - which art a person's nature comes from, which stays ADR 0035's first art learned
  - whether talking to a person reveals its arts, which would change ADR 0039
  - whether people draw an art at birth, which would change the region's generation
- **Known of.** The player's soul keeps a set of the arts it knows of, one bit an art, saved and hashed.
  - An art enters it when the body learns it, or stands within reading reach of its manual. Hearing of an art by rumour waits for a rumour that carries arts, an open question.
  - The window names an art in the set, and shows ??? for the rest.
  - It is a fact about one soul. No rule for people no driver steers reads it.
- **Bounds, with refusals.**
  - A person knows at most 16 arts, pack data within a core bound of 16. A manual, teaching or synthesis gives a person at its bound no new art, so a pack cannot grow a person without limit.
  - Each known art carries its art, its mastery and the tick its skills last hit, 8 bytes. The last-use tick is what training's least recently used reads.
  - A pack defines at most 256 arts, since the soul's set is a bit an art, and an art leads at most 16 edges, with at most 4,096 edges in all. A pack past any of these is refused at load with a `ContentRefusal` naming the bound (ADR 0049).
- **Saves and the hash.**
  - A person's known arts are 8 bytes each. At 50,000 people knowing about 2 arts each that is 800 KB, and at the bound of 16 each, 6.4 MB.
  - The soul gains 32 bytes, for up to 256 arts. A skill's stage and the graph are pack data, not save data.
  - `SAVE_VERSION` rises. The pinned hash changes, since mastery is folded.
- **Cost.**
  - Mastery is an addition in the strike rule for each hit with an art skill.
  - Training and synthesis are daily rules, spread over the day. Synthesis reads the edges led by each art a person knows, at most 16 arts × 16 edges, 256 checks. At 50,000 people spread over 1,800 ticks that is about 28 people and at most about 7,100 checks a tick, a few hundredths of a millisecond.
  - In a battle of #216's live tier of 5,000, each hit with an art skill finds its art among at most 16 known and adds to its mastery: a few hundred additions a tick.
- **The measure plan, `make sweep`.** Per seed, at years 10, 50 and 100, the sweep prints:
  - the count of known arts at each stage, by family and tier
  - the people who ever reached each tier, and by which way: manual, teaching or synthesis

  The implementing pull request tunes the chances until, over seeds 0 to 49 for 100 years, tier 3 arts stay rare: on average fewer than 1 in 200 of the people who ever lived reach one, and at least one in 10 seeds sees one.
- **The lineage window.** It opens with L, apart from the K panel, which keeps the five-slot loadout.
  - **Layout.** Columns are families and rows are tiers, peerless at the top.
  - **Nodes.**
    - An art the person knows shows its name, its stage on five pips, and its mastery as a bar.
    - An art its soul knows of but has not learned shows its name dimmed.
    - Any other art shows ??? on a blank seal.
  - **Edges.** Edges are drawn from their arts to the higher art, each labelled with the stage it needs, ticked when met. The core computes whether each condition is met, so the view decides nothing.
  - **Selection.** Selecting a node lists its forms with the stage and rank that open each. An unknown node lists nothing.
  - On a touch screen a tap selects and the window scrolls.
  - **Display fields.** The window's display fields, a family's colour and icon and a node's seal, are `view/` data, and names are `text/`, per ADR 0049.
  - The implementing pull request presents a mockup in `art/mockup/` for the PM's approval before the window is built, as earlier panels did.
- **Supersessions.** This ADR supersedes three points of ADR 0035:
  - its comprehension on a realm gained, replaced by forms opening by mastery stage and by synthesis
  - its manual teaching every skill the rank allows, now the initiate forms, with the rest by stage
  - its teaching of the first skill lacked, now an art's initiate forms and then daily mastery

  It also supersedes ADR 0036's three ways a skill is learned, which gain synthesis.

The implementation, in order, each a pull request with its acceptance:
1. **Family, tier and mastery.**
   - Acceptance: mastery rising by hits, training and teaching, scaled by tier and capped by talent, each with a test and its control.
   - Acceptance: forms opening by stage and rank, and the manual and teaching rules as above.
   - Acceptance: the pinned hash recorded, and `make sweep` rerun.
2. **The lineage graph and synthesis.**
   - Acceptance: a person meeting an edge comprehending its art at the chance, and one short of a stage never.
   - Acceptance: the starting graph's 18 arts and 6 edges.
   - Acceptance: the sweep's rarity target held.
3. **The lineage window,** after its mockup is approved. Acceptance: known, known-of and ??? nodes, and edges with their conditions.

## Decision

Arts gain a family and a tier as pack data. A family names its discipline and the weapon its skills need, and a tier sets how slowly mastery rises. Each known art carries a mastery from 0 to 10,000, raised by hits with its skills, by daily training at a sect's training ground and by teaching, and capped by talent. Its stages, initiate, minor, major, consummate and ultimate, open the art's forms together with the realm's rank.

Manuals and teaching give an art's initiate forms. Higher arts come by synthesis: a daily chance, scaled by talent, for anyone meeting an edge of the lineage graph, which is pack data of arts, stages, a higher art and a chance. The base pack starts with six families of three tiers and six edges, and tier 3 arts have no manual, so they stay rare.

What footwork and heart methods do beyond their skills, nature, arts named in talk and arts at birth are left to follow-ups. The player's soul keeps the arts it knows of, for a lineage window on L that hides the rest as ???.

This supersedes ADR 0035's comprehension on a realm gained, its manual teaching every skill the rank allows, and its teaching of the first skill lacked; and ADR 0036's three ways a skill is learned.

## Consequences

- Fighting and training now raise a person's arts, so veterans hold stronger forms than recruits of the same realm.
- The arts' balance moves to data: the tiers' rates, the stages and the edges' chances.
- Saves grow by about 800 KB at 50,000 people knowing 2 arts each, and by at most 6.4 MB at the bound of 16 arts each. The pinned hash changes.
- The K panel stays the loadout. The arts' depth moves to the L window.

## Signs the decision was wrong

- Mastery from hits makes the factions that fight most master everything, so training needs a larger share, or hits a smaller one.
- Synthesis never happens in the sweep, or happens to most elders, so the chances are wrong by an order of magnitude.
- Players find the ??? nodes frustrating rather than tempting, because nothing they can do reveals them.
