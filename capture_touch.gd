# Records the touch controls in play.tscn in each locale, one image for each state the mobile HUD
# mockups draw, driving the scene as a finger does through play.gd's touch(), with 4x
# nearest-neighbour copies:
# - mobile_hud_idle_<locale>.png: at rest, the body cooling down after its last strike.
# - mobile_hud_aim_<locale>.png: the first skill dragged right of its button to aim.
# - mobile_hud_cancel_<locale>.png: the same drag carried onto the cancel zone.
# - mobile_hud_self_<locale>.png: the second skill held past a long press.
# - mobile_hud_targets_<locale>.png: at rest, with every place a touch lands outlined and sized.
# The world is SEED's region with the player born into village 0, and the nearest hostile army
# marched on the player until one stands within the first skill's range, so raiders stand about it. The touch row and the language
# are chosen on the settings panel, kept in KEPT, which is removed at the end. Prints what each
# shows and fails if two images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
const SEED := 2026
const MARCH := 4000
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_touch.cfg"
var _dir := "."
var _frame := 0
var _step := 0
var _locale := 0
var _play: Node2D
var _saved := {}
var _failed := false
var _now := [0]
var _overlay: Control
var _fresh := false


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_open()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_fresh = true


# The scene once ready: the settings chosen, the world started and the raiders marched on.
func _set_up() -> void:
	_fresh = false
	var hud: CanvasLayer = _play.get_node("Hud")
	hud.settings.path = KEPT
	hud.settings.choose("language", _locale)
	hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on"))
	hud.touch.clock = func() -> int: return _now[0]
	_play.start(SEED)
	var world: WorldAdapter = _play.world
	var here := Vector2i((_play.lead() / WorldAdapter.CELL_SIZE).floor())
	world.order(_nearest_hostile(world, _play.steered), here, PackedInt64Array([0]))
	var person: int = world.roster(_play.steered)[0]
	var marched := 0
	while world.aim(person, 0, Vector2.ZERO) < 0 and marched < MARCH:
		_play.step()
		marched += 1
	print("capture_touch: marched %d ticks, a hostile person in range %s" % [marched, world.aim(person, 0, Vector2.ZERO) >= 0])


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	if _fresh:
		_set_up()
		return false
	var locale: String = LOCALES[_locale]
	var hud: CanvasLayer = _play.get_node("Hud")
	var touch: Control = hud.touch
	var person: int = _play.world.roster(_play.steered)[0]
	match _step:
		0:
			# A cast on the body itself, struck, so the buttons show the wait it left.
			hud.labels.notice.visible = false
		1:
			_shot("mobile_hud_idle_" + locale, "touch shown %s, skills %d, cooling %.2f with %d s left, ready at %d on tick %d" % [
					touch.visible, touch.skill_count, touch.cooling, touch.cooling_seconds,
					_play.world.body_ready(_play.steered)[0], _play.world.tick_count()])
			_play.touch(_finger(1, touch.SKILLS[0].at, true))
			_play.touch(_slide(1, touch.SKILLS[0].at + Vector2(touch.SLOP + 2, 0)))
			_play.touch(_slide(1, touch.SKILLS[0].at + Vector2(-26, -4) * touch.PAD / 36.0))
		2:
			_shot("mobile_hud_aim_" + locale, "aiming %s, range %d px" % [touch.aiming(), touch.reach[0].range])
			_play.touch(_slide(1, touch.CANCEL.at))
		3:
			_shot("mobile_hud_cancel_" + locale, "aiming %s" % touch.aiming())
			_play.touch(_finger(1, touch.CANCEL.at, false))
			_play.touch(_finger(2, touch.SKILLS[1].at, true))
			_now[0] += touch.LONG_PRESS_MS
		4:
			_shot("mobile_hud_self_" + locale, "holding self %s" % touch.holding_self())
			_play.touch(_finger(2, touch.SKILLS[1].at, false))
			_overlay = Targets.new()
			_overlay.touch = touch
			hud.add_child(_overlay)
		5:
			_shot("mobile_hud_targets_" + locale, "%d skill circles, the cancel zone, the stick's zone %s, %d buttons and pause" % [
					touch.skill_count, touch.STICK_ZONE, touch.BUTTONS.size()])
			_locale += 1
			if _locale == LOCALES.size():
				_play.queue_free()
				DirAccess.remove_absolute(ProjectSettings.globalize_path(KEPT))
				print("capture_touch: %d images, none repeated %s" % [_saved.size(), not _failed])
				quit(1 if _failed else 0)
				return true
			_open()
			_step = -1
	_step += 1
	return false


