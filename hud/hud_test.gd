# Checks that hud.tscn shows its words from the translations, Korean first, that switching the
# locale changes them to English, that the faction shown is the steered army's, that health shows
# its strength as a percentage of its starting strength and qi the person's against its store,
# that the technique bar shows five slots on Q W E R T, the person's first skills on them with the
# icon of their shape, and that the season and year are the core's. It also checks that the leader and duel panels name people by
# the adapter's person_name, Korean first and romanised in English, and that every word the HUD
# shows from hud.csv, and every name the world's data can make, fits the room its panel gives it, in
# each locale, measured in the font and size each label draws with.
extends RefCounted

const SCENE := "res://hud/hud.tscn"
const PersonName := preload("res://hud/person_name.gd")
const SEED := 2026
# A seed whose first Cult village's leader has the widest given name, Cheonmyeong.
const LONG_NAME_SEED := 18
# The keys a label shows besides the text it is built with, by its name in `labels`; the notice
# also shows every challenge verdict, every reason a cast is refused and whether an autosave was
# written.
const SHOWN := {
	"faction": ["FACTION_ORTHODOX", "FACTION_DEMONIC", "FACTION_UNORTHODOX"],
	"leader_value": ["HUD_LEADER_NONE", "HUD_LEADER_YOU"],
	"duel_challenger": ["HUD_LEADER_YOU"],
	"duel_leader": ["HUD_LEADER_YOU"],
	"notice": ["HUD_DUEL_NEW_LEADER", "HUD_DUEL_LEADER_HELD", "HUD_AUTOSAVED", "HUD_AUTOSAVE_FAILED"],
	"rumour_0": ["HUD_RUMOR_NONE"],
}
# The prefixes of the keys the panels and screens over the HUD draw, which their own tests measure,
# and of the rumours' and the lessons' words, which _rumours_pass and _lessons_pass measure.
const PANEL_PREFIXES := ["GENEALOGY_", "SECT_", "TITLE_", "NEW_GAME_", "PAUSE_", "CONTROLS_", "SETTINGS_", "LOCALE_",
		"APPLICATION_", "REBIRTH_", "RUMOUR_", "DISCIPLINE_", "TOUCH_", "ARTS_", "NOTICE_"]
const Arts := preload("res://hud/arts.gd")
# Ticks of five-tick days the lessons' check looks through for a lesson or a realm of a steered
# person.
const LESSON_TICKS := 6000
# Years with days of five ticks for rumours to reach the player.
const RUMOUR_YEARS := 10
const SHORT_DAY := 5
const DAYS_PER_YEAR := 120
# The season label is measured at a three-digit year.
const LATEST_YEAR := 999
# Text keeps clear of a panel's right edge by its 1-pixel border and the 4 pixels labels keep from
# the left one; a label outside any panel keeps 4 pixels from the screen's edge, as panels do.
const INSET := 5
const SCREEN_WIDTH := 480
const SCREEN_INSET := 4


