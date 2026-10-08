# ADR 0026: Keep sect membership on the person and seat each sect in a village

> Status: accepted; its elders by an attainment threshold superseded by ADR-0028, which makes them a share of a sect's members; its rejection of joining through the port superseded by ADR-0031, which lets a person apply; its manual of a discipline superseded by ADR-0035, which makes it name an art
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept separates a sect from a village (`docs/concept.md` §Sects). A sect is an organization within a faction, with members, elders, martial arts and a seat. Its members may live in several villages, and either can fall without the other. Issue #103 asks for the model as a design, answering the three questions §Sects leaves open:
- How a sect's seat relates to a village.
- How membership is gained and lost, per faction.
- What caps a sect's size.

Two rules must sit on the model: the Orthodox leader is elected by the favour of sect elders (§Factions), and secret manuals scatter or are lost when a sect is wiped out (§Martial arts).

Facts that shaped the choices, from `core/world.hpp` and `core/region.hpp` at `aa128dc`:
- Every living person stands on exactly one roster, and a village's residents are an army (ADR 0017). A person's faction is its army's (`faction_of`).
- A village's cap and state are derived on read from its structures and residents, and what a structure holds is per-kind data, `Holding` (ADR 0015, ADR 0022).
- A faction carries an optional `Duel` as its succession, and nothing names an heir when a leader dies outside a duel (ADR 0018).
- Attainment per discipline grows by practice (ADR 0019). Nothing in the core yet holds favour, infamy, money, techniques or items.

Options weighed:

- **What a sect is.** Data given with the world: a name, a faction, a seat and the manuals it holds. Membership is a field on the person, the sect it belongs to or none. Everything else about a sect is derived on read from its members: who they are, how many, its elders, its master, and whether it stands.
  - Rejected: a sect as an army, a roster of its members. A member already stands on its village's roster, and ADR 0017 allows one. Its members living in several villages is exactly a person on two rosters.
  - Rejected: a sect as a stored record holding a member list, its elders and a standing flag. Every death, migration and conscription would have to update it, and it can disagree with the people it lists, the drift ADR 0022 rejected for the village cap.
  - Rejected: one sect per faction, as faction data. The concept names several sects in one faction, Hwasan and Wudang both Orthodox.
- **The seat.** A sect's seat is a village of its own, by index, laid out from a seat template. It is a village like any other: residents, cap, food, raids, ruin and rebuilding all hold for it unchanged. Its residents need not be members, and members need not live there. The seat is held while its village is not ruins and its owner is the sect's faction.
  - Rejected: a compound within a village, a set of its structures marked as the sect's. The village would have two owners, its residents' faction and the sect's, and a structure would belong to two things. A raid that ruins the village ruins the compound with it, so the two could not fall apart as the concept asks.
  - Rejected: a seat as a cell with no structures. Nothing would stand there to raid, burn or rebuild, so losing the seat could not happen in play.
- **Admission and leaving.** `Admission`, optional per faction, checked once a day as kinship is. All ages are in days and chances per mille a year.
  - A living person of no sect, serving in an army of a sect's faction, of the admission age, with a talent limit in some discipline of at least the admission's talent and an attainment of at least its attainment, and a home within `reach` of the seat, joins with the admission chance.
  - It joins only a sect whose seat is held, with a living elder to admit it, and with fewer members than its cap. Among several such sects, the seed draws one.
  - The factions differ by numbers only. The Orthodox and the Cult assess talent; the Unorthodox, "proof of usefulness", take attainment and no talent.
  - A member leaves its sect when it dies, or when the daily check finds it serving in an army of another faction, as migration (ADR 0022) or conscription can make it.
  - Rejected: a per-faction rule, such as a branch for the Cult's members-only entry. Every sect admits only its faction's people, so "members only" is the rule for all.
  - Rejected: infamy and money as terms now. Neither exists in the core. Each joins `Admission` as one more threshold when reputation or coin arrives, without changing the rule's shape.
  - Rejected: joining as an order through the port, for the player's body. The player is born under the same draw as any NPC, and a port to ask for entry waits for a UI that asks.
- **Elders and the master.** An elder is a living member whose best attainment is at least the sect's faction's `elder_attainment`. The master is the elder of highest best attainment, ties to the lower index. Both are derived, so a member becomes an elder by practice and the master changes when a stronger elder rises or the master dies.
  - Rejected: a fixed number of elders, the top attainers. A sect of three would have as many elders as one of thirty, and its vote in the election would not reflect its standing.
  - Rejected: elders appointed and stored. Appointment needs a rule for who appoints, and a stored rank can outlive the attainment that earned it.
- **The cap.** A sect's cap is derived on read: the `pupils` of its seat's standing structures, while its seat is held, and 0 otherwise. `pupils` is a new per-kind term of `Holding`, beside housing, storage and yield. A hall teaches some; the proposed training ground (§Growing civilization) would teach more. A sect at or above its cap admits no one and expels no one.
  - Rejected: a fixed number per sect in data. Burning the seat would change nothing, and the cap would not follow the world.
  - Rejected: the seat's housing. Members live in several villages, so where they sleep does not bound how many a sect trains.
  - Rejected: pupils per elder. Elders are themselves members, so the cap would grow by admitting, a loop with no bound but the 500.
