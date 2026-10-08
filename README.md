# Evidence for #177

Mockup of the faction window proposed for #165 (PR #177, phase 1). This branch is never merged.

The images were made by `faction_window.gd`, the same file as `art/mockup/faction_window.gd` at 272e6e1, the head of `feat/faction-window`. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://faction_window.gd -- <dir>`. The world is seed 2026's region with five-tick days, lived 10 years. The window is drawn with `hud/panel.gd`'s frame, font and palette over the real `play.tscn`. Leaders, people, villages and the stance hostility are the world's. Two declarations are assumed: Orthodox at war with the Demonic Cult, and the Unorthodox at peace with it.

Names are `<view>_<run>_<locale>.png`. The runs:
- `pc_1920x1080`: the HUD at 2x in 960x540.
- `touch_1920x1080`: the HUD at 4x in 480x270.
- `touch_2400x1080`: the HUD at 4x in 480x270, centred.

The views:
- `hud`: the HUD without the "C 도전" line. On touch, the 도전 button is now 세력.
- `leader`: the Orthodox leader (army 0, person 0), with the Demonic Cult (at war) chosen. Make peace is lit; declare war is greyed with "already at war".
- `confirm`: the leader asked to declare war on the Unorthodox (ally). The confirmation stands over the window.
- `member`: a Demonic Cult member (army 4, person 8). No orders are shown, only the challenge to the leader.
- `refused`: an Orthodox member (army 3, person 6) pressed C. The core's `NO_SUCCESSION` word is shown.

The capture's log:

    
    faction_window: leader steers army 0, person 0, of Orthodox
    faction_window: member steers army 4, person 8, of Demonic
    faction_window: refused steers army 3, person 6, of Orthodox
    faction_window: confirm steers army 0, person 0, of Orthodox
    faction_window: hud steers army 0, person 0, of Orthodox
    faction_window: 0 words overrun their room
    faction_window: hud_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 80f6bcc54a901912
    faction_window: leader_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 8c66b1a574e85102
    faction_window: confirm_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 b9c3ca5d6994d7cf
    faction_window: member_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 4acefdc6e02fcae2
    faction_window: refused_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 30384eaa5aebfaba
    faction_window: hud_touch_1920x1080_ko.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 31bacb79b11f2183
    faction_window: leader_touch_1920x1080_ko.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 f5674b287224c6f2
    faction_window: confirm_touch_1920x1080_ko.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 f121e482de108668
    faction_window: member_touch_1920x1080_ko.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 a5554c3e7c9218f6
    faction_window: refused_touch_1920x1080_ko.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 c6d52170505009ab
    faction_window: hud_touch_2400x1080_ko.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 87cbb10e0f72eb2d
    faction_window: leader_touch_2400x1080_ko.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 4262a9f7e37e66d7
    faction_window: confirm_touch_2400x1080_ko.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 d9e58b2c10e9a369
    faction_window: member_touch_2400x1080_ko.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 9736374ac92ffa78
    faction_window: refused_touch_2400x1080_ko.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 c72b67f274e94d52
    faction_window: hud_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 296f44b9011dcff0
    faction_window: leader_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 65a7def84cf91ab5
    faction_window: confirm_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 aab54e47784a4a81
    faction_window: member_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 c32d5f2a2bfad0d9
    faction_window: refused_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 c4ffa87262500cb4
    faction_window: hud_touch_1920x1080_en.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 562a345bce2c93f1
    faction_window: leader_touch_1920x1080_en.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 0c9fca8c2faf3ab7
    faction_window: confirm_touch_1920x1080_en.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 1eef97a871a04a9d
    faction_window: member_touch_1920x1080_en.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 9868a345bd23ebb4
    faction_window: refused_touch_1920x1080_en.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 c8ddee97395ab9f5
    faction_window: hud_touch_2400x1080_en.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 56dbeaffc51a4f8d
    faction_window: leader_touch_2400x1080_en.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 2283b213223de2a1
    faction_window: confirm_touch_2400x1080_en.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 ca24e0949b5dbf84
    faction_window: member_touch_2400x1080_en.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 c4d99883fc79a6eb
    faction_window: refused_touch_2400x1080_en.png 2400x1080, HUD 4x in [P: (240, 0), S: (480, 270)], sha256 b8fb410306847166
    faction_window: done
