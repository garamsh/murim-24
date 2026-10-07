# ADR 0028: Make a sect's elders the share of its strongest members

> Status: accepted
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0026 made an elder any member whose best attainment reaches its faction's `elder_attainment`. It named, as a sign that this was wrong, that every old member becomes an elder, or none, so that the election is decided by age or never held.

Implementing it showed exactly that sign. The Orthodox and Unorthodox mend, and every mend that restores a structure is practice, so their members' attainment rises to their talent limit, which lies between 300 and 700. Above the threshold, attainment only mirrors talent.

Over 100 years with days of five ticks, on seeds 24, 2026 and 7, the elders and members of each standing sect at the end were:

| `elder_attainment` | Seed 24 | Seed 2026 | Seed 7 |
|---|---|---|---|
| 120 | 16 of 20, 20 of 20 | 20 of 20, 1 of 19, 20 of 20 | 19 of 19, 7 of 19, 8 of 8 |
| 400 | 12 of 19, 20 of 20 | 20 of 20, 12 of 20, 20 of 20 | 20 of 20, 4 of 20, 8 of 8 |
| 600 | 16 of 20, 3 of 20, 17 of 20 | 13 of 15, 4 of 20, 18 of 20 | 19 of 20, 2 of 20, 12 of 15 |
| 640 | 14 of 19, 0 of 11, 9 of 20 | 9 of 20 (two sects fallen) | 11 of 20, 3 of 19 (one fallen) |

- At thresholds up to 600, elders are most members.
- From 640 up, a founder whose limit falls short is no elder, and a sect with no elder admits no one, so sects fall.
- No threshold kept elders a minority while every standing sect kept one.

Options weighed:

- **A share of members.** A sect's elders are the `elder_share`, per mille, of its members of the highest best attainment, the lower index among equals, and at least one while it stands. The share is per-faction data in `Admission`, replacing `elder_attainment`.
  - The count scales with the sect's size, which ADR 0026 named as the reason to reject a fixed number of elders.
  - The election still counts elders, so the larger sect's master still has the more favour. That is the size ADR 0026 meant the vote to reflect, not the age of its members.
- Rejected: a fixed number of elders per sect. ADR 0026 rejected it, since a sect of three would have as many elders as one of thirty.
- Rejected: a threshold relative to the faction's talent limits. It moves the mirror of talent, and does not stop every member reaching it.
- Rejected: practice that slows near the limit. It changes ADR 0019's attainment rule for every discipline, not only for elders.

## Decision

A sect's elders are the `elder_share` per mille of its living members of the highest best attainment, the lower index among equals, and at least one while it stands. A faction with no admission has no elders. This supersedes ADR 0026's elders by an attainment threshold. The region gives every faction a share of a third, and its first founder an attainment of 120, so that it is its sect's master.

## Consequences

- Over 100 years with days of five ticks, every standing sect ends with at least one elder and at most a third of its members:
  - seed 24: 6 of 20 and 6 of 19, with Hwasan fallen and its manual lost
  - seed 2026: 6 of 20, 6 of 19 and 6 of 20
  - seed 7: 6 of 19, 6 of 19 and 2 of 8
- Masters changed 37, 43 and 40 times on those seeds, and the Orthodox leader was elected after a death 3, 2 and 3 times.
- An elder is a rank within its sect, so a weak sect still has a master to stand in an election, and a member of a strong sect may be no elder despite high attainment.
- The election's favour still scales with a sect's size. A rule of favour (ADR 0026) can change whom elders favour without changing who they are.

## Signs the decision was wrong

- Players expect elders to be recognised for strength, and a member of high attainment who is no elder because its sect is strong reads as wrong.
- The largest sect's master always leads, so the election reads as a member count, and favour needs to arrive before the share matters.
- Members' attainment stays a mirror of talent, so the elders are always the most talented, and practice needs its own revision.