func run() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var steered := world.army_count() - 1
	var hud: CanvasLayer = load(SCENE).instantiate()
	hud.show_player(world, steered)
	var hp: Label = hud.labels.hp
	var faction: Label = hud.labels.faction
	var korean := [TranslationServer.get_locale(), hp.atr(hp.text), faction.atr(faction.text)]
	var switched: String = hud.switch_locale()
	var english := [TranslationServer.get_locale(), hp.atr(hp.text), faction.atr(faction.text)]
	hud.switch_locale()
	var back := TranslationServer.get_locale()
	var expected_faction: String = "FACTION_" + world.faction_names()[world.army_faction(steered)].to_upper()
	# Setup: the label holds the key, so a translated text differing from it came from a file.
	var keyed := hp.text == "HUD_HP" and faction.text == expected_faction
	hud.refresh(world, steered, PackedInt64Array([0]))
	var strength := 0
	for body_strength in world.body_strengths(steered):
		strength += body_strength
	var hp_value: String = hud.labels.hp_value.text
	var qi_value: String = hud.labels.qi_value.text
	var percent := floori(100.0 * strength / world.army_strength(steered))
	var person := world.roster(steered)[0]
	var values_right: bool = hp_value == "%d%%" % percent \
			and qi_value == "%d / %d" % [world.person_qi(person), world.person_qi_capacity(person)] \
			and hud.hp_bar.value == 100.0 * strength / world.army_strength(steered)
	# Five slots on Q W E R T, the person's first skills on them, each with the icon of its shape;
	# control: the slots past its skills show none.
	var skills := world.person_skills(person)
	var icons_right: bool = skills.size() > 0 and skills.size() < hud.slot_icons.size() and hud.slot_icons.size() == 5
	var keys := []
	for slot in hud.slot_icons.size():
		var icon: TextureRect = hud.slot_icons[slot]
		var key: Label = icon.get_parent().get_child(1)
		keys.append(key.text)
		if slot < skills.size():
			var burst := int(skills[slot].radius) > 0
			var region := (icon.texture as AtlasTexture).region
			icons_right = icons_right and region.position.x == (16 if burst else 0) and hud.slot_skills[slot] == skills[slot]
		else:
			icons_right = icons_right and icon.texture == null and hud.slot_skills[slot].is_empty()
	icons_right = icons_right and keys == ["Q", "W", "E", "R", "T"]
	# The season and year the core gives, spring of the first year at the start; control: a later
	# date reads differently, so the label is not fixed text.
	var season: String = hud.labels.season.text
	var date := world.date()
	var later: String = hud.season_text({"year": 3, "season": 1, "day": 1})
	var dated: bool = date.year == 1 and date.season == 0 and season == "봄 · 1년" \
			and later == "여름 · 3년"
	var passed := keyed and korean == ["ko", "체력", "정파"] and switched == "en" \
			and english == ["en", "HP", "Orthodox"] and back == "ko" and values_right and icons_right \
			and dated
	print("hud_test: keys %s, %s; Korean %s; after switching to %s, %s; back to %s; HP %s, qi %s, its bar %d; %d skills on the slots %s with their icons, the rest empty %s: %s" % [
		hp.text, faction.text, korean, switched, english, back, hp_value, qi_value, hud.qi_bar.value, skills.size(),
		keys, icons_right, "pass" if passed else "FAIL"])
	print("hud_test: date %s shown as \"%s\"; year 3, season 1 as \"%s\": %s" % [
		date, season, later, "pass" if dated else "FAIL"])
	hud.free()
	var measure_passed := _measure_pass()
	var names_passed := _names_pass()
	var fit_passed := _fit_pass()
	var rumours_passed := _rumours_pass()
	var realm_passed := _realm_pass()
	var lessons_passed := _lessons_pass()
	var cast_words_passed := _cast_words_pass()
	return passed and measure_passed and names_passed and fit_passed and rumours_passed and realm_passed \
			and lessons_passed and cast_words_passed


