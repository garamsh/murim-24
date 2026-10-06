# Checks that hud.tscn shows its words from the translations, Korean first, that switching the
# locale changes them to English, that the faction shown is the steered army's, that health shows
# its strength as a percentage of its starting strength and inner energy a dash over an empty
# bar, and that the technique bar shows one slot per skill with the icon of its shape.
extends RefCounted

const SCENE := "res://hud/hud.tscn"
const SEED := 2026


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
	var passed := keyed and korean == ["ko", "체력", "정파"] and switched == "en" \
			and english == ["en", "HP", "Orthodox"] and back == "ko" and values_right and icons_right
	print("hud_test: keys %s, %s; Korean %s; after switching to %s, %s; back to %s; HP %s, inner energy %s, its bar %d; %d skills shown with their icons %s: %s" % [
		hp.text, faction.text, korean, switched, english, back, hp_value, qi_value, hud.qi_bar.value, skills.size(),
		icons_right, "pass" if passed else "FAIL"])
	hud.free()
	return passed
