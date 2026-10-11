## The settings panel, drawn as art/mockup/settings_screen.png lays it out: the speed of the world's
## time, the window mode, the resolution in whole multiples of the native size, the language, the
## volume of all sound, of the effects and of the music, whether a cast key casts on its press or on
## its release, the touch controls, on a phone or a tablet, forced on, forced on with a tap on the
## ground walking there, or off, and whose damage floats up as numbers over the struck, no one's,
## the player's own or everyone's, whether a hit draws blood, and whether a hit on or from the
## player holds the frame a moment and shakes the screen, each a row of its choices with the chosen
## one in jade, and a line saying what the row being changed sets. The choices are the player's preferences, not the world's
## state: they are kept in a file under user:// and applied to the window, the translations, the
## audio buses, the HUD's layout, the play scene's pace and the world's drawing. A row's choice is
## chosen by a tap as well as by the keys. Every word comes from hud.csv through Godot's
## translations. It holds no game rule.
extends "res://hud/panel.gd"

## Sent once every row's choice has been applied.
signal applied

const PATH := "user://settings.cfg"
const SECTION := "settings"
# The mockup's layout, in the native 480x270.
const DIALOG := Rect2(60, 4, 360, 256)
# The mockup's rows were 26 pixels apart; the rows for casting, touch, damage numbers, blood and
# hit stop close them up, and the dialog stands 6 pixels higher, to keep the line under them within
# the panel.
const ROW_STEP := 16
const CHOICE_WIDTH := 64
const VOLUME_WIDTH := 40
## The rows, in the order they are drawn, each with the hud.csv keys of its label and what it sets.
const ROWS := ["speed", "window", "resolution", "language", "master", "effects", "music", "cast",
		"touch", "numbers", "blood", "hitstop"]
const LABEL_KEYS := {"speed": "SETTINGS_SPEED", "window": "SETTINGS_WINDOW",
		"resolution": "SETTINGS_RESOLUTION", "language": "SETTINGS_LANGUAGE",
		"master": "SETTINGS_MASTER", "effects": "SETTINGS_EFFECTS", "music": "SETTINGS_MUSIC",
		"cast": "SETTINGS_CAST", "touch": "SETTINGS_TOUCH", "numbers": "SETTINGS_NUMBERS",
		"blood": "SETTINGS_BLOOD", "hitstop": "SETTINGS_HITSTOP"}
const ABOUT_KEYS := {"speed": "SETTINGS_SPEED_ABOUT", "window": "SETTINGS_WINDOW_ABOUT",
		"resolution": "SETTINGS_RESOLUTION_ABOUT", "language": "SETTINGS_LANGUAGE_ABOUT",
		"master": "SETTINGS_MASTER_ABOUT", "effects": "SETTINGS_EFFECTS_ABOUT",
		"music": "SETTINGS_MUSIC_ABOUT", "cast": "SETTINGS_CAST_ABOUT", "touch": "SETTINGS_TOUCH_ABOUT",
		"numbers": "SETTINGS_NUMBERS_ABOUT", "blood": "SETTINGS_BLOOD_ABOUT",
		"hitstop": "SETTINGS_HITSTOP_ABOUT"}
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
## The touch row's choices: on where the screen takes touch, on, on with tap-to-move, and off.
const TOUCH_MODES := ["auto", "on", "tap", "off"]
const TOUCH_KEYS := ["SETTINGS_TOUCH_AUTO", "SETTINGS_TOUCH_ON", "SETTINGS_TOUCH_TAP", "SETTINGS_TOUCH_OFF"]
## Whose damage floats up as numbers over the struck: no one's, the steered person's own losses and
## hits, or everyone's.
const NUMBERS_MODES := ["off", "mine", "everyone"]
const NUMBERS_KEYS := ["SETTINGS_NUMBERS_OFF", "SETTINGS_NUMBERS_MINE", "SETTINGS_NUMBERS_EVERYONE"]
## Whether a hit draws blood, or shows only its flash.
const BLOOD_KEYS := ["SETTINGS_BLOOD_ON", "SETTINGS_BLOOD_OFF"]
## Whether a hit on or from the player holds the struck and the striker a few frames and shakes the
## screen.
const HITSTOP_KEYS := ["SETTINGS_HITSTOP_ON", "SETTINGS_HITSTOP_OFF"]
## What each row starts at before anything is kept: the native size at 4x is the project's window,
## the music sits under the effects, a cast key casts on its press, as Albion Online's quick
## cast does, and only the player's own damage floats up as numbers, which a battle of hundreds
## would otherwise bury; and a hit draws blood and holds the frame.
const DEFAULTS := {"speed": 0, "window": 0, "resolution": 2, "language": 0, "master": 4,
		"effects": 4, "music": 3, "cast": 0, "touch": 0, "numbers": 1, "blood": 0, "hitstop": 0}

