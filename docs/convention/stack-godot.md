# Godot stack

The concrete Godot form of naming, file layout, tests, comments, the check entry points, asset export, and Android and iOS export, for the Godot project at the repository root and its GDScript.

Checked against Godot 4.7.2, its documentation for 4.7 (`docs.godotengine.org/en/4.7`), Aseprite 1.3.18.6, OpenJDK 17.0.20.1 and Xcode 26.6 with the iOS 26.5 SDK.

## Contents
- Naming
- File layout
- Tests
- Comments
- Entry points
- Asset export
- Terrain layers
- Android export
- iOS export

## Naming

GDScript, after the Godot 4.7 GDScript style guide (`tutorials/scripting/gdscript/gdscript_styleguide.html`):

| Kind | Form | Example |
|---|---|---|
| File, folder | `snake_case` | `world_adapter_test.gd` |
| Class, enum | `PascalCase` | `WorldAdapter` |
| Function, variable, parameter, signal | `snake_case` | `run`, `core_hash` |
| Private function or variable | `_snake_case` | `_adapter_hash` |
| Constant, enum member | `CONSTANT_CASE` | `TICKS` |
| Node in a scene | `PascalCase` | `Player` |

## File layout

- The repository root is the Godot project: `project.godot` and `murim.gdextension` sit there.
- `murim.gdextension` names, per platform, the library `make build` writes to `build/extension/`.
- A scene is `<name>/<name>.tscn` in a folder at the repository root named for it, with its script `<name>.gd` beside it, such as `world_view/world_view.tscn`. The scene keeps the scripts and resources only it uses in that folder; a resource two scenes share sits in the nearest folder both are under.
- Aseprite sources sit under `art/` at the repository root, each beside the script that draws it and its exports.
- Every `.uid` file Godot writes beside a script or resource is committed with it, and so is the `.import` file it writes beside each texture, font or translation a scene loads.
- A folder Godot must not import holds an empty `.gdignore`. Every build tree sits under `build/`, and `make build` writes `build/.gdignore`.

## Tests

- Framework: none beyond Godot. `run_tests.gd` at the root runs each test it is given and exits non-zero unless every test loaded and passed; a test that fails to parse counts as failed.
- A test is `<unit>_test.gd` next to the code it tests, extends `RefCounted`, and has `func run() -> bool`, returning `true` on pass and printing what it compared.
- Assertions read what GDScript reads: the return values of the registered classes' methods.

## Comments

- GDScript doc comment: `##` on the lines above the declaration.
- Every other GDScript comment: `#`, followed by a space.
- `.gdextension` file: `;`.

## Entry points

| Name | What it runs |
|---|---|
| `make build` | After the C++ builds, writes `build/.gdignore`, and `.godot/extension_list.cfg` naming every committed `*.gdextension`, then imports the project's resources into `.godot/` with `godot --headless --path . --import`. |
| `make test` | After `ctest`, `godot --headless --path . --script res://run_tests.gd` runs every `*_test.gd`. |
| `make bench` | After the tick benchmark, `godot --path . --script res://world_view/frame_bench.gd` renders `world_view/world_view.tscn` in a window with vsync off. It checks the scenario first, then prints p50, p99 and max frame time over its timed frames, and PASS or FAIL against a 16.7 ms p99. It needs a display, so it runs on a developer's machine only, and says so. `make bench` exits non-zero when either benchmark fails. |
| `make android` | After the C++ build for Android, `godot --headless --path . --export-debug Android build/android/murim.apk` exports the `Android` preset in `export_presets.cfg`. It needs the prerequisites in §Android export, so it is not part of `make ci`. |
| `make ios TEAM_ID=<id>` | After the C++ build for iOS, `godot --headless --path . --script res://ios_team_id.gd -- <id>` writes the Team ID into `.godot/export_credentials.cfg`, then `godot --headless --path . --export-debug iOS build/ios/murim.ipa` exports the `iOS` preset in `export_presets.cfg` as the Xcode project `build/ios/murim.xcodeproj`. It fails without `TEAM_ID`, and needs the prerequisites in §iOS export, so it is not part of `make ci`. |

`make` runs the `godot` on `PATH`; `make GODOT=<path>` runs another. It must be Godot 4.7.2: locally any 4.7.2 install, in CI the Linux release archive that `.github/scripts/install-godot.sh <dir>` downloads, checks against its pinned SHA-512 and installs as `<dir>/godot`.

## Asset export

Aseprite is the source of sprite art. The `.aseprite` file is committed, and its export is written beside it from the CLI:

```sh
aseprite --batch <name>.aseprite --sheet <name>.png --data <name>.json --format json-array --list-tags
```

The `.png` and `.json` are committed with it, because CI has no Aseprite.

## Terrain layers

The ground is laid from `art/terrain/` on `TileMapLayer`s with 16-pixel tiles:

