# Records play.tscn on the region of seed 2026 (the player born in village 0) around the region's
# first founded village, each moment held on the new village (village_*) and on the nearest stretch
# of border its territory adds (border_*): the tick before it is founded (*_before.png); the tick it
# is founded, its layout laid as rubble with its template's banner and road, and its territory
# claimed (*_founded.png); and once its settlers have raised every structure
# (village_raised.png). A second world of the same seed is run ahead first to find the founding
# tick, the place and the territory after. The script drives the ticks itself, the last of each
# stretch through play.step as play.gd does, holds the camera on what it captures and centres the
# HUD's map on it. Prints the tick, the date, the last village, the cells whose territory changed
# at that moment, the borders drawn and whether they were drawn from the core's territory.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SEED := 2026
const LIMIT := 2_000_000
const BATCH := 50_000
const SETTLE_FRAMES := 10
const MAP_SIZE := Vector2(76, 44)
var _play: Node
var _dir := "."
var _stage := 0
var _wait := 0
var _ticks := 0
var _scout: WorldAdapter
var _scout_ticks := 0
var _founding_tick := -1
var _founded := {}
var _village := Vector2.ZERO
var _border := Vector2.ZERO
var _last_territory := PackedInt64Array()
var _changed := 0
var _layout := PackedInt64Array()
# The hash of each image saved, so a frame that repeats an earlier one fails the run.
var _saved := {}


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	var args := OS.get_cmdline_user_args()
	_dir = args[0] if args.size() > 0 else "."
	# The script ticks the world itself, so the tick count is exact.
	_play.process_mode = Node.PROCESS_MODE_DISABLED
	root.add_child(_play)
	_scout = WorldAdapter.create_region(SEED, 0)


func _process(_delta: float) -> bool:
	var world: WorldAdapter = _play.world
	if _last_territory.is_empty():
		# The territory as the world begins, once the scene is ready.
		_last_territory = world.territory()
	match _stage:
		0:
			# The scout finds the tick on which the first village is founded, and where.
			var generated := _scout.villages().size()
			for i in BATCH:
				_scout.tick()
				_scout_ticks += 1
				if _scout.villages().size() > generated:
					_founding_tick = _scout_ticks
					_founded = _scout.villages()[-1]
					_village = Vector2(_founded.corner) + Vector2(_founded.area) / 2.0
					print("capture_territory: the scout found %s founded on tick %d" % [_founded, _founding_tick])
					_stage = 1
					return false
			return _scout_ticks >= LIMIT
		1:
			# Up to two ticks before the founding, then one through play.step.
			var to := mini(_ticks + BATCH, _founding_tick - 2)
			while _ticks < to:
				world.tick()
				_ticks += 1
			if _ticks < _founding_tick - 2:
				return false
			_play.step()
			_ticks += 1
			_border = _nearest_new_border(world.territory(), _scout.territory())
			_moment()
			_stage = 2
		2:
			if _shot("village_before", _village):
				_stage = 3
		3:
			if _shot("border_before", _border):
				_play.step()
				_ticks += 1
				for structure in world.structure_count():
					if Rect2i(_founded.corner, _founded.area).has_point(world.structure_cell(structure)):
						_layout.append(structure)
				_moment()
				_stage = 4
		4:
			if _shot("village_founded", _village):
				_stage = 5
		5:
			if _shot("border_founded", _border):
				_stage = 6
		6:
			# Until the settlers have raised the layout, the last tick through play.step.
			for i in BATCH:
				if _all_intact(world):
					break
				world.tick()
				_ticks += 1
			if not _all_intact(world):
				return _ticks >= LIMIT
			_play.step()
			_ticks += 1
			_moment()
			_stage = 7
		7:
			return _shot("village_raised", _village)
	return false


# Counts the cells whose territory changed since the last moment.
func _moment() -> void:
	var territory: PackedInt64Array = _play.world.territory()
	_changed = 0
	for i in territory.size():
		if territory[i] != _last_territory[i]:
			_changed += 1
	_last_territory = territory


# The centre of the cell nearest the new village that is a border cell in `after` and was not in
# `before`: a cell whose right or lower neighbour lies in another territory.
func _nearest_new_border(before: PackedInt64Array, after: PackedInt64Array) -> Vector2:
	var width: int = _play.world.map_rows()[0].length()
	var nearest := Vector2(INF, INF)
	for i in after.size():
		var cell := Vector2(i % width, i / width) + Vector2(0.5, 0.5)
		if _is_border(after, i, width) and not _is_border(before, i, width) \
				and cell.distance_to(_village) < nearest.distance_to(_village):
			nearest = cell
	return nearest


func _is_border(owners: PackedInt64Array, i: int, width: int) -> bool:
	return ((i + 1) % width != 0 and owners[i] != owners[i + 1]) \
			or (i + width < owners.size() and owners[i] != owners[i + width])


func _all_intact(world: WorldAdapter) -> bool:
	for structure in _layout:
		if world.structure_stage(structure) != WorldAdapter.STAGE_INTACT:
			return false
	return true


# Holds the camera on `focus` and redraws the world and the HUD, as play.gd does; after
# SETTLE_FRAMES frames saves the frame as `name` and returns true.
func _shot(name: String, focus: Vector2) -> bool:
	var world: WorldAdapter = _play.world
	var camera: Camera2D = _play.get_node("Camera")
	camera.position = (focus * _play.PIXELS_PER_CELL).floor()
	var screen: Vector2 = _play.get_viewport_rect().size / camera.zoom
	_play.get_node("WorldArt").refresh(Rect2(camera.get_screen_center_position() - screen / 2.0, screen))
	var hud: CanvasLayer = _play.get_node("Hud")
	if not world.body_positions(_play.steered).is_empty():
		hud.refresh(world, _play.steered, PackedInt64Array())
	# The map shows the land around what is captured rather than around the player.
	(hud._map.texture as AtlasTexture).region = Rect2((focus - MAP_SIZE / 2.0).floor(), MAP_SIZE)
	hud._dot.visible = false
	_wait += 1
	if _wait < SETTLE_FRAMES:
		return false
	_wait = 0
	var stages := []
	for structure in _layout:
		stages.append("%s %s %d" % [world.structure_kind(structure), world.structure_cell(structure),
				world.structure_stage(structure)])
	var image := root.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_territory: %s.png repeats %s.png" % [name, _saved[digest]])
		quit(1)
	_saved[digest] = name
	image.save_png("%s/%s.png" % [_dir, name])
	var owners: PackedInt64Array = _play.get_node("WorldArt").owners
	print("capture_territory: %s.png at tick %d, date %s, camera on cell %s, frame sha256 %s; %d villages, the last %s; %d cells changed territory at this moment; %d borders drawn, from a territory equal to the core's %s; layout (kind, cell, stage) %s" % [
		name, _ticks, world.date(), focus.floor(), digest.left(16), world.villages().size(),
		world.villages()[-1].name, _changed, _play.get_node("WorldArt/Territory").borders.size(),
		owners == world.territory(), stages])
	return true
