# Captures the shortcut rail of #186 in the real play.tscn, in each locale, at each PC window in
# WINDOWS, its input pushed through the window as a player's would be:
# - idle_<window>_<locale>.png: the world with the rail under the map, nothing open.
# - hover_<window>_<locale>.png: the cursor moved onto the factions row, which is marked in gold.
# - open_<window>_<locale>.png: a left click on the genealogy row, which opens the panel and lights
#   its row in jade over the panel's dimmed room.
# - settings_<window>_<locale>.png: a left click on the settings row, which opens the settings.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://rail_capture.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const LOCALES := ["ko", "en"]
const KEPT := "user://rail_capture.cfg"
const WINDOWS := [Vector2i(1280, 720), Vector2i(1920, 1080), Vector2i(2560, 1440)]
const VIEWS := ["idle", "hover", "open", "settings"]

var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _at := 0
var _play: Node2D


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for window: Vector2i in WINDOWS:
			for view: String in VIEWS:
				_shots.append({"locale": locale, "window": window, "view": view})
	_open()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(shot.window)
		1:
			_play.start(SEED)
		2:
			var rail: Control = hud.shortcuts
			var row: int = {"hover": 2, "open": 0, "settings": 4}.get(shot.view, -1)
			if row >= 0:
				var at: Vector2 = hud.to_window(rail.row_rect(row).get_center())
				var motion := InputEventMouseMotion.new()
				motion.position = at
				motion.global_position = at
				root.push_input(motion)
				if shot.view != "hover":
					var click := InputEventMouseButton.new()
					click.button_index = MOUSE_BUTTON_LEFT
					click.pressed = true
					click.position = at
					click.global_position = at
					root.push_input(click)
		3:
			var image := root.get_texture().get_image()
			var name := "%s_%dx%d_%s" % [shot.view, shot.window.x, shot.window.y, shot.locale]
			image.save_png(_dir.path_join(name + ".png"))
			var rail: Control = hud.shortcuts
			print("rail_capture: %s.png %dx%d; HUD room %s; rail %s; hovered %d, lit \"%s\"; open %s" % [name,
					image.get_width(), image.get_height(), hud.room, rail.rect(), rail.hovered, rail.lit,
					hud.screen().get_script().resource_path.get_file() if hud.screen() != null else
					(hud.panel().get_script().resource_path.get_file() if hud.panel() != null else "none")])
			_at += 1
			if _at >= _shots.size():
				print("rail_capture: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false
