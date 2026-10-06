# Evidence for PR #88

Captures of fix/hud-fit at 8aef887b4a8929b5f641922adf462e7162aaa2cf. This branch is never merged.

- hud_ko.png, hud_ko_4x.png: play.tscn, seed 2026, the player born into the region's first Cult village (village 1), frame 30, in Korean. Every line matches main.
- hud_en.png, hud_en_4x.png: the same run at frame 70, after F2 at frame 40. "Wanderer  Demonic Cult" and "C Challenge" end clear of the right edges of the 168-px status and leader panels. The map panel reads "Spring, Y1", and the rumour panel reads "No rumour has come yet.", both inside their panels.
- genealogy_en.png, genealogy_en_4x.png: the same run at frame 110, after G at frame 80 opened the genealogy panel on the player. The header and details read "Sama family".
- genealogy_ko.png, genealogy_ko_4x.png: the same run at frame 150, after F2 at frame 120: "사마씨 가문", unchanged.

"Dead" on a card, the 999-year season line and the widest surname are not on screen in these captures. hud_test.gd and genealogy_test.gd measure them.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/fit.png --quit-after 160 --script capture_fit.gd
    godot --headless --script upscale.gd -- <png>...
