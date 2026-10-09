## The faction window, drawn as art/mockup/faction_window.gd lays it out on PC and on a touch screen
## (evidence/pr-177). It lists every faction, the steered person's first, with how it stands toward
## the person's, its leader, its living people and its villages, and the basis of that standing:
## the stance, or who declared it and when. Under the list, the chosen faction's standing in words,
## and, for the faction's leader, the orders to declare war and to make peace, each greyed with the
## core's refusal when it may not be given, war asked again before it is declared; for anyone else,
## that only the leader decides, and the challenge to the leader, with the core's verdict when it is
## refused. Every word comes from hud.csv through Godot's translations. What it shows is read from
## the world through the adapter, the standing from its one read, relation(). It holds no game rule.
extends "res://hud/panel.gd"

## The orders a leader gives, in the order they are drawn.
const ORDERS := [WorldAdapter.DECLARATION_WAR, WorldAdapter.DECLARATION_PEACE]
const ORDER_KEYS := {WorldAdapter.DECLARATION_WAR: "FACTION_DECLARE", WorldAdapter.DECLARATION_PEACE: "FACTION_PEACE"}
## The hud.csv key of each relation's word, by WorldAdapter.RELATION_* constant.
const RELATION_KEYS := {
	WorldAdapter.RELATION_OWN: "FACTION_OURS",
	WorldAdapter.RELATION_ALLY: "FACTION_ALLY",
	WorldAdapter.RELATION_NEUTRAL: "FACTION_NEUTRAL",
	WorldAdapter.RELATION_HOSTILE: "FACTION_HOSTILE",
	WorldAdapter.RELATION_AT_WAR: "FACTION_AT_WAR",
}
const ABOUT_KEYS := {
	WorldAdapter.RELATION_ALLY: "FACTION_ABOUT_ALLY",
	WorldAdapter.RELATION_NEUTRAL: "FACTION_ABOUT_NEUTRAL",
	WorldAdapter.RELATION_HOSTILE: "FACTION_ABOUT_HOSTILE",
	WorldAdapter.RELATION_AT_WAR: "FACTION_ABOUT_AT_WAR",
}
const LEGEND_KEYS := {
	WorldAdapter.RELATION_ALLY: "FACTION_LEGEND_ALLY",
	WorldAdapter.RELATION_NEUTRAL: "FACTION_LEGEND_NEUTRAL",
	WorldAdapter.RELATION_HOSTILE: "FACTION_LEGEND_HOSTILE",
	WorldAdapter.RELATION_AT_WAR: "FACTION_LEGEND_AT_WAR",
}
## The hud.csv key of each refusal of an order, by WorldAdapter.DECLARE_* constant.
const REFUSAL_KEYS := {
	WorldAdapter.DECLARE_NONE: "",
	WorldAdapter.DECLARE_DEAD: "DECLARE_DEAD",
	WorldAdapter.DECLARE_NOT_THE_LEADER: "DECLARE_NOT_THE_LEADER",
	WorldAdapter.DECLARE_SAME_FACTION: "DECLARE_SAME_FACTION",
	WorldAdapter.DECLARE_ALREADY_AT_WAR: "DECLARE_ALREADY_AT_WAR",
	WorldAdapter.DECLARE_ALREADY_AT_PEACE: "DECLARE_ALREADY_AT_PEACE",
	WorldAdapter.DECLARE_TOO_SOON: "DECLARE_TOO_SOON",
}
const RELATION_FILL := {
	WorldAdapter.RELATION_ALLY: Palette.JADE_2,
	WorldAdapter.RELATION_NEUTRAL: Palette.INK_4,
	WorldAdapter.RELATION_HOSTILE: Palette.AMBER_2,
	WorldAdapter.RELATION_AT_WAR: Palette.CRIMSON_3,
}
const LEGEND := [WorldAdapter.RELATION_ALLY, WorldAdapter.RELATION_NEUTRAL, WorldAdapter.RELATION_HOSTILE,
		WorldAdapter.RELATION_AT_WAR]
