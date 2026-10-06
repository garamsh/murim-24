# Evidence for PR #71

Captures of fix/combat-pacing at ddd3e842d1a7cfbd7f8501badaf7a374c145a6fa. This branch is never merged.

- duel_started.png, duel_started_4x.png: play.tscn with the player born into the region's first Cult village (village 1), frame 40, just after C at frame 30. The duel panel shows both duellists, the leader panel shows person 26 and the notice says the challenge began. The Cult houses are drawn intact.
- duel_s01.png to duel_s16.png: one still for each second of play after the challenge, NN seconds in. Both duellists' bars fall as the duel is fought. duel_s08_4x.png is the 4x copy of the eighth second.
- duel_settled.png, duel_settled_4x.png: 20 frames after the duel settled, 488 ticks (16.3 s) after the challenge. The player leads the faction and the notice says a new leader stands. The houses are still intact.

Commands, from the project root after make build:

    godot --path . --write-movie <dir>/duel.avi --script capture_duel.gd -- 1 <dir>
    godot --headless --script upscale.gd -- <png>...

capture_duel.gd prints the challenge and the settlement:

    capture_duel: challenge at frame 30, tick 14, duel { "challenger": 140, "leader": 26 }
    capture_duel: settled at frame 1005, 488 ticks (16.3 s) after the challenge; leader 140

--write-movie fixes the frame rate at 60, so the run is the same every time. The AVI it writes is discarded; the stills are the record.
