# Godot stack

The concrete Godot form of naming, file layout, tests, comments, the check entry points and asset export, for the Godot project at the repository root and the GDExtension C++ under `extension/`.

Checked against Godot 4.7.2, its documentation for 4.7 (`docs.godotengine.org/en/4.7`), godot-cpp 10.0.0 and Aseprite 1.3.18.6.

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

GDExtension C++ under `extension/`:

- Code lives in the `murim_godot` namespace.
- A class registered with Godot is named as GDScript sees it, since `GDCLASS` registers the C++ name.
- `_bind_methods` keeps the name godot-cpp requires.

## File layout

- The repository root is the Godot project: `project.godot` and `murim.gdextension` sit there.
- `murim.gdextension` names, per platform, the library `make build` writes to `build/extension/`.
- `extension/CMakeLists.txt` is the CMake project for the GDExtension library; it adds `core/` as a subdirectory, and CMake fetches godot-cpp at configure time from the release archive pinned by hash there.
- A scene keeps the scripts and resources only it uses in its own folder; a resource two scenes share sits in the nearest folder both are under.
- Every `.uid` file Godot writes beside a script or resource is committed with it.
- A folder Godot must not import holds an empty `.gdignore`. `make build` writes `build/.gdignore`.

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
| `make build` | After `core/`, CMake configures `extension/` into `build/extension/` with the Ninja generator, then builds the GDExtension library and `world_hash`; then writes `build/.gdignore`, and `.godot/extension_list.cfg` naming every committed `*.gdextension`. |
| `make format` | `clang-format --dry-run --Werror` over every `extension/*.cpp` and `extension/*.hpp` as well, with `core/.clang-format` as the style. |
| `make lint` | clang-tidy reads no file under `extension/`: `GDCLASS` expands into code `core/.clang-tidy` rejects. |
| `make test` | After `ctest`, `godot --headless --path . --script res://run_tests.gd` runs every `*_test.gd`. |

`make` runs the `godot` on `PATH`; `make GODOT=<path>` runs another. It must be Godot 4.7.2: locally any 4.7.2 install, in CI the Linux release archive pinned by SHA-512 in `.github/workflows/ci.yml`.

## Asset export

Aseprite is the source of sprite art. The `.aseprite` file is committed, and its export is written beside it from the CLI:

```sh
aseprite --batch <name>.aseprite --sheet <name>.png --data <name>.json --format json-array --list-tags
```

The `.png` and `.json` are committed with it, because CI has no Aseprite.
