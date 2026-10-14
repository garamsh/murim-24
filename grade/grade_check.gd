## Checks what grade.tscn draws, which needs a renderer, as its test leaves the drawn pixels to a
## tool. It is a tool, not a test: no test runs it. Run it by hand on a machine with a display, as
## `godot --path . --script res://grade/grade_check.gd`, and report what it prints in the pull
## request with the captures. Over world_view.tscn's M1 battle, held still, it draws the frame with
## no grade pass, with the pass hidden, graded at noon in summer, at dusk and at night, and at
## standard against low. It checks that the hidden pass leaves the frame unchanged; that dusk and
## night change most of it, night darker; and that standard's vignette darkens pixels toward the
## corners only, its middle the same as low's. Then, at night, over a fixture of a dark pane and a
## block of the fire ramp's yellow laid in the world, it draws the frame unlit, lit by a light round
## the pane and one beside it, and lit at standard against low, the lights standing for a hearth.
## It checks that the lights brighten the frame within their reach, redder there than green, and
## leave it unchanged beyond; that the pane takes the fire ramp only when lit, and the ink of an
## outline across its bottom row never does; and that standard's bloom changes pixels round the
## fire block, which keeps its colour, and none away from it. Last, over a relief of its own, flat
## but for a block of the highest level near the screen's top, it draws a morning at standard clear,
## with fog, and with the clouds' shadows. It checks that the fog changes the screen's top more than
## its lower part and none of its bottom, nor the pixels the high block shows, while the same pixels
## beside it change; and that the clouds' shadows darken the frame and leave a block standing in the
## fixture unchanged. Until then every step holds the fog and the clouds off, so the checks before
## see neither. Then, at noon, it draws the frame dry and raining at standard, raining again a moment
## later, and dry and raining at low. It checks that the rain dims the frame; that its streaks move,
## changing some pixels from one moment to the next but few; and that at low the rain changes
## nothing. It exits 0 when every check passes, 1 when one fails and 2 when it cannot draw, as
## when its window draws too few frames to reach the last shot within MAX_FRAMES frames. Its window
## stays on top, since a covered window is not drawn.
extends SceneTree

const SCENE := "res://world_view/world_view.tscn"
const Grade := preload("res://grade/grade.gd")
const SUMMER := 1
const MAX_FRAMES := 600
# Frames the scene runs before it is held still, and frames drawn after each step before its shot.
const SETTLE := 60
const WAIT := 4
# The fixture's dark pane, of wood's darkest step, and fire block, in art pixels from the middle of
# the screen, and the light beside them, its reach.
const PANE := Rect2i(40, -20, 6, 4)
const FIRE := Rect2i(-60, 30, 2, 2)
const BESIDE := Vector2(-150, -60)
const REACH := 30.0
const EMITTERS: Array[Color] = [Color("f5a33c"), Color("fbe37a"), Color("fff8dc")]
const MIDDLE := Rect2i(560, 320, 320, 170)
# The darkest ink, as a body's outline is drawn in.
const INK := Color("171229")
# A block standing in the fixture, in art pixels from the middle of the screen; and the pixels the
# fog fixture's high block shows, and those beside it, likewise.
const STANDING := Rect2i(-20, -40, 16, 12)
const HIGH := Rect2i(-30, -110, 24, 14)
const BESIDE_HIGH := Vector2i(60, 0)

