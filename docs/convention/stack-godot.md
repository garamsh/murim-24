# Godot stack

The concrete Godot form of naming, file layout, tests, comments, the check entry points, asset export, Android export and PC export, for the Godot project at the repository root and its GDScript.

Checked against Godot 4.7.2, its documentation for 4.7 (`docs.godotengine.org/en/4.7`), Aseprite 1.3.18.6 and OpenJDK 17.0.20.1.

## Contents
- Naming
- File layout
- Tests
- Comments
- Entry points
- Asset export
- Ground
- Android export
- PC export

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
- `murim.gdextension` names, per platform, the library each entry point writes: `make build`'s debug libraries in `build/extension/`, `make android`'s in `build/android/`, and `make export`'s release libraries in `build/macos/` and `build/windows/`.
- A scene is `<name>/<name>.tscn` in a folder at the repository root named for it, with its script `<name>.gd` beside it, such as `world_view/world_view.tscn`. The scene keeps the scripts and resources only it uses in that folder; a resource two scenes share sits in the nearest folder both are under.
- Aseprite sources sit under `art/` at the repository root, each beside the script that draws it and its exports.
- Every `.uid` file Godot writes beside a script or resource is committed with it, and so is the `.import` file it writes beside each texture, font or translation a scene loads.
- A folder Godot must not import holds an empty `.gdignore`. Every build tree sits under `build/`, and `make build` writes `build/.gdignore`.

## Tests

- Framework: none beyond Godot. `run_tests.gd` at the root runs each test it is given and exits non-zero unless every test loaded and passed; a test that fails to parse counts as failed.
- A test is `<unit>_test.gd` next to the code it tests, extends `RefCounted`, and has `func run() -> bool`, returning `true` on pass and printing what it compared.
- A script a test runs in a Godot process of its own is `<name>_probe.gd` next to the test that runs it.
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
| `make android` | After the C++ build for Android, `godot --headless --path . --export-debug Android build/android/murim.apk` and `godot --headless --path . --export-debug "Android bench" build/android/murim_bench.apk` export the `Android` and `Android bench` presets in `export_presets.cfg`. It needs the prerequisites in §Android export, so it is not part of `make ci`. |
| `make export` | After the C++ builds for macOS and Windows, `godot --headless --path . --export-release macOS build/export/macos/Murim24.app` and `godot --headless --path . --export-release Windows build/export/windows/Murim24.exe` export the `macOS` and `Windows` presets in `export_presets.cfg`. It needs the prerequisites in §PC export, so it is not part of `make ci`. |

`make` runs the `godot` on `PATH`; `make GODOT=<path>` runs another. It must be Godot 4.7.2: locally any 4.7.2 install, in CI the Linux release archive that `.github/scripts/install-godot.sh <dir>` downloads, checks against its pinned SHA-512 and installs as `<dir>/godot`.

## Asset export

Aseprite is the source of sprite art. The `.aseprite` file is committed, and its export is written beside it from the CLI:

```sh
aseprite --batch <name>.aseprite --sheet <name>.png --data <name>.json --format json-array --list-tags
```

The `.png` and `.json` are committed with it, because CI has no Aseprite.

## Ground

The ground is drawn by a shader from the cells' data, not laid from tiles:

- Each area the view streams in is given to `world_art`'s ground shader as a texture of one texel a cell, holding what the core holds of the cell.
- The shader decides each pixel from the four nearest cells' values, interpolated and warped by noise, so no edge between kinds runs along a cell's side. Where two kinds are nearly even it dithers them in a narrow band.
- An area is drawn once into a texture when it streams in, and the viewport that drew it is freed. Nothing redraws the ground each frame.
- Detail (tufts, flowers, pebbles, reeds) and decorative trees are placed per area by Poisson-disc spacing, from a seed derived from the area's index, so an area drawn again shows the same. Anything a rule reads comes from the core, never from this placement.
- `art/terrain/` holds each kind's materials, exported as §Asset export says: a ramp of four colours, texture swatches, detail sprites, tree variants and face strata. What people build in straight lines, such as walls, bridges and paving, is drawn as sprites, not as ground.
- A test checks what the view gives the shader and where it places detail: the same area and seed give the same texture and the same placements. The drawn pixels are shown by captures in the pull request's evidence.

