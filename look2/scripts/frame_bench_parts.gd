## Measures frame time with world_view.tscn rendering and the simulation ticking together, vsync off
## so a frame takes as long as its work, and prints p50, p99 and max against the frame budget, with
## how many uses of skills and damage numbers the timed frames drew and how many bodies they outlined.
## `make bench` runs it; it needs a display, so it runs on a developer's machine and not in CI.
extends SceneTree
# frame_bench_parts.gd -- frame_bench.gd with parts of #249 switched off by the arguments after --:
# "noblood" turns blood off, "nohitstop" the hit stop and shake, and "noblooddraw" hides blood's two
# MultiMeshes with its work still done, set on the second frame, before the warm-up ends, so their
# cost can be read against the same bench with them on.

const SCENE := "res://world_view/world_view.tscn"
const WARM_UP_FRAMES := 120
const TIMED_FRAMES := 1200
const BUDGET_MS := 16.7
const EXPECTED_PEOPLE := 500

var _view: Node
var _frame := 0
var _last_usec := 0
var _ticks_before := 0
var _times := PackedFloat64Array()
var _uses_drawn := 0
var _numbers_drawn := 0
var _outlined := 0


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		_fail("rendering needs a display; run make bench on a machine with one")
		return
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	_view = load(SCENE).instantiate()
	root.add_child(_view)


func _process(_delta: float) -> bool:
	var now := Time.get_ticks_usec()
	_frame += 1
	if _frame == 2:
		var combat: Node2D = _view.get_node("WorldArt/Combat")
		var off := OS.get_cmdline_user_args()
		combat.blood = not off.has("noblood")
		combat.hit_stop = not off.has("nohitstop")
		_view.get_node("WorldArt/Drops").visible = not off.has("noblooddraw")
		_view.get_node("WorldArt/Sprays").visible = not off.has("noblooddraw")
		print("parts: blood %s, hit stop %s" % [combat.blood, combat.hit_stop])
	if _frame == WARM_UP_FRAMES:
		_check_setup()
		_ticks_before = _view.ticks
	elif _frame > WARM_UP_FRAMES:
		_times.append((now - _last_usec) / 1000.0)
		var combat: Node2D = _view.get_node("WorldArt/Combat")
		_uses_drawn += combat.shown_strikes(Time.get_ticks_msec()).size()
		_numbers_drawn += combat.shown_numbers(Time.get_ticks_msec()).size()
		_outlined += _view.bodies_outlined
		# The next frame is timed from here, so counting what this one drew is not timed.
		now = Time.get_ticks_usec()
	_last_usec = now
	if _times.size() == TIMED_FRAMES:
		_report()
	return false


# Fails before timing when the scene is not running the scenario it claims to.
func _check_setup() -> void:
	var world: WorldAdapter = _view.world
	var people := 0
	for army in world.army_count():
		people += world.body_positions(army).size()
	if people != EXPECTED_PEOPLE:
		_fail("the world must hold %d people, not %d" % [EXPECTED_PEOPLE, people])
	elif _view.bodies_drawn == 0:
		_fail("the camera must have bodies on screen to draw")
	elif _view.ticks == 0:
		_fail("the world must tick during the warm-up")
	elif _view.names_shown == 0:
		_fail("the view must name the people near its viewer")
	elif _view.get_node("WorldArt/Combat").shown_strikes(Time.get_ticks_msec()).is_empty():
		_fail("the battle must draw the uses of its skills")


func _report() -> void:
	var world: WorldAdapter = _view.world
	var broken := 0
	for structure in world.structure_count():
		if world.structure_stage(structure) != WorldAdapter.STAGE_INTACT:
			broken += 1
	var sorted := _times.duplicate()
	sorted.sort()
	var p50 := sorted[sorted.size() / 2]
	var p99 := sorted[ceili(sorted.size() * 0.99) - 1]
	var seconds := 0.0
	for time in _times:
		seconds += time / 1000.0
	var passed := p99 <= BUDGET_MS
	print("frame scene: %s, M1 scenario (core/m1_scenario.cpp), window %s, vsync off, %s, Godot %s" % [
		SCENE, DisplayServer.window_get_size(), RenderingServer.get_video_adapter_name(),
		Engine.get_version_info().string])
	print("frames: %d warm-up, then %d timed; %d ticks in %.1f s timed (%d per second set)" % [
		WARM_UP_FRAMES, TIMED_FRAMES, _view.ticks - _ticks_before, seconds,
		WorldAdapter.TICKS_PER_SECOND])
	print("bodies: %d in the world, %d on screen drawn as sprites through MultiMesh bands" % [
		_view.bodies, _view.bodies_drawn])
	print("outlines: %.1f bodies outlined over what hides them a frame on average, %d in the last frame" % [
		float(_outlined) / TIMED_FRAMES, _view.bodies_outlined])
	print("names: %d shown over the %d people within reach in the last frame" % [_view.names_shown, _view.named_near])
	print("structures broken: %d of %d" % [broken, world.structure_count()])
	print("combat feedback: %.1f uses of skills and %.1f damage numbers drawn a frame on average" % [
		float(_uses_drawn) / TIMED_FRAMES, float(_numbers_drawn) / TIMED_FRAMES])
	print("frame time: p50 %.3f ms, p99 %.3f ms, max %.3f ms" % [p50, p99, sorted[-1]])
	print("%s: p99 %.3f ms against a %.1f ms budget" % ["PASS" if passed else "FAIL", p99, BUDGET_MS])
	print("frame time is measured on this machine only: CI has no display and does not run make bench")
	quit(0 if passed else 1)


func _fail(message: String) -> void:
	printerr("frame bench: setup check failed: ", message)
	quit(2)
