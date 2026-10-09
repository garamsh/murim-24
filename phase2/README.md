# Evidence for trade in the real game

Phase 2 of PR #181: the real `play.tscn` at fffcfcb, the head of `feat/merchants`. This branch is never merged.

`trade_capture.gd` made the images. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://trade_capture.gd -- <dir>`.

What the capture does:
- The world is seed 2026's region with five-tick days, lived 10 years, one copy per locale.
- It finds the nearest seller of the herb not hostile to the player, the person `WorldAdapter.sells` names, and walks the player up to it until `talk_refusal` is `TALK_NONE`.
- Every action goes through the scene's own `talk()` and `command()`, so the core's `buy`, `consume` and their refusals decide each outcome.

Names are `<view>_<run>_<locale>.png`. The runs:
- `pc_1920x1080`: the HUD at 2x in 960x540.
- `touch_1920x1080`: the HUD at 4x in 480x270.

The views:
- `menu` (PC): the dialogue on seller 12, Orthodox. The menu holds news, wares (chosen) and farewell, and the card reads "의술 · 약장수 / Medicine · seller".
- `trade` (PC): its wares, with the player's 1,307 silver, ten years of wages and the purse.
- `bought` (PC): Enter on the herb. The player has 1,297 silver and holds 1, the line reads "샀다 · 약초 / Bought · Healing herb", and the belt shows Z 1.
- `pill` (PC): down and Enter on the pill. The player has 1,237 silver, holds 1 and 1, and the line reads "Bought · Lesser Return Pill".
- `full` (PC): the window closed, Z at full health. The notice reads "체력이 이미 가득하다 / Health is already full", and nothing is consumed.
- `hud` (PC): the player walked into hostile person 8 until wounded, at 88 of 120 health, holding a herb and a pill.
- `used` (PC): Z. Health goes to 118, the herb is consumed, and the notice reads "약초 · 체력 +30 / Healing herb · Health +30".
- `touch` (touch): the touch HUD with the silver strip under its square buttons and no belt (touch deferred).

Not shown in the real game: a purchase refused for too little silver. The player holds over 1,200 silver at year 10, so it never arises here. `hud/dialogue_test.gd` and `play/play_test.gd` show that refusal at the player's starting 30 silver, and the mockup's `short` view shows how it looks.

The capture's log, with the sha256 lines left out:

    trade_capture: ko: player 20 of Orthodox, 1307 silver, beside seller 12 of Orthodox, 2 cells apart
    trade_capture: en: player 20 of Orthodox, 1307 silver, beside seller 12 of Orthodox, 2 cells apart
    trade_capture: menu: silver 1307, held [0, 0], health 120, line "", notice ""
    trade_capture: trade: silver 1307, held [0, 0], health 120, line "", notice ""
    trade_capture: bought: silver 1297, held [1, 0], health 120, line "샀다 · 약초", notice ""
    trade_capture: pill: silver 1237, held [1, 1], health 120, line "샀다 · 소환단", notice ""
    trade_capture: full: silver 1237, held [1, 1], health 120, line "", notice "CONSUME_FULL_HEALTH"
    trade_capture: wounded to 88 of 120 by person 8
    trade_capture: hud: silver 1402, held [1, 1], health 88, line "", notice ""
    trade_capture: used: silver 1402, held [0, 1], health 118, line "", notice "약초 · 체력 +30"
    trade_capture: touch: silver 1402, held [0, 1], health 118, line "", notice ""
    trade_capture: menu: silver 1307, held [0, 0], health 120, line "", notice ""
    trade_capture: trade: silver 1307, held [0, 0], health 120, line "", notice ""
    trade_capture: bought: silver 1297, held [1, 0], health 120, line "Bought · Healing herb", notice ""
    trade_capture: pill: silver 1237, held [1, 1], health 120, line "Bought · Lesser Return Pill", notice ""
    trade_capture: full: silver 1237, held [1, 1], health 120, line "", notice "CONSUME_FULL_HEALTH"
    trade_capture: wounded to 88 of 120 by person 8
    trade_capture: hud: silver 1402, held [1, 1], health 88, line "", notice ""
    trade_capture: used: silver 1402, held [0, 1], health 118, line "", notice "Healing herb · Health +30"
    trade_capture: touch: silver 1402, held [0, 1], health 118, line "", notice ""
    trade_capture: done
