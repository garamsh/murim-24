# ADR 0049: Load the game's content from data packs, and let mods be packs over the base

> Status: proposed
> Date: 2026-10-09

Append-only: once merged, the body below is not rewritten. A fact later found wrong is corrected in an appended note, not edited out; a revised decision is a new ADR that supersedes this one.

## Context

The owner decided on 2026-10-09, in issue #202, that players can mod the game, changing equipment, stats, skills and more, and that the same game can later ship in a theme other than wuxia. The PM's direction in the issue:
- All game content moves out of code into data packs loaded at start, and today's game becomes the base wuxia pack.
- The C++ core holds generic rules and never names a wuxia concept. Theme words live in the pack's data and translations.
- A mod is a pack that adds or overrides entries by a stable id, in a declared order over the base. It is data only, so mods stay safe and the simulation stays deterministic.
- The packs loaded, with their versions, are part of the save and of the determinism hash.
- A broken mod is refused with readable errors, never half-loaded.

Where the content lives today, on `main` at `5047f29`:
- **The core's tables.** `core/region.cpp`'s `region_data()` defines every table once: terrain kinds, factions with their stances, succession, admission and election, village and seat templates, structure kinds and what each holds, the five disciplines, realms, arts and their skills, the 24 surnames and 24 given names, the wares, and the rules of life. The generator reads only what it is passed (`world-generation.md`). The world is given `Rules` and `Life` (`core/world.hpp`), and a save holds them with the state (ADR 0030).
- **Theme words in the core's types.** The theme-neutral rule fails at the type level as well as in the data:
  - `Orientation` names `ORTHODOX`, `UNORTHODOX` and `DEMONIC`, and `Polarity` names `YANG` and `YIN`, in `core/world.hpp`.
  - Qi, realms, cultivation and the heart method name the core's fields and functions. A count of `\bqi\b|_qi\b|\bqi_` finds 26 lines in `core/world.hpp`, 46 in `core/world.cpp`, 10 in `core/save.cpp` and 9 in `extension/world_adapter.cpp`. `realm|Realm` finds 17, 21, 13 and 16 lines, and `cultivat|Cultivation` 11, 35 and 15 in the first three.
  - `NameEntry` carries a Korean and a romanised string, so the core holds display text in two named languages.
- **The view's content.**
  - UI text is `hud/hud.csv`, 310 lines with keys and Korean and English columns. Godot's `csv_translation` importer turns it into `.translation` files (`hud/hud.csv.import`), and `project.godot` lists them, so the import runs in the editor and at export.
  - Faction sprites are per-faction files, such as `art/bodies/orthodox.png` and `demonic.png`. `hud/affiliation.gd` lists them as `["orthodox", "demonic", "unorthodox"]`.
  - `hud/arts.gd` keys a colour ramp and a word to each `WorldAdapter.ORIENTATION_*` constant.
  - Village dressing is `world_art/villages.json`.
- **The map.** ADR 0047, proposed, makes the hand-made map one entry of the base pack, received by the core in its own terms.

Options weighed:

- **The data format.**
  - JSON, read by a C++ parser outside the core that keeps integers exact. Chosen. Modders can read and write it in any editor, schemas for it are common, and the sweep, the bench, the core's tests and ADR 0047's build tool, which run without Godot, can read the same packs as the game.
  - Rejected: Godot resources (`.tres`). A resource can carry a script, so a mod could run code, and only a Godot process can read one, which leaves the sweep and the map tool out.
  - Rejected: TOML. Godot has no reader for it. Its gain, comments in data, does not outweigh a second format beside the JSON that `world_art/villages.json` and the Aseprite exports already use.
  - Rejected: CSV for text. Godot's translation importer runs in the editor and at export, never on a file a player adds, so a mod's text could not load.
- **How the core receives content.**
  - The core declares a port, `PackSource`, that serves each pack's manifest and documents as the core's own value tree, of maps, lists, strings, integers and booleans. The core orders the packs, merges them, validates the result and builds its tables. Chosen.
  - Two adapters serve the port. One reads pack directories through the JSON parser: in the game the extension passes it the bytes Godot's file access reads, since a path inside an export is no file-system path, and the sweep, bench and map tool pass it files. The other holds packs in memory for tests. The core reads no file and names no format.
  - Rejected: parsing in the adapter into the core's finished tables. Merging and validation would then live outside the core, written once for Godot and again for every tool.
  - Rejected: the core parsing JSON. `structure.md` makes a file format's name inside the application a defect.
