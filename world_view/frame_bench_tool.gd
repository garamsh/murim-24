## Measures frame time with world_view.tscn rendering and the simulation ticking together, vsync off
## so a frame takes as long as its work, and prints p50, p99 and max against the frame budget, with
## how many uses of skills and damage numbers the timed frames drew and how many bodies they outlined,
## and where a timed frame's time went: ticking the core, reading it through the adapter, the view's
## scripts with the native code they call, and rendering as the renderer measures its CPU and GPU
## time. `make bench` runs it; it needs a display, so it runs on a developer's machine and not in CI.
extends SceneTree

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
# Each part's time over the timed frames, in microseconds, as world_view.gd keeps it, at the start
# of the timed frames; and the renderer's CPU and GPU time over them, in milliseconds.
var _usec_before := PackedInt64Array([0, 0, 0])
var _render_cpu := 0.0
var _render_gpu := 0.0


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		_fail("rendering needs a display; run make bench on a machine with one")
		return
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	RenderingServer.viewport_set_measure_render_time(root.get_viewport_rid(), true)
	_view = load(SCENE).instantiate()
	root.add_child(_view)


func _process(_delta: float) -> bool:
	var now := Time.get_ticks_usec()
	_frame += 1
	if _frame == WARM_UP_FRAMES:
		_check_setup()
		_ticks_before = _view.ticks
		_usec_before = PackedInt64Array([_view.tick_usec, _view.adapter_usec, _view.view_usec])
	elif _frame > WARM_UP_FRAMES:
		_times.append((now - _last_usec) / 1000.0)
		var combat: Node2D = _view.get_node("WorldArt/Combat")
		_uses_drawn += combat.shown_strikes(Time.get_ticks_msec()).size()
		_numbers_drawn += combat.shown_numbers(Time.get_ticks_msec()).size()
		_outlined += _view.bodies_outlined
		_render_cpu += RenderingServer.viewport_get_measured_render_time_cpu(root.get_viewport_rid()) \
				+ RenderingServer.get_frame_setup_time_cpu()
		_render_gpu += RenderingServer.viewport_get_measured_render_time_gpu(root.get_viewport_rid())
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
	var per_frame := func(usec: int) -> float: return usec / 1000.0 / TIMED_FRAMES
	var core: float = per_frame.call(_view.tick_usec - _usec_before[0])
	var adapter: float = per_frame.call(_view.adapter_usec - _usec_before[1])
	var view_script: float = per_frame.call(_view.view_usec - _usec_before[2])
	print("frame breakdown, mean a timed frame: core tick %.3f ms, adapter %.3f ms, view script %.3f ms, render CPU %.3f ms (GPU %.3f ms), the rest %.3f ms of %.3f ms" % [
		core, adapter, view_script, _render_cpu / TIMED_FRAMES, _render_gpu / TIMED_FRAMES,
		seconds * 1000.0 / TIMED_FRAMES - core - adapter - view_script - _render_cpu / TIMED_FRAMES,
		seconds * 1000.0 / TIMED_FRAMES])
	print("%s: p99 %.3f ms against a %.1f ms budget" % ["PASS" if passed else "FAIL", p99, BUDGET_MS])
	print("frame time is measured on this machine only: CI has no display and does not run make bench")
	quit(0 if passed else 1)


func _fail(message: String) -> void:
	printerr("frame bench: setup check failed: ", message)
	quit(2)
