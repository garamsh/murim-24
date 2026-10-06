# Evidence for PR #64

Captures of feat/individual-npcs at 33bcc2dc68b4568400e34bf6bbb3e07856cb075a, showing every body as one person about a cell apart, for review against art/mockup/target_screen_4x.png. This branch is never merged.

- play_ko.png, play_ko_4x.png: play.tscn, frame 100, HUD in Korean; the first village's residents, one person each.
- play_en.png, play_en_4x.png: play.tscn, frame 170, after the locale switch at frame 120.
- m1_bench.png, m1_bench_4x.png: world_view/world_view.tscn (the make bench scene, M1 scenario of 500 people in one battle), frame 239.

Commands, from the project root after make build, with capture_play.gd and upscale.gd from this branch:

    godot --path . --write-movie <dir>/play.png --quit-after 180 --script capture_play.gd
    godot --path . --write-movie <dir>/m1.png --quit-after 240 res://world_view/world_view.tscn
    godot --headless --script upscale.gd -- <png>...