var _view: Node
var _grade: Grade
var _frame := 0
var _step := 0
var _shots := {}
var _failed := 0
var _zoom := 1.0
# Frames the renderer has drawn, and how many must be drawn before the next shot.
var _drawn := 0
var _next := 0
var _middle := Vector2.ZERO


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		printerr("grade_check: rendering needs a display")
		quit(2)
		return
	root.size = Vector2i(1440, 810)
	# A window other windows cover is not drawn.
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	_view = load(SCENE).instantiate()
	root.add_child(_view)
	_grade = _view.get_node("Grade")
	RenderingServer.frame_post_draw.connect(func() -> void: _drawn += 1)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame > MAX_FRAMES:
		printerr("grade_check: out of frames, %d drawn, at step %d" % [_drawn, _step])
		quit(2)
		return false
	if _frame < SETTLE:
		return false
	if _frame == SETTLE:
		# Held still from here: the world stops ticking and the grade is set by this check alone.
		_view.set_process(false)
		_lay_fixture()
	var steps := [["none", func() -> void: _grade.visible = false; _grade.get_node("Pass").visible = false],
		["hidden", func() -> void: _grade.get_node("Pass").visible = true; _grade.visible = false],
		["noon", func() -> void: _grade.quality = Grade.Quality.LOW; _grade.show_time(0.5, SUMMER)],
		["dusk", func() -> void: _grade.show_time(0.76, SUMMER)],
		["night", func() -> void: _grade.show_time(0.0, SUMMER)],
		["low", func() -> void: _grade.quality = Grade.Quality.LOW; _grade.show_time(0.5, SUMMER)],
		["standard", func() -> void: _grade.quality = Grade.Quality.STANDARD],
		["unlit", func() -> void: _grade.quality = Grade.Quality.LOW; _grade.show_time(0.0, SUMMER); _show_lights(false)],
		["lit", func() -> void: _show_lights(true)],
		["bloom", func() -> void: _grade.quality = Grade.Quality.STANDARD],
		["clear", func() -> void: _show_lights(false); _grade.show_relief(_high_relief()); _grade.show_time(0.3, SUMMER)],
		["fog", func() -> void: _only(true, false)],
		["clouds", func() -> void: _only(false, true)],
		["dry", func() -> void: _grade.raining = false; _grade.show_time(0.5, SUMMER)],
		["wet", func() -> void: _grade.raining = true],
		["wet_later", func() -> void: _grade.show_time(0.5, SUMMER)],
		["dry_low", func() -> void: _grade.quality = Grade.Quality.LOW; _grade.raining = false],
		["wet_low", func() -> void: _grade.raining = true]]
	# A shot waits for frames drawn since its step, not frames processed, which run ahead of the
	# renderer on a loaded machine.
	if _drawn < _next:
		return false
	_next = _drawn + WAIT
	if _step < steps.size():
		if _step > 0:
			_shots[steps[_step - 1][0]] = root.get_texture().get_image()
		steps[_step][1].call()
		if not ["fog", "clouds"].has(steps[_step][0]):
			_only(false, false)
		_step += 1
	elif _step == steps.size():
		_shots[steps[_step - 1][0]] = root.get_texture().get_image()
		_report()
	return false


