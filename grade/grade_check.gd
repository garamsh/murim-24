## Checks what grade.tscn draws, which needs a renderer, as its test leaves the drawn pixels to a
## tool. It is a tool, not a test: no test runs it. Run it by hand on a machine with a display, as
## `godot --path . --script res://grade/grade_check.gd`, and report what it prints in the pull
## request with the captures. Over world_view.tscn's M1 battle, held still, it draws the frame with
## no grade pass, with the pass hidden, graded at noon in summer, at dusk and at night, and at
## standard against low. It checks that the hidden pass leaves the frame unchanged; that dusk and
## night change most of it, night darker; and that standard's vignette darkens pixels toward the
## corners only, its middle the same as low's. It exits 0 when every check passes, 1 when one fails
## and 2 when it cannot draw, and quits within MAX_FRAMES frames.
extends SceneTree

const SCENE := "res://world_view/world_view.tscn"
const Grade := preload("res://grade/grade.gd")
const SUMMER := 1
const MAX_FRAMES := 600
# Frames the scene runs before it is held still, and frames each step waits for the frame to draw.
const SETTLE := 60
const WAIT := 4

var _view: Node
var _grade: Grade
var _frame := 0
var _step := 0
var _shots := {}
var _failed := 0


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		printerr("grade_check: rendering needs a display")
		quit(2)
		return
	root.size = Vector2i(1440, 810)
	_view = load(SCENE).instantiate()
	root.add_child(_view)
	_grade = _view.get_node("Grade")


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame > MAX_FRAMES:
		printerr("grade_check: out of frames")
		quit(1)
		return false
	if _frame < SETTLE:
		return false
	if _frame == SETTLE:
		# Held still from here: the world stops ticking and the grade is set by this check alone.
		_view.set_process(false)
	var steps := [["none", func() -> void: _grade.visible = false; _grade.get_node("Pass").visible = false],
		["hidden", func() -> void: _grade.get_node("Pass").visible = true; _grade.visible = false],
		["noon", func() -> void: _grade.quality = Grade.Quality.LOW; _grade.show_time(0.5, SUMMER)],
		["dusk", func() -> void: _grade.show_time(0.76, SUMMER)],
		["night", func() -> void: _grade.show_time(0.0, SUMMER)],
		["low", func() -> void: _grade.quality = Grade.Quality.LOW; _grade.show_time(0.5, SUMMER)],
		["standard", func() -> void: _grade.quality = Grade.Quality.STANDARD]]
	var at := _frame - SETTLE
	if at % WAIT == 0 and _step < steps.size():
		if _step > 0:
			_shots[steps[_step - 1][0]] = root.get_texture().get_image()
		steps[_step][1].call()
		_step += 1
	elif at % WAIT == 0 and _step == steps.size():
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
	var corners := _differ(_shots.low, _shots.standard, Rect2i(0, 0, 160, 90))
	var middle := _differ(_shots.low, _shots.standard, Rect2i(560, 320, 320, 170))
	_check("grade_check: standard's vignette changes %d pixels of the top-left corner's 160 x 90 and %d of the middle's 320 x 170" % [
		corners.count, middle.count], corners.count > 0 and middle.count == 0)
	print("grade_check: %d of 3 checks passed" % (3 - _failed))
	quit(0 if _failed == 0 else 1)


# How many pixels of `b` differ from `a` within `rect`, the whole frame by default, and by how much
# darker `b` is on average, in luma from 0 to 1.
func _differ(a: Image, b: Image, rect := Rect2i()) -> Dictionary:
	var area := rect if rect.has_area() else Rect2i(Vector2i.ZERO, a.get_size())
	var count := 0
	var darker := 0.0
	for y in range(area.position.y, area.end.y):
		for x in range(area.position.x, area.end.x):
			var ca := a.get_pixel(x, y)
			var cb := b.get_pixel(x, y)
			if not ca.is_equal_approx(cb):
				count += 1
			darker += ca.get_luminance() - cb.get_luminance()
	return {"count": count, "darker": darker / (area.size.x * area.size.y)}


func _check(line: String, passed: bool) -> void:
	print(line + (": pass" if passed else ": FAIL"))
	if not passed:
		_failed += 1
