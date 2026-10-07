## The settings panel, drawn as art/mockup/settings_screen.png lays it out: the speed of the
## world's time, the window mode, the resolution in whole multiples of the native size, the
## language, the volume of all sound, of the effects and of the music, and whether a cast key
## casts on its press or on its release, each a row of its choices
## with the chosen one in jade, and a line saying what the row being changed sets. The choices are
## the player's preferences, not the world's state: they are kept in a file under user:// and
## applied to the window, the translations, the audio buses and the play scene's pace. Every word comes from hud.csv through Godot's translations. It holds no game rule.
extends "res://hud/panel.gd"

const PATH := "user://settings.cfg"
const SECTION := "settings"
# The mockup's layout, in the native 480x270.
const DIALOG := Rect2(60, 10, 360, 250)
# The mockup's rows were 26 pixels apart; a row for casting closes them up to keep the line under
# them within the panel.
const ROW_STEP := 22
const CHOICE_WIDTH := 64
const VOLUME_WIDTH := 40
## The rows, in the order they are drawn, each with the hud.csv keys of its label and what it sets.
const ROWS := ["speed", "window", "resolution", "language", "master", "effects", "music", "cast"]
const LABEL_KEYS := {"speed": "SETTINGS_SPEED", "window": "SETTINGS_WINDOW",
		"resolution": "SETTINGS_RESOLUTION", "language": "SETTINGS_LANGUAGE",
		"master": "SETTINGS_MASTER", "effects": "SETTINGS_EFFECTS", "music": "SETTINGS_MUSIC",
		"cast": "SETTINGS_CAST"}
const ABOUT_KEYS := {"speed": "SETTINGS_SPEED_ABOUT", "window": "SETTINGS_WINDOW_ABOUT",
		"resolution": "SETTINGS_RESOLUTION_ABOUT", "language": "SETTINGS_LANGUAGE_ABOUT",
		"master": "SETTINGS_MASTER_ABOUT", "effects": "SETTINGS_EFFECTS_ABOUT",
		"music": "SETTINGS_MUSIC_ABOUT", "cast": "SETTINGS_CAST_ABOUT"}
## How many times the core's tick rate the world runs at.
const SPEEDS := [1, 2, 4]
const WINDOW_MODES := [DisplayServer.WINDOW_MODE_WINDOWED, DisplayServer.WINDOW_MODE_FULLSCREEN]
const WINDOW_KEYS := ["SETTINGS_WINDOWED", "SETTINGS_FULLSCREEN"]
## Whole multiples of the native 480x270.
const RESOLUTIONS := [Vector2i(960, 540), Vector2i(1440, 810), Vector2i(1920, 1080)]
## The volume rows' audio buses, and the share of full volume each of their choices sets.
const BUSES := {"master": &"Master", "effects": &"Effects", "music": &"Music"}
const VOLUMES := [0.0, 0.25, 0.5, 0.75, 1.0]
## Whether a cast key casts on its press or on its release.
const CAST_KEYS := ["SETTINGS_CAST_PRESS", "SETTINGS_CAST_RELEASE"]
## What each row starts at before anything is kept: the native size at 4x is the project's window,
## the music sits under the effects, and a cast key casts on its press, as Albion Online's quick
## cast does.
const DEFAULTS := {"speed": 0, "window": 0, "resolution": 2, "language": 0, "master": 4,
		"effects": 4, "music": 3, "cast": 0}

## The index of each row's chosen choice, by row.
var chosen := DEFAULTS.duplicate()
## The row being changed, an index into ROWS.
var row := 0
## The locales the language row chooses among, and the name of the key that switches them outside
## this panel, set by the HUD.
var locales: Array = []
var locale_key := ""
## The file the settings are kept in.
var path := PATH


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


## Opens the panel on its first row.
func open() -> void:
	row = 0
	visible = true
	queue_redraw()


## Works the panel with `key`: up and down choose the row, left and right change its choice,
## which is applied and kept at once, and Escape goes back. Returns "back" when the key does that,
## and "" otherwise.
func key(event: InputEventKey) -> String:
	if event.keycode == KEY_UP or event.keycode == KEY_DOWN:
		row = clampi(row + (1 if event.keycode == KEY_DOWN else -1), 0, ROWS.size() - 1)
	elif event.keycode == KEY_LEFT or event.keycode == KEY_RIGHT:
		var name: String = ROWS[row]
		choose(name, clampi(chosen[name] + (1 if event.keycode == KEY_RIGHT else -1), 0,
				choices(name).size() - 1))
	elif event.keycode == KEY_ESCAPE:
		close()
		return "back"
	queue_redraw()
	return ""


## Chooses choice `index` of row `name`, applies it and keeps every row in `path`.
func choose(name: String, index: int) -> void:
	chosen[name] = index
	apply()
	keep()


## Notes the locale the translations are in now as the language chosen, and keeps it: F2 switches
## the language outside this panel.
func note_locale() -> void:
	chosen.language = maxi(locales.find(TranslationServer.get_locale().left(2)), 0)
	keep()
	queue_redraw()


