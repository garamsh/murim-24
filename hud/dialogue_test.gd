# Checks the dialogue window against seed 2026's region lived RUMOUR_YEARS years of five-tick days.
# Opened on a living person, its menu holds a row for each of the person's offers, then farewell;
# asked for news, it lists that person's own rumours, newest first, each worded by kind, a birth or
# death naming the person. Control: a person far from the first tells other news, and a dead
# person, who offers nothing, leaves farewell alone on the menu. A seller's menu holds its wares, which
# list every ware and choose among them, show what came of a purchase and go back to the menu, and
# its card names its occupation; control: one who sells nothing has no wares on its menu
# and its sect under its card. Every word and name it draws fits the room it gives it, in each locale,
# every rumour whole on its line.
extends RefCounted

const Dialogue := preload("res://hud/dialogue.gd")
const Hud := preload("res://hud/hud.gd")
const PersonName := preload("res://hud/person_name.gd")
const WordFit := preload("res://hud/word_fit.gd")
const SEED := 2026
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const RUMOUR_YEARS := 10
# Far enough apart, in cells along an axis, that two people's rumours differ: twice the region's
# rumour reach.
const FAR := 64
# The age measured, three digits wide.
const OLDEST := 100


func run() -> bool:
	TranslationServer.set_locale(Hud.LOCALES[0])
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for tick in RUMOUR_YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		world.tick()
	var panel: Control = Dialogue.new()
	panel.use_font(load(Hud.FONT))
	var near := _most_heard(world, -1)
	var far := _most_heard(world, near)
	var passed := _news_pass(world, panel, near, far) and _menu_pass(world, panel, near) \
			and _trade_pass(world, panel) and _fit_pass(world, panel, near)
	panel.free()
	return passed


# The news the window lists for `near` is its own rumours, newest first, worded by kind; control:
# `far`'s differ, and both heard some.
func _news_pass(world: WorldAdapter, panel: Control, near: int, far: int) -> bool:
	panel.open(world, near)
	var menu_first: bool = not panel.showing_news and panel.news.is_empty()
	panel.show_news()
	var heard := world.rumours(near)
	var expected: Array[String] = []
	for event: Dictionary in heard:
		expected.append(panel.rumour_text(world, event))
	var newest_first: bool = heard.size() >= 2 and heard[0].sequence > heard[1].sequence
	# Setup: a birth or a death is among them.
	var births_deaths := 0
	var named := true
	for event: Dictionary in heard:
		if event.kind == "born" or event.kind == "died":
			births_deaths += 1
			named = named and panel.rumour_text(world, event).contains(PersonName.of(world, event.person, true))
	named = named and births_deaths > 0
	var near_news: Array[String] = panel.news.duplicate()
	# Scrolling stops at the last screenful.
	for step in heard.size():
		panel.move(1)
	var last: int = panel.first
	var scrolled: bool = last == maxi(heard.size() - Dialogue.NEWS_LINES, 0) and last > 0
	var went_back: bool = panel.back() and not panel.showing_news and not panel.back()
	panel.open(world, far)
	panel.show_news()
	var far_news: Array[String] = panel.news.duplicate()
	var apart := (_cell(world, near) - _cell(world, far)).abs()
	# Setup: the two stand FAR apart, and each heard something.
	var set_up := maxi(apart.x, apart.y) > FAR and not near_news.is_empty() and not far_news.is_empty()
	var shared := near_news.filter(func(line: String) -> bool: return far_news.has(line)).size()
	var passed := menu_first and near_news == expected and newest_first and named and scrolled and went_back \
			and set_up and near_news != far_news
	print("dialogue_test: person %d tells its %d rumours newest first %s, worded as rumour_text words them %s, %d births and deaths named %s; scrolled to %d; back to the menu %s; person %d, %s cells away, tells %d, %d of them shared: %s" % [
		near, near_news.size(), newest_first, near_news == expected, births_deaths, named, last, went_back, far, apart,
		far_news.size(), shared, "pass" if passed else "FAIL"])
	return passed


