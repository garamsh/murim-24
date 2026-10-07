## The sect panel, drawn as art/mockup/sect_panel.png lays it out: a sect's seat, shown by its
## gate in its stage now and whether the seat is held, its faction, master, elders and members
## against its cap, and the cards of its master and elders; beside it a column for a faction, with
## its leader, whether the faction elects it or the lead passes by duel, and its sects; and under
## the cards, the core's verdict on the player's application to the sect shown. Every word comes
## from hud.csv through Godot's translations. What it shows is read from the world through the
## adapter. It holds no game rule.
extends "res://hud/panel.gd"

const PersonName := preload("res://hud/person_name.gd")
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
# The text of each faction's name, by its sprite set in FACTIONS: docs/art-direction.md §Palette's
# accents, a step lighter where the accent is too dark to read on ink.
const FACTION_TEXT: Array[Color] = [Color("5b9ae0"), Color("d63a2f"), Color("e8a830")]
# docs/art-direction.md §Palette, beyond the panels' own.
const CRIMSON := Color("d63a2f")
const CRIMSON_DARK := Color("9c1f24")
const JADE_LIGHT := Color("a6f0cf")
const AZURE_DARK := Color("1f3d7a")

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
			"name": _sect_name(sect),
			"faction": _faction_name(_world.sect_faction(sect)),
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
		sects.append({"sect": index, "name": _sect_name(index), "state": state,
				"count": "%d/%d" % [_world.sect_members(index).size(), _world.sect_cap(index)]})
	column = {
		"tabs": tabs,
		"leader": PersonName.of(_world, leader, true) if leader >= 0 else tr("HUD_LEADER_NONE"),
		"leader_sect": _sect_name(leader_sect) if leader_sect >= 0 else "",
		"elects": _world.elects(faction),
		"duel": _world.challenge_refusal(_army, faction) != WorldAdapter.CHALLENGE_NO_SUCCESSION,
		"sects": sects,
	}
	_gate_frame = 0
	if sect >= 0:
		_gate_frame = _gate_stage(_world.sect_seat(sect))
	queue_redraw()


func _card(person: int, master: bool) -> Dictionary:
	return {"person": person, "name": PersonName.of(_world, person, true), "master": master,
			"line": tr("GENEALOGY_AGE") % _world.person_age(person)}


func _sect_name(index: int) -> String:
	return tr("SECT_" + _world.sect_name(index).to_upper().replace(" ", "_"))


func _faction_name(index: int) -> String:
	return tr("FACTION_" + _world.faction_names()[index].to_upper())


# Held while its seat is; lost once the seat is taken from a sect that stands; fallen once the
# sect itself does not.
func _seat_state(index: int) -> String:
	if not _world.sect_stands(index):
		return "fallen"
	return "held" if _world.sect_seat_held(index) else "lost"


# The stage of the gate in `village`'s area, as WorldAdapter.structure_stage gives it.
func _gate_stage(village: int) -> int:
	var seat: Dictionary = _world.villages()[village]
	var area := Rect2i(seat.corner, seat.area)
	for structure in _world.structure_count():
		if _world.structure_kind(structure) == "seat_gate" and area.has_point(_world.structure_cell(structure)):
			return _world.structure_stage(structure)
	return 0


func _faction_colour(index: int) -> Color:
	var sprite := maxi(FACTIONS.find(_world.faction_names()[index].to_lower()), 0)
	return FACTION_TEXT[sprite]


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
	_text(Vector2(x, PANEL.position.y + 4), title, PAPER, true)
	x += _width(title) + 10
	if sect >= 0:
		_text(Vector2(x, PANEL.position.y + 4), details.name, GOLD_LIGHT, true)
		x += _width(details.name) + 8
		_text(Vector2(x, PANEL.position.y + 4), details.faction, _faction_colour(_world.sect_faction(sect)))
		x += _width(details.faction) + 10
		if sect == own_sect:
			var own := tr("SECT_OWN")
			_tag(Vector2(x + _width(own) + 6, PANEL.position.y + 5), own, JADE_DARK)
	var close_text := tr("SECT_CLOSE")
	_text(Vector2(PANEL.end.x - 8 - _width(close_text), PANEL.position.y + 4), close_text, STONE)
	if sect >= 0:
		_draw_seat()
		_draw_figures()
		_draw_cards()
	if not verdict.is_empty():
		var said := fit(tr(verdict), FIGURES.end.x - SEAT.position.x)
		_text(Vector2(SEAT.position.x, VERDICT_Y), said, JADE_LIGHT if verdict == "APPLICATION_NONE" else CRIMSON)
	else:
		_text(SEAT.position + Vector2(8, 8), tr("SECT_NONE"), STONE)
	_draw_column()


func _draw_seat() -> void:
	_frame(SEAT, INK_DARK)
	var ground := Rect2(SEAT.position + Vector2(6, 6), Vector2(SEAT.size.x - 12, 70))
	draw_rect(ground, GRASS)
	draw_rect(Rect2(ground.position, Vector2(ground.size.x, 1)), GRASS_LIGHT)
	if _gate == null:
		_gate = load(ART + "structures/seat_gate.png")
	var side := _gate.get_height()
	# The gate's frames are its stages, side by side.
	draw_texture_rect_region(_gate, Rect2(SEAT.position + Vector2((SEAT.size.x - side) / 2.0, 6),
			Vector2(side, side)), Rect2(_gate_frame * side, 0, side, side))
	_text(SEAT.position + Vector2(8, 80), tr("SECT_SEAT"), GOLD)
	var state: String = details.seat
	var fill := JADE_DARK if state == "held" else (CRIMSON_DARK if state == "lost" else INK_LIGHT)
	_tag(SEAT.position + Vector2(SEAT.size.x - 8, 81), tr("SECT_" + state.to_upper()), fill)


