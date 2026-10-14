## The sect panel, drawn as art/mockup/sect_panel.png lays it out: a sect's seat, shown by its
## gate in its stage now and whether the seat is held, its faction, master, elders and members
## against its cap, and the cards of its master and elders; beside it a column for a faction, with
## its leader, whether the faction elects it or the lead passes by duel, and its sects; and under
## the cards, the core's verdict on the player's application to the sect shown. Every word comes
## from hud.csv through Godot's translations. What it shows is read from the world through the
## adapter. It holds no game rule.
extends "res://hud/panel.gd"

const MAX_ELDER_CARDS := 3
const MAX_SECT_ROWS := 5
# The mockup's layout, in the native 480x270.
const SEAT := Rect2(30, 40, 112, 100)
const FIGURES := Rect2(154, 40, 176, 100)
const FIGURE_STEP := 20
const PIP := 7
const PIPS_Y := 126
const RANKS_Y := 150
const CARDS_Y := 176
const COLUMN := Rect2(344, 38, 108, 196)
# The faction tabs share the column's width, 6 pixels in from each side and 4 apart, each word
# centred with 2 pixels to spare on each side.
const TAB_INSET := 6
const TAB_GAP := 4
const TAB_PAD := 2
const LIST_Y := 118
const VERDICT_Y := 222
# The hud.csv key of each verdict on an application, by WorldAdapter.APPLICATION_* constant.
const APPLICATION_KEYS := {
	WorldAdapter.APPLICATION_NONE: "APPLICATION_NONE",
	WorldAdapter.APPLICATION_DEAD: "APPLICATION_DEAD",
	WorldAdapter.APPLICATION_IN_A_SECT: "APPLICATION_IN_A_SECT",
	WorldAdapter.APPLICATION_NO_HOME: "APPLICATION_NO_HOME",
	WorldAdapter.APPLICATION_NOT_OF_FACTION: "APPLICATION_NOT_OF_FACTION",
	WorldAdapter.APPLICATION_TOO_YOUNG: "APPLICATION_TOO_YOUNG",
	WorldAdapter.APPLICATION_UNPROVEN: "APPLICATION_UNPROVEN",
	WorldAdapter.APPLICATION_TOO_FAR: "APPLICATION_TOO_FAR",
	WorldAdapter.APPLICATION_SEAT_LOST: "APPLICATION_SEAT_LOST",
	WorldAdapter.APPLICATION_NO_ELDER: "APPLICATION_NO_ELDER",
	WorldAdapter.APPLICATION_FULL: "APPLICATION_FULL",
}
const ROW_STEP := 18
# Where the leader's name and sect start in the column.
const LEADER_TEXT_X := 30

## The faction whose column is shown.
var faction := -1
## The sect shown on the left, or -1 for none.
var sect := -1
## The steered person's sect, which carries the title's tag, or -1 for none.
var own_sect := -1
## The shown sect's figures, by what they are: `name`, `faction`, `master`, `elders`, `members`
## and `cap`, as the panel draws them, and `seat`: held, lost or fallen.
var details := {}
## The cards shown under the figures, the master's first: each a Dictionary of `person`,
## `name`, `line` and `master`.
var cards: Array[Dictionary] = []
## The faction's column: `tab` for each faction, `leader` and `leader_sect` as drawn, `elects` when
## the faction elects its leader, `duel` when the lead passes by duel, and `sects`, each a Dictionary of `sect`, `name` and `count`, or
## `state` of lost or fallen.
var column := {}
## The hud.csv key of the verdict on the last application to the sect shown, or "" for none.
var verdict := ""

var _army := -1
var _gate: Texture2D
var _gate_frame := 0


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED and _world != null:
		_compose()


## Opens the panel on the sect of `army`'s person, or on the first sect of its faction when the
## person belongs to none.
func show_sect(world: WorldAdapter, army: int) -> void:
	_world = world
	_army = army
	var roster := world.roster(army)
	own_sect = world.person_sect(roster[0]) if not roster.is_empty() else -1
	faction = world.army_faction(army)
	verdict = ""
	sect = own_sect
	if sect >= 0:
		faction = world.sect_faction(sect)
	else:
		var sects := _sects_of(faction)
		sect = sects.front() if not sects.is_empty() else -1
	_compose()
	visible = true


## Moves the selection along the faction's sects: down for the next, up for the one before.
## Returns whether it moved.
func move(direction: Vector2i) -> bool:
	var sects := _sects_of(faction)
	var at := sects.find(sect) + direction.y
	if direction.y == 0 or at < 0 or at >= sects.size():
		return false
	sect = sects[at]
	verdict = ""
	_compose()
	return true