## The index of each row's chosen choice, by row.
var chosen := DEFAULTS.duplicate()
## The row being changed, an index into ROWS.
var row := 0
## The locales the language row chooses among, set by the HUD.
var locales: Array = []
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
		"touch":
			for touch_key: String in TOUCH_KEYS:
				words.append(tr(touch_key))
		"numbers":
			for numbers_key: String in NUMBERS_KEYS:
				words.append(tr(numbers_key))
		"blood":
			for blood_key: String in BLOOD_KEYS:
				words.append(tr(blood_key))
		"hitstop":
			for hitstop_key: String in HITSTOP_KEYS:
				words.append(tr(hitstop_key))
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


## Returns whether the touch controls are shown: on a phone or a tablet, unless the touch row
## turns them off, or anywhere it turns them on. A desktop's touch screen is not asked after: Godot
## 4.7.2 reports one wherever a mouse stands in for a finger, as it does while they are shown.
func touch_shown() -> bool:
	match TOUCH_MODES[chosen.touch]:
		"auto":
			return OS.has_feature("mobile")
		"off":
			return false
	return true


## Returns whose damage floats up as numbers over the struck, one of NUMBERS_MODES.
func damage_numbers() -> String:
	return NUMBERS_MODES[chosen.numbers]


## Returns whether a hit draws blood.
func blood() -> bool:
	return chosen.blood == 0


## Returns whether a hit on or from the steered person holds the frame a moment and shakes the
## screen.
func hit_stop() -> bool:
	return chosen.hitstop == 0


## Returns whether a tap on the ground walks the steered person there.
func tap_to_move() -> bool:
	return TOUCH_MODES[chosen.touch] == "tap"


## Works the panel with a tap at `at`: a row's choice tapped is chosen, as left and right choose
## it, and the close word, or anywhere outside the panel, goes back. Returns "back" when the tap
## does that, and "" otherwise.
func tap(at: Vector2) -> String:
	if not DIALOG.has_point(at) or _close_hit(DIALOG, tr("SETTINGS_BACK"), at):
		close()
		return "back"
	for n in ROWS.size():
		var name: String = ROWS[n]
		var y := DIALOG.position.y + 30 + n * ROW_STEP
		var slip := choice_width(name)
		var x := choices_x(choices(name).size(), slip)
		for k in choices(name).size():
			if Rect2(x + k * (slip + 4), y + 2, slip, SLIP_HEIGHT).has_point(at):
				row = n
				choose(name, k)
				queue_redraw()
				return ""
	return ""


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
		if value is int and value >= 0 and value < choices(name).size():
			chosen[name] = value
	return true


## Applies every row's choice: the language to the translations, the window mode and resolution
## to the window and the volumes to their buses, a volume of none muting its bus. The speed, the
## damage numbers, the blood and the hit stop are read by the play scene.
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
	applied.emit()


func _draw() -> void:
	_dim()
	_dialog(DIALOG, tr("SETTINGS_TITLE"), tr("SETTINGS_BACK"), tr("SETTINGS_KEYS"))
	var left := DIALOG.position.x + 10
	var width := DIALOG.size.x - 20
	for n in ROWS.size():
		var name: String = ROWS[n]
		var y := DIALOG.position.y + 30 + n * ROW_STEP
		if n > 0:
			draw_rect(Rect2(left, y - 5, width, 1), Palette.INK_3)
		if n == row:
			draw_rect(Rect2(left, y - 4, width, ROW_STEP - 2), Palette.INK_1)
			draw_rect(Rect2(left, y - 4, 1, ROW_STEP - 2), Palette.JADE_3)
		_text(Vector2(left + 8, y + 3), tr(LABEL_KEYS[name]), Palette.GOLD_4 if n == row else Palette.GOLD_3)
		var words := choices(name)
		var slip := choice_width(name)
		var x := choices_x(words.size(), slip)
		for k in words.size():
			_slip(Vector2(x, y + 2), slip, words[k], k == chosen[name])
			x += slip + 4
	var about_y := DIALOG.position.y + 30 + ROWS.size() * ROW_STEP
	draw_rect(Rect2(left, about_y - 5, width, 1), Palette.INK_3)
	_text(Vector2(left + 8, about_y + 2), about(ROWS[row]), Palette.STONE_4)


## Returns what row `name` sets, in the current locale.
func about(name: String) -> String:
	return tr(ABOUT_KEYS[name])


## Returns how wide each of row `name`'s choices' slips is.
static func choice_width(name: String) -> int:
	return VOLUME_WIDTH if BUSES.has(name) else CHOICE_WIDTH


## Returns where the first of `count` choices `width` wide starts, so that they end at the row's
## right.
static func choices_x(count: int, width: int) -> float:
	return DIALOG.end.x - 10 - 4 - count * (width + 4) + 4