func _draw_figures() -> void:
	var rows := [
		["SECT_FACTION", details.faction, _faction_colour(_world.sect_faction(sect))],
		["SECT_MASTER", details.master, PAPER],
		["SECT_ELDERS", details.elders, PAPER],
		["SECT_MEMBERS", "%d / %d" % [details.members, details.cap], PAPER],
	]
	for n in rows.size():
		var y := FIGURES.position.y + 4 + n * FIGURE_STEP
		if n > 0:
			draw_rect(Rect2(FIGURES.position.x, y - 4, FIGURES.size.x, 1), INK_LIGHT)
		var label := tr(rows[n][0])
		_text(Vector2(FIGURES.position.x + 2, y), label, GOLD)
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
			draw_rect(pip, JADE_DARK)
			draw_rect(Rect2(pip.position, Vector2(pip.size.x, 1)), JADE_LIGHT)
			draw_rect(Rect2(pip.position + Vector2(0, 1), Vector2(1, 7)), JADE)
		else:
			draw_rect(pip, INK_LIGHT)
			draw_rect(Rect2(pip.position + Vector2(0, 7), Vector2(pip.size.x, 1)), INK_PALE)


func _draw_cards() -> void:
	_text(Vector2(SEAT.position.x, RANKS_Y), tr("SECT_RANKS"), GOLD)
	var width := FIGURES.end.x - SEAT.position.x
	draw_rect(Rect2(SEAT.position.x, RANKS_Y + 13, width, 1), INK_LIGHT)
	var gap := floorf((width - (MAX_ELDER_CARDS + 1) * CARD.x) / MAX_ELDER_CARDS)
	for n in cards.size():
		var card: Dictionary = cards[n]
		var rect := Rect2(Vector2(SEAT.position.x + n * (CARD.x + gap), CARDS_Y), CARD)
		_person_card(rect, card.person, card.name, card.line, false,
				GOLD if card.master else INK_DARK, GOLD_DARK if card.master else INK_LIGHT)
		if card.master:
			_tag(Vector2(rect.end.x - 3, CARDS_Y - 9), tr("SECT_MASTER"), GOLD_DARK)


func _draw_column() -> void:
	_frame(COLUMN, INK_DARK)
	var top := COLUMN.position.y + 6
	var w := tab_width(column.tabs.size())
	for index in column.tabs.size():
		var tab: String = column.tabs[index]
		var x: float = COLUMN.position.x + TAB_INSET + index * (w + TAB_GAP)
		draw_rect(Rect2(x, top, w, 14), JADE_DARK if index == faction else INK_LIGHT)
		if index == faction:
			draw_rect(Rect2(x, top, w, 1), JADE)
		_text(Vector2(x + floorf((w - _width(tab)) / 2.0), top + 1), tab, PAPER if index == faction else STONE)
	draw_rect(Rect2(COLUMN.position.x + 6, top + 14, COLUMN.size.x - 12, 1), JADE_DARK)
	var at := COLUMN.position
	_text(at + Vector2(8, 28), tr("HUD_LEADER"), GOLD)
	if column.elects:
		_tag(at + Vector2(COLUMN.size.x - 8, 29), tr("SECT_ELECTED"), AZURE_DARK)
	elif column.duel:
		_tag(at + Vector2(COLUMN.size.x - 8, 29), tr("HUD_DUEL"), CRIMSON_DARK)
	draw_rect(Rect2(at + Vector2(8, 44), Vector2(16, 16)), GRASS)
	draw_rect(Rect2(at + Vector2(8, 44), Vector2(16, 1)), GRASS_LIGHT)
	var leader := _world.leader(faction)
	if leader >= 0:
		_portrait(leader, Rect2(at + Vector2(8, 36), Vector2(16, 24)), false)
	var room := COLUMN.size.x - LEADER_TEXT_X - 8
	_text(at + Vector2(LEADER_TEXT_X, 46), fit(column.leader, room), PAPER)
	_text(at + Vector2(LEADER_TEXT_X, 58), fit(column.leader_sect, room), STONE)
	draw_rect(Rect2(at.x + 6, LIST_Y - 4, COLUMN.size.x - 12, 1), INK_LIGHT)
	_text(Vector2(at.x + 8, LIST_Y), tr("SECT_TITLE"), GOLD)
	var sects: Array = column.sects
	for n in mini(sects.size(), MAX_SECT_ROWS):
		var row: Dictionary = sects[n]
		var y := LIST_Y + 4 + (n + 1) * ROW_STEP
		draw_rect(Rect2(at.x + 6, y - 4, COLUMN.size.x - 12, 1), INK_LIGHT)
		var chosen: bool = row.sect == sect
		if chosen:
			draw_rect(Rect2(at.x + 4, y - 3, COLUMN.size.x - 8, 16), INK)
			draw_rect(Rect2(at.x + 4, y - 3, 1, 16), JADE)
		var right: String = row.count
		var colour := STONE
		if row.state == "lost":
			right = tr("SECT_LOST")
			colour = CRIMSON
		elif row.state == "fallen":
			right = tr("SECT_FALLEN")
			colour = STONE_DARK
		_text(Vector2(COLUMN.end.x - 8 - _width(right), y), right, colour)
		# The name keeps 4 pixels from what stands right of it.
		var name := fit(row.name, COLUMN.size.x - 18 - _width(right) - 4)
		var name_colour := STONE_DARK if row.state == "fallen" else (GOLD_LIGHT if chosen else PAPER)
		_text(Vector2(at.x + 10, y), name, name_colour)
