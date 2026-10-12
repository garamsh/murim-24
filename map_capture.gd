# Captures of the player's map as built for #168 (ADR 0073), the real play.tscn and HUD, in each
# locale, at each window in RUNS:
# - minimap_<run>_<locale>.png: the HUD's map around the player, from the core's chart.
# - window_<run>_<locale>.png: the map window N opens, on PC; on a touch screen no button opens it.
# The world is SEED's region, lived YEARS years at five ticks a day; the player, steered as a new
# game steers it, walks to the JOURNEY villages nearest its home, then the world lives AFTER_YEARS
# more while it stands. The script prints the villages the map remembers that now differ from the
# world, owner, residents or ruins, and the villages founded out of sight.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://map_capture.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 30
const SEED := 2026
const YEARS := 10
const AFTER_YEARS := 40
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const JOURNEY := 3
const ARRIVED := 3
const LOCALES := ["ko", "en"]
const KEPT := "user://map_capture.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false, "views": ["minimap", "window"]},
	{"name": "pc_1280x720", "window": Vector2i(1280, 720), "touch": false, "views": ["minimap", "window"]},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true, "views": ["minimap"]},
]

var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _player := -1


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for run: Dictionary in RUNS:
			for view: String in run.views:
				_shots.append({"locale": locale, "run": run, "view": view})
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	_player = _world.player_army()
	for village: int in _nearest(JOURNEY):
		_walk_to(village)
	var known_before := _world.villages().size()
	for i in AFTER_YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		_world.tick()
	_player = _world.player_army()
	var live := _world.villages()
	var stale := []
	for village: Dictionary in _world.remembered_villages():
		var now: Dictionary = live[village.index]
		var peopled_now := _world.village_population(village.index) > 0
		if int(now.faction) != int(village.faction) or peopled_now != village.peopled:
			stale.append("%d: remembered %s %s, now %s %s" % [village.index, village.faction,
					"peopled" if village.peopled else "empty", now.faction, "peopled" if peopled_now else "empty"])
	var seen := _world.explored()
	print("map_capture: %d of %d cells seen, %d of %d villages remembered, %d founded since the journey; remembered and since changed: %s" % [
			seen.count(1), seen.size(), _world.remembered_villages().size(), live.size(), live.size() - known_before, stale])
	_open()


func _cell(at: Vector2) -> Vector2i:
	return Vector2i((at / WorldAdapter.CELL_SIZE).floor())


func _body() -> Vector2i:
	var roster := _world.roster(_player)
	return _cell(_world.person_position(roster[0])) if not roster.is_empty() else Vector2i(-1, -1)


func _centre(village: Dictionary) -> Vector2i:
	return Vector2i(village.corner) + Vector2i(village.area) / 2


# The `count` villages nearest the player's body, nearest first, its home left out.
func _nearest(count: int) -> Array:
	var here := _body()
	var order := []
	var villages := _world.villages()
	for index in villages.size():
		var distance := Vector2(_centre(villages[index]) - here).length()
		if distance > 6:
			order.append([distance, index])
	order.sort()
	return order.slice(0, count).map(func(pair: Array) -> int: return pair[1])


func _walk_to(village: int) -> void:
	var target := _centre(_world.villages()[village])
	var skills := PackedInt64Array(range(_world.skill_count(_player)))
	for i in DAYS_PER_YEAR * TICKS_PER_DAY:
		if _world.roster(_player).is_empty():
			return
		if Vector2(_body() - target).length() <= ARRIVED:
			return
		if i % 20 == 0:
			_world.order(_player, target, skills)
		_world.tick()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	# A delta of 0 draws and streams the ground without ticking the world.
	if _step >= 2 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if shot.run.touch else "off"))
			DisplayServer.window_set_size(shot.run.window)
		1:
			_play._play(_world)
			_play.set_process(false)
			_play._process(0.0)
			hud.labels.notice.visible = false
		2:
			if shot.view == "window":
				_play.open_panel(_play.MAP_KEY)
				print("map_capture: %s draws the map at %dx" % [shot.run.name, hud.map_window.map_scale()])
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("map_capture: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _save(shot: Dictionary) -> void:
	var path := "%s/%s_%s_%s.png" % [_dir, shot.view, shot.run.name, shot.locale]
	var image := root.get_viewport().get_texture().get_image()
	var error := image.save_png(path)
	print("map_capture: %s %s" % [path, "written" if error == OK else "not written: %d" % error])