# The PC layout, in the native 480x270.
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
const ORDER_WIDTH := 100
const ORDER_STEP := 112
const CHALLENGE_WIDTH := 140
const LINE := 14
## The touch layout: rows a finger presses, beside the chosen faction's detail.
const TOUCH_ROWS := Rect2(26, 52, 230, 0)
const TOUCH_ROW := 44
## Where a touch row's leader and strength start, right of the faction's name and relation.
const TOUCH_TEXT_X := 90
const TOUCH_DETAIL := Rect2(270, 52, 182, 0)
## The height of a button a finger presses: the touch HUD's least.
const TOUCH_BUTTON := 42
const TOUCH_BUTTONS_Y := 202
const PC_CONFIRM := Rect2(110, 84, 260, 104)
const TOUCH_CONFIRM := Rect2(100, 66, 280, 138)
## The room each word is given, by its hud.csv key, for the fit tests.
const ROOM := {"FACTION_TITLE": 80, "FACTION_CLOSE": 60, "FACTION_CLOSE_TOUCH": 60, "FACTION_NAME": 70,
		"FACTION_RELATION": 60, "FACTION_PEOPLE": 40, "FACTION_VILLAGES": 40, "FACTION_SINCE": 76,
		"FACTION_OURS": 60, "FACTION_YOU_LEAD": 100, "FACTION_ALLY": 50, "FACTION_NEUTRAL": 50,
		"FACTION_HOSTILE": 50, "FACTION_AT_WAR": 50, "FACTION_ABOUT_ALLY": 268, "FACTION_ABOUT_NEUTRAL": 268,
		"FACTION_ABOUT_HOSTILE": 268, "FACTION_ABOUT_AT_WAR": 268, "FACTION_LEGEND_ALLY": 88,
		"FACTION_LEGEND_NEUTRAL": 88, "FACTION_LEGEND_HOSTILE": 88, "FACTION_LEGEND_AT_WAR": 88,
		"FACTION_BY_STANCE": 76, "FACTION_DECLARE": 90, "FACTION_PEACE": 90, "FACTION_LEADER_ONLY": 268,
		"FACTION_CHALLENGE": 130, "FACTION_CHALLENGE_TOUCH": 130, "FACTION_KEYS_LEADER": 420,
		"FACTION_KEYS_MEMBER": 420, "FACTION_CONFIRM_TITLE": 200, "FACTION_CONFIRM_YES": 96,
		"FACTION_CONFIRM_NO": 96, "FACTION_CONFIRM_YES_TOUCH": 100, "FACTION_CONFIRM_NO_TOUCH": 100,
		"DECLARE_ALREADY_AT_WAR": 108, "DECLARE_ALREADY_AT_PEACE": 108, "DECLARE_TOO_SOON": 108,
		"DECLARE_NOT_THE_LEADER": 108, "DECLARE_DEAD": 108, "DECLARE_SAME_FACTION": 108}

## Whether the window is laid out for a touch screen.
var touch_layout := false
## The key that challenges the faction's leader, as the play scene binds it.
var challenge_key := KEY_NONE
## Each faction as the window lists it, the steered person's first: `faction`, `name`, `colour`,
## `relation`, a WorldAdapter.RELATION_* constant, `leader`, `people`, `villages`, `since`, and for
## a declared standing, `declared`, who declared it and when.
var rows: Array[Dictionary] = []
## The row chosen, never the steered person's own.
var chosen := 1
## Whether the steered person leads its faction, so the window offers it the orders.
var leads := false
## The orders for the chosen faction, as ORDERS lists them: each a Dictionary of `what` and
## `refusal`, the core's DECLARE_* verdict on it now.
var orders: Array[Dictionary] = []
## The order lit, by index in `orders`.
var order := 0
## The order asked again before it is given, a DECLARATION_* constant, or -1 for none.
var confirming := -1
## The hud.csv key of the core's verdict on the last challenge from the window, or "" for none.
var verdict := ""

