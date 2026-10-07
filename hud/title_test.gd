# Checks that the title screen's menu passes over continue, which waits for saving, and takes the
# entry chosen: down from a new game lands on the settings and up from them on a new game, and
# Enter takes what is chosen. Control: the settings, an entry that does not wait, are reached and
# taken by the same keys. Every word the screen draws from hud.csv fits the room it is given, in
# each locale.
extends RefCounted

const Title := preload("res://hud/title.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const SCREEN := Vector2(480, 270)
# The keys along the foot keep 8 pixels from each side of the screen.
const FOOT_INSET := 8


func run() -> bool:
	var title: Control = Title.new()
	title.open()
	var opened_on_new: bool = Title.ENTRIES[title.chosen] == "new"
	title.key(_key(KEY_DOWN))
	var down: String = Title.ENTRIES[title.chosen]
	var taken: String = title.key(_key(KEY_ENTER))
	title.key(_key(KEY_UP))
	var up: String = Title.ENTRIES[title.chosen]
	title.key(_key(KEY_UP))
	var held_at_top: String = Title.ENTRIES[title.chosen]
	# Setup: continue lies between a new game and the settings, so a step over it skipped it.
	var between: bool = Title.ENTRIES.find("continue") == Title.ENTRIES.find("new") + 1 \
			and Title.ENTRIES.find("settings") == Title.ENTRIES.find("continue") + 1
	var passed := opened_on_new and between and down == "settings" and taken == "settings" and up == "new" \
			and held_at_top == "new"
	print("title_test: opened on %s; continue between true %s; down to %s, Enter takes %s; up to %s, up again stays on %s: %s" % [
		Title.ENTRIES[0], between, down, taken, up, held_at_top, "pass" if passed else "FAIL"])
	title.free()
	return passed and _fit_pass()


# In each locale: the line under the title within the screen's width less the title's 40-pixel
# margin on each side; each menu entry, with the note of one that waits right of it, within the
# menu; the keys along the foot within the screen. Setup: every TITLE_ key is measured.
func _fit_pass() -> bool:
	var fit := WordFit.new()
	var menu_width := Title.MENU.size.x - 2 * Title.MENU_INSET.x
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var line := fit.say("TITLE_LINE")
		fit.check(line, Title.LINE.x + fit.width(line), SCREEN.x - Title.LINE.x)
		var note := fit.say("TITLE_LATER")
		for entry: String in Title.ENTRIES:
			var text := fit.say(Title.ENTRY_KEYS[entry])
			var reach := 12 + fit.width(text) + (4 + fit.width(note) + 4 if Title.WAITING.has(entry) else 0.0)
			fit.check(text, reach, menu_width)
		var keys := fit.say("TITLE_KEYS")
		fit.check(keys, FOOT_INSET + fit.width(keys), SCREEN.x - FOOT_INSET)
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["TITLE_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("title_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
