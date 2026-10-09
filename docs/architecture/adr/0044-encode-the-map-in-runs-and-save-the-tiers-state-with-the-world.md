# ADR 0044: Encode the map in runs, and save the tiers' state with the world

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

A save holds the world's whole state, its data included, in the core's own encoding (ADR 0030). The map is written a byte per cell, and every person ever born is kept, the dead included (ADR 0017). The 512-cell world (ADR 0041) has sixteen times the cells, and the bound is to rise to between 2,000 and 5,000 living (ADR 0045). The tiers add state of their own (ADR 0042): each area's hysteresis, each far army's anchor and its progress along its coarse path.

Measured on the development Mac (Apple M1, 8 GB, macOS, on AC power, sharing it with other workers' test runs at a load average of 7 to 9) by the `measure` program on branch `evidence/central-plains`. It saves through an in-memory store, using a copy of `core/` at `origin/main` 1d943f0 with `MAX_PEOPLE` and `REGION_SIDE` raised. The generated region of seed 2026 had its villages' people raised and ran 1,500 ticks:

| Side (cells) | People | Save (bytes) | Save (ms) |
|---|---|---|---|
| 128 | 500 | 134,080 | 1.2 |
| 128 | 2,000 | 443,080 | 2.5 |
| 128 | 5,000 | 1,062,055 | 5.3 to 9.7 |
| 512 | 500 | 379,840 | 13.5 |
| 512 | 2,000 | 688,840 | 34.1 |
| 512 | 5,000 | 1,306,840 | 43.9 |

Seed 7 at 128 cells gave saves within 600 bytes of seed 2026's. So:
- A person costs about 206 bytes.
- The map costs one byte per cell: the 512-cell saves exceed the 128-cell ones by 245,760 bytes, 512² − 128².
- The map's rows held 1,833 runs of one kind at 512 cells, and 464 at 128.

Lifespans of 45 to 75 years at a bound of 5,000 leave roughly 80 dead a year once the world is full. A century adds about 8,000 records, about 1.7 MB at that size.

Options weighed:

- **The map.** Rows are written as runs, a footing and a length, in place of a byte per cell. Today's terrain would take about 1,833 runs. ADR 0041's rivers and roads will add more, but still far fewer than 262,144 cells. Chosen.
  - Rejected: regenerating the map from the seed on load. A save would then depend on the build's tables, which ADR 0030 rejected.
  - Rejected: a general compressor such as zlib in the core. It is a dependency the core has none of, for a gain the runs mostly give. A store adapter may still compress its bytes, since the port carries bytes.
- **The dead.** A dead person's record drops what only the living use: position, health, cooldown, qi, hunger and a pending cast. What genealogy, souls and rumours read stays. Chosen.
  - Rejected: forgetting the dead past some generations. Genealogy and a soul's history read them, and ADR 0017 keeps every identity for as long as the world runs.
- **The tiers' state.** Each area's tier hysteresis, each far army's anchor and its progress along its coarse path are saved and checked on load like any reference, and folded into the hash. The coarse graph, the components and the area fields are derived, so they are rebuilt on load as flow fields are today.
- **Natural objects (#187).** Each object is saved as its kind, its cell and its stage, at about 8 bytes. That is at most 65,536 objects under ADR 0041's bound per area, about 512 KB, and less where the land holds fewer. Rejected: deriving objects from the seed and saving only changes, since growth and spread move every count away from what the seed gave within years.
- **Versioning.** The format version rises, and saves of the 128-cell region are refused as `UNSUPPORTED_VERSION`, as ADR 0030 refuses every other version. Rejected: a migration from the 128-cell region. Its map cannot become the 512-cell world.

## Decision

A save writes the map's rows as runs of one footing. A dead person's record leaves out the state only the living use. The tiers' hysteresis, the far anchors and the far progress are saved, checked and hashed, while the coarse graph and the area fields are rebuilt on load. The format version rises, and older saves are refused.

## Consequences

- A save of the 512-cell world at 5,000 living is projected at about 1.1 MB, its people's 1 MB and runs in place of 262,144 map bytes, with up to about 512 KB more for natural objects. The dead add up to about 1.7 MB a century before their records shrink.
- Saving at 512 cells took 13.5 to 43.9 ms, measured, mostly in the map. Runs shorten it, and the implementing pull request measures saving and loading against one frame on the development Mac.
- Loading regenerates the region the view draws from its seed, as ADR 0030 does. That cost is ADR 0041's generation time.
- Every pull request that adds state to the tiers raises the version in the same pull request, so the round-trip test keeps reading every field.

## Signs the decision was wrong

- Saves of a long-lived world grow past what an autosave every three in-game days can write without a hitch the player sees.
- Rivers, roads and generated detail cut rows into so many runs that the map costs nearly a byte per cell again.
- Players ask to keep a 128-cell world, and refusing its saves loses them progress they valued.
