# Evidence for PR #197

Before/after captures of every HUD panel and screen for PR #197. This branch is never merged.

## How they were made

- `hud_capture.gd` made every image. It ran with Godot 4.7.2 on macOS arm64, after `make build`, copied to the project root: `godot --path . --script res://hud_capture.gd -- <dir>`.
  - `before/` ran at origin/main 8611e63.
  - `after/` ran at the PR's head code (3514102).
- The world is seed 2026's region with five-tick days, lived 10 years, one copy per locale. The player is walked up to the nearest person it may talk to, for the dialogue views.
- The genealogy views steer army 1, whose person has a child and is in a sect.
- Every action is a key sent to the scene's own `_unhandled_input`, so the same script runs on both commits.
- The window is 1920x1080: the HUD at 2x on PC, 4x on touch.
- Names are `<view>_<locale>.png`.
- `compare.gd` compares two directories pixel by pixel. `sheet.gd` makes the contact sheets, and `crop.gd` the sect-line pairs.

## Sect line (#192)

- `sect_line_ko.png` and `sect_line_en.png` show the genealogy details column, before on the left and after on the right.
- Han Doyun (한도윤), army 1's person, used to read "마교" / "Demonic Cult", his faction. It now reads his sect, "천마신교" / "Heavenly Demon".
- The full screens are `before/genealogy_*.png` and `after/genealogy_*.png`.

## Keypad Enter (#191)

`genealogy_kp_entered` is G, Down, then keypad Enter. Its control `genealogy_entered` uses Enter in place of keypad Enter, and `genealogy_moved` is G and Down alone. The hashes are the first 16 hex digits of each PNG's sha256, from the capture logs:

| | moved | entered | kp_entered |
|---|---|---|---|
| before, ko | dfbda4c49ad9892c | bcd9b68be8955284 | dfbda4c49ad9892c, the same as moved: keypad Enter did nothing |
| after, ko | 43e5bacf9d4d4574 | ae02c8b2c2e5a430 | ae02c8b2c2e5a430, the same as entered |
| before, en | 835d77cbaea281a2 | 3f16408252f77822 | 835d77cbaea281a2 |
| after, en | 6306eec41018998c | 3055719a225fe6db | 3055719a225fe6db |

The moved and entered hashes differ between before and after only by the sect line.

## Contact sheets

`contact_after_ko.png` and `contact_after_en.png` show the 24 views after, four across and read row by row:

1. title, new_game, settings, hud
2. genealogy, genealogy_moved, genealogy_entered, genealogy_kp_entered
3. sect, sect_tab, arts, arts_down
4. factions, factions_down, dialogue, dialogue_news
5. pause, rebirth, closed (each panel opened and closed), hud_touch
6. genealogy_touch, arts_touch, factions_touch, sect_touch

## Pixel compare, before against after

`compare.gd` found 0 differing pixels in every view except these:

- **The genealogy views:** only the sect line's box.
- **genealogy_kp_entered:** the tree, now centred on the child.
- **A 17x17 patch (PC) or 23x23 patch (touch) on the player's marker at the screen's centre**, in hud_ko, closed_ko, dialogue_ko, dialogue_news_ko and en, hud_touch_ko and en. It is capture noise: two runs of the PR's code differ by the same patch.

The faction window's war confirmation is not captured. It needs a player who leads a faction.