## Shows the next faction's column, and its first sect.
func next_faction() -> void:
	faction = (faction + 1) % _world.faction_names().size()
	var sects := _sects_of(faction)
	sect = sects.front() if not sects.is_empty() else -1
	verdict = ""
	_compose()


## Shows the core's verdict on an application to the sect shown, one of the
## WorldAdapter.APPLICATION_* constants, and reads the panel again, so an admitted person's sect
## carries the title's tag.
func show_verdict(refusal: int) -> void:
	verdict = APPLICATION_KEYS.get(refusal, "")
	var roster := _world.roster(_army)
	own_sect = _world.person_sect(roster[0]) if not roster.is_empty() else -1
	_compose()


## Works the panel with `event`: up and down choose among the shown faction's sects, Tab shows the
## next faction, Enter asks to apply to the sect shown, and the toggle key or Escape closes it.
## Returns "apply", "close", "done", or "" when the key did nothing.
func key(event: InputEventKey) -> String:
	if event.keycode == toggle_key or event.keycode == KEY_ESCAPE:
		return "close"
	if event.keycode == KEY_TAB:
		next_faction()
		return "done"
	if ENTER_KEYS.has(event.keycode):
		return "apply"
	return _done(DIRECTIONS.has(event.keycode) and move(DIRECTIONS[event.keycode]))


## Works the panel with a tap anywhere: it closes. Returns "close".
func tap(_at: Vector2) -> String:
	return "close"


func _sects_of(of_faction: int) -> Array[int]:
	var sects: Array[int] = []
	for index in _world.sect_count():
		if _world.sect_faction(index) == of_faction:
			sects.append(index)
	return sects


# Reads what the panel shows from the world, in the current locale.
func _compose() -> void:
	details.clear()
	cards.clear()
	if sect >= 0:
		var master := _world.sect_master(sect)
		details = {
			"name": Affiliation.sect_name(_world, sect),
			"faction": Affiliation.faction_name(_world, _world.sect_faction(sect)),
			"master": PersonName.of(_world, master) if master >= 0 else tr("HUD_LEADER_NONE"),
			"elders": tr("SECT_COUNT") % _world.sect_elders(sect).size(),
			"members": _world.sect_members(sect).size(),
			"cap": _world.sect_cap(sect),
			"seat": _seat_state(sect),
		}
		if master >= 0:
			cards.append(_card(master, true))
		var elders := Array(_world.sect_elders(sect)).filter(func(elder: int) -> bool: return elder != master)
		for elder: int in elders.slice(0, MAX_ELDER_CARDS):
			cards.append(_card(elder, false))
	var leader := _world.leader(faction)
	var leader_sect := _world.person_sect(leader) if leader >= 0 else -1
	var tabs: Array[String] = []
	for index in _world.faction_names().size():
		tabs.append(tr("SECT_TAB_" + _world.faction_names()[index].to_upper()))
	var sects: Array[Dictionary] = []
	for index in _sects_of(faction):
		var state := _seat_state(index)
		sects.append({"sect": index, "name": Affiliation.sect_name(_world, index), "state": state,
				"count": "%d/%d" % [_world.sect_members(index).size(), _world.sect_cap(index)]})
	column = {
		"tabs": tabs,
		"leader": PersonName.of(_world, leader, true) if leader >= 0 else tr("HUD_LEADER_NONE"),
		"leader_sect": Affiliation.sect_name(_world, leader_sect) if leader_sect >= 0 else "",
		"elects": _world.elects(faction),
		"duel": _world.passes_by_duel(faction),
		"sects": sects,
	}
	_gate_frame = 0
	var gate := _world.sect_gate(sect) if sect >= 0 else -1
	if gate >= 0:
		_gate_frame = _world.structure_stage(gate)
	queue_redraw()


func _card(person: int, master: bool) -> Dictionary:
	return {"person": person, "name": PersonName.of(_world, person, true), "master": master,
			"line": tr("GENEALOGY_AGE") % _world.person_age(person)}


# Held while its seat is; lost once the seat is taken from a sect that stands; fallen once the
# sect itself does not.
func _seat_state(index: int) -> String:
	if not _world.sect_stands(index):
		return "fallen"
	return "held" if _world.sect_seat_held(index) else "lost"


## Returns the width of each of `tabs` faction tabs.
static func tab_width(tabs: int) -> float:
	return floorf((COLUMN.size.x - 2 * TAB_INSET - (tabs - 1) * TAB_GAP) / maxi(tabs, 1))


