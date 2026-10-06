# Evidence for PR #68

Captures of feat/challenge-in-game at 3d87ebef2431cb72cdb2aabbbd68121f1a656b14. This branch is never merged.

- duel_started.png, duel_started_4x.png: play.tscn, the player born into the region's first Cult village (village 1), frame 120, after C at frame 30: the duel panel with both duellists' health, the leader panel and the notice that the challenge began.
- duel_again_refused.png, duel_again_refused_4x.png: the same run at frame 200, after C again at frame 180: the duel still fought, and the notice that a duel is already being fought.
- no_duel_rule.png, no_duel_rule_4x.png: the player born into an Orthodox village (village 0), frame 60, after C at frame 30: the notice that the faction does not settle its lead by duel.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/duel.png --quit-after 240 --script capture_challenge.gd -- 1
    godot --path . --write-movie <dir>/refused.png --quit-after 90 --script capture_challenge.gd -- 0
    godot --headless --script upscale.gd -- <png>...
