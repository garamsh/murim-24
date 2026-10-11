# workshop_capture.gd -- <out dir>: grows region 2026 on five-tick days until it has built a smithy
# and an apothecary standing intact, then draws world_art.tscn round each at 3x, 1920 x 1080.
extends SceneTree

const DAY := 5
const MOST_DAYS := 9000

var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _out := ""
var _shots := []


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	_world = WorldAdapter.create_region(2026, 0, DAY)
	var found := {}
	var ticks := 0
	while found.size() < 2 and ticks < MOST_DAYS * DAY:
		for step in 50:
			_world.tick()
		ticks += 50
		for s in _world.structure_count():
			var kind := _world.structure_kind(s)
			if kind in ["smithy", "apothecary"] and not found.has(kind) and _world.structure_stage(s) == 0:
				found[kind] = s
	print("workshop_capture: after %d days found %s" % [ticks / DAY, found])
	_viewport = SubViewport.new()
	_viewport.size = Vector2i(1920, 1080)
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_viewport.canvas_item_default_texture_filter = Viewport.DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST
	root.add_child(_viewport)
	_art = load("res://world_art/world_art.tscn").instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, _world.army_count() - 1)
	for kind: String in found:
		var s: int = found[kind]
		var centre := (Vector2(_world.structure_cell(s)) + Vector2(_world.structure_footprint(s)) / 2.0) * Vector2(_art.CELL)
		_shots.append([kind + "_3x", centre])


func _process(_delta: float) -> bool:
	_frame += 1
	if _shots.is_empty():
		quit(1)
		return false
	var shot: Array = _shots[0]
	var size := Vector2(1920, 1080) / 3.0
	var view := Rect2(shot[1] - size / 2.0, size)
	_art.scale = Vector2(3, 3)
	_art.position = -view.position.round() * 3.0
	_art.refresh(view, 1.0)
	if _frame % 120 == 0:
		_viewport.get_texture().get_image().save_png(_out + "/" + shot[0] + ".png")
		print("workshop_capture: saved ", shot[0], " at ", view)
		_shots.pop_front()
		if _shots.is_empty():
			quit()
	return false
