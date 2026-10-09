# ADR 0045: Set the population bound by measuring a Central Plains scenario on PC

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The user decided on 2026-10-08 (issue #173, `docs/concept.md` §World and scale) to raise the population bound after measuring. The new bound is somewhere between 2,000 and 5,000: the largest that keeps a tick's p99 within the 4 ms budget with the level of detail. On 2026-10-09 the user scoped the measurement to PC, macOS and Linux when available. The mobile bound is measured at the port and may be lower.

Today's budget is measured by `make bench` on the M1 scenario: `MAX_PEOPLE` people, 500, all in one battle on a 128-cell map. That is the worst case of a world with no levels of detail (ADR 0017). With ADR 0042's tiers the worst case changes. A battle on screen sits in the live tier, and the people beyond it cost by their tier. The daily rules run for everyone, and growth rebuilds areas of the coarse graph (ADR 0043). One battle no longer stands for the world.

ADR 0042 records the full-detail figures this projection uses, from the `measure` program on branch `evidence/central-plains`:
- about 2.7 µs a person a tick fighting
- about 1 µs a person a tick living in a village
- daily rules of 2.0 to 6.2 ms on a day's tick at 5,000 after the second day

Projected with the tiers, at 5,000 people:
- live: 1,000 fighting, about 3.0 to 3.8 ms p99, measured at 1,000
- stepped: 1,500 living, at an eighth of 1 µs, about 0.2 ms
- far: no per-tick work for its people

That is about 3.2 to 4.0 ms, at the budget. At 2,000 people the same projection leaves room. The projection does not include the cost of the tiers themselves, area fields or far fights, so only a measurement can set the bound. Options weighed:

- **The scenario.** A Central Plains scenario:
  - the generated 512-cell world of seed 2026, its villages' people raised to N on the open cells nearest each village's centre, as the measurement for ADR 0042 raised them
  - the player's body born and steered in the village whose area holds the most people
  - the M1 battle of 500 people placed beside it in the live tier, counted in N

  Chosen. It is the worst case a player meets: a battle on screen in the most crowded place, with the rest of the world at the bound.
  - Rejected: the M1 battle scaled to N. With tiers, no one would place N people in one live battle, and the live cap would shrink the live tier instead.
  - Rejected: a world grown to N by living centuries. It is closer to play, but takes hours per run, and N would be what growth reached rather than what was asked.
- **The sample.** The scenario ticks for one in-game day of 1,800 ticks after 600 ticks of warm-up, so the daily rules fall inside the sample. It reports the p50, the p99, the maximum and the day's own tick. Rejected: today's 600 ticks, which hold no day's tick.
- **The rule.** For N from 2,000 to 5,000 in steps of 500, three runs each, the bound is the largest N whose median p99 is within 4 ms and whose day's tick is within 16.7 ms, one frame at 60 frames a second. Where the day's tick misses a frame, the daily rules are spread over the day's ticks before N is lowered.
- **The device.** The development Mac, an Apple M1 with 8 GB on macOS, on AC power with no other build or benchmark running, in a Release build. Linux is measured the same way when a machine is available, and the bound is the smaller of the two. Mobile is measured at the port.
  - The bound is one number for every platform. A world's births depend on it, so a save would hash differently on two platforms with different bounds. If mobile cannot hold it, that is decided at the port: by lowering it for all, by narrowing the tiers in rules data for all, or by accepting worlds that differ by platform.
- **What records the bound.** The measuring pull request sets `MAX_PEOPLE` and records each N's figures in `simulation.md`. Its own ADR supersedes ADR 0017's bound of 500 with the value measured. `make bench` runs the Central Plains scenario at the bound beside the M1 scenario. The M1 scenario keeps 500 people, as the live tier's battle, and no longer holds `MAX_PEOPLE` people.

## Decision

The population bound is the largest N from 2,000 to 5,000, in steps of 500, at which a Central Plains scenario holds both:
- a median p99 tick within 4 ms over three runs
- a day's tick within one frame

The scenario is the 512-cell world at N people, with the steered body in the most crowded village beside the M1 battle of 500, timed over a whole day. It is measured on the development Mac, and on Linux when available, before the bound is set. It is one bound for every platform. The mobile measurement comes at the port, where a lower bound there is decided for all platforms or accepted as a difference.

## Consequences

- The bound cannot be set until the tiers, the coarse graph and the 512-cell generation are built. It stays 500 until then.
- `make bench` grows a second scenario and takes longer. It still gates nothing in `make ci`.
- The growth data, villages' density and caps are retuned with `make sweep` after the bound is set, so the world approaches the bound rather than stopping far short of it.
- A phone too slow for the bound forces a decision for all platforms at the port, not a quiet per-platform constant.

## Signs the decision was wrong

- Play at the bound misses the budget in a place the scenario does not cover, such as a city larger than any village or a war that gathers several battles into the live tier.
- The bound measured is far below 2,000, which would show the tiers cost more than projected, or that the live cap needs to be smaller than a battle on screen.
- The mobile port cannot hold the bound, and the decision then left open costs more than a per-platform bound would have.