## Returns the words of row `name`'s choices, in the current locale.
func choices(name: String) -> Array[String]:
	var words: Array[String] = []
	match name:
		"speed":
			for speed: int in SPEEDS:
				words.append(tr("SETTINGS_SPEED_TIMES") % speed)
		"window":
			for window_key: String in WINDOW_KEYS:
				words.append(tr(window_key))
		"resolution":
			for resolution: Vector2i in RESOLUTIONS:
				words.append("%dx%d" % [resolution.x, resolution.y])
		"language":
			for locale: String in locales:
				words.append(tr("LOCALE_" + locale.to_upper()))
		"cast":
			for cast_key: String in CAST_KEYS:
				words.append(tr(cast_key))
		_:
			for volume: float in VOLUMES:
				words.append("%d%%" % roundi(volume * 100))
	return words


## Returns how many times the core's tick rate the world runs at.
func speed() -> int:
	return SPEEDS[chosen.speed]


## Returns whether a cast key casts on its release, after aiming while it is held, rather than on
## its press.
func cast_on_release() -> bool:
	return chosen.cast == 1


## Keeps every row's choice in `path`.
func keep() -> Error:
	var file := ConfigFile.new()
	for name: String in ROWS:
		file.set_value(SECTION, name, chosen[name])
	return file.save(path)


## Reads every row's choice from `path`; a row the file lacks, or holds out of range, keeps its
## default. Returns whether the file was read.
func read() -> bool:
	chosen = DEFAULTS.duplicate()
	var file := ConfigFile.new()
	if file.load(path) != OK:
		return false
	for name: String in ROWS:
		var value: Variant = file.get_value(SECTION, name, DEFAULTS[name])
		if value is int and value >= 0 and value < _count(name):
			chosen[name] = value
	return true


## Applies every row's choice: the language to the translations, the window mode and resolution
## to the window and the volumes to their buses, a volume of none muting its bus. The speed is
## read by the play scene.
func apply() -> void:
	for name: String in BUSES:
		var bus := AudioServer.get_bus_index(BUSES[name])
		var volume: float = VOLUMES[chosen[name]]
		AudioServer.set_bus_mute(bus, volume == 0.0)
		if volume > 0.0:
			AudioServer.set_bus_volume_linear(bus, volume)
	if not locales.is_empty():
		TranslationServer.set_locale(locales[chosen.language])
	DisplayServer.window_set_mode(WINDOW_MODES[chosen.window])
	if WINDOW_MODES[chosen.window] == DisplayServer.WINDOW_MODE_WINDOWED:
		DisplayServer.window_set_size(RESOLUTIONS[chosen.resolution])


func _count(name: String) -> int:
	match name:
		"speed":
			return SPEEDS.size()
		"window":
			return WINDOW_MODES.size()
		"resolution":
			return RESOLUTIONS.size()
		"language":
			return locales.size()
		"cast":
			return CAST_KEYS.size()
	return VOLUMES.size()


func _draw() -> void:
	_dim()
	_dialog(DIALOG, tr("SETTINGS_TITLE"), tr("SETTINGS_BACK"), tr("SETTINGS_KEYS"))
	var left := DIALOG.position.x + 10
	var width := DIALOG.size.x - 20
	for n in ROWS.size():
		var name: String = ROWS[n]
		var y := DIALOG.position.y + 30 + n * ROW_STEP
		if n > 0:
			draw_rect(Rect2(left, y - 5, width, 1), INK_LIGHT)
		if n == row:
			draw_rect(Rect2(left, y - 4, width, ROW_STEP - 2), INK_DARK)
			draw_rect(Rect2(left, y - 4, 1, ROW_STEP - 2), JADE)
		_text(Vector2(left + 8, y + 3), tr(LABEL_KEYS[name]), GOLD_LIGHT if n == row else GOLD)
		var words := choices(name)
		var slip := choice_width(name)
		var x := choices_x(words.size(), slip)
		for k in words.size():
			_slip(Vector2(x, y + 2), slip, words[k], k == chosen[name])
			x += slip + 4
	var about_y := DIALOG.position.y + 30 + ROWS.size() * ROW_STEP
	draw_rect(Rect2(left, about_y - 5, width, 1), INK_LIGHT)
	_text(Vector2(left + 8, about_y + 2), about(ROWS[row]), STONE)


## Returns what row `name` sets, in the current locale.
func about(name: String) -> String:
	return tr(ABOUT_KEYS[name]) % locale_key if name == "language" else tr(ABOUT_KEYS[name])


## Returns how wide each of row `name`'s choices' slips is.
static func choice_width(name: String) -> int:
	return VOLUME_WIDTH if BUSES.has(name) else CHOICE_WIDTH


## Returns where the first of `count` choices `width` wide starts, so that they end at the row's
## right.
static func choices_x(count: int, width: int) -> float:
	return DIALOG.end.x - 10 - 4 - count * (width + 4) + 4
