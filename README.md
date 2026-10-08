# Evidence for #157

Captures of `play.tscn` at `a67464f`, the head after the rebase onto main `c99a1fa`. They were made by `capture_touch.gd` with Godot 4.7.2 on macOS arm64 after `make build`: `godot --path . --script capture_touch.gd -- <dir>`. Each is 480x270 with a 4x nearest-neighbour copy (`_4x`), in Korean (`_ko`) and English (`_en`). This branch is never merged.

The touch HUD shows no key hints. In the touch layout the technique bar is hidden and no word names a key. Labelling the slots 1-9 or Q W E R T is out of scope here and belongs to #159 phase 2.

- `mobile_hud_idle_*`: at rest, the body cooling down after its last strike.
- `mobile_hud_aim_*`: the first skill dragged to aim, with the range ring, the aimed area, the pad and the cancel zone.
- `mobile_hud_cancel_*`: the same drag carried onto the cancel zone.
- `mobile_hud_self_*`: the second skill held past a long press.
- `mobile_hud_targets_*`: every place a touch lands, outlined and sized from `hud/touch.gd`'s constants. The cooldown notice shows because the long press was released, casting on self, while the body was cooling.
- `settings_touch_off_*`: the settings panel on the desktop, with the touch row off and lit, in the 1920x1080 window. The 9 rows are 21 px apart and the dialog is `(60, 4, 360, 256)`. Godot reports the stretch as 4x with no offset, so the `_4x` copy is that window pixel for pixel.
- `settings_touch_on_*`: the same panel with the touch row on, the touch controls shown behind it, in a 2400x1080 window (20:9, a phone's shape). Godot reports the root viewport's final transform as 4x at offset (240, 0). `settings_touch_on_*_2400x1080.png` is that window: the 480x270 frame scaled and placed by that transform over black, which leaves the 240 px pillars at each side.

The 2400x1080 images are composed, not screenshots. A script can read only the root viewport's 480x270 texture, not the window's pixels, and reading the screen instead needs a screen-recording permission the capture did not have.

The world animates in real time, so a rerun can differ by a few pixels in the `mobile_hud_*` images. The `settings_touch_off_*` images had the same SHA-256 on two runs.

The capture's log lines:

    capture_touch: marched 1423 ticks, a hostile person in range true
    capture_touch: mobile_hud_idle_ko.png 480x270, sha256 592e2404a583266d, locale ko; touch shown true, skills 2, cooling 0.76 with 1 s left, ready at 1445 on tick 1428
    capture_touch: mobile_hud_aim_ko.png 480x270, sha256 a7369020b5376eae, locale ko; aiming { "slot": 0, "aim": (-0.722222, -0.111111), "cancel": false }, range 48 px
    capture_touch: mobile_hud_cancel_ko.png 480x270, sha256 ef945b502b8dd56c, locale ko; aiming { "slot": 0, "aim": (0.108448, -0.994102), "cancel": true }
    capture_touch: mobile_hud_self_ko.png 480x270, sha256 c8f1191b4b7fd809, locale ko; holding self true
    capture_touch: mobile_hud_targets_ko.png 480x270, sha256 9f9d7e700293ef7f, locale ko; 2 skill circles, the cancel zone, the stick's zone [P: (0.0, 140.0), S: (200.0, 130.0)], 3 buttons and pause
    capture_touch: settings_touch_off_ko.png 480x270, sha256 7afda690254cf383, locale ko; settings open true, 9 rows 21 px apart in [P: (60.0, 4.0), S: (360.0, 256.0)], touch off, touch controls shown false, window (1920, 1080), stretch [X: (4.0, 0.0), Y: (0.0, 4.0), O: (0.0, 0.0)]
    capture_touch: settings_touch_on_ko.png 480x270, sha256 8aa251c3b8002075, locale ko; settings open true, 9 rows 21 px apart in [P: (60.0, 4.0), S: (360.0, 256.0)], touch on, touch controls shown true, window (2400, 1080), stretch [X: (4.0, 0.0), Y: (0.0, 4.0), O: (240.0, 0.0)], composed at (2400, 1080) by [X: (4.0, 0.0), Y: (0.0, 4.0), O: (240.0, 0.0)]
    capture_touch: marched 1423 ticks, a hostile person in range true
    capture_touch: mobile_hud_idle_en.png 480x270, sha256 d3446bd91b01c506, locale en; touch shown true, skills 2, cooling 0.76 with 1 s left, ready at 1445 on tick 1428
    capture_touch: mobile_hud_aim_en.png 480x270, sha256 2d74436986cadbda, locale en; aiming { "slot": 0, "aim": (-0.722222, -0.111111), "cancel": false }, range 48 px
    capture_touch: mobile_hud_cancel_en.png 480x270, sha256 4481dc13c1242872, locale en; aiming { "slot": 0, "aim": (0.108448, -0.994102), "cancel": true }
    capture_touch: mobile_hud_self_en.png 480x270, sha256 1fe89ee2360d4b4c, locale en; holding self true
    capture_touch: mobile_hud_targets_en.png 480x270, sha256 42de91ca64d9edd5, locale en; 2 skill circles, the cancel zone, the stick's zone [P: (0.0, 140.0), S: (200.0, 130.0)], 3 buttons and pause
    capture_touch: settings_touch_off_en.png 480x270, sha256 cd66064278d31d71, locale en; settings open true, 9 rows 21 px apart in [P: (60.0, 4.0), S: (360.0, 256.0)], touch off, touch controls shown false, window (1920, 1080), stretch [X: (4.0, 0.0), Y: (0.0, 4.0), O: (0.0, 0.0)]
    capture_touch: settings_touch_on_en.png 480x270, sha256 eee49facb6d0977f, locale en; settings open true, 9 rows 21 px apart in [P: (60.0, 4.0), S: (360.0, 256.0)], touch on, touch controls shown true, window (2400, 1080), stretch [X: (4.0, 0.0), Y: (0.0, 4.0), O: (240.0, 0.0)], composed at (2400, 1080) by [X: (4.0, 0.0), Y: (0.0, 4.0), O: (240.0, 0.0)]
    capture_touch: 14 images, none repeated true
