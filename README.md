# Captures of the target selection and attack intent

The built play scene of PR #237 at `7c8adf4`, the head of `feat/targeted-basic-attack`, in Korean at 1920x1080. They show seed 2026, born into village 0.

`capture.gd` made the images. I ran it with Godot 4.7.2 on macOS arm64 after `make build`: copied to the project root, then `godot --path . --script res://capture.gd -- <dir>`. It marches the nearest hostile army, steered, into the player's sight, as `play_test`'s `_intent_pass` does, then drives the play scene with the same input events a player sends.

What the script printed:

| Image | Input | Selected | Intent | Target frame |
|---|---|---|---|---|
| `select_ko.png` | left click on the nearest foe | 8 | none | 윤청운, 마교, 체력 120 |
| `cycle_ko.png` | Tab | 9 | none | 강설화, 마교, 체력 120 |
| `attack_ko.png` | Space, then ticks until the chase is within the guard strike's reach (62 ticks) | 9 | 9 | 강설화, 마교, 체력 120 |
| `own_ko.png` | left click on the nearest person of the player's own faction, out of talk reach | 1 | none | 한설화 |

What to look at:
- The target frame at the top middle names the selection, in crimson for a hostile faction, with its faction and health.
- The selected person's name is framed in gold.
- In `select_ko.png` and `cycle_ko.png` the player stands where it was ordered: selecting does not move it.
- In `attack_ko.png` the player has walked out of the village to the selected foe and is fighting it. The foe's name is left out because the talk prompt's hostile-refusal slip stands there, and the nameplates keep that slip clear (#199). Its health still reads 120 because the capture stops on arrival, before the first strike lands.
- In `own_ko.png` a person of the player's own faction is selected and named in jade in the frame. A click within talk reach would open the dialogue instead; `play_test`'s `_talk_pass` shows that.
