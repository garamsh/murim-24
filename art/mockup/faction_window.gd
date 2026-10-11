# Mockup of the faction window proposed for #165, drawn over play.tscn with the HUD's own panel
# frame, font and palette (hud/panel.gd), in each locale, at each window in RUNS:
# - hud_<run>_<locale>.png: the HUD without the "C 도전" line; on a touch screen the 도전 button
#   becomes a 세력 button.
# - leader_<run>_<locale>.png: the window as its faction's leader sees it, the faction at war
#   chosen: making peace offered, declaring war greyed with its refusal.
# - confirm_<run>_<locale>.png: the leader has chosen an ally and asked to declare war on it; the
#   confirmation stands over the window.
# - member_<run>_<locale>.png: the window as a member of a faction whose lead passes by duel sees
#   it: no orders, and the challenge to the leader offered.
# - refused_<run>_<locale>.png: a member of a faction that elects its leader has pressed C, and
#   the core's refusal (WorldAdapter.challenge_refusal) shows its word.
# Relations are the core's hostility (WorldAdapter.hostile) both ways, ally or hostile, with two
# declarations the mockup assumes: the Orthodox at war with the Demonic Cult, and the Unorthodox
# at peace with it. Leaders, people and villages are the world's. The words are the mockup's own,
# held in WORDS, and hud.csv's where it has them; the script prints any word that overruns its room.
# The world is SEED's region with days of five ticks, lived YEARS years, as evidence/pr-176 does.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://faction_window.gd -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const PersonName := preload("res://hud/person_name.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const LOCALES := ["ko", "en"]
const KEPT := "user://faction_window_mockup.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true},
	{"name": "touch_2400x1080", "window": Vector2i(2400, 1080), "touch": true},
]
const VIEWS := ["hud", "leader", "confirm", "member", "refused"]
# The declarations the mockup assumes, by faction name, both ways.
const DECLARED := {"Orthodox|Demonic": "war", "Unorthodox|Demonic": "peace"}

