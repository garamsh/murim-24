# ADR 0078: Remember each village's claim centre with its last-seen state

> Status: accepted
> Date: 2026-10-13

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #168 requires a place's territory to remain as last seen. ADR 0073 remembers a village's owner, whether it was peopled and its stage, but reads its claim centre from the live world. A village's seat is its resident army's objective, which can change through an order or rebuilding under another army.

On seed 0, after the player's first chart, ordering resident army 0 toward village 1 changed 90 remembered territory cells while village memory and chart revision stayed unchanged. The issue's implementing tests reproduce the failure with a watcher out of sight and a control in sight.

Alternatives:
- Remember the claim centre with the village: chosen; it keeps every mutable input to a remembered claim together.
- Make live village seats immutable: rejected here; it changes the simulation's live territory and NPC rules beyond this issue.
- Store a holder per seen cell: rejected for the cost ADR 0073 records; remembering the missing input meets this case within its village-based record.

## Decision

Each remembered village includes its claim centre as last seen, beside its owner, whether it was peopled and its stage. Remembered borders use that centre, and the record changes only when a structure of that village is seen again; changing the centre raises the chart revision even when the other fields stay the same.

This supersedes only ADR 0073's remembered village record and its use of a live claim centre. Its sparse seen cells, sight, soul inheritance, stage, fog, map window and refusal of older released saves stand.

## Consequences

- The centre is saved and hashed with the memory; a load refuses a centre outside the map, including negative coordinates.
- The first chart save format remains version 12: it is unpublished, and the centre is included before its first release, as the PM decided on 2026-10-13.
- Live territory and every NPC outcome are unchanged.
- On macOS arm64 with Apple Clang 21.0.0 (`clang-2100.1.1.101`), C++17, the built record occupies 24 bytes, and each optional village slot 32 bytes: 312.50 KiB for 10,000 slots before vector capacity and allocation overhead. A block occupies 132 bytes, its area index beside 128 bytes of bits; the area lookup costs 4 bytes per area, and a chart object 96 bytes before its allocations. These replace ADR 0073's storage estimates for the built record.

## Signs the decision was wrong

- A village's claim gains another mutable input that remembered borders still read live; the record must then include that input.
- Players require each territory cell to refresh independently of sighting its village; then a village-based record does not preserve the knowledge they expect, and the per-cell holder is worth its cost.
