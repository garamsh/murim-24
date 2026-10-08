## The whole-pixel scales the window draws at: the world's, which fills the window with as many
## world pixels as it holds at that scale, and the HUD's, with the room the HUD is laid out in.
## On a PC the world shows at least PC_VIEW and the HUD is drawn at half the world's scale, rounded
## up; with the touch controls the world shows at least TOUCH_VIEW and the HUD is drawn at the
## world's scale in TOUCH_VIEW, centred, as its touch layout is sized for.
extends RefCounted

## The least world pixels the view shows on a PC, and with the touch controls.
const PC_VIEW := Vector2i(640, 360)
const TOUCH_VIEW := Vector2i(480, 270)


## Returns how many window pixels each world pixel takes: the most that still shows `least` world
## pixels across and down a window of `window`, and never less than 1.
static func world_scale(window: Vector2i, touch: bool) -> int:
	var least := TOUCH_VIEW if touch else PC_VIEW
	return maxi(mini(window.x / least.x, window.y / least.y), 1)


## Returns how many window pixels each HUD pixel takes in a window of `window`.
static func hud_scale(window: Vector2i, touch: bool) -> int:
	var world := world_scale(window, touch)
	return world if touch else (world + 1) / 2


## Returns where the HUD is laid out in a window of `window`, in HUD pixels from the window pixel
## at its corner: the whole window on a PC, and TOUCH_VIEW centred in it with the touch controls.
static func hud_room(window: Vector2i, touch: bool) -> Rect2i:
	var scale := hud_scale(window, touch)
	if touch:
		return Rect2i((window - TOUCH_VIEW * scale) / 2, TOUCH_VIEW)
	return Rect2i(Vector2i.ZERO, window / scale)
