# Checks that the settings persist: choices changed on the panel are kept in its file under
# user://, and a panel that reads the file back holds the same choices, its speed, casting on
# release and damage numbers hidden among them. Each volume chosen is applied to its audio bus, and a
# volume of none mutes its bus while the buses turned down but not off stay unmuted. A value out of range in the file is refused for the
# row's default. Controls: a panel reading a file that does not exist holds every default, casting
# on press and damage numbers shown among them, and the
# in-range values beside the refused one are read. Every word the panel draws from hud.csv fits
# the room it is given, in each locale.
extends RefCounted

const Settings := preload("res://hud/settings.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const PATH := "user://settings_test.cfg"
const MISSING := "user://settings_test_missing.cfg"


func run() -> bool:
	_remove(PATH)
	_remove(MISSING)
	var panel := _panel(PATH)
	# Right twice on the first row, the speed; down and right on the window; down to the
	# resolution and left; down to the language and right; down and left once on the master
	# volume, twice on the effects and three times on the music; down and right on the casting; down
	# past the touch and right on the damage numbers.
	for keycode: Key in [KEY_RIGHT, KEY_RIGHT, KEY_DOWN, KEY_RIGHT, KEY_DOWN, KEY_LEFT, KEY_DOWN, KEY_RIGHT,
			KEY_DOWN, KEY_LEFT, KEY_DOWN, KEY_LEFT, KEY_LEFT, KEY_DOWN, KEY_LEFT, KEY_LEFT, KEY_LEFT,
			KEY_DOWN, KEY_RIGHT, KEY_DOWN, KEY_DOWN, KEY_RIGHT]:
		panel.key(_key(keycode))
	var chosen: Dictionary = panel.chosen.duplicate()
	var expected := {"speed": 2, "window": 1, "resolution": 1, "language": 1, "master": 3, "effects": 2,
		"music": 0, "cast": 1, "touch": 0, "numbers": 1}
	# The buses as the choices left them: the music muted, the others at their share. Control: the
	# master and the effects, turned down but not off, are not muted.
	var master := AudioServer.get_bus_index(&"Master")
	var effects := AudioServer.get_bus_index(&"Effects")
	var music := AudioServer.get_bus_index(&"Music")
	var volumes_applied := effects > 0 and music > 0 and AudioServer.is_bus_mute(music) \
			and not AudioServer.is_bus_mute(master) and not AudioServer.is_bus_mute(effects) \
			and is_equal_approx(AudioServer.get_bus_volume_linear(master), 0.75) \
			and is_equal_approx(AudioServer.get_bus_volume_linear(effects), 0.5)
	# Setup: the file holds what was chosen, read back without the panel.
	var file := ConfigFile.new()
	var on_disk := {}
	if file.load(PATH) == OK:
		for row: String in Settings.ROWS:
			on_disk[row] = file.get_value(Settings.SECTION, row)
	var reread := _panel(PATH)
	var read: bool = reread.read()
	var read_back: Dictionary = reread.chosen.duplicate()
	var kept: bool = chosen == expected and on_disk == expected and read and read_back == expected \
			and reread.speed() == Settings.SPEEDS[2] and reread.cast_on_release() and volumes_applied \
			and not reread.damage_numbers()

	# Control: a file that does not exist reads as every default.
	var missing := _panel(MISSING)
	var defaults: bool = not missing.read() and missing.chosen == Settings.DEFAULTS \
			and not missing.cast_on_release() and missing.damage_numbers()

	# A resolution out of range is refused; the speed beside it in range is read.
	file = ConfigFile.new()
	file.set_value(Settings.SECTION, "speed", 1)
	file.set_value(Settings.SECTION, "resolution", Settings.RESOLUTIONS.size())
	file.save(PATH)
	var ranged := _panel(PATH)
	ranged.read()
	var refused: bool = ranged.chosen.resolution == Settings.DEFAULTS.resolution and ranged.chosen.speed == 1

	TranslationServer.set_locale(Hud.LOCALES[0])
	_remove(PATH)
	var passed := kept and defaults and refused
	# The defaults restored, so the buses are left as the game starts them.
	missing.apply()
	print("settings_test: chosen %s, on disk %s, read back %s with speed %dx, casting on release and damage numbers hidden, the music muted and the master and effects at 0.75 and 0.5 unmuted %s: %s; a missing file reads the defaults %s; a resolution out of range refused, the speed beside it read %s: %s" % [
		chosen, on_disk, read_back, reread.speed(), volumes_applied, kept, defaults, refused,
		"pass" if passed else "FAIL"])
	for each: Control in [panel, reread, missing, ranged]:
		each.free()
	return passed and _tap_pass() and _fit_pass()


# A tap on a choice of the touch row chooses it, as the keys do, and sends `applied`: on shows the
# touch controls, tap-to-move shows them and walks where the ground is tapped, off hides them, and
# auto shows them only on a phone or a tablet. A tap outside the panel goes back. Control: a
# tap on the row's label, off every slip, chooses nothing and sends nothing.
func _tap_pass() -> bool:
	_remove(PATH)
	var panel := _panel(PATH)
	var sent := [0]
	panel.applied.connect(func() -> void: sent[0] += 1)
	panel.open()
	var row := Settings.ROWS.find("touch")
	var y := Settings.DIALOG.position.y + 30 + row * Settings.ROW_STEP + 2 + Settings.SLIP_HEIGHT / 2.0
	var slip := Settings.choice_width("touch")
	var first := Settings.choices_x(Settings.TOUCH_MODES.size(), slip)
	var shown := {}
	for mode in Settings.TOUCH_MODES.size():
		panel.tap(Vector2(first + mode * (slip + 4) + slip / 2.0, y))
		shown[Settings.TOUCH_MODES[mode]] = [panel.chosen.touch == mode, panel.touch_shown(), panel.tap_to_move()]
	var after_slips: int = sent[0]
	var on_label: String = panel.tap(Vector2(Settings.DIALOG.position.x + 20, y))
	var label_ignored: bool = on_label == "" and panel.chosen.touch == Settings.TOUCH_MODES.find("off") \
			and sent[0] == after_slips
	var outside: String = panel.tap(Vector2(4, 4))
	var mobile := OS.has_feature("mobile")
	var passed: bool = shown == {"auto": [true, mobile, false], "on": [true, true, false],
			"tap": [true, true, true], "off": [true, false, false]} and after_slips == 4 and label_ignored \
			and outside == "back" and not panel.visible
	print("settings_test: the touch row's choices tapped, each [chosen, shown, tap-to-move]: %s, on a phone or tablet %s; applied sent %d times; a tap on the label chose nothing %s; outside goes %s: %s" % [
		shown, mobile, after_slips, label_ignored, outside, "pass" if passed else "FAIL"])
	_remove(PATH)
	panel.free()
	return passed


# In each locale: the title clear of the close key in the title bar; each row's label clear of its
# choices, and each choice within its slip; what each row sets, and the keys along the foot, within
# the panel, and what each row sets above the foot. Setup: every SETTINGS_ and LOCALE_ key is measured.
func _fit_pass() -> bool:
	var fit := WordFit.new()
	var dialog := Settings.DIALOG
	var panel := _panel(MISSING)
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var title := fit.say("SETTINGS_TITLE")
		var back := fit.say("SETTINGS_BACK")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(back) + 8, dialog.size.x)
		for key: String in ["SETTINGS_SPEED_TIMES", "SETTINGS_WINDOWED", "SETTINGS_FULLSCREEN", "SETTINGS_CAST_PRESS",
				"SETTINGS_CAST_RELEASE", "LOCALE_KO", "LOCALE_EN"] + Settings.TOUCH_KEYS + Settings.NUMBERS_KEYS:
			fit.say(key)
		for row: String in Settings.ROWS:
			var choices: Array[String] = panel.choices(row)
			var label := fit.say(Settings.LABEL_KEYS[row])
			var slip := Settings.choice_width(row)
			fit.check(label, dialog.position.x + 10 + 8 + fit.width(label) + 4, Settings.choices_x(choices.size(), slip))
			for choice in choices:
				fit.check(choice, fit.width(choice) + 4, slip)
			fit.say(Settings.ABOUT_KEYS[row])
			var about: String = panel.about(row)
			fit.check(about, 10 + 8 + fit.width(about), dialog.size.x - 10)
		var keys := fit.say("SETTINGS_KEYS")
		fit.check(keys, 8 + fit.width(keys), dialog.size.x - 8)
		# The line under the rows, as tall as the font, ends above the line over the foot.
		var about_end := 30 + Settings.ROWS.size() * Settings.ROW_STEP + 2 + Hud.FONT_SIZE
		fit.check("%d rows" % Settings.ROWS.size(), about_end, dialog.size.y - 20)
	TranslationServer.set_locale(Hud.LOCALES[0])
	panel.free()
	var unmeasured := fit.unmeasured(["SETTINGS_", "LOCALE_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("settings_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed


# A settings panel kept in `path`, choosing among the HUD's locales.
func _panel(path: String) -> Control:
	var panel: Control = Settings.new()
	panel.path = path
	panel.locales = Hud.LOCALES
	return panel


func _remove(path: String) -> void:
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(ProjectSettings.globalize_path(path))


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
