# Evidence for PR #156

Captures of play.tscn at the native 480 x 270 over the region of seed 2026, recorded by capture_quick_cast.gd at feat/quick-cast 5bbf3cc, in Korean (`_ko`) and English (`_en`). This branch is never merged.

- aim_ko.png, aim_en.png: casting on release, Q held with the cursor north-east of the person inside the bolt's 3-cell range; the range is the dashed ring round the person, a dotted jade line runs out to the cursor and the aimed area sits there.
- aim_past_range_ko.png, aim_past_range_en.png: the same with the cursor beyond the range; the area is drawn on the ring along the line to the cursor, where the core brings such a point.
- refused_ko.png, refused_en.png: casting on press, a cast at the cursor, a tick, and a second cast; the core refuses it as on cooldown (`CAST_ON_COOLDOWN` = 3) and the HUD's notice says so.
- guide_ko.png, guide_en.png: Escape pauses; the controls guide shows the right click, QWERT, 1-9, 0, C, G, M, F2 and Escape as play.gd binds them.
- settings_ko.png, settings_en.png: the settings panel on its new casting row, on press or on release, with what it sets under the rows.

The capture's log lines:

    capture_quick_cast: saved aim_ko.png; aiming Q, the aim shown true from (1256.0, 1528.0) to (1286.0, 1514.0), range 48 px, area 6 px; notice ""
    capture_quick_cast: saved aim_en.png; aiming Q, the aim shown true from (1256.0, 1528.0) to (1286.0, 1514.0), range 48 px, area 6 px; notice ""
    capture_quick_cast: saved aim_past_range_ko.png; aiming Q, the aim shown true from (1256.0, 1528.0) to (1302.0, 1540.0), range 48 px, area 6 px; notice ""
    capture_quick_cast: saved aim_past_range_en.png; aiming Q, the aim shown true from (1256.0, 1528.0) to (1302.0, 1540.0), range 48 px, area 6 px; notice ""
    capture_quick_cast: first cast accepted true, second refused 3
    capture_quick_cast: saved refused_ko.png; aiming , the aim shown false from (0.0, 0.0) to (0.0, 0.0), range 0 px, area 0 px; notice "CAST_ON_COOLDOWN"
    capture_quick_cast: first cast accepted true, second refused 3
    capture_quick_cast: saved refused_en.png; aiming , the aim shown false from (0.0, 0.0) to (0.0, 0.0), range 0 px, area 0 px; notice "CAST_ON_COOLDOWN"
    capture_quick_cast: saved guide_ko.png; aiming , the aim shown false from (0.0, 0.0) to (0.0, 0.0), range 0 px, area 0 px; notice ""
    capture_quick_cast: saved guide_en.png; aiming , the aim shown false from (0.0, 0.0) to (0.0, 0.0), range 0 px, area 0 px; notice ""
    capture_quick_cast: saved settings_ko.png; aiming , the aim shown false from (0.0, 0.0) to (0.0, 0.0), range 0 px, area 0 px; notice ""
    capture_quick_cast: saved settings_en.png; aiming , the aim shown false from (0.0, 0.0) to (0.0, 0.0), range 0 px, area 0 px; notice ""

Command, from the project root after make build:

    godot --path . --resolution 480x270 --script capture_quick_cast.gd -- <dir>
