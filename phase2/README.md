# Phase 2 captures

Talking as `play.tscn` draws it at 0e380d7 on `feat/dialogue`, made by `dialogue_capture.gd` here, run with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://dialogue_capture.gd -- <dir>`. It finds and walks up the same pairs as the mockup, then lets the real scene pick whom to talk to (`talk_target`), open the window with Space (`talk`) and ask for news with Enter.

- `prompt`: the real prompt over person 21, 2 cells from the player.
- `refused`: an Orthodox army beside Demonic people; the prompt over the nearest, person 41, shows the refusal.
- `menu`, `news`: the window on person 21, its menu and its 8 rumours (5 shown).
- `far`: the window on person 38, the nearest to an Unorthodox army far away, its own news.
- `hud_touch`: the touch HUD, with no rumour log and no prompt.

The log:

    dialogue_capture: prompt steers army 10 of Orthodox, talks to person 21 of Orthodox, 2 cells apart, 8 rumours
    dialogue_capture: refused steers army 3 of Orthodox, talks to person 28 of Demonic, 1 cells apart, 9 rumours
    dialogue_capture: menu steers army 10 of Orthodox, talks to person 21 of Orthodox, 2 cells apart, 8 rumours
    dialogue_capture: news steers army 10 of Orthodox, talks to person 21 of Orthodox, 2 cells apart, 8 rumours
    dialogue_capture: far steers army 2 of Unorthodox, talks to person 26 of Unorthodox, 1 cells apart, 5 rumours
    dialogue_capture: the near person heard 8, the far person 5, 0 of them both
    dialogue_capture: prompt_pc_1920x1080_ko.png, prompt on 21 refused false shown true, window on -1 news false
    dialogue_capture: refused_pc_1920x1080_ko.png, prompt on 41 refused true shown true, window on -1 news false
    dialogue_capture: menu talks to 21 (wanted 21), opened true
    dialogue_capture: menu_pc_1920x1080_ko.png, prompt on -1 refused false shown false, window on 21 news false
    dialogue_capture: news talks to 21 (wanted 21), opened true
    dialogue_capture: news_pc_1920x1080_ko.png, prompt on -1 refused false shown false, window on 21 news true
    dialogue_capture: far talks to 38 (wanted 26), opened true
    dialogue_capture: far_pc_1920x1080_ko.png, prompt on -1 refused false shown false, window on 38 news true
    dialogue_capture: hud_touch_1920x1080_ko.png, prompt on -1 refused false shown false, window on -1 news false
    dialogue_capture: prompt_pc_1920x1080_en.png, prompt on 21 refused false shown true, window on -1 news false
    dialogue_capture: refused_pc_1920x1080_en.png, prompt on 41 refused true shown true, window on -1 news false
    dialogue_capture: menu talks to 21 (wanted 21), opened true
    dialogue_capture: menu_pc_1920x1080_en.png, prompt on -1 refused false shown false, window on 21 news false
    dialogue_capture: news talks to 21 (wanted 21), opened true
    dialogue_capture: news_pc_1920x1080_en.png, prompt on -1 refused false shown false, window on 21 news true
    dialogue_capture: far talks to 38 (wanted 26), opened true
    dialogue_capture: far_pc_1920x1080_en.png, prompt on -1 refused false shown false, window on 38 news true
    dialogue_capture: hud_touch_1920x1080_en.png, prompt on -1 refused false shown false, window on -1 news false
    dialogue_capture: done
