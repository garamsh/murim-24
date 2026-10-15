# ADR 0048: Scale each device by a quality profile, and measure the population bound on the lowest

> Status: proposed; its bound measured on ADR 0047's 4,096 map superseded by ADR-0070, under which it is measured on the 8,192 map; its standard profile's live cap of 1,000 superseded by ADR-0077, the later, which sets the live tier's budget at #216's 5,000 on the standard profile, as `simulation.md` records, high's cap held to standard's until measured
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-09, in issues #201 and #173:
- Keep the fixed hand-made map of 4,096 cells (ADR 0047), and scale per device without changing content. The world, its map, its villages and its population are the same on every device.
- A quality profile, chosen by the device or the player, as rules and config data. It sets:
  - the render distance and the chunk prefetch radius
  - the live tier's radius and the stepped tier's stride
  - effect quality
- Determinism tests run with a fixed profile. Different profiles may make far outcomes differ between devices, which is accepted for an offline game, and this is to be recorded.
- 2,000 to 5,000 people would leave a 4,096 map feeling deserted. The population bound is measured over a far wider range, such as 10,000 to 50,000 with the far tier, on the lowest profile the game supports.

ADR 0042 gives tier radii, the live cap and the stride as rules data, one set for the world. ADR 0045 measures the bound from 2,000 to 5,000 on the 512-cell world and makes it one number for every platform. It leaves a slower mobile device to be decided at the port: lower the bound for all, narrow the tiers for all, or accept worlds that differ by platform. The owner has now chosen per-device tiers with the same content.

What the design needs from the bound, estimated from ADR 0047's map:
- About half of the 3,840-cell square inside the frame is buildable ground, about 7.4 million cells.
- Settlements in the heartland about 150 cells apart, 80 seconds' walk, is one per 22,500 cells, about 330 settlements.
- At #169's larger villages of 50 to 100 people, plus cities of several hundred, that is roughly 20,000 to 40,000 living once growth has filled the land.

ADR 0042's measurements at 5,000 people put the daily rules at 2.0 to 6.2 ms on a day's tick after the second day. Scaled linearly to 50,000, that is 20 to 62 ms, past a frame.

Options weighed:

- **What a profile holds.** Only what changes cost and not content. Chosen:
  - the view's chunk prefetch margin and chunk builds a frame (#200, #204)
  - the live radius, the live cap, the stepped radius and the stride (ADR 0042)
  - effect quality

  Starting values:

  | Setting | Low | Standard | High |
  |---|---|---|---|
  | Chunk prefetch margin (pixels) | 32 | 64 | 128 |
  | Chunk builds a frame | 1 | 1 | 2 |
  | Live radius (areas) | 1 | 2 | 3 |
  | Live cap (people) | 500 | 1,000 | 1,500 |
  | Stepped radius (areas) | 3 | 4 | 6 |
  | Stride (ticks) | 8 | 8 | 4 |
  | Effects | reduced | full | full |

  Standard is ADR 0042's values. Low keeps a live cap of 500, the M1 battle on screen. The implementing pull requests measure each profile and may change these.
  - Rejected: a profile that also sets the population bound or the map's detail. The owner asked for the same world everywhere.
- **Where the profile lives.** With the player's settings, beside the save slot, not in the world. The world is given the profile's tier values at creation and at every load. They are not saved with the world and not folded into the hash. The device picks a default and the player may change it. Chosen.
  - Rejected: saving the profile in the world. A save moved to another device would carry tiers that device cannot run.
- **Determinism.** Every test that steers an army in a world with tiers runs on the standard profile. The same orders, steers and profile give the same hash. Two profiles give the same live outcomes near the player while the live and stepped tiers cover the same areas, and may give different far outcomes, within ADR 0042's consistency margins. Chosen, as the owner accepted.
- **The bound's range.** N from 10,000 to 50,000 in steps of 5,000. Chosen. The bound stays one number for every device, since births depend on it and a profile changes only how far away people are simulated in detail.
- **The bound's profile.** Measured on the low profile, the lowest the game supports, so every device runs the same population. Chosen.
- **The daily rules at this size.** They are spread over the day's ticks before the bound is measured, not only if the day's tick misses a frame. Each village and person is given a fixed phase in the day by index, so the order stays deterministic. Chosen, since the projection already misses a frame.

## Decision

A quality profile, kept with the player's settings, sets the view's chunk prefetch margin and builds per frame, the tiers' live radius, live cap, stepped radius and stride, and effect quality. It starts as low, standard and high, with standard at ADR 0042's values. The world, the map and the population are the same on every profile. The world is given the profile's tier values on creation and load. They are neither saved nor hashed, and tests run on standard. Far outcomes may differ between profiles, which is accepted.

The population bound is measured on the low profile, on ADR 0047's map, for N from 10,000 to 50,000 in steps of 5,000. ADR 0045's rules hold otherwise: a median p99 within 4 ms over three runs and a day's tick within a frame, on the development Mac and Linux when available. The daily rules are spread over the day's ticks first.

This supersedes ADR 0042's single set of tier values for every device, and ADR 0045's range of 2,000 to 5,000, its 512-cell world and its one tier set for every platform.

## Consequences

- Settings gain a quality profile.
- **`make bench`** measures the Central Plains scenario on the low profile. Tests run on standard. A test shows that changing the profile at load keeps every person, village and structure, and changes only the tiers.
- **The scenario** places N people across the starting cities and towns of ADR 0047's map, or of its test map until the first map lands. The steered body goes in the most crowded, with the M1 battle beside it.
- Spreading the daily rules changes when in a day a village's rules run. The pinned hash changes, and `make sweep` is rerun in that pull request.
- The bound sets how large villages may grow and how many there are, so #169's and #187's growth data are tuned to approach it, not stop short.
- A mobile port adds a profile below low if needed. Since the bound is measured on low, such a profile must hold the same N, or the bound is measured again on it.

## Signs the decision was wrong

- Players on different profiles find the world's history visibly different. Examples are a war won on one profile and lost on another, often enough that they name it.
- The low profile's live radius of one area is narrower than the view at some zoom, so people at the edge of the screen stutter at the stepped rate.
- The bound measured on low is below 20,000, so the map stays emptier than the design needs.
- Spreading the daily rules makes a village's day visibly uneven, such as a harvest one village sees a second after its neighbour.
