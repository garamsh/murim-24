# ADR 0030: Save a world through a save port in the core's own versioned encoding

> Status: accepted
> Date: 2026-10-08

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

Issue #128 asks for saving and loading a game. A loaded world must be the same world: the same determinism hash, and the same hash after it ticks on. `structure.md` already names save files as an adapter. So the core may name no file, path or format technology, and a port earns its place only with a real adapter and a test-harness adapter.

Facts that shaped the choices, from `core/` at `7b12870`:
- `World::hash` folds the world's whole state except its data: the rules of life, names and growth plans are read every tick but not hashed. So an equal hash right after a load does not show the data came back, and ticking on does.
- Every army's flow field equals one built from the map and its order's objective. Growth rebuilds them all whenever it lays a blocking structure. The structure index and the widest footprint follow from the structures alone.
- What the Godot view draws of a region, its terrain kinds, its villages' names and templates, its structures' kinds and the health each army was created with, lives in the adapter. The core generates all of it from the region's seed.
- The player's body carries the only soul a region world gives, and keeps it through every rebirth.

Options weighed:

- **The port.** `SaveStore` keeps bytes by `Slot`: `put` replaces a slot's bytes and says whether they were kept, and `get` returns them or nothing. `save` and `load` encode and decode through it. The Godot adapter `FileSaves` writes slot `n` to `user://saves/slot_<n>.sav`, and the test harness keeps bytes in memory.
  - Rejected: a port carrying a `World` to and from the adapter. The adapter would then choose the encoding, and the core's state would have to cross in Godot's terms.
  - Rejected: `save` and `load` on `World` itself. The world would then name the store, and the codec would sit in a file already 2,700 lines long.
- **What a save holds.** It holds the world's whole state, its data included: the map, the rules, the rules of life and every record. A save therefore never depends on the build's tables. Flow fields, the structure index and the widest footprint are left out, and a load rebuilds them. A save from a generated region also holds its `Origin`, the region's seed and the village the player was born into. From that the adapter regenerates what it draws.
  - Rejected: a seed and an order log replayed on load. It is exact, but loading a century replays a century of ticks.
  - Rejected: storing what the adapter draws. The encoding would carry presentation that the core already derives from the seed.
- **The encoding.** It is the core's own: little-endian, fixed-width integers, sizes as 64 bits, a count before every list, a tag byte before every optional, enum and variant, and strings as a length and bytes. A save is the bytes `MURIMSAV`, a 32-bit format version, the payload's length, the payload, and an FNV-1a checksum over all before it.
  - Rejected: a text format such as JSON. It is larger, slower and gives nothing a player reads. It would also need a library in the core or a hand-written parser.
- **Versioning.** A save whose version is not this build's, older or newer, is refused with `UNSUPPORTED_VERSION` and never misread. Any change to what a save holds raises the version. A migration from one version to the next is a later decision, made when a version is worth carrying over.
- **Refusal.** A load parses at the trust boundary and returns `std::variant<Saved, LoadError>`, so the caller cannot reach a world without handling the error:
  - an empty slot gives `MISSING`, and bytes that do not begin as a save does give `NOT_A_SAVE`
  - a save ending before its header or its stated length gives `TRUNCATED`
  - a save failing its checksum, running past its length, holding a tag out of range or holding a state no world can be in gives `CORRUPTED`. That last check covers a reference naming a record the world lacks, a living person on no roster or on two, an objective off the passable cells, and data outside the constructor's limits.
  - Rejected: trusting a save whose checksum holds. A crafted save would then index past a vector on its first tick, an exception the boundary should have turned into a refusal.
- **Who the player is after a load.** The adapter's `player_army` finds the army that the living person carrying the player's soul serves in. It is read from the core, so a save needs no field for it. The play scene still takes the last army at creation, which is the same army until growth forms another.

## Decision

The core saves a world through a save port, `SaveStore`, that keeps bytes by slot, served by Godot's `user://saves/` and by an in-memory store in tests. A save holds the world's whole state, its data included, and for a generated region the seed and birth village it was made from. Derived state is rebuilt on load. The encoding is the core's own little-endian binary under a magic, a format version, a length and a checksum. A load refuses, with a typed `LoadError`, a save of another version, a truncated or corrupted one, or one holding a state no world can be in.

## Consequences

- A generated region lived 10 years at five-tick days loads with the hash it was saved with, and with the same hash after 1,000 more ticks. Seed 7, lived 15 years, saves to 40,995 bytes. Through the Release extension, saving took 0.9 ms and loading, with the region regenerated, 5.6 ms.
- The pinned determinism hash is unchanged: no state was added and the hash folds nothing new.
- Every field added to the world's state must also be written and read by the codec, and checked by the restore if it refers to a record, or a save drops it. The round-trip test catches what the hash or the next 1,000 ticks read.
- Every save written before a version change is refused after it until a migration is written.
- A world made by `create` or `create_m1` has no origin, and the adapter does not save one.
- The adapter regenerates what it draws from the current build's region tables. A change to those tables changes what a loaded world looks like until the version is raised.
- Continue on the title screen, which #134 added and cannot yet be chosen, and choosing a slot, are left to a later issue.

## Signs the decision was wrong

- Saves are refused after most releases, because what a save holds changes often, and players lose progress until migrations are written. That would show the format should carry fields by name rather than by position.
- Loading takes longer than a frame on the target phones. That would show the save should hold less, with more of it derived, or be read in parts.
- A loaded world draws differently from the saved one, because the region's tables changed without a version bump. That would show the save should hold what the view draws after all.
- Saves are edited by players or shared, and refusing every state no world can be in proves too strict or too costly to keep current with the core's invariants.
