## The touch controls over the play scene, drawn as art/mockup/mobile_hud_idle.png,
## mobile_hud_aim.png, mobile_hud_cancel.png and mobile_hud_self.png lay them out after
## art/mockup/touch.lua: a joystick at the bottom left that a thumb anywhere in its zone puts under
## itself; up to SKILLS.size() round skill buttons in an arc at the bottom right, each showing how
## long the body has left before it may strike again; the genealogy, sect and faction buttons under the
## status panel, with the arts button as art/mockup/mobile_hud_arts.png adds it; and the pause
## button in the top-right corner. A skill button tapped casts at what the facing aims at, dragged
## shows its range and the area it aims at and casts there on release, dropped on the cancel zone
## casts nothing, and held casts on the body itself; a button whose slot is empty shows a cross and
## casts nothing. While the arts panel is open, as arts_panel_touch.png shows it, only the arc is
## drawn, ringed in jade while a skill waits to be placed, and the pause button's place closes the
## panel. take() turns each touch into what the play scene does with it; the ranges, the readiness
## and where the body stands on screen are given to it, read from the core by the play scene. It
## holds no game rule.
extends "res://hud/panel.gd"

## The skill buttons, the main one first: centre, drawn radius and the radius a touch lands in.
const SKILLS: Array[Dictionary] = [
	{"at": Vector2(440, 226), "r": 20, "hit": 24},
	{"at": Vector2(382, 226), "r": 16, "hit": 21},
	{"at": Vector2(399, 185), "r": 16, "hit": 21},
	{"at": Vector2(440, 168), "r": 16, "hit": 21},
	{"at": Vector2(330, 238), "r": 14, "hit": 21},
]
## The cancel zone, shown only while a skill is dragged.
const CANCEL := {"at": Vector2(452, 116), "r": 18, "hit": 24}
## The joystick at rest, its radius and its knob's; a touch anywhere in STICK_ZONE puts it under
## the thumb.
const STICK := Vector2(64, 212)
const STICK_RADIUS := 30.0
const KNOB_RADIUS := 12
const STICK_ZONE := Rect2(0, 140, 200, 130)
## How far the knob moves before the stick points anywhere.
const STICK_DEAD := 4.0
## How far a skill button's drag reaches; a drag that far aims at the skill's full range.
const PAD := 36.0
## How far a touch on a skill button moves before it is a drag, and how long it is held still
## before it is a long press.
const SLOP := 6.0
const LONG_PRESS_MS := 500
## The square buttons, each with the hud.csv key of its word, and the pause button.
const BUTTONS := {
	"genealogy": Rect2(4, 92, 42, 42),
	"sect": Rect2(50, 92, 42, 42),
	"factions": Rect2(96, 92, 42, 42),
	"arts": Rect2(142, 92, 42, 42),
}
const BUTTON_KEYS := {"genealogy": "TOUCH_GENEALOGY", "sect": "TOUCH_SECT", "factions": "TOUCH_FACTIONS",
		"arts": "TOUCH_ARTS"}
const PAUSE := Rect2(434, 4, 42, 42)
## The room each word is given, by its hud.csv key.
const ROOM := {"TOUCH_GENEALOGY": 36, "TOUCH_SECT": 36, "TOUCH_FACTIONS": 36, "TOUCH_ARTS": 36,
		"TOUCH_CANCEL": 40, "TOUCH_SELF": 50, "TOUCH_PLACE": 110}
## Where the slip asking for a button to place a skill on is centred, and its top.
const PLACE_SLIP := Vector2(404, 136)
## How far a finger on the arts panel moves up or down before it scrolls the panel's tiles.
const SCROLL := 20.0
const Shortcuts := preload("res://hud/shortcuts.gd")

