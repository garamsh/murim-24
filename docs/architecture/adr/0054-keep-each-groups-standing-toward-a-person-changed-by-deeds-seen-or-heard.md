# ADR 0054: Keep each group's standing toward a person, changed by deeds seen or heard

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #175 asks that what a person does to others changes how villages, sects and factions treat it, as in Mount & Blade. Through the dialogue a person can attack, extort, threaten, help or give a gift, and the groups of the person it acts on remember it. The owner called this decisive for play. The owner's decisions of 2026-10-08, in the issue, are binding:
- **Groups hold standing, each separately.** The village, the sect and the faction each hold a standing toward a person. A deed counts against or for every group of the person it is done to, with weights. Extorting a sect disciple who lives in village A drops A a lot, the sect moderately and the faction a little. Neither the village nor the sect owns the other, though a sect's seat is a village (ADR 0026).
- **Deeds become known through witnesses and rumour.** The witnesses' groups react at once, and farther groups learn as the rumour spreads over time. A deed no one saw stays hidden.
- **The same rules for NPCs and the player.** Bandits raiding a village, or a disciple killing a rival: the world builds its own grudges and debts.

Decided on PR #217's review, 2026-10-10:
- **By the owner.**
  - Standing belongs to the body. A reborn body starts with no standing, and warehouses and caves stay with the soul.
  - Individual NPCs' personal favor stays agreed and is implemented later. This ADR does not cover it.
- **By the PM.**
  - Hostility can be undone by giving and blood money, not only by decay.
  - A faction's negative standing turns its members against a person only when the faction stands hostile or at war with the person's side.
  - Extortion and the feel numbers, the war weight, the decay and the diplomacy's factors, stand as proposals in pack data, which the sweep checks.

The number 0053 is reserved for #205's ADR, so this ADR is 0054.

Facts from `main` at `aff1434`:
- **Reaches.** The region's rules give people a sight of 6 cells (ADR 0046), a talk reach of 2 (ADR 0039) and a rumour reach of 32 (ADR 0032), in `core/region.cpp`.
- **Events.** ADR 0032's ring keeps the latest 64 events, and `rumours` derives from it the ones reaching a person. ADR 0036 moved progress in the arts to a store of its own so the ring keeps its history.
- **Who fights whom.**
  - Factions stand hostile by stance unless a declaration stands between them (ADR 0038).
  - `hostile` is the one read every rule uses, faction to faction.
  - A duel makes two people hostile to each other alone (ADR 0018).
  - The view rings a body crimson when its faction stands hostile or at war toward the viewer's (`godot-adapter.md`).
- **Pending designs.** The open PR #215 (ADRs 0050 to 0052) waits on this ADR in three places: its witnesses marking a thief, a higher tax rate lowering reputation, and a leader's bad reputation feeding discontent.
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), so this ADR sizes everything at 50,000.

Options weighed:

- **Standing and its store.** Standing is an integer from −1,000 to 1,000 that a group holds toward a person. A group is a village, a sect or a faction. Zero is the default and is not stored. An entry is the person, the group, the value and the day it was last changed, 16 bytes.
  - The world holds at most 262,144 entries. When it is full, the entry nearest zero after decay goes, which forgets the least. Ties go to the oldest last change, then the lower entry index.
  - **Decay is computed, never written by a read.** Every entry decays by the same amount a day, so its value now is its stored value moved toward zero by the decay times the days since its last change. A read computes that and writes nothing.
  - **Finding the entry to evict.** Since decay is the same for all, the order of entries by nearness to zero never changes between writes. An entry's key, its stored magnitude plus the decay times its last-change day, ranks it for good. A binary min-heap of entry indices by that key, with each entry's place in the heap, finds the entry to evict at its root. An insert or an update costs a logarithm, about 18 steps, never a scan. An entry whose key shows it has decayed to zero is dropped when it reaches the root.
  - **Finding an entry.** An open-addressed hash table of entry indices keyed by group and person, at twice the entries, finds one in a few probes.
  - **Finding a person's entries.** Each person heads a list of its entries, linked through the entries, so a dead person's entries are dropped by walking its own list and nothing else.
  - **The hostile bit.** Each person carries a bit saying whether any group stands hostile toward it, so the rules that run every tick read the store only for those people.
  - **Memory.** An entry is 20 bytes: the person, the group, the value, the last-change day and the next entry of its person. The hash table's slots and the heap's places are 4 bytes each. At the bound that is 5 MiB of entries, 2 MiB of table and 2 MiB of heap and places.
  - Rejected: a table of every group by every person. At 50,000 people and about 400 villages, sects and factions, it is 20 million values, almost all zero.
  - Rejected: standing on the person, as a short list. A person known to many villages would overflow its list, and finding who is hostile to it would read every person.