- **What a pack holds.**
  - `pack.json`, the manifest: the pack's id, its version, its name as a text key, whether it is a base, the packs it requires with the lowest version of each, and the packs it loads after.
  - `data/`, one document per kind of content, each a list of entries. An entry has an `id` and its fields.
  - `text/`, one document per locale, from keys to strings.
  - `art/` and `sound/`: PNG images and Ogg sounds, with a document giving each an id and, for art, its frames and the footprint it covers.
  - `map/`: ADR 0047's sources and compiled map.
- **What a pack may change.** Every number and name the rules read, and everything the view shows:

  | Content | Where it lives today |
  |---|---|
  | Items and wares: names, prices, effects, who sells them | `region.cpp` wares |
  | Stats: the disciplines and their talent draws, health at birth, the qi store's growth and recovery, and the stats' names | `region.cpp` life, `hud/hud.csv` |
  | Realms: names, thresholds, might | `region.cpp` realms |
  | Arts and skills: discipline, nature, each skill's shape, range, radius, damage, cooldown, cost and rank | `region.cpp` arts |
  | Factions: names, stances, succession, admission, election, diplomacy, growth stance, colour and sprite set | `region.cpp` factions, `hud/palette.gd`, `hud/affiliation.gd` |
  | Sects: names, factions, seats, founders, manuals | `region.cpp` sects |
  | Names: surnames and given names | `region.cpp` names |
  | Structures and templates: kinds, footprints, holdings, workshops, layouts | `region.cpp` structure kinds and templates |
  | The rules of life and the world's rules: kinship, husbandry, growth, learning, trade, reaches, the day's ticks | `region.cpp` life and rules |
  | The natures' axes: how many values each has and their names | `Orientation` and `Polarity` in `core/world.hpp` |
  | Text: every UI string, and the words for events, rumours, notices and refusals | `hud/hud.csv` |
  | Art and sound | `art/`, `sound/`, `world_art/villages.json` |
  | The map | ADR 0047 |

- **What stays core-only.** The rules, as code: the tick's order, every rule's procedure, the set of skill shapes, the refusal reasons, the kinds of event, the calendar's seasons and years, fixed-point units, the save's encoding and the hash. So do the limits the core refuses data past, such as `MAX_PEOPLE` and the constructor's bounds, so a pack cannot make the world exceed its bound. A pack can rename and renumber a stat. It cannot add a stat with a behaviour of its own, since only a rule gives a stat a behaviour. A mod that needs a new rule asks for one in the game.
  - Rejected for now: script mods, in Lua or GDScript. A script can break determinism, reach files, and hold a rule outside the core. This is revisited if mods repeatedly ask for a behaviour that a data field cannot express.
  - Rejected: Godot's resource packs (`.pck`) as the mod format. A resource pack can replace any file in the project, scripts included.
- **Ids and overriding.**
  - An id is lowercase ASCII letters, digits and underscores, unique within its kind in a pack. A pack's new entries take its id as a prefix by convention, such as `ironfan_steel_fan`, so two mods rarely collide by accident.
  - An entry whose id is already loaded overrides it field by field. A field it gives replaces that field, a nested object merges the same way, and a list replaces the whole list. A field it does not give keeps its value. An entry with `"remove": true` removes the entry. Chosen: a mod changing one number of a skill gives that number and nothing else, and so keeps working when the base changes another field.
  - Rejected: replacing a whole entry. Every mod would copy the fields it does not change, and lose the base's later fixes to them.
  - Rejected: list operations such as insert-at and remove-from. Lists in entries are short, and replacing one is unambiguous.
  - References between entries are by id. The core turns them into indices once all packs are merged, and refuses an id that names no entry.
- **Load order and conflicts.**
  - Exactly one base pack loads first. A game build names its base, and a re-themed game is the same build with another base.
  - Mods follow in the player's order, kept with the player's settings. The order must load each pack after the packs it requires and the packs it names to load after. An order that cannot, by a cycle or a missing requirement, is refused with the packs named.
  - Within a pack, documents load in the byte order of their paths, and entries in their order in the document. An entry's index is its place in the first appearance of its id over that order, so the same packs in the same order give the same indices on every host.
  - Two packs setting the same field is not an error: the later one wins. The mods screen lists every field more than one pack set, with the packs, so a player can see which mod won. Rejected: refusing every overlap. Two mods that both rebalance one popular weapon would then never load together, even when the player wants exactly that.
  - Where the core needs an order of its own, such as realms ascending by qi, the merged data must give it, or it is refused.