## How many skill buttons are shown: the slots given, up to SKILLS.size().
var skill_count := 0
## Each shown slot's skill: its `icon` tag in art/icons/skill_icons, its `range` and its `radius` in
## pixels; an empty Dictionary for an empty slot.
var reach: Array[Dictionary] = []
## Whether the arts panel is open over the controls, so only the arc is drawn and touched, and
## whether a skill chosen on it waits for a button to be placed on.
var placing := false
var waiting := false
## Each square button's icon, by the button's name: a key of Shortcuts.ICONS, as the play scene's
## shortcut table names it; a button with none shows its word alone.
var icons := {}
## Where the steered body stands on screen, which the rings on the ground are drawn round.
var body_at := Vector2(240, 135)
## The share of the body's cooldown still to run, and the whole seconds it lasts; 0 when ready.
var cooling := 0.0
var cooling_seconds := 0
## The clock long presses are timed by, in milliseconds; a test gives its own.
var clock: Callable = Time.get_ticks_msec

var _discs := {}
var _dither := {}
# Each finger on the screen, by its index: a Dictionary of its `kind`, "stick", "skill", "button"
# or "world", and what that kind keeps.
var _fingers := {}
# Where the joystick's base is while a thumb holds it, and its knob's offset from there.
var _stick_base := STICK
var _knob := Vector2.ZERO


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	size = Vector2(480, 270)


func _process(_delta: float) -> void:
	# A held skill button turns into a long press while it is still, which the drawing shows.
	for finger: Dictionary in _fingers.values():
		if finger.kind == "skill" and not finger.self_shown and _held_long(finger):
			finger.self_shown = true
			queue_redraw()


## Shows a skill button for each of `skills`, its icon tag, range and radius in pixels as `skills`
## gives them, or a cross for an empty Dictionary.
func show_skills(skills: Array[Dictionary]) -> void:
	reach = skills
	skill_count = mini(skills.size(), SKILLS.size())
	queue_redraw()


## Turns `event`, a touch or a drag in the HUD's 480x270, into what the play scene does: a
## Dictionary of `do`, one of "steer" with the stick's `way`, a Vector2 of length 1 at most;
## "cast" with the skill button's `slot` and `how`, "tap", "point" with `aim`, the drag as a share
## of PAD, or "self"; "button" with its `button`, a key of BUTTONS or "pause"; "world" with `at`,
## the place tapped; or "none" when a finger took the event and asks nothing yet. While `placing`,
## it is "place" with the `slot` of the skill button pressed, "button" with "close" for the pause
## button's place, and for a finger lifted elsewhere, "panel" with `at`, where it was pressed, or
## "scroll" with `way`, 1 for a finger moved up and -1 down. Returns an empty Dictionary for an
## event that is no touch.
func take(event: InputEvent) -> Dictionary:
	var touch := event as InputEventScreenTouch
	var drag := event as InputEventScreenDrag
	if placing:
		return _take_placing(touch, drag)
	if touch != null and touch.pressed:
		return _press(touch.index, touch.position)
	if touch != null:
		return _release(touch.index)
	if drag != null and _fingers.has(drag.index):
		return _drag(drag.index, drag.position)
	return {}


## Returns the skill being dragged and where it aims, as a share of PAD, or an empty Dictionary
## while none is.
func aiming() -> Dictionary:
	for finger: Dictionary in _fingers.values():
		if finger.kind == "skill" and finger.aiming:
			return {"slot": finger.slot, "aim": finger.aim, "cancel": finger.over_cancel}
	return {}


## Returns whether a skill button has been held long enough to cast on the body itself.
func holding_self() -> bool:
	for finger: Dictionary in _fingers.values():
		if finger.kind == "skill" and not finger.aiming and _held_long(finger):
			return true
	return false


## Lets go of every finger, as when a screen opens over the HUD.
func release_all() -> void:
	_fingers.clear()
	_stick_base = STICK
	_knob = Vector2.ZERO
	queue_redraw()