- **Deeds.** A deed is one of a table of kinds, pack data (ADR 0049), each with a base value and a weight per mille for each tier of group. The base pack's table:

  | Deed | Base | Victim's village | Victim's sect | Victim's faction | Witnesses' groups |
  |---|---|---|---|---|---|
  | Attack: making someone not otherwise hostile a foe | −150 | 1,000 | 500 | 200 | 100 |
  | Kill | −500 | 1,000 | 500 | 200 | 100 |
  | Extort | −300 | 1,000 | 500 | 200 | 100 |
  | Threaten | −100 | 1,000 | 500 | 200 | 0 |
  | Help | +100 | 1,000 | 500 | 200 | 100 |
  | Gift, per silver given (100 copper once #215's ADR 0052 counts copper), at most 100 a day for each group | +1 | 1,000 | 500 | 200 | 0 |
  | Theft: picking up a pile scattered from a group's warehouse | −200 | 1,000 | 1,000 | 200 | 100 |

  - A deed's change to a group is the base times the tier's weight over 1,000, rounded toward zero.
  - A person who is the victim's kin, or of a group the victim belongs to, counts the deed against its own group's tiers, never twice: each group takes the largest weight it has in the deed.
  - **Provoked deeds.** A deed against someone who attacked the actor, or was its foe in a duel or a feud, within the last day counts at 0. Killing an attacker is self-defence.
  - **Kills in a war.** A kill of a soldier whose faction is at a declared war with the actor's counts at 300 per mille. Grudges build in war, but slower than in peace.
  - **Duels.** A kill in a duel for a faction's lead counts at 0 (ADR 0018).
  - **Blood money.** A gift to the kin of someone the giver killed within the last year counts double, toward the victim's groups.
  - **Theft.** A pile scattered from a destroyed warehouse (#215's ADR 0051) carries the group that owned the warehouse, 4 more bytes a pile:
    - a village warehouse's village
    - a sect vault's sect
    - a treasury's faction

    Picking it up is a theft against that group, by anyone not of it, so that group is the victim. It takes the theft at its own tier's weight, and the faction of a victim village or sect takes it at 200. A soldier at war looting it counts at the war weight.
    - **Witnesses** are those within sight of the pick-up, as for any deed.
    - **The rumour's anchor** is the owner group's anchor (below).
    - This designs the thief marking #215 deferred, and nothing marks a thief until this lands, as the owner decided there.
  - Rejected: a value per deed with no tiers. The owner's example needs the village to care more than the faction.
- **The verbs.** The dialogue's actions are orders through the port, the same for the player's body and any NPC. Each is refused by its first reason, as data, as `buy` is:
  - **Attack** makes the two foes of each other alone, a feud held on each person with its end, until one falls or a day passes. The person attacked fights back. Refused when they are already foes.
  - **Extort** takes half the victim's silver, pack data, when the actor's realm rank is above the victim's. Refused otherwise, as `NOT_WEAKER`, with no deed.
  - **Threaten**, against someone of a lower realm rank, makes it stop engaging the actor for a day. Refused like extortion.
  - **Help** uses one of the actor's wares on the other person, as `consume` uses it on oneself.
  - **Give** moves silver or a ware from the actor to the other person. It is the one transfer between people. When #215's ADR 0051 lands, it becomes that ADR's fifth verb for any stack.
  - All are refused beyond the talk reach.
  - **Help without the dialogue.** Killing someone who attacked a third person within the last day counts as help toward that person's groups. A defender earns standing without a word.
- **NPCs.** NPCs do deeds by the same rules: every kill and attack a fight makes, and help by defending. NPCs do not yet choose to extort, threaten or give. Those wait for #172's bandits and #170's routines, which will order them through the same port.
- **Witnesses.** At a deed, its witnesses are every living person within the rules' sight, 6 cells in the region, of the victim, the victim included while it lives, the actor excluded. Each group a witness belongs to, its home village, its sect and its faction, learns the deed at once:
  - **A group of the victim** takes the deed at its tier's weight.
  - **Any other group** takes it at the witnesses' weight, since a village that saw a murder thinks less of the murderer.

  A deed with no witness, such as a kill on an empty road, is hidden. It is never recorded, and no group ever learns it.
- **Rumour.** A witnessed deed spreads from its cell. Each group of the victim that has not yet learned it learns it once the rumour has travelled as far as the group's anchor:
  - a village's centre
  - a sect's seat
  - for a faction, the centre of its village nearest the deed

  The rumour travels 64 cells a day in the base pack, so a deed 40 cells away reaches in a day. A day is a minute at 1x, and the rumour travels at about half the walking speed of ADR 0047, which is 112 cells a minute.
  - Pending deeds are kept in a store of their own, `DEED_CAPACITY`, 4,096 in the core, apart from ADR 0032's ring, for the reason ADR 0036 gave: deeds would push the ring's history out. A deed leaves the store once every group of the victim has learned it, or after 60 days, when it is forgotten. A full store forgets the oldest.
  - A daily rule checks each pending deed against the anchors of its unknowing groups: at most 4,096 deeds and 3 groups.
  - The implementing pull request measures the store's pressure with `make sweep`, by its peak and the deeds forgotten for want of room, as ADR 0036 measured the ring.
  - Rejected: spreading only by people talking (ADR 0039). Rumours are derived, not carried, today. Carrying them would track who heard what for every person.
  - Rejected: deeds in ADR 0032's ring. A battle's kills alone would flood its 64.
- **Decay and repair.**
  - Standing moves toward zero by 2 a day, in the base pack, applied when an entry is read or changed, from the day it was last changed. So −600 heals in 300 days, about two and a half years.
  - Gifts and blood money raise it at once.
  - A group's standing toward a dead person is dropped, since it decides nothing. A soul's next body starts at zero with every group, as rebirth starts a new life.
- **Thresholds and what each changes.** The thresholds are pack data:

  | Standing | Tier | What the group's members do |
  |---|---|---|
  | −1,000 to −600 | Hostile | Treat the person as a foe: strike, engage within sight (ADR 0046), refuse to talk |
  | −599 to −200 | Unfriendly | Refuse to sell to it. Its sect refuses its application and its teaching. Its village's markets charge a quarter more, once #215's markets land |
  | −199 to 199 | Neutral | As today |
  | 200 to 599 | Friendly | Its sect's admission chance doubled. Markets charge a tenth less |
  | 600 to 1,000 | Honoured | Its sect admits it without the chance, by the thresholds alone, as `apply` does. Markets charge a fifth less |

  - A member's group for this purpose is each of its home village, its sect and its faction. The member takes the lowest tier any of them holds toward the person.
  - So `hostile` between two people becomes: their factions are hostile (ADR 0038), they are duellists, they are in a feud, or a group of one stands hostile toward the other.
  - The hostile bit keeps the per-tick reads to the people some group is hostile to.
- **Factions' relations.** Standing is toward people, never toward factions, so it feeds faction relations in one place only: ADR 0038's diplomacy.
  - A leader no driver steers declares war at twice its war chance on a faction whose leader its faction stands hostile toward, and makes peace at half its peace chance with one.
  - A deed counts once, against the person who did it. Its faction is never hostile for it. War comes only by a leader's declaration, which the standing toward the other leader makes likelier.
  - Rejected: standing between factions, raised by their members' deeds. One person's murder would turn a whole faction hostile, which counts the deed a second time.
- **What waits for #215.**
  - **A thief.** Witnesses of a pick-up from a warehouse the picker helped destroy record a theft deed, base −200, against the warehouse's village's groups.
  - **Tax.** A leader's tax rate above 100 per mille lowers its standing with each of its faction's villages by 1 a day for every 100 per mille above.
  - **Discontent.** A household whose village stands unfriendly or worse toward its faction's leader gains 1 discontent a day.
- **The view.**
  - The dialogue offers attack, extort, threaten, help and give, with each refusal's reason.
  - The dialogue's card and the faction window show the groups' standing toward the player by tier: its village, sect and faction.
  - A body whose groups stand hostile toward the player is ringed crimson like a hostile faction's, and an unfriendly one in a paler ring.
- **Tiers.** Deeds happen in every tier. In a far area, a fight's kills are witnessed by the two armies' members, read from their rosters, since positions there are coarse (ADR 0042). Rumour and decay are daily rules, untiered.
- **Cost.**
  - **A deed** reads one spatial query within sight, made only on a kill, an attack or a dialogue action: a few a tick in a battle.
  - **The hostile bit** keeps the strike and engagement rules at today's cost for the many people no group is hostile to. When either of two people is marked, `hostile` reads both directions, each person's three groups toward the other: 6 entries, each a few hash probes.
  - **A battle's worst case,** at 50,000 people with #216's live tier of 5,000:
    - Say ten kills a tick. That is five times the peak of the M1 battle's 500 scaled to 5,000, which loses half its people in about 2,700 ticks (ADR 0046).
    - Each kill queries sight's disc of 6 cells, about 113 cells. At the separation of a cell, that is at most about 113 witnesses, each with 3 groups: 339 group reads a kill.
    - The distinct groups among them are few, at most about 20, so each kill writes at most about 20 entries, each a hash lookup and a heap update of about 18 steps.
    - So ten kills are about 3,400 reads and 200 writes a tick, with 4,000 heap steps. That is well under a tenth of a millisecond by this estimate; the implementing pull request measures it.
  - **The daily rumour pass** is at most 12,288 anchor checks.
  - **Memory** is at most 9 MiB for the store, its table and its heap, and 128 KB of pending deeds. Each person adds a feud, a list head and a bit, 650 KB at 50,000.
- **Saves, the hash and determinism.** Entries, pending deeds, feuds and the hostile bits are saved and hashed. Everything is an integer. Witnesses and anchors are read in index order, and the lower index wins among equals.
- **A worked example.** The base pack's numbers, with the player's body P, of realm rank 3 and of no sect. V is a Hwasan disciple of rank 1 whose home is village A, an Orthodox village. P stands at zero with all three groups.
  1. P talks to V within 2 cells and extorts. P's rank is above V's, so V yields half its 40 silver, 20.
  2. V lives and witnesses its own deed, and V's groups are A, Hwasan and the Orthodox, so all three learn at once:
     - A takes −300 × 1,000 / 1,000 = −300: unfriendly. A's residents refuse to sell to P.
     - Hwasan takes −150: neutral.
     - The Orthodox take −60: neutral.
  3. A woodcutter of village B stands 5 cells away. B is not V's village, so B takes the witnesses' weight, −300 × 100 / 1,000 = −30.
  4. Thirty days later, decay has moved A to −240 and Hwasan to −90. The Orthodox and B are at 0.
  5. P gives a resident of A 50 silver: +50, so A is at −190, neutral, and sells to P again.
  6. Instead, P kills V on a road 40 cells from A with no one within 6 cells. The deed is hidden and nothing changes.
  7. Had the woodcutter seen the kill, B would take −50 at once. The rumour, at 64 cells a day, would reach A's centre, 40 cells off, on the first day: −500, unfriendly. The Orthodox, whose nearest village is A, would take −100 the same day. Hwasan's seat lies 120 cells off, so it would take −250 on the second day.

The implementation, in order, each a pull request with its acceptance:
1. **The store, deeds by fighting, witnesses, rumour and decay.**
   - Acceptance: tests with their controls show a kill witnessed changing the victim's groups at once, a hidden kill changing nothing, a rumour reaching a farther group on the day its distance says, decay, provoked and duel kills counting nothing, and the store's bound.
   - Acceptance: `make sweep` prints deeds a day, the pending store's peak and forgotten deeds, entries and the spread of tiers.
2. **Effects.** Hostility by standing, the sale and admission refusals and the teaching withheld, the hostile bit, and the diplomacy reading standing toward a leader.
   - Acceptance: each with a test and its control.
   - Acceptance: `make bench` showing the strike rule's cost unchanged with no one marked.
3. **The dialogue's verbs.** Attack and its feud, extort, threaten, help and give, each with its refusals, and help by defending.
   - Acceptance: the worked example as a test.
4. **The view.** Standing in the dialogue's card and the faction window, and the rings by tier.
5. **#215's hooks,** once its ADRs land: theft, tax and discontent.

## Decision

A village, a sect and a faction each hold a standing toward a person's body, from −1,000 to 1,000. Only non-zero entries are kept, at most 262,144. Decay is computed from each entry's last change and never written by a read. A min-heap keyed by magnitude and last-change day evicts the entry nearest zero, and a hash table and a list per person find entries without scanning. Each person carries a bit marking whether any group is hostile to it.

Deeds are pack data: attack, kill, extort, threaten, help, gift, and theft of a pile scattered from a group's warehouse, with a base value and weights for the victim's village, sect and faction and for the witnesses' groups. Provoked deeds and duels count nothing, kills at war less, and blood money double. The dialogue's verbs are orders refused for their reasons, the same for every person. Give is the one transfer between people.

The witnesses within sight teach their groups at once, and a deed no one saw stays hidden. A witnessed deed spreads from its cell at 64 cells a day to the anchors of the victim's groups, kept in a store of 4,096 apart from ADR 0032's ring.

Standing decays toward zero by 2 a day. Its tiers decide hostility, sales, admission, teaching and prices. Standing toward a faction's leader doubles war chances and halves peace chances in ADR 0038's diplomacy, so no faction turns hostile for one person's deed.

This supersedes these:
- ADR 0038's `hostile` as a reading of factions and duels alone, now also feuds and hostile standing; and its diplomacy reading no standing
- ADR 0039's talk refused only between hostile factions, now also by hostile standing
- ADR 0040's sale refused for no standing, now refused to a person a seller's group stands unfriendly toward
- ADR 0031's admission blind to standing, now refused when unfriendly, at double chance when friendly, and without the chance when honoured
- ADR 0046's foe as a person of a hostile faction, now as `hostile` reads it

## Consequences

- The world remembers. A village that was raided stands hostile toward the raiders who killed its people, and attacks them on sight for about a year after.
- The player's choices in the dialogue have lasting weight, and a gift can mend them.
- Raids leave grudges with individuals and not only with factions. That may change ADR 0023's raids in the sweep, which the implementing pull request reruns.
- The strike rule gains a per-person check for the few people some group is hostile to.
- Saves grow by up to about 5 MB at the bound.

## Signs the decision was wrong

- The store fills in ordinary play, so standing that matters is evicted.
- Most deeds are forgotten before their rumour arrives, so the pending store is too small or 60 days too short.
- Villages near a front stand hostile toward every soldier of the other side, so wars never end even after peace is made, and the war weight needs to be lower.
- Players find extortion pays more than it costs, because 2 a day heals −300 in 150 days, and ask for a harsher table.
- The hostile bit marks so many people in a long war that the strike rule's cost rises past ADR 0048's budget.