- **Validation and refusal.**
  - The core checks each document against the schema of its kind: every field known, of its type and within its limits, and every required field present. A misspelt field is refused, not ignored.
  - Numbers are integers. A number with a fraction or an exponent is refused, so no floating point enters the simulation's data.
  - The core then validates the merged content as a whole, with the checks the constructors and `region.cpp`'s `validate` make today: references resolve, orders hold, limits hold.
  - Every error is collected, not only the first. Each names the pack, the document, the entry's id, the path of the field and a typed reason, `ContentRefusal`, which the view words from the pack's text, so the errors read in the player's language.
  - A pack with any error is refused whole, with every pack that requires it. The new-game screen shows the refused packs and their errors, and starts only once the player accepts playing without them. Rejected: loading a pack's valid entries and skipping its broken ones. Half a mod gives a game its author never tested.
  - A merged set that fails as a whole is refused as a set, with the errors naming the packs that last set each failing field. The player removes or reorders mods.
  - A base pack that fails stops the game, since a test validates the base pack on every change.
  - The loader refuses a document past a size limit, so a broken file cannot exhaust memory.
- **Identity in the save and the hash.**
  - A pack's identity is its id and the hash of its simulation data: its `data/` documents as the core read them, as values and not as bytes, so reformatting a file changes nothing, and its compiled map's hash. Text, art and sound are not in it.
  - A save records the ordered identities of the packs with simulation data, and each pack's version for messages. A load is refused with a new `LoadError`, `PACKS_DIFFER`, naming the first pack missing, changed or out of order.
  - A pack of text, art and sound only, a re-skin or a translation, is recorded nowhere, and can be added or removed under a running save.
  - The world's hash folds the ordered identities. Every pinned hash changes once, in the pull request that adds the fold. No fold is conditional on packs, as #188 asks of every fold.
  - The save keeps holding the world's data, as ADR 0030 decided, because the world changes some of it in play, such as the hostility a duel turns. The pack identities say which content it was made from.
  - A test pins the base pack's data hash, so any change to the base's data shows in review as a changed hash, as the map's does under ADR 0047.
  - Rejected: a save holding only the identities, with no data. A duel's changed hostility would be lost, and the world's data would split between packs and state.
  - Rejected: loading a save with other packs and a warning. The data the save holds would still run, but the view would show other names and art for its indices, and a later mod edit could reorder them.
- **Determinism.** Packs are data, merged by one ordered rule, with integer numbers and ids in ASCII. The seed stays the only randomness. The same packs, in the same order, give the same tables and the same hash on every host.
- **Theme-neutral names in the core.** The core renames its theme words and keeps the rules. The theme word goes to the base pack's text:

  | In the core today | Becomes | The theme word, in the base pack |
  |---|---|---|
  | `Orientation`: `ORTHODOX`, `UNORTHODOX`, `DEMONIC` | the nature's first axis, an index into a list of values the pack names | 정 / 사 / 마, Orthodox / Unorthodox / Demonic |
  | `Polarity`: `YANG`, `YIN` | the nature's second axis, the same | 양강 / 음유, Yang / Yin |
  | qi, a person's store of qi | energy, and its capacity | 내공, Qi |
  | `Realm`, `realms` | `Level`, `levels`; `rank` stays the count of levels reached | 경지, Realm, and each level's name |
  | `Cultivation` | `Training`, how energy capacity grows | — |
  | the heart method, the first art learned | the first art, the art a person's nature comes from | 심법, Heart method |
  | `NameEntry`, Korean and romanised | a text key, worded by each locale's text | each name's Korean and romanised forms |
  | `ORIENTATION_*` and `POLARITY_*` in `extension/world_adapter.cpp` | the axis index, with its value's text key | — |
  | `Ware::qi`, `Trade::drained` | energy restored, and the energy share at which a person consumes | — |

  Art, skill, discipline, manual, sect, elder, master, faction, village, duel and the names of the structures are general words and stay. The base pack gives them their wuxia meaning in its text.
- **The view.** The view reads text, art and sound by id from the loaded packs:
  - the faction's accent and sprite set from its entry, in place of `hud/palette.gd`'s accents and `hud/affiliation.gd`'s list
  - an axis value's colour from the pack, in place of `hud/arts.gd`'s ramp by orientation
  - text from each pack's `text/`, given to Godot's translation server when the packs load

  A later pack's text and art override an earlier one's by id, as data does.
