# Draws the seat of Hwasan, the first sect, in the region of seed 2026 through world_art.tscn into a
# 480 x 270 viewport centred on its seat, with the player born in village 0 and days of five ticks.
# Ticks until at least PUPILS bodies stand on its training ground's cells, then saves the frame as
# seat.png and a 4x nearest-neighbour copy as seat_4x.png. Prints the seat, the tick, the bodies on
# the training ground and the image's hash.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const VIEW := Vector2i(480, 270)
const SHORT_DAY := 5
const PUPILS := 3
const LIMIT := 200000
var _dir := "."
var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _yard := Rect2i()
var _centre := Vector2.ZERO
var _ticks := 0


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_viewport = SubViewport.new()
	_viewport.size = VIEW
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(_viewport)
	_world = WorldAdapter.create_region(2026, 0, SHORT_DAY)
	_art = load("res://world_art/world_art.tscn").instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, _world.army_count() - 1)
	var seat: Dictionary = _world.villages()[_world.sect_seat(0)]
	var area := Rect2i(seat.corner, seat.area)
	for structure in _world.structure_count():
		if _world.structure_kind(structure) == "training_ground" and area.has_point(_world.structure_cell(structure)):
			_yard = Rect2i(_world.structure_cell(structure), _world.structure_footprint(structure))
	_centre = (Vector2(seat.corner) + Vector2(seat.area) / 2) * 16
	var camera := Camera2D.new()
	camera.position = _centre
	_viewport.add_child(camera)
	while _on_yard() < PUPILS and _ticks < LIMIT:
		_world.tick()
		_ticks += 1
		if _ticks % SHORT_DAY == 0:
			_art.note_tick()
	_art.note_tick()
	_art.refresh(Rect2(_centre - Vector2(VIEW) / 2, Vector2(VIEW)))


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame < 5:
		return false
	var image := _viewport.get_texture().get_image()
	image.save_png("%s/seat.png" % _dir)
	var big := image.duplicate()
	big.resize(VIEW.x * 4, VIEW.y * 4, Image.INTERPOLATE_NEAREST)
	big.save_png("%s/seat_4x.png" % _dir)
	print("capture_seat: %s, yard %s, after %d ticks (date %s), %d bodies on the training ground, sha256 %s" % [
		_world.sect_name(0), _yard, _ticks, _world.date(), _on_yard(),
		image.get_data().hex_encode().sha256_text().left(16)])
	return true


func _on_yard() -> int:
	var count := 0
	for army in _world.army_count():
		for at in _world.body_positions(army):
			if _yard.has_point(Vector2i(at / WorldAdapter.CELL_SIZE)):
				count += 1
	return count
