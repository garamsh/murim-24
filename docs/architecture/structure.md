# Structure

The shape the system is partitioned into, and the rules that shape imposes on every change.

## Current decisions

The application is the simulation core: C++ with no engine dependency. Adapters are everything around it — Godot rendering, input and UI in GDScript, save files, the test harness.

- **The port is the unit.** One port per conversation with the outside, named for that conversation and never for the technology serving it.
- **Dependencies point inward.** An adapter references the application; the application references no adapter. A technology's name inside the application — a Godot type, a file format, a GDExtension binding — is a defect, even where the language permits it.
- **Every crossing goes through a port.** A driving adapter reaches the application only through a port; the application reaches the outside only through a port an adapter implements. Something outside reached from the inside without a port is a defect even where the call compiles.
- **A port is declared in the application's terms.** Its operations and the types crossing it are the application's, and converting them to and from the technology's is the adapter's work. A type belonging to the technology in a port's signature puts the outside inside.
- **An adapter's only counterpart is its port.** One adapter calling another carries application work outside, where no port names it and no substitute stands in for it.
- **A new port is earned** by a conversation whose technology can be replaced without the application changing, and which the application can name without naming that technology. A port carries two or more adapters — the real technology, and one for the test harness — so a port with a single adapter that nothing substitutes is indirection.
- **GDScript holds no game rule.** It lives only in adapters; a rule written there exists outside the application.

## Rationale

- Hexagonal partitioning keeps the simulation's rules runnable and testable with no engine attached: ADR 0001 (`adr/0001-partition-the-system-hexagonally.md`).
- Godot is the presentation adapter and C++ the core's language, for PC and mobile: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).

## Open questions

- **Whether the inside is itself partitioned.** Cockburn's statement of the pattern (*Hexagonal architecture*, 2005) leaves it undivided; Palermo's Onion Architecture (2008) and Martin's *Clean Architecture* (2017) divide it into concentric rings with every reference pointing inward. The project takes a side, in an ADR, when the core holds more than one subject area.
