## Measures frame time as world_art.tscn draws a region of REGION_SEED while the view pans across
## it, so the ground streams its areas in and out as a player crossing the map makes it, the world
## ticking as it is drawn, vsync off. It prints p50, p99 and max against the frame budget, and how
## many areas the ground showed. It needs a display, so it runs by hand on a developer's machine,
## as `godot --path . --script res://world_view/pan_bench.gd`, beside make bench's frame bench.
extends SceneTree

const SCENE := "res://world_art/world_art.tscn"
const REGION_SEED := 2026
const WARM_UP_FRAMES := 120
const TIMED_FRAMES := 1200
const BUDGET_MS := 16.7
## The screen in the world's pixels, and how many of them the view moves a frame: a cell in four.
const SCREEN := Vector2(640, 360)
const SPEED := Vector2(4, 3)
## Frames between ticks, near the core's rate at 60 frames a second.
const FRAMES_PER_TICK := 3

var _world: WorldAdapter
var _art: Node2D
var _frame := 0
var _last_usec := 0
var _times := PackedFloat64Array()
var _at := Vector2.ZERO
var _way := Vector2.ONE
var _map := Vector2.ZERO
var _shown_most := 0


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		_fail("rendering needs a display; run it on a machine with one")
		return
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	root.size = Vector2i(1920, 1080)
	_world = WorldAdapter.create_region(REGION_SEED, 0)
	if _world == null:
		_fail("the core must make the region of seed %d" % REGION_SEED)
		return
	_art = load(SCENE).instantiate()
	root.add_child(_art)
	_art.show_world(_world, _world.player_army())
	var rows := _world.map_rows()
	_map = Vector2(rows[0].length(), rows.size()) * Vector2(_art.CELL)
	_art.scale = Vector2(3, 3)


func _process(_delta: float) -> bool:
	var now := Time.get_ticks_usec()
	_frame += 1
	if _world == null:
		return false
	# The view bounces between the map's edges, crossing it down and across.
	_at += SPEED * _way
	for axis in 2:
		if _at[axis] < 0.0 or _at[axis] > _map[axis] - SCREEN[axis]:
			_way[axis] = -_way[axis]
			_at[axis] = clampf(_at[axis], 0.0, _map[axis] - SCREEN[axis])
	if _frame % FRAMES_PER_TICK == 0:
		_world.tick()
		_art.note_tick()
	_art.position = -_at.round() * 3.0
	_art.refresh(Rect2(_at.round(), SCREEN))
	_shown_most = maxi(_shown_most, _art.get_node("Ground").get_child_count())
	if _frame > WARM_UP_FRAMES:
		_times.append((now - _last_usec) / 1000.0)
		now = Time.get_ticks_usec()
	_last_usec = now
	if _times.size() == TIMED_FRAMES:
		_report()
	return false


func _report() -> void:
	var sorted := _times.duplicate()
	sorted.sort()
	var p50 := sorted[sorted.size() / 2]
	var p99 := sorted[ceili(sorted.size() * 0.99) - 1]
	var passed := p99 <= BUDGET_MS
	print("pan scene: %s, region of seed %d (%d x %d pixels), window %s, vsync off, %s, Godot %s" % [
		SCENE, REGION_SEED, _map.x, _map.y, DisplayServer.window_get_size(),
		RenderingServer.get_video_adapter_name(), Engine.get_version_info().string])
	print("pan: %s pixels a frame at 3x, bouncing between the map's edges; at most %d ground nodes shown" % [
		SPEED, _shown_most])
	print("frame time: p50 %.3f ms, p99 %.3f ms, max %.3f ms" % [p50, p99, sorted[-1]])
	print("%s: p99 %.3f ms against a %.1f ms budget" % ["PASS" if passed else "FAIL", p99, BUDGET_MS])
	quit(0 if passed else 1)


func _fail(message: String) -> void:
	printerr("pan bench: setup check failed: ", message)
	quit(2)
