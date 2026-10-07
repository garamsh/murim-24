# ADR 0025: Start each village from households of married couples

> Status: accepted
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The concept starts the world from a few people who grow into a civilisation (`docs/concept.md` §Growing civilization). Issue #94 asks for at most four households per faction at the start, each a founding couple. Over 100 years on seeds 24, 2026 and 7, the living must at least triple and the villages at least double, every faction must keep a peopled village, and the living must stay within 500.

The world could not start with a couple. Every founder started single, and everyone an army started with shared that army's family (ADR 0021). Marriage needs another family, so the founders of one village could never marry each other. Nothing in `Life` or `Kinship` changed that. Tuning the data alone, the template residents only, over 100 years with days of five ticks:

| Founders | Living, seeds 24 / 2026 / 7 | Villages | A faction left without a peopled village |
|---|---|---|---|
| 2 per village | 12→17, 14→61, 14→12 | unchanged | on all three seeds |
| 4 per village | 24→53, 28→135, 28→77 | 6→6, 7→8, 7→7 | on seed 24 |

Options weighed:

- **Households in the village's data.** A village names how many households its residents start as. The first two residents of each are born a man and a woman, married to each other, each heading a family of their own, by the one birth rule. The rest are born as before, single, into the army's family. Templates give households per building in place of residents.
  - Rejected: marrying founders of one family to each other, which breaks marriage's rule that partners are of different families.
  - Rejected: a family per founder with no marriages at the start, which leaves a village of two waiting years to wed. A founder's partner may also be its only neighbour of the other sex, or none.
- **Settlers in households.** Measurement did not need it. A fixed count of 6 settlers founds from villages of a few households.

## Decision

A village's founders start as households, from template data: in each, a married man and woman, each heading a family of their own. One rule serves every village. Founders outside a household are born into their army's family, as before. This supersedes ADR 0021's rule that each family is one per army the world starts with, for the families of households.

## Consequences

- The region starts each village with one household: the hamlet in its first house, the village in its hall. Each faction holds two or three villages, so it starts with two or three households.
- The growth data is tuned to start small:
  - founders 16 to 28 years old (from 16 to 40)
  - a marrying neighbourhood of 80 cells (from 48)
  - pressure at a fifth of a village's cap (from four fifths)
  - 6 settlers (from 10)
- Over 100 years with days of five ticks:

  | Seed | Living | Peopled villages | Fewest factions peopled | Peak |
  |---|---|---|---|---|
  | 24 | 12 → 97 | 6 → 17 | 3 | 98 |
  | 2026 | 14 → 123 | 7 → 18 | 3 | 124 |
  | 7 | 14 → 125 | 7 → 19 | 3 | 125 |

  - On seeds 1 to 6 and 8, the living rise 6 to 11 times, every faction keeps a peopled village, and the villages, founded ones included, rise 2.9 to 4.9 times. The peopled villages double on seeds 1, 2, 3 and 5, but end at 11 of 6, 15 of 8 and 13 of 7 on seeds 4, 6 and 8.
  - On seed 9 the living fall from 12 to 5, and a faction loses its last peopled village.
  - With households but the old growth data, villages never multiply on any of the three seeds.
- Tests that measure combat pacing and caps start from villages as full as grown ones, since the starting villages no longer hold the 12 and 26 defenders those targets were set for.
- A child who founds a village heads a branch under its father's surname, so a child need not share its father's family index.

## Signs the decision was wrong

- Seeds like 9, where a faction dies out, prove common in play, and a faction needs a guaranteed household or a refuge rule.
- One household per village reads as empty in play for the first decades, and villages need more founding households, at the cost of the four-per-faction limit.
- The wide marrying neighbourhood makes marriages between distant villages read as wrong, and kinship needs migration of a spouse instead.
