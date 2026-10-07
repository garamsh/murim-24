# Records the title, new-game, pause and settings screens of play.tscn in each locale, driving
# them as a player does through play.gd's navigate(), with 4x nearest-neighbour copies:
# - title_<locale>.png: the scene as it opens.
# - new_game_<locale>.png: after Enter on a new game and the digits of SEED typed.
# - pause_<locale>.png: after Enter starts that world, SETTLE_FRAMES frames of play and Escape.
# - settings_<locale>.png: after Down and Enter on the pause panel's settings.
# The language is chosen on the settings panel, kept in KEPT, which is removed at the end.
# Prints what each shows and fails if two images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
const SEED := "4817203365"
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_screens.cfg"
var _dir := "."
var _frame := 0
var _step := 0
var _locale := 0
var _play: Node2D
var _saved := {}
var _failed := false


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_open()


func _open() -> void:
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	var locale: String = LOCALES[_locale]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			# The language is chosen on the settings panel, as a player does, kept in a file of this
			# script's own so the player's settings are left as they were.
			hud.settings.path = KEPT
			hud.settings.choose("language", _locale)
		1:
			_shot("title_" + locale, "screen %s" % hud.screen().get_script().resource_path)
			_press(KEY_ENTER)
			for digit in SEED:
				_press(KEY_0 + int(digit))
		2:
			_shot("new_game_" + locale, "typed %s" % hud.new_game.typed)
			_press(KEY_ENTER)
		3:
			_press(KEY_ESCAPE)
		4:
			_shot("pause_" + locale, "seed %d, ticks %d, guide %s" % [hud.pause.world_seed, _play.ticks,
					hud.pause.controls.map(func(row: Dictionary) -> String: return "%s %s" % [row.keys, tr(row.does)])])
			_press(KEY_DOWN)
			_press(KEY_ENTER)
		5:
			_shot("settings_" + locale, "chosen %s, speed %dx" % [hud.settings.chosen, hud.settings.speed()])
			_play.queue_free()
			_locale += 1
			if _locale == LOCALES.size():
				DirAccess.remove_absolute(ProjectSettings.globalize_path(KEPT))
				print("capture_screens: %d images, none repeated %s" % [_saved.size(), not _failed])
				quit(1 if _failed else 0)
				return true
			_open()
			_step = -1
	_step += 1
	return false


func _press(keycode: Key) -> void:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	_play.navigate(key)


func _shot(name: String, note: String) -> void:
	var image := root.get_texture().get_image()
	var sha := image.get_data().hex_encode().sha256_text().left(16)
	if _saved.has(sha):
		_failed = true
	_saved[sha] = name
	image.save_png("%s/%s.png" % [_dir, name])
	var big := image.duplicate()
	big.resize(image.get_width() * 4, image.get_height() * 4, Image.INTERPOLATE_NEAREST)
	big.save_png("%s/%s_4x.png" % [_dir, name])
	print("capture_screens: %s.png %dx%d, sha256 %s, locale %s; %s" % [name, image.get_width(),
			image.get_height(), sha, TranslationServer.get_locale(), note])
