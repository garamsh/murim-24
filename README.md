# Evidence for the PC button bar mockup

Mockup of a PC button bar for every panel and screen a key opens, proposed for #186 (phase 1). This branch is never merged.

`button_bar.gd` made the images. It is the same file as `art/mockup/button_bar.gd` at 2bf86f4, the head of `feat/pc-button-bar`. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://button_bar.gd -- <dir>`.

What the images show:
- Seed 2026's region, just generated, in the real `play.tscn` at 1920x1080: the HUD at 2x in 960x540.
- The bar uses `hud/panel.gd`'s frame, font and palette. Its entries are genealogy (G), sect (M), factions (F), arts (K), settings (no key) and pause (Esc), each a cell with its key and a 9x9 icon. The first four icons are the touch buttons' (`hud/touch.gd`); settings (a gear), pause (two bars) and the map are the mockup's own.
- In every option the settings button under the map is gone: the bar's settings entry stands in for it.
- The hover slip reads the entry's hud.csv word and its key, for example "세력 (F)" / "Factions (F)". Pause reads "일시정지 · 조준 취소 (Esc)", the pause guide's word, since Esc also cancels an aim.

Names are `<option>_<view>_<locale>.png`. The options:
- `corner`: one row anchored to the room's bottom-right corner, on the technique bar's line.
- `beside`: the same row left of the technique bar, so the foot reads buttons, techniques, wares.
- `under_map`: a grid of three by two under the map, where the settings button stood; the slip shows left of it.
- `rail`: a labelled column at the right edge under the map, each row its icon, word and key. It needs no slip.
- `corner_map`: `corner` with the map's entry for #168 reserved, greyed, to show the row still fits with seven.

The views:
- `hover`: the cursor on the factions entry: its cell trimmed in gold, the slip over it.
- `open`: the genealogy panel open (G): its entry lit in jade, the bar drawn over the panel's dimmed room so the active entry reads.

The fit check the script printed for every image: the bar's frame is clear of the technique bar, the ware belt, the map and a 160x90 box at the room's centre, and inside the room. All 20 images passed all five.

| Option | Frame at 960x540 (HUD px) |
|---|---|
| corner | (740, 510) 216x26 |
| beside | (177, 510) 216x26 |
| under_map | (845, 70) 111x48 |
| rail | (813, 70) 143x102 ko, (815, 70) 141x102 en |
| corner_map | (705, 510) 251x26 |

Proposals for the PM:
1. **Placement: `corner`** (recommended). The foot already holds the technique bar and the belt, so the player looks there for controls, and the corner keeps the bar clear of the world's centre and of the dialogue window, which opens across the foot's middle. `beside` crowds the centre line. `under_map` and `rail` move a click target to the top while the keys' other controls sit at the foot.
2. **Map: omitted until #168.** A greyed entry has no key and does nothing, which breaks "clicking one does exactly what its key does". #168 adds one row to the table, and the bar and the pause guide both gain it. `corner_map` shows it still fits.
3. **A click does exactly what the key does, by feeding the key itself through the play scene's input.** So a click on another entry while a panel is open does what that key does there (G on the sect panel does nothing; the open panel's own entry closes it; Esc's entry closes it).
4. **The settings button under the map is removed**; the bar's settings entry replaces it and opens the settings as the button did.
5. **Touch keeps its own buttons and words.** The table drives their icons and which panel each opens; their words (TOUCH_*) and layout stay.
