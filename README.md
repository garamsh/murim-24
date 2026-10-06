# Evidence for PR #59

Captures of feat/village-props at f3e3b7f85c57fc520de2e6d8570705ac8ce6b143, for comparison with art/mockup/target_screen_4x.png. This branch is never merged.

- play_ko.png, play_ko_4x.png: play.tscn, frame 230, HUD in Korean, the player ordered toward its village's square on frame 2.
- play_en.png, play_en_4x.png: play.tscn, frame 290, after the locale switch at frame 240.
- m1_bench.png, m1_bench_4x.png: world_view/world_view.tscn (the make bench scene, M1 scenario), frame 239, its barricade drawn as barrels.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/play.png --quit-after 300 --script capture_play.gd
    godot --path . --write-movie <dir>/m1.png --quit-after 240 res://world_view/world_view.tscn
    godot --headless --script upscale.gd -- <png>...
