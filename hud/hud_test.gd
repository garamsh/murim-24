# Checks that hud.tscn shows its words from the translations, Korean first, that switching the
# locale changes them to English, that the faction shown is the steered army's, that health shows
# its strength as a percentage of its starting strength and inner energy a dash over an empty
# bar, that the technique bar shows one slot per skill with the icon of its shape, and that the
# season and year are the core's. It also checks that the leader and duel panels name people by
# the adapter's person_name, Korean first and romanised in English.
extends RefCounted

const SCENE := "res://hud/hud.tscn"
const SEED := 2026
# A seed whose first Cult village's leader has the widest given name, Cheonmyeong.
const LONG_NAME_SEED := 11


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
	var values_right: bool = hp_value == "%d%%" % percent and qi_value == "—" \
			and hud.qi_bar.value == 0.0 and hud.hp_bar.value == 100.0 * strength / world.army_strength(steered)
	# One slot per skill, each with the icon of its shape; control: the slots past them are hidden.
	var skills := world.skills(steered)
	var icons_right := skills.size() > 0
	for slot in hud.slot_icons.size():
		var icon: TextureRect = hud.slot_icons[slot]
		var visible: bool = icon.get_parent().visible
		if slot < skills.size():
			var burst := int(skills[slot].radius) > 0
			var region := (icon.texture as AtlasTexture).region
			icons_right = icons_right and visible and region.position.x == (16 if burst else 0)
		else:
			icons_right = icons_right and not visible
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
	print("hud_test: keys %s, %s; Korean %s; after switching to %s, %s; back to %s; HP %s, inner energy %s, its bar %d; %d skills shown with their icons %s: %s" % [
		hp.text, faction.text, korean, switched, english, back, hp_value, qi_value, hud.qi_bar.value, skills.size(),
		icons_right, "pass" if passed else "FAIL"])
	print("hud_test: date %s shown as \"%s\"; year 3, season 1 as \"%s\": %s" % [
		date, season, later, "pass" if dated else "FAIL"])
	hud.free()
	return passed and _names_pass()


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
	var font: Font = hud.labels.leader_value.get_theme_font("font")
	var given_width := font.get_string_size(name.get("given_en", ""), HORIZONTAL_ALIGNMENT_LEFT, -1,
			hud.FONT_SIZE).x
	# Setup: the leader is someone else, named wider than the labels, and a duel with them is fought.
	var set_up: bool = leader >= 0 and not world.roster(steered).has(leader) and not name.is_empty() \
			and given_width > hud.labels.leader_value.size.x and given_width > hud.labels.duel_leader.size.x \
			and ordered and hud.duel_panel.visible
	var korean := shown_labels.map(func(label: Label) -> String: return label.text)
	hud.switch_locale()
	hud.refresh(world, steered, PackedInt64Array([0]))
	var english := shown_labels.map(func(label: Label) -> String: return label.text)
	# Within its label: the label asks for no more than its width, and the leader's clears the
	# words on either side of it.
	var within := shown_labels.all(func(label: Label) -> bool: return label.get_minimum_size().x <= label.size.x)
	var leader_word := font.get_string_size(hud.labels.leader.atr(hud.labels.leader.text),
			HORIZONTAL_ALIGNMENT_LEFT, -1, hud.FONT_SIZE).x
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
