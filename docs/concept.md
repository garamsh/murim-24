# Game Concept

This document is the single reference for the game concept agreed with the user.

## Contents
- Status tags
- Identity [agreed]
- Factions and the rules for gaining power [agreed]
- Sects [agreed]
- Reputation and rumors [agreed]
- Village life cycle [agreed]
- Growing civilization [agreed]
- Talent [settled]
- Reincarnation [settled]
- Lifespan [settled]
- Martial arts [draft]
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
- **Principle: the player is also one of the world's NPCs.** There are no player-only rules; the same rules as for NPCs apply (talent, lifespan, and so on).
- **Genre and engine**: 2D RPG, Godot 4.7.
- **Perspective and combat**: a three-quarter top-down view, with combat in real time (`docs/architecture/adr/0012-present-the-world-in-a-three-quarter-top-down-view.md`).
- **Art direction**: pixel art on a 16-pixel tile grid (`docs/art-direction.md`).
- **Language**: UI text in Korean first, with English supported.
- **Title**: "Murim24" is a working title for development. The 24 has no meaning, and the official title is decided after the MVP.

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
- **No fixed population per village.** The economy sets each village's cap, and the world's bound of about 500 NPCs (§Growing civilization) stays.

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
- **Population caps.** Each faction's population is capped by its housing, food and storage.
- **Construction is the NPCs', never the player's.** As a village grows, its NPCs decide what to build and build it: houses, fields, granaries, and later workshops such as a smithy. The player never places or orders a building. Growth lays structures and residents raise them by the repair rule (`docs/architecture/adr/0023-let-villages-build-found-and-raid-as-they-press-their-caps.md`).
- **A hard upper bound.** A world's population never exceeds about 500 NPCs. That bound is a ceiling, not a target: the caps below it set the actual population.
- **[proposed]** The cap formula:
  - A faction's cap is the smallest of its housing capacity, its food supply divided by the food one person consumes, and its storage capacity, each given as data.
  - Growth slows as the population nears the cap.
  - A shortage brings starvation or migration.
  - Expansion (building, new villages, territory, raids) is the motive for war.
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

## MVP scope [proposed]

- **Scale**: one region, three factions (Orthodox, Demonic, Unorthodox), six to eight villages.
- **Flow**: world generation → travel and combat → one loop of a village being destroyed and rebuilt → succession by duel in the Demonic Cult → death and rebirth.

## Open questions [open]

The items below are not yet decided.

- What happens after immortality is reached (for example, an ending in which ascension (deungseon) ends the cycle of reincarnation, versus remaining in the world).
- Whether to include factors that shorten lifespan (side effects of demonic martial arts, qi deviation, poison, and so on).
- Whether to include the martial clan and government factions.
- Distance-based simulation LOD: to be decided in an ADR together with the simulation core.
- How a sect's seat relates to a village: a compound within one, or a settlement of its own.
- How sect membership is gained and lost, per faction.
- What caps a sect's size.