# Born into a Cult village, the player challenges its leader: the leader panel and the duel panel
# name the leader as the adapter does, by surname and given name in Korean and by given name alone
# in English, and the challenger as the player. The seed's Cult leader's given name, Cheonmyeong, is
# wider than either label, and stays within it. Control: born into an Orthodox village, the leader
# panel names that faction's leader, whose name differs, so the name follows the person shown.
func _names_pass() -> bool:
	var world := WorldAdapter.create_region(LONG_NAME_SEED, 1)
	var steered := world.army_count() - 1
	var faction := world.army_faction(steered)
	var leader := world.leader(faction)
	var name := world.person_name(leader)
	var hud: CanvasLayer = load(SCENE).instantiate()
	hud.show_player(world, steered)
	var seat := Vector2i((world.person_position(leader) / WorldAdapter.CELL_SIZE).floor())
	var refusal := world.challenge_refusal(steered, faction)
	var ordered := refusal == WorldAdapter.CHALLENGE_NONE \
			and world.order(steered, seat, PackedInt64Array([0]), faction)
	hud.refresh(world, steered, PackedInt64Array([0]))
	var shown_labels: Array[Label] = [hud.labels.leader_value, hud.labels.duel_leader, hud.labels.duel_challenger]
	var given_width := _width(hud.labels.leader_value, name.get("given_en", ""))
	# Setup: the leader is someone else, named wider than the labels, and a duel with them is fought.
	var set_up: bool = leader >= 0 and not world.roster(steered).has(leader) and not name.is_empty() \
			and given_width > hud.labels.leader_value.size.x and given_width > hud.labels.duel_leader.size.x \
			and ordered and hud.duel_panel.visible
	var korean := shown_labels.map(func(label: Label) -> String: return label.text)
	hud.switch_locale()
	hud.refresh(world, steered, PackedInt64Array([0]))
	var english := shown_labels.map(func(label: Label) -> String: return label.text)
	# Within its label, and the leader's clear of the words on either side of it.
	var within := shown_labels.all(func(label: Label) -> bool: return _within(label, label.text))
	var leader_word := _width(hud.labels.leader, hud.labels.leader.atr(hud.labels.leader.text))
	var clear: bool = hud.labels.leader.position.x + leader_word <= hud.labels.leader_value.position.x \
			and hud.labels.leader_value.position.x + hud.labels.leader_value.size.x <= hud.labels.challenge_key.position.x
	hud.switch_locale()
	hud.free()
	var full: String = name.surname + name.given
	var named := korean == [full, full, "당신"] and english == [name.given_en, name.given_en, "You"]

	var orthodox := WorldAdapter.create_region(SEED, 0)
	var orthodox_steered := orthodox.army_count() - 1
	var other_leader := orthodox.leader(orthodox.army_faction(orthodox_steered))
	var other_name := orthodox.person_name(other_leader)
	var control_hud: CanvasLayer = load(SCENE).instantiate()
	control_hud.show_player(orthodox, orthodox_steered)
	control_hud.refresh(orthodox, orthodox_steered, PackedInt64Array([0]))
	var shown: String = control_hud.labels.leader_value.text
	control_hud.free()
	var other_full: String = other_name.get("surname", "") + other_name.get("given", "")
	var control := not other_name.is_empty() and other_full != full and shown == other_full

	var passed := set_up and named and within and clear and control
	print("hud_test: Cult leader %d named %s, its given name %d px wide; leader, duel leader and challenger shown %s in Korean, %s in English (setup %s), within their labels %s, the leader's clear of its neighbours %s; Orthodox leader %d shown \"%s\" for %s: %s" % [
		leader, name, given_width, korean, english, set_up, within, clear, other_leader, shown, other_full,
		"pass" if passed else "FAIL"])
	return passed