var _army := -1


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED and _world != null:
		_compose()


## Opens the window on `army`'s faction, laid out for a touch screen when `touch` is set.
func show_factions(world: WorldAdapter, army: int, touch: bool) -> void:
	_world = world
	_army = army
	touch_layout = touch
	chosen = 1
	order = 0
	confirming = -1
	verdict = ""
	_compose()
	visible = true


## Moves the choice: up and down among the other factions, left and right between the orders.
## Returns whether it moved.
func move(direction: Vector2i) -> bool:
	if confirming >= 0:
		return false
	if direction.y != 0:
		var at := chosen + direction.y
		if at < 1 or at >= rows.size():
			return false
		chosen = at
		verdict = ""
		_compose()
		return true
	var to := order + direction.x
	if not leads or direction.x == 0 or to < 0 or to >= orders.size():
		return false
	order = to
	queue_redraw()
	return true


## The faction chosen, by index.
func chosen_faction() -> int:
	return rows[chosen].faction if chosen < rows.size() else -1


## Asks for the order lit, as Enter does: war is asked again before it is given, and returns -1;
## peace is returned to be given at once. Returns the DECLARATION_* constant to give, or -1 for
## none: none for a person who does not lead, or an order the core refuses now.
func ask() -> int:
	if not leads or confirming >= 0 or orders.is_empty() or orders[order].refusal != WorldAdapter.DECLARE_NONE:
		return -1
	var what: int = orders[order].what
	if what == WorldAdapter.DECLARATION_WAR:
		confirming = what
		queue_redraw()
		return -1
	return what


## Answers the question asked again: returns the order to give when `yes`, and -1 otherwise, closing
## the question either way.
func answer(yes: bool) -> int:
	var what := confirming
	confirming = -1
	queue_redraw()
	return what if yes else -1


## Reads the window again after an order or a challenge, showing `refused`, the hud.csv key of the
## core's refusal of a challenge, when it is given.
func refresh(refused := "") -> void:
	verdict = refused
	_compose()


## Works the window with `event`, as tap() reads a tap: while war is asked again, Enter answers yes
## and Escape no; otherwise up and down choose a faction and left and right an order, Enter asks for
## the order lit, the challenge key challenges the faction's leader, and the toggle key or Escape
## closes it. Returns "yes", "no", "order", "challenge", "close", "done", or "" when the key did
## nothing.
func key(event: InputEventKey) -> String:
	if confirming >= 0:
		return "yes" if ENTER_KEYS.has(event.keycode) else ("no" if event.keycode == KEY_ESCAPE else "")
	if event.keycode == toggle_key or event.keycode == KEY_ESCAPE:
		return "close"
	if DIRECTIONS.has(event.keycode):
		return _done(move(DIRECTIONS[event.keycode]))
	if ENTER_KEYS.has(event.keycode):
		return "order"
	return "challenge" if event.keycode == challenge_key else ""


## Returns what a tap at `at`, in the window's pixels, asks on a touch screen: "close", "challenge",
## "yes" or "no" while war is asked again, "order" with the row's orders lit by the tap, "choose"
## for a faction tapped, or "" for nothing.
func tap(at: Vector2) -> String:
	if confirming >= 0:
		var buttons := _confirm_buttons(TOUCH_CONFIRM)
		if buttons[0].has_point(at):
			return "yes"
		if buttons[1].has_point(at):
			return "no"
		return ""
	if _close_hit(PANEL, tr("FACTION_CLOSE_TOUCH"), at):
		return "close"
	for n in range(1, rows.size()):
		if _touch_row(n).has_point(at):
			chosen = n
			verdict = ""
			_compose()
			return "choose"
	if leads:
		for n in orders.size():
			if _order_button(n).has_point(at):
				order = n
				queue_redraw()
				return "order"
	elif _challenge_button().has_point(at):
		return "challenge"
	return ""


