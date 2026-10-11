# Mockup of the player's map, proposed for #168 by ADR 0073, drawn over play.tscn with the HUD's own
# panel frame, font and palette (hud/panel.gd), in each locale, at each window in RUNS:
# - minimap_<run>_<locale>.png: the HUD's map around the player, drawn from what the player has seen:
#   unseen cells in fog, seen cells' terrain, borders derived from the villages as last seen, and
#   each village as last seen, a village whose residents were gone when seen in stone.
# - today_<run>_<locale>.png: the HUD's map as main draws it today, the live world, for comparison.
# - window_<run>_<locale>.png: the map window N opens, the same image at the largest whole multiple
#   at which the whole map fits the panel, with the player's place, the season, and a legend.
# On a touch screen only the minimap is drawn: the window's touch button is deferred (scope,
# 2026-10-09).
# What was seen is computed here, for the mockup only, as the ADR has the core do it: a disc of
# MAP_SIGHT cells round the player's body each tick, and every village with a cell of its area in
# it remembered as it stands then. The player walks to the JOURNEY nearest villages, then the world
# lives AFTER_YEARS more out of sight, so the map shows villages as last seen, not as they are.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://map_mockup.gd -- <dir>
extends SceneTree

const Palette := preload("res://hud/palette.gd")
const Affiliation := preload("res://hud/affiliation.gd")
const SETTLE_FRAMES := 30
const SEED := 2026
const YEARS := 10
const AFTER_YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const MAP_SIGHT := 15
const CLAIM := 16
const JOURNEY := 3
const ARRIVED := 3
const LOCALES := ["ko", "en"]
const KEPT := "user://map_mockup.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false, "views": ["today", "minimap", "window"]},
	{"name": "pc_1280x720", "window": Vector2i(1280, 720), "touch": false, "views": ["minimap", "window"]},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true, "views": ["minimap"]},
]
const KIND_COLOURS := {
	"plain": Color("6f8f3f"), "forest": Color("3f6a32"), "hills": Color("8a7a4a"),
	"water": Color("3a6a8c"), "mountain": Color("6a6460"),
}
const FOG := Color("1d1a17")
const WORDS := {
	"MAP": ["지도", "Map"],
	"CLOSE": ["N 닫기", "N Close"],
	"KEYS": ["가 본 곳만 보이고, 마지막으로 본 모습으로 남습니다", "Only what you have seen, as you last saw it"],
	"YOU": ["나", "You"],
	"RUINS": ["폐허로 본 마을", "Seen in ruins"],
	"UNSEEN": ["가 보지 않은 곳", "Not yet seen"],
	"LEGEND": ["범례", "Legend"],
}
const ROOM := {"MAP": 120, "CLOSE": 80, "KEYS": 420, "YOU": 120, "RUINS": 120, "UNSEEN": 120, "LEGEND": 120}


## The proposed map window: the panels' frame, the map at the whole multiple that fits on the left,
## and on the right the season and a legend of the factions' colours, ruins, fog and the player.
class MapWindow extends "res://hud/panel.gd":
	const MARGIN := 10
	const LEGEND_WIDTH := 132
	var frame := Rect2()
	var map: Texture2D
	var scale_by := 1
	var player := Vector2.ZERO
	var words := {}
	var factions: Array = []
	var season := ""

	func _draw() -> void:
		if map == null:
			return
		_font = get_theme_default_font() if _font == null else _font
		_dim()
		var size := Vector2(map.get_size()) * scale_by
		frame = Rect2(Vector2.ZERO, Vector2(size.x + LEGEND_WIDTH + 3 * MARGIN, size.y + 2 * MARGIN + 42))
		frame.size.x = maxf(frame.size.x, _width(words.KEYS) + 24)
		frame.position = ((SCREEN - frame.size) / 2.0).floor()
		_dialog(frame, words.MAP, words.CLOSE, words.KEYS)
		var MAP_AT := frame.position + Vector2(MARGIN, 20 + MARGIN)
		var LEGEND_X := MAP_AT.x + size.x + MARGIN * 2
		draw_rect(Rect2(MAP_AT - Vector2.ONE, size + Vector2(2, 2)), Palette.GOLD_3, false)
		draw_texture_rect(map, Rect2(MAP_AT, size), false)
		var dot := MAP_AT + player * scale_by
		draw_rect(Rect2(dot - Vector2(2, 2), Vector2(5, 5)), Palette.INK_1)
		draw_rect(Rect2(dot - Vector2(1, 1), Vector2(3, 3)), Palette.GOLD_6)
		var y := MAP_AT.y
		_text(Vector2(LEGEND_X, y), season, Palette.GOLD_6)
		y += 22
		_text(Vector2(LEGEND_X, y), words.LEGEND, Palette.STONE_4)
		y += 16
		for faction: Dictionary in factions:
			_swatch(Vector2(LEGEND_X, y), faction.colour)
			_text(Vector2(LEGEND_X + 14, y), fit(faction.name, 120), Palette.PAPER_6)
			y += 14
		y += 4
		_swatch(Vector2(LEGEND_X, y), Palette.STONE_3)
		_text(Vector2(LEGEND_X + 14, y), words.RUINS, Palette.PAPER_6)
		y += 14
		_swatch(Vector2(LEGEND_X, y), FOG)
		_text(Vector2(LEGEND_X + 14, y), words.UNSEEN, Palette.PAPER_6)
		y += 14
		draw_rect(Rect2(Vector2(LEGEND_X + 2, y + 3), Vector2(5, 5)), Palette.INK_1)
		draw_rect(Rect2(Vector2(LEGEND_X + 3, y + 4), Vector2(3, 3)), Palette.GOLD_6)
		_text(Vector2(LEGEND_X + 14, y), words.YOU, Palette.PAPER_6)

	func _swatch(at: Vector2, colour: Color) -> void:
		draw_rect(Rect2(at + Vector2(1, 2), Vector2(8, 8)), Palette.INK_1)
		draw_rect(Rect2(at + Vector2(2, 3), Vector2(6, 6)), colour)

	func width_of(text: String) -> float:
		_font = get_theme_default_font() if _font == null else _font
		return _width(text)