# In each locale and in both layouts, the keyboard's and the touch controls', every text a label
# the layout shows can show, measured in the HUD's font, fits the room its panel gives it: within the label where the label has a width of its own, clear of the label after it
# on its line, and clear of its panel's right edge. Every surname crossed with every given name in
# names(), as the leader and duel panels show it, fits each of their name labels or is cut short
# with an ellipsis, and in Korean fits whole. Setup: every key in hud.csv outside the panels' and
# screens' over the HUD is measured, and the widths are not zero.
func _fit_pass() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var hud: CanvasLayer = load(SCENE).instantiate()
	hud.show_player(world, world.army_count() - 1)
	var locales: Array = hud.LOCALES
	var measured := {}
	var overruns: Array[String] = []
	var spans := 0
	var names := 0
	for case in locales.size() * 2:
		var locale: String = locales[case % locales.size()]
		var touch_layout: bool = case >= locales.size()
		hud.use_touch(touch_layout)
		TranslationServer.set_locale(locale)
		var widest := {}
		var texts := {}
		for name: String in hud.labels:
			var label: Label = hud.labels[name]
			# The notice is hidden until it has something to say; any other label hidden is not
			# in this layout.
			if not label.visible and name != "notice":
				continue
			var shown := _texts(hud, name, label, measured)
			if shown.is_empty():
				continue
			texts[label] = shown
			widest[label] = 0.0
			for text: String in shown:
				widest[label] = maxf(widest[label], _width(label, text))
		# Each label's span on its line, by the panel it sits in and the line's height.
		var lines := {}
		for label: Label in widest:
			var x := label.position.x
			var y := label.position.y
			var parent := label.get_parent()
			if parent is BoxContainer:
				x = parent.position.x
				y = parent.position.y
				for sibling in parent.get_children():
					if sibling == label:
						break
					x += widest.get(sibling, 0.0) + parent.get_theme_constant("separation")
			# A label that aligns or trims its text holds a width of its own; any other is as wide as its text.
			var fixed := label.horizontal_alignment != HORIZONTAL_ALIGNMENT_LEFT \
					or label.text_overrun_behavior != TextServer.OVERRUN_NO_TRIMMING
			if fixed and widest[label] > label.size.x:
				overruns.append("%s %s \"%s\" is %d px in a %d px label" % [
					locale, label.text, texts[label], widest[label], label.size.x])
			var panel := parent if parent is Panel else parent.get_parent() as Panel
			var line := [panel, y]
			if not lines.has(line):
				lines[line] = []
			lines[line].append([x, x + (label.size.x if fixed else widest[label]), label.text if label.text != "" else str(texts[label])])
		for line: Array in lines:
			var panel: Panel = line[0]
			var edge: float = panel.size.x - INSET if panel != null else SCREEN_WIDTH - SCREEN_INSET
			var line_spans: Array = lines[line]
			line_spans.sort_custom(func(a: Array, b: Array) -> bool: return a[0] < b[0])
			for i in line_spans.size():
				spans += 1
				var limit: float = line_spans[i + 1][0] if i + 1 < line_spans.size() else edge
				if line_spans[i][1] > limit or line_spans[i][1] <= line_spans[i][0]:
					overruns.append("%s%s %s ends at %d past %d" % [locale, " touch" if touch_layout else "",
							line_spans[i][2], line_spans[i][1], limit])
		for name in _every_name(world):
			var text := PersonName.shown(name, true)
			names += 1
			for key: String in ["leader_value", "duel_challenger", "duel_leader"]:
				var label: Label = hud.labels[key]
				var whole := _width(label, text)
				if not _within(label, text) or (locale == "ko" and whole > label.size.x):
					overruns.append("%s %s is %d px in the %d px %s" % [locale, text, whole, label.size.x, key])
	TranslationServer.set_locale(locales[0])
	hud.use_touch(false)
	hud.free()
	var unmeasured: Array[String] = []
	var csv := FileAccess.open("res://hud/hud.csv", FileAccess.READ)
	csv.get_csv_line()
	while not csv.eof_reached():
		var key := csv.get_csv_line()[0]
		if key != "" and not PANEL_PREFIXES.any(func(prefix: String) -> bool: return key.begins_with(prefix)) \
				and not measured.has(key):
			unmeasured.append(key)
	var passed := unmeasured.is_empty() and overruns.is_empty() and names > 0
	print("hud_test: %d keys measured on %d label spans and %d names across %s in both layouts; unmeasured %s; overruns %s: %s" % [
		measured.size(), spans, names, locales, unmeasured, overruns, "pass" if passed else "FAIL"])
	return passed


# The texts label `name` can show in the current locale, recording the keys they come from in
# `measured`: the season at LATEST_YEAR in each season, the keys in SHOWN, or the key it holds.
func _texts(hud: CanvasLayer, name: String, label: Label, measured: Dictionary) -> Array:
	if name == "season":
		measured["HUD_SEASON_YEAR"] = true
		var seasons := []
		for season in 4:
			measured["SEASON_%d" % season] = true
			seasons.append(hud.season_text({"year": LATEST_YEAR, "season": season}))
		return seasons
	if name == "rumour_1" or name == "rumour_2":
		return []
	var keys: Array = SHOWN.get(name, [])
	if name == "notice":
		keys = keys + hud.CHALLENGE_KEYS.values() + hud.CAST_KEYS.values()
	elif keys.is_empty() and label.auto_translate_mode != Node.AUTO_TRANSLATE_MODE_DISABLED \
			and label.text != "":
		keys = [label.text]
	for key: String in keys:
		measured[key] = true
	return keys.map(func(key: String) -> String: return hud.tr(key))


# Every surname in `world`'s names() crossed with every given name, as person_name gives a name.
func _every_name(world: WorldAdapter) -> Array[Dictionary]:
	var table := world.names()
	var names: Array[Dictionary] = []
	for surname: Dictionary in table.surnames:
		for given: Dictionary in table.female + table.male:
			names.append({"surname": surname.korean, "given": given.korean,
					"surname_en": surname.romanised, "given_en": given.romanised})
	return names


# The width `text` takes in the font and size `label` draws with. Out of the scene tree, as these
# tests run, get_theme_font answers with the engine's fallback font, so the label's own override,
# or else its own theme, is read, as the tree reads them first.
func _width(label: Label, text: String) -> float:
	var font: Font = label.get_theme_font("font") if label.has_theme_font_override("font") \
			else label.theme.get_font("font", "Label")
	var size: int = label.get_theme_font_size("font_size") if label.has_theme_font_size_override("font_size") \
			else label.theme.get_font_size("font_size", "Label")
	return font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x


