# ADR 0038: Let faction leaders declare war and make peace over their stances

> Status: accepted; its diplomacy by numbers alone superseded by ADR-0052, which gates war and peace by the treasury; its `hostile` read from factions and duels alone, and its diplomacy reading no standing, superseded by ADR-0054, which adds feuds and hostile standing and weighs standing toward the other leader
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #165 asks for a faction window in which the player sees how every faction stands toward its own, and in which a faction's leader declares war or makes peace. The owner decided on 2026-10-08 that only the leader may do either. Anyone else sees the relations and may challenge the leader, under ADRs 0018 and 0026 as they stand. NPC leaders give the same order by the same rule, and raids and hostility read what is declared. The PM approved the window's mockup on PR #177, with four relation words: ally, neutral, hostile and at war.

Facts that shaped the choices, from `core/` at `0e3082a`:
- Hostility is one directed matrix in `Rules`, `hostile[a][b]`, given as data. A duel won with `allies_turn_hostile` writes into it (ADR 0018). Five places read it directly: the strike rule's foes, a village's damage and repair, its raided state, a raid's target, and a raid's end.
- No order changes how two factions stand, and nothing records what changed it.
- ADR 0011 records nothing of who steers an army, so a rule that decides for NPCs would decide for the player's body as well.
- Issue #175 will have villages, sects and factions hold standing toward one another, and that standing will feed faction relations. Its design is pending.

Options weighed:

- **(a) A declared relation over the stance, one per pair.** Each two factions hold at most one declaration, war or peace, with the faction whose leader made it and the tick. The rules' matrix stays the stance-given default. `hostile` reads the declaration first: war is hostile both ways, peace neither way, and with no declaration the matrix decides. `relation` is the one read of how a faction stands toward another, and the HUD, nameplates (#164) and dialogue (#166) all read it:
  - own
  - ally: neither faction is hostile to the other by the matrix
  - neutral: peace declared
  - hostile: either faction is hostile to the other by the matrix
  - at war: war declared

  Chosen. Every rule that read the matrix now reads `hostile`, so a declaration reaches strikes, village damage and repair, raids and the raided state with no rule of its own.
- **(b) Write declarations into the matrix, as a won duel does.** Rejected. Peace made would erase the stance underneath, so there would be no default to read once a declaration is superseded, and the window could not show the basis, who declared and when. A later input, #175's standing, could not tell a leader's order from a stance either.
- **(c) A declaration by one faction toward the other, directed like the matrix.** Rejected. A war one side fights alone, while the other stays at peace, has no word in the four the mockup shows. Each of those words names how a pair stands.

The order:
- `declare(person, faction, what)` takes effect at once for both factions, the same order for the player's body as for an NPC. `declaration_refusal` names the first reason it may not, and `declare` refuses exactly when there is one. The reasons, in order: the person is dead; it does not lead the faction of the army it serves in; the faction is its own; war is declared on a faction already at war with it; peace is made with one neither hostile to nor at war with it (ally or neutral); the pair's last declaration was made fewer than the diplomacy's `hold_days` days ago.
- War on a faction hostile by stance is allowed: it fixes the hostility so that no later change to the matrix can undo it. Peace with a faction hostile by stance is allowed for the player's order. The NPC rule below does not make it.

