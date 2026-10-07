# Records play.tscn on the region of seed 2026, the player born in village 0, re-created with days of
# five ticks so a village grows for decades in minutes: the first smithy growth lays, as rubble
# (workshops_smithy_laid.png), then the first village in which a smithy and an apothecary both stand
# intact (workshops_both_standing.png). The camera is held on what is captured, with the HUD hidden. Prints the tick,
# the date, and each workshop's kind, cell and stage (STAGE_* constants: 0 intact, 3 rubble).
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SEED := 2026
const DAY_TICKS := 5
# Workshops within this many cells of each other, along each axis, count as one village's.
const NEAR := 16
const LIMIT := 200_000
const BATCH := 2_000
const SETTLE_FRAMES := 10
var _play: Node
var _dir := "."
var _frame := 0
var _stage := 0
var _wait := 0
var _ticks := 0
var _shown := PackedInt64Array()
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
		_play.world = WorldAdapter.create_region(SEED, 0, DAY_TICKS)
		_play.steered = _play.world.army_count() - 1
		_play.get_node("WorldArt").show_world(_play.world, _play.steered)
		_play.get_node("Hud").show_player(_play.world, _play.steered)
	if _frame < 3:
		return false
	var world: WorldAdapter = _play.world
	match _stage:
		0:
			if not _run(func() -> bool: return _first(world, "smithy") >= 0):
				return _ticks >= LIMIT
			_shown = PackedInt64Array([_first(world, "smithy")])
		2:
			if not _run(func() -> bool: return not _pair(world).is_empty()):
				return _ticks >= LIMIT
			_shown = _pair(world)
	if _stage in [0, 2]:
		_focus = _middle(world, _shown)
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
			_shot("workshops_smithy_laid")
		3:
			_shot("workshops_both_standing")
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


# The first structure of `kind`; -1 when there is none.
func _first(world: WorldAdapter, kind: String) -> int:
	for structure in world.structure_count():
		if world.structure_kind(structure) == kind:
			return structure
	return -1


# The first intact smithy and an intact apothecary near it; empty when there is none.
func _pair(world: WorldAdapter) -> PackedInt64Array:
	for smithy in world.structure_count():
		if world.structure_kind(smithy) != "smithy" or world.structure_stage(smithy) != WorldAdapter.STAGE_INTACT:
			continue
		for apothecary in world.structure_count():
			if world.structure_kind(apothecary) != "apothecary" \
					or world.structure_stage(apothecary) != WorldAdapter.STAGE_INTACT:
				continue
			var apart: Vector2i = (world.structure_cell(smithy) - world.structure_cell(apothecary)).abs()
			if apart.x <= NEAR and apart.y <= NEAR:
				return PackedInt64Array([smithy, apothecary])
	return PackedInt64Array()


# The middle of `structures`' footprints, in cells.
func _middle(world: WorldAdapter, structures: PackedInt64Array) -> Vector2:
	var sum := Vector2.ZERO
	for structure in structures:
		sum += Vector2(world.structure_cell(structure)) + Vector2(world.structure_footprint(structure)) / 2.0
	return sum / structures.size()


# Holds the camera on the focus and redraws the world, as play.gd does.
func _show() -> void:
	var camera: Camera2D = _play.get_node("Camera")
	camera.position = (_focus * _play.PIXELS_PER_CELL).floor()
	var screen: Vector2 = _play.get_viewport_rect().size / camera.zoom
	# The body bands are made anew where the camera lands.
	if _wait == 0:
		var art = _play.get_node("WorldArt")
		for band in art._bands:
			band.queue_free()
		art._bands.clear()
	_play.get_node("WorldArt").refresh(Rect2(camera.get_screen_center_position() - screen / 2.0, screen))
	# The HUD is hidden: its panels would cover the workshops at the frame's top and bottom.
	_play.get_node("Hud").visible = false


func _shot(name: String) -> void:
	var world: WorldAdapter = _play.world
	var shown := []
	for structure in _shown:
		shown.append("%s %s %d" % [world.structure_kind(structure), world.structure_cell(structure),
			world.structure_stage(structure)])
	var image := root.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_workshops: %s.png repeats %s.png" % [name, _saved[digest]])
		quit(1)
	_saved[digest] = name
	image.save_png("%s/%s.png" % [_dir, name])
	print("capture_workshops: %s.png at tick %d, date %s, sha256 %s, workshops (kind, cell, stage) %s" % [
		name, _ticks, world.date(), digest.substr(0, 16), shown])