# Whether `label` shows `text` within its width: whole, or cut short with an ellipsis.
func _within(label: Label, text: String) -> bool:
	return _width(label, text) <= label.size.x or label.text_overrun_behavior == TextServer.OVERRUN_TRIM_ELLIPSIS


# The checks measure in the HUD's font, Galmuri9 at FONT_SIZE: an untrimmed name label one pixel
# narrower than a Korean name, as Galmuri9 draws it, fails it, and the same label exactly as wide
# passes it. The engine's fallback font draws that name narrower at that size and wider at its own,
# so measuring in it reverses one of the two.
func _measure_pass() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var hud: CanvasLayer = load(SCENE).instantiate()
	hud.show_player(world, world.army_count() - 1)
	var probe: Label = hud.labels.leader_value.duplicate()
	probe.text_overrun_behavior = TextServer.OVERRUN_NO_TRIMMING
	var text := "남궁설화"
	var galmuri: Font = load(hud.FONT)
	var drawn := galmuri.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, hud.FONT_SIZE).x
	probe.size.x = drawn - 1
	var narrow_refused := not _within(probe, text)
	probe.size.x = drawn
	var exact_accepted := _within(probe, text)
	# Setup: the probe draws untrimmed, in its own theme's Galmuri9 at FONT_SIZE.
	var set_up: bool = probe.text_overrun_behavior == TextServer.OVERRUN_NO_TRIMMING \
			and probe.theme.get_font("font", "Label").resource_path == hud.FONT \
			and probe.theme.get_font_size("font_size", "Label") == hud.FONT_SIZE and drawn > 0.0
	probe.free()
	hud.free()
	var passed := set_up and narrow_refused and exact_accepted
	print("hud_test: \"%s\" is %d px in Galmuri9; refused in a %d px untrimmed label %s, accepted in a %d px one %s (setup %s): %s" % [
		text, drawn, drawn - 1, narrow_refused, drawn, exact_accepted, set_up, "pass" if passed else "FAIL"])
	return passed


# Every reason the core refuses a cast, each WorldAdapter.CAST_* constant but CAST_NONE, is shown
# on the notice by its own key, which hud.csv words in each locale. Setup: the constants read from
# the adapter's class include CAST_NO_QI and CAST_OUT_OF_RANGE. Control: a key hud.csv lacks
# translates to itself, so a key translating to other text was worded by the file.
func _cast_words_pass() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var hud: CanvasLayer = load(SCENE).instantiate()
	hud.show_player(world, world.army_count() - 1)
	var refusals := {}
	for constant: String in ClassDB.class_get_integer_constant_list("WorldAdapter", true):
		if constant.begins_with("CAST_") and constant != "CAST_NONE":
			refusals[constant] = ClassDB.class_get_integer_constant("WorldAdapter", constant)
	var set_up: bool = refusals.has("CAST_NO_QI") and refusals.has("CAST_OUT_OF_RANGE")
	var unworded := []
	for constant: String in refusals:
		var refusal: int = refusals[constant]
		if not hud.CAST_KEYS.has(refusal):
			unworded.append(constant)
			continue
		hud.show_cast(refusal)
		var key: String = hud.labels.notice.text
		for locale: String in hud.LOCALES:
			TranslationServer.set_locale(locale)
			if key != constant or hud.tr(key) == key or hud.tr(key).is_empty():
				unworded.append("%s in %s" % [constant, locale])
	TranslationServer.set_locale(hud.LOCALES[0])
	var lacking: String = hud.tr("CAST_NOT_A_REASON")
	hud.free()
	var passed := set_up and unworded.is_empty() and lacking == "CAST_NOT_A_REASON"
	print("hud_test: %d cast refusals %s; unworded %s; a key hud.csv lacks reads \"%s\" (setup %s): %s" % [
		refusals.size(), refusals.keys(), unworded, lacking, set_up, "pass" if passed else "FAIL"])
	return passed