const WORDS := {
	"TITLE": ["세력", "Factions"],
	"CLOSE": ["F 닫기", "F Close"],
	"CLOSE_TOUCH": ["닫기", "Close"],
	"FACTION": ["세력", "Faction"],
	"RELATION": ["관계", "Relation"],
	"LEADER": ["수장", "Leader"],
	"PEOPLE": ["인원", "People"],
	"VILLAGES": ["마을", "Villages"],
	"SINCE": ["근거", "Basis"],
	"STRENGTH": ["%d명 · 마을 %d", "%d people · %d villages"],
	"OURS": ["우리 세력", "Ours"],
	"YOU_LEAD": ["당신이 수장이다", "You lead"],
	"ally": ["동맹", "Ally"],
	"neutral": ["중립", "Neutral"],
	"hostile": ["적대", "Hostile"],
	"war": ["전쟁", "At war"],
	"ABOUT_ally": ["성향이 맞아 서로 공격하지 않는다.", "Friends by stance; neither attacks."],
	"ABOUT_neutral": ["화평을 맺어 서로 공격하지 않는다.", "Peace made; neither attacks."],
	"ABOUT_hostile": ["성향이 맞서 서로 공격하고 약탈한다.", "Foes by stance; both fight and raid."],
	"ABOUT_war": ["전쟁 중이라 서로 공격하고 약탈한다.", "At war; both fight and raid."],
	"LEGEND_ally": ["성향이 맞음", "friendly stance"],
	"LEGEND_neutral": ["화평을 맺음", "peace made"],
	"LEGEND_hostile": ["성향이 맞섬", "hostile stance"],
	"LEGEND_war": ["전쟁을 선포함", "war declared"],
	"BY_STANCE": ["성향", "Stance"],
	"DECLARED_BY": ["%s 수장이 %s에 선포", "Declared by the %s leader, %s"],
	"DECLARE": ["전쟁 선포", "Declare war"],
	"PEACE": ["화평", "Make peace"],
	"REFUSED_AT_WAR": ["이미 전쟁 중이다.", "Already at war."],
	"REFUSED_AT_PEACE": ["이미 서로 공격하지 않는다.", "Already at peace."],
	"LEADER_ONLY": ["전쟁과 화평은 수장만 정한다.", "Only the leader makes war or peace."],
	"CHALLENGE": ["C 수장에게 도전", "C Challenge the leader"],
	"CHALLENGE_TOUCH": ["수장에게 도전", "Challenge the leader"],
	"KEYS_LEADER": ["방향키 고르기 · Enter 명령 · F 닫기", "Arrows choose · Enter order · F close"],
	"KEYS_MEMBER": ["방향키 세력 고르기 · C 도전 · F 닫기", "Arrows choose · C challenge · F close"],
	"KEYS_TOUCH_LEADER": ["세력을 누른 뒤 명령을 누른다", "Tap a faction, then an order"],
	"KEYS_TOUCH_MEMBER": ["세력을 눌러 관계를 본다", "Tap a faction to see where it stands"],
	"CONFIRM_TITLE": ["전쟁 선포", "Declare war"],
	"CONFIRM_LINE": ["%s에 전쟁을 선포한다.", "Declare war on the %s."],
	"CONFIRM_ALLY": ["동맹이 깨지고 서로 공격하고 약탈한다.", "The alliance ends; both fight and raid."],
	"CONFIRM_YES": ["Enter 선포", "Enter Declare"],
	"CONFIRM_NO": ["Esc 취소", "Esc Cancel"],
	"CONFIRM_YES_TOUCH": ["선포", "Declare"],
	"CONFIRM_NO_TOUCH": ["취소", "Cancel"],
	"BUTTON": ["세력", "Faction"],
}
# The room each word is given, for the overrun check.
const ROOM := {"TITLE": 80, "CLOSE": 60, "CLOSE_TOUCH": 60, "FACTION": 70, "RELATION": 60, "LEADER": 80,
		"PEOPLE": 40, "VILLAGES": 40, "SINCE": 76, "OURS": 60, "YOU_LEAD": 100, "ally": 50, "neutral": 50,
		"hostile": 50, "war": 50, "ABOUT_ally": 268, "ABOUT_neutral": 268, "ABOUT_hostile": 268,
		"ABOUT_war": 268, "LEGEND_ally": 88, "LEGEND_neutral": 88, "LEGEND_hostile": 88,
		"LEGEND_war": 88, "STRENGTH": 124, "BY_STANCE": 76, "DECLARE": 90, "PEACE": 90, "REFUSED_AT_WAR": 130,
		"REFUSED_AT_PEACE": 130, "LEADER_ONLY": 268, "CHALLENGE": 130, "CHALLENGE_TOUCH": 130,
		"KEYS_LEADER": 420, "KEYS_MEMBER": 420, "KEYS_TOUCH_LEADER": 420, "KEYS_TOUCH_MEMBER": 420,
		"CONFIRM_TITLE": 200, "CONFIRM_ALLY": 260, "CONFIRM_YES": 100, "CONFIRM_NO": 100,
		"CONFIRM_YES_TOUCH": 100, "CONFIRM_NO_TOUCH": 100, "BUTTON": 36}


