# ADR 0079: Move qi nature gradually on two axes by the heart method

> Status: accepted direction; runtime pending
> Date: 2026-10-13

The selected qi management model and the boundaries of its implementation follow-up.

## Context

- The owner selected model B on 2026-10-13: qi nature changes gradually on righteous-to-demonic and yin-to-yang axes according to the heart method. [Issue #293](https://github.com/garamsh/murim-24/issues/293) records the authority; [#309](https://github.com/garamsh/murim-24/issues/309) records this documentation task.
- ADR 0072 left the management-model choice pending and drafted numerical details. Its merged body stays frozen; selecting a direction does not approve those details or implement them.
- At `main` `3a46fcd44c181041dab936e77a8d62a51ec7d241`, `core/arts.hpp` defines categorical nature and `core/world.cpp` derives a person's nature from its first learned art. There is no gradual nature state.

## Decision

- Select model B: a body's qi nature moves gradually along righteous-to-demonic and yin-to-yang axes according to its heart method. The core owns the rule, shared by NPCs and players; adapters only present its results.
- Resolve only ADR 0072's pending management-model choice. Its insight, passive, learning and scroll proposals retain their status. This selects a replacement direction for ADR 0035's first-art derivation; that derivation remains the implemented behavior until the runtime change lands.
- Leave numerical ranges, rates and cadence, five elements, forced use, deviation penalties and reborn-body initialization undecided. Art compatibility, path bands, and selection or change of heart method also need decisions before their implementation; ADR 0072's signed bytes, daily step and thresholds are not approvals.

Alternatives:
- Retain first-art nature as the final model: not selected by the owner; retained as current runtime behavior.
- Adopt every numerical and consequence proposal in ADR 0072: rejected as an inference beyond the answer's scope.

## Consequences

Implementation remains tracked by #293. The PM can dispatch these bounded changes after the named gates; these are acceptance obligations for future work, not test results from #309.

| Change | Prerequisites and decision gates | Observable acceptance with controls |
|---|---|---|
| Drift and persistence | Decide bounds, representation, cadence, rates, initial nature for ordinary births, and heart-method selection/change; content must express the selected method's influence. | With agreed fixtures, each axis moves as specified toward the method's influence across successive updates; a fixture already at its target stays there. Equal NPC/player state evolves equally, including after a method change. Save/load preserves both axes and resumes the same trajectory/hash as uninterrupted play; changing either axis changes the hash, while an unchanged-state control keeps it. Invalid state and unsupported saves are refused under the agreed format policy. |
| Learning compatibility | Drift/persistence landed; ADR 0055's art records and ADR 0072's insight/realm/prerequisite checks available; decide compatibility boundaries and their interaction with existing ease/potency. | Holding all other requirements equal, compatible nature permits learning and incompatible nature reports the unmet nature requirement. Manual, teaching and synthesis use the same compatibility check for NPCs and players; changing an unrelated requirement still produces its own refusal. No forced-use or deviation behavior is introduced without a separate decision. |
| Rebirth sealing and presentation | Compatibility landed; soul-held arts/skills/insights and passive availability available under #293; decide reborn-body initialization and when availability is recomputed. | Rebirth retains knowledge and seals an art when realm or nature fails; meeting both restores availability, while failing either keeps it sealed. Sealed active skills and passives provide no effect; an otherwise equal usable-art control does. Load restores the same knowledge and availability, and the adapter displays core known/sealed states without deciding them. |

- Each runtime change updates the paired responsibility documents and records the repository's required verification for its implemented scope.
- The documentation change adds no runtime, save format or hash change. Rebirth initialization cannot be inferred from the previous body's nature or the selected direction.

## Signs the decision was wrong

- The implemented rule gives equal NPC/player state different drift solely because one is steered.
- The agreed rates or compatibility boundaries prevent retained arts from becoming usable through the intended heart-method progression; revisit those choices with the failing fixtures before changing the selected model.