# The region lived RUMOUR_YEARS years of five-tick days: the rumour log shows the latest rumours
# that reach the player, newest first, each worded as rumour_text words it, and a person's birth
# or death names that person. Control: the same region as it begins has no rumour, and the log
# says none has come. In each locale, every rumour about villages, sects and leads, with every
# faction and sect name, fits its line whole; one naming a person fits whole in Korean, and in
# English is cut short with an ellipsis past its line, for every surname crossed with every given
# name. In the touch layout, whose lines are the mockup's 156 pixels, every rumour is whole or cut
# short with an ellipsis. Setup: every RUMOUR_ key is measured.
func _rumours_pass() -> bool:
	var world := WorldAdapter.create_region(SEED, 0, SHORT_DAY)
	var steered := world.army_count() - 1
	var hud: CanvasLayer = load(SCENE).instantiate()
	hud.show_player(world, steered)
	hud.refresh(world, steered, PackedInt64Array([0]))
	var none_shown: String = hud.labels.rumour_0.text
	var none_right: bool = world.rumours(world.roster(steered)[0]).is_empty() \
			and none_shown == hud.tr("HUD_RUMOR_NONE") and hud.labels.rumour_1.text == ""
	for tick in RUMOUR_YEARS * DAYS_PER_YEAR * SHORT_DAY:
		world.tick()
	hud.refresh(world, steered, PackedInt64Array([0]))
	var heard := world.rumours(world.roster(steered)[0])
	var shown := []
	var expected := []
	for line in hud.RUMOUR_LINES:
		shown.append(hud.labels["rumour_%d" % line].text)
		expected.append(hud.rumour_text(world, heard[line]) if line < heard.size() else "")
	var newest_first: bool = heard.size() >= 2 and heard[0].sequence > heard[1].sequence
	var named := true
	for event: Dictionary in heard:
		if event.kind == "born" or event.kind == "died":
			named = named and hud.rumour_text(world, event).contains(PersonName.of(world, event.person, true))
	var live_right: bool = heard.size() >= hud.RUMOUR_LINES and shown == expected and newest_first and named
	# Fits, in each locale.
	var label: Label = hud.labels.rumour_0
	var measured := {}
	var overruns: Array[String] = []
	var cut := 0
	var checks := 0
	var factions := world.faction_names()
	var touch_cut := 0
	for case in hud.LOCALES.size() * 2:
		var locale: String = hud.LOCALES[case % hud.LOCALES.size()]
		var touch_layout: bool = case >= hud.LOCALES.size()
		hud.use_touch(touch_layout)
		TranslationServer.set_locale(locale)
		var texts: Array[String] = []
		for owner in factions.size():
			texts.append(_said(hud, measured, "RUMOUR_FOUNDED", [_faction(hud, factions, owner)]))
			texts.append(_said(hud, measured, "RUMOUR_RUINED", [_faction(hud, factions, owner)]))
			for raider in factions.size():
				texts.append(_said(hud, measured, "RUMOUR_RAIDED",
						[_faction(hud, factions, raider), _faction(hud, factions, owner)]))
		for sect in world.sect_count():
			texts.append(_said(hud, measured, "RUMOUR_SECT_FELL",
					[hud.tr("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_"))]))
		for text in texts:
			checks += 1
			if touch_layout:
				touch_cut += 0 if _width(label, text) <= label.size.x else 1
			if not _within(label, text) or (_width(label, text) > label.size.x and not touch_layout):
				overruns.append("%s \"%s\" is %d px in %d" % [locale, text, _width(label, text), label.size.x])
		for name in _every_name(world):
			var person := PersonName.shown(name, true)
			for owner in factions.size():
				for key: String in ["RUMOUR_ELECTED", "RUMOUR_DUEL", "RUMOUR_BORN", "RUMOUR_DIED"]:
					var args := [person, _faction(hud, factions, owner)] if key in ["RUMOUR_ELECTED", "RUMOUR_DUEL"] else [person]
					var text := _said(hud, measured, key, args)
					checks += 1
					var whole := _width(label, text) <= label.size.x
					if touch_layout:
						touch_cut += 0 if whole else 1
					else:
						cut += 0 if whole else 1
					if not _within(label, text) or (locale == "ko" and not whole and not touch_layout):
						overruns.append("%s \"%s\" is %d px in %d" % [locale, text, _width(label, text), label.size.x])
	TranslationServer.set_locale(hud.LOCALES[0])
	hud.use_touch(false)
	hud.free()
	var unmeasured := ["RUMOUR_FOUNDED", "RUMOUR_RAIDED", "RUMOUR_RUINED", "RUMOUR_SECT_FELL", "RUMOUR_ELECTED",
			"RUMOUR_DUEL", "RUMOUR_BORN", "RUMOUR_DIED"].filter(func(key: String) -> bool: return not measured.has(key))
	var passed := none_right and live_right and unmeasured.is_empty() and overruns.is_empty()
	print("hud_test: a new region shows \"%s\" %s; after %d years %d rumours reach the player, the log showing %s against %s, newest first %s, births and deaths named %s; %d rumours measured in both layouts, %d cut short in English and %d in the touch layout's two lines; unmeasured %s; overruns %s: %s" % [
		none_shown, none_right, RUMOUR_YEARS, heard.size(), shown, expected, newest_first, named, checks, cut, touch_cut,
		unmeasured, overruns.slice(0, 3), "pass" if passed else "FAIL"])
	return passed


