## The genealogy panel, drawn as art/mockup/genealogy_panel.png lays it out: a three-generation
## tree around one person (their parents, them and their spouse, their children), each a card with
## a portrait, a name and an age or that they died, and a column with that person's details. Every
## word comes from hud.csv through Godot's translations. What it shows is read from the world: names,
## kin, ages and factions through the adapter; talent and lifespan are hidden, and shown unknown.
## It holds no game rule.
extends Control

const ART := "res://art/"
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
const MAX_CHILDREN := 3
const FONT_SIZE := 10
# The mockup's layout, in the native 480x270.
const PANEL := Rect2(20, 12, 440, 246)
const CARD := Vector2(66, 40)
const ROWS := [46, 112, 182]
const LEFT := 88.0
const RIGHT := 188.0
const CHILD_STEP := 78.0
# The details column is wider than the mockup's so that English, the longer, fits beside the portrait.
const DETAILS := Rect2(310, 38, 142, 196)
# docs/art-direction.md §Palette.
const INK := Color("2a2433")
const INK_DARK := Color("14101a")
const INK_LIGHT := Color("3d3647")
const GOLD_DARK := Color("8c6a2a")
const GOLD := Color("c99a3c")
const GOLD_LIGHT := Color("f0d080")
const LACQUER := Color("a83a2c")
const PAPER := Color("efe3c2")
const STONE := Color("8a94a0")
const STONE_DARK := Color("66707d")
const JADE := Color("4fc49a")
const JADE_DARK := Color("2f8c6e")
const GRASS := Color("3b6138")
const GRASS_LIGHT := Color("4f7a3f")
const AZURE_LIGHT := Color("5b9ae0")
const DEAD_TINT := Color(0.45, 0.45, 0.5)

## The person at the centre of the tree.
var centre := -1
## The person whose card is selected; Enter centres the tree on them.
var selected := -1
## The steered person, whose card carries the jade frame.
var own := -1
## The cards shown, each a Dictionary of `role` (parent, centre, spouse or child), `person`,
## `name`, `short` (the name as the card shows it), `line` (the age, or that they died) and `dead`.
var cards: Array[Dictionary] = []
## The centred person's details, by what they are: `name`, `family`, `sect`, `age`,
## `generation` (as the header shows it), `talent`, `lifespan`, `spouse` and `children`.
var details := {}

var _world: WorldAdapter
# Generations counted so far, by person; parents never change once a person is born.
var _generations := {}
var _font: Font
var _bodies := {}
var _scrim: ImageTexture


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	size = Vector2(480, 270)
	texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
	visible = false


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED and _world != null:
		show_person(_world, centre, own)


## Opens the panel on `person`, with `steered` the person the player plays.
func show_person(world: WorldAdapter, person: int, steered: int) -> void:
	if world != _world:
		_generations.clear()
	_world = world
	own = steered
	centre = person
	selected = person
	cards.clear()
	var parents := world.person_parents(person)
	for parent in parents.slice(0, 2):
		cards.append(_card("parent", parent))
	cards.append(_card("centre", person))
	var spouse := world.person_spouse(person)
	if spouse >= 0:
		cards.append(_card("spouse", spouse))
	var children := world.person_children(person)
	for child in children.slice(0, MAX_CHILDREN):
		cards.append(_card("child", child))
	var name := world.person_name(person)
	details = {
		"name": _name_of(person),
		"family": tr("GENEALOGY_FAMILY") % _surname(name),
		"sect": _sect(person),
		"age": str(world.person_age(person)),
		"generation": tr("GENEALOGY_GENERATION") % generation_of(person),
		"talent": tr("GENEALOGY_UNKNOWN"),
		"lifespan": tr("GENEALOGY_UNKNOWN"),
		"spouse": _name_of(spouse) if spouse >= 0 else tr("GENEALOGY_NONE"),
		"children": tr("GENEALOGY_COUNT") % children.size(),
	}
	visible = true
	queue_redraw()


## Returns `person`'s generation: 1 for someone with no parents, as the people a world begins
## with have none, and otherwise one more than their parents' deepest. It reads the parents the
## core holds and counts them, deciding nothing.
func generation_of(person: int) -> int:
	if _generations.has(person):
		return _generations[person]
	var deepest := 0
	for parent in _world.person_parents(person):
		deepest = maxi(deepest, generation_of(parent))
	_generations[person] = deepest + 1
	return deepest + 1


