# Mockup of a PC button bar for every panel and screen a key opens, proposed for #186, drawn over
# play.tscn with the HUD's own panel frame, font and palette (hud/panel.gd), in each locale, at
# 1920x1080, the HUD at 2x in 960x540. Each option places the same entries, genealogy (G), sect (M),
# factions (F), arts (K), settings and pause (Esc), with the technique bar and the Z/X ware belt in
# view; the settings button under the map goes, its entry standing in for it:
# - corner_<view>_<locale>.png: a row of cells, each its key and its icon, anchored to the room's
#   bottom-right corner on the technique bar's line.
# - beside_<view>_<locale>.png: the same row left of the technique bar, so the foot reads buttons,
#   techniques, wares.
# - under_map_<view>_<locale>.png: the cells in a grid of three by two under the map, where the
#   settings button stood.
# - rail_<view>_<locale>.png: a labelled column, each row its icon, its word and its key, at the
#   right edge under the map.
# - corner_map_<view>_<locale>.png: the corner row with the map's entry reserved for #168, greyed.
# The views: `hover`, the cursor on the factions entry, which shows its name and key on a slip; and
# `open`, the genealogy panel open, its entry lit in jade over the panel's dimmed room.
# The icons of genealogy, sect, factions and arts are the touch buttons' (hud/touch.gd); those of
# settings, pause and the map are the mockup's own, held in ICONS. Words are hud.csv's.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://button_bar.gd -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const Touch := preload("res://hud/touch.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const WINDOW := Vector2i(1920, 1080)
const LOCALES := ["ko", "en"]
const KEPT := "user://button_bar_mockup.cfg"
const OPTIONS := ["corner", "beside", "under_map", "rail", "corner_map"]
const VIEWS := ["hover", "open"]
## The entries, as the table would hold them: what each does, its key's word, its icon and its
## hud.csv word.
const ENTRIES: Array[Dictionary] = [
	{"action": "genealogy", "key": "G", "icon": "genealogy", "label": "CONTROLS_GENEALOGY"},
	{"action": "sect", "key": "M", "icon": "sect", "label": "CONTROLS_SECTS"},
	{"action": "factions", "key": "F", "icon": "factions", "label": "CONTROLS_FACTIONS"},
	{"action": "arts", "key": "K", "icon": "arts", "label": "CONTROLS_ARTS"},
	{"action": "settings", "key": "", "icon": "settings", "label": "SETTINGS_TITLE"},
	{"action": "pause", "key": "Esc", "icon": "pause", "label": "CONTROLS_PAUSE_CANCEL"},
]
const MAP_ENTRY := {"action": "map", "key": "", "icon": "map", "label": "MAP", "off": true}
# The mockup's own 9x9 icons, in touch.gd's letters: a gear, two bars, a folded map.
const ICONS := {
	"settings": ["....#....", ".#.###.#.", "..#####..", ".##...##.", "###...###", ".##...##.",
			"..#####..", ".#.###.#.", "....#...."],
	"pause": [".........", ".##...##.", ".##...##.", ".##...##.", ".##...##.", ".##...##.",
			".##...##.", ".##...##.", "........."],
	"map": ["##..##..#", "#.##.##.#", "#..#..#.#", "#.##..#.#", "#..#.##.#", "#.##..#.#",
			"#..#..#.#", "#.##.##.#", "##..##..#"],
}
## The map's word, which hud.csv lacks yet.
const MAP_WORDS := {"ko": "지도", "en": "Map"}