var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _player := -1
var _seen := {}
# By village index: its faction, whether it had residents, its corner and area, when last seen.
var _remembered := {}
var _window: MapWindow
var _checked := false


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for run: Dictionary in RUNS:
			for view: String in run.views:
				_shots.append({"locale": locale, "run": run, "view": view})
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		_world.tick()
	_player = _world.player_army()
	_world.steer(_player, true)
	_chart()
	for village: int in _nearest(JOURNEY):
		_walk_to(village)
	var seen_before := _remembered.duplicate(true)
	for i in AFTER_YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		_world.tick()
	var changed := 0
	var villages := _world.villages()
	for index: int in seen_before:
		var now: Dictionary = villages[index]
		var live := {"faction": int(now.faction), "peopled": _world.village_population(index) > 0}
		if live.faction != seen_before[index].faction or live.peopled != seen_before[index].peopled:
			changed += 1
	print("map_mockup: %d of %d cells seen, %d of %d villages remembered, %d changed since out of sight" % [
			_seen.size(), _world.map_rows().size() * _world.map_rows()[0].length(), _remembered.size(),
			villages.size(), changed])
	_open()


func _cell(at: Vector2) -> Vector2i:
	return Vector2i((at / WorldAdapter.CELL_SIZE).floor())


func _body() -> Vector2i:
	var roster := _world.roster(_player)
	return _cell(_world.person_position(roster[0])) if not roster.is_empty() else Vector2i(-1, -1)


func _centre(village: Dictionary) -> Vector2i:
	return Vector2i(village.corner) + Vector2i(village.area) / 2


# The `count` villages nearest the player's body, nearest first, its home left out.
func _nearest(count: int) -> Array:
	var here := _body()
	var order := []
	var villages := _world.villages()
	for index in villages.size():
		var distance := Vector2(_centre(villages[index]) - here).length()
		if distance > 6:
			order.append([distance, index])
	order.sort()
	return order.slice(0, count).map(func(pair: Array) -> int: return pair[1])


func _walk_to(village: int) -> void:
	var target := _centre(_world.villages()[village])
	var skills := PackedInt64Array(range(_world.skill_count(_player)))
	for i in DAYS_PER_YEAR * TICKS_PER_DAY:
		if _world.roster(_player).is_empty():
			return
		if Vector2(_body() - target).length() <= ARRIVED:
			return
		if i % 20 == 0:
			_world.order(_player, target, skills)
		_world.tick()
		_chart()


# What the ADR has the core do at the end of every tick, done here for the mockup.
func _chart() -> void:
	var here := _body()
	if here.x < 0:
		return
	var rows := _world.map_rows()
	for y in range(maxi(here.y - MAP_SIGHT, 0), mini(here.y + MAP_SIGHT, rows.size() - 1) + 1):
		for x in range(maxi(here.x - MAP_SIGHT, 0), mini(here.x + MAP_SIGHT, rows[0].length() - 1) + 1):
			if (x - here.x) * (x - here.x) + (y - here.y) * (y - here.y) <= MAP_SIGHT * MAP_SIGHT:
				_seen[Vector2i(x, y)] = true
	var villages := _world.villages()
	for index in villages.size():
		var area := Rect2i(villages[index].corner, villages[index].area)
		var nearest := here.clamp(area.position, area.end - Vector2i.ONE)
		if Vector2(nearest - here).length() <= MAP_SIGHT:
			_remembered[index] = {"faction": int(villages[index].faction), "corner": area.position,
					"area": area.size, "peopled": _world.village_population(index) > 0}