- **Where mods live.** The base pack ships inside the game. Mods are directories under `user://mods/`. The mods screen lists the ones found, lets the player enable and order them, and shows each one's errors and overlaps. Archives and an online catalogue are out of scope; the game is offline (ADR 0047).
- **The map in a mod.** A mod may replace the map whole, with its sources and compiled map, and never patch cells. The game checks its hash and runs ADR 0047's validator on a map that is not the base's, keeping the verdict by hash so it runs once.

The implementation, in order, each a pull request with its acceptance:
1. **Packs and the base pack.** `PackSource`, the value tree, merging, validation with `ContentRefusal`, the JSON adapter and the in-memory one, and `packs/wuxia/` holding exactly today's `region_data()` tables. `region_data()` is deleted.
   - Acceptance: every region test passes against the base pack, and the region's hashes are unchanged.
   - Acceptance: each refusal has a test with its control, such as an unknown field, a fraction, an unresolved id, a cycle and a missing requirement.
   - Acceptance: a test mod overriding one skill's damage changes only that field.
2. **Theme-neutral names.** The renames in the table above, in the core and the adapter, with behaviour unchanged. Names become text keys, and the base pack's `text/` holds today's Korean and romanised names. This follows #190, which moves the HUD's copies of rules into the core.
3. **Identity in the save and the hash.** `PACKS_DIFFER`, the fold, `SAVE_VERSION` raised, and the pinned hashes and the base pack's data hash recorded. This follows #188, so the identities are listed once for saving, hashing and checking.
   - Acceptance: a save made with a mod is refused without it and loads with it, and a re-skin pack added under a save changes nothing.
4. **Text.** `hud/hud.csv` moves to the base pack's `text/ko.json` and `text/en.json`, loaded into the translation server at start, and the `.csv` and its import are deleted.
   - Acceptance: every HUD test passes, and a test pack's text overrides one key.
5. **Art and sound.** The sprites, icons, structure art and sounds are listed by id in the base pack, with faction accents and sprite sets read from faction entries.
   - Acceptance: a test pack recolours one faction with no code change.
6. **Mods.** Discovery under `user://mods/`, the mods screen, the order in settings, and the new-game screen's refusals.
7. **The map** enters the base pack with ADR 0047's map format, the third step of #173's amended plan.

## Decision

All game content is data in packs: a manifest, one JSON document per kind of content, text per locale, art, sound and the map. The game loads one base pack, today's game as `wuxia`, then the player's mods in a declared order that respects each one's requirements. Entries carry stable ids. A later pack overrides an earlier one field by field, the later winning every overlap, which the mods screen shows. The core receives packs through a port, `PackSource`, as its own value tree, and orders, merges and validates them itself. Every error is collected with its pack, document, entry and field, and a pack with any error is refused whole, with the packs that require it. Numbers are integers and ids ASCII, so the same packs give the same tables on every host. A save records the ordered identities, id and data hash, of the packs with simulation data, and is refused under others. The world's hash folds them. Text, art and sound are the view's and outside both. Rules stay code: the core renames its wuxia words to general ones, and the base pack's text gives the theme back.

## Consequences

- **What becomes easier.**
  - A balance change is a data edit with a pinned data hash to update.
  - A re-themed game is another base pack.
  - A translation is a text pack.
- **What becomes harder.**
  - Every new field a rule reads needs a schema entry and a refusal test.
  - A rename in the core touches the base pack's documents in the same pull request.
- **The save version** rises with step 3, and every earlier save is refused.
- **The pinned hashes** change once, with step 3.
- **Mods cannot add behaviour.** A request for one is a request for a rule in the game, which keeps the core's rules in one place.
- **#211's items** are data of this format from the start: weapons, armour and goods are entries of a kind, with the shapes and numbers ADR 0046 gives skills.
- **ADR 0047's map identity** is the base pack's identity, which includes the compiled map's hash.

## Signs the decision was wrong

- Modders ask for the same missing behaviour again and again, such as a skill that heals over time, and each request is a new field, so the schema grows a field per mod.
- Field-by-field overrides make two popular mods combine into something neither author tested, and players report it as a bug in the game.
- Loading and validating the packs takes more than a second at start on the development Mac.
- Players lose saves to `PACKS_DIFFER` after a mod's small update often enough to name it, so a save needs to load under a changed pack after all.
- A re-themed base pack needs a rule the wuxia one does not, such as a stat with a behaviour of its own, and the theme-neutral core turns out to be wuxia-shaped.