# The army nearest army `army` whose faction its own is hostile to.
func _nearest_hostile(world: WorldAdapter, army: int) -> int:
	var here: Vector2 = world.body_positions(army)[0]
	var nearest := -1
	var best := INF
	for other in world.army_count():
		if other == army or world.roster(other).is_empty() \
				or not world.hostile(world.army_faction(army), world.army_faction(other)):
			continue
		var distance := world.body_positions(other)[0].distance_to(here)
		if distance < best:
			best = distance
			nearest = other
	return nearest


func _finger(index: int, at: Vector2, pressed: bool) -> InputEventScreenTouch:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	return event


func _slide(index: int, at: Vector2) -> InputEventScreenDrag:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = at
	return event


func _shot(name: String, note: String) -> void:
	var image := root.get_texture().get_image()
	var sha := image.get_data().hex_encode().sha256_text().left(16)
	if _saved.has(sha):
		_failed = true
	_saved[sha] = name
	image.save_png("%s/%s.png" % [_dir, name])
	var big := image.duplicate()
	big.resize(image.get_width() * 4, image.get_height() * 4, Image.INTERPOLATE_NEAREST)
	big.save_png("%s/%s_4x.png" % [_dir, name])
	print("capture_touch: %s.png %dx%d, sha256 %s, locale %s; %s" % [name, image.get_width(),
			image.get_height(), sha, TranslationServer.get_locale(), note])


# Every place a touch lands, read from touch.gd's constants, outlined in amber and sized in
# pixels of the native 480x270, as mobile_hud_targets.png marks them.
class Targets extends Control:
	var touch: Control

	func _ready() -> void:
		size = Vector2(480, 270)
		mouse_filter = Control.MOUSE_FILTER_IGNORE

	func _draw() -> void:
		var amber := Color("e8a830")
		var font := ThemeDB.fallback_font
		var galmuri: Font = load("res://hud/fonts/Galmuri9.ttf")
		for slot in touch.skill_count:
			var s: Dictionary = touch.SKILLS[slot]
			_dashed_circle(s.at, s.hit, amber)
			_size(galmuri, s.at + Vector2(-8, -s.hit - 12), str(s.hit * 2), amber)
		_dashed_circle(touch.CANCEL.at, touch.CANCEL.hit, amber)
		_size(galmuri, touch.CANCEL.at + Vector2(-8, -touch.CANCEL.hit - 12), str(touch.CANCEL.hit * 2), amber)
		for rect: Rect2 in touch.BUTTONS.values() + [touch.PAUSE, touch.STICK_ZONE]:
			draw_rect(rect, amber, false, 1.0)
			_size(galmuri, rect.position + Vector2(2, rect.size.y - 12),
					"%dx%d" % [rect.size.x, rect.size.y] if rect == touch.STICK_ZONE else str(int(rect.size.x)), amber)

	func _dashed_circle(at: Vector2, radius: float, colour: Color) -> void:
		var dashes := int(TAU * radius / 6.0)
		for n in dashes:
			var from := TAU * n / dashes
			draw_arc(at, radius, from, from + TAU / dashes / 2.0, 3, colour, 1.0)

	func _size(font: Font, at: Vector2, text: String, colour: Color) -> void:
		var w := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, 10).x
		draw_rect(Rect2(at, Vector2(w + 4, 12)), Color("14101a"))
		draw_string(font, at + Vector2(2, font.get_ascent(10)), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, colour)
