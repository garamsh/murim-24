# ADR 0027: Bear each child of the sex its village's unmarried lack

> Status: accepted
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #113 asks that every generated world grow from its founding households. Every faction must keep a peopled village on at least 48 of seeds 0 to 49, and the living must at least double on at least 45, over 100 years with days of five ticks. The fix must be data or an approved rule, with no seed-specific branch.

With one household per starting village (ADR 0025), each faction starts with two or three households. Under ADR 0021, every child's sex is drawn at even odds. On the region's data at 4739a29:

- Every faction kept a peopled village on 42 of 50 seeds, and the living doubled on 48.
- Seed 9 fell from 12 living to 5:
  - 17 of its first 18 children were daughters, and 6 of all 26 were sons.
  - No man was free to marry them, and all 33 deaths were from age.
  - No village was ever founded.
  - The Unorthodox's two hamlets emptied in year 92.
- Seeds 15, 23 and 35 died out the same way. On seeds 20, 25, 32 and 42, raids also killed many of a small faction's people.

Options weighed, every faction peopled out of seeds 0 to 49:

| Option | Every faction peopled | Living at least doubled |
|---|---|---|
| None (4739a29) | 42 | 48 |
| Marrying neighbourhood 90, 100, 110 or 128 cells (from 80) | 44, 47, 44, 45 | 49 |
| Neighbourhood 100 with surer marriage, later marrying, likelier or closer births, or 15 or 20 raiders | 42 to 47 | 49 to 50 |
| The Unorthodox building first | 40 to 47 | 48 to 49 |
| Nine starting villages, three per faction (from six to eight) | 41 to 47 | 49 to 50 |
| Two households per village, against ADR 0025's four per faction | 45 to 47 | 49 to 50 |
| No raid on a faction's last peopled village | 42 | 48 |
| An unmarried person with no partner in reach moving to the nearest village of its faction with room and one free to marry | 43 | 48 |
| A child of the sex its village's unmarried lack | 50 | 50 |

- Data alone stops at 47: the more a region grows, the sooner raids empty a small faction.
- Moving to marry was preferred because people can see it, but it measured 43, and 42 on seeds 50 to 99.
- On seeds 50 to 99, held out from the tuning:
  - every faction stayed peopled on 48 with balanced sexes, against 44 without
  - the living doubled on all 50 either way

## Decision

A child born to a married couple takes the sex fewer among the living unmarried whose home is its village and who are not past marrying age, children included. When they are as many, or the kinship data does not balance sexes, its sex is drawn at even odds. The one birth function bears it as before, given the sex. This supersedes ADR 0021's rule that every birth draws its sex at even odds, for children born where kinship balances sexes.

## Consequences

- `Kinship` gains `balanced_sexes`, off by default. The region's data turns it on.
- The founders of households keep their given sex (ADR 0025). Others the world starts with, and souls born again, draw theirs as before.
- A village of daughters bears sons until its unmarried are even. Sex ratios no longer swing far enough to end a faction's line on seeds 0 to 49.
- `make sweep` lives each seed of a range for a number of years and prints the table above, so the next growth tuning measures the same thing.
- The pinned determinism hash is unchanged: the M1 world has no kinship.

## Signs the decision was wrong

- Play shows villages bearing sons and daughters in turn, and the even balance reads as a hand on the scale. Moving to marry should then be measured again, together with raids that spare a small faction.
- Factions still die out on new seeds or new data from raids, not from age. The cause then lies in growth or raiding, not in births.
- Sects (ADR 0026) or other rules take unmarried people out of their villages, so the count by home stops meaning who can marry there.
