# Godot adapter

The GDExtension adapter through which Godot drives the simulation core: what exists of it now, and the decisions it rests on.

## Current decisions

### Build

- The adapter is a GDExtension library built from `extension/`, linking `core/` unchanged. `murim.gdextension` at the repository root loads it into the Godot project there.
- `make build` builds the library for the host.
- `make android` builds it for Android arm64 through the NDK's CMake toolchain file, with the same `extension/CMakeLists.txt`. It exports two debug APKs from that library: the `Android` preset's, which opens on the title screen, and the `Android bench` preset's, which runs `world_view/frame_bench.gd`.
- `make export` builds release libraries for macOS, arm64 and x86_64 in one universal library, for Windows x86_64, cross-built with a pinned llvm-mingw, and for Linux x86_64, cross-built with that toolchain's Clang against a pinned glibc 2.31 sysroot (ADR 0074). It exports a release build for each that opens on the title screen.

### The port

- The library registers one class, `WorldAdapter`. GDScript cannot construct it; four factories make one:
  - `create` builds a world from what the core's constructor takes, as Godot types: a seed, map rows, armies, structures, rules and optionally each faction's leadership, as `Dictionary`s keyed by the core's field names.
  - `create_m1` builds the core's M1 benchmark scenario (`core/m1_scenario.cpp`), the one the tick benchmark runs.
  - `create_region` builds the region the core generates from a seed, with one person born into a village as the last army, and days of the core's length unless another is given. That person carries the player's soul, so it is born again into the same army when it dies, and the army is marked steered through the core's `steer`, so the rules that decide for people on their own decide nothing for it.
  - The region's tables are the base content pack's: the adapter reads `res://packs/wuxia/` through Godot's file access, which reads inside an export, and hands its documents to the core as bytes through `packs_json/`'s `BytePacks` (ADR 0049). A base pack the core refuses makes `create_region` and `load` report it with `push_error` and return null.
  - `load` reads a saved world back from a slot.
- A world from `create` or `create_m1` has no region: no terrain kinds, villages, sects, manuals, realms, arts or names, and it cannot be saved.
- The adapter keeps what the view draws of a region and the world does not hold: each cell's terrain kind, the kinds' and factions' names, and the villages with their templates. `load` regenerates it from the region's seed and the player's birth village, which the save holds.
- Reads give back what the core holds or derives, by subject: the world's hash, date and tick, the date with the tick within the day and the day's length for the view's time of day; armies, rosters, bodies and structures; leadership, duels, relations and declarations; names and kin; villages, fields and territory; what the player's soul has seen, its chart; sects and manuals; qi, realms, arts and a person's skills; rumours and attainment; talking and trade; and the last tick's strikes, the events and the steps of progress kept from a sequence on.
- `snapshot` gives in one call, as packed arrays, what a view reads after a tick: every living body with its person, position, health and ready tick, each army's faction, each structure's stage, and the tick's strikes with whom each struck. The world art and the sound read one snapshot a tick between them (ADR 0075).
- The library also registers `BodyView`, the view's native module, which keeps the bodies and what the fighting did to them by person, packs the bodies on screen into the rows' buffers each frame and draws the strikes, the target's brackets and the numbers in one call. It holds no game rule; its timings and buffer layout cross as its constants (ADR 0075).
- Orders go to the core through the port: `order`, `end_intent`, `steer`, `declare`, `apply`, `cast`, `buy` and `consume`.
- `attack_refusal`, `challenge_refusal`, `declaration_refusal`, `application_refusal`, `cast_refusal`, `talk_refusal`, `buy_refusal` and `consume_refusal` give the core's verdict before an order is made, as one of the class's constants.
- An order the core refuses is reported with `push_error`, and the call returns false; a factory that fails returns null after `push_error`.
- `player_army` gives the army the living person carrying the player's soul serves in, so a loaded world finds its player with no field of its own.
- Saving goes through the core's save port, served by `FileSaves`, which keeps one file per slot in `user://saves` unless a call names another directory. A save of a world with no region is refused.
- Positions cross as `Vector2`s in sub-cell units. The tick rate, the cell size, an arc's half-width, the structure stages, and every kind, refusal and event the core enumerates cross as the class's integer constants, so GDScript repeats none of them.
- Converting Godot types to the core's, and the hash back to a signed int, is the adapter's work.
- Input the core refuses, or that is missing, of the wrong type or outside the core's integer types, makes `create` report it with `push_error` and return null.
  - `create` catches `std::invalid_argument` alone, and `create_region` and `load` catch `std::logic_error`.
  - `save`, `load_refusal`, `order`, `declare`, `apply`, `cast`, `cast_refusal`, `aim`, `buy` and `consume` catch `std::invalid_argument`, each reporting it with `push_error`.
  - A read given an index out of range, or called on an adapter no factory made, stops through `CRASH_COND_MSG`.
  - No other exception is caught.