func _take_placing(touch: InputEventScreenTouch, drag: InputEventScreenDrag) -> Dictionary:
	if touch != null and touch.pressed:
		queue_redraw()
		if PAUSE.has_point(touch.position):
			return {"do": "button", "button": "close"}
		for slot in skill_count:
			if touch.position.distance_to(SKILLS[slot].at) <= SKILLS[slot].hit:
				return {"do": "place", "slot": slot}
		_fingers[touch.index] = {"kind": "panel", "from": touch.position, "at": touch.position}
		return {"do": "none"}
	if drag != null and _fingers.has(drag.index):
		_fingers[drag.index].at = drag.position
		return {"do": "none"}
	if touch == null or not _fingers.has(touch.index):
		return {}
	var finger: Dictionary = _fingers[touch.index]
	_fingers.erase(touch.index)
	# A finger pressed before the panel opened, such as the one on the arts button, asks nothing.
	if finger.kind != "panel":
		return {"do": "none"}
	var moved: float = finger.at.y - finger.from.y
	if absf(moved) >= SCROLL:
		return {"do": "scroll", "way": 1 if moved < 0 else -1}
	return {"do": "panel", "at": finger.from}


func _press(index: int, at: Vector2) -> Dictionary:
	queue_redraw()
	if PAUSE.has_point(at):
		_fingers[index] = {"kind": "button"}
		return {"do": "button", "button": "pause"}
	for button: String in BUTTONS:
		if BUTTONS[button].has_point(at):
			_fingers[index] = {"kind": "button"}
			return {"do": "button", "button": button}
	for slot in skill_count:
		if at.distance_to(SKILLS[slot].at) <= SKILLS[slot].hit and reach[slot].is_empty():
			_fingers[index] = {"kind": "button"}
			return {"do": "none"}
		if at.distance_to(SKILLS[slot].at) <= SKILLS[slot].hit:
			_fingers[index] = {"kind": "skill", "slot": slot, "from": at, "since": clock.call(),
					"aiming": false, "aim": Vector2.ZERO, "over_cancel": false, "self_shown": false}
			return {"do": "none"}
	if STICK_ZONE.has_point(at):
		_fingers[index] = {"kind": "stick"}
		_stick_base = at
		_knob = Vector2.ZERO
		return {"do": "none"}
	_fingers[index] = {"kind": "world"}
	return {"do": "world", "at": at}


func _drag(index: int, at: Vector2) -> Dictionary:
	var finger: Dictionary = _fingers[index]
	if finger.kind == "stick":
		_knob = (at - _stick_base).limit_length(STICK_RADIUS)
		queue_redraw()
		return {"do": "steer", "way": _way()}
	if finger.kind == "skill":
		var centre: Vector2 = SKILLS[finger.slot].at
		if not finger.aiming and at.distance_to(finger.from) > SLOP:
			finger.aiming = true
		if finger.aiming:
			finger.aim = (at - centre).limit_length(PAD) / PAD
			finger.over_cancel = at.distance_to(CANCEL.at) <= CANCEL.hit
			queue_redraw()
	return {"do": "none"}


func _release(index: int) -> Dictionary:
	if not _fingers.has(index):
		return {}
	var finger: Dictionary = _fingers[index]
	_fingers.erase(index)
	queue_redraw()
	if finger.kind == "stick":
		_stick_base = STICK
		_knob = Vector2.ZERO
		return {"do": "steer", "way": Vector2.ZERO}
	if finger.kind != "skill":
		return {"do": "none"}
	if finger.aiming:
		if finger.over_cancel:
			return {"do": "none"}
		return {"do": "cast", "slot": finger.slot, "how": "point", "aim": finger.aim}
	return {"do": "cast", "slot": finger.slot, "how": "self" if _held_long(finger) else "tap"}


# The way the stick points, of length 1 at most, or none within STICK_DEAD of its middle.
func _way() -> Vector2:
	return Vector2.ZERO if _knob.length() < STICK_DEAD else _knob / STICK_RADIUS


func _held_long(finger: Dictionary) -> bool:
	return int(clock.call()) - int(finger.since) >= LONG_PRESS_MS


func _draw() -> void:
	_font = get_theme_default_font() if _font == null else _font
	if placing:
		_draw_placing()
		return
	var aimed := aiming()
	var on_self := holding_self()
	if not aimed.is_empty():
		_draw_aim(aimed)
	elif on_self:
		_ground_ring(body_at, 16, Palette.JADE_3, false, Palette.JADE_2)
		_ground_ring(body_at, 14, Palette.JADE_4, false, null)
	for button: String in BUTTONS:
		_square(BUTTONS[button], button)
	_pause_button()
	_draw_stick()
	for slot in skill_count:
		if aimed.get("slot", -1) != slot:
			_skill(slot, "" if not (on_self and _self_slot() == slot) else "jade")
	if not aimed.is_empty():
		_pad(aimed)
		_cancel(aimed.cancel)
	elif on_self:
		var s: Dictionary = SKILLS[_self_slot()]
		draw_arc(s.at, s.r + 3, 0, TAU, 48, Palette.JADE_4, 2.0)
		_word_slip("TOUCH_SELF", Vector2(s.at.x, s.at.y - s.r - 22), Palette.JADE_3)


