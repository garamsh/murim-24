# Evidence for the dialogue mockup

Mockup of talking to people, proposed for #166 (phase 1). This branch is never merged.

The images were made by `dialogue.gd`, the same file as `art/mockup/dialogue.gd` at 1e4d1b0, the head of `feat/dialogue`. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://dialogue.gd -- <dir>`. The world is seed 2026's region with five-tick days, lived 10 years, one copy per pair. The window and the prompt are drawn with `hud/panel.gd`'s frame, font and palette over the real `play.tscn`, its rumour log hidden. People, names, factions, relations (`WorldAdapter.relation`) and rumours (`WorldAdapter.rumours`, worded by `hud.gd`'s `rumour_text`) are the world's. The script walks the steered army up to its person (that person's army held in place) until they stand within the assumed talk reach of 2 cells.

Names are `<view>_<run>_<locale>.png`. The runs:
- `pc_1920x1080`: the HUD at 2x in 960x540.
- `touch_1920x1080`: the HUD at 4x in 480x270.

The views:
- `prompt` (PC): the player (army 10) stands 2 cells from an Orthodox villager (person 21); "Space 대화 · <name>" stands over the villager.
- `refused` (PC): an Orthodox army (3) stands 1 cell from a Demonic Cult person (28), hostile; the prompt says it will not talk, with no key.
- `menu` (PC): the dialogue window on person 21: card, faction, sect, age, greeting, and the menu (Any news? / Farewell).
- `news` (PC): person 21's own rumours, newest first (5 of 8 shown).
- `far` (PC): the control: an Unorthodox person (26) far from person 21 tells 5 rumours, none of which person 21 heard.
- `hud` (touch): the touch HUD with the rumour log gone and no talk prompt or button (touch is deferred).

The capture's log:

    dialogue: prompt steers army 10 of Orthodox, talks to person 21 of Orthodox, 2 cells apart, 8 rumours
    dialogue: refused steers army 3 of Orthodox, talks to person 28 of Demonic, 1 cells apart, 9 rumours
    dialogue: menu steers army 10 of Orthodox, talks to person 21 of Orthodox, 2 cells apart, 8 rumours
    dialogue: news steers army 10 of Orthodox, talks to person 21 of Orthodox, 2 cells apart, 8 rumours
    dialogue: far steers army 2 of Unorthodox, talks to person 26 of Unorthodox, 1 cells apart, 5 rumours
    dialogue: the near person heard 8, the far person 5, 0 of them both
    dialogue: 0 words overrun their room
    dialogue: prompt_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 07edc79a4d05396b
    dialogue: refused_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 9da93192daf219cf
    dialogue: menu_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 4d42435e35507ed0
    dialogue: news_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 c84213c42fdd7843
    dialogue: far_pc_1920x1080_ko.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 8534cbe80990fd2e
    dialogue: hud_touch_1920x1080_ko.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 09c2d90d9056597e
    dialogue: prompt_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 5097257ad7959193
    dialogue: refused_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 b37bdb45099a863a
    dialogue: menu_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 e41f1cd5be78a93e
    dialogue: news_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 010503246e48d559
    dialogue: far_pc_1920x1080_en.png 1920x1080, HUD 2x in [P: (0, 0), S: (960, 540)], sha256 6c6b607b5995112a
    dialogue: hud_touch_1920x1080_en.png 1920x1080, HUD 4x in [P: (0, 0), S: (480, 270)], sha256 b8ca04fa584cd164
    dialogue: done