Who decides for whom (decided by the PM on the coordinator channel during #165):
- **An army is self-directed or steered.** A driver marks the armies it steers with `steer`. The mark is read by `steered`, saved and hashed. The adapter marks the player's army when it makes a region. Every rule that decides for people on their own, diplomacy now and the routines #170 adds later, acts only for leaders and armies no driver steers, by one rule. A steered leader's faction changes relations only by the leader's own orders. This supersedes ADR 0011's point that nothing records who steers an army. Orders themselves are unchanged: any driver may order any army.
- Rejected: **the rule acts for every leader, the player's included**, as the automatic strike acts for the player's body. It would make war for the player's faction.
- Rejected: **exclude leaders who serve in an army of one**, which the player's body is. It is a proxy, and #172's wandering martial artists, also armies of one, would break it.

The NPC rule, `Diplomacy` in `Life`, runs once a day with no player branch. For each faction whose living leader no driver steers, it looks at each other faction with living people, in index order:
- toward a faction allied with or neutral to its own, it declares war with `war_chance` per mille a year while its own living people number at least `war_margin` per mille of the other's
- toward a faction at a declared war with its own, it makes peace with `peace_chance` per mille a year while its own living people are fewer
- toward a faction hostile by stance alone, it does nothing

The region's data (`region_data()`): `war_margin` 1,500, `war_chance` 100, `peace_chance` 200, `hold_days` 600 (five years).

The first draft also let an outnumbered leader make peace with a faction hostile by stance. `Region.VillagesBuildFoundAndRaidOverFiftyYearsWithinTheBound` then failed on seed `NEIGHBOURLY`: no raiding party was sent in 50 years, because the smaller factions made peace with the Demonic Cult, whose only stance is to raid. Limiting NPC peace to declared wars keeps the stances' raids and passes the test.

Measured with `make sweep`, seeds 0 to 49 for 100 years each at days of five ticks:

| | Every faction peopled through 100 years | Wars declared | Peaces made |
|---|---|---|---|
| `main` at `0e3082a` | 50 of 50 | none | none |
| with this decision | 48 of 50 | 60 (0 on 12 seeds, 1 on 22, 2 on 11, 3 on 4, 4 on 1) | 33 (0 on 25 seeds, 1 on 18, 2 on 6, 3 on 1) |

So an NPC leader declares war about 1.2 times in a region's century, and peace follows about half as often. The wars cost two seeds, 27 and 48, a faction that once held no peopled village at a year's end. That is within the issue's bound of 48.

How #175 builds on this: its standing is another input to `relation` and `hostile`. The declaration stays a separate, small layer, a leader's order alone, which standing never writes. A later ADR may put standing between the declaration and the stance, reading a declaration first, then standing, then the matrix. It may also have the diplomacy rule weigh standing in place of headcounts. Neither changes what a declaration is or how it is stored.

## Decision

Two factions stand by their stances unless one of their leaders declares war or makes peace, a declaration over the stance held per pair with who made it and when. `relation` is the one read of how they stand and `hostile` the one read every rule uses. Only a faction's leader may declare, through `declare`, refused for its reasons as data. NPC leaders declare by one daily rule, `Diplomacy`, for every leader whose army no driver steers. A driver marks the armies it steers, and that mark is saved and hashed.

## Consequences

- Strikes, village damage and repair, raids and the raided state follow a declaration with no change of their own. Peace made ends a raid at its next check.
- The HUD reads one relation, and nameplates and dialogue can read the same one.
- A save holds the steered marks, the declarations and the diplomacy, so `SAVE_VERSION` rises to 6. The hash folds each only while it is held, so a world with none keeps its hash.
- The rules that decide for NPCs need the steered mark to skip the player. A test or a driver that forgets to mark an army has the rules decide for it.
- A won duel's `allies_turn_hostile` still writes the stance. Under a declared peace it changes nothing until no declaration stands.
- No event records a declaration, so no rumour carries news of a war.

## Signs the decision was wrong

- #175's standing needs to change a declaration, or to be stored in the same place, so the layers merge after all.
- Players find that a war fixed over a stance's hostility and the stance alone feel the same, and the four relations collapse to three.
- NPC wars and peaces flip back and forth every `hold_days` in long runs, or a faction's raids stop, as the first draft's peace with stance enemies stopped them.
- Wars wipe factions out more often than the two seeds of fifty measured here, and `war_chance` or `war_margin` needs a bound.
- More rules need to tell the player's armies apart in ways the steered mark cannot express, such as a party the player leads but does not steer.
