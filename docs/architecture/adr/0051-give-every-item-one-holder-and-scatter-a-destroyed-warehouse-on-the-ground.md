# ADR 0051: Give every item one holder, and scatter a destroyed warehouse on the ground

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #211 asks for inventories, what drops on death, warehouses with keepers, sect vaults, hidden caches, looting by people and the player, plunder in war, and warehouses that survive the player's rebirth. The owner's decisions of 2026-10-09 in its comments are binding:
- **Caves are the player's alone.** Hidden caches in caves are the player's. NPCs neither enter nor loot caves.
- **A destroyed warehouse scatters its contents on the ground.** Whoever picks an item up owns it:
  - a person picks it up as its own and may sell it
  - a soldier at war picks it up as loot, which goes to its faction's warehouse, and the faction sells it for income
- **PM direction.** Every item has one holder: a person, a warehouse, a faction or the ground. Four verbs move items between holders: pick up, deposit or withdraw, buy and sell. Loot is a soldier at war picking up, routed to its faction's warehouse.

Decided on PR #215's review, 2026-10-09:
- **By the owner.** Nothing marks a thief now. Witnesses will, once #175's reputation lands. The map places treasure in some caves.
- **By the PM, on this ADR's recommendation,** with the owner told. An heirless household's estate goes to the faction owning its village.

ADR 0050 defines items as stacks of a kind, and the four slots. ADR 0052 defines households, markets and treasuries. This ADR sits between them.

Facts from `main` at `5047f29`:
- A person who dies keeps its record and leaves its roster, and "the dead keep" their silver (ADR 0040).
- A village takes damage only from factions hostile to its owner while it has a living resident (ADR 0015). Its owner changes when an army restores a structure of a village with no living resident.
- A sect that falls scatters its manuals within a reach of its seat, or loses them (ADR 0026).
- Only a person with a soul is born again, and the player's body has one (ADR 0019). Steered armies are marked, saved and hashed (ADR 0038).
- ADR 0042's tiers run per-tick rules in live and stepped areas, and far areas move armies on the coarse graph and resolve fights by expected damage. Daily rules are untiered.

Options weighed:

- **Holders.** An item is held by exactly one of these:
  - a person: its slots and inventory
  - a warehouse, in a lot owned by an account
  - the ground: a pile on a cell

  An account is a household, a soul, a sect, a faction or a merchant house. A faction's holdings are its account's lots in its warehouse, as the owner said its treasury is. Chosen. Every move between holders is one of the four verbs, and each verb moves a stack whole or in part, so no item is ever in two places. Equipping and consuming are uses, not moves.
- **Silver.** Silver is held as a count: on a person, in a household's purse (ADR 0052), in an account's balance at a warehouse, or on the ground as a pile of the silver kind. On the ground it is a stack like any other, so the pick-up rule and its expiry treat it alike.
- **The inventory.** A person carries up to `carry` stacks, 8 in the base pack and at most 16 in the core, beside its four slots. A stack of a stacking kind holds up to its kind's stack size. Chosen. Slots are cheap to check, easy to show and need no weights.
  - Rejected: a carry weight. Every kind would need a weight, and every pick-up a sum, for a limit players read less easily than a row of boxes.
- **Death.** Everything the dead carried drops on the cell where it died: its slots, its inventory and its pocket silver. Anyone may pick it up. This supersedes ADR 0040's rule that the dead keep their silver, which held silver no one could reach.
- **The ground.**
  - A pile is one stack on a cell, with the tick it expires. Piles are kept by 32-cell area.
  - A pile expires 30 days after it lands, in the base pack. A pile of silver that expires leaves the world, one of ADR 0052's sinks.
  - The world holds at most 131,072 piles, 16 bytes each, 2 MiB. When it is full, the oldest pile goes first.
  - Rejected: piles that never expire. A long war would fill the world's memory with swords no one will walk back for.
- **Picking up.**
  - Once a second, in live and stepped areas, each pile is taken by the nearest living person within a cell of it who has room and is not steered, the lower index among equals.
  - A person wants a pile, in order, when it is silver, when the gear rule would wear it or it meets a spare it lacks (ADR 0050), when it is food while the person is hungry, when it is a valuable, and otherwise when it has a price. A person with no room for a wanted pile leaves it.
  - The player picks up through the port, by an order naming a pile within a cell, refused with its reason as data, as `buy` is.
  - Rejected: picking up only what a person needs. Piles nobody needs would wait out their days, and the scrap of a battle would never reach a market.