# The menu holds a row for each of the person's offers and then farewell, the first chosen; moving
# down chooses farewell. Control: a dead person offers nothing, and farewell stands alone.
func _menu_pass(world: WorldAdapter, panel: Control, living: int) -> bool:
	panel.open(world, living)
	var offered := world.offers(living)
	var expected: Array[int] = []
	for offer in offered:
		expected.append(offer)
	expected.append(Dialogue.FAREWELL)
	# Setup: the person offers its news first, whatever else it offers.
	var rows_right: bool = not offered.is_empty() and offered[0] == WorldAdapter.OFFER_NEWS \
			and panel.rows == expected and panel.choice() == WorldAdapter.OFFER_NEWS
	# Down steps through each offer to farewell, and no further.
	var moved := true
	for step in offered.size():
		moved = moved and panel.move(1)
	moved = moved and panel.choice() == Dialogue.FAREWELL and not panel.move(1)
	# A person struck dead by a hostile neighbour in a world of two.
	var struck := _struck()
	var dead := 0
	panel.open(struck, dead)
	var dead_rows: Array[int] = panel.rows.duplicate()
	var farewell: Array[int] = [Dialogue.FAREWELL]
	var passed: bool = rows_right and moved and struck.person_health(dead) == 0 \
			and struck.offers(dead).is_empty() and dead_rows == farewell
	print("dialogue_test: person %d offers %s, the menu %s; down chooses farewell %s; dead person %d offers %s, the menu %s: %s" % [
		living, offered, expected, moved, dead, struck.offers(dead),
		dead_rows, "pass" if passed else "FAIL"])
	panel.open(world, living)
	return passed


# A seller's menu offers its wares after its news; shown to the player, they list every ware, the
# first chosen, down choosing the next and no further; a refusal shows its word, refused, and a
# purchase the ware bought; Escape goes back to the menu. Its card names its occupation.
# Control: the nearest person who sells nothing has no wares on its menu and its sect, or none, under
# its card.
func _trade_pass(world: WorldAdapter, panel: Control) -> bool:
	var player := world.roster(world.player_army())[0]
	var seller := -1
	var plain := -1
	for army in world.army_count():
		for person in world.roster(army):
			var sells := world.sells(person, 0)
			if sells and seller < 0:
				seller = person
			elif not sells and plain < 0:
				plain = person
	# Setup: the region holds a seller and someone who sells nothing.
	if seller < 0 or plain < 0:
		print("dialogue_test: no seller and no one else to talk to: FAIL")
		return false
	panel.open(world, seller)
	var rows: Array[int] = panel.rows.duplicate()
	var seller_rows: Array[int] = [WorldAdapter.OFFER_NEWS, WorldAdapter.OFFER_TRADE, Dialogue.FAREWELL]
	var seller_card: String = panel.particulars()[1]
	panel.move(1)
	panel.show_wares(player)
	var listed: bool = panel.trading and panel.ware == 0 and panel.move(1) and panel.ware == 1 and not panel.move(1)
	panel.show_bought(WorldAdapter.BUY_NO_SILVER)
	var refused: bool = panel.refused and panel.line == tr("BUY_NO_SILVER")
	panel.show_bought(WorldAdapter.BUY_NONE)
	var bought: bool = not panel.refused and panel.line.contains(panel.ware_name(world.wares()[1]))
	var went_back: bool = panel.back() and not panel.trading and panel.line.is_empty()
	panel.open(world, plain)
	var plain_rows: Array[int] = panel.rows.duplicate()
	var plain_card: String = panel.particulars()[1]
	var sect := world.person_sect(plain)
	var expected_card := tr("DIALOGUE_NO_SECT") if sect < 0 \
			else tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_"))
	var seller_word := Dialogue.occupation_name(world, world.person_occupation(seller))
	var passed: bool = rows == seller_rows and seller_card == seller_word and listed and refused and bought \
			and went_back and plain_rows == [WorldAdapter.OFFER_NEWS, Dialogue.FAREWELL] and plain_card == expected_card
	print("dialogue_test: seller %d's menu %s, card %s; wares listed and chosen %s, refusal shown %s, purchase shown %s, back %s; person %d's menu %s, card %s: %s" % [
		seller, rows, seller_card, listed, refused, bought, went_back, plain, plain_rows, plain_card,
		"pass" if passed else "FAIL"])
	return passed