## The proposed window, drawn from what `view` holds, in the native 480x270 the other panels are
## laid out in: on PC a table of factions over the chosen one's detail and a legend; on a touch
## screen the factions as rows a finger presses, 42 pixels tall, beside the chosen one's detail.
class FactionWindow extends "res://hud/panel.gd":
	# PC layout.
	const NAME_X := 32
	const RELATION_X := 112
	const LEADER_X := 178
	const PEOPLE_RIGHT := 314
	const VILLAGES_RIGHT := 358
	const SINCE_X := 372
	const OWN_Y := 34
	const TITLES_Y := 52
	const ROWS_Y := 68
	const ROW := 18
	const DETAIL_X := 32
	const DIVIDER_X := 310
	const LEGEND_X := 318
	# Touch layout.
	const TOUCH_ROWS := Rect2(26, 52, 230, 0)
	const TOUCH_ROW := 44
	const TOUCH_DETAIL := Rect2(270, 52, 182, 0)
	const TOUCH_BUTTON := 42
	const TOUCH_BUTTONS_Y := 202
	const AMBER_DARK := Color("6b4310")
	const CRIMSON := Color("9c1f24")
	const CRIMSON_LIGHT := Color("d63a2f")
	const RELATION_FILL := {"ally": Palette.JADE_2, "neutral": Palette.INK_5, "hostile": AMBER_DARK, "war": CRIMSON}
	const LINE := 14

	## What the window shows: `words`, `own` (`name`, `colour`, `leader`, `you_lead`), `rows`
	## (`name`, `colour`, `relation`, `leader`, `people`, `villages`, `since`, and `declared` for a
	## declared relation), `chosen`, `leader_view`, `orders` ([word, refusal] each), `order`, the
	## lit one, `refusal`, the challenge's refusal word, and `confirm`, its `line`.
	var view := {}
	var touch_layout := false

	func w(key: String) -> String:
		return view.words[key]

	func _draw() -> void:
		if view.is_empty():
			return
		_dim()
		if touch_layout:
			_draw_touch()
		else:
			_draw_pc()
		if not view.confirm.is_empty():
			_draw_confirm()

	func _draw_pc() -> void:
		_dialog(PANEL, w("TITLE"), w("CLOSE"), w("KEYS_LEADER" if view.leader_view else "KEYS_MEMBER"))
		_draw_own(Vector2(NAME_X, OWN_Y))
		_text(Vector2(NAME_X, TITLES_Y), w("FACTION"), Palette.STONE_4)
		_text(Vector2(RELATION_X, TITLES_Y), w("RELATION"), Palette.STONE_4)
		_text(Vector2(LEADER_X, TITLES_Y), w("LEADER"), Palette.STONE_4)
		_right(PEOPLE_RIGHT, TITLES_Y, w("PEOPLE"), Palette.STONE_4)
		_right(VILLAGES_RIGHT, TITLES_Y, w("VILLAGES"), Palette.STONE_4)
		_text(Vector2(SINCE_X, TITLES_Y), w("SINCE"), Palette.STONE_4)
		var rows: Array = view.rows
		for n in rows.size():
			var row: Dictionary = rows[n]
			var y: int = ROWS_Y + n * ROW
			_row_back(Rect2(PANEL.position.x + 6, y, PANEL.size.x - 12, ROW - 1), n)
			_text(Vector2(NAME_X + 6, y + 2), row.name, row.colour)
			if row.relation == "own":
				_text(Vector2(RELATION_X, y + 2), w("OURS"), Palette.GOLD_6)
			else:
				_relation_tag(Vector2(RELATION_X, y + 3), row.relation)
			_text(Vector2(LEADER_X, y + 2), fit(row.leader, PEOPLE_RIGHT - 40 - LEADER_X), Palette.PAPER_6)
			_right(PEOPLE_RIGHT, y + 2, str(row.people), Palette.PAPER_6)
			_right(VILLAGES_RIGHT, y + 2, str(row.villages), Palette.PAPER_6)
			_text(Vector2(SINCE_X, y + 2), fit(row.since, PANEL.end.x - 8 - SINCE_X), Palette.STONE_4)
		var top := ROWS_Y + rows.size() * ROW + 2
		draw_rect(Rect2(PANEL.position.x + 6, top, PANEL.size.x - 12, 1), Palette.GOLD_3)
		draw_rect(Rect2(DIVIDER_X, top + 4, 1, PANEL.end.y - 24 - top - 4), Palette.INK_4)
		top += 6
		var row: Dictionary = rows[view.chosen]
		_text(Vector2(DETAIL_X, top), row.name, row.colour, true)
		_relation_tag(Vector2(DETAIL_X + _width(row.name) + 8, top + 1), row.relation)
		_text(Vector2(DETAIL_X, top + 16), w("ABOUT_" + row.relation), Palette.PAPER_6)
		if row.has("declared"):
			_text(Vector2(DETAIL_X, top + 30), row.declared, Palette.STONE_4)
		var y := top + 50
		if view.leader_view:
			var orders: Array = view.orders
			for n in orders.size():
				var order: Array = orders[n]
				var x: float = DETAIL_X + n * 112
				_order_slip(Rect2(x, y, 100, SLIP_HEIGHT), w(order[0]), order[1].is_empty(), n == view.order)
				if not order[1].is_empty():
					_text(Vector2(x, y + 17), fit(w(order[1]), 108), Palette.STONE_3)
		else:
			_text(Vector2(DETAIL_X, y - 2), w("LEADER_ONLY"), Palette.STONE_3)
			_order_slip(Rect2(DETAIL_X, y + 14, 140, SLIP_HEIGHT), w("CHALLENGE"), true, true)
			if not view.refusal.is_empty():
				_text(Vector2(DETAIL_X, y + 32), fit(view.refusal, DIVIDER_X - 8 - DETAIL_X), CRIMSON_LIGHT)
		_text(Vector2(LEGEND_X, top), w("RELATION"), Palette.GOLD_6)
		for n in 4:
			var relation: String = ["ally", "neutral", "hostile", "war"][n]
			var ly := top + 18 + n * 17
			_relation_tag(Vector2(LEGEND_X, ly + 1), relation)
			_text(Vector2(LEGEND_X + 46, ly), w("LEGEND_" + relation), Palette.STONE_4)

	func _draw_touch() -> void:
		# The touch chrome: the frame and title bar, the close word a finger finds, no keys.
		_frame(PANEL, Palette.INK_2)
		draw_rect(Rect2(PANEL.position + Vector2(3, 3), Vector2(PANEL.size.x - 6, 16)), Palette.INK_1)
		draw_rect(Rect2(PANEL.position + Vector2(3, 19), Vector2(PANEL.size.x - 6, 1)), Palette.GOLD_3)
		_text(PANEL.position + Vector2(8, 4), w("TITLE"), Palette.PAPER_6, true)
		_text(Vector2(PANEL.end.x - 8 - _width(w("CLOSE_TOUCH")), PANEL.position.y + 4), w("CLOSE_TOUCH"), Palette.STONE_4)
		_draw_own(Vector2(TOUCH_ROWS.position.x + 6, OWN_Y))
		var rows: Array = view.rows
		for n in rows.size():
			var row: Dictionary = rows[n]
			var rect := Rect2(TOUCH_ROWS.position.x, TOUCH_ROWS.position.y + n * TOUCH_ROW, TOUCH_ROWS.size.x,
					TOUCH_BUTTON)
			draw_rect(rect, Palette.GOLD_3 if n == view.chosen else Palette.INK_5)
			draw_rect(rect.grow(-1), Palette.INK_4 if n == view.chosen else Palette.INK_2)
			if n == view.chosen:
				draw_rect(Rect2(rect.position + Vector2(1, 1), Vector2(2, rect.size.y - 2)), Palette.JADE_4)
			var x := rect.position.x + 8
			_text(Vector2(x, rect.position.y + 6), row.name, row.colour)
			if row.relation == "own":
				_text(Vector2(x, rect.position.y + 23), w("OURS"), Palette.GOLD_6)
			else:
				_relation_tag(Vector2(x, rect.position.y + 24), row.relation)
			var rx := rect.position.x + 100
			_text(Vector2(rx, rect.position.y + 6), fit(w("LEADER") + " " + row.leader, rect.end.x - 6 - rx), Palette.PAPER_6)
			_text(Vector2(rx, rect.position.y + 23), w("STRENGTH") % [row.people, row.villages], Palette.STONE_4)
		var d := TOUCH_DETAIL
		draw_rect(Rect2(d.position.x - 8, d.position.y, 1, PANEL.end.y - 8 - d.position.y), Palette.INK_4)
		var row: Dictionary = rows[view.chosen]
		_text(d.position, row.name, row.colour, true)
		_relation_tag(Vector2(d.position.x + _width(row.name) + 8, d.position.y + 1), row.relation)
		var y := _wrapped(d.position + Vector2(0, 18), w("ABOUT_" + row.relation), d.size.x, Palette.PAPER_6)
		if row.has("declared"):
			y = _wrapped(Vector2(d.position.x, y + 2), row.declared, d.size.x, Palette.STONE_4)
		if view.leader_view:
			var orders: Array = view.orders
			var bw := (d.size.x - 8) / 2.0
			for n in orders.size():
				var order: Array = orders[n]
				if not order[1].is_empty():
					_wrapped(Vector2(d.position.x, TOUCH_BUTTONS_Y - 18), w(order[1]), d.size.x, Palette.STONE_3)
				_touch_button(Rect2(d.position.x + n * (bw + 8), TOUCH_BUTTONS_Y, bw, TOUCH_BUTTON), w(order[0]),
						order[1].is_empty(), n == view.order, "")
		else:
			y = _wrapped(Vector2(d.position.x, y + 6), w("LEADER_ONLY"), d.size.x, Palette.STONE_3)
			if not view.refusal.is_empty():
				_wrapped(Vector2(d.position.x, y + 4), view.refusal, d.size.x, CRIMSON_LIGHT)
			_touch_button(Rect2(d.position.x, TOUCH_BUTTONS_Y, d.size.x, TOUCH_BUTTON), w("CHALLENGE_TOUCH"),
					true, true, "")

	func _draw_own(at: Vector2) -> void:
		var own: Dictionary = view.own
		_text(at, own.name, own.colour, true)
		var x := at.x + _width(own.name) + 10
		_text(Vector2(x, at.y), w("LEADER"), Palette.GOLD_6)
		_text(Vector2(x + _width(w("LEADER")) + 6, at.y), own.leader, Palette.PAPER_6)
		if own.you_lead:
			_tag(Vector2(PANEL.end.x - 10, at.y + 1), w("YOU_LEAD"), Palette.GOLD_3)

	func _row_back(rect: Rect2, n: int) -> void:
		if n > 0:
			draw_rect(Rect2(rect.position - Vector2(0, 1), Vector2(rect.size.x, 1)), Palette.INK_4)
		if n == view.chosen:
			draw_rect(rect, Palette.INK_4)
			draw_rect(Rect2(rect.position, Vector2(1, rect.size.y)), Palette.JADE_4)
			for k in 4:
				draw_rect(Rect2(rect.position.x + 4 + k, rect.position.y + 5 + k, 1, 7 - 2 * k), Palette.GOLD_6)

	func _draw_confirm() -> void:
		draw_texture_rect(_scrim_texture(), _room(), true)
		var rect := Rect2(100, 66, 280, 138) if touch_layout else Rect2(110, 84, 260, 104)
		_frame(rect, Palette.INK_2)
		draw_rect(Rect2(rect.position + Vector2(3, 3), Vector2(rect.size.x - 6, 16)), Palette.INK_1)
		draw_rect(Rect2(rect.position + Vector2(3, 19), Vector2(rect.size.x - 6, 1)), Palette.GOLD_3)
		_text(rect.position + Vector2(8, 4), w("CONFIRM_TITLE"), Palette.PAPER_6, true)
		var y := _wrapped(rect.position + Vector2(12, 28), view.confirm.line, rect.size.x - 24, Palette.PAPER_6)
		_wrapped(Vector2(rect.position.x + 12, y + 2), w("CONFIRM_ALLY"), rect.size.x - 24, Palette.STONE_4)
		if touch_layout:
			var bw := (rect.size.x - 36) / 2.0
			var by := rect.end.y - 12 - TOUCH_BUTTON
			_touch_button(Rect2(rect.position.x + 12, by, bw, TOUCH_BUTTON), w("CONFIRM_YES_TOUCH"), true, true, "",
					true)
			_touch_button(Rect2(rect.position.x + 24 + bw, by, bw, TOUCH_BUTTON), w("CONFIRM_NO_TOUCH"), true,
					false, "")
		else:
			var by := rect.end.y - 14 - SLIP_HEIGHT
			_order_slip(Rect2(rect.position.x + 18, by, 100, SLIP_HEIGHT), w("CONFIRM_YES"), true, true, true)
			_order_slip(Rect2(rect.end.x - 118, by, 100, SLIP_HEIGHT), w("CONFIRM_NO"), true, false)

	# `text` from `top_left`, broken at spaces into lines `width` wide; returns the y under it.
	func _wrapped(top_left: Vector2, text: String, width: float, colour: Color) -> float:
		var line := ""
		var y := top_left.y
		for word in text.split(" "):
			var longer := word if line.is_empty() else line + " " + word
			if not line.is_empty() and _width(longer) > width:
				_text(Vector2(top_left.x, y), line, colour)
				y += LINE
				line = word
			else:
				line = longer
		_text(Vector2(top_left.x, y), line, colour)
		return y + LINE

	# A relation's word on a slip of its colour, from `top_left`.
	func _relation_tag(top_left: Vector2, relation: String) -> void:
		var text := w(relation)
		draw_rect(Rect2(top_left, Vector2(_width(text) + 6, 11)), RELATION_FILL[relation])
		_text(top_left + Vector2(3, -1), text, Palette.PAPER_6)

	# An order on a slip in `rect`: jade when lit, crimson when lit and it is war's, ink and greyed
	# when refused.
	func _order_slip(rect: Rect2, text: String, open: bool, lit: bool, war := false) -> void:
		var on := open and lit
		draw_rect(rect, (CRIMSON if war else Palette.JADE_2) if on else Palette.INK_4)
		draw_rect(Rect2(rect.position, Vector2(rect.size.x, 1)), (CRIMSON_LIGHT if war else Palette.JADE_4) if on else Palette.INK_5)
		var colour := Palette.PAPER_6 if open else Palette.STONE_3
		_text(rect.position + Vector2(floorf((rect.size.x - _width(text)) / 2.0), 1), text, colour)

	# A button a finger presses, lit as _order_slip lights it, framed in gold while it can be
	# pressed, with `note` under its word when refused.
	func _touch_button(rect: Rect2, text: String, open: bool, lit: bool, note: String, war := false) -> void:
		var on := open and lit
		draw_rect(rect, Palette.GOLD_3 if open else Palette.INK_5)
		draw_rect(rect.grow(-1), (CRIMSON if war else Palette.JADE_2) if on else Palette.INK_4)
		var colour := Palette.PAPER_6 if open else Palette.STONE_3
		var ty := rect.position.y + floorf((rect.size.y - 14) / 2.0) - (7 if not note.is_empty() else 0)
		_text(Vector2(rect.position.x + floorf((rect.size.x - _width(text)) / 2.0), ty), text, colour)
		if not note.is_empty():
			var shown := fit(note, rect.size.x - 6)
			_text(Vector2(rect.position.x + floorf((rect.size.x - _width(shown)) / 2.0), ty + 14), shown, Palette.STONE_4)

	func _right(right: float, y: float, text: String, colour: Color) -> void:
		_text(Vector2(right - _width(text), y), text, colour)

	func width_of(text: String) -> float:
		_font = get_theme_default_font() if _font == null else _font
		return _width(text)


