## Captures the settings panel over the play scene at 3d20657, in Korean, at 1920x1080, on its
## damage numbers row, with each of the row's choices, off, mine and everyone, chosen in turn: the
## dialog, 1440x1024, saved once for each. The choices are kept in a file of the capture's own, not
## the player's. Exits on its own after the third.
extends SceneTree

const SCENE := "res://play/play.tscn"
const SEED := 2026
const CULT_VILLAGE := 1
const DIALOG := Rect2i(240, 16, 1440, 1024)

var _scene: Node2D
var _settings
var _frame := 0
var _out := ""


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	_scene = load(SCENE).instantiate()
	_scene.birth_village = CULT_VILLAGE
	_scene.save_directory = "user://capture_saves"
	root.add_child(_scene)


# Keeps the choices in a file of the capture's own and sets them to their defaults, in Korean, once
# the scene has read the player's.
func _defaults() -> void:
	var settings = _scene.get_node("Hud").settings
	settings.path = "user://capture_r2_settings.cfg"
	settings.chosen = settings.DEFAULTS.duplicate()
	settings.chosen.language = settings.locales.find("ko")
	settings.apply()
	DisplayServer.window_set_size(Vector2i(1920, 1080))


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 1:
		_defaults()
	if _frame == 2:
		_settings = _scene.get_node("Hud").settings
		_scene.start(SEED)
		_scene.open_settings()
		_settings.row = _settings.ROWS.find("numbers")
	# Each choice is chosen, drawn for a few frames, then saved.
	var choice := (_frame - 10) / 10
	if _frame >= 10 and (_frame - 10) % 10 == 0 and choice < 3:
		_settings.choose("numbers", choice)
		_settings.queue_redraw()
	if _frame >= 10 and (_frame - 10) % 10 == 8 and choice < 3:
		var name: String = _settings.NUMBERS_MODES[choice]
		print("settings: %s chosen, the art showing %s, locale %s" % [name,
				_scene.get_node("WorldArt/Combat").damage_numbers, TranslationServer.get_locale()])
		root.get_texture().get_image().get_region(DIALOG).save_png("%s/settings_%d_%s.png" % [_out, choice, name])
	if _frame >= 45:
		quit(0)
	return false
