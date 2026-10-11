# Game Concept

This document is the single reference for the game concept agreed with the user.

## Contents
- Status tags
- Identity [agreed]
- World and scale [agreed]
- Factions and the rules for gaining power [agreed]
- Sects [agreed]
- Reputation and rumors [agreed]
- Village life cycle [agreed]
- Growing civilization [agreed]
- Talent [settled]
- Reincarnation [settled]
- Lifespan [settled]
- Martial arts [draft]
- Qi and martial arts [agreed]
- Arts that reshape the world [agreed]
- Mods and re-theming [agreed]
- Gear, wealth and livelihoods [agreed]
- MVP scope [proposed]
- Open questions [open]

## Status tags

Each section carries one of the following statuses.

- **[settled]** Agreed and final.
- **[agreed]** The direction is agreed, but the details may change.
- **[draft]** At the draft stage.
- **[proposed]** At the proposal stage and not yet final.
- **[candidate]** A candidate whose inclusion is not yet decided.
- **[open]** Not yet decided. This document does not decide it.

## Identity [agreed]

- **One-line pitch**: "A single life dropped into a living gangho (the martial world of wuxia fiction). When it dies, it is reborn as someone else in the same world."
- **Three pillars**
  1. **A living gangho**: a simulation in which factions and villages change even without the player.
  2. **A wuxia story of one life**: the starting point and the path differ by origin.
  3. **Reincarnation**: the world is never reset.