# In each locale, every word the window draws within the room it gives it: the greeting, each
# offer's row, farewell and the news on its lines, every rumour whole for every faction, sect and
# name the world can make; the person's faction, sect or none and age under its card; the name and
# close word in the title bar, and the keys along the foot; and the prompt's words. Setup: every
# DIALOGUE_ and RUMOUR_ key is measured.
func _fit_pass(world: WorldAdapter, panel: Control, person: int) -> bool:
	var fit := WordFit.new()
	var line: float = panel.line_room()
	var factions := world.faction_names()
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		for key: String in ["DIALOGUE_GREETING", "DIALOGUE_NONE"]:
			fit.check(key, fit.width(fit.say(key)), line)
		for key: String in Dialogue.OFFER_KEYS.values() + ["DIALOGUE_FAREWELL"]:
			fit.check(key, 12 + fit.width(fit.say(key)), line - 4)
		fit.check("DIALOGUE_HEARD", fit.width(fit.say("DIALOGUE_HEARD")) + 8 + fit.width("999 / 999"), line)
		for key: String in ["DIALOGUE_KEYS_MENU", "DIALOGUE_KEYS_NEWS", "DIALOGUE_KEYS_TRADE"]:
			fit.check(key, fit.width(fit.say(key)) + 16, Dialogue.RECT.size.x)
		_fit_wares(world, fit, line)
		var close := fit.say("DIALOGUE_CLOSE")
		var age := fit.say("DIALOGUE_AGE") % OLDEST
		fit.check(age, fit.width(age), panel.card_room())
		fit.check("DIALOGUE_NO_SECT", fit.width(fit.say("DIALOGUE_NO_SECT")), panel.card_room())
		for name in factions:
			var named := tr("FACTION_" + name.to_upper())
			fit.check(named, fit.width(named), panel.card_room())
		for sect in world.sect_count():
			var named := tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_"))
			fit.check(named, fit.width(named), panel.card_room())
		fit.say("DIALOGUE_TALK")
		fit.say("DIALOGUE_HOSTILE")
		var texts: Array[String] = []
		for owner in factions.size():
			texts.append(_said(fit, "RUMOUR_FOUNDED", [_faction(factions, owner)]))
			texts.append(_said(fit, "RUMOUR_RUINED", [_faction(factions, owner)]))
			for raider in factions.size():
				texts.append(_said(fit, "RUMOUR_RAIDED", [_faction(factions, raider), _faction(factions, owner)]))
		for sect in world.sect_count():
			texts.append(_said(fit, "RUMOUR_SECT_FELL", [tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_"))]))
		for name in _every_name(world):
			var full := PersonName.shown(name)
			fit.check(full, 8 + fit.width(full) + 10 + fit.width(close) + 8, Dialogue.RECT.size.x)
			var short := PersonName.shown(name, true)
			for owner in factions.size():
				texts.append(_said(fit, "RUMOUR_ELECTED", [short, _faction(factions, owner)]))
				texts.append(_said(fit, "RUMOUR_DUEL", [short, _faction(factions, owner)]))
			texts.append(_said(fit, "RUMOUR_BORN", [short]))
			texts.append(_said(fit, "RUMOUR_DIED", [short]))
		for text in texts:
			fit.check(text, 8 + fit.width(text), line)
	TranslationServer.set_locale(Hud.LOCALES[0])
	panel.open(world, person)
	var unmeasured := fit.unmeasured(["DIALOGUE_", "RUMOUR_", "BUY_", "WARE_", "SILVER"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty() and fit.checks > 0
	print("dialogue_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns.slice(0, 3),
		"pass" if passed else "FAIL"])
	return passed


# The wares' words within their rooms in the current locale: the greeting beside the silver, each
# ware's name, effect, count and price on its row, the line for each purchase and refusal, and each
# occupation's word, which a seller's card shows.
func _fit_wares(world: WorldAdapter, fit: WordFit, line: float) -> void:
	var silver := fit.say("SILVER") % 99999
	fit.check("DIALOGUE_TRADE_GREETING", fit.width(fit.say("DIALOGUE_TRADE_GREETING")), line - 80)
	fit.check("SILVER", fit.width(silver) + 18, 80)
	fit.check("WARE_HELD", fit.width(fit.say("WARE_HELD") % 99), 44)
	for ware: Dictionary in world.wares():
		var key := "WARE_" + String(ware.name).to_upper()
		var named := fit.say(key)
		fit.check(key, fit.width(named), 104)
		var effect := fit.say("WARE_HEALTH") % int(ware.health) if int(ware.health) > 0 \
				else fit.say("WARE_QI") % "99,999"
		fit.check(effect, fit.width(effect), 70)
		fit.check("DIALOGUE_BOUGHT", fit.width(fit.say("DIALOGUE_BOUGHT") % named), line)
	for key: String in Dialogue.BUY_KEYS.values():
		fit.check(key, fit.width(fit.say(key)), line)
	for occupation in world.occupation_names().size():
		var key := "OCCUPATION_" + world.occupation_names()[occupation].to_upper()
		fit.check(key, fit.width(fit.say(key)), 84)


# Person 0, struck dead on the first tick by person 1, of a faction hostile to its own.
func _struck() -> WorldAdapter:
	var rows := PackedStringArray([".".repeat(4), ".".repeat(4)])
	var idle := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1}
	var strike := {"range": 2 * WorldAdapter.CELL_SIZE, "radius": 0, "body_damage": 1, "structure_damage": 0,
			"cooldown": 1000}
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(0, 0), "members": [{"cell": Vector2i(0, 0), "health": 1}], "faction": 0, "skills": [idle]},
		{"objective": Vector2i(1, 0), "members": [{"cell": Vector2i(1, 0), "health": 1}], "faction": 1, "skills": [strike]},
	]
	var rules := {"separation": 0, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}
	var made := WorldAdapter.create(1, rows, armies, [], rules)
	made.tick()
	return made


# The living person, at least FAR cells from `other` along an axis unless `other` is -1, who has
# heard the most rumours, the lowest index among equals.
func _most_heard(world: WorldAdapter, other: int) -> int:
	var best := -1
	for army in world.army_count():
		for person in world.roster(army):
			if other >= 0:
				var apart := (_cell(world, person) - _cell(world, other)).abs()
				if maxi(apart.x, apart.y) <= FAR:
					continue
			if best < 0 or world.rumours(person).size() > world.rumours(best).size():
				best = person
	return best


func _cell(world: WorldAdapter, person: int) -> Vector2i:
	return Vector2i((world.person_position(person) / WorldAdapter.CELL_SIZE).floor())


func _said(fit: WordFit, key: String, args: Array) -> String:
	return fit.say(key) % args


func _faction(factions: PackedStringArray, index: int) -> String:
	return tr("FACTION_" + factions[index].to_upper())


# Every surname in `world`'s names() crossed with every given name, as person_name gives a name.
func _every_name(world: WorldAdapter) -> Array[Dictionary]:
	var table := world.names()
	var names: Array[Dictionary] = []
	for surname: Dictionary in table.surnames:
		for given: Dictionary in table.female + table.male:
			names.append({"surname": surname.korean, "given": given.korean,
					"surname_en": surname.romanised, "given_en": given.romanised})
	return names
