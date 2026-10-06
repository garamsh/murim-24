# ADR 0007: Move each army at its own phase within its tier, and strike on the tier's common tick

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0005 has a tier's armies act every `interval` ticks, and every army in a tier acted on the same tick. The M1 benchmark (#20) measured a tick p99 of 4.396 ms against the 4 ms budget, and every 4th tick, when the middle tier's 4,500 agents moved together, had a median of 3.4 ms against 0.22 ms on other ticks. Spreading a tier's work across its interval is a change to when armies act, so it is recorded here. ADR 0006 keeps a battle's winner the same in every tier, and any change to when bodies strike has to keep that. Options weighed:

- **Phase as data, for movement only.** Each army moves on the ticks where its tick count plus its phase is a multiple of its tier's interval. The phase is the army's index, so a tier's armies spread over the interval's ticks with no branch per army or tier. Strikes stay on the ticks the interval divides, with every army of the tier. Uses per step and their carried remainder (ADR 0006) are unchanged, so tier-invariance holds as before.
- **Phase for strikes too.** Rejected after measuring it. Far from the focus, a whole step of uses lands at once. The army whose phase comes first then hits a full step before the other answers, and the answer comes from an army already reduced. In the near-versus-far area-skill matchup, the area side kept 772 strength near and 1,168 far, outside the tests' tolerance of 300. On the common tick, both armies' steps land together, after every body has struck.
- **Phase for strikes, with damage landing on the target's own phase.** Rejected: it adds a pending-damage value to every body that a collapse would have to merge and the hash to cover. It would spread combat work as well, but the measured spike came from movement.
- **A phase drawn from the seed.** Rejected: the index is already deterministic data. A drawn phase spreads a tier no better and costs a random draw per army.

## Decision

Each army moves at its own phase within its tier's interval, set by its index. It strikes on the ticks the interval divides, together with every army of its tier, as before.

## Consequences

- A tier's movement work spreads over the ticks of its interval. In the M1 scenario that brings p99 from 4.32 ms to 1.95 ms; part of the gain comes from cheaper neighbour queries in the same change (`simulation.md`).
- When a battle is won, and the near-versus-far outcomes, are unchanged, because strikes keep their tick.
- Armies of a tier no longer stand still or move in step with one another between strikes. An army can strike from a position its own phase reached before the tier's common tick.
- The pinned determinism hash changes, because positions now change on different ticks and the phase is part of the hashed state.

## Signs the decision was wrong

- A tier's combat work, rather than its movement, sets the p99: for example, a large middle-tier battle spikes on the ticks the interval divides.
- Armies of one tier visibly move out of step in a way players notice, such as a formation of several armies drifting apart.
- A rule starts to depend on two armies of a tier moving on the same tick, and needs the phase removed or matched.
