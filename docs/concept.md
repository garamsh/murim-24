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
- **Genre and engine**: 2D RPG, Godot 4.7.
- **Perspective and combat**: a three-quarter top-down view, with combat in real time (`docs/architecture/adr/0012-present-the-world-in-a-three-quarter-top-down-view.md`).
- **Art direction**: pixel art on a 16-pixel tile grid (`docs/art-direction.md`).
- **Language**: UI text in Korean first, with English supported.
- **Title**: "Murim24" is a working title for development. The 24 has no meaning, and the official title is decided after the MVP.

## World and scale [agreed]

Decided by the user on 2026-10-08 (issue #173), and revised by the owner on 2026-10-09 (issue #201): one hand-made map, physical distances, an offline game. The design is ADR 0047 (`docs/architecture/adr/0047-author-one-continuous-map-of-4096-cells-by-hand-at-a-li-a-cell.md`), with the quality profiles of ADR 0048.

- **One continuous map.** The world is one map of 4,096 x 4,096 cells, with no transitions between regional maps. The regions are named lands on it, and travel between them crosses the land itself: ranges and rivers block the way, and passes, fords and ferries are places one walks through.
- **Made by hand, the same in every game.** The map is authored, not generated, and every player gets the same one. No seed shapes the terrain; a new game's seed draws only the simulation's randomness: who is born with what talent, and what happens.
- **Physical distance.** In the heartland a cell is a li, 500 metres between places, so travel takes the time the real distance suggests. At 1x, Luoyang to Chang'an is about 6 minutes on foot, a region about 12, and edge to edge about 36. The outer lands are compressed toward the map's edge. Roads are faster than open ground; horses and river boats are planned.
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
- **The same world on every device.** A quality profile, chosen by the device or the player, sets only how much is drawn and how far around the player people are simulated in full. The map, its villages and its people are the same on every profile. Far events may turn out differently on different profiles, which is accepted for an offline game.
- **The population bound is measured.** The map needs far more people than the first region did to feel lived in. The bound is raised from about 500 to the largest number between 10,000 and 50,000 that keeps a tick's p99 within its 4 ms budget with the level of detail, measured on the lowest quality profile. It is measured on PC, macOS first and Linux when available.

## Factions and the rules for gaining power [agreed]

| Faction | Values | How to become its leader | Joining requirement |
|---|---|---|---|
| Demonic Cult (Magyo) | The strong prey on the weak | Challenge the Cult Leader and win the duel. The Cult Leader's allied factions become enemies | Only a member of the Cult may challenge |
| Orthodox faction (Jeongpa; the Murim Alliance, Murimmaeng) | Righteousness and chivalry | Win an election by gathering the favor of sect elders. Cannot be taken by force | Enter a sect; infamy below a set threshold |
| Unorthodox faction (Sapa) | Profit | Money, bribery, assassination, and backroom deals | Money, or proof of usefulness |

- **[candidate]** The martial clans (Sega; succession by bloodline and marriage) and the government (Gwanbu) are listed as candidates only. They are not in the MVP and are not decided.

## Sects [agreed]

- **A village and a sect are separate.**
  - A **village** is where people live. Its population cap comes from its housing, food and storage.
  - A **sect** (munpa, for example Hwasan or Wudang) is an organization within a faction, with members, elders, martial arts and a seat (bonsan).
- **Members may live in several villages.** Membership does not follow from where one lives.
- **Each can fall without the other.** A sect can be wiped out while its villages stand, and a village can burn while its sect survives.
- **No fixed population per village.** The economy sets each village's cap, under the world's population bound (§Growing civilization).

## Reputation and rumors [agreed]

- Favor has two layers.
  - Personal favor with a village or an individual NPC
  - Reputation with a faction
- Actions become rumors and spread with a time delay.
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
- **Population caps.** Each village's population is capped by its own housing, food and storage (`docs/architecture/adr/0022-cap-each-village-by-its-housing-food-and-storage.md`):
  - A village's cap is the smallest of its housing capacity, its food supply divided by the food one person consumes, and its storage capacity, derived from its structures, with what each kind holds given as data.
  - Growth slows as the population nears the cap.
  - A shortage brings starvation or migration.
- **Construction is the NPCs', never the player's.** As a village grows, its NPCs decide what to build and build it: houses, fields, granaries, and later workshops such as a smithy. The player never places or orders a building. Growth lays structures and residents raise them by the repair rule (`docs/architecture/adr/0023-let-villages-build-found-and-raid-as-they-press-their-caps.md`).
- **A hard upper bound.** A world's population never exceeds a bound. That bound is a ceiling, not a target: the caps below it set the actual population. It is about 500 until it is measured for the larger world (§World and scale).
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
- **Three ways to gain an art:**
  1. **Comprehension (kkaedareum).** On each realm gained, a chance to comprehend a skill of an art one practises. Talent raises the chance. Not every skill unlocks this way.
  2. **Secret manuals (bigeup).** Manuals are items in the world (`docs/architecture/adr/0026-keep-sect-membership-on-the-person-and-seat-each-sect-in-a-village.md`). Reading one teaches its art when one's qi suffices.
  3. **Sect teaching (jeonsu).** A member learns its sect's arts from its master and elders, as its realm allows.
- **[proposed]** Risks such as qi deviation (juhwaipma) when forcing an art of the wrong nature or above one's realm.

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