# The arc while the arts panel is open: each button ringed in jade while a skill waits to be placed,
# with a slip asking for one, and the pause button's place closing the panel.
func _draw_placing() -> void:
	for slot in skill_count:
		_skill(slot, "jade" if waiting else "")
	if waiting:
		_word_slip("TOUCH_PLACE", PLACE_SLIP, Palette.JADE_3)
	_trim(PAUSE)
	for k in range(-6, 7):
		draw_rect(Rect2(PAUSE.position + Vector2(20 + k, 20 + k), Vector2(2, 2)), Palette.PAPER_3)
		draw_rect(Rect2(PAUSE.position + Vector2(20 + k, 20 - k), Vector2(2, 2)), Palette.PAPER_3)


func _self_slot() -> int:
	for finger: Dictionary in _fingers.values():
		if finger.kind == "skill" and not finger.aiming and _held_long(finger):
			return finger.slot
	return 0


# The range round the body, dashed, a dotted line out to where the drag aims and the skill's area
# there, in jade; in stone while the thumb is on the cancel zone.
func _draw_aim(aimed: Dictionary) -> void:
	var skill: Dictionary = reach[aimed.slot] if aimed.slot < reach.size() else {"range": 0.0, "radius": 0.0}
	var target: Vector2 = body_at + aimed.aim * float(skill.range)
	var area := maxf(float(skill.radius), 8.0)
	if aimed.cancel:
		_ground_ring(body_at, skill.range, Palette.STONE_3, true, null)
		_ground_ring(target, area, Palette.STONE_3, true, null)
		return
	_ground_ring(body_at, skill.range, Palette.PAPER_3, true, null)
	var length := body_at.distance_to(target)
	var t := 0.0
	while t < length:
		draw_rect(Rect2((body_at + (target - body_at) * t / length).floor(), Vector2(2, 2)), Palette.JADE_3)
		t += 5.0
	_ground_ring(target, area, Palette.JADE_3, false, Palette.JADE_2)
	_ground_ring(target, area - 2.0, Palette.JADE_4, false, null)


# A ring on the ground round `at`, `radius` pixels across: its rim in `colour`, every other run of
# four pixels when `dashed`, with a dither of `fill` inside when one is given.
func _ground_ring(at: Vector2, radius: float, colour: Color, dashed: bool, fill: Variant) -> void:
	if radius < 1.0:
		return
	if fill != null:
		draw_texture(_dither_texture(int(radius), fill), (at - Vector2.ONE * int(radius)).floor())
	if not dashed:
		draw_arc(at.floor(), radius, 0, TAU, maxi(int(radius) * 2, 24), colour, 1.0)
		return
	var dashes := maxi(int(TAU * radius / 8.0), 6)
	for n in dashes:
		var from := TAU * n / dashes
		draw_arc(at.floor(), radius, from, from + TAU / dashes / 2.0, 4, colour, 1.0)


func _draw_stick() -> void:
	var held := _knob != Vector2.ZERO or _fingers.values().any(func(f: Dictionary) -> bool: return f.kind == "stick")
	draw_circle(_stick_base, STICK_RADIUS, Palette.SHADE)
	draw_arc(_stick_base, STICK_RADIUS - 0.5, 0, TAU, 64, Palette.INK_1, 1.0)
	draw_arc(_stick_base, STICK_RADIUS - 1.5, 0, TAU, 64, Palette.GOLD_2, 1.0)
	# A chevron at each of the four ways.
	for way: Vector2 in [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]:
		var tip := _stick_base + way * (STICK_RADIUS - 5)
		var across := Vector2(way.y, way.x)
		for k in 3:
			var row := tip - way * k
			draw_rect(Rect2((row - across * k).floor(), Vector2.ONE + across.abs() * 2 * k), Palette.GOLD_3)
	_disc(_stick_base + _knob, KNOB_RADIUS, "jade" if held else "gold")
	draw_circle(_stick_base + _knob - Vector2(3, 3), 3, Palette.INK_3)


