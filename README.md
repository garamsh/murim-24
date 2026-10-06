# Evidence for PR #51

Captures of feat/visible-game at b7f05e0124d829ac13d6facfd8c884a9a61a4a81, for comparison with art/mockup/target_screen_4x.png. This branch is never merged.

- play_ko.png, play_ko_4x.png: play.tscn, frame 100, HUD in Korean.
- play_en.png, play_en_4x.png: play.tscn, frame 170, after the locale switch at frame 120.
- m1_bench.png, m1_bench_4x.png: world_view/world_view.tscn (the make bench scene, M1 scenario), frame 239.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/play.png --quit-after 180 --script capture_play.gd
    godot --path . --write-movie <dir>/m1.png --quit-after 240 res://world_view/world_view.tscn
    godot --headless --script upscale.gd -- <png>...