func _report() -> void:
	var hidden := _differ(_shots.none, _shots.hidden)
	_check("grade_check: the pass hidden changes %d pixels of the frame drawn without it" % hidden.count, hidden.count == 0)
	var dusk := _differ(_shots.none, _shots.dusk)
	var night := _differ(_shots.none, _shots.night)
	var size: int = _shots.none.get_width() * _shots.none.get_height()
	_check("grade_check: dusk changes %.2f of the frame, night %.2f, night %.3f darker than the frame against dusk's %.3f" % [
		float(dusk.count) / size, float(night.count) / size, night.darker, dusk.darker],
		dusk.count > size / 2 and night.count > size / 2 and night.darker > dusk.darker and night.darker > 0.0)
	# Standard blooms too, so the middle is compared away from the emitters.
	var corners := _differ(_shots.low, _shots.standard, Rect2i(0, 0, 160, 90))
	var middle := _differ(_shots.low, _shots.standard, MIDDLE, _blooming([_shots.low, _shots.standard], MIDDLE))
	_check("grade_check: standard's vignette changes %d pixels of the top-left corner's 160 x 90 and %d of the middle's 320 x 170" % [
		corners.count, middle.count], corners.count > 0 and middle.count == 0)
	var reach := Rect2i(_screen(BESIDE) - Vector2i.ONE * int(REACH * 0.5 * _zoom), Vector2i.ONE * int(REACH * _zoom))
	var lit := _differ(_shots.unlit, _shots.lit, reach)
	var beyond := _differ(_shots.unlit, _shots.lit, Rect2i(_screen(BESIDE + Vector2(-REACH - 20, 0)) - Vector2i(10, 10), Vector2i(20, 20)))
	var warmth := _warmth(_shots.lit, reach)
	_check("grade_check: the light changes %d pixels within half its reach, %.3f brighter, red over green there %.3f, and %d beyond it" % [
		lit.count, -lit.darker, warmth, beyond.count], lit.count > 0 and lit.darker < 0.0 and warmth > 0.0 and beyond.count == 0)
	var pane := Rect2i(_screen(Vector2(PANE.position)), (PANE.size - Vector2i(0, 1)) * int(_zoom))
	var ink := Rect2i(_screen(Vector2(PANE.position + Vector2i(0, PANE.size.y - 1))), Vector2i(PANE.size.x, 1) * int(_zoom))
	var lit_panes := _emitting(_shots.lit, pane)
	var unlit_panes := _emitting(_shots.unlit, pane)
	var lit_ink := _emitting(_shots.lit, ink)
	_check("grade_check: %d of the pane's %d pixels take the fire ramp lit, %d unlit, and %d of the ink's %d lit" % [
		lit_panes, pane.get_area(), unlit_panes, lit_ink, ink.get_area()], lit_panes > 0 and unlit_panes == 0 and lit_ink == 0)
	var fire := Rect2i(_screen(Vector2(FIRE.position)), FIRE.size * int(_zoom))
	var round_fire := _differ(_shots.lit, _shots.bloom, fire.grow(int(2 * _zoom)))
	var kept := _emitting(_shots.bloom, fire)
	var away := _differ(_shots.lit, _shots.bloom, MIDDLE, _blooming([_shots.lit, _shots.bloom], MIDDLE))
	_check("grade_check: standard's bloom changes %d pixels within two art pixels of the fire block, which keeps %d of its %d, and %d of the middle beyond two art pixels of every emitter" % [
		round_fire.count, kept, fire.get_area(), away.count], round_fire.count > 0 and kept == fire.get_area() and away.count == 0)
	var height: int = _shots.clear.get_height()
	var width: int = _shots.clear.get_width()
	var top := _differ(_shots.clear, _shots.fog, Rect2i(0, 0, width, height / 4))
	var lower := _differ(_shots.clear, _shots.fog, Rect2i(0, height * 5 / 8, width, height / 4))
	var bottom := _differ(_shots.clear, _shots.fog, Rect2i(0, height * 93 / 100, width, height * 7 / 100))
	_check("grade_check: the fog changes %d pixels of the screen's top quarter, %d of a quarter lower down, and %d of its bottom 7%%" % [
		top.count, lower.count, bottom.count], top.count > lower.count and lower.count >= 0 and bottom.count == 0 and top.count > 0)
	var high := Rect2i(_screen(Vector2(HIGH.position)), HIGH.size * int(_zoom))
	var on_high := _differ(_shots.clear, _shots.fog, high)
	var beside := _differ(_shots.clear, _shots.fog, Rect2i(high.position + BESIDE_HIGH * int(_zoom), high.size))
	_check("grade_check: the fog changes %d pixels of the high ground's %d and %d of as many beside it" % [
		on_high.count, high.get_area(), beside.count], on_high.count == 0 and beside.count > 0)
	var shaded := _differ(_shots.clear, _shots.clouds)
	var standing := _differ(_shots.clear, _shots.clouds, Rect2i(_screen(Vector2(STANDING.position)), STANDING.size * int(_zoom)))
	_check("grade_check: the clouds' shadows change %d pixels of the frame, %.4f darker, and %d of the block standing" % [
		shaded.count, shaded.darker, standing.count], shaded.count > 0 and shaded.darker > 0.0 and standing.count == 0)
	var wet := _differ(_shots.dry, _shots.wet)
	_check("grade_check: the rain changes %d pixels of the frame, %.3f darker" % [wet.count, wet.darker],
			wet.count > 0 and wet.darker > 0.0)
	var moved := _differ(_shots.wet, _shots.wet_later)
	var frame_size: int = _shots.wet.get_width() * _shots.wet.get_height()
	_check("grade_check: a moment later the rain's streaks change %d pixels, %.3f of the frame" % [
		moved.count, float(moved.count) / frame_size], moved.count > 0 and moved.count < frame_size / 5)
	var at_low := _differ(_shots.dry_low, _shots.wet_low)
	_check("grade_check: at low the rain changes %d pixels" % at_low.count, at_low.count == 0)
	print("grade_check: %d of 12 checks passed" % (12 - _failed))
	quit(0 if _failed == 0 else 1)


# How many pixels of `b` differ from `a` within `rect`, the whole frame by default, those `skip`
# holds left out, and by how much darker `b` is on average, in luma from 0 to 1.
func _differ(a: Image, b: Image, rect := Rect2i(), skip := {}) -> Dictionary:
	var area := rect if rect.has_area() else Rect2i(Vector2i.ZERO, a.get_size())
	var count := 0
	var darker := 0.0
	for y in range(area.position.y, area.end.y):
		for x in range(area.position.x, area.end.x):
			var ca := a.get_pixel(x, y)
			var cb := b.get_pixel(x, y)
			if not ca.is_equal_approx(cb) and not skip.has(Vector2i(x, y)):
				count += 1
			darker += ca.get_luminance() - cb.get_luminance()
	return {"count": count, "darker": darker / (area.size.x * area.size.y)}