# Reads what the window shows from the world, in the current locale.
func _compose() -> void:
	var own := _world.army_faction(_army)
	var roster := _world.roster(_army)
	var people := {}
	for army in _world.army_count():
		var faction := _world.army_faction(army)
		people[faction] = people.get(faction, 0) + _world.roster(army).size()
	var villages := {}
	for village: Dictionary in _world.villages():
		villages[village.faction] = villages.get(village.faction, 0) + 1
	var listed: Array[int] = [own]
	for faction in _world.faction_count():
		if faction != own:
			listed.append(faction)
	rows.clear()
	for faction: int in listed:
		var row := {"faction": faction, "name": Affiliation.faction_name(_world, faction), "colour": _faction_colour(faction),
				"relation": _world.relation(own, faction), "leader": _who(_world.leader(faction), roster),
				"people": people.get(faction, 0), "villages": villages.get(faction, 0), "since": ""}
		var declared := _world.declaration(own, faction)
		if faction != own and declared.is_empty():
			row.since = tr("FACTION_BY_STANCE")
		elif faction != own:
			var date := tr("HUD_SEASON_YEAR") % [tr("SEASON_%d" % declared.season), declared.year]
			row.since = date
			row.declared = tr("FACTION_DECLARED_BY") % [Affiliation.faction_name(_world, declared.by), date]
		rows.append(row)
	chosen = clampi(chosen, 1, maxi(rows.size() - 1, 1))
	leads = not roster.is_empty() and _world.leader(own) == roster[0]
	orders.clear()
	var target := chosen_faction()
	for what: int in ORDERS:
		var refusal := WorldAdapter.DECLARE_NOT_THE_LEADER
		if not roster.is_empty() and target >= 0:
			refusal = _world.declaration_refusal(roster[0], target, what)
		orders.append({"what": what, "refusal": refusal})
	queue_redraw()


# The leader: the player when it is the steered person, or by name, the given name alone in English.
func _who(person: int, own: PackedInt64Array) -> String:
	if person < 0:
		return tr("HUD_LEADER_NONE")
	return tr("HUD_LEADER_YOU") if own.has(person) else PersonName.of(_world, person, true)


func _draw() -> void:
	if _world == null or rows.is_empty():
		return
	_font = get_theme_default_font() if _font == null else _font
	_dim()
	if touch_layout:
		_draw_touch()
	else:
		_draw_pc()
	if confirming >= 0:
		_draw_confirm()


