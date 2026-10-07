# Checks that the sect panel shows what the adapter reads: after the region has lived long enough
# for its sects to admit members and raise elders, the panel opened on a member's army shows that
# person's sect with its name, faction, master, elders, members, cap and seat as the adapter gives
# them, the master's card first and then elders', and its faction's leader, sects and whether its
# lead passes by duel or the faction elects it; Tab shows the next faction's column and sect.
# Control: the Orthodox carry the election tag and no duel tag, the Cult the duel tag and no
# election tag, and the Unorthodox, who do neither, carry neither; a world from create() opens on
# no sect. The verdict on an application reads admitted, then already belonging, where a player
# lacking proof reads unproven. Every word the panel draws from hud.csv, every verdict among them,
# and every name the world's data can make, fits the room the panel gives it, in each locale.
extends RefCounted

const SEED := 2026
const SHORT_DAY := 5
# Years for the sects to admit members: by forty, each has elders besides its master.
const YEARS := 40
const DAYS_PER_YEAR := 120
const Sect := preload("res://hud/sect.gd")
const Hud := preload("res://hud/hud.gd")
const PersonName := preload("res://hud/person_name.gd")
# Counts are measured at two digits, ages at two: a sect's cap and people's ages stay under 100.
const WIDEST_NUMBER := 99
# In the region of SEED, the player born into village 1, of the Cult, may join the Heavenly Demon,
# and born into village 2, of the Unorthodox, has not the attainment the Black Fang asks.
const CULT_VILLAGE := 1
const UNORTHODOX_VILLAGE := 2
const BLAST := {"range": 768, "radius": 512, "body_damage": 2, "structure_damage": 9, "cooldown": 64}


func run() -> bool:
	var fits := _fit_pass()
	TranslationServer.set_locale("ko")
	var world := WorldAdapter.create_region(SEED, 0, SHORT_DAY)
	for i in YEARS * DAYS_PER_YEAR * SHORT_DAY:
		world.tick()
	var army := -1
	for candidate in world.army_count():
		var roster := world.roster(candidate)
		if army < 0 and not roster.is_empty() and world.person_sect(roster[0]) >= 0:
			army = candidate
	# Setup: someone leads an army from within a sect, whose elders are more than its master.
	var person: int = world.roster(army)[0] if army >= 0 else -1
	var sect := world.person_sect(person) if person >= 0 else -1
	if sect < 0 or world.sect_elders(sect).size() < 2:
		print("sect_test: no army led by a member of a sect with elders after %d years: FAIL" % YEARS)
		return false
	var panel: Control = Sect.new()
	panel.show_sect(world, army)
	var master := world.sect_master(sect)
	var faction := world.sect_faction(sect)
	var details_right: bool = panel.sect == sect and panel.own_sect == sect and panel.faction == faction \
			and panel.details.name == tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_")) \
			and panel.details.faction == tr("FACTION_" + world.faction_names()[faction].to_upper()) \
			and panel.details.master == PersonName.of(world, master) \
			and panel.details.elders == tr("SECT_COUNT") % world.sect_elders(sect).size() \
			and panel.details.members == world.sect_members(sect).size() \
			and panel.details.cap == world.sect_cap(sect) \
			and panel.details.seat == ("held" if world.sect_seat_held(sect) else "lost")
	var shown: Array = panel.cards.map(func(card: Dictionary) -> int: return card.person)
	var elders: Array = Array(world.sect_elders(sect)).filter(func(elder: int) -> bool: return elder != master)
	var cards_right: bool = shown == [master] + elders.slice(0, Sect.MAX_ELDER_CARDS) \
			and panel.cards[0].master and panel.cards.slice(1).all(func(card: Dictionary) -> bool: return not card.master)
	var leader := world.leader(faction)
	var listed: Array = panel.column.sects.map(func(row: Dictionary) -> int: return row.sect)
	var of_faction: Array = range(world.sect_count()).filter(func(index: int) -> bool: return world.sect_faction(index) == faction)
	var column_right: bool = panel.column.leader == PersonName.of(world, leader, true) \
			and listed == of_faction and panel.column.sects[0].count == "%d/%d" % [
				world.sect_members(sect).size(), world.sect_cap(sect)]

	# Tab shows each faction in turn; the election tag stands where the faction elects its leader,
	# the duel tag where the lead passes by duel.
	var duels := {}
	var elections := {}
	var tabbed := true
	for step in world.faction_names().size():
		duels[world.faction_names()[panel.faction]] = panel.column.duel
		elections[world.faction_names()[panel.faction]] = panel.column.elects
		panel.next_faction()
		tabbed = tabbed and panel.faction == (faction + step + 1) % world.faction_names().size() \
				and (panel.sect < 0 or world.sect_faction(panel.sect) == panel.faction)
	var duel_right: bool = duels == {"Orthodox": false, "Demonic": true, "Unorthodox": false} \
			and elections == {"Orthodox": true, "Demonic": false, "Unorthodox": false}

	# Control: a world from create() has no sects, and the panel opens on none.
	var bare := WorldAdapter.create(42, PackedStringArray(["...."]), [{"objective": Vector2i(3, 0), "faction": 0,
			"skills": [BLAST], "members": [{"cell": Vector2i(0, 0), "health": 10}]}], [], {"separation": 256,
			"hostile": [[false]], "stage_floors": [75, 25, 1]})
	if bare == null:
		print("sect_test: the world without sects was refused: FAIL")
		return false
	var empty: Control = Sect.new()
	empty.show_sect(bare, 0)
	var control: bool = bare.sect_count() == 0 and empty.sect == -1 and empty.cards.is_empty()
	empty.free()

	var passed := details_right and cards_right and column_right and tabbed and duel_right and control
	print("sect_test: after %d years army %d's lead %d in sect %d; details %s; cards %s against master %d and elders %s; column %s; Tab through each faction %s, duel tags %s, election tags %s; a world without sects opens on none %s: %s" % [
		YEARS, army, person, sect, "match" if details_right else str(panel.details), shown, master, elders,
		"match" if column_right else str(panel.column), tabbed, duels, elections, control, "pass" if passed else "FAIL"])
	panel.free()
	return passed and fits and _verdict_pass()


