# Captures villages of seeds 0 to 49 in the real play.tscn at 1920x1080, in Korean, for #169: each
# shot plays the region of its seed lived its years at five ticks a day, the player born into its
# village, the camera on the player there. It prints, per shot, the village's residents, its
# structures and how many of its fields stand on hill cells, and its nearest other village.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://capture_villages.gd -- <dir> <seed:years:village> ...
extends SceneTree

const SETTLE_FRAMES := 20
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const WINDOW := Vector2i(1920, 1080)
const KEPT := "user://capture_villages.cfg"

var _dir := "."
var _shots: Array = []
var _shot := 0
var _frame := 0
var _play: Node2D


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	_dir = args[0]
	for spec: String in args.slice(1):
		var parts := spec.split(":")
		_shots.append({"seed": int(parts[0]), "years": int(parts[1]), "village": int(parts[2])})
	DisplayServer.window_set_size(WINDOW)


func _describe(world: WorldAdapter, village: int) -> String:
	var villages := world.villages()
	var here: Dictionary = villages[village]
	var centre := Vector2(here.corner) + Vector2(here.area) / 2.0
	var nearest := INF
	for other: Dictionary in villages:
		var at := Vector2(other.corner) + Vector2(other.area) / 2.0
		if at != centre:
			nearest = minf(nearest, at.distance_to(centre))
	var terrain := world.terrain()
	var names := world.terrain_names()
	var rows := world.map_rows()
	var width: int = rows[0].length()
	var fields := 0
	var terraced := 0
	var near := 0
	for structure in world.structure_count():
		var cell := world.structure_cell(structure)
		if cell.distance_to(Vector2i(centre)) > 20:
			continue
		near += 1
		if world.structure_kind(structure) == "field":
			fields += 1
			var size := world.structure_footprint(structure)
			var on_hill := false
			for y in size.y:
				for x in size.x:
					on_hill = on_hill or names[terrain[(cell.y + y) * width + cell.x + x]] == "hills"
			terraced += 1 if on_hill else 0
	return "%s %d (%s) square %s at %s: %d residents, %d structures within 20 cells, %d fields, %d of them on hills; nearest other village %.1f cells" % [
		here.name, village, here.kind, here.area, here.corner, world.village_population(village), near, fields,
		terraced, nearest]


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	if _play != null:
		var shot: Dictionary = _shots[_shot]
		var path := "%s/seed%d_year%d_village%d_ko.png" % [_dir, shot.seed, shot.years, shot.village]
		root.get_viewport().get_texture().get_image().save_png(path)
		print("capture: wrote %s; %s" % [path, _describe(_play.world, shot.village)])
		_play.queue_free()
		_play = null
		_shot += 1
		return false
	if _shot >= _shots.size():
		quit(0)
		return true
	var shot: Dictionary = _shots[_shot]
	var made := WorldAdapter.create_region(shot.seed, shot.village, TICKS_PER_DAY)
	for i in shot.years * DAYS_PER_YEAR * TICKS_PER_DAY:
		made.tick()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	var hud: CanvasLayer = _play.get_node("Hud")
	hud.settings.path = KEPT
	hud.settings.choose("language", 0)
	hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
	_play._play(made)
	_play.set_process(true)
	return false
