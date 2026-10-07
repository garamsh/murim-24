# Records saving and continuing in play.tscn in each locale, driving it as a player does through
# play.gd's navigate(), with 4x nearest-neighbour copies:
# - title_no_save_<locale>.png: the scene as it opens with slot 0 empty; Continue is greyed.
# - pause_saved_<locale>.png: after a new game from SEED, Escape, Down and Enter on Save.
# - title_continue_<locale>.png: the scene opened again, as leaving for the title reloads it, with
#   Down choosing Continue.
# - continued_<locale>.png: after Enter on Continue, the world resumed.
# - title_load_error_<locale>.png: the scene opened again over a slot 0 of bytes no save begins
#   with, after Down and Enter on Continue.
# The language is chosen on the settings panel at each opening, kept in KEPT, which is removed at the end. A save
# already in slot 0 is set aside before and put back after. Prints what each shows and fails if two
# images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
const SEED := "4817203365"
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_saves.cfg"
const SLOT := "user://saves/slot_0.sav"
const ASIDE := "user://saves/slot_0.sav.aside"
var _dir := "."
var _frame := 0
var _step := 0
var _locale := 0
var _play: Node2D
var _saved := {}
var _failed := false
var _hash := 0


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	if FileAccess.file_exists(SLOT):
		DirAccess.rename_absolute(SLOT, ASIDE)
	_open()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	# The language is chosen on the settings panel, as a player does, kept in a file of this
	# script's own so the player's settings are left as they were; each opening reads the
	# player's file, so it is chosen again.
	var hud: CanvasLayer = _play.get_node("Hud")
	hud.settings.path = KEPT
	hud.settings.choose("language", _locale)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	var locale: String = LOCALES[_locale]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			pass
		1:
			_shot("title_no_save_" + locale, "unavailable %s, slot 0 refusal %d" % [hud.title_screen.unavailable,
					WorldAdapter.load_refusal(0)])
			_press(KEY_ENTER)
			for digit in SEED:
				_press(KEY_0 + int(digit))
			_press(KEY_ENTER)
		2:
			_press(KEY_ESCAPE)
			_press(KEY_DOWN)
			_press(KEY_ENTER)
		3:
			_hash = _play.world.hash()
			_shot("pause_saved_" + locale, "chosen %s, notice %s, seed %d, ticks %d, hash %d, slot 0 refusal %d" % [
					hud.pause.ENTRIES[hud.pause.chosen], hud.pause.notice, hud.pause.world_seed, _play.ticks, _hash,
					WorldAdapter.load_refusal(0)])
			_open()
		4:
			_press(KEY_DOWN)
		5:
			_shot("title_continue_" + locale, "chosen %s, unavailable %s" % [
					hud.title_screen.ENTRIES[hud.title_screen.chosen], hud.title_screen.unavailable])
			_press(KEY_ENTER)
		6:
			_shot("continued_" + locale, "world seed %d, steered army %d, player_army %d, screen %s, hash at load %d" % [
					_play.world_seed, _play.steered, _play.world.player_army(), hud.screen(),
					WorldAdapter.load(0).hash()])
			var file := FileAccess.open(SLOT, FileAccess.WRITE)
			file.store_string("not a save")
			file.close()
			_open()
		7:
			_press(KEY_DOWN)
			_press(KEY_ENTER)
		8:
			_shot("title_load_error_" + locale, "world %s, notice %s, slot 0 refusal %d" % [_play.world,
					hud.title_screen.notice, WorldAdapter.load_refusal(0)])
			DirAccess.remove_absolute(SLOT)
			_locale += 1
			if _locale == LOCALES.size():
				_play.queue_free()
				DirAccess.remove_absolute(ProjectSettings.globalize_path(KEPT))
				if FileAccess.file_exists(ASIDE):
					DirAccess.rename_absolute(ASIDE, SLOT)
				print("capture_saves: %d images, none repeated %s" % [_saved.size(), not _failed])
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
	print("capture_saves: %s.png %dx%d, sha256 %s, locale %s; %s" % [name, image.get_width(),
			image.get_height(), sha, TranslationServer.get_locale(), note])