func _draw_pc() -> void:
	_dialog(PANEL, tr("FACTION_TITLE"), tr("FACTION_CLOSE"), tr("FACTION_KEYS_LEADER" if leads else "FACTION_KEYS_MEMBER"))
	_draw_own(Vector2(NAME_X, OWN_Y))
	_text(Vector2(NAME_X, TITLES_Y), tr("FACTION_NAME"), Palette.STONE_4)
	_text(Vector2(RELATION_X, TITLES_Y), tr("FACTION_RELATION"), Palette.STONE_4)
	_text(Vector2(LEADER_X, TITLES_Y), tr("HUD_LEADER"), Palette.STONE_4)
	_right(PEOPLE_RIGHT, TITLES_Y, tr("FACTION_PEOPLE"), Palette.STONE_4)
	_right(VILLAGES_RIGHT, TITLES_Y, tr("FACTION_VILLAGES"), Palette.STONE_4)
	_text(Vector2(SINCE_X, TITLES_Y), tr("FACTION_SINCE"), Palette.STONE_4)
	for n in rows.size():
		var row: Dictionary = rows[n]
		var y: int = ROWS_Y + n * ROW
		_row_back(Rect2(PANEL.position.x + 6, y, PANEL.size.x - 12, ROW - 1), n)
		_text(Vector2(NAME_X + 6, y + 2), row.name, row.colour)
		_relation_tag(Vector2(RELATION_X, y + 3), row.relation)
		_text(Vector2(LEADER_X, y + 2), fit(row.leader, PEOPLE_RIGHT - 40 - LEADER_X), Palette.PAPER_3)
		_right(PEOPLE_RIGHT, y + 2, str(row.people), Palette.PAPER_3)
		_right(VILLAGES_RIGHT, y + 2, str(row.villages), Palette.PAPER_3)
		_text(Vector2(SINCE_X, y + 2), fit(row.since, PANEL.end.x - 8 - SINCE_X), Palette.STONE_4)
	var top := ROWS_Y + rows.size() * ROW + 2
	draw_rect(Rect2(PANEL.position.x + 6, top, PANEL.size.x - 12, 1), Palette.GOLD_2)
	draw_rect(Rect2(DIVIDER_X, top + 4, 1, PANEL.end.y - 24 - top - 4), Palette.INK_3)
	top += 6
	var row: Dictionary = rows[chosen]
	_text(Vector2(DETAIL_X, top), row.name, row.colour, true)
	_relation_tag(Vector2(DETAIL_X + _width(row.name) + 8, top + 1), row.relation)
	_text(Vector2(DETAIL_X, top + 16), fit(tr(ABOUT_KEYS.get(row.relation, "")), DIVIDER_X - 8 - DETAIL_X), Palette.PAPER_3)
	if row.has("declared"):
		_text(Vector2(DETAIL_X, top + 30), fit(row.declared, DIVIDER_X - 8 - DETAIL_X), Palette.STONE_4)
	var y := top + 50
	if leads:
		for n in orders.size():
			var x: float = DETAIL_X + n * ORDER_STEP
			var refusal: int = orders[n].refusal
			_order_slip(Rect2(x, y, ORDER_WIDTH, SLIP_HEIGHT), tr(ORDER_KEYS[orders[n].what]),
					refusal == WorldAdapter.DECLARE_NONE, n == order)
			if refusal != WorldAdapter.DECLARE_NONE:
				_text(Vector2(x, y + 17), fit(tr(REFUSAL_KEYS[refusal]), ORDER_STEP - 4), Palette.STONE_3)
	else:
		_text(Vector2(DETAIL_X, y - 2), fit(tr("FACTION_LEADER_ONLY"), DIVIDER_X - 8 - DETAIL_X), Palette.STONE_3)
		_order_slip(Rect2(DETAIL_X, y + 14, CHALLENGE_WIDTH, SLIP_HEIGHT), tr("FACTION_CHALLENGE"), true, true)
		if not verdict.is_empty():
			_text(Vector2(DETAIL_X, y + 32), fit(tr(verdict), DIVIDER_X - 8 - DETAIL_X), Palette.CRIMSON_4)
	_text(Vector2(LEGEND_X, top), tr("FACTION_RELATION"), Palette.GOLD_4)
	for n in LEGEND.size():
		var ly := top + 18 + n * 17
		_relation_tag(Vector2(LEGEND_X, ly + 1), LEGEND[n])
		_text(Vector2(LEGEND_X + 46, ly), tr(LEGEND_KEYS[LEGEND[n]]), Palette.STONE_4)


