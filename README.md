# Evidence for #176

Mockup of the PC HUD sizes proposed for #163 (PR #176, phase 1). It was made by `capture_pc_mockup.gd` at `a5be12d` (main, before this PR's changes) with Godot 4.7.2 on macOS arm64 after `make build`: the script was copied to the project root, then run as `godot --path . --script res://capture_pc_mockup.gd -- <dir>`. This branch is never merged.

Each image is composed from real 480x270 captures of `play.tscn` (seed 2026's region, five-tick days, 10 years lived, steering army 2's person 4, as evidence/pr-161 does): the world alone, the HUD alone on a transparent background, and the settings dialog alone. These are scaled by nearest neighbour. The HUD's panels are cut out of its capture and placed in a 960x540 layout anchored to the world's corners. The settings button under the map is a mock built from the HUD's own panel and label with `SETTINGS_TITLE`. F2's hint is left out.

- `before_<res>_<locale>`: main as it draws now. Everything is at the world's whole multiple, 4x at 1920x1080 and 2x letterboxed at 1280x720.
- `after_<res>_<locale>`: proposed. The world is at the same scale, the HUD at half the world's scale rounded up: 2x at 1920x1080, 1x at 1280x720.
- `settings_<res>_<locale>`: proposed, with the settings button clicked: the panel's checker over the whole picture and its dialog centred.

The capture's log lines:

    capture_pc_mockup: steering army 2, person 4
    capture_pc_mockup: before_1920x1080_ko.png 1920x1080, sha256 2647d952445fe556
    capture_pc_mockup: after_1920x1080_ko.png 1920x1080, sha256 0d2ca36bf2642bf2
    capture_pc_mockup: settings_1920x1080_ko.png 1920x1080, sha256 ca23a0e9cf3433a4
    capture_pc_mockup: before_1280x720_ko.png 1280x720, sha256 b46f156ae605fa35
    capture_pc_mockup: after_1280x720_ko.png 1280x720, sha256 f315f90b71e5ca9a
    capture_pc_mockup: settings_1280x720_ko.png 1280x720, sha256 4496c34b956b2e91
    capture_pc_mockup: steering army 2, person 4
    capture_pc_mockup: before_1920x1080_en.png 1920x1080, sha256 2536811ae4db812a
    capture_pc_mockup: after_1920x1080_en.png 1920x1080, sha256 332e169d340fa25a
    capture_pc_mockup: settings_1920x1080_en.png 1920x1080, sha256 e3f032b769b1a8cd
    capture_pc_mockup: before_1280x720_en.png 1280x720, sha256 a9600dcf2d6b8fdf
    capture_pc_mockup: after_1280x720_en.png 1280x720, sha256 ffd59123c0a403d5
    capture_pc_mockup: settings_1280x720_en.png 1280x720, sha256 92a819a8a03df831
    capture_pc_mockup: done
