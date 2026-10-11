# ADR 0050: Carry and wear items as pack data, in four slots that break to empty

> Status: proposed; its item at no durability gone superseded by ADR-0064, under which a broken item becomes scrap of its main metal, one unit for each unit in its recipe
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner said on 2026-10-09, in issue #211, that people look too alike and that the world should give real-life freedom. Everyone carries and wears gear of many kinds, and a faction shows in its gear's dye. People take up better gear they find, and gear wears out. The issue's comments of the same day are binding and replace parts of its body:
- **A broken weapon or armour leaves the slot empty.** Nothing is given for free, so the person fights bare-handed or unarmoured until it equips something it owns. This replaces the body's "breaking back to a basic piece".
- **People carry spares where they can,** within their carry limit, and swap one in when a piece breaks.
- **The design states:**
  - the bare-handed strike, its shape and reach under ADR 0046, and how fist arts use it
  - how many spares a person aims to carry, by role and wealth, as a need that drives buying
  - what an empty slot does to combat and to looks
  - whether the demand for spares, repairs and replacements helps keep the money supply bounded
- **PM direction.** Every item has one holder: a person, a warehouse, a faction or the ground. Four verbs move items between holders: pick up, deposit or withdraw, buy and sell. Needs are deterministic thresholds, not planning.

Decided on PR #215's review, 2026-10-09:
- **By the owner.** The faction dye is the wearer's faction now. An art needs its discipline's weapon family: a sword art a sword, and a fist art bare hands or gauntlets.
- **By the PM, on this ADR's recommendation,** with the owner told. Darts are used up: each throw spends one.

This ADR is the first of three that #211 splits into. ADR 0051 holds inventories, warehouses and looting. ADR 0052 holds markets, households, taxes, treasuries and discontent. All three use ADR 0049's pack format, so every item is data and the core names none.

Facts from `main` at `5047f29`:
- **Strikes.** An army's skills are faction data in `core/region.cpp`:
  - The guard strike sweeps an arc at `MELEE`, a cell and a half, for 10 damage every 15 ticks. Every faction has it.
  - The Orthodox and Unorthodox mend, and the Cult torches.
  - Each art's first skill strikes for 20, blades in an arc at `MELEE` and darts along an 8-cell line.
  - Every person has 120 health.
- **Disciplines.** There are five, sword, saber, fist, hidden weapons and medicine. The three arts practise sword, saber and hidden weapons; no art practises fist.
- **The smithy.** Under ADR 0029, a smithy arms 40 people of its village, who strike a quarter harder. That edge stands in for weapons the world did not have.
- **Wares.** ADR 0040's herbs and elixirs are wares held as counts on the person.
- **The bound.** ADR 0048 measures the population bound between 10,000 and 50,000, so this ADR sizes everything at 50,000.

Options weighed:

- **Slots.** Four: weapon, armour, head and accessory. Chosen. The weapon decides the strike, the armour and the head guard against damage, and all three are drawn, so a crowd differs in silhouette and colour. The accessory carries a small effect and a look.
  - Rejected: separate slots for hands, feet, legs and a second weapon. Each adds a layer of art for every body and a choice to the gear rule, for little difference on a 16-pixel body.
- **What an item is.** An item kind is an entry of the pack's `items` document, with fields such as these:
  - its family: a weapon family, armour, head, accessory, tool, herb, elixir, food, material or valuable
  - its tier and material, which set its look and its worth
  - its durability, or a stack size for kinds that stack
  - its base price
  - its effects

  An item a person holds is a stack: its kind, and either its durability left or its count. Chosen. The pack defines each kind outright, such as an iron sword or a steel sword. Tier and material label the kind; no formula derives one from the other, so a pack can make any combination.
  - Rejected: unique items with their own ids. Every item would need a record of its own, and no rule yet needs to tell two iron swords apart.
- **Weapons and the strike.** A weapon family gives the person's basic strike: a skill as ADR 0046 shapes it, with a shape, range, radius, damage and cooldown. A weapon kind may change that damage and adds its `edge` per mille, which multiplies the body damage of art skills used with it. The guard strike, an army skill today, becomes the weapon's basic strike. Mending and torching stay army skills. Chosen. A spear reaches past arm's length and a dart flies far, so gear changes how a fight is shaped and not only by how much.

  The base pack's starting values:

  | Family | Shape | Range | Damage | Cooldown (ticks) | Art discipline |
  |---|---|---|---|---|---|
  | Bare hands | arc | `MELEE`, 1.5 cells | 6 | 15 | fist |
  | Fist (gauntlets) | arc | 1.5 cells | 8 | 12 | fist |
  | Sword | arc | 1.5 cells | 10 | 15 | sword |
  | Saber | arc | 1.5 cells | 12 | 18 | saber |
  | Spear | line, half a cell either side | 2.5 cells | 10 | 18 | none yet |
  | Staff | arc | 2 cells | 8 | 15 | none yet |
  | Fan | arc | 1.5 cells | 8 | 12 | none yet |
  | Hidden weapons | line, half a cell either side | 8 cells | 6 | 30 | hidden weapons |

  - Darts are a stack. Every throw, the basic strike or a hidden-weapons art skill, spends one dart, as the PM decided. A thrower with none left strikes bare-handed until it holds darts again.
  - Iron is the base material. A steel kind deals a fifth more and a fine-steel kind two fifths more, each with the edge to match. The implementing pull request measures fights at these values against ADR 0046's targets.
  - Rejected: weapons as a multiplier on today's guard strike. Every weapon would then strike in the same shape at the same reach.