# A person the world starts with, cultivated for its founding age, shows its realm by name, Korean
# first and in English as romanised, and its qi against its store with their thousands grouped,
# its bar the share of the store it holds. Control: the player, newborn to the world, has no realm
# and shows a dash, and no qi in no store. In each locale, every realm's name and the qi at the
# highest realm's threshold, right-aligned on their lines, keep 4 pixels clear of their labels.
func _realm_pass() -> bool:
	TranslationServer.set_locale("ko")
	var world := WorldAdapter.create_region(SEED)
	var hud: CanvasLayer = load(SCENE).instantiate()
	var cultivated := -1
	for army in world.army_count():
		var roster := world.roster(army)
		if cultivated < 0 and not roster.is_empty() and world.person_realm(roster[0]) >= 1 \
				and world.person_qi_capacity(roster[0]) >= 1000:
			cultivated = army
	var person := world.roster(cultivated)[0] if cultivated >= 0 else -1
	var realms := world.realms()
	var korean := ""
	var english := ""
	var qi := ""
	var bar := -1.0
	if person >= 0:
		hud.show_player(world, cultivated)
		hud.refresh(world, cultivated, PackedInt64Array([0]))
		korean = hud.labels.realm_value.text
		qi = hud.labels.qi_value.text
		bar = hud.qi_bar.value
		hud.switch_locale()
		hud.refresh(world, cultivated, PackedInt64Array([0]))
		english = hud.labels.realm_value.text
		hud.switch_locale()
	var rank := world.person_realm(person) if person >= 0 else 0
	var store := world.person_qi_capacity(person) if person >= 0 else 0
	var held := world.person_qi(person) if person >= 0 else 0
	var shown_right: bool = person >= 0 and korean == realms[rank - 1].korean and english == realms[rank - 1].romanised \
			and qi == "%s / %s" % [Arts.grouped(held), Arts.grouped(store)] and qi.contains(",") \
			and is_equal_approx(bar, 100.0 * held / store)
	var steered := world.army_count() - 1
	hud.show_player(world, steered)
	hud.refresh(world, steered, PackedInt64Array([0]))
	var newborn: String = hud.labels.realm_value.text
	var newborn_qi: String = hud.labels.qi_value.text
	var player_right: bool = world.person_realm(world.roster(steered)[0]) == 0 and newborn == "—" and newborn_qi == "0 / 0"
	var overruns: Array[String] = []
	var checks := 0
	var top := Arts.grouped(realms.back().qi)
	for locale: String in hud.LOCALES:
		TranslationServer.set_locale(locale)
		for line: Array in [[hud.labels.realm, hud.labels.realm_value, range(1, realms.size() + 1).map(
				func(n: int) -> String: return Arts.realm_name(world, n))],
				[hud.labels.qi, hud.labels.qi_value, ["%s / %s" % [top, top]]]]:
			var label: Label = line[0]
			var value: Label = line[1]
			var room := value.size.x - _width(label, hud.tr(label.text)) - 4
			for text: String in line[2]:
				checks += 1
				if _width(value, text) > room:
					overruns.append("%s %s is %d px in %d" % [locale, text, _width(value, text), room])
	TranslationServer.set_locale(hud.LOCALES[0])
	hud.free()
	var passed: bool = shown_right and player_right and overruns.is_empty() and checks == 2 * (realms.size() + 1)
	print("hud_test: person %d of realm %d shown \"%s\", in English \"%s\", qi \"%s\" with its bar at %.1f; the newborn player shown \"%s\" and \"%s\"; %d values measured, overruns %s: %s" % [
		person, rank, korean, english, qi, bar, newborn, newborn_qi, checks, overruns, "pass" if passed else "FAIL"])
	return passed


