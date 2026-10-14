# #209 closing round

Main `0473304f`, its own extension (`make build`). Apple M1, macOS, AC power, Godot 4.7.2.

- `images/`: Korean HUD captures at 1920x1080, seed 2026's birth village, zoom 3, standard quality (the settings' default): day, dusk, night, rain by day and rain by night.
- `logs/bench.log`: one `make bench` as it stands, then three rounds of the M1 battle at high and raining, the night village at high and raining with a hearth listed, and the village at morning at high (fog and clouds). The night-village runs there exited 2: the harness's setup check expected the dry night gain. `logs/bench-night-rain.log` holds those three runs again with the check expecting the rain-dimmed gain.
- `harness/`: m1_rain.gd, village.gd, bench.sh, capture.gd.
