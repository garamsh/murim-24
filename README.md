# Evidence for PR #142

Captures of feat/save-menu at eb65759, the PR's head, in a 480 x 270 window, each with a _4x nearest-neighbour copy. This branch is never merged.

capture_saves.gd drives play.tscn as a player does, through play.gd's navigate(), in each locale. It fails if any two images repeat. The language is chosen on the settings panel at each opening, and kept in a file of the script's own, which it removes at the end. A save already in slot 0 is set aside before the run and put back after it. None was there for this run.

- title_no_save_ko.png, title_no_save_en.png: the scene as it opens with slot 0 empty. Continue is greyed with 저장 없음 / No save, and `load_refusal(0)` is 0, `LOAD_MISSING`.
- pause_saved_ko.png, pause_saved_en.png: after a new game from seed 4817203365, Escape, Down and Enter on Save. The line under the menu reads 저장했다. / Saved.
- title_continue_ko.png, title_continue_en.png: the scene opened again, as leaving for the title reloads it, with Down choosing Continue, which can now be taken.
- continued_ko.png, continued_en.png: after Enter on Continue. The world is resumed, steering army 10, the army `player_army` names. The hash in the slot is the hash saved.
- title_load_error_ko.png, title_load_error_en.png: the scene opened again over a slot 0 holding the bytes `not a save`, after Down and Enter on Continue. No world starts, and the line under the menu names `LOAD_NOT_A_SAVE`: 이 게임의 저장이 아니라 이어할 수 없다. / Not a save of this game, so it cannot continue.

Run: `godot --path . --script <capture_saves.gd> -- <dir>`. It logged:

    capture_saves: title_no_save_ko.png 480x270, sha256 428be8c227ffda72, locale ko; unavailable ["continue"], slot 0 refusal 0
    capture_saves: pause_saved_ko.png 480x270, sha256 a2232f8ce9c76eaf, locale ko; chosen save, notice PAUSE_SAVED, seed 4817203365, ticks 4, hash 6399365181124728065, slot 0 refusal -1
    capture_saves: title_continue_ko.png 480x270, sha256 3eb638792f8bf7c3, locale ko; chosen continue, unavailable []
    capture_saves: continued_ko.png 480x270, sha256 88b0bb8bfaf7d9ea, locale ko; world seed 4817203365, steered army 10, player_army 10, screen <null>, hash at load 6399365181124728065
    capture_saves: title_load_error_ko.png 480x270, sha256 acd782e2d17dfaa1, locale ko; world <null>, notice TITLE_LOAD_NOT_A_SAVE, slot 0 refusal 1
    capture_saves: title_no_save_en.png 480x270, sha256 26a3464c89cbb60f, locale en; unavailable ["continue"], slot 0 refusal 0
    capture_saves: pause_saved_en.png 480x270, sha256 9e958867b81cee64, locale en; chosen save, notice PAUSE_SAVED, seed 4817203365, ticks 5, hash -6261549340689345155, slot 0 refusal -1
    capture_saves: title_continue_en.png 480x270, sha256 b40a490895c16ff0, locale en; chosen continue, unavailable []
    capture_saves: continued_en.png 480x270, sha256 5be6a4733fb677c7, locale en; world seed 4817203365, steered army 10, player_army 10, screen <null>, hash at load -6261549340689345155
    capture_saves: title_load_error_en.png 480x270, sha256 f662a2b014d000d9, locale en; world <null>, notice TITLE_LOAD_NOT_A_SAVE, slot 0 refusal 1
    capture_saves: 10 images, none repeated true
