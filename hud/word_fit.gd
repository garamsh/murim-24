## Measures the words a screen over the HUD draws, for the screens' fit tests: each in the HUD's
## font at its size, each hud.csv key read noted as measured, and each check of how far a word
## reaches against the room the screen gives it.
extends RefCounted

const Hud := preload("res://hud/hud.gd")

var font: Font = load(Hud.FONT)
## The hud.csv keys read, the keys a locale lacks, the checks made and those that overran.
var measured := {}
var untranslated: Array[String] = []
var checks := 0
var overruns: Array[String] = []


## Returns `key` in the current locale, noting it measured, and untranslated where the locale
## lacks it.
func say(key: String) -> String:
	measured[key] = true
	if tr(key) == key:
		untranslated.append("%s %s" % [TranslationServer.get_locale(), key])
	return tr(key)


func width(text: String) -> float:
	return font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, Hud.FONT_SIZE).x


## Checks that `what`, drawn so that it reaches `reach` pixels, keeps within `room`, and that it
## is drawn at all.
func check(what: String, reach: float, room: float) -> void:
	checks += 1
	if reach > room or width(what) <= 0.0:
		overruns.append("%s %s reaches %d past %d" % [TranslationServer.get_locale(), what, reach, room])


## Returns the hud.csv keys starting with one of `prefixes` that no check read.
func unmeasured(prefixes: Array) -> Array[String]:
	var keys: Array[String] = []
	var csv := FileAccess.open("res://hud/hud.csv", FileAccess.READ)
	csv.get_csv_line()
	while not csv.eof_reached():
		var key := csv.get_csv_line()[0]
		if prefixes.any(func(prefix: String) -> bool: return key.begins_with(prefix)) and not measured.has(key):
			keys.append(key)
	return keys