func _draw() -> void:
	if _world == null:
		return
	_chrome(tr("SECT_KEYS"))
	# Title bar: the panel's name, the sect's, its faction, and whether it is the player's.
	var x := PANEL.position.x + 8
	var title := tr("SECT_TITLE")
	_text(Vector2(x, PANEL.position.y + 4), title, Palette.PAPER_6, true)
	x += _width(title) + 10
	if sect >= 0:
		_text(Vector2(x, PANEL.position.y + 4), details.name, Palette.GOLD_6, true)
		x += _width(details.name) + 8
		_text(Vector2(x, PANEL.position.y + 4), details.faction, _faction_colour(_world.sect_faction(sect)))
		x += _width(details.faction) + 10
		if sect == own_sect:
			var own := tr("SECT_OWN")
			_tag(Vector2(x + _width(own) + 6, PANEL.position.y + 5), own, Palette.JADE_2)
	var close_text := tr("SECT_CLOSE")
	_text(Vector2(PANEL.end.x - 8 - _width(close_text), PANEL.position.y + 4), close_text, Palette.STONE_4)
	if sect >= 0:
		_draw_seat()
		_draw_figures()
		_draw_cards()
	if not verdict.is_empty():
		var said := fit(tr(verdict), FIGURES.end.x - SEAT.position.x)
		_text(Vector2(SEAT.position.x, VERDICT_Y), said, Palette.JADE_7 if verdict == "APPLICATION_NONE" else Palette.CRIMSON_5)
	else:
		_text(SEAT.position + Vector2(8, 8), tr("SECT_NONE"), Palette.STONE_4)
	_draw_column()


func _draw_seat() -> void:
	_frame(SEAT, Palette.INK_1)
	var ground := Rect2(SEAT.position + Vector2(6, 6), Vector2(SEAT.size.x - 12, 70))
	draw_rect(ground, Palette.GRASS_3)
	draw_rect(Rect2(ground.position, Vector2(ground.size.x, 1)), Palette.GRASS_4)
	if _gate == null:
		_gate = load(ART + "structures/seat_gate.png")
	var side := _gate.get_height()
	# The gate's frames are its stages, side by side.
	draw_texture_rect_region(_gate, Rect2(SEAT.position + Vector2((SEAT.size.x - side) / 2.0, 6),
			Vector2(side, side)), Rect2(_gate_frame * side, 0, side, side))
	_text(SEAT.position + Vector2(8, 80), tr("SECT_SEAT"), Palette.GOLD_4)
	var state: String = details.seat
	var fill := Palette.JADE_2 if state == "held" else (Palette.CRIMSON_4 if state == "lost" else Palette.INK_4)
	_tag(SEAT.position + Vector2(SEAT.size.x - 8, 81), tr("SECT_" + state.to_upper()), fill)


func _draw_figures() -> void:
	var rows := [
		["SECT_FACTION", details.faction, _faction_colour(_world.sect_faction(sect))],
		["SECT_MASTER", details.master, Palette.PAPER_6],
		["SECT_ELDERS", details.elders, Palette.PAPER_6],
		["SECT_MEMBERS", "%d / %d" % [details.members, details.cap], Palette.PAPER_6],
	]
	for n in rows.size():
		var y := FIGURES.position.y + 4 + n * FIGURE_STEP
		if n > 0:
			draw_rect(Rect2(FIGURES.position.x, y - 4, FIGURES.size.x, 1), Palette.INK_4)
		var label := tr(rows[n][0])
		_text(Vector2(FIGURES.position.x + 2, y), label, Palette.GOLD_4)
		# The value keeps 4 pixels from its label.
		var value := fit(rows[n][1], FIGURES.size.x - 4 - _width(label) - 4)
		_text(Vector2(FIGURES.end.x - 2 - _width(value), y), value, rows[n][2])
	# Members against the cap, one pip a place: jade for a member, ink for a place still open.
	var cap: int = details.cap
	var step := mini(PIP, floori((FIGURES.size.x + 2) / maxf(cap, 1)))
	var left := FIGURES.position.x + floorf((FIGURES.size.x - cap * step + 2) / 2.0)
	for n in cap:
		var pip := Rect2(left + n * step, PIPS_Y, step - 2, 8)
		if n < details.members:
			draw_rect(pip, Palette.JADE_2)
			draw_rect(Rect2(pip.position, Vector2(pip.size.x, 1)), Palette.JADE_7)
			draw_rect(Rect2(pip.position + Vector2(0, 1), Vector2(1, 7)), Palette.JADE_4)
		else:
			draw_rect(pip, Palette.INK_4)
			draw_rect(Rect2(pip.position + Vector2(0, 7), Vector2(pip.size.x, 1)), Palette.INK_5)