# Skill button `slot`, its rim in `rim`'s ramp or gold, its icon, and while the body cools down,
# its well dimmed, the share left darkened clockwise from the top and the seconds over it.
func _skill(slot: int, rim: String) -> void:
	var s: Dictionary = SKILLS[slot]
	_disc(s.at, s.r, "gold" if rim.is_empty() else rim)
	if slot < reach.size() and reach[slot].is_empty():
		var cross := Palette.JADE_2 if rim == "jade" else Palette.INK_4
		draw_rect(Rect2(s.at + Vector2(-4, 0), Vector2(9, 1)), cross)
		draw_rect(Rect2(s.at + Vector2(0, -4), Vector2(1, 9)), cross)
		return
	if slot < reach.size():
		_draw_icon("skill_icons", reach[slot].icon, Rect2(s.at - Vector2(8, 8), Vector2(16, 16)))
	if cooling <= 0.0 or placing:
		return
	draw_circle(s.at, s.r - 2, Palette.SHADE)
	var top := -PI / 2.0
	# The share left, clockwise from the top, as a fan of the well.
	var points := PackedVector2Array([s.at])
	var steps := maxi(int(32 * cooling), 1)
	for n in steps + 1:
		var angle := top + TAU * (1.0 - cooling) + TAU * cooling * n / steps
		points.append(s.at + Vector2.from_angle(angle) * (s.r - 2))
	if points.size() >= 3:
		draw_colored_polygon(points, Palette.DIM)
	var text := str(cooling_seconds)
	var at := Vector2(s.at.x - floorf(_width(text) / 2.0), s.at.y - 7)
	for offset: Vector2 in [Vector2(-1, -1), Vector2(0, -1), Vector2(1, -1), Vector2(-1, 0), Vector2(1, 0),
			Vector2(-1, 1), Vector2(0, 1), Vector2(1, 1)]:
		_text(at + offset, text, Palette.INK_1)
	_text(at, text, Palette.PAPER_3)


# The pad round a dragged skill button, its reach dashed, the button lit in jade, or in crimson
# while the thumb is on the cancel zone, and the knob where the thumb is.
func _pad(aimed: Dictionary) -> void:
	var s: Dictionary = SKILLS[aimed.slot]
	var ramp := "crimson" if aimed.cancel else "jade"
	draw_circle(s.at, PAD, Palette.SHADE)
	var dashes := int(TAU * PAD / 6.0)
	for n in dashes:
		var from := TAU * n / dashes
		draw_arc(s.at, PAD, from, from + TAU / dashes / 2.0, 3, Palette.CRIMSON_4 if aimed.cancel else Palette.JADE_4, 1.0)
	_skill(aimed.slot, ramp)
	if not aimed.cancel:
		_disc((s.at + aimed.aim * PAD).floor(), 6, ramp)


# The cancel zone, `hot` while the thumb is on it, with its word under it.
func _cancel(hot: bool) -> void:
	var at: Vector2 = CANCEL.at
	_disc(at, CANCEL.r, "crimson")
	if hot:
		draw_circle(at, CANCEL.r - 2, Palette.CRIMSON_2)
	var mark := Palette.PAPER_3 if hot else Palette.CRIMSON_4
	for k in range(-5, 6):
		draw_rect(Rect2(at + Vector2(k, k), Vector2(2, 2)), mark)
		draw_rect(Rect2(at + Vector2(k, -k - 1), Vector2(2, 2)), mark)
	_word_slip("TOUCH_CANCEL", Vector2(at.x, at.y + CANCEL.r + 3), Palette.CRIMSON_3 if hot else Palette.INK_3)