- The plain is one layer on the cell grid, a full tile on every cell.
- Every other kind has a layer of its own above it, offset by half a cell up and left. Its tile at (x, y) has its corners on cells (x - 1, y - 1), (x, y - 1), (x - 1, y) and (x, y), and its corner mask adds 1, 2, 4 and 8 for each of those cells that is of the kind.
- Mask 0 places no tile. Masks 1 to 14 place frame `from + mask` of the kind's tag in `transitions.json`, as atlas coordinates (`from + mask`, 0) of `transitions.png`. Mask 15 places one of the kind's four frames in `terrain.json`, picked by a seeded hash of the tile's coordinates.
- Tiles are placed with `set_cell`. `set_cells_terrain_connect` is not used: in `TERRAIN_MODE_MATCH_CORNERS` it gives a painted cell's corner the terrain only when all four cells sharing it are painted, which leaves a lone tile at every convex step of a border.
- A landform from `landforms.json` is drawn with everything standing, sorted by the bottom edge of its footprint, with its frame's bottom-left corner on its footprint's bottom-left. Its tag names its kind and footprint in cells, `<kind>_<width>x<height>`. It stands only on cells of its kind, and no two landforms share a cell.

## Android export

What Godot 4.7's "Exporting for Android" (`tutorials/export/exporting_for_android.html`) requires on the exporting machine:

| Prerequisite | Version |
|---|---|
| JDK | OpenJDK 17 |
| Android SDK packages | `platform-tools` 35.0.0 or later, `build-tools;35.0.1`, `platforms;android-35`, `cmdline-tools;latest`, `cmake;3.10.2.4988404`, `ndk;28.1.13356709` |
| Export templates | Godot 4.7.2's, `Godot_v4.7.2-stable_export_templates.tpz` from the godotengine/godot `4.7.2-stable` release, checked against that release's `SHA512-SUMS.txt` |

- Install the SDK packages with `sdkmanager --sdk_root=<android_sdk_path>` and the package names above, after accepting their licences with `sdkmanager --licenses`.
- In Editor Settings, `export/android/java_sdk_path` is the JDK and `export/android/android_sdk_path` the SDK. `make android` reads the SDK from `ANDROID_HOME`, by default `~/Library/Android/sdk`, and builds with the NDK at `ndk/28.1.13356709` under it.
- The `Android` preset exports arm64-v8a only, from the prebuilt template without Gradle, signed with the editor's debug keystore. Its `command_line/extra_args` runs `world_view/frame_bench.gd` at launch.
- `project.godot` sets `rendering/textures/vram_compression/import_etc2_astc`, without which Godot refuses an Android export.
- A release export needs a release keystore, which is never committed.

## iOS export

What Godot 4.7's "Exporting for iOS" (`tutorials/export/exporting_for_ios.html`) requires on the exporting Mac, and what Xcode 26.6 adds:

| Prerequisite | Version |
|---|---|
| Xcode | 26.6 with the iOS 26.5 SDK, selected with `xcode-select` |
| iOS platform | The iOS 26.5 Simulator runtime, installed with `xcodebuild -downloadPlatform iOS`. Without it, Xcode 26.6 refuses the exported project's asset catalog and launch storyboard even in a device build, with "No available simulator runtimes" or "iOS 26.5 Platform Not Installed". |
| Export templates | Godot 4.7.2's, as in §Android export; they include `ios.zip` |
| Signing | An Apple ID signed in under Xcode › Settings › Accounts: a free Personal Team installs on the user's own devices for 7 days, a paid Apple Developer team for a year. |

- The Team ID is the 10-character ID of the signing team, free Personal Teams included. With the Apple ID signed in, Xcode › Settings › Accounts › Manage Certificates creates an "Apple Development" certificate if none exists. Its Organizational Unit (OU) in Keychain Access is the Team ID, which `security find-certificate -c "Apple Development" -p | openssl x509 -noout -subject` also prints.
- Godot 4.7.2 refuses to export with an empty `application/app_store_team_id`, and the committed preset leaves it empty. `make ios TEAM_ID=<id>` writes it into `.godot/export_credentials.cfg`, which is never committed and which Godot reads over `export_presets.cfg`. Setting it in the editor's export dialog writes it into `export_presets.cfg` instead, and that change is not committed.
- The `iOS` preset exports arm64 only, with `application/export_project_only`, so Godot writes the Xcode project and builds no `.ipa`. Its `application/additional_plist_content` sets `godot_cmdline`, the launch arguments the iOS app reads from its `Info.plist`, to run `world_view/frame_bench.gd` at launch.
- iOS loads no shared extension. `murim.gdextension` names a static archive for `ios.debug.arm64`, and the core's and godot-cpp's archives under `[dependencies]`. The export copies all three into the project and links them, and registers `murim_init`.
- To run on a device: open `build/ios/murim.xcodeproj` in Xcode. Under the `murim` target's Signing & Capabilities, turn on "Automatically manage signing" and pick the team. Connect the iPhone and turn on Settings › Privacy & Security › Developer Mode. Pick the iPhone as the run destination, and Run. With a Personal Team, the first launch also needs the developer trusted under Settings › General › VPN & Device Management. Xcode's console shows the frame benchmark's output.
- If Xcode refuses the bundle identifier as taken, change `application/bundle_identifier` in the preset and export again.
