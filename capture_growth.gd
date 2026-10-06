# Records play.tscn on the region of seed 2026 (the player born in village 0) as its villages grow
# at play pace, 1,800 ticks a day, the script driving the ticks itself: the first village founded,
# its layout laid as rubble (growth_founded_laid.png) and once its settlers have raised every
# structure (growth_founded_raised.png); then the first building a village lays outside a
# settlement, as rubble (growth_built_laid.png) and once raised (growth_built_raised.png). The
# camera is held on what is captured. Prints the tick, the date, and each structure's kind and stage
# (STAGE_* constants: 0 intact, 3 rubble).
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLEMENT := 6
const SETTLEMENT_MIDDLE := Vector2(3.5, 4.5)
const LIMIT := 4_000_000
const BATCH := 50_000
const SETTLE_FRAMES := 10
var _play: Node
var _dir := "."
var _frame := 0
var _stage := 0
var _wait := 0
var _ticks := 0
var _founded := PackedInt64Array()
var _built := -1
var _focus := Vector2.ZERO
# The hash of each image saved, so a frame that repeats an earlier one fails the run.
var _saved := {}


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	var args := OS.get_cmdline_user_args()
	_dir = args[0] if args.size() > 0 else "."
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 2:
		_play.set_process(false)
	if _frame < 2:
		return false
	var world: WorldAdapter = _play.world
	match _stage:
		0:
			# Ticks in batches until a settlement's layout is laid at once.
			var before := world.structure_count()
			if not _run(func() -> bool: return world.structure_count() - before >= SETTLEMENT):
				return _ticks >= LIMIT
			for structure in range(before, world.structure_count()):
				_founded.append(structure)
			_focus = Vector2(world.structure_cell(_founded[0])) + SETTLEMENT_MIDDLE
			print("capture_growth: settlement laid at tick %d, date %s, settlers %d in army %d" % [
				_ticks, world.date(), world.body_positions(world.army_count() - 1).size(), world.army_count() - 1])
		1:
			pass
		2:
			if not _run(func() -> bool: return _all_intact(world, _founded)):
				return _ticks >= LIMIT
		4:
			# The first single structure a village lays that is drawn as a sprite.
			var before := world.structure_count()
			if not _run(func() -> bool: return _new_building(world, before) >= 0):
				return _ticks >= LIMIT
			_built = _new_building(world, before)
			_focus = Vector2(world.structure_cell(_built)) + Vector2(world.structure_footprint(_built)) / 2.0
		6:
			if not _run(func() -> bool: return _all_intact(world, PackedInt64Array([_built]))):
				return _ticks >= LIMIT
	if _stage in [0, 2, 4, 6]:
		# A few ticks through play.step, so the view reads the armies the core formed.
		for i in 3:
			_play.step()
			_ticks += 1
		_stage += 1
		return false
	_show()
	_wait += 1
	if _wait < SETTLE_FRAMES:
		return false
	_wait = 0
	match _stage:
		1:
			_shot("growth_founded_laid", _founded)
		3:
			_shot("growth_founded_raised", _founded)
		5:
			_shot("growth_built_laid", PackedInt64Array([_built]))
		7:
			_shot("growth_built_raised", PackedInt64Array([_built]))
			return true
	_stage += 1
	return false


# Ticks the world in a batch, stopping early once `done` holds. Returns whether it holds.
func _run(done: Callable) -> bool:
	var world: WorldAdapter = _play.world
	for i in BATCH:
		if done.call():
			return true
		world.tick()
		_ticks += 1
	return done.call()


# The first structure from `from` on that is drawn as a building, laid alone; -1 when there is none.
func _new_building(world: WorldAdapter, from: int) -> int:
	var laid := world.structure_count() - from
	if laid != 1:
		return -1
	var kind := world.structure_kind(from)
	return from if kind in ["house", "granary"] else -1


func _all_intact(world: WorldAdapter, structures: PackedInt64Array) -> bool:
	for structure in structures:
		if world.structure_stage(structure) != WorldAdapter.STAGE_INTACT:
			return false
	return true


# Holds the camera on the focus and redraws the world and the HUD, as play.gd does.
func _show() -> void:
	var world: WorldAdapter = _play.world
	var camera: Camera2D = _play.get_node("Camera")
	camera.position = (_focus * _play.PIXELS_PER_CELL).floor()
	var screen: Vector2 = _play.get_viewport_rect().size / camera.zoom
	# The body bands are made anew where the camera lands: bands filled first at the player's
	# village and refilled after the camera jumps across the region drew no body.
	if _wait == 0:
		var art = _play.get_node("WorldArt")
		for band in art._bands:
			band.queue_free()
		art._bands.clear()
	_play.get_node("WorldArt").refresh(Rect2(camera.get_screen_center_position() - screen / 2.0, screen))
	if not world.body_positions(_play.steered).is_empty():
		_play.get_node("Hud").refresh(world, _play.steered, PackedInt64Array())


func _shot(name: String, structures: PackedInt64Array) -> void:
	var world: WorldAdapter = _play.world
	var shown := []
	for structure in structures:
		shown.append("%s %s %d" % [world.structure_kind(structure), world.structure_cell(structure),
			world.structure_stage(structure)])
	var image := root.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_growth: %s.png repeats %s.png" % [name, _saved[digest]])
		quit(1)
	_saved[digest] = name
	image.save_png("%s/%s.png" % [_dir, name])
	print("capture_growth: %s.png at tick %d, date %s, structures (kind, cell, stage) %s" % [
		name, _ticks, world.date(), shown])
