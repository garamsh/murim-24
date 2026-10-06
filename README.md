# Evidence for PR #86

Captures of fix/hud-names at e1e538a1de1dccf956b80fa22f415dc0cfa50622. This branch is never merged.

- leader_duel_ko.png, leader_duel_ko_4x.png: play.tscn, seed 2026, the player born into the region's first Cult village (village 1), frame 120, after C at frame 30. The leader panel and the duel panel name the leader 강하린, as `person_name` gives, and the challenger 당신.
- leader_duel_en.png, leader_duel_en_4x.png: the same run at frame 200, after F2 at frame 150. The leader is named by given name alone, Harin, clear of "Leader" and "C Challenge".
- leader_orthodox_ko.png, leader_orthodox_ko_4x.png: the player born into an Orthodox village (village 0), frame 60, after C at frame 30. The leader panel names 독고지아, person 0, shown as "인물 0" before this change; the notice says the faction does not settle its lead by duel.
- leader_orthodox_en.png, leader_orthodox_en_4x.png: the same run at frame 200, after F2 at frame 150: Leader Jia.
- long_name_en.png, long_name_en_4x.png: hud.tscn alone, in English, over seed 11 with the player born into village 1, frame 20, after a challenge at its first frame. The Cult leader's given name, Cheonmyeong, is the widest the name data makes, and is cut short with an ellipsis in both panels. play.tscn's seed is fixed, so the world is not drawn beneath the HUD.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/v1.png --quit-after 240 --script capture_names.gd -- 1
    godot --path . --write-movie <dir>/v0.png --quit-after 240 --script capture_names.gd -- 0
    godot --path . --write-movie <dir>/long.png --quit-after 30 --script capture_long_name.gd
    godot --headless --script upscale.gd -- <png>...
