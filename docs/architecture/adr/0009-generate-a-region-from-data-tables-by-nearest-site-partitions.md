# ADR 0009: Generate a region from data tables by nearest-site partitions

> Status: accepted; its villages on free passable cells superseded by ADR-0024, which places them on free buildable cells; its villages placed among every corner that fits superseded by ADR-0076, which keeps every village's centre `apart` cells from another's
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The MVP is one region with three factions and six to eight villages, generated first (`docs/concept.md` §MVP scope). Issue #30 asks for a region generated from a seed into the inputs a `World` already takes, with terrain, faction territory and villages added, all defined by data and deterministic under ADR 0005: integers only, the seed the only randomness. Options weighed:

- **Content.** Terrain kinds, factions and village templates as three tables passed to the generator, with no code path per kind; a faction's hostility is the list of factions it attacks, so a new faction is one row. Rejected: an enum per kind with a branch per case, which needs code for every new kind; and a full hostility matrix in the data, which makes adding a faction an edit to every existing row.
- **Terrain.** Each cell takes the kind of its nearest site, sites at random cells with kinds drawn by weight. Rejected: integer value or Perlin noise, which needs a noise implementation, thresholds per kind, and more code for the same blob-shaped regions.
- **Territory.** The same nearest-site partition around one capital per faction, so each territory is one convex piece. A capital is the farthest from those already placed among a fixed number of random cells (Mitchell's best-candidate sampling, 1991), so territories come out of comparable size. Rejected: purely random capitals, which can sit side by side and leave one faction a sliver with no room for a village; and flood-fill growth from the capitals, which gives irregular borders at the cost of an ordering rule that decides ties.
- **Villages.** Owned in turn by each faction, so every faction has one while there are at least as many villages as factions. A village's corner is drawn among every corner where its square lies on free passable cells of its owner's territory, from one grid holding the owner of each free passable cell. Rejected: placing villages first and taking the owner from the cell, which can leave a faction without a village; and rejection sampling with an attempt limit, which fails on a seed that only ran out of attempts and cannot tell that from no room.

## Decision

A region is generated from a seed and three data tables. Terrain and territory each partition the grid by nearest site, with capitals spread by best-candidate sampling. Six to eight villages are owned in turn by each faction and placed at a corner drawn among every one that fits.

## Consequences

- A terrain kind, faction or village template is added by data alone; a test generates a fourth faction that way.
- Territory ignores terrain, so a border can cross impassable ground and a faction's territory can be mostly blocked.
- Placing a village visits every corner of the region, which is cheap at 96 cells square and grows with the region's area times a village's.
- A region without room for a village fails generation rather than giving fewer villages.

## Signs the decision was wrong

- A seed in play leaves a faction without room for a village, or with a territory too small to matter.
- Straight territory borders across mountains and rivers read as wrong in play, and borders need to follow terrain.
- A kind of content needs behaviour that a row of data cannot express, so the tables grow fields that only one row uses.