- Every order and every verdict is the core's: the adapter passes each order to the core, which decides whether it is valid, and each `*_refusal` read gives the core's own reason. The adapter still derives three answers the core does not give: the player's army, by scanning the rosters for the person carrying the player's soul; a founded village's name; and a grown structure's kind, from its plan (#190).
- The genealogy panel reads a person's name, parents, children, spouse, age and sect, and shows the family as its surname. No scene reads `names` or `person_family`, which the tests read.
- `skills` gives an army's skills, which the tests read. The controls read a skill's range from `person_skills`, and the cooldown indicators read `body_ready` against `tick_count`.

### World art

- `world_art/world_art.tscn` draws a world in the approved art, and every scene that shows a world uses it.
- The world is drawn at Albion's angle (ADR 0059): ground and roofs seen from above at that angle, facades and standing bodies drawn upright, and everything standing sorted by the row its footprint ends on, so whoever stands behind raised ground or a building is drawn behind it.
- Every cell-to-pixel and pixel-to-cell turn goes through one set of functions in `world_art.tscn`, raised with the ground, so a click picks the ground drawn under it.
- `world_art/relief.gd` derives each cell's kind of ground, level, face and slope from the core's terrain kinds, until the core's map carries levels. A cliff stands only where the core blocks movement.
- `world_art/ground.gd` draws the ground off the cell grid with `world_art/ground.gdshader`, as `docs/convention/stack-godot.md` §Ground sets out. `relief.gd` repeats the shader's heights, so bodies stand on what it draws.
  - Built cells, under a structure or paved, meet the ground in straight seams where the ground round them is flat.
  - Each 32-cell area is baked once, scattered with detail and decorative trees from a seed its index gives, and shown or freed as the view nears or leaves it. Without a renderer nothing is baked.
- Territory borders are drawn from `territory` in each owner's accent, and redrawn only when the territory moved or a village was founded.
- Each structure is drawn over the footprint the core gives it, in its stage's frame of its kind's art, with a shadow while it stands. A structure from `create` has no kind and is not drawn.
- Each village's paving, roads and banners come from `world_art/villages.json` by its template's name. They never block or break, and a founded village's are laid on the tick that finds it.
- Each field is tiled by the state the core gives it, and re-tiled when its state or the season changes.
- Bodies are drawn only on screen, through one `MultiMesh` per row of cells, whose buffers `BodyView` packs from the tick's snapshot, so no frame loops over every person in GDScript. Every body is one person, drawn as its faction's sprite, walking, facing and striking as the core's positions and strikes show.
- A body is drawn between where the last two ticks left it, as far as the frame lies between ticks, so it moves on every frame; the core's positions stay the only ones there are.
- A body hidden behind a structure, a tree or raised ground is outlined through `world_art/outline.gdshader`.
- Names are read round a viewer, the steered army in `play.tscn`, with how each person's faction stands toward the viewer's, through `relation`. No body is ringed: the hostile are told by their red names alone (#236). The people near the viewer are listed as packed arrays, so no frame makes a Dictionary per person.
- `world_art/combat.gd` shows what the fighting did, from the tick's snapshot and the core's `events` and from what changed since the tick before, through `BodyView`; it keeps the blood itself. In palette colours only:
  - each use that deals body damage flashes its shape, in its user's faction's accent
  - a struck person flashes, is knocked back, and its loss floats up for the people the settings name
  - a hit on or from the steered person holds the bodies involved for a few view frames and shakes the camera, while the settings' hit stop is on; view time only, the ticks go on, a hold counting down whether or not its body is drawn and holding no one dead
  - a world shown anew on the same art, as a load shows one, starts with no blood, hit stop or held body
  - a person a use hit bleeds while the settings' blood is on, drawn by `world_art/blood_pixels.gd`; a loss of hunger or age does not bleed
  - a person low on health is drawn hurt, and lower maimed, read from the core's health each tick, so wounds clear as health recovers
  - a person a use killed falls and fades; one who died of hunger or age is not drawn falling
  - the steered person's last target is marked while it lives
- The thresholds, durations and sizes of what `world_art` draws are constants of its scripts, and of `BodyView` for what it decides.

### Sound

- `sound/sound.tscn` plays what the core reports, as `docs/audio.md` sets out its sounds, their sources and their mixing.
- After each tick it reads the core's `events` recorded since the last. What the core records no event of, a strike, a hit, a fire or a mend, it finds by comparing the tick's snapshot with the one before.
- Each sound is heard where it happened, when that lies on screen, through a bounded pool of voices on the Effects bus; what finds no free voice is dropped.
- The music on the Music bus is the battle track while strikes were heard recently, and the explore track otherwise.
- The buses are laid out in `default_bus_layout.tres`.
- Leaving the tree, it stops every player and waits for the audio server to finish a mix, since in Godot 4.7.2 the server frees a stopped sound only in its next mix and reports one still held at shutdown as leaked.

### Scenes

- `world_view/world_view.tscn` shows and sounds the M1 scenario through `world_art.tscn` and `sound/sound.tscn`, ticking at the core's tick rate whatever the frame rate, with names shown, so `make bench` measures a frame with names on. `make bench` renders it at the PC's scale and prints where a frame's time goes: the core's tick, the adapter's snapshot, the view's scripts with the native code they call, and rendering. `world_view/village_bench.gd`, run by hand, times a frame in a generated region's village.
- `world_view/pan_bench.gd` pans the view across a region while the ground streams its areas and the world ticks, and reports frame time against the same budget; `make bench` runs it after the frame bench.
- The view fills the window at a whole-pixel scale, drawing textures without filtering, as `view_scale.gd` sets it, with a smaller least view on touch than on a PC. Bodies stand on whole world pixels.
- `play/play.tscn`, the project's main scene, opens on the title screen. A new game has `create_region` make the region of a seed typed or drawn at random, and Continue resumes the world saved in slot 0.
- The play scene steers the army `player_army` names, re-read when the world is made or loaded and after every tick, so it follows the soul after a rebirth.
- It turns input into orders through the port:
  - a right click, or the button held, moves the steered person toward a cell
  - the loadout's five slots cast at the cursor through `cast`, on a key's press or, as the settings choose, its release, with `play/aim.gd` drawing the aim meanwhile
  - a left click on a foe selects it as the target; on another person the player may talk to, it opens the talk
  - an attack intent on the selection, or on the nearest foe in sight, makes the core chase and strike it until the intent ends
  - talking, buying and consuming go through `talk_refusal`, `buy` and `consume`
  - a refusal from the core shows its reason on the HUD
- Each key is bound once, as a constant of `play.gd`, and the pause panel's guide is drawn from those constants.
- On a PC, the HUD's shortcut rail lists `SHORTCUTS`, the play scene's one table of what each panel's row opens, with its key, icon and word. A click on a row presses its key through the same path a key takes.
- The world ticks at the core's tick rate times the speed the settings choose, and no tick runs while a screen is open.
- The camera follows the steered person where its body is drawn, and stops at the map's bounds at any scale, so nothing past them shows. On a map narrower than the screen it shows the map's middle, and a hit's shake stays inside the bounds.
- When a tick finds the player's soul born again, the play scene opens the rebirth screen, and no further tick runs that frame.
- The pause panel's Save, and an autosave every few in-game days, write slot 0 and the loadout beside it, once a body carries the player's soul.

### HUD, panels and screens

- `hud/hud.tscn` is drawn at a whole-pixel scale of its own, as `view_scale.gd` gives it, and laid out in the window's size at that scale.
- The HUD shows:
  - the player's portrait, faction, health, qi and realm
  - a map around the player of what the player's soul has seen, from `explored`, `remembered_territory` and `remembered_villages`: unseen cells in fog, borders from remembered claim centres only against seen cells, and each village as last seen on its seen cells, in stone if seen in ruins; it is drawn again when `chart_revision` moves, and shows the core's season and year
  - its faction's leader, and a duel for the lead while one is fought
  - the target frame on the person the player selected
  - the notices of the core's refusals and of the player's lessons and realms, read from `progress`
  - the technique bar of the loadout's five slots
  - the player's silver and wares, through `hud/purse.gd`
- Panels and screens, each opened by its key, its rail row or its touch button, and laid out as its mockup under `art/mockup/` lays it out:
  - `hud/genealogy.gd`: a three-generation tree round the steered person, counting the person's generation from `person_parents`
  - `hud/sect.gd`: a sect, its seat, master, elders and members against its cap, and its faction's leadership and sects
  - `hud/faction.gd`: every faction with how it stands toward the player's, and the leader's war, peace and the challenge, each greyed with the core's refusal
  - `hud/dialogue.gd`: the person talked to, a menu of the core's `offers`, its own `rumours` as news, and its wares. The world runs on while it is open, and it closes once `talk_refusal` stops letting the two talk
  - `hud/arts.gd`: the person's realm, qi, heart method and known arts, and the loadout's five slots. Its left column is wider than the mockup's, so the longest English names fit, and every skill's icon is the technique bar's, since the mockups' own icons for each art skill are not in the production set
  - `hud/title.gd`, `hud/new_game.gd`, `hud/pause.gd` and `hud/settings.gd`: the screens. The title names the core's `LoadError` when slot 0 does not load
  - `hud/rebirth.gd`: the life just ended beside the life begun, in the panels' frame, with no mockup
  - `hud/map_window.gd`: the minimap's image at the largest whole multiple at which it and its legend fit the HUD's room, in a frame sized to them, with the player's place and season following the world while open, and a legend, as evidence/map-mockup lays it out; N opens it, and its row on the rail shows a map icon drawn in `hud/shortcuts.gd`
- `grade/grade.tscn` grades the world under the HUD, in the play scene and `world_view.tscn`, by the time of day the adapter's date gives (its tick within the day over the day's length) and its season, from `grade/grades.json`. At night it lights the windows `world_art/lights.json` names for each kind of structure at its stage, and the steered person's torch, as `world_art.tscn` lists those on screen, drawn once a frame at the art's pixels into a light field the pass reads, and emitters bloom while a hearth it lists is on screen. Its fog reads the relief's levels so high ground rises above it, and the clouds' shadows it drifts are a material it gives `world_art.tscn` for the ground and its raised strips alone; the fog's thickness and the clouds' shade are drawn into the same light field, once an art pixel; the settings' screen effects row, which ADR 0048's quality profile takes over once accepted, sets its quality. `grade_test` checks what it gives its shader; `grade/grade_check.gd`, run by hand on a display, checks the pixels it draws.
- `hud/nameplates.gd` names the people `world_art.tscn` lists over their heads, coloured by relation, at most `NAME_CAP` at once, the selected and the hostile first, leaving out a name that would cover another or a panel. The play scene names no one while a panel is open.
- `hud/talk_prompt.gd` names, over the person to talk to, the talk key or the core's refusal.
- `hud/loadout.gd` holds the skill on each of the five slots by what it is, so a slot keeps its skill as the person learns more. It is the player's choice, not the world's state: the core's save does not hold it, and the play scene writes it beside slot 0.
- The settings are the player's preference, not the world's state: each choice is kept at once in `user://settings.cfg`, and a choice missing or out of range keeps its default. Touch controls show automatically on a phone or a tablet (Godot's `mobile` feature); a desktop's touch screen is not asked after, since Godot 4.7.2 reports one wherever a mouse stands in for a finger.
- The screen open on top, or else the panel open over the world, takes every key and every tap, and the play scene acts on what it returns.
- The HUD's words come from `hud/hud.csv` through Godot's translations: Korean first, English supported. A name or word wider than its room is cut short with an ellipsis. Names come from the core, in Korean or romanised.
- The HUD draws in the colours of `hud/palette.gd`, and names factions and sects through `hud/affiliation.gd`. The panels share their frame, cards, menus and fitting in `hud/panel.gd`.
- Art is pixel art, from the Aseprite sources under `art/`, made to `docs/art-direction.md`.

### Touch

- `hud/touch.gd`, the touch controls, is laid out as the `art/mockup/mobile_hud_*.png` mockups after `art/mockup/touch.lua`. It turns each touch into what the play scene does.
- A joystick steers the person; five skill buttons cast the loadout's slots, tapped, dragged to aim, dropped on the cancel zone or held on the person itself; an attack and a next-foe button stand beside them; the panels' buttons press their keys.
- The skill buttons dim while the body cools down, from `body_ready` and `tick_count`: the core's cooldown is the body's, not a skill's.
- Rings on the ground are drawn round, not flattened, since the core measures a range the same each way.
- With the touch controls, the HUD keeps the native touch layout at the world's scale, centred in the window, and shows no shortcut rail. A mouse stands in for a finger where the screen takes no touch, so the layout can be tried at a desk.

### Tests

- Headless Godot tests, run by `run_tests.gd`, drive the adapter, the scenes and every panel and screen. The adapter's test compares the core's hash for a scenario against `world_hash`, a program that runs the core with no Godot.
- Every panel and screen test measures each word and name it draws against its room, in each locale.
- Every run gives the tests a directory of its own under the system's temp folder (`extension/test_saves.gd`), so two runs at once never write the same files. `extension/saves_race_check.sh`, run by hand, checks that.
- Pixels a headless run cannot read are checked by tools run by hand on a display: `world_art/outline_check.gd` and `world_art/ground_check.gd`.
- `sound/exit_probe.gd`, run in a Godot of its own, checks that freeing the sound scene leaks nothing.

## Rationale

- Remembered borders use the village's claim centre as last seen, and a changed centre redraws the map: ADR 0078 (`adr/0078-remember-each-villages-claim-centre-with-its-last-seen-state.md`).

- Godot is the presentation adapter and the core reaches it through GDExtension: ADR 0002 (`adr/0002-build-on-godot-with-a-cpp-simulation-core.md`).
- The binding release, its pin, the build path, the scenario format, how refusals reach GDScript, the test runner and how CI installs Godot: ADR 0004 (`adr/0004-bind-the-core-to-godot-with-godot-cpp-through-cmake.md`).
- The map drawn from the player's chart, of what it has seen as it was last seen, and the map window on N: ADR 0073 (`adr/0073-chart-the-land-a-soul-has-seen-and-show-each-village-as-it-was-last-seen.md`).
- The calendar kept by the core and read by the HUD, and the player's body carrying a soul: ADR 0019 (`adr/0019-draw-talent-and-lifespan-at-birth-and-bear-a-soul-again.md`).
- Every body one person, read and drawn one by one, and no focus to set: ADR 0017 (`adr/0017-make-every-npc-an-individual-and-every-army-a-roster.md`).
- Saving through the core's save port, served by files under `user://saves/`, and what the adapter regenerates from the region's origin on load: ADR 0030 (`adr/0030-save-a-world-through-a-save-port-in-the-cores-own-versioned-encoding.md`).
- Cross-building the Windows library with a pinned llvm-mingw, and the alternatives weighed: ADR 0033 (`adr/0033-cross-build-the-windows-extension-with-a-pinned-llvm-mingw.md`).
- One packed read of the world a tick, and the bodies and the fighting kept, packed and drawn natively in the adapter: ADR 0075 (`adr/0075-read-the-world-once-a-tick-and-draw-bodies-and-the-fighting-natively.md`).
- Cross-building the Linux library against a pinned glibc 2.31 sysroot, the floor it sets, and the alternatives weighed: ADR 0074 (`adr/0074-cross-build-the-linux-extension-against-a-pinned-glibc-2-31-sysroot.md`).
- Applying to a sect through the core's thresholds, and the rumours read from the core's ring of events: ADR 0031 (`adr/0031-let-a-person-apply-to-a-sect-by-the-admissions-thresholds.md`) and ADR 0032 (`adr/0032-keep-a-bounded-ring-of-world-events-and-read-rumours-from-it.md`).
- Steering through the orders every army follows: ADR 0011 (`adr/0011-steer-a-body-by-the-orders-every-army-follows.md`).
- Casting through the core at a point, a person or itself, and a tap's target read from the core with the facing the adapter gives: ADR 0034 (`adr/0034-let-a-person-cast-a-skill-at-a-point-a-person-or-itself.md`).
- The faction window and the nameplates reading the core's one relation, declarations given only by a faction's leader through the core's port, and the player's army marked steered so the core's diplomacy decides nothing for it: ADR 0038 (`adr/0038-let-faction-leaders-declare-war-and-make-peace-over-their-stances.md`).
- Qi, realms, natures and a person's skills read as the core holds them, and casts indexed among the skills a person knows: ADR 0035 (`adr/0035-cultivate-qi-into-realms-and-gain-arts-by-comprehension-manuals-and-teaching.md`).
- Talking within the core's talk reach, refused for hostility as `relation` reads it, a menu of what the core says a person offers, and rumours heard only by talking: ADR 0039 (`adr/0039-let-a-person-talk-to-another-within-a-reach-and-hear-the-news-it-has-heard.md`).
- Buying and consuming wares through the core's orders, refusals worded from tables by constant, the trade an offer in the data-driven menu, and the player's silver and wares on the HUD: ADR 0040 (`adr/0040-buy-elixirs-and-herbs-from-merchants-with-silver.md`).
- The HUD's notices of lessons and realms read from the core's store of progress, apart from the events rumours read: ADR 0036 (`adr/0036-keep-progress-in-the-martial-arts-in-a-store-of-its-own.md`).
- Each use of a skill drawn from the core's record of the last tick's uses, which is for a view and neither hashed nor saved: ADR 0046 (`adr/0046-strike-by-shape-at-arms-length-and-close-on-a-foe-in-sight.md`).
- Upright facades, structure footprints, real-time combat, Aseprite art and Korean-first UI text, and the alternatives rejected: ADR 0012 (`adr/0012-present-the-world-in-a-three-quarter-top-down-view.md`), which supersedes ADR 0008 (`adr/0008-present-the-world-top-down-in-real-time-as-pixel-art.md`).
- The angle, cells 16 by 12 raised by levels, the relief derived from terrain kinds with cliffs only where movement is blocked, and the ground baked once per area off the cell grid: ADR 0059 (`adr/0059-draw-the-world-at-albions-angle-on-cells-16-wide-and-12-deep-raised-by-levels.md`), which supersedes ADR 0012's grid and tiles.

## Open questions

- **Banners after a change of owner.** A banner keeps the colours of the owner its village had when it was laid, while the map's square and the borders follow the owner now.
- **Panels by touch.** On a touch screen the genealogy and sect panels open and close, but their selection, Tab's next faction and Enter's application have no touch of their own yet.
- **Talking by touch.** On a touch screen no button talks and no prompt shows, while touch is deferred (scope, 2026-10-09); a tap only closes an open dialogue window.
- **Wares by touch.** On a touch screen no belt shows and no button consumes a ware, while touch is deferred (scope, 2026-10-09); the silver strip shows.
- **Touch layout on a wide screen.** With the touch controls, the HUD keeps its native layout centred, so on a screen wider than 16:9 the world fills the sides while the joystick and the skill buttons stand inward of the screen's edges.
- **Bodies hidden by bodies.** A body hidden by another body is not outlined, and one outlined behind a building is outlined over another body standing between them too. The ground has no quality profile of ADR 0048's yet (#212).
- **Levels in the core.** The relief is derived by the view until ADR 0047's map carries levels (#173), which #214's climbing and sight need.
- **Rules outside the core.** #190 moves into the core the answers the adapter and GDScript still compute: the player's army, a founded village's name and a grown structure's kind in the adapter; the heart method, the weaker tag and a skill locked by realm in `hud/arts.gd`; the duel tag and a sect's gate in `hud/sect.gd`; and the person to talk to in `play/play.gd`.
- **Exceptions the adapter does not catch.** An exception from a path outside the handlers above, such as a `.at()` in `person_skills` or `structure_kind`, is not caught by the adapter. Whether any such path can throw on a world the core accepts has not been exercised.

- **Content from packs.** ADR 0049 (`adr/0049-load-the-games-content-from-data-packs-and-let-mods-be-packs-over-the-base.md`), proposed; nothing above changes until the pull requests that build it land.
  - **Packs.** The extension passes the bytes Godot's file access reads from the base pack and from `user://mods/` to the pack reader, which parses them into the core's terms.
  - **Text.** `hud/hud.csv` becomes each pack's `text/` documents per locale, given to the translation server when the packs load, since Godot imports a CSV only in the editor and at export.
  - **Art.** Faction accents and sprite sets, and each nature axis value's colour, come from the packs' `view/` documents, apart from the hashed data, in place of `hud/palette.gd`'s accents, `hud/affiliation.gd`'s list of sprite sets and `hud/arts.gd`'s ramp by orientation.
  - **Mods.** A mods screen lists the mods found, with their order, errors and overlaps, and the new-game screen shows the packs refused.
- **Gear and the economy's screens.** ADRs 0050 to 0052, proposed. The body is drawn in layers, body, armour, head and weapon, with armour and headgear tinted by the wearer's faction. New screens:
  - an inventory and equipment window on I
  - a warehouse window at a keeper or a cave
  - a cave's entrance drawn on the ground and the maps only once the player's soul has found it, by a search order or by stepping on it
  - a prompt and a list for piles on the ground
  - a market window, replacing the wares screen
  - a household panel
  - a treasury window for a leader
- **The faction management window.** ADR 0065, proposed. For a leader, it takes in the treasury window above, with panels for policies and the term left on each, the treasury, villages, armies and the effects seen last season against the one before. A change refused while its term runs shows the days left. Its mockup goes to the PM before it is built.
- **Standing in the view.** ADR 0054, proposed. The dialogue offers attack, extort, threaten, help and give, with their refusals. Its card and the faction window show the player's standing with the village, sect and faction by tier. A body whose groups stand hostile toward the player is ringed crimson like a hostile faction's, and an unfriendly one in a paler ring.
- **Qi nature and sealing, runtime pending.** ADR 0079 (`adr/0079-move-qi-nature-gradually-on-two-axes-by-the-heart-method.md`) records the owner-selected two-axis drift direction. Under [#293](https://github.com/garamsh/murim-24/issues/293), the adapter will expose core nature, unmet learning requirements and known/sealed states when implemented; the HUD will display them without computing drift or compatibility. Today nature still follows the first learned art. Values, rates, compatibility and reborn-body initialization await decisions; the selected direction adds no player-only rule.
- **The lineage window.** ADR 0055, proposed, and ADR 0072, proposed, which builds it from scrolls, as the owner decided. A window on L, apart from the K loadout panel, opens on a shelf of the scrolls the soul holds. A chosen scroll opens as a vertical hanging lineage, entry arts at the foot and the peak at the top, with known arts at their stage and mastery, sealed arts drawn as sealed, arts and insights known of dimmed and the rest as ???. Requirements within a scroll are drawn as paths, and those from another scroll as labels naming it, such as 【수련】 검의 이치. The PM's mockup (tab 八) sets the look.
- **Cliffs in the view and the input.** ADR 0058, proposed. A cliff is lifted 16 pixels a level for its first three levels and 6 a level beyond, in place of ADR 0059's 16 at every height, so a ten-level cliff is 90 pixels high; those numbers are the pack's `view/` data. Ground the viewer cannot see, read from the core's veil per area, is drawn as terrain outline only, with no people, items or structures. While the core recomputes it, up to about 40 ticks after the viewer moves, the view keeps drawing the last complete veil. A cliff the viewer can leap up is never veiled. A leap is drawn as a jump, up and down alike, with a stamina bar under a body below full stamina. Clicking a cell on top of a cliff the body can leap up, or below one it can jump down, walks there and leaps; a cell beyond reach is refused with its reason shown. Until the map's levels reach the core, the view derives levels for itself, as the 2.5D mockup does (#200, #204).
- **Natural objects in the view.** ADR 0056, proposed. The view draws each slot of the areas in its chunks, from the core's slot table: its kind's sprite for its stage, from the pack's `view/`, redrawn when a slot's stage changes. A grown tree's canopy is drawn upright, like a structure's facade.
- **Zooming out for world-scale arts.** ADR 0057, proposed. The camera zooms out to a half, a quarter, an eighth and a sixteenth. The ground is drawn from downscaled area images built with the map and redrawn for changed areas, with people as dots below a quarter and as one cluster per area below an eighth. A wave's aim shows its full disc, and the cast pulls the camera back. An area whose terrain changes has its ground re-baked once after the change settles, spread over frames, which extends ADR 0059's bake once per area.
- **Animals in the view.** ADR 0067, proposed. The view draws each animal of the areas in its chunks from the core's animal store, by kind and state, with sprites from the pack's `view/`: grazing, fleeing, fighting and dying. A spirit beast's territory is shown once the player has seen its beast. Hunting needs no new input: the player strikes an animal, or takes it as a cast's target.
- **The map by touch.** On a touch screen no button opens the map window, while touch is deferred (scope, 2026-10-09); the minimap shows.
- **Riders and boats in the view.** ADR 0061, proposed. A rider is drawn on its horse, and a boat as a vehicle on the river's cells with its passengers aboard. `mount`, `dismount` and boarding are orders through the port, with their refusals shown.
- **The map's images at 8,192 cells.** ADR 0070, proposed. The minimap, the map window and the zoomed-out images are built with the map at four times the pixels of a 4,096 map. Chunks, bakes and the streaming radius stay per area, so what the view holds around the camera is unchanged.
- **Walls in the view.** ADR 0062, proposed. #239 already draws the walled town's pieces as art, in the view only: wall runs, corner towers, gate towers and steps. ADR 0062's segments, corner towers and gate towers are drawn from those pieces at their height in levels, with a walkway and crenellations, as #242 sizes them. A closed gate is drawn shut, and a breach as a broken segment.
- **Personal favor in the dialogue.** ADR 0060, proposed. The dialogue's card shows the person's favor toward the player by tier, when it holds one, beside its groups' standing, read from the core.
- **Which platforms the library is built for.** It is built for the host, macOS arm64 and Linux x86_64 so far, for Android arm64, and for the PC exports, macOS universal, Windows x86_64 and Linux x86_64; iOS, Linux arm64 and Windows arm64 exports are not built.