## Moves the selection to the nearest card in `direction`: up to the parents, down to the
## children, left and right along a row. Returns whether it moved.
func move(direction: Vector2i) -> bool:
	var from := _card_rect(_index_of(selected))
	var best := -1
	var best_distance := INF
	for index in cards.size():
		var at := _card_rect(index)
		var offset := at.get_center() - from.get_center()
		var along := offset.dot(Vector2(direction))
		if along <= 0.0:
			continue
		var distance := along + absf(offset.dot(Vector2(direction.y, direction.x))) * 2.0
		if distance < best_distance:
			best_distance = distance
			best = index
	if best < 0:
		return false
	selected = cards[best].person
	queue_redraw()
	return true


## Centres the tree on the selected person.
func centre_selected() -> void:
	show_person(_world, selected, own)


func close() -> void:
	visible = false


func _card(role: String, person: int) -> Dictionary:
	var dead := _world.person_health(person) == 0
	return {
		"role": role, "person": person, "name": _name_of(person), "dead": dead,
		"line": tr("GENEALOGY_DEAD") if dead else tr("GENEALOGY_AGE") % _world.person_age(person),
		"short": _name_of(person, true),
	}


# A person's name in the locale's order: surname first in Korean, given name first in English.
# `short` gives the English given name alone, which fits a card; Korean names fit whole.
func _name_of(person: int, short := false) -> String:
	var name := _world.person_name(person)
	if name.is_empty():
		return tr("GENEALOGY_UNNAMED")
	if TranslationServer.get_locale().begins_with("ko"):
		return name.surname + name.given
	if short:
		return name.given_en
	return "%s %s" % [name.given_en, name.surname_en]


func _surname(name: Dictionary) -> String:
	if name.is_empty():
		return tr("GENEALOGY_UNNAMED")
	return name.surname if TranslationServer.get_locale().begins_with("ko") else name.surname_en


func _sect(person: int) -> String:
	var faction := _world.person_faction(person)
	var names := _world.faction_names()
	if faction < 0 or faction >= names.size():
		return tr("GENEALOGY_NONE")
	return tr("FACTION_" + names[faction].to_upper())


func _index_of(person: int) -> int:
	for index in cards.size():
		if cards[index].person == person:
			return index
	return 0


# Where card `index` stands: parents over the centre and the spouse, children under them.
func _card_rect(index: int) -> Rect2:
	var card: Dictionary = cards[index]
	var role: String = card.role
	var x := LEFT
	var y: float = ROWS[1]
	if role == "parent":
		y = ROWS[0]
		x = LEFT if _nth(index, "parent") == 0 else RIGHT
	elif role == "spouse":
		x = RIGHT
	elif role == "child":
		y = ROWS[2]
		var count := _count("child")
		var knot := _couple_knot()
		x = knot - ((count - 1) * CHILD_STEP + CARD.x) / 2.0 + _nth(index, "child") * CHILD_STEP
	return Rect2(Vector2(x, y), CARD)


func _nth(index: int, role: String) -> int:
	var n := 0
	for k in index:
		if cards[k].role == role:
			n += 1
	return n


func _count(role: String) -> int:
	var n := 0
	for card in cards:
		if card.role == role:
			n += 1
	return n


# Where descent to the children starts: the couple's knot, or the centre's card alone.
func _couple_knot() -> float:
	if _count("spouse") > 0:
		return (LEFT + CARD.x + RIGHT) / 2.0
	return LEFT + CARD.x / 2.0


func _draw() -> void:
	if _world == null:
		return
	_font = get_theme_default_font() if _font == null else _font
	draw_texture_rect(_scrim_texture(), Rect2(Vector2.ZERO, size), true)
	_frame(PANEL, INK)
	# Title bar.
	draw_rect(Rect2(PANEL.position + Vector2(3, 3), Vector2(PANEL.size.x - 6, 16)), INK_DARK)
	draw_rect(Rect2(PANEL.position + Vector2(3, 19), Vector2(PANEL.size.x - 6, 1)), GOLD_DARK)
	var title := tr("GENEALOGY_TITLE")
	var family_x := 8 + _width(title) + 10
	_text(PANEL.position + Vector2(8, 4), title, PAPER, true)
	_text(PANEL.position + Vector2(family_x, 4), details.family, GOLD_LIGHT, true)
	_text(PANEL.position + Vector2(family_x + _width(details.family) + 14, 4), details.generation, STONE)
	var close_text := tr("GENEALOGY_CLOSE")
	_text(Vector2(PANEL.end.x - 8 - _width(close_text), PANEL.position.y + 4), close_text, STONE)
	# Generation labels.
	for n in 3:
		var label: String = tr(["GENEALOGY_PARENTS", "GENEALOGY_ROW_SELF", "GENEALOGY_CHILDREN"][n])
		_text(Vector2(PANEL.position.x + 10, ROWS[n] + 14), label, GOLD)
		draw_rect(Rect2(PANEL.position.x + 10, ROWS[n] + 28, 26, 1), INK_LIGHT)
	_kin_lines()
	for index in cards.size():
		_draw_card(index)
	_draw_details()
	# Keys along the foot.
	var keys := tr("GENEALOGY_KEYS")
	draw_rect(Rect2(PANEL.position.x + 3, PANEL.end.y - 20, PANEL.size.x - 6, 1), GOLD_DARK)
	_text(Vector2(PANEL.position.x + (PANEL.size.x - _width(keys)) / 2.0, PANEL.end.y - 17), keys, STONE)