## The proposed bar, drawn as each option lays it out, in the HUD's pixels.
class Bar extends "res://hud/panel.gd":
	const CELL_HEIGHT := 20
	const STEP_GAP := 2
	const RAIL_ROW := 16
	var option := "corner"
	var entries: Array[Dictionary] = []
	## Where the bar's frame stands, set by the mockup from the HUD's layout.
	var at := Vector2.ZERO
	var hovered := -1
	var active := ""
	var map_word := ""

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE

	func cell_width() -> float:
		_font = get_theme_default_font() if _font == null else _font
		var widest := 0.0
		for entry: Dictionary in entries:
			widest = maxf(widest, _width(entry.key))
		# The key, a pixel's gap and the 9-pixel icon, inside a 2-pixel margin each side.
		return widest + 2 + 9 + 6

	func columns() -> int:
		return 3 if option == "under_map" else entries.size()

	func frame_size() -> Vector2:
		if option == "rail":
			return Vector2(rail_width(), entries.size() * RAIL_ROW + 6)
		var rows := ceili(float(entries.size()) / columns())
		return Vector2(columns() * (cell_width() + STEP_GAP) - STEP_GAP + 8, rows * (CELL_HEIGHT + STEP_GAP) - STEP_GAP + 6)

	func rail_width() -> float:
		var widest := 0.0
		for entry: Dictionary in entries:
			widest = maxf(widest, _width(word(entry)) + _width(entry.key))
		return widest + 9 + 4 + 8 + 10

	func word(entry: Dictionary) -> String:
		return map_word if entry.action == "map" else tr(entry.label)

	func cell(n: int) -> Rect2:
		if option == "rail":
			return Rect2(at + Vector2(3, 3 + n * RAIL_ROW), Vector2(rail_width() - 6, RAIL_ROW))
		var w := cell_width()
		return Rect2(at + Vector2(4 + (n % columns()) * (w + STEP_GAP), 3 + (n / columns()) * (CELL_HEIGHT + STEP_GAP)),
				Vector2(w, CELL_HEIGHT))

	func _draw() -> void:
		_font = get_theme_default_font() if _font == null else _font
		_frame(Rect2(at, frame_size()), Palette.INK_2)
		for n in entries.size():
			var entry := entries[n]
			var c := cell(n)
			var off: bool = entry.get("off", false)
			var lit: bool = entry.action == active
			var over := n == hovered and not off
			if option == "rail":
				if lit or over:
					draw_rect(c, Palette.INK_1)
					draw_rect(Rect2(c.position, Vector2(1, c.size.y)), Palette.JADE_4 if lit else Palette.GOLD_4)
				_icon(entry.icon, c.position + Vector2(4, 3), off, lit)
				_text(c.position + Vector2(17, 2), word(entry), Palette.STONE_3 if off else (Palette.JADE_7 if lit else Palette.PAPER_6))
				_right(c.end.x - 3, c.position.y + 2, entry.key, Palette.STONE_3 if off else Palette.GOLD_4)
				continue
			draw_rect(c, Palette.JADE_4 if lit else (Palette.GOLD_4 if over else Palette.INK_2))
			draw_rect(c.grow(-1), Palette.JADE_2 if lit else Palette.INK_1)
			var key_colour := Palette.STONE_3 if off else (Palette.PAPER_6 if lit else Palette.GOLD_4)
			_text(c.position + Vector2(3, 4), entry.key, key_colour)
			_icon(entry.icon, Vector2(c.end.x - 3 - 9, c.position.y + 5), off, lit)
		if hovered >= 0 and option != "rail":
			_hint(hovered)

	func _hint(n: int) -> void:
		var entry := entries[n]
		var text := word(entry) if entry.key.is_empty() else "%s (%s)" % [word(entry), entry.key]
		var c := cell(n)
		var w := _width(text) + 8
		var room: Vector2 = get_parent().room
		var left := clampf(c.position.x + floorf((c.size.x - w) / 2.0), 2, room.x - 2 - w)
		var top := c.position.y - 3 - 14 - 3
		# Over the cell on a bar at the foot; left of the grid under the map, on the cell's row.
		if option == "under_map":
			left = at.x - 3 - w
			top = c.position.y + 3
		draw_rect(Rect2(left, top, w, 14), Palette.INK_1)
		draw_rect(Rect2(left, top, w, 1), Palette.GOLD_4)
		_text(Vector2(left + 4, top + 1), text, Palette.PAPER_6)

	func _icon(name: String, top_left: Vector2, off: bool, lit: bool) -> void:
		var rows: Array = Touch.BUTTON_ICONS[name] if Touch.BUTTON_ICONS.has(name) else ICONS[name]
		var colour := Palette.STONE_3 if off else (Palette.JADE_7 if lit else (Palette.PAPER_6 if name == "arts" else Palette.GOLD_4))
		for y in rows.size():
			var row: String = rows[y]
			for x in row.length():
				if row[x] == ".":
					continue
				var ink := colour
				if not off and row[x] == "p":
					ink = Palette.LACQUER_5
				elif not off and row[x] == "l":
					ink = Palette.GOLD_4
				draw_rect(Rect2(top_left + Vector2(x, y), Vector2.ONE), ink)


