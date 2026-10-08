# Evidence for #176

This branch is never merged. It holds two sets of captures.

## Build-out, at 0157a1d

These were captured from `play.tscn` at `0157a1d`, the head of `feat/pc-ui-scale`. The script was `build/capture_pc_hud.gd`, run with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://capture_pc_hud.gd -- <dir>`. The window is sized to each resolution in turn. The world is seed 2026's region with five-tick days, lived 10 years, steering army 2's person 4, as in evidence/pr-161. Every image is at the window's own size, in ko and en, under `build/`:

- `hud_<res>_<locale>`: the keyboard HUD at 1280x720, 1920x1080 and 2560x1440. The world fills the window at 2x, 3x and 4x, showing 640x360 world px. The HUD is at 1x, 2x and 2x, with the settings button under the map.
- `settings_<res>_<locale>`: the settings button clicked. The settings open over the paused world, with the checker over the whole window.
- `arts_<res>_<locale>`: K's arts panel in the middle.
- `title_<res>_<locale>`: the title at the least whole multiple covering the window, standing on its foot (3x at 1280x720 and 2560x1440, which cuts sky; 2x at 1920x1080 in the HUD's pixels).
- `touch_<res>_<locale>`, `title_touch_<res>_<locale>`: the touch controls on at 1920x1080 and on a 2400x1080 phone. The world fills the screen at 4x, and the HUD keeps 480x270 at 4x, centred.

Frame benchmark, `godot --path . --script res://world_view/frame_bench.gd` (M1 scenario, 500 bodies, window 1920x1080, vsync off, Apple M1):

| Build | View | Bodies drawn | p50 | p99 (runs) |
|---|---|---|---|---|
| main a5be12d | 480x270 at 4x | 248-249 | 1.44 ms | 6.38, 5.40 ms |
| this PR | 640x360 at 3x | 301 | 2.21-2.25 ms | 8.31, 7.15, 6.03, 7.37, 5.88 ms |

All runs pass the 16.7 ms p99 budget.

The capture's log lines:

    capture_pc_hud: title_1280x720_ko.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 75ac3a2cef84aec8; title
    capture_pc_hud: hud_1280x720_ko.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 3ebdb91ebfbf4b49; settings button at (1224.0, 70.0), (52.0, 16.0)
    capture_pc_hud: settings_1280x720_ko.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 56c5e1e695efb172; settings open true
    capture_pc_hud: arts_1280x720_ko.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 16d1fa2a02646dba; arts open true
    capture_pc_hud: title_1920x1080_ko.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 a8efb8d02b9e42a4; title
    capture_pc_hud: hud_1920x1080_ko.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 6c75b5d90bc388d2; settings button at (904.0, 70.0), (52.0, 16.0)
    capture_pc_hud: settings_1920x1080_ko.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 a0bb09be495a3873; settings open true
    capture_pc_hud: arts_1920x1080_ko.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 297d61255428fb6f; arts open true
    capture_pc_hud: title_2560x1440_ko.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 3578dffd3556ba17; title
    capture_pc_hud: hud_2560x1440_ko.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 d4bda9985943fe54; settings button at (1224.0, 70.0), (52.0, 16.0)
    capture_pc_hud: settings_2560x1440_ko.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 d9db2e62fa492dfc; settings open true
    capture_pc_hud: arts_2560x1440_ko.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 f9c1735ee43e00ef; arts open true
    capture_pc_hud: title_touch_1920x1080_ko.png 1920x1080, world 4x, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 a8efb8d02b9e42a4; title
    capture_pc_hud: touch_1920x1080_ko.png 1920x1080, world 4x, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 f09a6e860d54b5a7; touch shown true
    capture_pc_hud: title_touch_2400x1080_ko.png 2400x1080, world 4x, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 8ee378e955ec6ee6; title
    capture_pc_hud: touch_2400x1080_ko.png 2400x1080, world 4x, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 8abc123e34f7241d; touch shown true
    capture_pc_hud: title_1280x720_en.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 81fc6ff55a3c9071; title
    capture_pc_hud: hud_1280x720_en.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 050da8330d1fe34a; settings button at (1224.0, 70.0), (52.0, 16.0)
    capture_pc_hud: settings_1280x720_en.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 17f969f2a4a8d0de; settings open true
    capture_pc_hud: arts_1280x720_en.png 1280x720, world 2x, HUD 1x in [P: (0, 0), S: (1280, 720)], sha256 dcf972101634cfa5; arts open true
    capture_pc_hud: title_1920x1080_en.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 911011549a2bf595; title
    capture_pc_hud: hud_1920x1080_en.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 598eb7171b06de91; settings button at (904.0, 70.0), (52.0, 16.0)
    capture_pc_hud: settings_1920x1080_en.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 218a68098b45c269; settings open true
    capture_pc_hud: arts_1920x1080_en.png 1920x1080, world 3x, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 57a60fa5f05e4a32; arts open true
    capture_pc_hud: title_2560x1440_en.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 1a8c71df0b5d1f7e; title
    capture_pc_hud: hud_2560x1440_en.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 f7a525c9fb1bd560; settings button at (1224.0, 70.0), (52.0, 16.0)
    capture_pc_hud: settings_2560x1440_en.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 5bba40e6f0ee48b6; settings open true
    capture_pc_hud: arts_2560x1440_en.png 2560x1440, world 4x, HUD 2x in [P: (0, 0), S: (1280, 720)], sha256 fba18fdbfb7ae946; arts open true
    capture_pc_hud: title_touch_1920x1080_en.png 1920x1080, world 4x, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 911011549a2bf595; title
    capture_pc_hud: touch_1920x1080_en.png 1920x1080, world 4x, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 cc4a19dc0dfb58ce; touch shown true
    capture_pc_hud: title_touch_2400x1080_en.png 2400x1080, world 4x, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 c4292afeaf13c37c; title
    capture_pc_hud: touch_2400x1080_en.png 2400x1080, world 4x, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 5bca10d67fc91feb; touch shown true
    capture_pc_hud: done

## Phase 1 mockup, at a5be12d

The mockup files are at the root of this branch. The text below is the README they were pushed with.

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