## The touch HUD's 도전 button redrawn as the 세력 button, at the place hud/touch.gd gives it.
class FactionButton extends Control:
	const RECT := Rect2(96, 92, 42, 42)
	const ICON := ["#.......#", ".#.....#.", "..#...#..", "...#.#...", "....#....", "...#.#...",
			".##...##.", ".#.....#.", "#.......#"]
	var word := ""
	var font: Font

	func _draw() -> void:
		draw_rect(RECT, Color("8c6a2a"))
		draw_rect(Rect2(RECT.position + Vector2(1, 2), RECT.size - Vector2(2, 3)), Color("2a2433"))
		var left := RECT.position + Vector2(floorf((RECT.size.x - 9) / 2.0), 7)
		for y in ICON.size():
			for x in ICON[y].length():
				if ICON[y][x] != ".":
					draw_rect(Rect2(left + Vector2(x, y), Vector2.ONE), Color("c99a3c"))
		var tw := font.get_string_size(word, HORIZONTAL_ALIGNMENT_LEFT, -1, 10).x
		draw_string(font, Vector2(RECT.position.x + floorf((RECT.size.x - tw) / 2.0), RECT.position.y + 24
				+ font.get_ascent(10)), word, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color("efe3c2"))


var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _window: FactionWindow
var _button: FactionButton
var _armies := {}
var _checked := false


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for run: Dictionary in RUNS:
			for view: String in VIEWS:
				_shots.append({"locale": locale, "run": run, "view": view})
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		_world.tick()
	_armies = {"leader": _find(func(army: int, faction: int) -> bool:
			return _world.roster(army)[0] == _world.leader(faction)),
		"member": _find(func(army: int, faction: int) -> bool:
			return _world.challenge_refusal(army, faction) == WorldAdapter.CHALLENGE_NONE),
		"refused": _find(func(army: int, faction: int) -> bool:
			return (_world.roster(army)[0] != _world.leader(faction)
					and _world.challenge_refusal(army, faction) == WorldAdapter.CHALLENGE_NO_SUCCESSION))}
	_armies.confirm = _armies.leader
	_armies.hud = _armies.leader
	for view: String in _armies:
		var army: int = _armies[view]
		print("faction_window: %s steers army %d, person %d, of %s" % [view, army, _world.roster(army)[0],
				_world.faction_names()[_world.army_faction(army)]])
	_open()