# The map as the player last saw it: hud.gd's own drawing, from the remembered villages, with fog.
func _map_image() -> Image:
	var rows := _world.map_rows()
	var size := Vector2i(rows[0].length(), rows.size())
	var image := Image.create_empty(size.x, size.y, false, Image.FORMAT_RGBA8)
	var names := _world.terrain_names()
	var terrain := _world.terrain()
	var held := {}
	var nearest := {}
	for index: int in _remembered:
		var village: Dictionary = _remembered[index]
		if not village.peopled:
			continue
		var centre: Vector2i = village.corner + village.area / 2
		for y in range(maxi(centre.y - CLAIM, 0), mini(centre.y + CLAIM, size.y - 1) + 1):
			for x in range(maxi(centre.x - CLAIM, 0), mini(centre.x + CLAIM, size.x - 1) + 1):
				var d := (x - centre.x) * (x - centre.x) + (y - centre.y) * (y - centre.y)
				var cell := Vector2i(x, y)
				if d < nearest.get(cell, 1 << 30):
					nearest[cell] = d
					held[cell] = village.faction
	var accents := []
	for faction in _world.faction_names().size():
		accents.append(Palette.ACCENTS[Affiliation.sprite(_world, faction)])
	for y in size.y:
		for x in size.x:
			var cell := Vector2i(x, y)
			if not _seen.has(cell):
				image.set_pixel(x, y, FOG)
				continue
			var colour: Color = KIND_COLOURS.get(names[terrain[y * size.x + x]], KIND_COLOURS.plain)
			var mine: int = held.get(cell, -1)
			for next: Vector2i in [cell + Vector2i.RIGHT, cell + Vector2i.DOWN]:
				if next.x < size.x and next.y < size.y and held.get(next, -1) != mine:
					var faction: int = mine if mine >= 0 else held.get(next, -1)
					if faction >= 0:
						colour = accents[faction]
			image.set_pixel(x, y, colour)
	for index: int in _remembered:
		var village: Dictionary = _remembered[index]
		var colour: Color = accents[village.faction].lightened(0.3) if village.peopled else Palette.STONE_3
		image.fill_rect(Rect2i(village.corner, village.area), colour)
	return image


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	# A delta of 0 draws and streams the ground without ticking the world.
	if _step >= 2 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if shot.run.touch else "off"))
			DisplayServer.window_set_size(shot.run.window)
		1:
			_play._play(_world)
			_play.set_process(false)
			_play._process(0.0)
			hud.labels.notice.visible = false
			if shot.view != "today":
				var image := _map_image()
				hud.map_texture = ImageTexture.create_from_image(image)
				(hud._map.texture as AtlasTexture).atlas = hud.map_texture
			hud.refresh(_world, _player, PackedInt64Array())
			_window = MapWindow.new()
			_window.use_font(hud.arts._font)
			hud.add_child(_window)
			if not _checked:
				_check_rooms()
		2:
			var room: Vector2 = hud.room
			_window.position = Vector2(floorf((room.x - _window.SCREEN.x) / 2.0), floorf((room.y - _window.SCREEN.y) / 2.0))
			_window.map = hud.map_texture
			var fits := floori(minf((_window.PANEL.size.x - _window.LEGEND_WIDTH - 3 * _window.MARGIN) / hud.map_texture.get_width(),
					(_window.PANEL.size.y - 2 * _window.MARGIN - 42) / hud.map_texture.get_height()))
			_window.scale_by = maxi(fits, 1)
			_window.player = Vector2(_body())
			_window.season = hud.labels.season.text
			_window.words = {}
			for key: String in WORDS:
				_window.words[key] = _word(key)
			_window.factions = []
			for faction in _world.faction_names().size():
				_window.factions.append({"name": tr(_world.faction_names()[faction]),
						"colour": Palette.ACCENTS[Affiliation.sprite(_world, faction)]})
			_window.visible = shot.view == "window"
			_window.queue_redraw()
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("map_mockup: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _check_rooms() -> void:
	_checked = true
	for key: String in ROOM:
		for locale in LOCALES.size():
			var width := _window.width_of(WORDS[key][locale])
			if width > ROOM[key]:
				print("map_mockup: %s in %s is %d wide, over %d" % [key, LOCALES[locale], width, ROOM[key]])


func _word(key: String) -> String:
	return WORDS[key][LOCALES.find(TranslationServer.get_locale().left(2))]


func _save(shot: Dictionary) -> void:
	var path := "%s/%s_%s_%s.png" % [_dir, shot.view, shot.run.name, shot.locale]
	var image := root.get_viewport().get_texture().get_image()
	var error := image.save_png(path)
	print("map_mockup: %s %s" % [path, "written" if error == OK else "not written: %d" % error])
