# Checks that hud.tscn shows its words from the translations, Korean first, that switching the
# locale changes them to English, and that the faction shown is the steered army's.
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
	var passed := keyed and korean == ["ko", "체력", "정파"] and switched == "en" \
			and english == ["en", "HP", "Orthodox"] and back == "ko"
	print("hud_test: keys %s, %s; Korean %s; after switching to %s, %s; back to %s: %s" % [
		hp.text, faction.text, korean, switched, english, back, "pass" if passed else "FAIL"])
	hud.free()
	return passed