# The first army with a living lead for which `test`, given the army and its faction, holds.
func _find(test: Callable) -> int:
	for army in _world.army_count():
		if _world.roster(army).is_empty():
			continue
		if test.call(army, _world.army_faction(army)):
			return army
	push_error("faction_window: no army for a view")
	return 0


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
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
			_steer(hud, _armies[shot.view])
			_world.tick()
			_play._art.note_tick()
			_play._process(0.0)
			hud.labels.notice.visible = false
			hud.labels.challenge_key.visible = false
			_button = FactionButton.new()
			_button.font = hud.arts._font
			hud.add_child(_button)
			_window = FactionWindow.new()
			_window.use_font(hud.arts._font)
			hud.add_child(_window)
			if not _checked:
				_check_rooms()
		2:
			var room: Vector2 = hud.room
			_window.position = ((room - _window.SCREEN) / 2.0).floor()
			_window.touch_layout = shot.run.touch
			_button.position = hud.touch.position
			_button.visible = shot.run.touch
			_button.word = _word("BUTTON")
			_window.visible = shot.view != "hud"
			_window.view = _compose(shot.view)
			_window.queue_redraw()
			_button.queue_redraw()
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("faction_window: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _steer(hud: CanvasLayer, army: int) -> void:
	_play.steered = army
	_play._art.steered = army
	_play._body = _world.roster(army)[0]
	hud.show_player(_world, army)


func _word(key: String) -> String:
	return WORDS[key][LOCALES.find(TranslationServer.get_locale().left(2))]


func _words() -> Dictionary:
	var words := {}
	for key: String in WORDS:
		words[key] = _word(key)
	return words


func _faction_name(faction: int) -> String:
	return TranslationServer.translate("FACTION_" + _world.faction_names()[faction].to_upper())


func _relation(from: int, to: int) -> String:
	var names := _world.faction_names()
	for pair: String in DECLARED:
		if pair == names[from] + "|" + names[to] or pair == names[to] + "|" + names[from]:
			return "war" if DECLARED[pair] == "war" else "neutral"
	return "hostile" if _world.hostile(from, to) or _world.hostile(to, from) else "ally"


# The faction whose leader made the declaration between `a` and `b`: the first DECLARED names.
func _declarer(a: int, b: int) -> int:
	var names := _world.faction_names()
	for pair: String in DECLARED:
		var two := pair.split("|")
		if (two[0] == names[a] and two[1] == names[b]) or (two[0] == names[b] and two[1] == names[a]):
			return names.find(two[0])
	return a


# What the window shows for `view`, read from the world, with DECLARED on top.
func _compose(view: String) -> Dictionary:
	var army: int = _armies[view]
	var own := _world.army_faction(army)
	var roster := _world.roster(army)
	var colours: Array[Color] = [Color("5b9ae0"), Color("d63a2f"), Color("e8a830")]
	var people := {}
	for other in _world.army_count():
		var f := _world.army_faction(other)
		people[f] = people.get(f, 0) + _world.roster(other).size()
	var villages := {}
	for village: Dictionary in _world.villages():
		villages[village.faction] = villages.get(village.faction, 0) + 1
	var rows: Array[Dictionary] = []
	var order := [own]
	for f in _world.faction_count():
		if f != own:
			order.append(f)
	var date := season(_world.date())
	for f: int in order:
		var leader := _world.leader(f)
		var relation := "own" if f == own else _relation(own, f)
		var row := {"name": _faction_name(f), "colour": colours[f % 3], "relation": relation,
				"leader": _who(leader, roster), "people": people.get(f, 0), "villages": villages.get(f, 0),
				"since": "" if f == own else _word("BY_STANCE")}
		if relation == "war" or relation == "neutral":
			row.since = date
			row.declared = _word("DECLARED_BY") % [_faction_name(_declarer(own, f)), date]
		rows.append(row)
	var words := _words()
	var leads := roster.has(_world.leader(own))
	var chosen := 1
	for n in rows.size():
		if rows[n].relation == ("ally" if view == "confirm" else "war"):
			chosen = n
	var refusal := ""
	if view == "refused":
		var key: String = hud_challenge_key(_world.challenge_refusal(army, own))
		refusal = TranslationServer.translate(key)
	var relation: String = rows[chosen].relation
	var orders := [["DECLARE", "REFUSED_AT_WAR" if relation == "war" else ""],
			["PEACE", "REFUSED_AT_PEACE" if relation == "ally" or relation == "neutral" else ""]]
	var confirm := {}
	if view == "confirm":
		confirm = {"line": _word("CONFIRM_LINE") % rows[chosen].name}
	return {"words": words, "own": {"name": rows[0].name, "colour": rows[0].colour, "leader": rows[0].leader,
			"you_lead": leads}, "rows": rows, "chosen": chosen, "leader_view": leads, "orders": orders,
			"order": 0 if relation != "war" else 1, "refusal": refusal, "confirm": confirm}


static func hud_challenge_key(refusal: int) -> String:
	var keys := {WorldAdapter.CHALLENGE_NONE: "CHALLENGE_NONE",
		WorldAdapter.CHALLENGE_NO_SUCCESSION: "CHALLENGE_NO_SUCCESSION",
		WorldAdapter.CHALLENGE_NO_LEADER: "CHALLENGE_NO_LEADER",
		WorldAdapter.CHALLENGE_NOT_A_MEMBER: "CHALLENGE_NOT_A_MEMBER",
		WorldAdapter.CHALLENGE_IS_LEADER: "CHALLENGE_IS_LEADER"}
	return keys.get(refusal, "CHALLENGE_NONE")


func season(date: Dictionary) -> String:
	return TranslationServer.translate("HUD_SEASON_YEAR") % [TranslationServer.translate("SEASON_%d" % date.season),
			date.year]


func _who(person: int, own: PackedInt64Array) -> String:
	if person < 0:
		return TranslationServer.translate("HUD_LEADER_NONE")
	if own.has(person):
		return TranslationServer.translate("HUD_LEADER_YOU")
	return PersonName.of(_world, person, true)


# Prints every word, in both locales, that overruns the room ROOM gives it.
func _check_rooms() -> void:
	_checked = true
	var over := 0
	for key: String in WORDS:
		for n in LOCALES.size():
			var text: String = WORDS[key][n].replace("%s", "")
			var width := _window.width_of(text)
			if ROOM.has(key) and width > ROOM[key]:
				over += 1
				print("faction_window: %s (%s) is %d past %d" % [key, text, width, ROOM[key]])
	print("faction_window: %d words overrun their room" % over)


func _save(shot: Dictionary) -> void:
	var name: String = "%s_%s_%s" % [shot.view, shot.run.name, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("faction_window: %s.png %dx%d, HUD %dx in %s, sha256 %s" % [name, image.get_width(), image.get_height(),
			ViewScale.hud_scale(shot.run.window, shot.run.touch), ViewScale.hud_room(shot.run.window, shot.run.touch),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
