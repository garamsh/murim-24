# Checks that the new-game panel takes the seed typed into its field: digits are typed, Backspace
# takes the last back, Enter asks to start from the typed seed, and a digit past MAX_DIGITS is
# refused. Control: the digit that fills the field is taken. R draws a seed into the field, an
# empty field draws one at Enter, and Escape goes back and closes the panel. Every word the panel
# draws from hud.csv, and a seed as long as the field takes, fits the room it is given, in each
# locale.
extends RefCounted

const NewGame := preload("res://hud/new_game.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
# Seeded with SKIP_DRAW, Godot's generator draws REFUSED_DRAW, a seed the core generates no region
# from, then KEPT_DRAW, one it does; seeded with PLAIN_DRAW, it draws PLAIN_KEPT first, one the core
# generates.
const SKIP_DRAW := 105
const REFUSED_DRAW := 418156736
const KEPT_DRAW := 2229168570
const PLAIN_DRAW := 2026
const PLAIN_KEPT := 4259094339


func run() -> bool:
	var panel: Control = NewGame.new()
	panel.open()
	for keycode: Key in [KEY_4, KEY_8, KEY_KP_1, KEY_7]:
		panel.key(_key(keycode))
	panel.key(_key(KEY_BACKSPACE))
	var typed: String = panel.typed
	var start: String = panel.key(_key(KEY_ENTER))
	var typed_seed: int = panel.world_seed()

	# A full field takes its last digit and refuses the next.
	panel.open()
	for n in NewGame.MAX_DIGITS - 1:
		panel.key(_key(KEY_9))
	panel.key(_key(KEY_1))
	var filled: String = panel.typed
	panel.key(_key(KEY_2))
	var full_kept: bool = panel.typed == filled and filled.length() == NewGame.MAX_DIGITS and filled.ends_with("1")

	panel.open()
	panel.key(_key(KEY_R))
	var drawn: String = panel.typed
	var drawn_ok := drawn.is_valid_int() and drawn.length() <= NewGame.MAX_DIGITS
	panel.open()
	var empty_seed: int = panel.world_seed()
	var back: String = panel.key(_key(KEY_ESCAPE))
	var passed := typed == "481" and start == "start" and typed_seed == 481 and full_kept and drawn_ok \
			and empty_seed >= 0 and back == "back" and not panel.visible
	print("new_game_test: typed 4, 8, keypad 1, 7 and Backspace as %s, Enter %s from seed %d; a full field kept %s with its last digit taken %s; R drew %s; an empty field drew %d; Escape %s, closed %s: %s" % [
		typed, start, typed_seed, filled, full_kept, drawn, empty_seed, back, not panel.visible,
		"pass" if passed else "FAIL"])
	panel.free()
	return passed and _fit_pass() and _skip_pass() and _refusal_pass()


# In each locale: the title clear of the close key in the title bar; the seed's label clear of its
# field, and a full field's digits and caret within the field; the random draw's and the start's
# words within their slips; both lines and the keys along the foot within the panel. Setup: every
# NEW_GAME_ key is measured.
func _fit_pass() -> bool:
	var fit := WordFit.new()
	var dialog := NewGame.DIALOG
	var field := NewGame.FIELD
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var title := fit.say("NEW_GAME_TITLE")
		var back := fit.say("NEW_GAME_BACK")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(back) + 8, dialog.size.x)
		var label := fit.say("NEW_GAME_SEED")
		fit.check(label, dialog.position.x + 12 + fit.width(label) + 4, field.position.x)
		var full := "9".repeat(NewGame.MAX_DIGITS)
		fit.check(full, 6 + fit.width(full) + 1, field.size.x - 1)
		var random := fit.say("NEW_GAME_RANDOM")
		fit.check(random, fit.width(random) + 4, dialog.end.x - 12 - NewGame.RANDOM_X)
		var start := fit.say("NEW_GAME_START")
		fit.check(start, fit.width(start) + 4, NewGame.START_WIDTH)
		for key: String in ["NEW_GAME_SAME", "NEW_GAME_REFUSED", "NEW_GAME_EMPTY", "NEW_GAME_KEYS"]:
			var text := fit.say(key)
			fit.check(text, 12 + fit.width(text), dialog.size.x - 12)
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["NEW_GAME_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("new_game_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


# The random slip, or an empty field at Enter, passes over a seed the core refuses: seeded with
# SKIP_DRAW, R draws KEPT_DRAW, not REFUSED_DRAW, and so does the empty field. Control: seeded
# with PLAIN_DRAW, whose first draw the core generates, R keeps that first draw. Setup: the core
# refuses REFUSED_DRAW and generates KEPT_DRAW and PLAIN_KEPT.
func _skip_pass() -> bool:
	var setup := WorldAdapter.region_refusal(REFUSED_DRAW) == WorldAdapter.REGION_REFUSED \
			and WorldAdapter.region_refusal(KEPT_DRAW) == WorldAdapter.REGION_NONE \
			and WorldAdapter.region_refusal(PLAIN_KEPT) == WorldAdapter.REGION_NONE
	var panel: Control = NewGame.new()
	panel.open()
	seed(SKIP_DRAW)
	panel.key(_key(KEY_R))
	var skipped: String = panel.typed
	panel.open()
	seed(SKIP_DRAW)
	var empty_skipped: int = panel.world_seed()
	panel.open()
	seed(PLAIN_DRAW)
	panel.key(_key(KEY_R))
	var plain: String = panel.typed
	var passed := setup and skipped == str(KEPT_DRAW) and empty_skipped == KEPT_DRAW and plain == str(PLAIN_KEPT)
	print("new_game_test: setup %s; seeded with %d, R drew %s past %d, an empty field %d; seeded with %d, R drew %s: %s" % [
		setup, SKIP_DRAW, skipped, REFUSED_DRAW, empty_skipped, PLAIN_DRAW, plain, "pass" if passed else "FAIL"])
	panel.free()
	return passed


# A refusal the core names shows the panel's notice, until a key changes the field or the panel
# opens again. Control: REGION_NONE shows none.
func _refusal_pass() -> bool:
	var panel: Control = NewGame.new()
	panel.open()
	panel.refuse(WorldAdapter.REGION_NONE)
	var none_shown: bool = panel.refused
	panel.refuse(WorldAdapter.REGION_REFUSED)
	var shown: bool = panel.refused
	panel.key(_key(KEY_4))
	var typed_cleared: bool = not panel.refused
	panel.refuse(WorldAdapter.REGION_REFUSED)
	panel.open()
	var reopened_cleared: bool = not panel.refused
	var passed := not none_shown and shown and typed_cleared and reopened_cleared
	print("new_game_test: REGION_NONE shows the notice %s; REGION_REFUSED shows it %s; a digit clears it %s; opening again clears it %s: %s" % [
		none_shown, shown, typed_cleared, reopened_cleared, "pass" if passed else "FAIL"])
	panel.free()
	return passed
