# Evidence for #161

Captures of `play.tscn` at `569bb03ad92bab1593322dfc5d6cd208f1c8c25f`, the head of `feat/arts-hud` after the review's changes (the first set was at `335fcdc`) (#161, phase 2 of #159), made by `capture_arts.gd` with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://capture_arts.gd -- <dir>`. Each is 480x270 with a 4x nearest-neighbour copy (`_4x`), in Korean (`_ko`) and English (`_en`). This branch is never merged.

The world is seed 2026's region with five-tick days, lived 10 years, steering the army of the first person who knows an art skill (army 2, person 4: Black Fang Dart, second-rate). The play scene's own ticks are held so it keeps steering that army. No person in 8 seeds lived 30 years learned a second art, so no capture shows the 약화/Weaker tag.

- `hud_arts_*`: the keyboard HUD: qi against the store, the realm by name, and the technique bar's five slots on Q W E R T holding the loadout, R and T empty, each key in a column of its own left of its slot's icon.
- `arts_panel_*`: K's arts panel, Black Fang Dart chosen and put on T (jade); its unlearned skills greyed, the realms above the person's in crimson; the army's skills with no cost and a shared cooldown.
- `notice_*`: the notice of a realm gained, from the core's store of progress (ADR 0036), after ticking the world until one reached the steered person.
- `pause_*`: the pause panel, its guide with K's row.
- `mobile_hud_arts_*`: the touch HUD with the 무공 button and the arc holding the loadout, the empty slots crossed.
- `arts_panel_touch_*`: the 무공 button tapped, then the Black Fang Dart tile tapped: the arc ringed in jade, waiting, and the close button in the pause button's place.
- `arts_panel_touch_placed_*`: the same skill placed on the fourth button.

The capture's log lines:

    capture_arts: lived 10 years; steering army 2, person 4, knowing 1 arts, 3 skills, realm 2, qi 3751 of 3752
    capture_arts: hud_arts_ko.png 480x270, sha256 9c5a7baf1e8a851e, locale ko; qi 3,751 / 3,752, realm 이류, slots ["army", "army", "Black Fang Dart", "-", "-"]
    capture_arts: arts_panel_ko.png 480x270, sha256 5e3f5caf9274c7d4, locale ko; selected { "art": 2, "skill": 0 }, just 4, slots [{ "art": -1, "skill": 0 }, { "art": -1, "skill": 1 }, {  }, {  }, { "art": 2, "skill": 0 }]
    capture_arts: ticked 11240 for a notice: ["경지 돌파: 일류"]
    capture_arts: notice_ko.png 480x270, sha256 4e3c7b76ce994043, locale ko; notice "경지 돌파: 일류" shown true
    capture_arts: pause_ko.png 480x270, sha256 749fdb947bd58fd2, locale ko; 10 controls in the guide
    capture_arts: mobile_hud_arts_ko.png 480x270, sha256 9555709303cf73c7, locale ko; touch shown true, 5 buttons, slots [true, true, false, false, true]
    capture_arts: arts_panel_touch_ko.png 480x270, sha256 a2a5388a6d815253, locale ko; panel open true, placing true, waiting true, selected { "art": 2, "skill": 0 }
    capture_arts: arts_panel_touch_placed_ko.png 480x270, sha256 4e3e238cddd7830f, locale ko; slots [{ "art": -1, "skill": 0 }, { "art": -1, "skill": 1 }, {  }, { "art": 2, "skill": 0 }, {  }], just 3
    capture_arts: lived 10 years; steering army 2, person 4, knowing 1 arts, 3 skills, realm 2, qi 3751 of 3752
    capture_arts: hud_arts_en.png 480x270, sha256 61fd24acd761ff5b, locale en; qi 3,751 / 3,752, realm Second-rate, slots ["army", "army", "Black Fang Dart", "-", "-"]
    capture_arts: arts_panel_en.png 480x270, sha256 0469da98ad6a5b54, locale en; selected { "art": 2, "skill": 0 }, just 4, slots [{ "art": -1, "skill": 0 }, { "art": -1, "skill": 1 }, {  }, {  }, { "art": 2, "skill": 0 }]
    capture_arts: ticked 11240 for a notice: ["Realm reached: First-rate"]
    capture_arts: notice_en.png 480x270, sha256 b4c4437fb55c73ec, locale en; notice "Realm reached: First-rate" shown true
    capture_arts: pause_en.png 480x270, sha256 ee00afc75de766ce, locale en; 10 controls in the guide
    capture_arts: mobile_hud_arts_en.png 480x270, sha256 4ffe2239b175faf7, locale en; touch shown true, 5 buttons, slots [true, true, false, false, true]
    capture_arts: arts_panel_touch_en.png 480x270, sha256 820e9977c910e341, locale en; panel open true, placing true, waiting true, selected { "art": 2, "skill": 0 }
    capture_arts: arts_panel_touch_placed_en.png 480x270, sha256 8f76e2abd81b3bdc, locale en; slots [{ "art": -1, "skill": 0 }, { "art": -1, "skill": 1 }, {  }, { "art": 2, "skill": 0 }, {  }], just 3
    capture_arts: 14 images, none repeated true
