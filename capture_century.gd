# Draws the whole region of seed 2026, with the player born in village 0 and days of five ticks, as
# the core's century test lives it, through world_art.tscn into a 2048 x 2048 viewport, at year 0
# (century_year0.png) and after 100 years (century_year100.png). Prints the living, the villages
# and the peopled villages each time. Fails if the two images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SIDE := 2048
const SHORT_DAY := 5
const YEARS := 100
var _dir := "."
var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _saved := {}


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_viewport = SubViewport.new()
	_viewport.size = Vector2i(SIDE, SIDE)
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(_viewport)
	_world = WorldAdapter.create_region(2026, 0, SHORT_DAY)
	_art = load("res://world_art/world_art.tscn").instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, _world.army_count() - 1)
	_art.refresh(Rect2(Vector2.ZERO, Vector2(SIDE, SIDE)))


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 5:
		_shot("century_year0")
		for tick in YEARS * 120 * SHORT_DAY:
			_world.tick()
		_art.note_tick()
		_art.refresh(Rect2(Vector2.ZERO, Vector2(SIDE, SIDE)))
	if _frame == 10:
		_shot("century_year100")
		return true
	return false


func _shot(name: String) -> void:
	var living := 0
	for army in _world.army_count():
		living += _world.body_positions(army).size()
	var villages := _world.villages()
	var peopled := 0
	for village in villages.size():
		peopled += 1 if _world.village_population(village) > 0 else 0
	var image := _viewport.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_century: %s.png repeats %s.png" % [name, _saved[digest]])
		quit(1)
	_saved[digest] = name
	image.save_png("%s/%s.png" % [_dir, name])
	print("capture_century: %s.png, date %s, sha256 %s, living %d, villages %d, peopled %d" % [
		name, _world.date(), digest.left(16), living, villages.size(), peopled])