var _dir := "."
var _frame_count := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _bar: Bar


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for option: String in OPTIONS:
			for view: String in VIEWS:
				_shots.append({"locale": locale, "option": option, "view": view})
	_world = WorldAdapter.create_region(SEED, 0)
	_open()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame_count += 1
	if _frame_count % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(WINDOW)
		1:
			_play._play(_world)
			_play._process(0.0)
			_play.set_process(false)
			hud.labels.notice.visible = false
			hud.settings_button.visible = false
			if shot.view == "open":
				_play.open_panel(_play.GENEALOGY_KEY)
			_bar = Bar.new()
			_bar.use_font(hud.arts._font)
			_bar.map_word = MAP_WORDS[shot.locale]
			hud.add_child(_bar)
			# Over the panels, under the screens.
			hud.move_child(_bar, hud.pause.get_index())
		2:
			_compose(hud, shot)
		3:
			_save(shot, hud)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("button_bar: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


# Lays the bar out as `shot`'s option places it in the HUD's room.
func _compose(hud: CanvasLayer, shot: Dictionary) -> void:
	var option: String = shot.option
	_bar.option = "corner" if option == "corner_map" else option
	_bar.entries = ENTRIES.duplicate()
	if option == "corner_map":
		_bar.entries.insert(0, MAP_ENTRY)
	var room: Vector2 = hud.room
	var bar: Control = hud._skill_bar
	var size := _bar.frame_size()
	var map: Control = hud._map_panel
	match _bar.option:
		"corner":
			_bar.at = Vector2(room.x - 4 - size.x, bar.position.y)
		"beside":
			_bar.at = Vector2(bar.position.x - 4 - size.x, bar.position.y)
		"under_map":
			_bar.at = Vector2(map.position.x + map.size.x - size.x, map.position.y + map.size.y + 4)
		"rail":
			_bar.at = Vector2(room.x - 4 - size.x, map.position.y + map.size.y + 4)
	_bar.position = Vector2.ZERO
	_bar.size = room
	_bar.hovered = _bar.entries.find(ENTRIES[2]) if shot.view == "hover" else -1
	_bar.active = "genealogy" if shot.view == "open" else ""
	_bar.queue_redraw()
	var frame := Rect2(_bar.at, size)
	var centre := Rect2(room / 2.0 - Vector2(80, 45), Vector2(160, 90))
	var belt: Rect2 = hud.purse.belt_rect()
	print("button_bar: %s %s %s frame %s; clear of the technique bar %s, the belt %s, the map %s, the centre %s, inside the room %s" % [
			shot.locale, option, shot.view, frame, not frame.intersects(Rect2(bar.position, bar.size)),
			not frame.intersects(belt), not frame.intersects(Rect2(map.position, map.size)),
			not frame.intersects(centre), Rect2(Vector2.ZERO, room).encloses(frame)])


func _save(shot: Dictionary, _hud: CanvasLayer) -> void:
	var name: String = "%s_%s_%s" % [shot.option, shot.view, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("button_bar: %s.png %dx%d, HUD %dx in %s" % [name, image.get_width(), image.get_height(),
			ViewScale.hud_scale(WINDOW, false), ViewScale.hud_room(WINDOW, false)])
