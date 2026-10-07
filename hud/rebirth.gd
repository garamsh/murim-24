## The rebirth screen, shown when the steered person dies and its soul is born again: the life just
## ended, by the person's card, family, years lived, sect, best attainment and children, beside the
## life begun, by the new body's card, family, faction and age. Enter goes on into the new body.
## Every word comes from hud.csv through Godot's translations, and what it shows is read from the
## world through the adapter. It holds no game rule.
extends "res://hud/panel.gd"

const PersonName := preload("res://hud/person_name.gd")
const Attainment := preload("res://hud/attainment.gd")
# The layout, in the native 480x270, in the other screens' frame.
const DIALOG := Rect2(70, 30, 340, 210)
const COLUMN_WIDTH := 150
const COLUMN_GAP := 20
const CARD_Y := 52
const ROWS_Y := 102
const ROW_STEP := 18

## The person whose life ended and the one its soul was born as.
var past := -1
var next := -1
## The rows each side shows, in order: each a Dictionary of `label`, a hud.csv key, and `value`, as
## drawn.
var past_rows: Array[Dictionary] = []
var next_rows: Array[Dictionary] = []


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED and _world != null:
		_compose()


## Opens the screen on `ended`, the person whose life ended, and `begun`, the one its soul was
## born as, in `world`.
func open(world: WorldAdapter, ended: int, begun: int) -> void:
	_world = world
	past = ended
	next = begun
	_compose()
	visible = true


## Works the screen with `key`: Enter or Escape goes on. Returns "go_on" when it does, and ""
## for any other key.
func key(event: InputEventKey) -> String:
	if event.keycode in [KEY_ENTER, KEY_KP_ENTER, KEY_ESCAPE]:
		return "go_on"
	return ""


# Reads what both sides show from the world, in the current locale.
func _compose() -> void:
	var sect := _world.person_sect(past)
	past_rows = [
		{"label": "REBIRTH_FAMILY", "value": _family(past)},
		{"label": "REBIRTH_YEARS", "value": tr("REBIRTH_YEARS_VALUE") % _world.person_age(past)},
		{"label": "REBIRTH_SECT", "value": _sect_name(sect) if sect >= 0 else tr("REBIRTH_NONE")},
		{"label": "REBIRTH_ATTAINMENT", "value": Attainment.of(_world, past)},
		{"label": "REBIRTH_CHILDREN", "value": tr("GENEALOGY_COUNT") % _world.person_children(past).size()},
	]
	var faction := _world.person_faction(next)
	next_rows = [
		{"label": "REBIRTH_FAMILY", "value": _family(next)},
		{"label": "REBIRTH_FACTION",
			"value": tr("FACTION_" + _world.faction_names()[faction].to_upper()) if faction >= 0 else tr("REBIRTH_NONE")},
		{"label": "REBIRTH_AGE", "value": tr("GENEALOGY_AGE") % _world.person_age(next)},
	]
	queue_redraw()


func _family(person: int) -> String:
	var name := _world.person_name(person)
	if name.is_empty():
		return tr("GENEALOGY_UNNAMED")
	var surname: String = name.surname if TranslationServer.get_locale().begins_with("ko") else name.surname_en
	return tr("GENEALOGY_FAMILY") % surname


func _sect_name(index: int) -> String:
	return tr("SECT_" + _world.sect_name(index).to_upper().replace(" ", "_"))


func _draw() -> void:
	if _world == null:
		return
	_dim()
	_dialog(DIALOG, tr("REBIRTH_TITLE"), tr("REBIRTH_CLOSE"), tr("REBIRTH_KEYS"))
	var left := DIALOG.position.x + 10
	_side(left, "REBIRTH_PAST", past, past_rows, true)
	draw_rect(Rect2(left + COLUMN_WIDTH + COLUMN_GAP / 2.0, DIALOG.position.y + 26, 1, DIALOG.size.y - 52), INK_LIGHT)
	_side(left + COLUMN_WIDTH + COLUMN_GAP, "REBIRTH_NEXT", next, next_rows, false)


# One side from `x`: its heading, the person's card, and its rows, each label against the left and
# its value, cut short where it would reach the label, against the right.
func _side(x: float, heading: String, person: int, rows: Array[Dictionary], dead: bool) -> void:
	_text(Vector2(x, DIALOG.position.y + 26), tr(heading), GOLD)
	draw_rect(Rect2(x, DIALOG.position.y + 39, COLUMN_WIDTH, 1), INK_LIGHT)
	_person_card(Rect2(Vector2(x, DIALOG.position.y + CARD_Y), CARD), person,
			PersonName.of(_world, person, true), tr("GENEALOGY_DEAD") if dead else "",
			dead, GOLD if not dead else INK_DARK, GOLD_DARK if not dead else INK_LIGHT)
	for n in rows.size():
		var y := DIALOG.position.y + ROWS_Y + n * ROW_STEP
		if n > 0:
			draw_rect(Rect2(x, y - 4, COLUMN_WIDTH, 1), INK_LIGHT)
		var label := tr(rows[n].label)
		_text(Vector2(x + 2, y), label, GOLD)
		var value := fit(rows[n].value, COLUMN_WIDTH - 4 - _width(label) - 4)
		_text(Vector2(x + COLUMN_WIDTH - 2 - _width(value), y), value, PAPER)
