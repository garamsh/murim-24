# Checks that the pause panel's menu takes the entry chosen, with Escape resuming, and that every
# word it draws from hud.csv, whether a save was written among them, the controls guide as play.tscn binds it, and a seed as long as the
# new-game field takes fit the room the panel gives them, in each locale.
extends RefCounted

const Pause := preload("res://hud/pause.gd")
const NewGame := preload("res://hud/new_game.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const PLAY := "res://play/play.tscn"


func run() -> bool:
	var play: Node2D = load(PLAY).instantiate()
	play.notification(Node.NOTIFICATION_READY)
	var controls: Array[Dictionary] = play.controls()
	var panel: Control = Pause.new()
	panel.open(controls, 4817203365)
	var opened_on: String = Pause.ENTRIES[panel.chosen]
	var steps: Array[String] = []
	for n in Pause.ENTRIES.size():
		panel.key(_key(KEY_DOWN))
		steps.append(Pause.ENTRIES[panel.chosen])
	var taken: String = panel.key(_key(KEY_ENTER))
	var escape: String = panel.key(_key(KEY_ESCAPE))
	var passed := opened_on == "resume" and steps == ["save", "settings", "title", "quit", "quit"] \
			and taken == "quit" and escape == "resume"
	print("pause_test: opened on %s; down steps %s; Enter takes %s; Escape %s: %s" % [
		opened_on, steps, taken, escape, "pass" if passed else "FAIL"])
	panel.free()
	var fits := _fit_pass(controls)
	play.free()
	return passed and fits


# In each locale: the title clear of the close key in the title bar; each menu entry, and whether a
# save was written, within the menu; the seed's label clear of a seed of the new-game field's length; each control's caps within
# the room left of what it does, and what it does within the panel; every row of the guide above
# the foot, and the keys along the foot within the panel. Setup: every PAUSE_ and CONTROLS_ key is
# measured, and the guide has rows.
func _fit_pass(controls: Array[Dictionary]) -> bool:
	var fit := WordFit.new()
	var dialog := Pause.DIALOG
	var panel: Control = Pause.new()
	panel.use_font(fit.font)
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var title := fit.say("PAUSE_TITLE")
		var close := fit.say("PAUSE_CLOSE")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(close) + 8, dialog.size.x)
		for entry: String in Pause.ENTRIES:
			var text := fit.say(Pause.ENTRY_KEYS[entry])
			fit.check(text, 12 + fit.width(text), Pause.MENU_WIDTH - 4)
		for key: String in ["PAUSE_SAVED", "PAUSE_NOT_SAVED"]:
			var notice := fit.say(key)
			fit.check(notice, 2 + fit.width(notice), Pause.MENU_WIDTH - 4)
		var label := fit.say("PAUSE_SEED")
		var longest := "9".repeat(NewGame.MAX_DIGITS)
		fit.check(label, 2 + fit.width(label) + 4 + fit.width(longest), Pause.MENU_WIDTH - 4)
		var heading := fit.say("CONTROLS_TITLE")
		fit.check(heading, Pause.GUIDE_X + fit.width(heading), dialog.end.x - 10)
		for control: Dictionary in controls:
			var caps := -2.0
			for cap: String in control.keys:
				caps += panel.keycap_width(fit.say(cap) if cap.begins_with("CONTROLS_") else cap) + 2
			var does := fit.say(control.does)
			fit.check(str(control.keys), caps, Pause.GUIDE_TEXT_X - 2)
			fit.check(does, Pause.GUIDE_X + Pause.GUIDE_TEXT_X + fit.width(does), dialog.end.x - 10)
		var rows_end := 44 + controls.size() * Pause.GUIDE_STEP
		fit.check("%d rows" % controls.size(), rows_end, dialog.size.y - 20)
		var keys := fit.say("PAUSE_KEYS")
		fit.check(keys, 8 + fit.width(keys), dialog.size.x - 8)
	TranslationServer.set_locale(Hud.LOCALES[0])
	panel.free()
	var unmeasured := fit.unmeasured(["PAUSE_", "CONTROLS_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty() \
			and not controls.is_empty()
	print("pause_test: %d keys and %d controls measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), controls.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated,
		fit.overruns, "pass" if passed else "FAIL"])
	return passed


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
