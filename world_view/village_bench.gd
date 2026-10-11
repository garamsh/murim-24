## Measures frame time in a generated region's village by day: the region of seed SEED through
## world_art.tscn, the camera on the steered person in its birth village at the PC's scale, the world
## ticking at the core's tick rate, vsync off so a frame takes as long as its work. Prints p50, p99
## and max over its timed frames. It reads the art only through show_world, note_tick, refresh and
## body_at, so the same script measures an older tree for comparison. Run by hand, on a machine with
## a display: godot --path . --script res://world_view/village_bench.gd
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const SEED := 2026
const WARM_UP_FRAMES := 120
const TIMED_FRAMES := 1200
const MAX_TICKS_PER_FRAME := 4

var _world: WorldAdapter
var _art: Node2D
var _camera: Camera2D
var _steered := -1
var _frame := 0
var _last_usec := 0
var _since_tick := 0.0
var _ticks := 0
var _times := PackedFloat64Array()


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		printerr("village bench: rendering needs a display")
		quit(2)
		return
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	_world = WorldAdapter.create_region(SEED)
	_steered = _world.army_count() - 1
	_art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(_art)
	_art.show_world(_world, _steered)
	_camera = Camera2D.new()
	var scale := ViewScale.world_scale(DisplayServer.window_get_size(), false)
	_camera.zoom = Vector2(scale, scale)
	root.add_child(_camera)


func _process(delta: float) -> bool:
	var step := 1.0 / WorldAdapter.TICKS_PER_SECOND
	_since_tick = minf(_since_tick + delta, step * MAX_TICKS_PER_FRAME)
	while _since_tick >= step:
		_world.tick()
		_art.note_tick()
		_ticks += 1
		_since_tick -= step
	var between := _since_tick / step
	if _world.body_positions(_steered).size() > 0:
		_camera.position = _art.body_at(_steered, 0, between).round()
	var screen := Vector2(DisplayServer.window_get_size()) / _camera.zoom
	_art.refresh(Rect2(_camera.position - screen / 2.0, screen), between)
	var now := Time.get_ticks_usec()
	_frame += 1
	if _frame == WARM_UP_FRAMES and (_art.drawn == 0 or _ticks == 0):
		printerr("village bench: setup check failed: %d bodies drawn, %d ticks" % [_art.drawn, _ticks])
		quit(2)
	elif _frame > WARM_UP_FRAMES:
		_times.append((now - _last_usec) / 1000.0)
	_last_usec = now
	if _times.size() == TIMED_FRAMES:
		var sorted := _times.duplicate()
		sorted.sort()
		print("village bench: region of seed %d, steered army %d, %d bodies drawn in the last frame, %d ticks, window %s, %s" % [
			SEED, _steered, _art.drawn, _ticks, DisplayServer.window_get_size(), RenderingServer.get_video_adapter_name()])
		print("village frame time: p50 %.3f ms, p99 %.3f ms, max %.3f ms" % [
			sorted[sorted.size() / 2], sorted[ceili(sorted.size() * 0.99) - 1], sorted[-1]])
		quit(0)
	return false