# Lays the fixture in the world under the grade, on whole art pixels from the middle of the screen,
# the camera on a whole world pixel.
func _lay_fixture() -> void:
	var camera: Camera2D = _view.get_node("Camera")
	camera.position = camera.position.round()
	_zoom = camera.zoom.x
	_middle = camera.position
	var image := Image.create_empty(160, 80, false, Image.FORMAT_RGBA8)
	var origin := Vector2i(80, 40)
	image.fill_rect(Rect2i(origin + PANE.position, PANE.size), Color("22132c"))
	image.fill_rect(Rect2i(origin + PANE.position + Vector2i(0, PANE.size.y - 1), Vector2i(PANE.size.x, 1)), INK)
	image.fill_rect(Rect2i(origin + FIRE.position, FIRE.size), EMITTERS[1])
	image.fill_rect(Rect2i(origin + STANDING.position, STANDING.size), Color("8a6f4e"))
	var sprite := Sprite2D.new()
	sprite.texture = ImageTexture.create_from_image(image)
	sprite.centered = false
	sprite.position = _middle - Vector2(origin)
	sprite.z_index = 100
	_view.add_child(sprite)
	# The names are drawn over the grade, where they could hide an emitter the pass blooms round.
	for child in _view.get_children():
		if child is CanvasLayer and child != _grade:
			child.visible = false


# Gives the grade the light round the pane and the one beside it, or none.
func _show_lights(on: bool) -> void:
	var view := Rect2(_middle - Vector2(root.size) / _zoom / 2.0, Vector2(root.size) / _zoom)
	var lights := PackedFloat32Array()
	if on:
		var pane := _middle + Vector2(PANE.position)
		lights.append_array([pane.x, pane.y, PANE.size.x, PANE.size.y, REACH])
		lights.append_array([_middle.x + BESIDE.x, _middle.y + BESIDE.y, 0, 0, REACH])
	# The light round the pane stands for a hearth too, so the bloom shows.
	_grade.show_lights(lights, view, 1 if on else 0)


# Shows the grade again, then holds the fog off unless `fog`, in the pass and in the light field,
# and the clouds' shadows unless `clouds`.
func _only(fog: bool, clouds: bool) -> void:
	_grade.quality = _grade.quality
	if not fog:
		(_grade.get_node("Pass").material as ShaderMaterial).set_shader_parameter("fog", 0.0)
		(_grade.get_node("LightField/Field").material as ShaderMaterial).set_shader_parameter("fog", 0.0)
	if not clouds:
		_grade.clouds.set_shader_parameter("clouds", 0.0)


# A relief the size of the world's, flat but for the highest level under the cells the fog reads
# for the pixels HIGH covers, and a cell round them: those of a pixel's world point lowered by the
# highest level's lift.
func _high_relief() -> Relief:
	var relief := Relief.new(_view.get_node("WorldArt").relief.size)
	var world := Rect2(_middle + Vector2(HIGH.position) + Vector2(0, Relief.TOP * Relief.LEVEL_PX), Vector2(HIGH.size))
	var first := Vector2i((world.position / Relief.CELL).floor()) - Vector2i.ONE
	var last := Vector2i((world.end / Relief.CELL).floor()) + Vector2i.ONE
	for y in range(first.y, last.y + 1):
		for x in range(first.x, last.x + 1):
			relief.lay(Vector2i(x, y), "rock", Relief.TOP, "rock_face", false)
	return relief


# Where `art`, art pixels from the middle of the screen, lies on it, in screen pixels.
func _screen(art: Vector2) -> Vector2i:
	return Vector2i((Vector2(root.size) / 2.0 + art * _zoom).round())


# The pixels within `rect` that bloom can reach in any of `images`: within two art pixels of an
# emitter's, and one more for the art pixels' grid.
func _blooming(images: Array[Image], rect: Rect2i) -> Dictionary:
	var reach := int(3 * _zoom)
	var near := {}
	var outer := rect.grow(reach)
	for image in images:
		for y in range(maxi(outer.position.y, 0), mini(outer.end.y, image.get_height())):
			for x in range(maxi(outer.position.x, 0), mini(outer.end.x, image.get_width())):
				var c := image.get_pixel(x, y)
				if EMITTERS.any(func(e: Color) -> bool: return c.is_equal_approx(e)):
					for dy in range(-reach, reach + 1):
						for dx in range(-reach, reach + 1):
							near[Vector2i(x + dx, y + dy)] = true
	return near


# How much redder than green `image` is within `rect` on average, so lamplight reads warm.
func _warmth(image: Image, rect: Rect2i) -> float:
	var sum := 0.0
	for y in range(rect.position.y, rect.end.y):
		for x in range(rect.position.x, rect.end.x):
			var c := image.get_pixel(x, y)
			sum += c.r - c.g
	return sum / rect.get_area()


# How many pixels of `image` within `rect` are an emitter's colour.
func _emitting(image: Image, rect: Rect2i) -> int:
	var count := 0
	for y in range(rect.position.y, rect.end.y):
		for x in range(rect.position.x, rect.end.x):
			var c := image.get_pixel(x, y)
			if EMITTERS.any(func(e: Color) -> bool: return c.is_equal_approx(e)):
				count += 1
	return count


func _check(line: String, passed: bool) -> void:
	print(line + (": pass" if passed else ": FAIL"))
	if not passed:
		_failed += 1
