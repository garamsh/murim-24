# Checks the faction window against seed 2026's region as it begins. Every faction is listed, the
# steered person's first, each standing as the core's one read, relation(), gives it. A person who
# does not lead its faction is offered no order: each is refused, and asking gives none. Control:
# its faction's leader is offered war on an ally and refused peace with it, war is asked again
# before it is given, and once war stands the window greys war with its refusal's word and shows
# the war's basis. On a touch screen a tap chooses a faction, an order and an answer. Every word
# and name the window draws fits the room it gives it, in each locale, in both layouts.
extends RefCounted

const Faction := preload("res://hud/faction.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const SEED := 2026
const TICKS_PER_DAY := 5
# The widest people and village counts the window shows: every person in one faction.
const MOST := 500


func run() -> bool:
	TranslationServer.set_locale(Hud.LOCALES[0])
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	var player := world.player_army()
	var panel: Control = Faction.new()
	panel.use_font(load(Hud.FONT))

	# A member: every faction listed, as relation() reads it, and no order.
	panel.show_factions(world, player, false)
	var own := world.army_faction(player)
	var listed: bool = panel.rows.size() == world.faction_count() and panel.rows[0].faction == own \
			and panel.rows[0].relation == WorldAdapter.RELATION_OWN
	for row: Dictionary in panel.rows:
		listed = listed and row.relation == world.relation(own, row.faction)
	var member_refused: bool = not panel.leads and panel.orders.all(func(order: Dictionary) -> bool:
			return order.refusal == WorldAdapter.DECLARE_NOT_THE_LEADER)
	var member_asked: int = panel.ask()
	var member_none: bool = member_asked == -1 and panel.confirming == -1
	print("faction_test: a member of faction %d sees %d factions as relation() reads them %s; offered no order %s, asking gives %d: %s" % [
		own, panel.rows.size(), listed, member_refused, member_asked,
		"pass" if listed and member_refused and member_none else "FAIL"])

	var leading := _leader_army(world, own)
	var leader := world.roster(leading)[0]
	panel.show_factions(world, leading, false)
	var ally := _row_of(panel, WorldAdapter.RELATION_ALLY)
	var setup: bool = panel.leads and ally > 0
	while panel.chosen < ally:
		panel.move(Vector2i.DOWN)
	var target: int = panel.chosen_faction()
	var offered: bool = panel.orders[0].refusal == WorldAdapter.DECLARE_NONE \
			and panel.orders[1].refusal == WorldAdapter.DECLARE_ALREADY_AT_PEACE
	var first: int = panel.ask()
	var asked_again: bool = first == -1 and panel.confirming == WorldAdapter.DECLARATION_WAR
	var given: int = panel.answer(true)
	var declared: bool = given == WorldAdapter.DECLARATION_WAR and world.declare(leader, target, given)
	panel.refresh()
	var row: Dictionary = panel.rows[panel.chosen]
	var greyed: bool = row.relation == WorldAdapter.RELATION_AT_WAR \
			and panel.orders[0].refusal == WorldAdapter.DECLARE_ALREADY_AT_WAR \
			and tr(Faction.REFUSAL_KEYS[panel.orders[0].refusal]) != Faction.REFUSAL_KEYS[panel.orders[0].refusal] \
			and panel.orders[1].refusal == WorldAdapter.DECLARE_TOO_SOON and row.has("declared") \
			and row.since != tr("FACTION_BY_STANCE")
	panel.order = 0
	var refused_asked: int = panel.ask()
	var leader_passed: bool = setup and offered and asked_again and declared and greyed and refused_asked == -1 \
			and panel.confirming == -1
	print("faction_test: the leader (person %d) of faction %d offered war on its ally %d %s, asked again %s, declared %s; then war greyed with \"%s\", peace %s, the basis \"%s\", asking gives %d: %s" % [
		leader, own, target, offered, asked_again, declared, tr(Faction.REFUSAL_KEYS.get(panel.orders[0].refusal, "")),
		Faction.REFUSAL_KEYS.get(panel.orders[1].refusal, ""), row.get("declared", ""), refused_asked,
		"pass" if leader_passed else "FAIL"])

	var tapped := _tap_pass(world, leading)
	var fit := _fit_pass(world, panel)
	panel.free()
	return member_none and member_refused and listed and leader_passed and tapped and fit


# The army led by faction `faction`'s leader, or the player's when none is.
func _leader_army(world: WorldAdapter, faction: int) -> int:
	for army in world.army_count():
		var roster := world.roster(army)
		if not roster.is_empty() and roster[0] == world.leader(faction):
			return army
	return world.player_army()


# The first row of the window standing as `relation`, or -1.
func _row_of(panel: Control, relation: int) -> int:
	for n in range(1, panel.rows.size()):
		if panel.rows[n].relation == relation:
			return n
	return -1


# On a touch screen, a tap on a hostile faction's row chooses it, one on the war button lights it,
# and once war is asked again a tap on the question's yes answers it. Control: a tap on the panel's
# empty middle asks nothing.
func _tap_pass(world: WorldAdapter, army: int) -> bool:
	var panel: Control = Faction.new()
	panel.use_font(load(Hud.FONT))
	panel.show_factions(world, army, true)
	var hostile := _row_of(panel, WorldAdapter.RELATION_HOSTILE)
	var row_rect: Rect2 = panel._touch_row(hostile)
	var chose: bool = hostile > 0 and panel.tap(row_rect.get_center()) == "choose" and panel.chosen == hostile
	var nothing: bool = panel.tap(Vector2(140, 236)) == ""
	var order: String = panel.tap(panel._order_button(0).get_center())
	var war: bool = order == "order" and panel.order == 0
	var opened := false
	if panel.orders[0].refusal == WorldAdapter.DECLARE_NONE:
		panel.ask()
		opened = panel.confirming == WorldAdapter.DECLARATION_WAR \
				and panel.tap(panel._confirm_buttons(Faction.TOUCH_CONFIRM)[0].get_center()) == "yes"
	panel.answer(false)
	var closed: bool = panel.tap(Vector2(Faction.PANEL.end.x - 10, Faction.PANEL.position.y + 8)) == "close"
	var passed := chose and nothing and war and opened and closed
	print("faction_test: on a touch screen a row tapped chooses it %s, the middle asks nothing %s, war tapped lit %s, its question answered yes %s, the close word closes %s: %s" % [
		chose, nothing, war, opened, closed, "pass" if passed else "FAIL"])
	panel.free()
	return passed


# In each locale, every word the window draws from hud.csv within the room ROOM gives it, and on
# their lines: each faction's name in its column and in the detail beside its relation, a leader's
# word and name on a touch row, the most people and villages there are on a touch row and in their
# columns, a declaration's basis in its column and its words in the detail, the title and close
# word in the title bar, and the keys along the foot. Setup: every FACTION_ and DECLARE_ key is
# measured.
func _fit_pass(world: WorldAdapter, panel: Control) -> bool:
	var fit := WordFit.new()
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		for key: String in Faction.ROOM:
			var word := fit.say(key)
			fit.check(word, fit.width(word), Faction.ROOM[key])
		var widest_tag := 0.0
		for key: String in ["FACTION_ALLY", "FACTION_NEUTRAL", "FACTION_HOSTILE", "FACTION_AT_WAR", "FACTION_OURS"]:
			widest_tag = maxf(widest_tag, fit.width(fit.say(key)) + 6)
		var date := tr("HUD_SEASON_YEAR") % [tr("SEASON_3"), 100]
		for name in world.faction_names():
			var named := fit.say("FACTION_" + name.to_upper())
			fit.check(named, Faction.NAME_X + 6 + fit.width(named), Faction.RELATION_X - 4)
			fit.check(named, fit.width(named) + 8 + widest_tag, Faction.DIVIDER_X - 8 - Faction.DETAIL_X)
			fit.check(named, fit.width(named) + 8 + widest_tag, Faction.TOUCH_DETAIL.size.x)
			fit.check(named, 8 + fit.width(named), Faction.TOUCH_TEXT_X - 4)
			var basis := fit.say("FACTION_DECLARED_BY") % [named, date]
			if locale == "ko":
				fit.check(basis, fit.width(basis), Faction.DIVIDER_X - 8 - Faction.DETAIL_X)
			var line := fit.say("FACTION_CONFIRM_LINE") % named
			fit.check(line, fit.width(line), Faction.PC_CONFIRM.size.x - 24)
		for key: String in ["FACTION_CONFIRM_ALLY", "FACTION_CONFIRM_WAR"]:
			var word := fit.say(key)
			fit.check(word, fit.width(word), Faction.PC_CONFIRM.size.x - 24)
		fit.check(date, fit.width(date), Faction.PANEL.end.x - 8 - Faction.SINCE_X)
		var strength := fit.say("FACTION_STRENGTH") % [MOST, MOST]
		fit.check(strength, fit.width(strength), Faction.TOUCH_ROWS.size.x - Faction.TOUCH_TEXT_X - 6)
		fit.check(str(MOST), fit.width(fit.say("FACTION_PEOPLE")) + fit.width(str(MOST)),
				Faction.PEOPLE_RIGHT - Faction.LEADER_X)
		fit.check(str(MOST), fit.width(str(MOST)), Faction.VILLAGES_RIGHT - Faction.PEOPLE_RIGHT - 4)
		fit.check("relation column", widest_tag, Faction.LEADER_X - Faction.RELATION_X - 4)
		var title := fit.say("FACTION_TITLE")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(fit.say("FACTION_CLOSE")) + 8, Faction.PANEL.size.x)
		for key: String in ["FACTION_KEYS_LEADER", "FACTION_KEYS_MEMBER"]:
			fit.check(key, fit.width(fit.say(key)) + 16, Faction.PANEL.size.x)
		for key: String in ["FACTION_LEGEND_ALLY", "FACTION_LEGEND_NEUTRAL", "FACTION_LEGEND_HOSTILE",
				"FACTION_LEGEND_AT_WAR"]:
			fit.check(key, Faction.LEGEND_X + 46 + fit.width(fit.say(key)), Faction.PANEL.end.x - 6)
		var you := fit.say("FACTION_YOU_LEAD")
		for name in world.faction_names():
			var named := tr("FACTION_" + name.to_upper())
			fit.check(you, Faction.NAME_X + fit.width(named) + 10 + fit.width(tr("HUD_LEADER")) + 6 + 56 + 8
					+ fit.width(you) + 6, Faction.PANEL.end.x - 10)
		panel.refresh()
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["FACTION_", "DECLARE_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("faction_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed
