# Evidence for PR #115

Captures of feat/sects at 90adb3caa618e11d340c27924c715363f4766af1, the PR's head after its rebase onto 28e7eb4 and ADR 0028. This branch is never merged. Each PNG is the frame its log line below names. The two files have different SHA-256 hashes, and the script fails if a capture repeats an earlier one.

sects_year0.png and sects_year100.png show the whole region of seed 2026, with the player born in village 0 and days of five ticks, at the start and after 100 years. Its three sects' seats are villages 7, 8 and 9. world_art does not draw a seat's training ground, manual hall or gate yet, so a seat shows its house, hall, granary and field. capture_sects.gd prints the living, the villages, the peopled villages, the Orthodox leader, and each sect's seat, whether it is held, its members, elders, master and cap:

    capture_sects: sects_year0.png, date { "year": 1, "season": 0, "day": 1 }, sha256 f4aca1ddabed5a1f, living 21, villages 10, peopled 10, Orthodox leader 0; ["Hwasan seat 7 held true members 2 elders 1 master 14 cap 20", "Heavenly Demon seat 8 held true members 2 elders 1 master 16 cap 20", "Black Fang seat 9 held true members 2 elders 1 master 18 cap 20"]
    capture_sects: sects_year100.png, date { "year": 101, "season": 0, "day": 1 }, sha256 5a87e968643e07c9, living 179, villages 36, peopled 17, Orthodox leader 206; ["Hwasan seat 7 held true members 20 elders 6 master 187 cap 20", "Heavenly Demon seat 8 held true members 20 elders 6 master 141 cap 20", "Black Fang seat 9 held true members 20 elders 6 master 136 cap 20"]

At year 100 each sect has 6 elders of 20 members, a third by ADR 0028's share. The Orthodox leader, 206, was elected after an earlier leader's death. The Orthodox election has no term, so a leader keeps the lead until it dies, even after another member rises to be Hwasan's master (187).

## File hashes

SHA-256 of each file, first 16 hex digits:

    0d7d63ff0c2aaf88  sects_year0.png
    34cf932a66aeaa3d  sects_year100.png

## Command

Run from the project root after make build:

    godot --path . --script capture_sects.gd -- <dir>