- **Principle: the player is also one of the world's NPCs.** There are no player-only rules; the same rules as for NPCs apply (talent, lifespan, and so on). The one exception is caves, which only the player enters (§Gear, wealth and livelihoods).
- **Principle: realistic scale.** Decided by the owner on 2026-10-10 (issue #227, from #200 and #169). Places, buildings and land follow real proportions, measured against a person, who fills a cell.
  - **Settlements.** A walled town encloses a real town: at least about 40 × 30 cells inside thick, tall walls, with corner towers, a gate tower over each gate and 2 to 4 gates, around streets, wards, a market square, a hall or yamen, temples and courtyard houses. A major city is larger again, about 80 × 60 cells or more, with inner wards. A village stays unwalled and smaller.
  - **What the principle covers.** It covers the size of villages, towns and cities and their walls, fields per household, roads by class, river widths, bridges and ferries, sect seats as compounds rather than a hall, and a household's house against a hall.
  - **Exaggeration** is allowed only where a rule needs it, and is stated where it is made.
  - **Two scales.** The li of ADR 0047 measures the distance between places. A place's layout patch carries the size of what stands there, at a person's scale. A town of 40 × 30 cells does not move its neighbours 40 li away.
  - **[proposed]** The reference sizes in `docs/art-direction.md` §Scale, the base pack's proposals in pack data.
  - **Fields and their yield.** Decided by the PM on #242, 2026-10-11: a household's fields stay at 4 to 8 cells, and the base pack's yield is lowered to about 1 person a cell, so food binds the population.
- **Principle: simple rules, complex consequences.** Decided by the owner on 2026-10-10 (issue #229). The world runs on simple rules whose consequences combine into complex cause and effect, the way money shapes villages' forms, their growth and each person's life. Our job is to tune the balance, not to script outcomes.
  - **Population has no separate limit: it is bounded by the food that can be had.** Fruit, game and fish regrow, fields yield, and how fast they regrow is what governs how many people the world feeds.
- **Genre and engine**: 2D RPG, Godot 4.7.
- **Perspective and combat**: a three-quarter top-down view, with combat in real time (`docs/architecture/adr/0012-present-the-world-in-a-three-quarter-top-down-view.md`).
- **Art direction**: pixel art on a 16-pixel tile grid (`docs/art-direction.md`).
- **Language**: UI text in Korean first, with English supported.
- **Title**: "Murim24" is a working title for development. The 24 has no meaning, and the official title is decided after the MVP.

## World and scale [agreed]

Decided by the user on 2026-10-08 (issue #173), and revised by the owner on 2026-10-09 (issue #201) and 2026-10-11 (issue #255): one hand-made map, physical distances, an offline game. The design is ADR 0047 (`docs/architecture/adr/0047-author-one-continuous-map-of-4096-cells-by-hand-at-a-li-a-cell.md`), with ADR 0070 (`docs/architecture/adr/0070-double-the-map-to-8192-cells-of-half-a-li-so-every-journey-takes-twice-as-long.md`), proposed, enlarging it, and the quality profiles of ADR 0048.

- **One continuous map.** The world is one map, vaster than the 4,096 x 4,096 cells first planned (revised by the owner, 2026-10-11, #255), with no transitions between regional maps. The regions are named lands on it, and travel between them crosses the land itself: ranges and rivers block the way, and passes, fords and ferries are places one walks through.
- **Made by hand, the same in every game.** The map is authored, not generated, and every player gets the same one. No seed shapes the terrain; a new game's seed draws only the simulation's randomness: who is born with what talent, and what happens.
- **Physical distance.** Revised by the owner, 2026-10-11 (#255): the world is vaster, so travel takes at least twice as long as before (about 12 minutes from Luoyang to Chang'an on foot at 1x). The owner chose a bigger world over the same size made emptier, and over building the 4,096 map sooner. Distances between places stay physical, and the outer lands are compressed toward the map's edge. Roads are faster than open ground; horses and river boats are planned.
- **[proposed]** ADR 0070's details (`docs/architecture/adr/0070-double-the-map-to-8192-cells-of-half-a-li-so-every-journey-takes-twice-as-long.md`):
  - a map of 8,192 x 8,192 cells, a cell half a li, 250 metres, between places in the heartland, so every journey is twice as long
  - towns, fields and roads keeping their sizes at a person's scale
  - elevation, biome and region painted per 16 x 16 cells, the same hand work as before
  - inns on the roads a day's walk apart, and caves in the wilderness
  - a core budget of 128 MiB on the low profile for the map, about 95 MiB at worst, and a load within 2 seconds
  - **[proposed]** ADR 0061 (`docs/architecture/adr/0061-travel-on-foot-by-horse-or-by-boat-at-speeds-set-by-travel-class-and-means.md`), for issue #207: horses as items in a mount slot, bought at towns with a stable, fed a ration a day, and ridden at about twice walking pace, striking only with a weapon's basic strike; boats owned by merchant houses and villages, sailing river and canal routes between landings for a fare, fast downstream and slow upstream; ferries charging a fare.
- **An offline game.** There is no online play, and none is planned.
- **The regions.** Ten regions, each with its real basis:
  - 중원 (Central Plains): Henan and the lower Yellow River. Luoyang, Kaifeng, Song Shan and Shaolin.
  - 관중 (Guanzhong): the Wei valley. Chang'an, and Hua Shan, where Hwasan sits.
  - 하북·산서 (Hebei and Shanxi): Taiyuan and Yanjing, and the Taihang, where the Black Fang sits.
  - 사천 (Sichuan): the Sichuan basin. Chengdu, Chongqing and Emei.
  - 호광 (Huguang): the middle Yangtze and its lakes. Wuchang, Xiangyang and Wudang.
  - 강남 (Jiangnan): the Yangtze delta. Nanjing, Yangzhou, Suzhou and Hangzhou.
  - 서역 (Western Regions): the Hexi Corridor, the deserts and Tianshan, where the Demonic Cult sits.
  - 운남·남만 (Yunnan and the southern barbarians): Dali, and Lingnan beyond the Nanling.
  - 새북 (beyond the northern wall): the steppe, a frontier.
  - 요동 (Liaodong): Liaoyang and Changbai, a frontier.
- **The gates.** Regions join only through these:
  - Tong and Hangu passes
  - Jianmen Pass
  - Wu Pass
  - the Mengjin and Pujin ferries
  - the Three Gorges
  - the Nanyang gap
  - the Huai fords and the Grand Canal
  - Meiling Pass
  - Yanmen and Juyong passes
  - Shanhai Pass
  - Jiayu Pass
  - the oasis route
- **Natural borders.** The map is closed by the sea, mountain ranges and desert, never by a hard edge.
- **Sects on famous mountains.** Each sect's seat stands on a famous mountain: Hwasan on Hua Shan, the Black Fang in the Taihang, and the Demonic Cult on Tianshan in the far west, as Korean wuxia places it. Other famous mountains stand as landmarks until their sects are added.
- **Villages grow.** The map places only the starting cities, major towns and sect seats. Every other village is founded and grows by the villages' own rules, the same way in every game.
- **Distance level of detail.** Places near the player are simulated in full; far places are simulated coarsely. Every person stays an individual in every tier (§Growing civilization).
- **Cliffs are terrain to climb.** Decided by the owner on 2026-10-09 (issue #214). ADR 0058 (`docs/architecture/adr/0058-leap-up-and-down-cliffs-by-realm-and-stamina-and-veil-a-cliffs-top-from-those-who-cannot-leap-it.md`), proposed, designs it.
  - A person of a high realm climbs a cliff easily, and a low one less.
  - A high cliff, not a hill or a low step, hides what is on top of it from those below. Low steps stay walkable and visible.
  - A cliff's height shows by its size on screen.
  - Decided by the owner on 2026-10-10 (#223's review): climbing spends a stamina of its own, not qi. A climber neither strikes nor casts, and a hit takes its stamina. Footwork arts' effect is climbing; whether they also give speed stays open.
  - Revised by the owner on 2026-10-10 (#214), in place of a hand climb and a fall: every climb is a leap with a jump motion, and going down is a leap too. Only a drop the person can descend may be jumped down, so nobody leaps to their death. A leap's duration follows the height, the same for every realm.
  - Decided by the owner on 2026-10-10 (#214): a cliff the viewer can climb shows its top, and the veil stays only on a cliff higher than the viewer can climb. From above, the ground below is visible. Clicking a cell on top of a climbable cliff walks to its foot and leaps up; a cell on a cliff too high is not a destination.
  - Decided by the PM on 2026-10-10, delegated by the owner (#214): a person may jump down twice the height it can climb, a jump down costs no stamina, and a leap takes 0.2 seconds plus 0.2 seconds a level.
- **[proposed]** ADR 0058's details:
  - steps of one level walked, and cliffs of two levels or more leapt
  - stamina recovering only on flat ground
  - a band leaping only where its weakest member can
  - a cliff of three levels hiding the ground above it from those who cannot leap it, drawn veiled, the three as pack data
  - the veil and sight reading the height a person can climb on full stamina, not its current stamina
  - the drawn height compressed above three levels, its numbers as pack data
- **The map is the player's own.** Decided by the owner on 2026-10-08 (issue #168): the map completes and updates as the player moves, and does not update a place until the player has looked at it again. ADR 0073 (`docs/architecture/adr/0073-chart-the-land-a-soul-has-seen-and-show-each-village-as-it-was-last-seen.md`), proposed, designs it.
- **[proposed]** ADR 0073's details:
  - the chart kept by the soul, so a body born again keeps it, and a new game starts blank
  - a map sight of its own, 15 cells in the region, wider than the 6 cells within which people close on a foe
  - a village shown with its owner, residents and stage as last seen, and borders drawn from the villages remembered
  - a map window on N, with its touch button deferred
- **The same world on every device.** A quality profile, chosen by the device or the player, sets only how much is drawn and how far around the player people are simulated in full. The map, its villages and its people are the same on every profile. Far events may turn out differently on different profiles, which is accepted for an offline game.
- **The population bound is measured.** The map needs far more people than the first region did to feel lived in. The bound is raised from about 500 to the largest number between 10,000 and 50,000 that keeps a tick's p99 within its 4 ms budget with the level of detail, measured on the lowest quality profile. It is measured on PC, macOS first and Linux when available.

## Factions and the rules for gaining power [agreed]

| Faction | Values | How to become its leader | Joining requirement |
|---|---|---|---|
| Demonic Cult (Magyo) | The strong prey on the weak | Challenge the Cult Leader and win the duel. The Cult Leader's allied factions become enemies | Only a member of the Cult may challenge |
| Orthodox faction (Jeongpa; the Murim Alliance, Murimmaeng) | Righteousness and chivalry | Win an election by gathering the favor of sect elders. Cannot be taken by force | Enter a sect; infamy below a set threshold |
| Unorthodox faction (Sapa) | Profit | Money, bribery, assassination, and backroom deals | Money, or proof of usefulness |

- **Factions run policies.** Decided by the owner on 2026-10-10 (issue #230). Through a faction management system, a faction sets policies that adapt it to the world's causes and effects, so it strengthens and expands itself. A player who leads a faction sets them, and an NPC leader chooses them by rules. ADR 0065 (`docs/architecture/adr/0065-run-each-faction-by-a-few-policies-that-move-existing-rules-inputs-set-by-its-leader.md`), proposed, designs it.
- **[proposed]** ADR 0065's details:
  - eight policies: tax with its kinds, a levy of soldiers, corvée, spending priorities, trade, diplomacy, settlement and law
  - each policy moving an existing rule's input, never adding a bonus
  - an NPC leader changing a policy a step at most, after a year's term, by its faction's state and character
  - a faction management window for a leader
- **[candidate]** The martial clans (Sega; succession by bloodline and marriage) and the government (Gwanbu) are listed as candidates only. They are not in the MVP and are not decided.

## Sects [agreed]

- **A village and a sect are separate.**
  - A **village** is where people live. Its population cap comes from its housing, food and storage.
  - A **sect** (munpa, for example Hwasan or Wudang) is an organization within a faction, with members, elders, martial arts and a seat (bonsan).
- **Members may live in several villages.** Membership does not follow from where one lives.
- **Each can fall without the other.** A sect can be wiped out while its villages stand, and a village can burn while its sect survives.
- **No fixed population per village.** The economy sets each village's cap, under the world's population bound (§Growing civilization).

## Reputation and rumors [agreed]

Decided by the owner on 2026-10-08 and 2026-10-10 (issue #175, PR #217). ADR 0054 (`docs/architecture/adr/0054-keep-each-groups-standing-toward-a-person-changed-by-deeds-seen-or-heard.md`), proposed, designs the groups' standing.

- **Groups hold standing toward a person, each separately:** the village, the sect and the faction. A deed counts against or for every group the other person belongs to, with weights. Extorting a sect disciple who lives in village A drops A a lot, the sect moderately and the faction a little.
- **Individual NPCs hold a personal favor toward a person.** This stays agreed and is implemented later, in issue #219 (https://github.com/mandacode-labs/murim-24/issues/219), a follow-up to ADR 0054, which does not cover it. In outline: favor is held only toward people one has dealt with often, and is bounded per person.
- **[proposed]** ADR 0060's details. ADR 0060 (`docs/architecture/adr/0060-keep-each-persons-favor-toward-the-few-it-deals-with-often-beside-its-groups-standing.md`), proposed, designs it:
  - favor toward at most 8 people: kin, those dealt with three times in 30 days, and anyone whose single deed moved one strongly
  - deeds felt first-hand at full weight, by kin less, by witnesses least, and betrayal counting double
  - a person's own conduct following its favor where it holds one, and its groups' standing otherwise
- **Deeds.** Through the dialogue a person can attack, extort, threaten, help or give a gift, and the groups of the other person remember it.
- **Deeds become known through witnesses and rumour.** The witnesses' groups react at once. Farther groups learn as the rumour spreads over time. A deed no one saw stays hidden.
- **Standing belongs to the body.** A reborn body starts with no standing. Warehouses and caves stay with the soul.
- **The same rules for NPCs and the player.** Bandits raiding a village, or a disciple killing a rival: the world builds its own grudges and debts.
- An epithet (byeolho) is generated automatically from one's deeds.

## Village life cycle [agreed]

Prosperity → Raided → Ruins → (survivors return / another faction moves in) → Rebuilt → Prosperity

- The faction a village belongs to can change depending on who rebuilds it.
- The memory of who destroyed it remains after it is rebuilt.

## Growing civilization [agreed]

- **The world starts with few people.** NPCs meet and have children, and a civilization grows from them.
- **Genealogy and families.** Every NPC has parents and belongs to a family (clan), and the genealogy is kept in the world.
- **Every NPC is an individual person**, with a name, parents, a family, talent and a lifespan.
- **An army is a roster** of the NPCs conscripted into it, never a count of soldiers.
- **Population follows food.** Revised by the owner, 2026-10-10 (#229), in place of a cap on each village by its housing, food and storage (`docs/architecture/adr/0022-cap-each-village-by-its-housing-food-and-storage.md`):
  - A village's population follows the food it can have. No number stops it.
  - Housing and storage limit people only through what they cause: crowding and a full store.
  - A shortage brings starvation or migration.
- **Nature grows on its own.** Decided by the owner on 2026-10-09 (issue #187). ADR 0056 (`docs/architecture/adr/0056-grow-natural-objects-in-slots-the-map-places-and-build-villages-from-what-people-gather.md`), proposed, designs it.
  - Terrain is a biome, and the trees and fruit on it grow naturally. Today's forest is ground on which trees grow, spread and can be felled.
  - NPCs move, gather and hunt on their own, and can found new villages.
- **Animals arise where they can live.** Decided by the owner on 2026-10-10 (issue #171). ADR 0067 (`docs/architecture/adr/0067-let-animals-arise-where-food-feeds-them-hunt-them-for-meat-and-materials-and-let-spirit-beasts-hold-territories.md`), proposed, designs it.
  - An animal appears on its own where its kind can live, at a rate set by the food there: forage for grazers, prey for predators. There are no fixed spawn points, and none appear in deserts without forage or in towns.
  - Animals do not mate or raise young. Grazers crop forage, which regrows, and predators hunt grazers. Their numbers follow food: a balance of predators, prey and forage, not a cap.
  - Spirit beasts (영수) are rare, strong animals that hold a territory, attack anyone entering it and never roam beyond it. A spirit beast arises the same way, in a suitable territory left empty, so killing one leaves it empty for a while.
  - A spirit beast leaves a spirit core (영단), a cultivation item consumed or refined for qi or realm progress, and materials worth more than an ordinary animal's.
  - Every animal leaves materials when killed, by species as pack data. Meat is food: it feeds the hunter's household and counts as food. Hide, bone and horn feed crafting.
  - Hunters are a trade, and NPCs hunt for food and materials as the player does.
  - Decided by the owner on 2026-10-11 (#171): many kinds of creatures, as pack data: grazers, browsers, small game, predators of several sizes, birds and fish where cheap, and spirit beasts. Some kinds move and graze in herds, as one, by simple flocking behind a leader or centre that follows forage. Prey that sees a predator flees toward its group or cover, and predators hunt, chase and give up after a while. The ecosystem balances from forage to grazers to predators, bounded by food with no caps, with no extinction everywhere and no explosion over 100 years. Animals avoid predators and larger groups, giving way when outnumbered, by the same threat appraisal people use.
- **[proposed]** ADR 0067's details:
  - grass as a forage slot, a grazer needing 3 of them
  - predators eating every 16 days, starving after 3 cycles unfed
  - herds of up to 12 behind their lowest-index member, flocking within 4 cells of their centre
  - a predator's chase budget of 10 seconds, and in areas not simulated in full a kill chance from the two kinds' speeds
  - birds and fish as gathered slots, not animals
  - an appearance chance scaled by the spare food: forage over the grazers' need, prey over the predators'
  - none appearing in an area holding a village structure
  - hunting as a livelihood for idle earners within 32 cells, not paid by a treasury, meat eaten at home first
  - spirit beasts at least 48 cells from every village and 128 from another, holding 12 cells
  - a spirit core adding its qi up to the talent's limit, or refined by an apothecary into pills
- **Villages are built from trees and stones.** Trees and stones are world objects, and NPCs build villages from them, automatically. Construction stays NPC-only: the player may gather, but never builds.
- **Villages grow like WorldBox,** with people and resources: centre first, houses placed outward into suitable ground, expanding, and founding new villages. Villages are larger by default.
- **[proposed]** ADR 0056's details:
  - natural objects in fixed slots the map places, with fruit trees, bushes, rocks and herbs beside trees
  - a village's materials account, with its idle earners assigned to gather what its next building lacks and paid from the treasury
  - buildings rising only as far as their materials are delivered
  - settlers carrying a first house's materials
  - hunting waiting for animals (#171)
- **Walls grow with a village.** Decided by the owner on 2026-10-10 (issue #228). ADR 0062 (`docs/architecture/adr/0062-raise-a-towns-walls-by-its-wealth-its-factions-strength-and-the-threats-it-meets-in-stages-that-grow-with-it.md`), proposed, designs it.
  - A village becomes a walled town as it and its faction grow. Whether it has walls, and how great they are, depends on the faction's strength and the village's wealth.
  - Building walls, like building the village itself, takes materials and labour.
  - Walls are part of the village and grow with it. When the town expands past its walls, suburbs come first, and a new, larger circuit follows once wealth and labour allow. The old wall stays as an inner wall or is taken down and its stone reused.
- **[proposed]** ADR 0062's details:
  - a circuit planned when a village reaches a town's scale and wealth, its faction is strong, and threats have pressed it
  - earthwork, then stone, then towers, over about ten years
  - corvée for a threatened town, at a cost in discontent
  - gates that close before a hostile army
  - upkeep and decay
  - hunting by ADR 0067's animals
- **Construction is the NPCs', never the player's.** As a village grows, its NPCs decide what to build and build it: houses, fields, granaries, and later workshops such as a smithy. The player never places or orders a building. Growth lays structures and residents raise them by the repair rule (`docs/architecture/adr/0023-let-villages-build-found-and-raid-as-they-press-their-caps.md`).
- **No separate limit.** Revised by the owner on 2026-10-10 (issue #229): population is bounded by the food that can be had, not by a limit of its own (§Identity).
- **[proposed]** ADR 0063's details (`docs/architecture/adr/0063-bound-the-population-by-the-food-that-can-be-had-and-keep-the-living-bound-a-technical-capacity.md`):
  - births scaled by a household's days of food, from its purse and its village's stock, and slowed by crowding past housing
  - a village's housing and food caps kept only as consequences, read by growth and migration, with no birth stopping at them
  - the core's people storage a technical capacity, measured as ADR 0048 measures the bound, which the balance must never reach
  - field and wild food yields by biome and season in pack data, as the levers
  - `make sweep` reporting population, food, money, villages and hunger against target bands over 100 years
- **[proposed]** Expansion (building, new villages, territory, raids) is the motive for war.
- **[proposed]** Building kinds beyond the current set. The current set, in `core/region.cpp`, is houses and halls (housing), fields (harvest), granaries and storehouses (storage), and wells, barrels and fences, which hold nothing. Proposed additions:
  - Smithy: makes and mends weapons.
  - Apothecary: makes medicine and heals the wounded.
  - Training ground: where residents train their martial arts.

## Talent [settled]

- Every living being receives talent at random at birth.
- Talent is not current ability but **potential** (growth rate and limit).
- Example axes: physique, comprehension, inner energy affinity, aptitude for a discipline (sword, saber, fist, hidden weapons, medicine, and so on), rare special constitutions.
- **Talent is hidden.** It is revealed through how training feels, through abilities such as reading faces and taking the pulse, and through the assessment of masters.
- Talent takes on meaning multiplied by the circumstances of one's origin (the resources of one's family and faction).

## Reincarnation [settled]

- On death, one is reborn in the same world into a random faction and into a family of its genealogy (§Growing civilization).
- The deeds of a past life remain in the world (legends, infamy, hidden secret manuals, and so on).
- **Arts carry over, sealed.** Decided by the owner on 2026-10-11 (#257): insights and learned arts carry to the new body, sealed until its realm and qi nature fit them again (§Qi and martial arts).
- **Meta progression = widening the talent distribution.** It is not a guaranteed power-up. Even a first-time player can be born a genius with luck.
- How it widens
  - **Mainly by discipline**: aptitude for the disciplines learned in a past life comes up more readily in the next life.
  - **A small widening of the upper bound**: rare ranges that open only after repeated reincarnation.
  - Raising the lower bound is not used, or is kept to a minimum.
- Countering power creep
  - The world moves on as well.
  - The amount of widening shrinks as reincarnations repeat.

## Lifespan [settled]

- Lifespan is limited.
- **Lifespan differs from character to character.** Like talent, it is decided at birth.
- Lifespan grows with one's level of martial attainment, and at the extreme **immortality is possible**.
- NPCs follow the same rules. Old masters who have lived a long time can exist in the world.

## Martial arts [draft]

- **Inner energy (naegong)**: a resource.
- **Techniques (choesik)**: skills.
- **Lightness skill (gyeonggong)**: movement.
- **Enlightenment**: replaces leveling up.
- Secret manuals are items that actually exist in the world, and they are scattered or lost when a sect is wiped out.

## Qi and martial arts [agreed]

- **Qi amount.** Every person holds a store of inner energy (naegong), which grows by cultivation: time and practice. Talent bounds how far and how fast it grows, and stays hidden (§Talent).
- **Realms (gyeongji).** Thresholds of qi make realms, on a ladder familiar from Korean wuxia: for example third-rate (samryu), second-rate (iryu), first-rate (ilryu), peak (jeoljeong), beyond peak (chojeoljeong), transformation (hwagyeong), mystery (hyeongyeong) and life-and-death (saengsagyeong). The names and thresholds are data.
- **Qi nature.** Qi has two axes:
  - Orientation: orthodox (jeong), unorthodox (sa) or demonic (ma), set by the person's heart method (simbeop).
  - Polarity: firm yang (yanggang) or soft yin (eumyu).
  - Each art has a nature. An art of another nature than one's own is harder to learn or weaker to use.
- **Skills need qi.** Each skill has a minimum qi or realm to learn and to use, and costs qi to cast.
- **Families and tiers.** Decided by the owner on 2026-10-09 (issue #194). ADR 0055 (`docs/architecture/adr/0055-give-arts-a-family-a-tier-and-a-mastery-and-gain-higher-arts-by-synthesis-over-a-lineage-graph.md`), proposed, designs them.
  - An art keeps its skills, its forms.
  - Arts gain a family, such as sword, saber, fist or palm, hidden weapons, footwork or heart method, and a tier, from a third-rate art up to a peerless one, as data.
- **Growth is the realm and a mastery per art.** The realm, the qi store, stays. Each art also has a mastery that rises with use: casting its skills, fighting and training. Forms open by mastery stage, and mastery feeds the comprehension of higher arts.
- **Higher arts by synthesis.** Knowing two or more arts that an edge of the arts' lineage graph names, each at enough mastery, gives a chance to comprehend the higher art. The graph is data, and the rule is the same for NPCs.
- **The lineage window** shows the graph's shape and edges. An unknown higher art's name and content stay hidden, as ???, until it is comprehended, or learned of by rumour or manual.
- **Three ways to gain an art:**
  1. **Comprehension (kkaedareum).** On each realm gained, a chance to comprehend a skill of an art one practises. Talent raises the chance. Not every skill unlocks this way.
  2. **Secret manuals (bigeup).** Manuals are items in the world (`docs/architecture/adr/0026-keep-sect-membership-on-the-person-and-seat-each-sect-in-a-village.md`). Reading one teaches its art when one's qi suffices.
  3. **Sect teaching (jeonsu).** A member learns its sect's arts from its master and elders, as its realm allows.
- **[proposed]** ADR 0055's changes to these ways:
  - Comprehension on a realm gained gives way to forms opening by mastery stage, and comprehension becomes synthesis.
  - A manual teaches only its art's first-stage forms.
  - Mastery is raised by training at a sect's training ground and by sect teaching.
  - Tier 3 arts start with no manual.
- **Insight is separate from arts.** Decided by the owner on 2026-10-11 (issue #257). ADR 0072 (`docs/architecture/adr/0072-gate-arts-on-insights-gained-by-manuals-nature-and-repetition-and-let-arts-grant-passive-skills.md`), proposed, designs it.
  - An insight (kkaedareum) is gained from a manual, naturally over time and conditions such as meditation, seasons or a place, or by repeating an action: sword strikes, swimming in rapids, chopping wood, leaping cliffs, or surviving a near death.
  - Learning an art has requirements: certain insights, a minimum realm, and sometimes a lower art learned first. If any is missing, learning fails, and the attempt says what was missing.
  - An art grants several skills. Some are active casts, and some are passive and always on, such as regeneration, damage by family, sight and stamina.
  - The arts' lineage is visible at a glance.
  - Decided by the owner on 2026-10-11 (#257): on rebirth, insights and learned arts and skills carry over to the new body, since knowledge belongs to the soul. The new body starts at a low realm, so a carried art is known but sealed, and shown as such, until the body meets its realm and its qi's nature again.
  - Decided by the owner on 2026-10-11 (#257): qi has natures, and some arts need a special qi, such as demonic qi. A system that manages qi's nature is needed.
  - Decided by the owner on 2026-10-11 (#257): the arts window is built from scrolls. It opens on a shelf of the scrolls one holds, and a chosen scroll opens as a vertical hanging landscape, entry arts at the foot and its peak at the top. Every person holds the humble base scroll 입문. Path scrolls, 정도, 사도 and 마도, appear when one first starts an art on that path, and insight scrolls, 자연, 수련 and 서, when their first insight is gained. Requirements from another scroll are labels, not lines. Higher scrolls, 태극 and 무극, are left for later.
- **[proposed]** ADR 0072's details:
  - up to 256 insights and 16 practices counted on each person, each insight rolled daily once its practice passes a threshold or its conditions hold
  - a failed manual reading costing its qi and 30 days before the same manual
  - teaching and synthesis rolled only for arts whose requirements hold
  - passives adding where they add, taking the strongest where they multiply
  - footwork's climbing effect carried as a footwork passive
  - qi nature as a point on two axes, righteous to demonic and yin to yang, pulled daily by the heart method, with each art requiring a range on both (the PM's model B, its options the owner's to choose)
  - a sealed art's skills and passives off, unsealed when the body's realm rank or nature changes
  - the path scrolls read from the righteous-to-demonic band
- **[proposed]** Risks such as qi deviation (juhwaipma) when forcing an art of the wrong nature or above one's realm.

## Arts that reshape the world [agreed]

Decided by the owner on 2026-10-09 (issue #203). ADR 0057 (`docs/architecture/adr/0057-resolve-world-scale-arts-as-waves-over-area-indexes-absorbed-by-materials-and-reshape-terrain-in-a-conserving-delta-layer.md`), proposed, designs it.

- **The highest arts cover vast areas.** A single move can cover an area so wide that the player zooms out to aim it. Everything inside is struck: people, buildings, natural objects and the terrain itself. The cost scales with what lies inside, never with the map.
- **Power reshapes terrain.** As a person's power grows, its arts can blast a crater, cut or level a cliff, fell a forest, or breach a wall or gate. The change is written to the world, and people, paths and sight follow it.
- **Mass is conserved.** Earth blasted away lands elsewhere, at a crater's rim, as scree at a cliff's foot, or filling water. Felled trees leave logs and breached walls leave stones, which people may gather.
- **Obstacles absorb blows by what they are made of.** A wall soaks up a strike, and those behind it are spared unless the blow breaks through, weakened. This holds for every strike, not only the greatest.
- **[proposed]** ADR 0057's details:
  - the wave spreading ring by ring from where it lands
  - scorched ground healing and forests regrowing, while reshaped earth never heals and stays until someone reshapes it again
  - the core holding elevation for every cell

## Mods and re-theming [agreed]

Decided by the owner on 2026-10-09 (issue #202). The design is ADR 0049 (`docs/architecture/adr/0049-load-the-games-content-from-data-packs-and-let-mods-be-packs-over-the-base.md`).

- **Players can mod the game.** A mod changes equipment, stats, skills, arts, factions, sects, names, structures, text, art, sound or the map.
- **Content is data in packs.** Everything the rules read and everything the screen shows is data, loaded at start. Today's game is the base pack, `wuxia`.
- **Mods are packs over the base.** A mod adds entries or changes them by a stable id. Mods load in an order the player sets, and where two change the same thing the later one wins and the game shows it.
- **Mods are data only.** A mod cannot add a rule, so mods stay safe and the world stays deterministic. A behaviour no data expresses is a request for a rule in the game.
- **A broken mod is refused whole**, with readable errors in the player's language, never half-loaded.
- **Saves know their packs.** A save loads only with the same packs that change the simulation, in the same order. A pack of text, art and sound alone, such as a translation or a re-skin, can be added or removed at any time.
- **Another theme is another base pack.** The rules name no wuxia concept; qi, realms and orthodox and demonic natures are the base pack's words for general rules. The same game can ship in another theme with another base pack.

## Gear, wealth and livelihoods [agreed]

Decided by the owner on 2026-10-09 (issue #211). The design is ADRs 0050, 0051 and 0052 (`docs/architecture/adr/0050-carry-and-wear-items-as-pack-data-in-four-slots-that-break-to-empty.md`, `0051-give-every-item-one-holder-and-scatter-a-destroyed-warehouse-on-the-ground.md`, `0052-run-the-economy-from-households-needs-through-town-markets-taxed-by-the-power-that-holds-the-land.md`).

- **Everyone carries and wears gear.** A weapon, armour, headgear and an accessory, from many kinds, so a crowd does not look alike. A faction shows in its gear's dye.
  - The weapon decides how one strikes. A spear reaches farther than a sword, and darts fly.
  - A martial art needs its weapon: a sword art a sword, a fist art bare hands or gauntlets.
- **Gear wears out and breaks.** A broken weapon or armour leaves the slot empty, and nothing is given for free. One fights bare-handed or unarmoured until one equips something one owns.
  - People carry spares where they can, and take up better gear they find, buy or are issued. What a soldier at war loots goes to its faction, which may issue it to its soldiers or sell it.
- **Gear is made from materials, and broken gear can be recycled.** Decided by the owner on 2026-10-10 (issue #231). ADR 0064 (`docs/architecture/adr/0064-make-gear-from-materials-by-recipe-and-reforge-broken-metal-at-a-loss.md`), proposed, designs it.
  - Weapons and armour take resources to make: iron, steel, leather, wood and cloth.
  - A broken piece can be recycled, with materials conserved.
  - Decided by the owner on 2026-10-10 (#231's comment): animal and spirit-beast parts are crafting materials. Hide, bone, horn and claw come from every animal, and rarer parts from spirit beasts (#171).
- **[proposed]** ADR 0064's details, after the PM's direction:
  - metals recycled but not for free: a smith reforges scrap with charcoal and labour, and half the metal is lost as slag
  - organic materials, leather, wood and cloth, lost when a piece breaks
  - scrap as loot, so a faction short of iron arms itself from its battlefields
  - recipes of materials and labour as pack data, with ore from mines on the map
  - workshops making what their market lacks most, and treasuries ordering gear
- **Every item has one owner:** a person, a warehouse, a faction or the ground. There are four ways to move one: pick it up, deposit or withdraw it, buy it and sell it.
  - The dead drop what they carried, for anyone to pick up.
- **Warehouses.** Villages keep warehouses run by keepers, sects keep vaults, and factions keep treasuries. A destroyed warehouse scatters its contents on the ground, and whoever picks an item up owns it. A soldier at war picks it up as loot for its faction, which sells it.
- **Caves are the player's alone.** Only the player finds and enters caves. A cave is hidden until the player searches near it or steps on it, and its cache is the player's. Some caves hold treasure. The player's soul keeps its warehouses and caches through rebirth.
- **People earn to eat and to keep their families.** The household is the unit, with a purse. Earners feed dependants, buy their rations, keep their house and their tools, and buy gear as their roles need.
  - Failing to earn brings, in turn, a debt to kin, selling one's gear, then hunger, then leaving for a richer town. People borrow and sell before they starve.
- **Markets from needs.** Each town has a market whose prices move with stock against demand. Merchant houses send caravans to buy where it is cheap and sell where it is dear.
- **Taxes and treasuries.** The power holding the land taxes trade, households and caravans at its gates, and sects take dues. Treasuries pay for wars. A faction that cannot pay loses soldiers and seeks peace sooner, and a rich one can arm and march more.
- **The tax rate follows the treasury's needs.** A leader raises it when war or upkeep demands and lowers it on a surplus. A player who leads sets it by hand.
- **Discontent drives flight.** High tax, hunger and kin lost in war make households discontent. Past a point they flee to another faction's land they have heard of, the richer, safer and lower-taxed the better, which keeps rates in check.
- **Not now:** loans, interest, banks, exchange rates and haggling.

## MVP scope [proposed]

- **Scale**: one continuous hand-made map (§World and scale), three factions (Orthodox, Demonic, Unorthodox), and the cities and towns the map places.
- **Flow**: a new world on the map → travel and combat → one loop of a village being destroyed and rebuilt → succession by duel in the Demonic Cult → death and rebirth.

## Open questions [open]

The items below are not yet decided.

- What happens after immortality is reached (for example, an ending in which ascension (deungseon) ends the cycle of reincarnation, versus remaining in the world).
- Whether to include factors that shorten lifespan (side effects of demonic martial arts, qi deviation, poison, and so on).
- Whether to include the martial clan and government factions.
- How a sect's seat relates to a village: a compound within one, or a settlement of its own.
- How sect membership is gained and lost, per faction.
- What caps a sect's size.
- The realm names and thresholds, which are data.
- How much an art of a mismatched nature costs: how much harder to learn, how much weaker to use.
- Whether a lost manual can be copied.
- **Realistic scale (#227), for the owner.** Each has options and a recommendation:
  - **What a cell is at a person's scale.** (a) About 2 metres, a person's height and reach, which the reference sizes assume; (b) about 1.5 metres. Recommended: (a), so a 4 x 3 house is about 8 x 6 m, a rural house of three bays.
  - **Great rivers.** A real Yellow River or Yangtze is hundreds of cells wide. (a) 16 to 40 cells, crossed by ferries and fords; (b) the real width. Recommended: (a), so the river still reads on one screen.
  - **Today's house and hall.** The drawn house is 3 x 3 and the hall 5 x 3, against the reference 4 x 3 and 6 x 4. (a) Grow them in the next structure art round; (b) keep them as stated exceptions. Recommended: (a).
  - **When a village becomes a town.** (a) Only when its growth reaches the walled town's size, as #169's comment says; (b) by population alone. Recommended: (a), which #169 designs.
  - **Climbing walls.** A town's wall of 2 levels is a drop every realm can climb under ADR 0058, proposed in #223. (a) Walls are structures, never climbed, entered at gates or breached; (b) walls climbed as cliffs of their height. Recommended: (a), so a wall still holds a siege.