- **Arts and weapon families.** Each discipline names the weapon family its arts need. An art skill can be used only while the weapon slot holds that family:
  - a sword art needs a sword, and a saber art a saber
  - a fist art needs gauntlets or an empty weapon slot
  - a hidden-weapons art needs darts in the weapon slot
  - medicine needs none

  A cast refused for this names a new `CastRefusal`, `WRONG_WEAPON`. The automatic strike draws only among the skills the weapon allows. Decided by the owner, so gear matters to a martial artist and to a merchant selling to one.
  - Rejected: arts usable with any weapon. A swordsman's sword would then add damage and nothing else.
- **The bare-handed strike.** It is the empty weapon slot's strike: an arc at arm's length, the same shape and reach as the sword's, at 6 damage instead of 10. It costs nothing and never wears. Fist arts strike bare-handed at an edge of 1,000, and with gauntlets at the gauntlets' edge, 1,200 for iron. So an unarmed person still holds its ground under ADR 0046's engagement, at three fifths of a sword's damage, and a fist artist loses nothing by losing a weapon.
- **Armour.** Armour and headgear each give a `guard` per mille. Body damage taken is multiplied by (1,000 − the sum of the guards) / 1,000. The guard never passes 600, so a strike always lands. The base pack's starting guards:
  - cloth 50, leather 150 and lamellar 250 for armour
  - a headband 0, a hat 25 and a helmet 75 for the head

  The headband and the hat are there for their looks.
- **Accessories.** An accessory gives energy recovery per mille, or nothing. A jade pendant or a silver hairpin is a valuable worn for its look and its price, and ADR 0052 makes valuables the world's sink for spare silver.
- **Tools.** A tool, such as a hoe, an axe or a hammer, is carried in the inventory, not in a slot. A person working at the trade the tool serves produces half without one (ADR 0052). A tool loses one durability a working day. Chosen, because the owner named broken tools among the needs that drive buying.
- **Durability.**
  - A weapon loses one durability each time its strike or an art skill used with it hits anyone or anything.
  - Armour and headgear each lose one each time their wearer takes body damage.
  - A tool loses one each working day.
  - Accessories never wear.
  - Nothing wears with time alone, so a stored item keeps its durability and no daily pass is needed over every item.
  - Starting values: an iron sword lasts 600 hits, about ten skirmishes at ADR 0046's mean length, and lamellar armour 400 hits.
- **Breaking.** An item at no durability is gone. A broken weapon or armour leaves its slot empty, as the owner decided. The gear rule then fills the slot from the person's own inventory if it holds a fitting piece. Otherwise the slot stays empty until the person buys, picks up or withdraws one.
- **Repair.** Repair is a sale. A worn item sold to a market returns to its stock as the durability it has left. A smithy in that market's village restores durability from materials, and the market sells whole items (ADR 0052). To the person this is repair, at the price of the wear and the market's margin, and the rules keep four verbs.
  - Rejected: a fifth verb for repair, kept by a smith for a fee. It adds a second way of pricing the same wear beside the market's.
- **The gear rule.** Each item kind has a `worth` per slot, an integer from the pack. A person ranks the weapons it holds by three keys in turn:
  1. the family its first art needs, then the families its other arts need
  2. worth
  3. durability left, the lower index among equals

  It ranks armour, head and accessories by worth, then durability. In each slot it wears the best piece it holds, in the slot or in its inventory. The rule runs only when what the person holds changes: on a pick-up, a purchase, a withdrawal, a break or a draw at birth. So its cost scales with those events, not with the population. A steered body is never re-equipped by the rule. The player equips by hand.
  - Rejected: comparing gear on a schedule for everyone. At 50,000 people that is work every day when nothing has changed.
- **Spares.** A spare is a second piece for a slot, carried in the inventory. A person aims to carry the spares its role sets, as pack data:

  | Role | Weapon spares | Armour spares | Darts, for a hidden-weapons art |
  |---|---|---|---|
  | Child, under coming of age | 0 | 0 | 0 |
  | Villager | 0 | 0 | 0 |
  | Sect member | 1 | 0 | 20 |
  | Soldier, serving in an army other than its village's residents | 1 | 1 | 30 |
  | Caravan guard | 1 | 0 | 20 |

  - A household whose purse is above its comfortable reserve (ADR 0052) raises each adult's target by one weapon spare.
  - An empty slot, or fewer spares than the target, is a need. ADR 0052's buying rule meets it within the household's budget.
  - Spares wear only when worn, so the demand they create is for replacement as pieces break.
  - On the money supply: wear destroys goods, not silver. Its effect is that silver keeps moving from households to markets and smiths. The faucets and sinks of silver, and the bound on them, are ADR 0052's.
