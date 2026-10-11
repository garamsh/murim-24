# ADR 0060: Keep each person's favor toward the few it deals with often, beside its groups' standing

> Status: proposed
> Date: 2026-10-10

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner keeps it agreed in `concept.md` (2026-10-10, PR #217) that, besides the standing of villages, sects and factions (ADR 0054), individual NPCs hold a personal favor toward a person. It is implemented later, held only toward people one has dealt with often, and bounded per person. Issue #219 asks the design to state:
- who holds favor, among kin, master and disciples, trading partners and rivals, and the bound per person
- what raises or lowers it: ADR 0054's deeds seen first-hand, gifts, help, betrayal
- how it combines with group standing in talk, trade, admission and hostility
- decay
- memory and tick cost at 50,000 people and 5,000 live
- determinism, and that favor belongs to the body, as standing does

Facts from `main` at `2462921`:
- **ADR 0054's standing.**
  - Standing is held by a village, a sect and a faction toward a person, from −1,000 to 1,000, in non-zero entries, at most 262,144.
  - Its deeds are attack, kill, extort, threaten, help, gift and theft, each with a base and weights.
  - Witnesses within sight teach their groups at once. Decay is 2 a day, computed and never written by a read.
  - Its tiers are hostile, unfriendly, neutral, friendly and honoured. A member acts on the lowest tier among its village, its sect and, under a guard, its faction.
  - Standing belongs to the body.
- **Dealings today.**
  - Kin are recorded on the person: spouses, parents and children (ADR 0021).
  - Sect teaching and elders exist (ADR 0026, ADR 0035).
  - `buy` moves a ware from seller to buyer (ADR 0040).
  - Duels and feuds make two people foes (ADR 0018, ADR 0054).
- **Scale.** The bound is measured between 10,000 and 50,000 (ADR 0048), and #216's live tier holds 5,000.

Options weighed:

- **Who holds favor, and toward whom.** Each person holds favor toward at most 8 others, its acquaintances, in slots on the person. A slot is the other person, the favor from −1,000 to 1,000, the day it last changed, and its kind, 8 bytes. A person enters a slot in three ways:
  - **Kin.** Spouses, parents and children enter at birth or marriage, and are never evicted. Kin hold at most 6 of the 8 slots: the living spouse first, then parents, then children from the eldest. Further kin hold none, so 2 slots always stay for others.
  - **Dealt with often.** Every dealing between two people counts toward each other in a pending slot of 4 a person, holding the other, a count and the day of the first dealing, 8 bytes. A dealing is any one of these:
    - a purchase from the other, or a sale to it
    - teaching or being taught in its sect, one day of it
    - talk through the dialogue
    - one of ADR 0054's deeds done to or by the other

    Three dealings within 30 days promote the other to a slot. A sect's master and its elders count their pupils' days of teaching. Rivals are those met as a foe in a duel or a feud.
  - **A deed too strong to wait.** A deed whose change to the person would be 150 or more either way enters the actor at once: an attack on oneself, ADR 0054's −150 at a victim's weight of 1,000, or a kill of kin. A grudge needs no acquaintance. A gift moves standing by at most 100 a day for each group (ADR 0054), so it counts as a dealing and builds a debt of gratitude over days rather than at once.
  - Rejected: favor toward everyone met. At 50,000 people it is the full table ADR 0054 rejected for groups.
- **The bound and eviction.** With all 8 slots full, a new entry replaces the non-kin slot nearest zero after decay. Ties go to the oldest change, then the lower index. If every non-kin slot is further from zero than the newcomer, the newcomer is dropped. A pending slot replaces the oldest pending one.
- **What raises and lowers it.** Personal favor takes ADR 0054's deeds, seen first-hand, at their own weights. These are pack data, and the base pack's are:

  | The holder is | Weight of the deed's base |
  |---|---|
  | its victim | 1,000 per mille |
  | the victim's kin | 600 |
  | a witness within sight, `sees` in ADR 0058 when that lands | 200 |

  - **Gifts and help** done to the holder count as ADR 0054's gift and help at the victim's weight.
  - **Betrayal.** A harmful deed by someone the holder favors at friendly or above counts double, and a kind deed by someone it holds hostile counts half.
  - **Provoked deeds and duels** count as ADR 0054 counts them: nothing.
  - A deed no one saw changes no one's favor but its victim's, if the victim lives.
- **Decay.** Favor moves toward zero by 1 a day, computed from the last-change day and never written by a read, as standing's is.
  - Kin favor decays only toward a floor of 200 for spouses, parents and children, so kin stay friendly unless wronged.
  - Rejected: no decay. An old quarrel would fill a slot forever.
- **How it combines with group standing.** Favor is the person's own conduct, and standing is its groups'. Each rule asks the one that owns the act:

  | Act | Asks |
  |---|---|
  | whether the holder strikes, engages or refuses to talk | the holder's favor toward the other if it holds a slot, else its groups' tier as ADR 0054 reads it |
  | whether the holder sells to the other | the same |
  | the price at a market | the market's village standing (ADR 0054), less a tenth if the market's keeper favors the buyer at friendly or above |
  | admission to a sect | the sect's standing (ADR 0054); the sect's master's favor at friendly or above doubles the chance again, and at hostile refuses |
  | whether the holder joins its group's hostility | its favor, if it holds a slot, else the group's tier |

  So a friend in a hostile village does not strike, and a personal enemy in a friendly village does. A group's own acts still follow the group.
  - Rejected: summing favor and standing into one number. A strong personal tie would then move a whole village's prices, and a village grudge would turn a friend.
- **Belonging to the body.** Favor is held toward a body. A reborn body starts with no favor anywhere, and a dead person's slots, and the slots others hold toward it, are dropped.
  - Each person's slots are checked once a day for a dead other, spread over the day by index.
  - The hostile bit (ADR 0054) also marks a person someone holds a hostile favor toward, so the per-tick rules read favor only for those.
- **Tiers.** Favor is held by everyone in every tier. Dealings and deeds in far areas count as anywhere. Far fights' kills record their deeds as ADR 0054's far witnesses do.
- **Cost, at 50,000 people with #216's live tier of 5,000.**
  - **Memory.** 8 slots of 8 bytes and 4 pending slots of 8 bytes, 96 bytes a person: 48 KB at ADR 0017's 500, 480 KB at #216's live tier of 5,000, and 4.8 MB at 50,000.
  - **A dealing** finds or makes a pending slot among 4: a few reads, made only on a purchase, a talk, a day's teaching or a deed. A battle's deeds at the live tier are ADR 0054's worst case of about ten kills a tick. Each kill updates its victim's kin and witnesses, at most about 113 holders, each scanning 8 slots: about 9,000 slot reads a tick, a few hundredths of a millisecond by this estimate, which step 2's `make bench` measures, as ADR 0054 has its own measured.
  - **`hostile`.** Each check scans the holder's 8 slots when the bit is set, 8 reads, beside ADR 0054's 6 entries.
  - **The daily pass** over each person's 8 slots for the dead is 400,000 slot reads a day at 50,000, about 220 a tick.
- **Bounds, with refusals.** These are core bounds. A pack past any of them is refused at load with a `ContentRefusal` naming the bound (ADR 0049):
  - slots per person at most 8
  - pending slots at most 4
  - the dealings to promote at most 15
  - a deed's weight at most 2,000 per mille
- **Saves, the hash and determinism.** Slots and pending slots are saved and hashed. Every value is an integer. Slots are scanned in index order, and eviction's ties break by the oldest change, then the lower index.
- **The view.** The dialogue's card shows the person's favor toward the player by tier, when it holds a slot, beside the groups' tier. The core exposes the tier, so the view decides nothing.

The implementation, in order, each a pull request with its acceptance, after ADR 0054's standing is built:
1. **Slots, kin and dealings.**
   - Acceptance, each with a test and its control: kin entered at marriage and birth; a trading partner promoted after three purchases in 30 days, where two leave it pending; eviction of the slot nearest zero.
   - Acceptance: 96 bytes a person saved and hashed.
2. **Deeds, decay and the combination.**
   - Acceptance, each with a control: a victim's, a kin's and a witness's favor changed by one kill; betrayal counting double; a friend in a hostile village not striking; a personal enemy in a friendly village striking; a master's favor in admission; kin decaying to their floor.
   - Acceptance: `make bench` within budget with a battle's deeds updating favor at the live tier.
   - Acceptance: `make sweep` 48/50 or better, since favor changes who strikes.
3. **The view's favor tier** in the dialogue's card.

## Decision

Each person holds favor toward at most 8 others: kin, up to 6, entered at birth or marriage and never evicted, those it has dealt with three times in 30 days, and anyone whose single deed moved it by 150 or more. Four pending slots count the dealings first. Favor takes ADR 0054's deeds first-hand, at full weight as victim, 600 per mille as kin and 200 as a witness, with gifts and help, betrayal doubled, and provoked deeds and duels as nothing. It decays 1 a day, computed and never written by a read, and kin only to a floor of 200.

A person's own conduct follows its favor where it holds a slot, and its groups' tier otherwise. Its groups' acts, prices and admission follow standing, with the keeper's or master's favor as a modifier.

Favor belongs to the body, and a dead person's slots are dropped daily. The cost is 96 bytes a person, 4.8 MB at 50,000.

This supersedes ADR 0054's member acting on its groups' tier alone, now its own favor first where it holds one.

## Consequences

- NPCs have friends and enemies of their own, so a village's grudge spares its friends of the player, and a single NPC can pursue a vendetta its village does not share.
- The player gains reasons to return to the same merchant and master.
- A save grows by 4.8 MB at the bound.
- The hostile bit marks more people in a long war, for personal grudges, so the per-tick reads grow with them.

## Signs the decision was wrong

- 8 slots fill with rivals in a war, so kin and partners are evicted in play, and the bound or the kinds need weights.
- Personal enemies attacking inside friendly villages cause fights the owner finds senseless.
- Favor hardly ever forms, because 3 dealings in 30 days is too rare for NPCs, so NPCs have no friends to show.
- The dialogue's two tiers, the NPC's and its groups', confuse players.