## Android export

What Godot 4.7's "Exporting for Android" (`tutorials/export/exporting_for_android.html`) requires on the exporting machine:

| Prerequisite | Version |
|---|---|
| JDK | OpenJDK 17 |
| Android SDK packages | `platform-tools` 35.0.0 or later, `build-tools;35.0.1`, `platforms;android-35`, `cmdline-tools;latest`, `cmake;3.10.2.4988404`, `ndk;28.1.13356709` |
| Export templates | Godot 4.7.2's, `Godot_v4.7.2-stable_export_templates.tpz` from the godotengine/godot `4.7.2-stable` release, checked against that release's `SHA512-SUMS.txt` |

- Install the SDK packages with `sdkmanager --sdk_root=<android_sdk_path>` and the package names above, after accepting their licences with `sdkmanager --licenses`.
- In Editor Settings, `export/android/java_sdk_path` is the JDK and `export/android/android_sdk_path` the SDK. `make android` reads the SDK from `ANDROID_HOME`, by default `~/Library/Android/sdk`, and builds with the NDK at `ndk/28.1.13356709` under it.
- Two presets export the same library. Each exports arm64-v8a only, from the prebuilt template without Gradle, signed with the editor's debug keystore.
- The `Android` preset, the one marked runnable, is the game: it opens on `run/main_scene`, `play/play.tscn`, under the package name `com.example.murim24` and the app name `Murim24`, and leaves `*_test.gd`, `*_probe.gd` and `run_tests.gd` out of the pack by `exclude_filter`.
- The `Android bench` preset's `command_line/extra_args` runs `world_view/frame_bench.gd` at launch, under the package name `com.example.murim24.bench` and the app name `Murim24 bench`.
- `project.godot` sets `rendering/textures/vram_compression/import_etc2_astc`, without which Godot refuses an Android export.
- A release export needs a release keystore, which is never committed.

## PC export

What Godot 4.7's "Exporting for macOS" (`tutorials/export/exporting_for_macos.html`) and "Exporting for Windows" (`tutorials/export/exporting_for_windows.html`) require on the exporting machine:

| Prerequisite | Version |
|---|---|
| Host | A Mac with the Xcode command line tools, for `xcrun` and the macOS SDK |
| Export templates | Godot 4.7.2's, as §Android export names them |

- `make export` writes `build/export/macos/Murim24.app`, and `build/export/windows/Murim24.exe` with `Murim24.pck` and the extension's DLL beside it.
- The `macOS` preset exports a universal app; the `Windows` preset exports x86_64, its pack beside the executable. Both export the release template, leave `*_test.gd`, `*_probe.gd` and `run_tests.gd` out of the pack by `exclude_filter`, and scale `application/config/icon`, `art/icon/icon.png`, by nearest neighbour.
- The window title is `application/config/name`.
- The bundle identifier is the placeholder `com.example.murim24`, as Android's package name is, and changes in `export_presets.cfg` alone once the game's name is chosen.
- The macOS app is signed ad hoc, Godot's default, and not notarized. Its entitlements disable library validation, which the guide asks of an app loading a GDExtension or signed ad hoc. A copy downloaded to another Mac is refused on first launch until it is opened once from System Settings, Privacy & Security, Open Anyway.
- A release for others is signed with a Developer ID Application certificate and notarized with an Apple Developer account, both the account holder's: set `codesign/codesign`, `codesign/identity` and `notarization/notarization` in the preset on the signing machine, and never commit the certificate or credentials.
- The Windows executable is not signed, so SmartScreen warns on first launch; More info, Run anyway opens it.