# A region of five-tick days, played through by the HUD steering the first army whose person learns
# a skill or gains a realm while it does: each of that person's lessons and realms recorded after
# the HUD began following the world waits as a notice, in order, worded by how the skill was
# learned with its name, or with the realm's name. Control: the lessons and realms of everyone else
# recorded meanwhile wait as none. In each locale, every notice any art skill or realm can make fits
# the notice's width.
func _lessons_pass() -> bool:
	TranslationServer.set_locale("ko")
	var lessons := {
		WorldAdapter.LEARNED_BY_COMPREHENSION: "NOTICE_COMPREHENDED",
		WorldAdapter.LEARNED_BY_MANUAL: "NOTICE_READ",
		WorldAdapter.LEARNED_BY_TEACHING: "NOTICE_TAUGHT",
	}
	# A first run finds the army whose first person learns soonest, and runs on until someone else
	# learns too; the second, from the same seed, plays as long through the HUD.
	var probe := WorldAdapter.create_region(SEED, 0, 5)
	var army := -1
	var learner := -1
	var other_seen := false
	var ticks := 0
	while (army < 0 or not other_seen) and ticks < LESSON_TICKS:
		probe.tick()
		ticks += 1
		for event: Dictionary in probe.events(0):
			if event.tick != probe.tick_count() - 1 or not event.has("person"):
				continue
			for each in probe.army_count():
				var roster := probe.roster(each)
				if army < 0 and not roster.is_empty() and roster[0] == event.person:
					army = each
					learner = event.person
			other_seen = other_seen or (learner >= 0 and event.person != learner)
	var world := WorldAdapter.create_region(SEED, 0, 5)
	var hud: CanvasLayer = load(SCENE).instantiate()
	var expected: Array[String] = []
	var others := 0
	if army >= 0:
		hud.show_player(world, army)
		for i in ticks:
			world.tick()
			hud.refresh(world, army, PackedInt64Array([0]))
		for event: Dictionary in world.events(0):
			if not event.has("person"):
				continue
			if event.person != learner:
				others += 1
			elif event.kind == WorldAdapter.EVENT_LEARNED:
				expected.append(hud.tr(lessons[int(event.by)]) % Arts.named(world.arts()[event.art].skills[event.skill]))
			else:
				expected.append(hud.tr("NOTICE_REALM") % Arts.realm_name(world, event.rank))
	var waiting: Array[String] = hud.waiting_notices()
	var announced := army >= 0 and not expected.is_empty() and waiting == expected
	var overruns: Array[String] = []
	var checks := 0
	var notice: Label = hud.labels.notice
	for locale: String in hud.LOCALES:
		TranslationServer.set_locale(locale)
		var texts: Array[String] = []
		for art: Dictionary in world.arts():
			for skill: Dictionary in art.skills:
				for key: String in lessons.values():
					texts.append(hud.tr(key) % Arts.named(skill))
		for rank in range(1, world.realms().size() + 1):
			texts.append(hud.tr("NOTICE_REALM") % Arts.realm_name(world, rank))
		for text in texts:
			checks += 1
			if _width(notice, text) > notice.size.x or text.begins_with("NOTICE_"):
				overruns.append("%s %s is %d px in %d" % [locale, text, _width(notice, text), notice.size.x])
	TranslationServer.set_locale(hud.LOCALES[0])
	hud.free()
	var passed := announced and others > 0 and overruns.is_empty() and checks > 0
	print("hud_test: army %d's person %d learned within %d ticks; waiting %s against %s; %d lessons and realms of others announced none; %d notices measured, overruns %s: %s" % [
		army, learner, ticks, waiting, expected, others, checks, overruns, "pass" if passed else "FAIL"])
	return passed


# `key` in the current locale with `args`, noting it measured.
func _said(hud: CanvasLayer, measured: Dictionary, key: String, args: Array) -> String:
	measured[key] = true
	return hud.tr(key) % args


func _faction(hud: CanvasLayer, factions: PackedStringArray, index: int) -> String:
	return hud.tr("FACTION_" + factions[index].to_upper())
