# Phase 2 captures of the nameplates

The built nameplates of PR #199 at 2163072, the head of `feat/nameplates`, in the real `play.tscn`. They show the same crowd as the mockup at the branch root. The PM's changes on PR #199 are applied: no overlap, the hostile named first, at most 8 names, and own names dimmer.

`capture.gd` made the images. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://capture.gd -- <dir>`. It stages the crowd as `art/mockup/nameplates.gd` at 9bde82c does, draws nothing of its own, and saves the window.

Names are `names_<window>_<locale>.png`, at 1280x720 (the HUD at 1x), 1920x1080 and 2560x1440 (2x), in ko and en.

What the script printed (relations: 0 own, 4 at war):

| Window | ko | en |
|---|---|---|
| 1280x720 | 7 at war, 1 own | 7 at war, 1 own |
| 1920x1080 | 5 at war, 3 own | 4 at war, 4 own |
| 2560x1440 | 7 at war, 1 own | 7 at war, 1 own |

43 people stood within 8 cells of the player in every image.

What to look at:
- Every Unorthodox body, at war with the player, has the crimson ring outside its ink outline, named or not. The ring is drawn in `world_art/body_band.gdshader`.
- No more than 8 names show. The hostile are named before anyone else, then the nearest. No name covers another, a panel, the purse or the talk prompt's slip; at least one pixel separates any two names.
- The neutral Demonic group on the left is unnamed in these images: the 8 names go to the hostile and to own people standing nearer. `hud/nameplates_test.gd` checks the neutral colour, paper `a8946c`, against a person within the reach.
- Own names are jade `2f8c6e`, dimmer than the mockup's `4fc49a`, as the PM asked. Hostile names are crimson `d63a2f`.
- The talk prompt shows "Hostile · will not talk" over the person within talk reach. That person's name is left out because the prompt's slip is kept clear.
