# Checks that the rebirth screen sums up the life it is given as the adapter reads it: opened on a
# sect's founding master, whose life is taken as ended, and on the player as the life begun, the
# ended side shows the master's family, years, sect, art and children, and the begun side the
# player's family, faction and age. Control: opened the other way round, the ended side reads the
# player, who belongs to no sect and has attained nothing, so each row follows the person given.
# Enter and Escape go on, and any other key does nothing. Every word the screen draws from hud.csv,
# with every surname, sect, faction and discipline the world's data can make, fits the room the
# screen gives it, in each locale.
extends RefCounted

const Rebirth := preload("res://hud/rebirth.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const Attainment := preload("res://hud/attainment.gd")
const SEED := 2026
# Years, children and ages are measured at two digits, and attainment at three: people live under
# 100 years and a talent limit stays under 1,000.
const WIDEST_COUNT := 99
const WIDEST_ATTAINMENT := 999


func run() -> bool:
	TranslationServer.set_locale("ko")
	var world := WorldAdapter.create_region(SEED)
	var player := world.roster(world.army_count() - 1)[0]
	var master := world.sect_master(0)
	var screen: Control = Rebirth.new()
	screen.open(world, master, player)
	var sect := world.person_sect(master)
	var past: Array = screen.past_rows.map(func(row: Dictionary) -> String: return row.value)
	var expected_past := [
		"%s씨 가문" % world.person_name(master).surname,
		"%d세" % world.person_age(master),
		screen.tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_")),
		Attainment.of(world, master),
		"%d명" % world.person_children(master).size(),
	]
	var next: Array = screen.next_rows.map(func(row: Dictionary) -> String: return row.value)
	var expected_next := [
		"%s씨 가문" % world.person_name(player).surname,
		screen.tr("FACTION_" + world.faction_names()[world.person_faction(player)].to_upper()),
		"%d세" % world.person_age(player),
	]
	# Setup: the master belongs to a sect and has attained something, where the player has neither.
	var set_up := sect >= 0 and Attainment.of(world, master) != Attainment.NONE \
			and world.person_sect(player) < 0 and Attainment.of(world, player) == Attainment.NONE
	var read_right: bool = screen.past == master and screen.next == player and past == expected_past \
			and next == expected_next
	screen.open(world, player, master)
	var swapped: Array = screen.past_rows.map(func(row: Dictionary) -> String: return row.value)
	var control: bool = swapped[2] == screen.tr("REBIRTH_NONE") and swapped[3] == Attainment.NONE \
			and swapped[0] == "%s씨 가문" % world.person_name(player).surname
	var keys := [screen.key(_key(KEY_ENTER)), screen.key(_key(KEY_ESCAPE)), screen.key(_key(KEY_1))]
	var keys_right := keys == ["continue", "continue", ""]
	screen.free()
	var passed := set_up and read_right and control and keys_right
	print("rebirth_test: ended %s against %s, begun %s against %s (setup %s); swapped %s; Enter, Escape and 1 give %s: %s" % [
		past, expected_past, next, expected_next, set_up, swapped, keys, "pass" if passed else "FAIL"])
	return passed and _fit_pass(world)


# In each locale: the title clear of the close key; each side's heading within its column; each
# row's label and its widest value 4 pixels apart within the column, the value its row can show
# being every surname's family, every sect, faction and discipline, none, and the widest numbers;
# the keys along the foot within the dialog. Setup: every REBIRTH_ key is measured.
func _fit_pass(world: WorldAdapter) -> bool:
	var fit := WordFit.new()
	var dialog := Rebirth.DIALOG
	var column := Rebirth.COLUMN_WIDTH
	var names := world.names()
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var title := fit.say("REBIRTH_TITLE")
		var close := fit.say("REBIRTH_CLOSE")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(close) + 8, dialog.size.x)
		for heading: String in ["REBIRTH_PAST", "REBIRTH_NEXT"]:
			var text := fit.say(heading)
			fit.check(text, fit.width(text), column)
		var none := fit.say("REBIRTH_NONE")
		var families: Array[String] = []
		for surname: Dictionary in names.surnames:
			families.append(fit.say("GENEALOGY_FAMILY") % (surname.korean if locale == "ko" else surname.romanised))
		var sects: Array[String] = [none]
		for sect in world.sect_count():
			sects.append(fit.say("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_")))
		var factions: Array[String] = [none]
		for faction: String in world.faction_names():
			factions.append(fit.say("FACTION_" + faction.to_upper()))
		var arts: Array[String] = [Attainment.NONE]
		for discipline: String in world.discipline_names():
			arts.append(Attainment.shown(discipline, WIDEST_ATTAINMENT))
		var rows := {
			"REBIRTH_FAMILY": families,
			"REBIRTH_YEARS": [fit.say("REBIRTH_YEARS_VALUE") % WIDEST_COUNT],
			"REBIRTH_SECT": sects,
			"REBIRTH_ATTAINMENT": arts,
			"REBIRTH_CHILDREN": [fit.say("GENEALOGY_COUNT") % WIDEST_COUNT],
			"REBIRTH_FACTION": factions,
			"REBIRTH_AGE": [fit.say("GENEALOGY_AGE") % WIDEST_COUNT],
		}
		for label: String in rows:
			var said := fit.say(label)
			for value: String in rows[label]:
				fit.check("%s %s" % [said, value], 2 + fit.width(said) + 4 + fit.width(value) + 2, column)
		var keys := fit.say("REBIRTH_KEYS")
		fit.check(keys, 8 + fit.width(keys), dialog.size.x - 8)
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["REBIRTH_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("rebirth_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
