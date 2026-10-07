# Evidence for PR #123

Captures of feat/workshops at b7824d5ebdce0d7db0d3397a2d3f1c08b5f5affe, the PR's head. This branch is never merged. Each PNG is the frame its log line below names. The two differ in SHA-256, and the script fails if a capture repeats an earlier one.

The scene is play.tscn on seed 2026 with the player born in village 0. The script re-creates its world with days of five ticks so that villages grow for decades in minutes. It drives the ticks itself, holds the camera on what it captures, and hides the HUD, whose panels would cover the workshops.

- workshops_smithy_laid.png (and its _4x copy): the first smithy growth lays. Year 32, at (82, 92), north of a village. The residents' mend has already raised it from rubble to the destroyed stage.
- workshops_both_standing.png (and its _4x copy): the first time an intact smithy and an intact apothecary stand within 16 cells of each other. Year 62, in the same village: the smithy north of it at (82, 92), and the apothecary south of it at (81, 105).

capture_workshops.gd prints the tick, the date, the first 16 hex digits of each frame's SHA-256, and each workshop's kind, cell and stage (0 intact, 3 rubble):

    capture_workshops: workshops_smithy_laid.png at tick 18753, date { "year": 32, "season": 1, "day": 1 }, sha256 7904064369bf882e, workshops (kind, cell, stage) ["smithy (82, 92) 2"]
    capture_workshops: workshops_both_standing.png at tick 37178, date { "year": 62, "season": 3, "day": 26 }, sha256 1070aca6aa08f3e0, workshops (kind, cell, stage) ["smithy (82, 92) 0", "apothecary (81, 105) 0"]

SHA-256 of the saved files:

    7ed21dca4b902565852f75ae252fb26380588cc6308f423ab38c7709bab26aad  workshops_both_standing_4x.png
    f26afa2305d304c317425a0e3e20131f16cc528276a736a886f0a4434d5fcab9  workshops_both_standing.png
    b8bf3c0fc6c844029e31b937b2e82855d7d00db94351aaab33e7b3d557e00afe  workshops_smithy_laid_4x.png
    3263466da29a0c5832316cf511adbaa1460727d771a460d9b8e698d217a63546  workshops_smithy_laid.png

Commands, from the project root after make build, with each script copied there:

    godot --path . --script res://capture_workshops.gd -- <dir>
    godot --headless --path . --script res://upscale.gd -- <png>...