func _draw_cards() -> void:
	_text(Vector2(SEAT.position.x, RANKS_Y), tr("SECT_RANKS"), Palette.GOLD_4)
	var width := FIGURES.end.x - SEAT.position.x
	draw_rect(Rect2(SEAT.position.x, RANKS_Y + 13, width, 1), Palette.INK_4)
	var gap := floorf((width - (MAX_ELDER_CARDS + 1) * CARD.x) / MAX_ELDER_CARDS)
	for n in cards.size():
		var card: Dictionary = cards[n]
		var rect := Rect2(Vector2(SEAT.position.x + n * (CARD.x + gap), CARDS_Y), CARD)
		_person_card(rect, card.person, card.name, card.line, false,
				Palette.GOLD_4 if card.master else Palette.INK_1, Palette.GOLD_3 if card.master else Palette.INK_4)
		if card.master:
			_tag(Vector2(rect.end.x - 3, CARDS_Y - 9), tr("SECT_MASTER"), Palette.GOLD_3)


func _draw_column() -> void:
	_frame(COLUMN, Palette.INK_1)
	var top := COLUMN.position.y + 6
	var w := tab_width(column.tabs.size())
	for index in column.tabs.size():
		var tab: String = column.tabs[index]
		var x: float = COLUMN.position.x + TAB_INSET + index * (w + TAB_GAP)
		draw_rect(Rect2(x, top, w, 14), Palette.JADE_2 if index == faction else Palette.INK_4)
		if index == faction:
			draw_rect(Rect2(x, top, w, 1), Palette.JADE_4)
		_text(Vector2(x + floorf((w - _width(tab)) / 2.0), top + 1), tab, Palette.PAPER_6 if index == faction else Palette.STONE_4)
	draw_rect(Rect2(COLUMN.position.x + 6, top + 14, COLUMN.size.x - 12, 1), Palette.JADE_2)
	var at := COLUMN.position
	_text(at + Vector2(8, 28), tr("HUD_LEADER"), Palette.GOLD_4)
	if column.elects:
		_tag(at + Vector2(COLUMN.size.x - 8, 29), tr("SECT_ELECTED"), Palette.AZURE_2)
	elif column.duel:
		_tag(at + Vector2(COLUMN.size.x - 8, 29), tr("HUD_DUEL"), Palette.CRIMSON_4)
	draw_rect(Rect2(at + Vector2(8, 44), Vector2(16, 16)), Palette.GRASS_3)
	draw_rect(Rect2(at + Vector2(8, 44), Vector2(16, 1)), Palette.GRASS_4)
	var leader := _world.leader(faction)
	if leader >= 0:
		_portrait(leader, Rect2(at + Vector2(8, 36), Vector2(16, 24)), false)
	var room := COLUMN.size.x - LEADER_TEXT_X - 8
	_text(at + Vector2(LEADER_TEXT_X, 46), fit(column.leader, room), Palette.PAPER_6)
	_text(at + Vector2(LEADER_TEXT_X, 58), fit(column.leader_sect, room), Palette.STONE_4)
	draw_rect(Rect2(at.x + 6, LIST_Y - 4, COLUMN.size.x - 12, 1), Palette.INK_4)
	_text(Vector2(at.x + 8, LIST_Y), tr("SECT_TITLE"), Palette.GOLD_4)
	var sects: Array = column.sects
	for n in mini(sects.size(), MAX_SECT_ROWS):
		var row: Dictionary = sects[n]
		var y := LIST_Y + 4 + (n + 1) * ROW_STEP
		draw_rect(Rect2(at.x + 6, y - 4, COLUMN.size.x - 12, 1), Palette.INK_4)
		var chosen: bool = row.sect == sect
		if chosen:
			draw_rect(Rect2(at.x + 4, y - 3, COLUMN.size.x - 8, 16), Palette.INK_2)
			draw_rect(Rect2(at.x + 4, y - 3, 1, 16), Palette.JADE_4)
		var right: String = row.count
		var colour := Palette.STONE_4
		if row.state == "lost":
			right = tr("SECT_LOST")
			colour = Palette.CRIMSON_5
		elif row.state == "fallen":
			right = tr("SECT_FALLEN")
			colour = Palette.STONE_3
		_text(Vector2(COLUMN.end.x - 8 - _width(right), y), right, colour)
		# The name keeps 4 pixels from what stands right of it.
		var name := fit(row.name, COLUMN.size.x - 18 - _width(right) - 4)
		var name_colour := Palette.STONE_3 if row.state == "fallen" else (Palette.GOLD_6 if chosen else Palette.PAPER_6)
		_text(Vector2(at.x + 10, y), name, name_colour)
