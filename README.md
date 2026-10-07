# Evidence for PR #119

A frame of world_art.tscn in a 480 x 270 viewport, recorded by capture_seat.gd at feat/sect-seat-art 64905167411f27df1dba7d1a42a4ad7e17851ef5, the PR's head. This branch is never merged.

- seat.png: Hwasan's seat in the region of seed 2026, with the player born in village 0 and days of five ticks. The script ticks until at least 3 bodies stand on the training ground's cells and then saves the frame. The training ground is laid with the ground, and the bodies on its east edge are drawn over it and over its back-row racks. The manual hall stands at the head of the seat with a sect banner on each side, and the seat gate stands on its one-row footprint with a sect banner on each side. The houses, granaries and fields around the seat are the village's growth by year 190.
- seat_4x.png: the same frame at 4x, nearest neighbour.

The capture logged:

    capture_seat: Hwasan, yard [P: (13, 113), S: (6, 4)], after 113405 ticks (date { "year": 190, "season": 0, "day": 2 }), 3 bodies on the training ground, sha256 b0bb64d99c222e7c

The core gives no one a reason to gather on the training ground, so people cross it only in passing. 113,405 ticks is the first time 3 stood on it together.

## File hashes

SHA-256 of each file, first 16 hex digits:

    5bee1867099d0782  seat.png
    6729db13f202bdad  seat_4x.png

## Command

Run from the project root after make build:

    godot --path . --script capture_seat.gd -- <dir>
