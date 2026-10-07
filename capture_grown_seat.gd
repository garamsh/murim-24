# Draws the seat of Hwasan, the first sect, in the region of seed 2026 through world_art.tscn into a
# 480 x 270 viewport centred on its seat, with the player born in village 0 and days of five ticks,
# after YEARS years. Saves the frame as grown_seat.png and a 4x nearest-neighbour copy as
# grown_seat_4x.png. Prints the seat, the date, the structures growth laid within the view and how
# many of them cover a cell of the seat's court or path, and the image's hash.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const VIEW := Vector2i(480, 270)
const SHORT_DAY := 5
const YEARS := 100
const DAYS_PER_YEAR := 120
# The seat's court and its path to the gate, as offsets from its corner, as the seat template
# keeps them clear.
const COURT := [Rect2i(0, 3, 12, 2), Rect2i(8, 5, 1, 3)]
var _dir := "."
var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _centre := Vector2.ZERO
var _seat := {}
var _laid_from := 0


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_viewport = SubViewport.new()
	_viewport.size = VIEW
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(_viewport)
	_world = WorldAdapter.create_region(2026, 0, SHORT_DAY)
	_laid_from = _world.structure_count()
	_art = load("res://world_art/world_art.tscn").instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, _world.army_count() - 1)
	_seat = _world.villages()[_world.sect_seat(0)]
	_centre = (Vector2(_seat.corner) + Vector2(_seat.area) / 2) * 16
	var camera := Camera2D.new()
	camera.position = _centre
	_viewport.add_child(camera)
	for tick in YEARS * DAYS_PER_YEAR * SHORT_DAY:
		_world.tick()
		if tick % SHORT_DAY == 0:
			_art.note_tick()
	_art.note_tick()
	_art.refresh(Rect2(_centre - Vector2(VIEW) / 2, Vector2(VIEW)))


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame < 5:
		return false
	var image := _viewport.get_texture().get_image()
	image.save_png("%s/grown_seat.png" % _dir)
	var big := image.duplicate()
	big.resize(VIEW.x * 4, VIEW.y * 4, Image.INTERPOLATE_NEAREST)
	big.save_png("%s/grown_seat_4x.png" % _dir)
	var view := Rect2i(Vector2i(_centre / 16) - Vector2i(15, 9), Vector2i(30, 18))
	var laid := []
	var on_court := 0
	for structure in range(_laid_from, _world.structure_count()):
		var at := Rect2i(_world.structure_cell(structure), _world.structure_footprint(structure))
		if not view.intersects(at):
			continue
		laid.append("%s at %s" % [_world.structure_kind(structure), at.position - Vector2i(_seat.corner)])
		for court: Rect2i in COURT:
			if Rect2i(court.position + Vector2i(_seat.corner), court.size).intersects(at):
				on_court += 1
	print("capture_grown_seat: %s, corner %s, date %s, growth laid in view (offsets from the seat's corner) %s, %d on its court or path, sha256 %s" % [
		_world.sect_name(0), _seat.corner, _world.date(), laid, on_court,
		image.get_data().hex_encode().sha256_text().left(16)])
	return true