- **Loot.**
  - A soldier at war is a person serving in an army that is not a village's residents and not a caravan, while its faction is hostile to the faction of the person whose drop it is, or of the warehouse's village, as `hostile` reads it (ADR 0038).
  - What a soldier at war picks up is marked as loot, a flag on the stack. Loot is carried and cannot be worn or sold.
  - When the soldier's army disbands at home, or the soldier stands within a cell of a warehouse holding its faction's account, the loot is deposited to its faction's account there.
  - A soldier who dies drops its loot as any stack, unmarked, so loot can be lost and taken back.
  - A faction with no warehouse leaves the loot with the soldier, unmarked, when its army disbands.
  - The faction sells it by ADR 0052's treasury rule.
  - Rejected: loot sent to the faction's warehouse at the moment it is picked up. Nothing would be carried home, so ambushing a raid on its way back would gain nothing.
- **Warehouses.** A warehouse is a structure whose kind's holding gives `lots`, the stacks it stores, and `accounts`, the owners it keeps. The base pack's kinds:

  | Kind | Where | Lots | Accounts | Fee a stack deposited | Who withdraws |
  |---|---|---|---|---|---|
  | Warehouse | a village or town, run by a keeper | 256 | 64 | 1 silver, to the keeper's household | each account's owner |
  | Vault | a sect's seat | 128 | 4 | none | the sect's master and elders |
  | Treasury | a faction's capital | 512 | 8 | none | the faction's leader, and soldiers drawing issue |
  | Counting house | a merchant house's home town | 256 | 4 | none | the house's merchants |

  - The pack's map names each faction's capital (ADR 0047). A faction whose capital has no standing treasury keeps its account at its first village, by index, with a warehouse, holding only what reaches it from then on.
  - Growth may build a warehouse in a village by the scarcity rule (ADR 0023), with its own `lots` as a holding.
  - Rejected: one shared store per village with no accounts. Every deposit would belong to everyone, so no one could own anything stored.
- **Who owns a deposit.** A person deposits into its household's account, or into its soul's account if it has a soul. Any adult of the household may withdraw from the household's account; only a body of the soul may withdraw from the soul's. A sect member deposits into its sect's account only by an order its master or an elder gives. The faction's account is filled by loot, taxes and purchases (ADR 0052). A soldier of the faction may withdraw issue: gear of the kinds its spares and slots need, up to them.
- **The keeper.** A village warehouse's keeper is the first resident whose trade is keeping (#170), and until trades exist, the first resident of its village's first household. Fees go to the keeper's household. A keeper who dies is replaced by the same rule the next day, and the lots are untouched. The keeper never withdraws another account's lots.
- **An owner's death.** A household's account passes to the household that takes in its last member (ADR 0052). With none, it passes to the faction owning the village, the way an estate with no heir goes to the crown, as the PM decided. A soul's account outlives every body. A sect that falls passes its vault's accounts to the faction owning the seat, as its manuals scatter.
- **Destroying a warehouse.** A warehouse whose structure reaches the destroyed stage scatters every lot and every balance onto the ground. Each stack becomes a pile on a passable cell drawn by the seed within 2 cells of its footprint, and the accounts are emptied. Then the pick-up rule applies: a person takes a pile as its own, and a soldier at war as loot. Only people whose faction is hostile to the village's owner can damage it while it has a living resident (ADR 0015), so robbing a warehouse is an act of war or of an enemy.
  - Rejected: opening a standing warehouse by force, as a fifth verb. The owner decided that destruction scatters, and that rule already covers it.
  - Nothing marks anyone a thief for picking up from a warehouse they helped destroy, as the owner decided. Once #175's reputation lands, witnesses within sight of such a pick-up mark the picker a thief in it.