# A round button at `at` of radius `r`: an ink well in a rim of `ramp`, lit from the upper left.
func _disc(at: Vector2, r: int, ramp: String) -> void:
	draw_texture(_disc_texture(r, ramp), at.floor() - Vector2.ONE * (r + 2))


# The word `key` on an ink slip centred on `x` with its top at `y`, ruled along its top in `rule`.
func _word_slip(key: String, top: Vector2, rule: Color) -> void:
	var text := tr(key)
	var w := _width(text) + 8
	var left := top.x - floorf(w / 2.0)
	draw_rect(Rect2(left, top.y, w, 14), Palette.INK_1)
	draw_rect(Rect2(left, top.y, w, 1), rule)
	_text(Vector2(left + 4, top.y + 1), text, Palette.PAPER_3)


# A square button in the HUD's frame with its icon over its word.
func _square(rect: Rect2, button: String) -> void:
	_trim(rect)
	if icons.has(button):
		var colour := Palette.PAPER_3 if icons[button] == "arts" else Palette.GOLD_3
		_pixel_icon(Shortcuts.ICONS[icons[button]], rect.position + Vector2(floorf((rect.size.x - 9) / 2.0), 7), colour)
	var text := tr(BUTTON_KEYS[button])
	_text(Vector2(rect.position.x + floorf((rect.size.x - _width(text)) / 2.0), rect.position.y + 24), text, Palette.PAPER_3)


func _pause_button() -> void:
	_trim(PAUSE)
	draw_rect(Rect2(PAUSE.position + Vector2(15, 13), Vector2(4, 16)), Palette.PAPER_3)
	draw_rect(Rect2(PAUSE.position + Vector2(23, 13), Vector2(4, 16)), Palette.PAPER_3)


# A HUD panel's frame, as the HUD's panels are drawn: ink with a gold trim.
func _trim(rect: Rect2) -> void:
	draw_rect(rect, Palette.GOLD_2)
	draw_rect(Rect2(rect.position + Vector2(1, 2), rect.size - Vector2(2, 3)), Palette.INK_2)


# A round button of radius `r` as touch.lua's disc draws it, pixel by pixel: an ink outline, a rim
# of `ramp` lit on its upper-left half, an ink ring and an ink well.
func _disc_texture(r: int, ramp: String) -> ImageTexture:
	var key := "%d %s" % [r, ramp]
	if _discs.has(key):
		return _discs[key]
	var ramps := {"gold": [Palette.GOLD_2, Palette.GOLD_3], "jade": [Palette.JADE_3, Palette.JADE_4], "crimson": [Palette.CRIMSON_3, Palette.CRIMSON_4]}
	var side := 2 * r + 5
	var image := Image.create_empty(side, side, false, Image.FORMAT_RGBA8)
	var centre := r + 2
	for y in side:
		for x in side:
			var dx := float(x - centre)
			var dy := float(y - centre)
			var distance := sqrt(dx * dx + dy * dy)
			if distance > r + 1.5:
				continue
			var colour := Palette.INK_1
			if distance <= r + 0.5:
				var d := distance / (r + 0.5)
				if d > (r - 2.0) / r:
					colour = ramps[ramp][1] if dx + dy < 0 else ramps[ramp][0]
				elif d > (r - 4.0) / r:
					colour = Palette.INK_1
				else:
					colour = Palette.INK_2
			image.set_pixel(x, y, colour)
	_discs[key] = ImageTexture.create_from_image(image)
	return _discs[key]


# A disc `r` pixels across dithered with `fill` on an ordered 4x4 pattern, as the mockup fills an
# aimed area.
func _dither_texture(r: int, fill: Color) -> ImageTexture:
	var key := "%d %s" % [r, fill.to_html()]
	if _dither.has(key):
		return _dither[key]
	const BAYER := [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
	var side := 2 * r + 1
	var image := Image.create_empty(side, side, false, Image.FORMAT_RGBA8)
	for y in side:
		for x in side:
			var d := Vector2(x - r, y - r).length() / r
			if d < 1.0 and (BAYER[(y % 4) * 4 + x % 4] + 0.5) / 16.0 > 0.8:
				image.set_pixel(x, y, fill)
	_dither[key] = ImageTexture.create_from_image(image)
	return _dither[key]
