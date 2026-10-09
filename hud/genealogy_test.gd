# Checks that the genealogy panel shows what the adapter reads: after the region has lived long
# enough for families to form, the panel on a parent shows that person's name, spouse and
# children, their family name, age and sect, and talent and lifespan as unknown; moving down and
# pressing Enter centres it on a child, as keypad Enter does through its keys, whose parents then
# include that parent, and whose generation in the header is one more than its parents'. Control: a
# person the world began with has no parents, the panel shows none, and its header reads the first
# generation. A person in a sect shows its sect's name, and a sectless person of the same faction
# shows none. Every word the panel draws from hud.csv, and every name and sect name the world's data
# can make, fits the room the panel gives it, in each locale.
extends RefCounted

const SEED := 2026
# Days for marriages and births to come: with births slowed by each village's headroom, the first
# comes within sixty.
const DAYS := 70
const TICKS_PER_DAY := 1800
const Genealogy := preload("res://hud/genealogy.gd")
const Hud := preload("res://hud/hud.gd")
const PersonName := preload("res://hud/person_name.gd")
# Ages, generations and counts are measured at two digits: people live 45 to 75 years.
const WIDEST_NUMBER := 99


func run() -> bool:
	var fits := _fit_pass()
	TranslationServer.set_locale("ko")
	var world := WorldAdapter.create_region(SEED, 0)
	for i in DAYS * TICKS_PER_DAY:
		world.tick()
	var parent := -1
	for army in world.army_count():
		for person in world.roster(army):
			if parent < 0 and world.person_spouse(person) >= 0 and world.person_children(person).size() > 0:
				parent = person
	# Setup: someone has married and had a child, so the panel has kin to show.
	if parent < 0:
		print("genealogy_test: no one has a child after %d days: FAIL" % DAYS)
		return false
	var panel: Control = preload("res://hud/genealogy.gd").new()
	panel.show_person(world, parent, parent)
	var name := world.person_name(parent)
	var spouse := world.person_spouse(parent)
	var children := world.person_children(parent)
	var shown := {}
	for card: Dictionary in panel.cards:
		if not shown.has(card.role):
			shown[card.role] = []
		shown[card.role].append(card.person)
	var centre_card: Dictionary = panel.cards.filter(func(card: Dictionary) -> bool: return card.role == "centre")[0]
	var kin_right: bool = centre_card.name == name.surname + name.given \
			and shown.get("spouse", []) == [spouse] \
			and shown.get("child", []) == Array(children.slice(0, 3)) \
			and shown.get("parent", []) == Array(world.person_parents(parent).slice(0, 2))
	var details_right: bool = panel.details.family == "%s씨 가문" % name.surname \
			and panel.details.age == str(world.person_age(parent)) \
			and panel.details.talent == "알 수 없음" and panel.details.lifespan == "알 수 없음" \
			and panel.details.children == "%d명" % children.size() \
			and panel.details.sect == _sect_shown(world, parent)

	var moved: bool = panel.move(Vector2i.DOWN)
	var child: int = panel.selected
	panel.centre_selected()
	var child_generation: String = panel.details.generation
	var recentred: bool = moved and children.has(child) and panel.centre == child \
			and panel.cards.any(func(card: Dictionary) -> bool: return card.role == "parent" and card.person == parent)

	# Keypad Enter, taken as the play scene passes keys, centres the tree as Enter does.
	panel.show_person(world, parent, parent)
	var keyed: Array[String] = [panel.key(_key(KEY_DOWN)), panel.key(_key(KEY_KP_ENTER))]
	var keypad_centred: bool = keyed == ["done", "done"] and panel.centre == child
	recentred = recentred and keypad_centred

	# Control: the person the world began with, steered, has no parents to show.
	var founder: int = world.roster(world.army_count() - 1)[0]
	panel.show_person(world, founder, founder)
	var founder_parents: int = panel.cards.filter(func(card: Dictionary) -> bool: return card.role == "parent").size()
	var control := world.person_parents(founder).is_empty() and founder_parents == 0
	# The header's generation: the founder's first, the child's one more than its parents'.
	var founder_generation: String = panel.details.generation
	# Setup: seventy days in, the child's parents are people the world began with, so it is the
	# second generation.
	var parents_founders := world.person_parents(child).size() > 0
	for p in world.person_parents(child):
		parents_founders = parents_founders and world.person_parents(p).is_empty()
	var generations_right: bool = founder_generation == "1대" and parents_founders \
			and child_generation == "2대"
	# A sect member and a sectless person of its faction: the line reads the sect, not the faction.
	var member := -1
	var sectless := -1
	for army in world.army_count():
		for person in world.roster(army):
			if member < 0 and world.person_sect(person) >= 0:
				member = person
	for army in world.army_count():
		for person in world.roster(army):
			if sectless < 0 and member >= 0 and world.person_sect(person) < 0 \
					and world.person_faction(person) == world.person_faction(member):
				sectless = person
	var sect_set_up := member >= 0 and sectless >= 0
	var member_sect := ""
	var sectless_sect := ""
	if sect_set_up:
		panel.show_person(world, member, member)
		member_sect = panel.details.sect
		panel.show_person(world, sectless, sectless)
		sectless_sect = panel.details.sect
	var sect_right: bool = sect_set_up and member_sect == _sect_shown(world, member) \
			and member_sect != tr("GENEALOGY_NONE") and sectless_sect == tr("GENEALOGY_NONE")
	var passed := kin_right and details_right and recentred and control and generations_right and sect_right
	print("genealogy_test: after %d days person %d %s%s, spouse %d, children %s; cards %s; details %s; down and Enter centre on child %d %s, keypad Enter %s; the founder %d shows %d parents; generations: founder %s, child %s; person %d of faction %d shows the sect \"%s\" and sectless person %d of it \"%s\" %s (setup %s): %s" % [
		DAYS, parent, name.surname, name.given, spouse, children, shown, panel.details if not details_right else "match",
		child, recentred, keypad_centred, founder, founder_parents, founder_generation, child_generation, member,
		world.person_faction(member) if member >= 0 else -1, member_sect, sectless, sectless_sect, sect_right,
		sect_set_up, "pass" if passed else "FAIL"])
	panel.free()
	return passed and fits


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


