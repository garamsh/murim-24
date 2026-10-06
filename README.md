# Evidence for PR #98

Frames of world_art.tscn in a window, recorded by capture_jump.gd over the region of seed 2026. The camera holds on the player's village until frame 20, then jumps to the village farthest from it, (1440, 224). world_art refreshes every frame and never recreates its bands. This branch is never merged.

- jump_main.png: frame 30, ten frames after the jump, with world_art.gd from main at 91f97a7. The village shows no body, although the capture logged `capture_jump: frame 30, camera (1440.0, 224.0), 12 bodies counted drawn`.
- jump_fixed.png: the same frame with world_art.gd from fix/band-bounds at 9f68ac1. The village's 12 bodies stand on the road and around the banner, and the log line is the same.

Both runs logged `capture_jump: frame 19, camera (72.0, 1672.0), 27 bodies counted drawn` before the jump.

Commands, from the project root after make build, with the world_art.gd under test in place:

    godot --path . --write-movie <dir>/jump.png --quit-after 40 --script capture_jump.gd
