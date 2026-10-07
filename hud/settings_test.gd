# Checks that the settings persist: choices changed on the panel are kept in its file under
# user://, and a panel that reads the file back holds the same choices, its speed among them; a
# choice F2 made is kept as the language. Each volume chosen is applied to its audio bus, and a
# volume of none mutes its bus while the buses turned down but not off stay unmuted. A value out of range in the file is refused for the
# row's default. Controls: a panel reading a file that does not exist holds every default, and the
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
	# volume, twice on the effects and three times on the music.
	for keycode: Key in [KEY_RIGHT, KEY_RIGHT, KEY_DOWN, KEY_RIGHT, KEY_DOWN, KEY_LEFT, KEY_DOWN, KEY_RIGHT,
			KEY_DOWN, KEY_LEFT, KEY_DOWN, KEY_LEFT, KEY_LEFT, KEY_DOWN, KEY_LEFT, KEY_LEFT, KEY_LEFT]:
		panel.key(_key(keycode))
	var chosen: Dictionary = panel.chosen.duplicate()
	var expected := {"speed": 2, "window": 1, "resolution": 1, "language": 1, "master": 3, "effects": 2,
		"music": 0}
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
			and reread.speed() == Settings.SPEEDS[2] and volumes_applied

	# F2 switches the locale outside the panel; noting it keeps it.
	TranslationServer.set_locale("ko")
	reread.note_locale()
	var noted := _panel(PATH)
	noted.read()
	var locale_kept: bool = noted.chosen.language == 0

	# Control: a file that does not exist reads as every default.
	var missing := _panel(MISSING)
	var defaults: bool = not missing.read() and missing.chosen == Settings.DEFAULTS

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
	var passed := kept and locale_kept and defaults and refused
	# The defaults restored, so the buses are left as the game starts them.
	missing.apply()
	print("settings_test: chosen %s, on disk %s, read back %s with speed %dx, the music muted and the master and effects at 0.75 and 0.5 unmuted %s: %s; F2's Korean kept %s; a missing file reads the defaults %s; a resolution out of range refused, the speed beside it read %s: %s" % [
		chosen, on_disk, read_back, reread.speed(), volumes_applied, kept, locale_kept, defaults, refused,
		"pass" if passed else "FAIL"])
	for each: Control in [panel, reread, noted, missing, ranged]:
		each.free()
	return passed and _fit_pass()


# In each locale: the title clear of the close key in the title bar; each row's label clear of its
# choices, and each choice within its slip; what each row sets, and the keys along the foot, within
# the panel. Setup: every SETTINGS_ and LOCALE_ key is measured.
func _fit_pass() -> bool:
	var fit := WordFit.new()
	var dialog := Settings.DIALOG
	var panel := _panel(MISSING)
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var title := fit.say("SETTINGS_TITLE")
		var back := fit.say("SETTINGS_BACK")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(back) + 8, dialog.size.x)
		for key: String in ["SETTINGS_SPEED_TIMES", "SETTINGS_WINDOWED", "SETTINGS_FULLSCREEN", "LOCALE_KO", "LOCALE_EN"]:
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
	panel.locale_key = OS.get_keycode_string(Hud.LOCALE_KEY)
	return panel


func _remove(path: String) -> void:
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(ProjectSettings.globalize_path(path))


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
