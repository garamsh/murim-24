# ADR 0011: Steer a body by the orders every army follows

> Status: accepted; its focus set by the driver superseded by ADR-0017, and its rejection of a one-shot strike by ADR-0034
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The player steers one body of the world with no rule of its own (`docs/concept.md` §Identity; issue #36). Before this decision an army's intent was fixed at creation: it marched toward its spec's objective, and each strike drew one of its skills by the seed (ADRs 0005 and 0006). The player's intent comes from input instead. `structure.md` makes that intent a port, with Godot input as one adapter and a test harness as the other. The focus already set each army's tier. Options weighed:

- **The shape of intent.** An order: an objective cell, and the skills, by index among the army's own, that a strike draws among by the seed. An army created from data follows its objective with all its skills, which draws exactly as before. "Use skill N" is the order whose skills are N alone. Rejected: a player flag, or a player body type with a code path of its own, which `docs/concept.md` rules out and which every later rule would have to branch on; a one-shot "strike now" command, which would need a resolution path outside the tier's strike tick that ADR 0006 keeps tier-invariant; and a draw per army rather than per body, which changes how often each skill is used and so every battle's outcome.
- **The port.** `World::order(army, order)`, validated where it enters and applied from the next tick. A new objective rebuilds that army's flow field. Rejected: orders queued inside the world with their tick, which only a replay needs and which a driver can keep outside.
- **The steered body.** An army of one agent, so it stays one body in every tier. It is born into a village by `born_in`, which the rebirth task can reuse. Rejected: steering a resident army, which moves many agents at once. The first version placed the agent at the village's centre, among the residents; the separation rule held it there whatever it was ordered, so it is placed at the village's corner.
- **The focus.** The driver sets the focus on the steered army's lead body before each tick, through the existing `set_focus`. Rejected: a focus that follows an army inside the core, which would be a second kind of focus beside the free camera that the M1 view needs.

## Decision

Every army follows an order of an objective cell and the skills it strikes with. An NPC's order comes from its data, and any driver may give an army another through `World::order`, with no record of who steers it. The player's body is a one-agent army born into a village, and its driver sets the focus on that body's lead before each tick.

## Consequences

- An NPC behaviour written later produces orders and goes through the same resolution as the player's input.
- The order is part of the world's state and its hash, so the pinned determinism hash changed.
- A replay is a log of orders with the ticks they were given before, played through the same focus rule. A driver that sets the focus another way does not replay.
- Giving an order that changes the objective costs one flow-field search over the map.

## Signs the decision was wrong

- A player action needs a rule an order cannot express, such as a dodge or a strike at a chosen point, and the order grows fields only the player uses.
- NPC behaviour needs orders that change every tick, and the flow-field rebuild per new objective dominates the tick time.
- A replay diverges because something other than orders and the focus rule changed the world from outside.