func _draw_touch() -> void:
	# The touch chrome: the frame and title bar, the close word a finger finds, no keys.
	_titled(PANEL, tr("FACTION_TITLE"), tr("FACTION_CLOSE_TOUCH"))
	_draw_own(Vector2(TOUCH_ROWS.position.x + 6, OWN_Y))
	for n in rows.size():
		var row: Dictionary = rows[n]
		var rect := _touch_row(n)
		draw_rect(rect, Palette.GOLD_2 if n == chosen else Palette.INK_4)
		draw_rect(rect.grow(-1), Palette.INK_3 if n == chosen else Palette.INK_2)
		if n == chosen:
			draw_rect(Rect2(rect.position + Vector2(1, 1), Vector2(2, rect.size.y - 2)), Palette.JADE_3)
		var x := rect.position.x + 8
		_text(Vector2(x, rect.position.y + 6), row.name, row.colour)
		_relation_tag(Vector2(x, rect.position.y + 24), row.relation)
		var rx := rect.position.x + TOUCH_TEXT_X
		_text(Vector2(rx, rect.position.y + 6), fit(tr("HUD_LEADER") + " " + row.leader, rect.end.x - 6 - rx), Palette.PAPER_3)
		_text(Vector2(rx, rect.position.y + 23), tr("FACTION_STRENGTH") % [row.people, row.villages], Palette.STONE_4)
	var d := TOUCH_DETAIL
	draw_rect(Rect2(d.position.x - 8, d.position.y, 1, PANEL.end.y - 8 - d.position.y), Palette.INK_3)
	var row: Dictionary = rows[chosen]
	_text(d.position, row.name, row.colour, true)
	_relation_tag(Vector2(d.position.x + _width(row.name) + 8, d.position.y + 1), row.relation)
	var y := _wrapped(d.position + Vector2(0, 18), tr(ABOUT_KEYS.get(row.relation, "")), d.size.x, Palette.PAPER_3)
	if row.has("declared"):
		y = _wrapped(Vector2(d.position.x, y + 2), row.declared, d.size.x, Palette.STONE_4)
	if leads:
		for n in orders.size():
			var refusal: int = orders[n].refusal
			if refusal != WorldAdapter.DECLARE_NONE and n == order:
				_wrapped(Vector2(d.position.x, TOUCH_BUTTONS_Y - 18), tr(REFUSAL_KEYS[refusal]), d.size.x, Palette.STONE_3)
			_touch_button(_order_button(n), tr(ORDER_KEYS[orders[n].what]), refusal == WorldAdapter.DECLARE_NONE,
					n == order)
	else:
		y = _wrapped(Vector2(d.position.x, y + 6), tr("FACTION_LEADER_ONLY"), d.size.x, Palette.STONE_3)
		if not verdict.is_empty():
			_wrapped(Vector2(d.position.x, y + 4), tr(verdict), d.size.x, Palette.CRIMSON_4)
		_touch_button(_challenge_button(), tr("FACTION_CHALLENGE_TOUCH"), true, true)


func _draw_own(at: Vector2) -> void:
	var own: Dictionary = rows[0]
	_text(at, own.name, own.colour, true)
	var x := at.x + _width(own.name) + 10
	_text(Vector2(x, at.y), tr("HUD_LEADER"), Palette.GOLD_4)
	_text(Vector2(x + _width(tr("HUD_LEADER")) + 6, at.y), own.leader, Palette.PAPER_3)
	if leads:
		_tag(Vector2(PANEL.end.x - 10, at.y + 1), tr("FACTION_YOU_LEAD"), Palette.GOLD_2)


func _row_back(rect: Rect2, n: int) -> void:
	if n > 0:
		draw_rect(Rect2(rect.position - Vector2(0, 1), Vector2(rect.size.x, 1)), Palette.INK_3)
	if n == chosen:
		draw_rect(rect, Palette.INK_3)
		draw_rect(Rect2(rect.position, Vector2(1, rect.size.y)), Palette.JADE_3)
		for k in 4:
			draw_rect(Rect2(rect.position.x + 4 + k, rect.position.y + 5 + k, 1, 7 - 2 * k), Palette.GOLD_4)


func _draw_confirm() -> void:
	draw_texture_rect(_scrim_texture(), _room(), true)
	var rect := TOUCH_CONFIRM if touch_layout else PC_CONFIRM
	_titled(rect, tr("FACTION_CONFIRM_TITLE"), "")
	var row: Dictionary = rows[chosen]
	var y := _wrapped(rect.position + Vector2(12, 28), tr("FACTION_CONFIRM_LINE") % row.name, rect.size.x - 24, Palette.PAPER_3)
	var then := "FACTION_CONFIRM_ALLY" if row.relation == WorldAdapter.RELATION_ALLY else "FACTION_CONFIRM_WAR"
	_wrapped(Vector2(rect.position.x + 12, y + 2), tr(then), rect.size.x - 24, Palette.STONE_4)
	var buttons := _confirm_buttons(rect)
	if touch_layout:
		_touch_button(buttons[0], tr("FACTION_CONFIRM_YES_TOUCH"), true, true, true)
		_touch_button(buttons[1], tr("FACTION_CONFIRM_NO_TOUCH"), true, false)
	else:
		_order_slip(buttons[0], tr("FACTION_CONFIRM_YES"), true, true, true)
		_order_slip(buttons[1], tr("FACTION_CONFIRM_NO"), true, false)


