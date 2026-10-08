# Checks the arts panel against a region lived ten years of five-tick days, steering the first
# person who knows an art skill. The panel lists the art first, as the heart method, with every
# skill of it: those learned not greyed, those of a realm above the person's marked, each with its
# cost, realm and cooldown as the core gives them; then the army's skills. On PC the first skill
# the person has is chosen, the arrows move over the skills it has, passing those it lacks, and
# stop at the ends; a skill put on a slot leaves the slot it was on, and Delete takes it off. On a
# touch screen none is chosen until a tile of a skill the person has is tapped. Given an art of
# another nature, the panel tags it weaker. Every word and name the panel draws fits the room it
# gives it, in each locale, in both layouts.
extends RefCounted

const Arts := preload("res://hud/arts.gd")
const Loadout := preload("res://hud/loadout.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const SEED := 2026
const TICKS_PER_DAY := 5
const LIVED_TICKS := 10 * 120 * TICKS_PER_DAY


func run() -> bool:
	TranslationServer.set_locale(Hud.LOCALES[0])
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in LIVED_TICKS:
		world.tick()
	var army := _learned_army(world)
	if army < 0:
		print("arts_test: no person knows an art after %d ticks: FAIL" % LIVED_TICKS)
		return false
	var person := world.roster(army)[0]
	var skills := world.person_skills(person)
	var arts := world.arts()
	var art := -1
	var drilled := 0
	for skill: Dictionary in skills:
		if skill.art >= 0 and art < 0:
			art = skill.art
		drilled += 1 if skill.art < 0 else 0
	var rank := world.person_realm(person)
	var loadout := Loadout.new()
	loadout.follow(world, person)
	var panel: Control = Arts.new()
	panel.use_font(load(Hud.FONT))
	panel.show_arts(world, army, loadout, false)

	# The heart art first, every skill of it, and the army's skills last.
	var groups: Array = panel.shown.groups
	var heart: Dictionary = groups[0]
	var listed_right: bool = heart.name == arts[art].korean and heart.skills.size() == arts[art].skills.size() \
			and heart.tags.size() == 1 and heart.tags[0].text == "심법" and groups.back().has("faction") \
			and groups.back().skills.size() == drilled and panel.shown.heart.name == arts[art].korean
	var greyed := 0
	var locked := 0
	for index in heart.skills.size():
		var shown: Dictionary = heart.skills[index]
		var taught: Dictionary = arts[art].skills[index]
		var learned := false
		for at in skills.size():
			learned = learned or Loadout.identity(skills, at) == {"art": art, "skill": index}
		listed_right = listed_right and shown.known == learned and shown.locked == (int(taught.rank) > rank) \
				and shown.cost == Arts.grouped(taught.qi_cost) and shown.realm == Arts.realm_name(world, taught.rank) \
				and shown.cooldown == "%d초" % ceili(float(taught.cooldown) / WorldAdapter.TICKS_PER_SECOND)
		greyed += 0 if shown.known else 1
		locked += 1 if shown.locked else 0
	# Setup: the person lacks a skill of its art, and one is past its realm, so both marks are tried.
	var set_up := greyed > 0 and locked > 0 and drilled > 0
	print("arts_test: army %d, person %d of realm %d, knows %d skills of %s and %d of its army; listed %s, %d greyed, %d past its realm: %s" % [
		army, person, rank, skills.size() - drilled, heart.name, drilled, groups.map(func(g: Dictionary) -> String: return g.name),
		greyed, locked, "pass" if listed_right and set_up else "FAIL"])

	# The arrows over the skills the person has, passing those it lacks; stops at both ends.
	var first: Dictionary = panel.selected
	var up_refused: bool = not panel.move(Vector2i.UP) and panel.selected == first
	var known_skills := []
	known_skills.append(panel.selected)
	while panel.move(Vector2i.DOWN):
		known_skills.append(panel.selected)
	var down_refused: bool = not panel.move(Vector2i.DOWN)
	var expected := []
	for at in skills.size():
		if skills[at].art >= 0:
			expected.append(Loadout.identity(skills, at))
	for at in skills.size():
		if skills[at].art < 0:
			expected.append(Loadout.identity(skills, at))
	var moved_right := first == {"art": art, "skill": 0} and up_refused and down_refused \
			and known_skills.size() == skills.size() and known_skills == expected
	print("arts_test: chosen first %s; up refused %s; down through %s, refused past the last %s: %s" % [
		first, up_refused, known_skills, down_refused, "pass" if moved_right else "FAIL"])

	# The first skill put on T leaves its own slot; Delete takes it off. Control: Delete again finds
	# it on no slot.
	while panel.move(Vector2i.UP):
		pass
	var was: int = loadout.slot_of(panel.selected)
	var assigned: bool = panel.assign(4)
	var moved_slot: bool = loadout.slots[4] == panel.selected and (was < 0 or was == 4 or loadout.slots[was].is_empty()) \
			and panel.just == 4
	var cleared: bool = panel.clear_selected() and loadout.slot_of(panel.selected) < 0 and loadout.slots[4].is_empty()
	var cleared_again: bool = panel.clear_selected()
	var assign_right := assigned and moved_slot and cleared and not cleared_again
	print("arts_test: %s on slot %d put on T %s, leaving its slot %s; Delete took it off %s, again %s: %s" % [
		panel.selected, was, assigned, moved_slot, cleared, cleared_again, "pass" if assign_right else "FAIL"])

	# On a touch screen nothing is chosen until a tile is tapped: a tile of a skill the person lacks
	# chooses nothing, one it has chooses that skill. Control: the PC layout chose one on opening.
	panel.show_arts(world, army, loadout, true)
	var none_chosen: bool = panel.selected.is_empty()
	var lacking := Vector2.ZERO
	var having := Vector2.ZERO
	var had := {}
	for tile: Dictionary in panel._tiles():
		if not tile.skill.known and lacking == Vector2.ZERO:
			lacking = tile.rect.get_center()
		if tile.skill.known and having == Vector2.ZERO:
			having = tile.rect.get_center()
			had = tile.skill.held
	var lacking_refused: bool = not panel.tap(lacking) and panel.selected.is_empty()
	var having_chosen: bool = panel.tap(having) and panel.selected == had
	var scroll_refused: bool = groups.size() <= Arts.GROUPS_SHOWN and not panel.scroll(1)
	var touch_right := none_chosen and lacking_refused and having_chosen and scroll_refused and not first.is_empty()
	print("arts_test: touch, none chosen on opening %s; a tile it lacks refused %s; a tile it has chose %s %s; %d groups scroll nowhere %s: %s" % [
		none_chosen, lacking_refused, had, having_chosen, groups.size(), scroll_refused, "pass" if touch_right else "FAIL"])
	panel.free()
	var weaker_right := _weaker_pass(world, person)
	return listed_right and set_up and moved_right and assign_right and touch_right and weaker_right \
			and _fit_pass(world)


# The same person's reads, given the first skill of an art of another nature than its heart
# method's: that art is listed second, tagged weaker, the heart method keeping its own tag. Control:
# the same art made of the heart method's nature, in orientation and polarity, carries no tag.
func _weaker_pass(world: WorldAdapter, person: int) -> bool:
	var reads := Arts.read(world, person)
	var heart := -1
	for skill: Dictionary in reads.skills:
		if skill.art >= 0 and heart < 0:
			heart = skill.art
	var other := -1
	for art in reads.arts.size():
		if other < 0 and (reads.arts[art].orientation != reads.arts[heart].orientation \
				or reads.arts[art].polarity != reads.arts[heart].polarity):
			other = art
	var given := reads.duplicate(true)
	var taught: Dictionary = given.arts[other].skills[0]
	given.skills.append({"range": taught.range, "radius": taught.radius, "body_damage": 0,
			"structure_damage": 0, "cooldown": taught.cooldown, "qi_cost": taught.qi_cost, "ready_at": 0,
			"art": other, "skill": 0, "rank": taught.rank, "korean": taught.korean,
			"romanised": taught.romanised, "orientation": given.arts[other].orientation,
			"polarity": given.arts[other].polarity})
	var tags := func(composed: Dictionary) -> Array:
		return composed.groups.map(func(group: Dictionary) -> Array:
			return group.tags.map(func(tag: Dictionary) -> String: return tag.text))
	var weaker: Array = tags.call(Arts.compose(given))
	var alike := given.duplicate(true)
	alike.arts[other].orientation = alike.arts[heart].orientation
	alike.arts[other].polarity = alike.arts[heart].polarity
	var unmarked: Array = tags.call(Arts.compose(alike))
	# Setup: the person's nature is its heart method's, and the art given is not yet known to it.
	var set_up: bool = heart >= 0 and other >= 0 and reads.nature.orientation == reads.arts[heart].orientation \
			and reads.nature.polarity == reads.arts[heart].polarity \
			and reads.skills.all(func(skill: Dictionary) -> bool: return skill.art != other)
	var passed: bool = set_up and weaker.size() == 3 and weaker[0] == ["심법"] and weaker[1] == ["약화"] \
			and unmarked.size() == 3 and unmarked[0] == ["심법"] and unmarked[1] == []
	print("arts_test: heart method %d given art %d of another nature, tags %s; made of the heart's nature, %s (setup %s): %s" % [
		heart, other, weaker, unmarked, set_up, "pass" if passed else "FAIL"])
	return passed


# The army whose first person knows the most art skills, the first among equals, or -1.
func _learned_army(world: WorldAdapter) -> int:
	var best := -1
	var most := 0
	for army in world.army_count():
		var roster := world.roster(army)
		if roster.is_empty():
			continue
		var known := 0
		for skill: Dictionary in world.person_skills(roster[0]):
			known += 1 if skill.art >= 0 else 0
		if known > most:
			most = known
			best = army
	return best


# In each locale, every word the panel draws from hud.csv within the room ROOM gives it, and every
# realm, art and skill name, and nature word, within the room the panel gives that kind; and on
# their lines, in both layouts: the realm and the next realm beside their labels, the qi at the
# highest realm's threshold beside its label, an art's header with its nature and tag, a skill's
# cost, realm and cooldown in their columns, the tapped skill's details, and the keys along the
# foot. Setup: every ARTS_ key is measured.
func _fit_pass(world: WorldAdapter) -> bool:
	var fit := WordFit.new()
	var realms := world.realms()
	var arts := world.arts()
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		for key: String in Arts.ROOM:
			var word := fit.say(key)
			if key in ["ARTS_CELLS", "ARTS_SECONDS"]:
				word = word % 88
			fit.check(word, fit.width(word), Arts.ROOM[key])
		var widest_realm := 0.0
		for rank in range(1, realms.size() + 1):
			var name := Arts.realm_name(world, rank)
			widest_realm = maxf(widest_realm, fit.width(name))
			fit.check(name, fit.width(name), Arts.REALM_ROOM)
			# The next realm with the qi that reaches it, or the qi alone where both overrun on PC;
			# in Korean, and on a touch screen, both whole.
			var threshold := Arts.grouped(realms[rank - 1].qi)
			var next := "%s %s" % [name, threshold]
			var next_reach := fit.width(fit.say("ARTS_NEXT")) + 4 + fit.width(next)
			if locale != "ko" and next_reach > Arts.COLUMN_WIDTH:
				next_reach = fit.width(fit.say("ARTS_NEXT")) + 4 + fit.width(threshold)
			fit.check(next, next_reach, Arts.COLUMN_WIDTH)
			fit.check(next, fit.width(fit.say("ARTS_NEXT")) + 4 + fit.width(next), Arts.DETAIL.size.x)
			for column: float in [Arts.COLUMN_WIDTH, Arts.DETAIL.size.x]:
				fit.check("%s %s" % [fit.say("ARTS_REALM"), name], fit.width(fit.say("ARTS_REALM")) + 4 + fit.width(name), column)
		var top := Arts.grouped(realms.back().qi)
		var qi := "%s / %s" % [top, top]
		for column: float in [Arts.COLUMN_WIDTH, Arts.DETAIL.size.x]:
			fit.check(qi, fit.width(fit.say("ARTS_QI")) + 4 + fit.width(qi), column)
		var tags := [fit.say("ARTS_HEART_TAG"), fit.say("ARTS_WEAKER")]
		var widest_tag := maxf(fit.width(tags[0]), fit.width(tags[1])) + 6
		for art: Dictionary in arts:
			var name := Arts.named(art)
			var orientation := fit.say(Arts.ORIENTATION_KEYS[int(art.orientation)])
			var polarity := fit.say(Arts.POLARITY_KEYS[int(art.polarity)])
			fit.check(orientation, fit.width(orientation), Arts.NATURE_ROOM)
			fit.check(polarity, fit.width(polarity), Arts.POLARITY_ROOM)
			var nature := "%s · %s" % [orientation, polarity]
			fit.check("%s %s" % [name, nature], fit.width(name) + 6 + fit.width(nature) + 6 + widest_tag,
					Arts.LIST_RIGHT - Arts.LIST_LEFT)
			# On a touch screen an art's name is cut short before its tag; in Korean it fits whole.
			if locale == "ko":
				fit.check("%s tag" % name, fit.width(name) + 4 + widest_tag, Arts.TOUCH_DIVIDER_X - Arts.GROUPS_LEFT - 4)
			fit.check("%s heart" % name, fit.width(name), Arts.COLUMN_WIDTH)
			fit.check("%s tags" % nature, fit.width(orientation) + 6 + 3 + fit.width(polarity) + 6, Arts.COLUMN_WIDTH)
			fit.check("%s detail tags" % nature, fit.width(orientation) + 6 + 3 + fit.width(polarity) + 6 + 3
					+ fit.width(tags[1]) + 6, Arts.DETAIL.size.x)
			for skill: Dictionary in art.skills:
				var skill_name := Arts.named(skill)
				fit.check(skill_name, fit.width(skill_name), Arts.NAME_ROOM)
				fit.check(skill_name, Arts.NAME_X + fit.width(skill_name),
						Arts.COST_RIGHT - 4 - fit.width(Arts.grouped(skill.qi_cost)))
				fit.check(skill_name, fit.width(skill_name), Arts.DETAIL.size.x)
				var cost := Arts.grouped(skill.qi_cost)
				fit.check(cost, fit.width(cost), Arts.ROOM.ARTS_COST)
				var cooldown := fit.say("ARTS_SECONDS") % ceili(float(skill.cooldown) / WorldAdapter.TICKS_PER_SECOND)
				fit.check(cooldown, fit.width(cooldown), Arts.COOLDOWN_RIGHT - Arts.REALM_RIGHT - 4)
		fit.check("realm column", widest_realm, Arts.REALM_RIGHT - Arts.COST_RIGHT - 4)
		for key: String in ["ARTS_UNLEARNED", "ARTS_LOCKED"]:
			fit.check(key, 10 + fit.width(fit.say(key)), Arts.COLUMN_WIDTH)
		for key: String in ["ARTS_COST", "ARTS_REALM_COLUMN", "ARTS_COOLDOWN", "ARTS_RANGE", "ARTS_AREA"]:
			var widest_value := maxf(widest_realm, fit.width(fit.say("ARTS_CELLS") % 88))
			fit.check(key, fit.width(fit.say(key)) + 4 + widest_value, Arts.DETAIL.size.x)
		fit.check("loadout", Arts.COLUMN.x + fit.width(fit.say("ARTS_LOADOUT")), Arts.LIST_LEFT + 30)
		var title := fit.say("ARTS_TITLE")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(fit.say("ARTS_CLOSE")) + 8, Arts.DIALOG.size.x)
		fit.check("ARTS_KEYS", fit.width(fit.say("ARTS_KEYS")) + 16, Arts.DIALOG.size.x)
		fit.check("ARTS_TOUCH_KEYS", fit.width(fit.say("ARTS_TOUCH_KEYS")) + 16, Arts.TOUCH_DIALOG.size.x)
		fit.check("ARTS_FACTION", fit.width(fit.say("ARTS_FACTION")), Arts.TOUCH_DIVIDER_X - Arts.GROUPS_LEFT - 4)
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["ARTS_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("arts_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed
