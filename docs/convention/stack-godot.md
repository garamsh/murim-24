# Godot stack

The concrete Godot form of naming, file layout, tests, comments, the check entry points and asset export, for the Godot project at the repository root and its GDScript.

Checked against Godot 4.7.2, its documentation for 4.7 (`docs.godotengine.org/en/4.7`) and Aseprite 1.3.18.6.

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
- Every `.uid` file Godot writes beside a script or resource is committed with it.
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
| `make build` | After the C++ builds, writes `build/.gdignore`, and `.godot/extension_list.cfg` naming every committed `*.gdextension`. |
| `make test` | After `ctest`, `godot --headless --path . --script res://run_tests.gd` runs every `*_test.gd`. |
| `make bench` | After the tick benchmark, `godot --path . --script res://world_view/frame_bench.gd` renders `world_view/world_view.tscn` in a window with vsync off. It checks the scenario first, then prints p50, p99 and max frame time over its timed frames, and PASS or FAIL against a 16.7 ms p99. It needs a display, so it runs on a developer's machine only, and says so. `make bench` exits non-zero when either benchmark fails. |

`make` runs the `godot` on `PATH`; `make GODOT=<path>` runs another. It must be Godot 4.7.2: locally any 4.7.2 install, in CI the Linux release archive that `.github/scripts/install-godot.sh <dir>` downloads, checks against its pinned SHA-512 and installs as `<dir>/godot`.

## Asset export

Aseprite is the source of sprite art. The `.aseprite` file is committed, and its export is written beside it from the CLI:

```sh
aseprite --batch <name>.aseprite --sheet <name>.png --data <name>.json --format json-array --list-tags
```

The `.png` and `.json` are committed with it, because CI has no Aseprite.
