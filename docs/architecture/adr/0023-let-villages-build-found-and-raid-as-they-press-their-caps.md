# ADR 0023: Let villages build, found and raid as they press their caps

> Status: accepted; its laying clear of an impassable cell superseded by ADR-0024, which lays only on buildable cells; its raiding party of a fixed count superseded by ADR-0052, which caps it by what the treasury pays; its plans raised by the repair rule alone and its founding with nothing carried superseded by ADR-0056, under which a structure rises only as far as its materials are delivered and settlers carry a first house's cost; its growth stance as fixed data superseded by ADR-0065, under which the growth order is a faction's settlement policy; its founding laying the settlement's layout, and its building nearest the centre within one vicinity, superseded by ADR-0076, which founds a core and places each plan by role and path distance from the square; read by #378 as having a hostile army ordered into a village's claim, a raiding party or any other, pursue its standing structures once no foe is in sight and nothing standing is in its reach, corrected by #392 to nothing standing in its objective's reach, since a lone member in reach held the whole band; its growth by pressing villages alone amended by ADR-0082, under which every village lays the square kinds its tier names at the growth check, pressed or not
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept makes expansion (building, new villages, territory and raids) the motive for war (`docs/concept.md` §Growing civilization). ADR 0022 caps each village by its housing, food and storage. Issue #83 asks for one growth rule, stated as data, that chooses among building, founding and raiding from a village's cap, its neighbours and its faction's stance, with no branch per faction.

Three facts shaped the choices:
- Repair is the one strike rule with a negative structure effect, and a village's state is derived, never stored (ADR 0015).
- Every footprint blocked from the world's creation, so no flow field ever changed under an army (ADR 0013).
- A family was one per starting army, and nothing could make another (ADR 0021).

Options weighed:

- **The rule.** `Growth` in `Life`, checked every `interval` days. A village with living residents presses against its cap once its people reach `pressure` per mille of it. It then takes the first expansion of its owner's stance that it can. A stance is per-faction data, an ordered list of `BUILD`, `FOUND` and `RAID`.
  - In the region, the Orthodox build first, the Unorthodox found first, and the Demonic Cult, whose torch raises nothing, only raids.
  - Rejected: weights or chances per expansion, which need a draw for a choice that the order already settles.
  - Rejected: a stance as one enum per faction, which cannot say what to do when the first choice is closed.
- **Building.** The village lays one plan from `builds`, the one that most raises its scarcest holding, measured in people. It lays it as rubble on the free cells nearest its centre, within `vicinity`, a cell clear of any structure or impassable cell. Its residents raise it by the repair rule, and it houses or stores once it stands. A village builds only while every structure of its own is intact, so it raises one thing at a time.
  - Rejected: a construction system with progress of its own, which would be a second rule beside repair.
  - Rejected: a structure placed intact, which skips raising, so a faction that cannot mend would build as well as one that can.
  - Rejected: building whatever raises the cap at once. With the region's templates no single plan raises it, since food and storage bind together.
- **Founding.** The village lays `layout` as rubble on a `settlement` area of free land. The area is drawn by the seed among those whose centre lies within `reach` of its own, `spacing` cells clear of any structure, and held by no other faction. Its `settlers` march there as the new village's residents. The new village is a village like any other.
  - The settlers keep their old home, and eat from its stock, until a harvest finds the new village's cap holding them all. Then it becomes their home.
  - Rejected: making it their home at once. A village laid as rubble stores and harvests nothing, so its settlers would starve before they raise it.
  - Rejected: carrying a share of food with them. The new village has no standing storage to hold it, and ADR 0022's stock never exceeds storage.
- **Families.** The founder is the first resident, of the sex the lineage follows, who leads no faction. Its spouse and unmarried children go with it, then other residents in roster order. The founder and its children take a new family, a branch that bears its old family's surname. So a family can branch, and only by founding.
  - Rejected: the founder keeping its family, which leaves every village of a line one family forever.
  - Rejected: a new surname drawn for the branch, which would cut the line that the genealogy reads through names.
- **Raiding.** The village sends `raiders` of its residents as a party against the nearest village of a faction it is hostile to, with living residents, within `reach`. The raid ends once its party has no one left, its target has no living resident, or its target's owner is no longer one it is hostile to. Its members then return to their homes.
  - A party that restores a structure of an emptied village becomes its residents by ADR 0015's rule, and the village changes owner.
  - Each village keeps one party army and reuses it, so armies grow with villages, not with raids.
  - Rejected: ordering the residents themselves, which empties the village.
- **Territory.** It is derived on read, never stored. A cell lies in the territory of the owner of the nearest village with living residents within `claim` cells. So founding, emptying and capture all move it.
  - Rejected: the region's partition by capitals. Nothing in it changes as villages do, and the generator keeps it only to place villages.
- **Laying a blocking structure.** When growth lays a structure that blocks, every army's flow field is rebuilt on the new map. A placement that would leave any person unable to reach its army's objective, or the settlers their new centre, is refused. Placements also avoid every army's objective and every living person's cell.
  - This supersedes ADR 0013's rule that footprints block only from the world's creation, so flow fields never change under an army. ADR 0013's concern was a rebuild on the tick a structure falls in battle. A growth check comes at most once a day, and the M1 scenario has no growth.

## Decision

Villages grow by one data rule, `Growth` in `Life`. A village pressing against its cap takes the first expansion of its faction's stance that it can: build, found or raid. Building and founding lay structures as rubble for residents to raise by the repair rule. Founders head a new family branch under their surname, and settlers make the new village their home at the first harvest it can hold them. Territory is derived from peopled villages. Laying a blocking structure rebuilds every army's flow field, superseding ADR 0013's rule that footprints block only from the world's creation.

## Consequences

- Over 50 years of seed 24 with days of five ticks, villages went from 6 to 9 and armies from 6 to 11. The Orthodox held 1,830 cells at the start and 3,597 at the end, the Demonic Cult 1,454 and 1,558, and the Unorthodox 1,634 and 931. The living peaked at 121.
- At play pace, 1,800 ticks a day, seed 2026 founds its first village in year 4 and lays its first building in year 5.
- A founded village reads as ruins until its settlers' first mend lands, since every structure starts as rubble.
- The view draws laid structures and new armies, but its borders and village decorations still follow the region's generation, so a founded village shows no banner or paving.
- A faction that cannot mend cannot grow by building or founding, whatever its stance says.
- The pinned determinism hash is unchanged. Its world has no villages, and the state this adds is a village's party and the structures, armies, villages and families growth makes.

## Signs the decision was wrong

- Cult raids empty the region faster than births refill it, so population falls over decades and growth stops. Raids would then need a limit, such as a truce or a cost.
- Villages that are laid out but never settled pile up, because raids burn them back faster than settlers raise them. Founding would then need an end for a settlement that fails.
- Rebuilding every flow field on a growth check costs a visible spike in play once armies number in the hundreds.
- Branches under one surname read as the same family in play, and a branch needs a name of its own.
