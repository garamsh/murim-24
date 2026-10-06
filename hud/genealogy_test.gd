# Checks that the genealogy panel shows what the adapter reads: after the region has lived long
# enough for families to form, the panel on a parent shows that person's name, spouse and
# children, their family name, age and sect, and talent and lifespan as unknown; moving down and
# pressing Enter centres it on a child, whose parents then include that parent, and whose
# generation in the header is one more than its parents'. Control: a person the world began with
# has no parents, the panel shows none, and its header reads the first generation. Every word the
# panel draws from hud.csv fits the room the panel gives it, in each locale.
extends RefCounted

const SEED := 2026
# Days for marriages and births to come: with births slowed by each village's headroom, the first
# comes within sixty.
const DAYS := 70
const TICKS_PER_DAY := 1800
const Genealogy := preload("res://hud/genealogy.gd")
const Hud := preload("res://hud/hud.gd")
# The widest surname the name data makes, by locale.
const WIDEST_SURNAME := {"ko": "남궁", "en": "Namgung"}
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
			and panel.details.sect == tr("FACTION_" + world.faction_names()[world.person_faction(parent)].to_upper())

	var moved: bool = panel.move(Vector2i.DOWN)
	var child: int = panel.selected
	panel.centre_selected()
	var child_generation: String = panel.details.generation
	var recentred: bool = moved and children.has(child) and panel.centre == child \
			and panel.cards.any(func(card: Dictionary) -> bool: return card.role == "parent" and card.person == parent)

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
	var passed := kin_right and details_right and recentred and control and generations_right
	print("genealogy_test: after %d days person %d %s%s, spouse %d, children %s; cards %s; details %s; down and Enter centre on child %d %s; the founder %d shows %d parents; generations: founder %s, child %s: %s" % [
		DAYS, parent, name.surname, name.given, spouse, children, shown, panel.details if not details_right else "match",
		child, recentred, founder, founder_parents, founder_generation, child_generation,
		"pass" if passed else "FAIL"])
	panel.free()
	return passed and fits


# In each locale, every word the panel draws, in the HUD's font at the panel's size, fits where
# _draw places it: the header clear of the close key, the row labels clear of the cards, a card's
# line and the steered person's tag within the card, the details within their column, each detail
# row's label clear of its value, and the keys along the foot within the panel. Setup: every
# GENEALOGY_ key in hud.csv is measured, and the widths are not zero.
func _fit_pass() -> bool:
	var font: Font = load(Hud.FONT)
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
		var families: Array[String] = []
		for surname: String in [WIDEST_SURNAME[locale], say.call("GENEALOGY_UNNAMED")]:
			families.append(say.call("GENEALOGY_FAMILY") % surname)
		var sects: Array[String] = [say.call("GENEALOGY_NONE")]
		for faction: String in Hud.FACTIONS:
			sects.append(say.call("FACTION_" + faction.to_upper()))
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
			fits.append([line, 23 + width.call(line), Genealogy.CARD.x - 2])
		var tag: String = say.call("GENEALOGY_SELF")
		fits.append([tag, width.call(tag) + 6 + 3, Genealogy.CARD.x])
		for text in families + sects:
			fits.append([text, 50 + width.call(text), Genealogy.DETAILS.size.x - 8])
		var unknown: String = say.call("GENEALOGY_UNKNOWN")
		var rows := [
			["GENEALOGY_AGE_LABEL", str(WIDEST_NUMBER)], ["GENEALOGY_TALENT", unknown],
			["GENEALOGY_LIFESPAN", unknown], ["GENEALOGY_SPOUSE", say.call("GENEALOGY_NONE")],
			["GENEALOGY_CHILDREN", say.call("GENEALOGY_COUNT") % WIDEST_NUMBER],
		]
		for row: Array in rows:
			# The label and its value 4 pixels apart at least.
			fits.append([row, 8 + width.call(say.call(row[0])) + 4 + width.call(row[1]), Genealogy.DETAILS.size.x - 8])
		var keys: String = say.call("GENEALOGY_KEYS")
		fits.append([keys, 8 + width.call(keys), Genealogy.PANEL.size.x - 8])
		for fit: Array in fits:
			checks += 1
			if fit[1] > fit[2] or width.call(str(fit[0])) <= 0.0:
				overruns.append("%s %s reaches %d past %d" % [locale, fit[0], fit[1], fit[2]])
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured: Array[String] = []
	var csv := FileAccess.open("res://hud/hud.csv", FileAccess.READ)
	csv.get_csv_line()
	while not csv.eof_reached():
		var key := csv.get_csv_line()[0]
		if key.begins_with("GENEALOGY_") and not measured.has(key):
			unmeasured.append(key)
	var passed := unmeasured.is_empty() and overruns.is_empty()
	print("genealogy_test: %d keys measured in %d checks across %s; unmeasured %s; overruns %s: %s" % [
		measured.size(), checks, Hud.LOCALES, unmeasured, overruns, "pass" if passed else "FAIL"])
	return passed