# Lines of kin in gold: a marriage is a double line with a knot, descent a single line.
func _kin_lines() -> void:
	var parents := _count("parent")
	if parents == 2:
		_marriage(LEFT + CARD.x, RIGHT, ROWS[0] + CARD.y / 2.0)
	if parents > 0:
		var from_x := (LEFT + CARD.x + RIGHT) / 2.0 if parents == 2 else LEFT + CARD.x / 2.0
		var from_y: float = ROWS[0] + CARD.y / 2.0 + 3 if parents == 2 else ROWS[0] + CARD.y
		var bend: float = ROWS[0] + CARD.y + 10
		_line(Vector2(from_x, from_y), Vector2(from_x, bend))
		_line(Vector2(LEFT + CARD.x / 2.0, bend), Vector2(from_x, bend))
		_line(Vector2(LEFT + CARD.x / 2.0, bend), Vector2(LEFT + CARD.x / 2.0, ROWS[1] - 5))
	if _count("spouse") > 0:
		_marriage(LEFT + CARD.x, RIGHT, ROWS[1] + CARD.y / 2.0)
	var children := _count("child")
	if children > 0:
		var knot := _couple_knot()
		var top: float = ROWS[1] + CARD.y / 2.0 + 3 if _count("spouse") > 0 else ROWS[1] + CARD.y
		var bar: float = ROWS[2] - 10
		_line(Vector2(knot, top), Vector2(knot, bar))
		var first := _card_rect(_first("child")).get_center().x
		var last := _card_rect(_first("child") + children - 1).get_center().x
		_line(Vector2(first, bar), Vector2(last, bar))
		for k in children:
			var x := _card_rect(_first("child") + k).get_center().x
			_line(Vector2(x, bar), Vector2(x, ROWS[2]))


func _first(role: String) -> int:
	for index in cards.size():
		if cards[index].role == role:
			return index
	return -1


func _marriage(x0: float, x1: float, y: float) -> void:
	_line(Vector2(x0, y - 1), Vector2(x1, y - 1))
	_line(Vector2(x0, y + 1), Vector2(x1, y + 1))
	var knot := floorf((x0 + x1) / 2.0)
	draw_rect(Rect2(knot - 1, y - 2, 3, 5), GOLD_LIGHT)
	draw_rect(Rect2(knot, y, 1, 1), LACQUER)


func _line(from: Vector2, to: Vector2) -> void:
	draw_rect(Rect2(from.min(to).floor(), (to - from).abs().floor() + Vector2.ONE), GOLD_DARK)


func _draw_card(index: int) -> void:
	var card: Dictionary = cards[index]
	var rect := _card_rect(index)
	var own_card: bool = card.person == own
	var chosen: bool = card.person == selected
	var border := JADE if own_card else (GOLD if chosen else INK_DARK)
	draw_rect(rect, border)
	draw_rect(rect.grow(-1), JADE_DARK if own_card else (GOLD_DARK if chosen else INK_LIGHT))
	draw_rect(rect.grow(-2), INK)
	# The body stands on a cell of ground, as it does in the world.
	draw_rect(Rect2(rect.position + Vector2(4, 20), Vector2(16, 16)), GRASS)
	draw_rect(Rect2(rect.position + Vector2(4, 20), Vector2(16, 1)), GRASS_LIGHT)
	_portrait(card.person, Rect2(rect.position + Vector2(4, 12), Vector2(16, 24)), card.dead)
	_text(rect.position + Vector2(23, 6), card.short, STONE if card.dead else PAPER)
	_text(rect.position + Vector2(23, 21), card.line, STONE_DARK if card.dead else STONE)
	if own_card:
		var tag := tr("GENEALOGY_SELF")
		var w := _width(tag) + 6
		draw_rect(Rect2(rect.end.x - w - 3, rect.position.y - 5, w, 11), JADE_DARK)
		_text(Vector2(rect.end.x - w, rect.position.y - 5), tag, PAPER)