# Born into a Cult village of the region, whose sect admits the player, the panel shows the core's
# verdict on an application: the player joins, the verdict reads admitted, and the sect it shows
# carries the title's tag as the player's own; applying again reads that it already belongs.
# Control: born into an Unorthodox village, whose sect takes proof the player lacks, the verdict
# reads unproven and the player stays out. Moving to another faction clears the verdict.
func _verdict_pass() -> bool:
	var world := WorldAdapter.create_region(SEED, CULT_VILLAGE)
	var army := world.army_count() - 1
	var person := world.roster(army)[0]
	var panel: Control = Sect.new()
	panel.show_sect(world, army)
	var sect: int = panel.sect
	var before: int = panel.own_sect
	var refusal := world.application_refusal(person, sect)
	var joined := refusal == WorldAdapter.APPLICATION_NONE and world.apply(person, sect)
	panel.show_verdict(refusal)
	var admitted: bool = joined and panel.verdict == "APPLICATION_NONE" and panel.own_sect == sect \
			and world.person_sect(person) == sect and before == -1
	panel.show_verdict(world.application_refusal(person, sect))
	var again: bool = panel.verdict == "APPLICATION_IN_A_SECT"
	panel.next_faction()
	var cleared: bool = panel.verdict == ""
	panel.free()

	var other := WorldAdapter.create_region(SEED, UNORTHODOX_VILLAGE)
	var other_army := other.army_count() - 1
	var other_person := other.roster(other_army)[0]
	var refused_panel: Control = Sect.new()
	refused_panel.show_sect(other, other_army)
	refused_panel.show_verdict(other.application_refusal(other_person, refused_panel.sect))
	var refused: bool = refused_panel.verdict == "APPLICATION_UNPROVEN" and refused_panel.own_sect == -1 \
			and other.person_sect(other_person) == -1
	refused_panel.free()
	var passed := admitted and again and cleared and refused
	print("sect_test: born in village %d, the player applies to sect %d: admitted %s, again %s, cleared on Tab %s; born in village %d, refused as unproven %s: %s" % [
		CULT_VILLAGE, sect, admitted, again, cleared, UNORTHODOX_VILLAGE, refused, "pass" if passed else "FAIL"])
	return passed