- **Falling.** A sect stands while it has a living member, and falls when its last member leaves, by death or by serving another faction. A fallen sect has no elder, so it admits no one, and it stays fallen with no flag to store.
- **Manuals.** A manual is an item: a discipline and a place, a sect holding it, a cell, or lost. The place is stored, since nothing else records where a thing lies. When a sect falls, each manual it holds scatters, with the `Manuals` scatter chance, to a passable cell drawn by the seed within `scatter_reach` of its seat, and is otherwise lost. Who may pick one up, and what reading it does, wait for martial arts.
  - Rejected: manuals as attainment held by the sect, derived from its members. A fallen sect has no members, so nothing would remain to scatter.
- **The election.** `Election`, optional per faction, beside `Duel`. A faction with an election chooses a leader when its leader is dead or no one leads, and every `term` days if the data gives one. The candidates are the masters of its sects. Every elder of the faction's sects gives its favour to one candidate, and the candidate with the most favour leads, ties to the higher attainment, then the lower index.
  - Until favour exists, an elder favours its own sect's master, so the master of the sect with the most elders leads. Reputation replaces whom an elder favours, not who votes or how the votes are counted.
  - A faction with an election and no duel refuses every challenge, as ADR 0018's rule already does for a faction without one, so the Orthodox lead "cannot be taken by force" with no new refusal.
  - Rejected: the strongest master leads. That is a duel without the fight, and the concept says the Orthodox lead is won by favour, not force.
  - Rejected: waiting for reputation before any election. Until then a dead Orthodox leader leaves the faction led by the dead, the gap ADR 0018 names.
- **Armies.** An army stays a roster with a faction, owned by neither a village nor a sect. A village's residents and its raid party are armies, as ADR 0023 has them. A sect fields a force by conscripting its members from their rosters into an army of its faction, through `conscript()`. No army records a sect.
  - Rejected: an army field naming its sect. Nothing yet reads it, and a sect war, the first rule that would, is not designed.
- **What changes in data.**
  - `Person` gains its sect, or none. Its home, family, army, talent and attainment stay as they are.
  - `Faction` gains an optional `Admission`, with its `elder_attainment`, and an optional `Election` beside its `Duel`. Its hostility, skills and stance stay.
  - `Holding` and `StructureKind` gain `pupils`. Growth's rule stays: it lays plans for the scarcest of housing, food and storage, so it never builds for a sect's cap, and founding never makes a seat or a sect. Settlers keep their sect, since membership does not follow residence.
  - The world gains its sects, each a name, a faction and a seat village, and `Manuals` in `Life`: the manuals each sect starts with, the scatter chance and reach. The region places one seat per sect from a seat template.
  - Villages, armies, the duel and the one skill rule stay as they are.

## Decision

A sect is data, its faction, seat and manuals, and membership is a field on the person; a sect's members, elders, master, cap and whether it stands are derived on read. Its seat is a village of its own, and its cap is the pupils its seat's standing structures teach while the seat is held. People of a sect's faction join by a daily draw against per-faction `Admission` thresholds, and leave by death or by serving another faction. A sect falls when its last member leaves, scattering or losing its manuals. A faction with an `Election` makes the master of the sect with the most elders' favour its leader, an elder favouring its own master until favour exists.

## Consequences

- A sect survives its seat's ruin, admitting no one until the seat stands again, and its seat village survives the sect's fall as an ordinary village.
- The Orthodox gain an heir when their leader dies, which ADR 0018 left open. The Cult's duel is unchanged.
- Each day a world with sects scans the living once for admission and leaving, as kinship does. Members, elders and caps are counted on read, at most once per sect a day.
- Infamy, money and favour each enter as data on rules already shaped for them: a threshold in `Admission`, the choice of whom an elder favours.
- A person can belong to a sect while living anywhere within its reach, and serving in any army of its faction, so a raid party may carry members of several sects.
- The pinned determinism hash will change: it folds each person's sect and each manual's place.
- No one can join a faction by entering its sect. The core has no rule for a person to change faction, so the concept's Orthodox entry "Enter a sect" holds only for people already of the faction.
- The world-creation data must give each sect founding members with an elder among them, or no sect admits anyone until a member practises to `elder_attainment`.

## Signs the decision was wrong

- Players expect a sect to defend its seat or march as one, and a sect needs an army of its own that a field on the person cannot give.
- Seat villages read as ordinary villages, and the seat needs state or art of its own beside a village's.
- Pupils from the seat's structures never bind, because members leave or die faster than sects fill, so the cap reads as decoration.
- Elders by an attainment threshold make every old member an elder, or none, so the election is decided by age or never held.
- People switch faction often enough by migration that sects shed members every season, and membership needs to survive a change of army.