- **Starting gear.** The pack's `loadouts` document gives weighted choices for each slot, by faction, sect and role. Each person draws its own by the seed when the world begins. A person coming of age draws nothing; it buys by the need rule. A loadout lists several kinds and looks for each slot, so two villagers of one village wear different hats and clothes. The base pack's starting loadouts:
  - villagers wear cloth and a headband or a hat, with a hoe
  - sect members carry their art's weapon, in leather
  - the founders of sects carry steel
  - soldiers drawn at the start carry a sword or saber, by faction, in lamellar
  - **Rejected: a basic piece issued free.** The owner decided against it.
- **An empty slot's looks.** The view draws a body in plain clothes, of its sex. An empty weapon slot shows empty hands, and an empty head slot shows the body's hair. Every item kind's sprite is named in the pack's `view/` (ADR 0049), outside the hashed data, and the view draws the layers in order: body, armour, head, weapon.
- **Faction dye.** Each armour and head sprite has a dye mask: pixels the view tints with a faction's colour, both from the packs' `view/` documents (ADR 0049), so a recoloured or redrawn item changes no simulation data. The tint is the wearer's faction now, read from the army it serves in. A person who changes faction, or wears looted gear, shows its own faction at once. Decided by the owner. The core stores nothing for it, and a battle reads friend from foe by colour.
  - Rejected: the dye as the maker's faction, stored on each item. Looted gear would show its old colours, which reads well in a story but misleads in a fight, and it costs a byte for every stack.
- **Memory.** A stack is 8 bytes: a kind of 16 bits, durability or count of 16 bits, and flags. A person holds 4 slots and 8 inventory stacks (ADR 0051), 96 bytes. At 50,000 people that is 4.8 MB. A pack may define up to 65,535 item kinds. The dead hold nothing: what they carried drops where they die (ADR 0051).
- **Cost per tick.**
  - Wear is a decrement inside the strike and damage loops, which already visit every hit.
  - The basic strike reads the weapon slot in place of the army's first skill.
  - The gear rule runs per change, and touches at most 12 stacks. It runs at most once a person a tick, after every change to what that person holds that tick: breaks, purchases, withdrawals and pick-ups. Breaks are at most the tick's hits, about 30 in the M1 battle. Pick-ups reach at most the people ADR 0051's scan visits, about 1,700 a tick at worst. Purchases and withdrawals are daily rules spread over the day. So the worst case is about 1,750 runs, or 21,000 stack comparisons, a tick. That is a few hundredths of a millisecond, under 1% of ADR 0048's 4 ms p99, by this estimate; the implementing pull request measures it.
- **Determinism, saves and the hash.** All of it is integers. Equipment and inventories are saved and hashed, and `SAVE_VERSION` rises.
- **Wares.** ADR 0040's herbs and elixirs become item kinds that stack, held in the inventory and used by the consume order as before. A person's wares are now stacks, not counts.

## Decision

Items are pack data: kinds with a family, tier, material, worth, durability or stack size, price and effects. The core names none. A person wears four slots, weapon, armour, head and accessory, and carries spares in its inventory. A weapon family gives the basic strike, its shape, reach and damage, and an edge for art skills. An art skill needs its discipline's weapon family, and an empty weapon slot strikes bare-handed: an arc at arm's length for 6, which fist arts use at their own edge. Armour and headgear guard against body damage, and are capped so a strike always lands. Weapons wear by hits made, armour by hits taken, tools by days worked, and nothing by time. A broken piece leaves its slot empty until the person equips one it owns. One rule ranks and wears the best piece held, run only when what a person holds changes. Spares by role and wealth are needs that drive buying. Starting gear is drawn from weighted loadouts. The view tints armour and headgear with the wearer's faction.

This supersedes ADR 0046's guard strike as an army skill, which becomes the weapon's basic strike; ADR 0029's edge, since a smithy no longer raises its village's strikes and instead makes and restores gear for the market; ADR 0040's wares as counts, which become stacks; and ADR 0035's draw among all of a person's skills whatever it holds, since an art skill now needs its discipline's weapon family.

## Consequences

- Combat balance moves to item data. Fights become longer bare-handed and shorter with steel, and ADR 0046's targets are measured again in the implementing pull request.
- The M1 scenario gives each side its loadout, so `make bench` times weapons and wear.
- People have a reason to spend. A soldier's spares and breaks are the steady demand behind ADR 0052's markets.
- A spear's reach of 2.5 cells holds a mover farther off under ADR 0037's hold, since reach is the longest range of a person's skills.
- A save holds every person's slots and inventory, about 96 bytes each. The pinned hashes change.
- The view needs a sprite and a dye mask for every wearable kind. Art for the base pack's starting kinds is part of the work.

## Signs the decision was wrong

- Bare-handed people survive so badly that a village whose smithy falls loses every fight after it, which reads as a death spiral rather than hardship.
- The arts' need for a weapon family leaves most sect members unable to use their arts most of the time, because spares run out faster than markets restock.
- Players cannot tell gear apart on a 16-pixel body, so the four slots add art cost without adding variety.
- The gear rule's events, picks and breaks, cost a battle's tick more than its strikes do.
