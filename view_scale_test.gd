# Checks the scales the window draws at. On a PC, at 1280x720, 1920x1080 and 2560x1440, the world
# takes 2, 3 and 4 window pixels a pixel and so shows 640x360 of the world, and the HUD 1, 2 and 2,
# laid out in 1280x720, 960x540 and 1280x720 of its own pixels from the window's corner; a window
# smaller than 640x360 still draws at 1. Control: one more than each world scale would show less
# than 640x360. With the touch controls, at 1920x1080 and on a 2400x1080 phone, the world and the
# HUD take 4, and the HUD keeps 480x270 in the middle of the window. Over desktop and phone windows
# of every common shape, odd ones among them, both scales are whole numbers of at least 1, so the
# world and the HUD never draw at a non-integer scale (#209).
extends RefCounted

const ViewScale := preload("res://view_scale.gd")
## Each PC window, with its world scale, HUD scale and HUD room.
const PC := [
	[Vector2i(1280, 720), 2, 1, Rect2i(0, 0, 1280, 720)],
	[Vector2i(1920, 1080), 3, 2, Rect2i(0, 0, 960, 540)],
	[Vector2i(2560, 1440), 4, 2, Rect2i(0, 0, 1280, 720)],
	[Vector2i(600, 300), 1, 1, Rect2i(0, 0, 600, 300)],
]
## Windows of desktop monitors, laptops, phones and tablets, in landscape, odd sizes among them.
const SWEEP := [Vector2i(800, 480), Vector2i(1024, 600), Vector2i(1280, 800), Vector2i(1366, 768),
	Vector2i(1440, 900), Vector2i(1536, 864), Vector2i(1600, 900), Vector2i(1919, 1079), Vector2i(2340, 1080),
	Vector2i(2532, 1170), Vector2i(2560, 1600), Vector2i(2732, 2048), Vector2i(3440, 1440), Vector2i(3840, 2160)]
const TOUCH := [
	[Vector2i(1920, 1080), 4, 4, Rect2i(0, 0, 480, 270)],
	[Vector2i(2400, 1080), 4, 4, Rect2i(240, 0, 480, 270)],
]


func run() -> bool:
	var wrong := []
	var widest := []
	for touch: bool in [false, true]:
		for case: Array in (TOUCH if touch else PC):
			var window: Vector2i = case[0]
			var got := [ViewScale.world_scale(window, touch), ViewScale.hud_scale(window, touch),
					ViewScale.hud_room(window, touch)]
			if got != case.slice(1):
				wrong.append("%s touch %s: %s, not %s" % [window, touch, got, case.slice(1)])
			# Control: a scale one more shows less than the least view, where the window holds it.
			var least := ViewScale.TOUCH_VIEW if touch else ViewScale.PC_VIEW
			var world_scale: int = got[0]
			var shown := window / world_scale
			var more := window / (world_scale + 1)
			widest.append((shown.x >= least.x and shown.y >= least.y) or world_scale == 1)
			if more.x >= least.x and more.y >= least.y:
				wrong.append("%s touch %s: %dx still shows %s" % [window, touch, world_scale + 1, more])
	var fractional := []
	for touch: bool in [false, true]:
		for window: Vector2i in SWEEP:
			for scale: Variant in [ViewScale.world_scale(window, touch), ViewScale.hud_scale(window, touch)]:
				if typeof(scale) != TYPE_INT or scale < 1:
					fractional.append("%s touch %s: %s" % [window, touch, scale])
	var passed := wrong.is_empty() and widest.all(func(holds: bool) -> bool: return holds) and fractional.is_empty()
	print("view_scale_test: %d PC and %d touch windows; at least the least view %s; wrong %s; %d windows swept with and without touch, scales not whole %s: %s" % [
		PC.size(), TOUCH.size(), widest, wrong, SWEEP.size(), fractional, "pass" if passed else "FAIL"])
	return passed
