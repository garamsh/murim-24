# Checks that the map window draws the player's map at the largest whole multiple at which it and its
# legend fit the HUD's room, in a frame inside that room: at 1x in the native 480x270 room and at
# 3x in the 960x540 room of a 1920x1080 window, the frame inside the room in both; that the key it
# was opened by and Escape close it, and any other key does nothing; and that every word it draws
# from hud.csv, with every faction's name, fits the room it gives it, in each locale. Setup: the
# map shown is the world's own, of what the player has seen after a tick, so it is not blank.
extends RefCounted

const MapWindow := preload("res://hud/map_window.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")
const SEED := 2026
## The HUD's room on a 1920x1080 window, at its 2x, and the native one.
const WIDE_ROOM := Vector2(960, 540)


func run() -> bool:
	var world := WorldAdapter.create_region(SEED)
	world.tick()
	var map := ImageTexture.create_from_image(_seen_image(world))
	var window: Control = MapWindow.new()
	window.toggle_key = KEY_N
	window.show_map(world, map, Vector2(4, 4), "봄 · 1년")
	var set_up: bool = window.visible and world.explored().has(1) and map.get_width() > 0
	var native_scale: int = window.map_scale()
	var native_inside := Rect2(Vector2.ZERO, MapWindow.SCREEN).encloses(window.frame())
	window.position = ((WIDE_ROOM - MapWindow.SCREEN) / 2.0).floor()
	var wide_scale: int = window.map_scale()
	var wide_inside := Rect2(-window.position, WIDE_ROOM).encloses(window.frame())
	var keys := [window.key(_key(KEY_N)), window.key(_key(KEY_ESCAPE)), window.key(_key(KEY_1))]
	var keys_right := keys == ["close", "close", ""]
	var passed := set_up and native_scale == 1 and wide_scale == 3 and native_inside and wide_inside \
			and keys_right
	print("map_window_test: a %dx%d map at %dx in the native room and %dx in %s, the frame inside both %s %s; N, Escape and 1 give %s (setup %s): %s" % [
		map.get_width(), map.get_height(), native_scale, wide_scale, WIDE_ROOM, native_inside, wide_inside,
		keys, set_up, "pass" if passed else "FAIL"])
	var fitted := _fit_pass(world, window)
	window.free()
	return passed and fitted


# In each locale: the title clear of the close key across the frame; the keys along its foot within
# the frame; the season, the legend's heading, each faction's name and each legend row within the
# legend. Setup: every MAP_ key is measured.
func _fit_pass(world: WorldAdapter, window: Control) -> bool:
	var fit := WordFit.new()
	var frame: Rect2 = window.frame()
	var legend: float = MapWindow.LEGEND_WIDTH - MapWindow.SWATCH
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		var title := fit.say("MAP_TITLE")
		var close := fit.say("MAP_CLOSE")
		fit.check(title, 8 + fit.width(title) + 10 + fit.width(close) + 8, frame.size.x)
		var keys := fit.say("MAP_KEYS")
		fit.check(keys, 8 + fit.width(keys), frame.size.x - 8)
		var heading := fit.say("MAP_LEGEND")
		fit.check(heading, fit.width(heading), MapWindow.LEGEND_WIDTH)
		for key: String in ["MAP_RUINS", "MAP_UNSEEN", "MAP_YOU"]:
			var row := fit.say(key)
			fit.check(row, fit.width(row), legend)
		for faction: String in world.faction_names():
			var name := fit.say("FACTION_" + faction.to_upper())
			fit.check(name, fit.width(name), legend)
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["MAP_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("map_window_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed


# A pixel a cell, lit where the player has seen the cell.
func _seen_image(world: WorldAdapter) -> Image:
	var rows := world.map_rows()
	var image := Image.create_empty(rows[0].length(), rows.size(), false, Image.FORMAT_RGBA8)
	var seen := world.explored()
	for i in seen.size():
		image.set_pixel(i % rows[0].length(), i / rows[0].length(), Color.WHITE if seen[i] == 1 else Color.BLACK)
	return image


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
