# Records play.tscn on the region's first village (village 0 of seed 2026, the player born there),
# its camera held on the village's centre while the script drives the ticks: at session start in
# spring (economy_spring.png), on the first day of autumn (economy_autumn.png), and once every field
# of the village is burnt after the residents of village RAIDERS, of the Demonic Cult, are ordered
# to its centre with all their skills (economy_burnt.png). Prints the date and each field's state.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const VILLAGE := 0
const RAIDERS := 4
const TICKS_PER_DAY := 1800
const AUTUMN_DAY := 60
const RAID_LIMIT := 20000
const SETTLE_FRAMES := 10
var _play: Node
var _dir := "."
var _frame := 0
var _stage := 0
var _wait := 0


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	_play.birth_village = VILLAGE
	var args := OS.get_cmdline_user_args()
	_dir = args[0] if args.size() > 0 else "."
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 2:
		_play.set_process(false)
	if _frame < 2:
		return false
	_show()
	_wait += 1
	if _wait < SETTLE_FRAMES:
		return false
	_wait = 0
	var world: WorldAdapter = _play.world
	match _stage:
		0:
			_shot("economy_spring")
			# To the first day of autumn.
			for i in AUTUMN_DAY * TICKS_PER_DAY - _play.ticks + 1:
				_play.step()
		1:
			_shot("economy_autumn")
			var village: Dictionary = world.villages()[VILLAGE]
			var centre: Vector2i = village.corner + village.area / 2
			print("capture_economy: ordering army %d of faction %d to %s: %s" % [RAIDERS, world.army_faction(RAIDERS), centre,
				world.order(RAIDERS, centre, PackedInt64Array([0, 1]))])
			var ticks := 0
			while ticks < RAID_LIMIT and not _all_burnt(world):
				_play.step()
				ticks += 1
			print("capture_economy: every field burnt after %d ticks: %s" % [ticks, _all_burnt(world)])
		2:
			_shot("economy_burnt")
			return true
	_stage += 1
	return false


# Holds the camera on the village's centre and redraws the world and the HUD, as play.gd does.
func _show() -> void:
	var world: WorldAdapter = _play.world
	var village: Dictionary = world.villages()[VILLAGE]
	var centre := Vector2(village.corner) + Vector2(village.area) / 2.0
	var camera: Camera2D = _play.get_node("Camera")
	camera.position = (centre * _play.PIXELS_PER_CELL).floor()
	var screen: Vector2 = _play.get_viewport_rect().size / camera.zoom
	_play.get_node("WorldArt").refresh(Rect2(camera.get_screen_center_position() - screen / 2.0, screen))
	if not world.body_positions(_play.steered).is_empty():
		_play.get_node("Hud").refresh(world, _play.steered, PackedInt64Array())


func _fields(world: WorldAdapter) -> Array[int]:
	var village: Dictionary = world.villages()[VILLAGE]
	var area := Rect2i(village.corner, village.area)
	var found: Array[int] = []
	for structure in world.structure_count():
		if world.structure_field_state(structure) >= 0 and area.has_point(world.structure_cell(structure)):
			found.append(structure)
	return found


func _all_burnt(world: WorldAdapter) -> bool:
	for structure in _fields(world):
		if world.structure_field_state(structure) != WorldAdapter.FIELD_BURNT:
			return false
	return true


func _shot(name: String) -> void:
	var world: WorldAdapter = _play.world
	var states := []
	for structure in _fields(world):
		states.append(world.structure_field_state(structure))
	root.get_texture().get_image().save_png("%s/%s.png" % [_dir, name])
	print("capture_economy: %s.png at tick %d, date %s, village fields %s, population %d of cap %d, stock %d" % [
		name, _play.ticks, world.date(), states, world.village_population(VILLAGE), world.village_cap(VILLAGE),
		world.village_stock(VILLAGE)])
