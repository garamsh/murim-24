# Evidence for PR #112

Captures of feat/start-small at be8ae31f228955d44d53e7768e72f799a51917cf, the PR's head. This branch is never merged. Each PNG is the frame its log line below names. The six files have different SHA-256 hashes, and each script fails if a capture repeats an earlier one.

## The play session opens

play_start.png and play_genealogy.png (and their _4x copies) show play.tscn on seed 2026 with the player born in village 0, at tick 0.
- play_start.png: the village with its one founding household, a married man and woman, and the player born beside them.
- play_genealogy.png: G opens the genealogy panel on the player, who bears the Moyong family's name, as the player did before this PR.

capture_start.gd prints the player and the village's residents:

    capture_start: play_start.png at tick 0, sha256 98da73671e33cccb; player 14 Moyong Yeonhwa of family 7 in village 1, whose residents are ["Jo Doyun, spouse 1", "Jang Maehwa, spouse 0"]
    capture_start: play_genealogy.png at tick 0, sha256 faa53f89cae00b8a; player 14 Moyong Yeonhwa of family 7 in village 1, whose residents are ["Jo Doyun, spouse 1", "Jang Maehwa, spouse 0"]

## A century

century_year0.png and century_year100.png show the whole region of seed 2026, with the player in village 0 and days of five ticks, as the core's century test lives it, at the start and after 100 years. capture_century.gd prints the living, all villages and the peopled ones:

    capture_century: century_year0.png, date { "year": 1, "season": 0, "day": 1 }, sha256 4855724689fdc3bd, living 15, villages 7, peopled 7
    capture_century: century_year100.png, date { "year": 101, "season": 0, "day": 1 }, sha256 310a0fafa11730a9, living 96, villages 33, peopled 17

Villages laid out but not yet settled, and those raided empty, count among the 33 and not among the 17 peopled ones. Dark roofs are burnt structures.

## File hashes

SHA-256 of each file, first 16 hex digits:

    070174a7f0189c8a  century_year0.png
    7fe8eef46a7fa356  century_year100.png
    1b0be6bc7f3f1ac7  play_genealogy_4x.png
    e73a68349cfa3c70  play_genealogy.png
    91dc78260120f31c  play_start_4x.png
    6199c91118699811  play_start.png

## Commands

Run from the project root after make build:

    godot --path . --script capture_start.gd -- <dir>
    godot --path . --script capture_century.gd -- <dir>
    godot --headless --script upscale.gd -- <png>...
