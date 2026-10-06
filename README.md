# Evidence for PR #91

Captures of fix/name-fit at 69749fe, from hud.tscn alone over seed 11 with the player born into village 1, the region's first Cult village. That village's leader, Hwang Cheonmyeong (황천명), bears the widest given name the name data makes. The player challenges the leader at the first frame. play.tscn's seed is fixed, so the world is not drawn beneath the HUD. This branch is never merged.

- genealogy_ko.png, genealogy_ko_4x.png: frame 30, the genealogy panel open on the leader, in Korean. "황천명" is drawn whole on the card and in the column, as on main.
- genealogy_en.png, genealogy_en_4x.png: frame 70, after F2 at frame 40. The column shows "Cheonmyeong…" and the card "Cheon…", each within its frame. "Hwang family" and "Demonic Cult" are whole.
- leader_duel_en.png, leader_duel_en_4x.png: frame 110, after the panel closed at frame 80. The leader panel shows "Cheonmy…" and the duel panel "Cheonm…", cut by their labels' ellipsis as before.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/names.png --quit-after 120 --script capture_names.gd
    godot --headless --script upscale.gd -- <png>...
