# ADR 0084: Make a person's wander period one hour of the world's day

> Status: proposed
> Date: 2026-10-18

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

ADR 0081 moves a person awake to a new spot "every 90 ticks". Building its second step (#376, #387) gave that period a second job: each person's period begins at its own phase, and a person reads its band, its place and whether it sleeps at the hour its current period began. A village's people therefore change band, wake and fall asleep over a period's ticks, rather than all in the tick of the hour. On the hour, the live scenario of 5,000 found about 2,000 errands again in one tick, over the 4 ms budget.

With 90 fixed, the period stops tracking the day wherever the day is not the base pack's:
- **A short day.** `hud/dialogue_test.gd` lives seed 2026's region on 5-tick days. There a 90-tick period is 18 days, so each person holds one band for 18 days, chosen by its own phase. A village's people stop sharing the square, fewer meet, and the person who has heard the most holds 5 rumours instead of more than a screenful, failing the test.
- **A constant standing in for a rule.** The 90 is a tuning number standing in for something the world already defines, the hour. `simulation.md` §The principle calls such a constant a defect.

The PM ruled on #387 that the period is the hour.

## Decision

A person's wander period is one hour of the world's day: `ticks_per_day / 24` ticks, at least one tick. That is 75 ticks at the base pack's 1,800-tick day and 1 tick at a 5-tick day, so the schedule keeps a day's rhythm at any length of day.

A person's phase is drawn within that period, and its band, place and sleep are read at the hour its current period began. Each person changes band within an hour of the band's own hour.

This supersedes ADR 0081's fixed 90 ticks between a person's wanders. The rest of ADR 0081 stands.

## Consequences

- At the base pack's day, errands are found again every 75 ticks rather than every 90, about 20% more often. `make bench` measures whether the live median stays within ADR 0081's 0.2 ms.
- Where an hour is a single tick, every period begins on the hour and no phase can spread the change. A village's people then change band together, as they did before #387.
- A test of a schedule names hours, not a tick count, so it holds at any length of day.

## Signs the decision was wrong

- The live median rises more than 0.2 ms over a schedule-free world at the base pack's day, from the shorter period alone.
- A day so short that an hour is one tick shows a tick over budget from every resident changing band at once.