# In each locale, every word the panel draws, in the HUD's font at the panel's size, fits where
# _draw places it: the title bar's words and tag clear of the close key, each faction tab's word
# within its tab, the leader's label clear of the election and duel tags, the seat's label clear
# of its state's tag, each figure's label clear of its value, the sect list's names clear of their
# counts and states, the master's tag over its card, and the keys along the foot within the panel. Every
# surname crossed with every given name in names() is measured as a card, the master's figure and
# the leader show it, as the panel fits it, and in Korean drawn whole. Setup: every SECT_ key in
# hud.csv is measured, every sect the region holds has its name in hud.csv, and the widths are
# not zero.
func _fit_pass() -> bool:
	var font: Font = load(Hud.FONT)
	var region := WorldAdapter.create_region(SEED, 0)
	var table := region.names()
	var names: Array[Dictionary] = []
	for surname: Dictionary in table.surnames:
		for given: Dictionary in table.female + table.male:
			names.append({"surname": surname.korean, "given": given.korean,
					"surname_en": surname.romanised, "given_en": given.romanised})
	var panel: Control = Sect.new()
	panel.use_font(font)
	var measured := {}
	var untranslated: Array[String] = []
	var cut := {}
	var overruns: Array[String] = []
	var checks := 0
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var width := func(text: String) -> float:
			return font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, Sect.FONT_SIZE).x
		var say := func(key: String) -> String:
			measured[key] = true
			if tr(key) == key:
				untranslated.append("%s %s" % [locale, key])
			return tr(key)
		var sects: Array[String] = []
		for index in region.sect_count():
			sects.append(say.call("SECT_" + region.sect_name(index).to_upper().replace(" ", "_")))
		var factions: Array[String] = []
		var tabs: Array[String] = []
		for faction: String in region.faction_names():
			factions.append(say.call("FACTION_" + faction.to_upper()))
			tabs.append(say.call("SECT_TAB_" + faction.to_upper()))
		var states: Array[String] = [say.call("SECT_HELD"), say.call("SECT_LOST"), say.call("SECT_FALLEN")]
		var count := "%d/%d" % [WIDEST_NUMBER, WIDEST_NUMBER]
		# Each check: what is measured, how far it reaches and the room it has.
		var fits := []
		var close: float = width.call(say.call("SECT_CLOSE"))
		var own: String = say.call("SECT_OWN")
		for sect in sects:
			for faction in factions:
				fits.append([[sect, faction], 8 + width.call(say.call("SECT_TITLE")) + 10 + width.call(sect) + 8
						+ width.call(faction) + 10 + width.call(own) + 6, Sect.PANEL.size.x - 8 - close - 10])
		for tab in tabs:
			fits.append([tab, width.call(tab) + 2 * Sect.TAB_PAD, Sect.tab_width(tabs.size())])
		for succession: String in [say.call("HUD_DUEL"), say.call("SECT_ELECTED")]:
			fits.append([succession, 8 + width.call(say.call("HUD_LEADER")) + 4 + width.call(succession) + 6,
					Sect.COLUMN.size.x - 8])
		for state in states:
			fits.append([state, 8 + width.call(say.call("SECT_SEAT")) + 4 + width.call(state) + 6, Sect.SEAT.size.x - 8])
		var rows := [["SECT_ELDERS", say.call("SECT_COUNT") % WIDEST_NUMBER],
				["SECT_MEMBERS", "%d / %d" % [WIDEST_NUMBER, WIDEST_NUMBER]], ["SECT_MASTER", say.call("HUD_LEADER_NONE")]]
		for faction in factions:
			rows.append(["SECT_FACTION", faction])
		for row: Array in rows:
			# The label and its value 4 pixels apart at least.
			fits.append([row, 2 + width.call(say.call(row[0])) + 4 + width.call(row[1]), Sect.FIGURES.size.x - 2])
		for sect in sects:
			for right in states.slice(1) + [count]:
				var drawn: String = panel.fit(sect, Sect.COLUMN.size.x - 18 - width.call(right) - 4)
				fits.append([drawn, 10 + width.call(drawn) + 4 + width.call(right), Sect.COLUMN.size.x - 8])
				if drawn != sect:
					cut[locale] = cut.get(locale, 0) + 1
		var tag: String = say.call("SECT_MASTER")
		fits.append([tag, width.call(tag) + 6 + 3, Sect.CARD.x])
		var line: String = say.call("GENEALOGY_AGE") % WIDEST_NUMBER
		fits.append([line, Sect.CARD_TEXT_X + width.call(line), Sect.CARD.x - 2])
		fits.append([say.call("SECT_RANKS"), width.call(say.call("SECT_RANKS")), Sect.FIGURES.end.x - Sect.SEAT.position.x])
		fits.append([say.call("SECT_NONE"), 8 + width.call(say.call("SECT_NONE")), Sect.SEAT.size.x - 8])
		fits.append([say.call("SECT_TITLE"), 8 + width.call(say.call("SECT_TITLE")), Sect.COLUMN.size.x - 8])
		var master_word: float = width.call(say.call("SECT_MASTER"))
		for name in names:
			var places := [[PersonName.shown(name, true), Sect.CARD.x - 2 - Sect.CARD_TEXT_X],
					[PersonName.shown(name), Sect.FIGURES.size.x - 4 - master_word - 4],
					[PersonName.shown(name, true), Sect.COLUMN.size.x - Sect.LEADER_TEXT_X - 8]]
			for place: Array in places:
				var drawn: String = panel.fit(place[0], place[1])
				fits.append([drawn, width.call(drawn), place[1]])
				if drawn != place[0]:
					cut[locale] = cut.get(locale, 0) + 1
					if locale == "ko":
						overruns.append("ko %s drawn as %s" % [place[0], drawn])
		var keys: String = say.call("SECT_KEYS")
		fits.append([keys, 8 + width.call(keys), Sect.PANEL.size.x - 8])
		for verdict: String in Sect.APPLICATION_KEYS.values():
			var said: String = say.call(verdict)
			fits.append([said, width.call(said), Sect.FIGURES.end.x - Sect.SEAT.position.x])
		for fit: Array in fits:
			checks += 1
			if fit[1] > fit[2] or width.call(str(fit[0])) <= 0.0:
				overruns.append("%s %s reaches %d past %d" % [locale, fit[0], fit[1], fit[2]])
	TranslationServer.set_locale(Hud.LOCALES[0])
	panel.free()
	var unmeasured: Array[String] = []
	var csv := FileAccess.open("res://hud/hud.csv", FileAccess.READ)
	csv.get_csv_line()
	while not csv.eof_reached():
		var key := csv.get_csv_line()[0]
		if (key.begins_with("SECT_") or key.begins_with("APPLICATION_")) and not measured.has(key):
			unmeasured.append(key)
	var passed := unmeasured.is_empty() and untranslated.is_empty() and overruns.is_empty() \
			and names.size() > 0 and region.sect_count() > 0
	print("sect_test: %d keys, %d sects and %d names measured in %d checks across %s, cut short %s; unmeasured %s; untranslated %s; %d overruns, the first %s: %s" % [
		measured.size(), region.sect_count(), names.size(), checks, Hud.LOCALES, cut, unmeasured, untranslated,
		overruns.size(), overruns.slice(0, 6), "pass" if passed else "FAIL"])
	return passed
