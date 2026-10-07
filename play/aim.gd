## The aim of a skill held ready to cast, drawn on the ground as art/mockup/mobile_hud_aim.png
## draws it: the skill's range a dashed ring round the caster, a dotted line out to where it is
## aimed, and its area there in jade. It holds no game rule: the range and area are the core's.
extends Node2D

const PAPER := Color("efe3c2")
const JADE := Color("4fc49a")
const JADE_DARK := Color("2f8c6e")
# Pixels between the dots of the line out to the aim, and in each dash of the range's ring.
const DOT_STEP := 5.0
const DASH := 4.0
# The radius the area is drawn at when the skill's own is smaller, so a single target shows.
const LEAST_AREA := 6.0

## The caster, the aimed point, the range and the area's radius, in pixels, as last shown.
var caster := Vector2.ZERO
var point := Vector2.ZERO
var reach := 0.0
var area := 0.0


func _ready() -> void:
	visible = false


## Shows the aim of a skill reaching `range_pixels` from `from`, its area `radius` across, at `at`,
## drawn at the ring where `at` lies beyond it, as the core brings such a point within range.
func show_aim(from: Vector2, range_pixels: float, at: Vector2, radius: float) -> void:
	caster = from.floor()
	reach = range_pixels
	area = maxf(radius, LEAST_AREA)
	point = (from + (at - from).limit_length(range_pixels)).floor()
	visible = true
	queue_redraw()


func _draw() -> void:
	_ring(caster, reach, PAPER, true)
	var length := caster.distance_to(point)
	var t := 0.0
	while t < length:
		draw_rect(Rect2(caster.lerp(point, t / length).floor(), Vector2(2, 2)), JADE)
		t += DOT_STEP
	for y in range(-area, area + 1):
		for x in range(-area, area + 1):
			if (x + y) % 2 == 0 and y % 2 == 0 and Vector2(x, y).length() < area - 1:
				draw_rect(Rect2(point + Vector2(x, y), Vector2.ONE), JADE_DARK)
	_ring(point, area, JADE, false)


# A ring `radius` round `centre`, one pixel thick, every other run of DASH pixels when `dashed`.
func _ring(centre: Vector2, radius: float, colour: Color, dashed: bool) -> void:
	if radius <= 0.0:
		return
	var steps := maxi(int(TAU * radius), 8)
	for n in steps:
		if dashed and int(n / DASH) % 2 == 1:
			continue
		var angle := TAU * n / steps
		draw_rect(Rect2((centre + Vector2(cos(angle), sin(angle)) * radius).floor(), Vector2.ONE), colour)
