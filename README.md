# Evidence for PR #125

A frame of world_art.tscn in a 480 x 270 viewport, recorded by capture_grown_seat.gd at fix/seat-court 5a4958f8b7deb3dacd79cedb34f47d37082c0137, the PR's head. This branch is never merged.

- grown_seat.png: Hwasan's seat in the region of seed 2026, with the player born in village 0 and days of five ticks, after 100 years. The court before the manual hall and the halls is paved; the road down column 8 from the court to the gate is drawn mostly behind the granary's roof and the gate, which rise above their footprints. Growth's houses, granaries, fields, a smithy and an apothecary stand around the seat, none on its court, its path or the two rows before its gate.
- grown_seat_4x.png: the same frame at 4x, nearest neighbour.
- The same seat before this change, grown to year 190, is evidence/pr-119/seat_4x.png.

The capture logged:

    capture_grown_seat: Hwasan, corner (13, 108), date { "year": 101, "season": 0, "day": 1 }, growth laid in view (offsets from the seat's corner) ["house at (5, -4)", "granary at (4, 10)", "field at (19, 10)", "house at (18, -6)", "granary at (16, -6)", "house at (-6, -7)", "house at (-9, 4)", "house at (-11, 10)", "barrels at (-7, 7)", "field at (-8, 10)", "smithy at (11, 8)", "field at (-5, 7)", "apothecary at (0, -3)"], 0 on its court or path, sha256 65b69029cc98503d

## File hashes

SHA-256 of each file, first 16 hex digits:

    75464e2d84006c78  grown_seat.png
    1a3fafedf864a312  grown_seat_4x.png

## Command

Run from the project root after make build:

    godot --path . --script capture_grown_seat.gd -- <dir>
