# Evidence for #157

Captures of the touch controls in `play.tscn` at 0dd597b, made by `capture_touch.gd` (`godot --path . --script capture_touch.gd -- <dir>`). Each is 480x270, with a 4x nearest-neighbour copy, in Korean and English.

- `mobile_hud_idle_*`: at rest, the body cooling down after its last strike.
- `mobile_hud_aim_*`: the first skill dragged to aim, with the range ring, the aimed area, the pad and the cancel zone.
- `mobile_hud_cancel_*`: the same drag carried onto the cancel zone.
- `mobile_hud_self_*`: the second skill held past a long press.
- `mobile_hud_targets_*`: every place a touch lands, outlined and sized from `hud/touch.gd`'s constants. The cooldown notice shows because the long press released just before cast on self while the body was cooling.
