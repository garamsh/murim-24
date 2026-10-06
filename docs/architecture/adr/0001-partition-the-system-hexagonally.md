# ADR 0001: Partition the system hexagonally

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The game is a world simulation the player lives inside (`docs/concept.md`). The same rules must hold whether the simulation is rendered, running far from the player, or running in a test with nothing attached, and the player is governed by the same rules as every NPC. The rendering and input technology runs on a different clock from those rules: ADR 0002 picks the engine, and that choice is expected to be tested by measurement before it settles. Layered partitioning was rejected because it divides by technical role and leaves the simulation's subject matter spread across every layer; domain partitioning was rejected for now because no domain exists in code yet to draw a boundary around.

## Decision

The system is partitioned hexagonally: the simulation is the application inside, and every technology — the engine's rendering, input and audio, the filesystem for saves, the test harness — is an adapter reaching it through a port. Whether the inside is itself partitioned is left open in `docs/architecture/structure.md` until the project meets the question.

## Consequences

- The simulation runs and is tested with no engine attached, so balance and performance can be measured headless.
- Replacing or adding a technology changes an adapter and not the rules.
- Every conversation with the outside is written twice, as a port and an adapter.

## Signs the decision was wrong

- A technology's vocabulary appears in the application's own signatures — a framework type, a column name, a status code.
- Adding or replacing a technology still changes the application.
- Most ports have one adapter, and the tests substitute none of them.
- The same rule is implemented in two adapters, and a fix has to be made in each.
- Ports are added and dropped with every feature, because they were drawn per use case rather than per conversation.
- Code cannot be found by subject matter, because everything the system decides sits in one undivided inside.
