# Records play.tscn as #163 sizes it, at each window size in WINDOWS, in each locale:
# - hud_<size>_<locale>.png: the keyboard HUD over the world.
# - settings_<size>_<locale>.png: the settings, opened by the settings button.
# - arts_<size>_<locale>.png: K's arts panel.
# - title_<size>_<locale>.png: the title screen;
# and with the touch controls on, at TOUCH_WINDOWS:
# - touch_<size>_<locale>.png and title_touch_<size>_<locale>.png: the touch HUD and the title.
# The window is sized to each in turn, after the settings apply their own size; the screen must
# hold the largest. The world is SEED's region with days of five ticks, lived YEARS years, steering the army
# of the first person who knows an art skill, as evidence/pr-161's capture_arts.gd does. The play
# scene's own ticks are held. Prints each image's size, scales and what it shows.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_pc_hud.cfg"
const WINDOWS := [Vector2i(1280, 720), Vector2i(1920, 1080), Vector2i(2560, 1440)]
const TOUCH_WINDOWS := [Vector2i(1920, 1080), Vector2i(2400, 1080)]
var _dir := "."
var _frame := 0
var _step := 0
var _runs: Array = []
var _run := 0
var _play: Node2D
var _world: WorldAdapter
var _army := -1


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for window: Vector2i in WINDOWS:
			_runs.append({"locale": locale, "window": window, "touch": false})
		for window: Vector2i in TOUCH_WINDOWS:
			_runs.append({"locale": locale, "window": window, "touch": true})
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		_world.tick()
	for army in _world.army_count():
		var roster := _world.roster(army)
		if roster.is_empty():
			continue
		var known := false
		for skill: Dictionary in _world.person_skills(roster[0]):
			known = known or skill.art >= 0
		if known:
			_army = army
			break
	_open()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _hud() -> CanvasLayer:
	return _play.get_node("Hud")


func _process(_delta: float) -> bool:
	_frame += 1
	# The play scene's own frames draw the bodies, its ticks held.
	if _step >= 3 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var run: Dictionary = _runs[_run]
	var hud := _hud()
	var name: String = "%dx%d_%s" % [run.window.x, run.window.y, run.locale]
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(run.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if run.touch else "off"))
			hud.title_screen.queue_redraw()
			DisplayServer.window_set_size(run.window)
		1:
			_shot(("title_touch_" if run.touch else "title_") + name, "title")
			_play._play(_world)
			_play.set_process(false)
			_play.steered = _army
			_play._art.steered = _army
			_play._body = _world.roster(_army)[0]
			hud.show_player(_world, _army)
		2:
			# A tick, so the bodies are drawn where it leaves them.
			_world.tick()
			_play._art.note_tick()
			_play._process(0.0)
			hud.labels.notice.visible = false
		3:
			if run.touch:
				_shot("touch_" + name, "touch shown %s" % hud.touch.visible)
				_next()
				return _run >= _runs.size()
			_shot("hud_" + name, "settings button at %s, %s" % [hud.settings_button.position, hud.settings_button.size])
			var click := InputEventMouseButton.new()
			click.button_index = MOUSE_BUTTON_LEFT
			click.pressed = true
			hud.settings_button.gui_input.emit(click)
		4:
			_shot("settings_" + name, "settings open %s" % hud.settings.visible)
			hud.settings.key(_key(KEY_ESCAPE))
			_play._take("back")
			_play.command(_key(KEY_K))
		5:
			_shot("arts_" + name, "arts open %s" % hud.arts.visible)
			_next()
			return _run >= _runs.size()
	_step += 1
	return false


func _next() -> void:
	_run += 1
	if _run >= _runs.size():
		print("capture_pc_hud: done")
		quit(0)
		return
	_open()


func _shot(name: String, what: String) -> void:
	var run: Dictionary = _runs[_run]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("capture_pc_hud: %s.png %dx%d, world %dx, HUD %dx in %s, sha256 %s; %s" % [name, image.get_width(),
			image.get_height(), ViewScale.world_scale(run.window, run.touch), ViewScale.hud_scale(run.window, run.touch),
			ViewScale.hud_room(run.window, run.touch), FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16), what])


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