# The confirmation's buttons in `rect`, yes then no.
func _confirm_buttons(rect: Rect2) -> Array[Rect2]:
	if rect == TOUCH_CONFIRM:
		var w := (rect.size.x - 36) / 2.0
		var y := rect.end.y - 12 - TOUCH_BUTTON
		return [Rect2(rect.position.x + 12, y, w, TOUCH_BUTTON), Rect2(rect.position.x + 24 + w, y, w, TOUCH_BUTTON)]
	var by := rect.end.y - 14 - SLIP_HEIGHT
	return [Rect2(rect.position.x + 18, by, ORDER_WIDTH, SLIP_HEIGHT),
			Rect2(rect.end.x - 18 - ORDER_WIDTH, by, ORDER_WIDTH, SLIP_HEIGHT)]


func _touch_row(n: int) -> Rect2:
	return Rect2(TOUCH_ROWS.position.x, TOUCH_ROWS.position.y + n * TOUCH_ROW, TOUCH_ROWS.size.x, TOUCH_BUTTON)


func _order_button(n: int) -> Rect2:
	var w := (TOUCH_DETAIL.size.x - 8) / 2.0
	return Rect2(TOUCH_DETAIL.position.x + n * (w + 8), TOUCH_BUTTONS_Y, w, TOUCH_BUTTON)


func _challenge_button() -> Rect2:
	return Rect2(TOUCH_DETAIL.position.x, TOUCH_BUTTONS_Y, TOUCH_DETAIL.size.x, TOUCH_BUTTON)


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


# A relation's word on a slip of its colour, from `top_left`; the steered person's own faction's
# word stands alone, in gold.
func _relation_tag(top_left: Vector2, relation: int) -> void:
	var text := tr(RELATION_KEYS[relation])
	if relation == WorldAdapter.RELATION_OWN:
		_text(top_left - Vector2(0, 1), text, Palette.GOLD_4)
		return
	draw_rect(Rect2(top_left, Vector2(_width(text) + 6, 11)), RELATION_FILL[relation])
	_text(top_left + Vector2(3, -1), text, Palette.PAPER_3)


# An order on a slip in `rect`: jade when lit, crimson when lit and it is war's, ink and greyed when
# refused.
func _order_slip(rect: Rect2, text: String, open: bool, lit: bool, war := false) -> void:
	var on := open and lit
	draw_rect(rect, (Palette.CRIMSON_3 if war else Palette.JADE_2) if on else Palette.INK_3)
	draw_rect(Rect2(rect.position, Vector2(rect.size.x, 1)), (Palette.CRIMSON_4 if war else Palette.JADE_3) if on else Palette.INK_4)
	_text(rect.position + Vector2(floorf((rect.size.x - _width(text)) / 2.0), 1), text, Palette.PAPER_3 if open else Palette.STONE_3)


# A button a finger presses, lit as _order_slip lights it, framed in gold while it can be pressed.
func _touch_button(rect: Rect2, text: String, open: bool, lit: bool, war := false) -> void:
	var on := open and lit
	draw_rect(rect, Palette.GOLD_2 if open else Palette.INK_4)
	draw_rect(rect.grow(-1), (Palette.CRIMSON_3 if war else Palette.JADE_2) if on else Palette.INK_3)
	var top := rect.position.y + floorf((rect.size.y - 14) / 2.0)
	_text(Vector2(rect.position.x + floorf((rect.size.x - _width(text)) / 2.0), top), text, Palette.PAPER_3 if open else Palette.STONE_3)
