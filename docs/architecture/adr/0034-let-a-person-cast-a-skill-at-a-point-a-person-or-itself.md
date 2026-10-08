# ADR 0034: Let a person cast a skill at a point, a person or itself

> Status: accepted; its cast of one of its army's skills only superseded by ADR-0035, which takes the art skills a person knows too
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The user decided on 2026-10-08 (issue #147) that skills are used the way Albion Online's controls work:
- on mobile, a tap casts at the nearest enemy, favouring the facing direction; a drag aims, a release casts, and a drop on X cancels; a long press targets self
- on PC, Q, W, E and R cast at the cursor, on press or release

The skills themselves still come from the person's arts and faction.

ADR 0011 made intent an order: an objective cell, and the skills a strike draws among by the seed. It rejected a one-shot "strike now" command, because that "would need a resolution path outside the tier's strike tick that ADR 0006 keeps tier-invariant". ADR 0017 removed tiers, so that reason no longer holds.

Facts that shaped the choices, from `core/world.cpp` at `7e3e6cd`:
- A person whose cooldown has passed draws one of its order's skills by the seed. It aims at the nearest living hostile person in range, or at itself for a skill of range 0, and resolves the skill in `strike`.
- Damage lands after everyone has struck, so the order people strike in decides nothing.
- People move only after every strike, so positions read between two ticks are the positions the next tick's strikes read.
- A challenge and an application are refused by one function each, `challenge_refusal` and `application_refusal`, whose reasons the port refuses on (ADRs 0018 and 0031).
- The Godot view already draws each body facing the way it moved since the last tick, and the joystick's direction is input. Neither is state of the core.

Options weighed:

- **The port.** `cast(person, skill, target)`, where the skill is an index among the person's army's skills, not only its order's, and the target is a point in sub-cell units, a person, or the caster itself. `cast_refusal` returns the first reason that holds, in this order: `DEAD`, `NO_SKILL`, `ON_COOLDOWN` (not ready on the next tick), `NO_TARGET` (a dead person or one the world lacks) and `OUT_OF_RANGE` (a person beyond the skill's range). `cast` refuses exactly when it names a reason.
  - A point beyond the range is brought within it along the line from the caster, as a drag's aim stops at the range. Rejected: refusing such a point, which leaves the clamp to the adapter, a game rule outside the core.
  - Rejected: holding a cast on cooldown until it is ready. A cast would fire seconds after the player let go, and the issue asks for a refusal.
  - Rejected: a cast as a field of `Order`. An order is an army's march; a cast is one person's single use.
- **When a cast resolves.** On the next tick, in that person's place in the strikes, in place of its automatic strike. It goes through the same resolution as every strike, with damage landing after everyone has struck. Its cooldown starts even when it hits nothing. A later cast before that tick replaces it. On the tick, it is assessed again by the same function, and lapses if refused, such as after a conscription into an army without the skill. The tick clears every cast.
  - Rejected: resolving a cast at once inside `cast`. That needs no state, but damage would land before anyone else strikes, and outside the tick's handling of deaths: a person brought to 0 health would still strike on the next tick.
- **The automatic strike.** It stays, the same for every person, player and NPC alike. A ready person with no cast draws among its order's skills and aims at the nearest living hostile person in range. Choosing is now separate from resolving. Resolving is one function, which a cast also goes through, so the player's default attack is its order's skills, and any NPC can be made to cast through the same port.
  - Rejected: removing the automatic strike, so that NPCs choose casts by a data rule and the player only casts. It rewrites every battle's rule, with growth and the tick budget at risk, for nothing the issue needs, and a player who gives no input would never fight back.
- **Self for a damaging skill.** Its disc is centred on the caster, never refused. The disc hits only people hostile to the caster, so the caster is unhurt. A skill of range 0 already lands this way.
  - Rejected: refusing a damaging skill cast at oneself. That is a branch on the kind of skill, which ADR 0006 names as a sign its rule was wrong.
- **Facing.** The core keeps none. `aim(person, skill, facing)` reads, with no change to the world, the hostile person a skill would aim at. It is the nearest within the skill's range ahead of the facing, within a quarter turn either side. With no one ahead, it is the nearest behind. The first in roster order wins among equals, as the strikes break ties. With a facing of (0, 0) everyone is ahead, which is the automatic strike's aim. A tap is `aim` followed by a cast at that person.
  - Rejected: facing as state of the core, such as each person's last step. That is a field in the hash and the save for a value that only input uses, and the adapter has it already.

## Decision

A person casts one of its army's skills at a point, a person or itself through `cast`. `cast_refusal` names the first reason it may not, and `cast` refuses exactly then. The cast is kept on the person and resolves on the next tick in place of its automatic strike, through the one resolution every strike uses. A point beyond the range is brought within it, and a cast at itself is centred on the caster. The automatic strike stays for everyone. The core keeps no facing: `aim` reads the target a tap would take, favouring a facing the adapter gives. This supersedes ADR 0011's rejection of a one-shot strike.

## Consequences

- The player's body and every NPC cast through the same function, so the core has no player branch. The tests cast for an NPC at another faction.
- A pending cast is state: the hash folds it and a save holds it, so `SAVE_VERSION` rises to 3, and every earlier save is refused. A cast is folded only while one is pending, so a world in which no one casts hashes as before, and the pinned determinism hash is unchanged.
- A log that replays a world must carry its casts as well as its orders and applications, each with the tick it was made before.
- A cast refused on its tick lapses with no feedback to the caster. The adapter reads only that the cooldown did not move.
- The qi system the user is designing next will make skills cost qi and carry a nature and an element. A resource check would be one more reason in `CastRefusal` and one more step in the resolution every strike shares. Nothing is added for it now.
- Mobile tap and drag controls and PC quick-cast are presentation over `aim`, `cast`, `cast_refusal`, `skills` and `body_ready`, and are left to later issues.

## Signs the decision was wrong

- Players find casts dropped: a cast refused on its tick, or one replaced by a second before the tick, loses input the player meant. That would show casts should queue rather than replace.
- NPC behaviour needs targets the nearest-hostile rule cannot choose, such as healing an ally or focusing a leader, and a second automatic rule grows beside the first. That would show NPCs should cast through a data rule after all.
- A skill needs to know which way its caster faces, such as a cone or a dash, and facing has to enter the core as state.
- The automatic strike fires a skill the player wanted held for a cast, so players ask for the default attack to be off. That would show the automatic strike should not be the player's default.
