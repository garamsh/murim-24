# ADR 0010: Keep the core's inside undivided

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

`structure.md` left open whether the application is itself partitioned, to be settled when the core held more than one subject area. With world generation (ADR 0009, PR #34) it holds two: the simulation and the region generator. Rule 7 of `docs/architecture/README.md` therefore asks for a side now. At this revision `core/` holds eight library and tool source files beside their tests, flat in one folder. `region.hpp` includes `world.hpp` and `map.hpp`; no simulation file includes `region.hpp`. `random.hpp` is shared by the generator and the M1 scenario. Options weighed:

- **Undivided**, as in Cockburn's statement of the pattern (*Hexagonal architecture*, 2005): the application is one inside, its files flat in `core/`, and the only boundary that binds is the one between the application and its adapters.
- **Concentric rings**, as in Palermo's Onion Architecture (2008) or Martin's *Clean Architecture* (2017): the inside is split into layers with every reference pointing inward. Rejected: the generator already depends only on the simulation's types and nothing depends back, so a ring rule would forbid nothing the code does and add a rule every change has to be checked against.
- **A folder per subject area** under `core/`. Rejected: two subject areas of a few files each do not yet hold more than one thing per folder, and `simplicity.md` makes a division a response, not a plan.

## Decision

The application's inside stays undivided: no rings and no folder per subject area, with the core's files flat in `core/`.

## Consequences

- A new core unit is a file pair in `core/`, and the build declares it there; nothing decides which ring or folder it belongs to.
- Which subject area depends on which is not enforced by structure. It holds only as long as the includes keep it, and a review reads them.
- Adapters keep the only structural boundary: the port rules in `structure.md` are unchanged.

## Signs the decision was wrong

- A change has to edit the internals of both the generator and the simulation together for one reason, so the two are no longer separable by reading their includes.
- A simulation file comes to include a generation file, or the two depend on each other.
- A third subject area arrives, such as orders or village life, and `core/` holds files a reader cannot place by name.
- A file belongs to neither subject area and is not shared infrastructure like `map` or `random`.