- **War taking a village.** When a village's owner changes and its warehouse stands, household and soul accounts stay. The old faction's and sect's accounts there pass to the new owner, as spoils. A raid that destroys the warehouse scatters it instead.
- **Caves.** A cave is a named place in the map's data, an entrance cell with a cache of 32 lots and 1 account, its soul's.
  - Only a steered body standing at the entrance opens it, through the warehouse window. A cave blocks nothing and draws no one, and no rule for people no driver steers reads caves. That is the owner's decision. It is the one rule that tells the player from an NPC, an exception to the concept's principle that the same rules apply to both, recorded in `concept.md`.
  - A cache cannot be destroyed, and only its soul's bodies withdraw from it.
  - The map places treasure in some caves, as the owner decided: a cache's starting lots, owned by no one until the player withdraws them, after which the rest of the cache is the soul's.
- **Rebirth.** The player's soul owns its account at every warehouse it deposited into, and its caches. A new body of the soul withdraws from them by walking there; the keeper knows the soul as a matter of the world's rules, not of the body. The soul's holdings are listed in the inventory window, by warehouse and place, so the player knows where to go.
  - Rejected: a token item that unlocks the account. A token dropped at death would lose the account, and the owner asked that warehouses survive rebirth.
  - Rejected: a password the player types. It is the same rule with a step the world never checks.
- **Tiers.**
  - Live and stepped areas pick up per tick, stepped areas on their stride (ADR 0042).
  - In a far area, a fight's dead drop as anywhere. The side that holds the field then takes the piles in aggregate: each stack goes to its members with room in roster order, as loot where they are soldiers at war, and the rest wait out their days.
  - Deposits, withdrawals, fees, keepers and estates are daily rules, run for every village whatever its tier.
- **Memory at 50,000 people.**
  - A lot is 12 bytes: its stack and its account. A balance is 12 bytes: an account and a 64-bit count.
  - About 1,000 warehouses averaging 256 lots and 32 balances come to about 3.4 MB.
  - Piles are at most 2 MiB.
  - Persons' slots and inventories are ADR 0050's 4.8 MB.
  - Caves are the map's, and few.
- **Saves, the hash and determinism.** Lots, balances, piles and their expiries, keepers and the loot flag are saved and hashed. Piles are read by area and index, and ties go to the lower index.
- **The view.**
  - The **inventory and equipment window** (I) shows the four slots on the body, the carry boxes, the pocket silver and the soul's holdings by place.
  - The **warehouse window** opens by talking to a keeper or at a cave: what one carries beside one's account there, with deposit and withdraw.
  - **Piles** show on the ground. A prompt picks up the nearest within a cell, and a list shows them when several lie there.
  - **Loot** is marked in the inventory.

## Decision

Every item has one holder: a person's slots and inventory of 8 stacks, a warehouse's lot owned by an account, or a pile on the ground. Silver is held the same way. An account is a household, a soul, a sect, a faction or a merchant house. Four verbs move items: pick up, deposit or withdraw, buy and sell.

The dead drop everything where they die, and piles expire after 30 days, within a bound of 131,072. Once a second the nearest person with room takes a pile it wants. A soldier at war takes it as loot, carried until it is deposited to its faction's account.

Warehouses keep lots and balances by account. A keeper's warehouse charges a fee a stack. A sect's vault answers to its master and elders, and a faction's treasury to its leader and to soldiers drawing issue. A destroyed warehouse scatters everything on the ground for whoever picks it up. A village taken with its warehouse standing passes the old owner's accounts to the new. A dead household's account passes to its heirs, or else to the village's faction.

Caves are the player's alone: only a steered body opens a cave's cache, by the owner's decision. The soul owns its accounts and caches, so they outlive every body. Far areas loot in aggregate.

This supersedes ADR 0040's rule that the dead keep their silver.

## Consequences

- Death is worth something to the living. Battles leave gear on the field, and a raid's return trip matters.
- The player's soul has a reason to store wealth, and a reason to walk back to it after rebirth.
- A ruined village's warehouse spills into the hands of whoever stands there, which makes raids pay. That changes ADR 0023's growth balance, and the sweep is rerun in the implementing pull request.
- Caves add the game's only rule for the player alone, so the concept's principle now has one written exception.
- Saves grow by the lots, balances and piles, a few MB at the bound.

## Signs the decision was wrong

- Piles are mostly expiring unpicked, because the pick-up reach of a cell is too short for people walking past, so battles' gear never reaches markets.
- Factions grow rich on loot alone, and raiding outpaces building in the sweep.
- Players find the warehouse keeper knowing their reborn soul strange enough to name it, and ask for a token or password after all.
- Piles reach the bound in ordinary play, so the oldest are lost before 30 days.