func _draw_details() -> void:
	_frame(DETAILS, INK_DARK)
	var at := DETAILS.position
	draw_rect(Rect2(at + Vector2(8, 8), Vector2(36, 44)), GRASS)
	draw_rect(Rect2(at + Vector2(8, 8), Vector2(36, 1)), GRASS_LIGHT)
	_portrait(centre, Rect2(at + Vector2(10, 4), Vector2(32, 48)), _world.person_health(centre) == 0)
	_text(at + Vector2(50, 10), details.name, PAPER, true)
	_text(at + Vector2(50, 24), details.family, GOLD_LIGHT)
	_text(at + Vector2(50, 38), details.sect, AZURE_LIGHT)
	var rows := [
		["GENEALOGY_AGE_LABEL", details.age], ["GENEALOGY_TALENT", details.talent],
		["GENEALOGY_LIFESPAN", details.lifespan], ["GENEALOGY_SPOUSE", details.spouse],
		["GENEALOGY_CHILDREN", details.children],
	]
	for n in rows.size():
		var y := at.y + 62 + n * 20
		draw_rect(Rect2(at.x + 6, y - 4, DETAILS.size.x - 12, 1), INK_LIGHT)
		_text(Vector2(at.x + 8, y), tr(rows[n][0]), GOLD)
		var value: String = rows[n][1]
		var unknown := value == tr("GENEALOGY_UNKNOWN")
		_text(Vector2(DETAILS.end.x - 8 - _width(value), y), value, STONE_DARK if unknown else PAPER)


# A person's idle frame of their faction's body, greyed once dead; someone serving in no army
# is drawn in the first faction's body, greyed.
func _portrait(person: int, rect: Rect2, dead: bool) -> void:
	var faction := _world.person_faction(person)
	var names := _world.faction_names()
	var sprite: String = FACTIONS[0]
	if faction >= 0 and faction < names.size():
		sprite = names[faction].to_lower()
	if not _bodies.has(sprite):
		_bodies[sprite] = load(ART + "bodies/" + sprite + ".png")
	var tint := DEAD_TINT if dead or faction < 0 else Color.WHITE
	draw_texture_rect_region(_bodies[sprite], rect, Rect2(0, 0, 16, 24), tint)


# A HUD panel: ink with a gold trim and lacquer corner studs.
func _frame(rect: Rect2, fill: Color) -> void:
	draw_rect(rect, INK)
	draw_rect(rect.grow(-1), INK_DARK)
	draw_rect(rect.grow(-2), fill)
	draw_rect(Rect2(rect.position + Vector2(2, 0), Vector2(rect.size.x - 4, 1)), GOLD)
	draw_rect(Rect2(rect.position + Vector2(2, rect.size.y - 1), Vector2(rect.size.x - 4, 1)), GOLD_DARK)
	draw_rect(Rect2(rect.position + Vector2(0, 2), Vector2(1, rect.size.y - 4)), GOLD_DARK)
	draw_rect(Rect2(rect.position + Vector2(rect.size.x - 1, 2), Vector2(1, rect.size.y - 4)), GOLD_DARK)
	for corner in [rect.position, Vector2(rect.end.x - 3, rect.position.y),
			Vector2(rect.position.x, rect.end.y - 3), rect.end - Vector2(3, 3)]:
		draw_rect(Rect2(corner, Vector2(3, 3)), GOLD)
		draw_rect(Rect2(corner + Vector2.ONE, Vector2.ONE), LACQUER)


func _text(top_left: Vector2, text: String, colour: Color, shadow := false) -> void:
	var baseline := top_left + Vector2(0, _font.get_ascent(FONT_SIZE))
	if shadow:
		draw_string(_font, baseline + Vector2.ONE, text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, INK_DARK)
	draw_string(_font, baseline, text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, colour)


func _width(text: String) -> float:
	return _font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x


# The game behind the panel shows through a checker of ink, so the panel reads as the one thing
# open; a checker, not a blend, keeps every pixel in the palette.
func _scrim_texture() -> ImageTexture:
	if _scrim == null:
		var image := Image.create_empty(2, 2, false, Image.FORMAT_RGBA8)
		image.set_pixel(0, 0, INK_DARK)
		image.set_pixel(1, 1, INK_DARK)
		_scrim = ImageTexture.create_from_image(image)
	return _scrim


## Uses `font` for every word the panel draws.
func use_font(font: Font) -> void:
	_font = font
