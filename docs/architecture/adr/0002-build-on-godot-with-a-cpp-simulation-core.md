# ADR 0002: Build on Godot with a C++ simulation core

> Status: accepted
> Date: 2026-10-06

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The targets are PC and mobile (Android, iOS). The load is the simulation — tens of thousands of agents, wars, area attacks that damage objects — not 2D rendering, so the engine matters as an adapter (ADR 0001) and the core's language decides performance. Options weighed:

- Godot with C#: C# export to Android and iOS is experimental in Godot 4.7 (Godot 4.7 documentation, `tutorials/scripting/c_sharp/index.html`), a risk carried to release.
- Unity with DOTS: proven for mass units, but its ECS rendering targets 3D, and its editor-bound project format resists review as text.
- Bevy, or an engine of our own: the engine work — platform ports, input, audio, CJK text — would displace the game.

## Decision

Godot 4.7 is the engine and presentation adapter. The simulation core is C++ with no engine dependency, loaded into Godot through GDExtension. GDScript is used only in adapters — presentation, input, UI — and holds no game rule.

## Consequences

- One stable native path covers PC, Android and iOS, and the core runs headless for tests and benchmarks.
- Godot's text scene format keeps every change reviewable in a pull request.
- C++ is slower to write than a managed language, and the core needs its own build and test tooling.
- A rule written in GDScript is a defect, because it would exist outside the core.

## Signs the decision was wrong

- With level-of-detail simulation in place, the benchmark misses its frame or tick budget on PC or a mid-range Android device, and the cost is in the engine rather than the core.
- GDExtension builds or exports for Android or iOS block a release.
- Game rules keep appearing in GDScript because the core is too slow to change.
