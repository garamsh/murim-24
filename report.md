# Evidence for #398

Seed 2026, a new game born in village 0, Korean HUD, captured with `harness/capture.gd` on an Apple M1:

```sh
godot --path . --script harness/capture.gd -- <out prefix> <width>x<height> 0 2000
```

- `images/main_*`: main's camera rule, which is the fix branch with `camera_bounds()` returning the drawn bounds unchanged, as `_process()` used them on main e8322743.
- `images/fix_*`: the fix at 3618ec70.

At ticks 0 and 2000, 1920×1080 and 1280×720:
- **Main:** the player and the villagers beside it stand under the technique bar, and the talk prompt's slip ends at HUD y 509 against the bar's top at 510 (1920×1080), and at 679 against 690 (1280×720).
- **Fix:** the map's bottom rises to the bar's top. The player's feet stand on it, and the prompt's slip ends at 476 (1920×1080) and 645 (1280×720). Below the drawn bottom, past the raised ground's last row, the screen shows nothing, and only where the bars stand.

`logs/foot-main.log` is the new `play_test` pass under main's rule, run with `harness/foot.gd`: it failed at all 30 checks, every one on the player's body. `logs/foot-fix.log` is the same pass at the fix.
