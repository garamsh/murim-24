# ADR 0017: Make every NPC an individual and every army a roster

> Status: accepted
> Date: 2026-10-07

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The user decided on 2026-10-06 (issue #58):
- every NPC is an individual
- an army is the roster of the people conscripted into it, never a count
- a world's population never exceeds about 500, a hard upper bound and not a target

Several earlier decisions erase individual identity, which genealogy, talent, lifespan and succession (#41, #42) need:
- ADR 0005 merges an army's agents into an aggregate body away from the focus.
- ADR 0006 scales a strike by the agents its user stands for and the agents its disc covers.
- ADR 0007 spreads a tier's movement over its interval.

Their reason was 20,000 agents. Two issues fold in here:
- #52: people a 16-pixel sprite wide stood 72 sub-cell units, about 0.28 of a cell, apart.
- #47: a body ordered out of a standing crowd never left it, because separation steps were held to the path rule.

Options weighed:

- **A person.** A position, health, a cooldown and a home village, identified by its index for as long as the world runs. The dead keep their record and leave their roster, so they cost no per-tick work and stay out of the spatial index. Rejected: removing the dead, which renumbers everyone after them.
- **An army.** A roster of person indices, with an objective, a faction and skills. Every living person stands on exactly one roster. A village's residents stay an army, so residence is membership of its roster and a member's home is that village. Conscription moves one person between rosters; disbanding returns members with a home to its residents. Rejected:
  - a residence separate from armies, which is a second place a person can be and a second set of rules for who acts
  - keeping counts beside rosters, which is the drift the user ruled out
- **Levels of detail.** None. Every person acts on every tick. Rejected: keeping tiers as update rates. At 500 people the M1 worst case measured a tick p99 of 0.487 ms on the development Mac, against a budget of 4 ms, so no measurement justifies them. The focus that chose a tier goes with them.
- **The skill rule.** Each use hits every hostile person in its disc once, and every structure its disc reaches once. Rejected: keeping ADR 0006's multiplicities, which stand for people who are no longer aggregated. Its near-versus-far guarantee holds trivially, since there is no far.
- **Spacing and the crowd stall.** Separation is about a cell in the game's data (CELL_SIZE in the region and M1 scenario), as wide as a body is drawn. The movement rule:
  - A person steps away from anyone closer than the separation, the harder the closer, and otherwise along its flow field's pull.
  - Within its arrival ring, the square root of its rank in cells from the objective, it feels no pull, so an arrived army spreads out.
  - Within two cells of that ring it counts as standing still. People on the move do not give way to people standing still, who step aside across the mover's way, so a standing crowd parts.
  - Any step may enter a cell the flow field reaches.
  - Rejected:
    - Movers ignoring all neighbours, which lets a marching column collapse onto one path.
    - Weighting the pull against the pushes, which at every weight tried (1, 32, 64 and 256) either let a column close to 137 sub-cell units or left a person stuck at a crowd's edge.
    - A sidestep for a mover held back, which no scenario needed once standing people step aside.

## Decision

Every NPC is a person with a stable identity, and every army is a roster of the living people in it. People move between rosters by conscription and disbanding, and a world holds at most 500 living people. Every person acts on every tick, and a skill hits each hostile person in its disc once. People keep about a cell apart, and people standing still step aside for people on the move. This supersedes ADRs 0005 and 0007, ADR 0006's scaling by count and agents covered, and ADR 0011's focus set by the driver.

## Consequences

- A person can be followed through conscription, battle and disbanding, which genealogy and succession need. A test does so.
- Combat no longer depends on the focus, so the near-versus-far matchup tests are replaced by the same matchups run once.
- ADR 0005's flow fields, spatial hash and fixed-point numbers stay; its tiers, aggregates and focus go.
- The M1 scenario is rescaled to 500 people, every one in one battle, as the worst case. The pinned determinism hash changed.
- A tick costs about an eighth of the budget on the development Mac. A phone several times slower than it would still fit, but no phone has been measured.
- People on screen are as many as people in the world, drawn one sprite each, so the frame budget scales with the population bound.

## Signs the decision was wrong

- A world that respects the bound still misses the tick or frame budget on a target device, and an update-rate level of detail has to come back.
- Crowds deadlock in play: two groups on the move hold each other up, or a crowd never settles.
- Rosters and homes drift from what players see, because a person is somewhere the rules do not record, such as in two armies or in none.