# The sect line `person`'s details show: its sect's name, or none.
func _sect_shown(world: WorldAdapter, person: int) -> String:
	var sect := world.person_sect(person)
	return tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_")) if sect >= 0 else tr("GENEALOGY_NONE")


# In each locale, every word the panel draws, in the HUD's font at the panel's size, fits where
# _draw places it: the header clear of the close key, the row labels clear of the cards, a card's
# line and the steered person's tag within the card, the details within their column, each detail
# row's label clear of its value, and the keys along the foot within the panel. Every surname
# crossed with every given name in names() is measured as a card, the details column and a spouse
# show it, and every sect's name as the details column shows it, as the panel fits them: cut short
# with an ellipsis where they would overrun, and in Korean drawn whole. Setup: every GENEALOGY_ key
# in hud.csv is measured, and the widths are not zero.
func _fit_pass() -> bool:
	var font: Font = load(Hud.FONT)
	var region := WorldAdapter.create_region(SEED, 0)
	var table := region.names()
	var names: Array[Dictionary] = []
	for surname: Dictionary in table.surnames:
		for given: Dictionary in table.female + table.male:
			names.append({"surname": surname.korean, "given": given.korean,
					"surname_en": surname.romanised, "given_en": given.romanised})
	var panel: Control = Genealogy.new()
	panel.use_font(font)
	var cut := {}
	var measured := {}
	var overruns: Array[String] = []
	var checks := 0
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var width := func(text: String) -> float:
			return font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, Genealogy.FONT_SIZE).x
		var say := func(key: String) -> String:
			measured[key] = true
			return tr(key)
		var families: Array[String] = [say.call("GENEALOGY_FAMILY") % say.call("GENEALOGY_UNNAMED")]
		for surname: Dictionary in table.surnames:
			families.append(say.call("GENEALOGY_FAMILY") % (surname.korean if locale == "ko" else surname.romanised))
		var sects: Array[String] = [say.call("GENEALOGY_NONE")]
		for index in region.sect_count():
			sects.append(tr("SECT_" + region.sect_name(index).to_upper().replace(" ", "_")))
		# Each check: what is measured, how far it reaches and the room it has.
		var fits := []
		var close: float = width.call(say.call("GENEALOGY_CLOSE"))
		var generation: String = say.call("GENEALOGY_GENERATION") % WIDEST_NUMBER
		for family in families:
			fits.append([family, 8 + width.call(say.call("GENEALOGY_TITLE")) + 10 + width.call(family) + 14
					+ width.call(generation), Genealogy.PANEL.size.x - 8 - close - 10])
		for key: String in ["GENEALOGY_PARENTS", "GENEALOGY_ROW_SELF", "GENEALOGY_CHILDREN"]:
			fits.append([key, Genealogy.PANEL.position.x + 10 + width.call(say.call(key)), Genealogy.LEFT])
		for line: String in [say.call("GENEALOGY_DEAD"), say.call("GENEALOGY_AGE") % WIDEST_NUMBER]:
			# Within the card's 2-pixel frame.
			fits.append([line, Genealogy.CARD_TEXT_X + width.call(line), Genealogy.CARD.x - 2])
		var tag: String = say.call("GENEALOGY_SELF")
		fits.append([tag, width.call(tag) + 6 + 3, Genealogy.CARD.x])
		for text in families:
			fits.append([text, Genealogy.DETAILS_TEXT_X + width.call(text), Genealogy.DETAILS.size.x - 8])
		var unknown: String = say.call("GENEALOGY_UNKNOWN")
		var rows := [
			["GENEALOGY_AGE_LABEL", str(WIDEST_NUMBER)], ["GENEALOGY_TALENT", unknown],
			["GENEALOGY_LIFESPAN", unknown], ["GENEALOGY_SPOUSE", say.call("GENEALOGY_NONE")],
			["GENEALOGY_CHILDREN", say.call("GENEALOGY_COUNT") % WIDEST_NUMBER],
		]
		for row: Array in rows:
			# The label and its value 4 pixels apart at least.
			fits.append([row, 8 + width.call(say.call(row[0])) + 4 + width.call(row[1]), Genealogy.DETAILS.size.x - 8])
		var spouse_word: float = width.call(say.call("GENEALOGY_SPOUSE"))
		var detail_room: float = Genealogy.DETAILS.size.x - 8 - Genealogy.DETAILS_TEXT_X
		var places: Array = []
		for sect in sects:
			places.append([sect, detail_room])
		for name in names:
			var card_room: float = Genealogy.CARD.x - 2 - Genealogy.CARD_TEXT_X
			var spouse_room: float = Genealogy.DETAILS.size.x - 8 - 8 - spouse_word - 4
			places.append_array([[PersonName.shown(name, true), card_room], [PersonName.shown(name), detail_room],
					[PersonName.shown(name), spouse_room]])
		for place: Array in places:
			var drawn: String = panel.fit(place[0], place[1])
			fits.append([drawn, width.call(drawn), place[1]])
			if drawn != place[0]:
				cut[locale] = cut.get(locale, 0) + 1
				if locale == "ko":
					overruns.append("ko %s drawn as %s" % [place[0], drawn])
		var keys: String = say.call("GENEALOGY_KEYS")
		fits.append([keys, 8 + width.call(keys), Genealogy.PANEL.size.x - 8])
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
		if key.begins_with("GENEALOGY_") and not measured.has(key):
			unmeasured.append(key)
	var passed := unmeasured.is_empty() and overruns.is_empty() and names.size() > 0
	print("genealogy_test: %d keys and %d names measured in %d checks across %s, names cut short %s; unmeasured %s; %d overruns, the first %s: %s" % [
		measured.size(), names.size(), checks, Hud.LOCALES, cut, unmeasured, overruns.size(), overruns.slice(0, 6),
		"pass" if passed else "FAIL"])
	return passed
