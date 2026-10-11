## The names over the people near the player, as art/mockup/nameplates.gd proposed them
## (evidence/pr-199): each in the colour of how the person's faction stands toward the player's,
## outlined in ink. At most NAME_CAP at once: the target the player selected first, framed in gold,
## then the person under the cursor, underlined, then the hostile and then the nearest, and none
## over another name, a place the HUD keeps clear or the room's edge. The play scene says who stands
## where and whom the player selected and points at.
## It holds no game rule.
extends "res://hud/panel.gd"

## The most names shown at once.
const NAME_CAP := 8
## The colour of a name by how its person's faction stands toward the player's, as
## WorldAdapter.relation gives it: the player's side and the neutral no brighter than the hostile, so
## red reads first.
const COLOURS := {
	WorldAdapter.RELATION_OWN: Palette.JADE_2,
	WorldAdapter.RELATION_ALLY: Palette.JADE_2,
	WorldAdapter.RELATION_NEUTRAL: Palette.PAPER_3,
	WorldAdapter.RELATION_HOSTILE: Palette.CRIMSON_5,
	WorldAdapter.RELATION_AT_WAR: Palette.CRIMSON_5,
}
const HOSTILE_RELATIONS := [WorldAdapter.RELATION_HOSTILE, WorldAdapter.RELATION_AT_WAR]

## The names shown, each as `person`, `text`, `colour`, `rect`, where its words and their outline
## stand in the HUD's pixels, and `mark`: "selected", "hovered" or "".
var plates: Array[Dictionary] = []

# Each person's name as last shown, by person, and the locale it was shown in.
var _names := {}
var _locale := ""


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


## Names the people of `people` in `world`, nearest first, as world_art.gd's `near` gives them:
## packed arrays of each one's `person`, `head`, where its head stands, which `to_hud` takes to the
## HUD's pixels, and `relation`. `selected` comes first and `hovered` next when among them, then the
## hostile, then the rest, each in that order, until NAME_CAP are named, leaving out a name that would
## cover one already placed, any of `clear` or the edge of the room, the control's own rect. Only the
## heads of those tried are taken to the HUD, and no Dictionary is made for anyone not named.
func show_names(world: WorldAdapter, people: Dictionary, clear: Array[Rect2], selected := -1,
		hovered := -1, to_hud := Transform2D.IDENTITY) -> void:
	_font = get_theme_default_font() if _font == null else _font
	if world != _world or TranslationServer.get_locale() != _locale:
		_world = world
		_locale = TranslationServer.get_locale()
		_names.clear()
	var persons: PackedInt64Array = people.get("person", PackedInt64Array())
	var heads: PackedVector2Array = people.get("head", PackedVector2Array())
	var relations: PackedInt64Array = people.get("relation", PackedInt64Array())
	var order := PackedInt64Array()
	for i in persons.size():
		if persons[i] == selected:
			order.append(i)
	for i in persons.size():
		if persons[i] == hovered and hovered != selected:
			order.append(i)
	for hostile: bool in [true, false]:
		for i in persons.size():
			if (relations[i] in HOSTILE_RELATIONS) == hostile and persons[i] != selected and persons[i] != hovered:
				order.append(i)
	var room := Rect2(Vector2.ZERO, size)
	var taken := clear.duplicate()
	plates.clear()
	for i in order:
		if plates.size() == NAME_CAP:
			break
		var person := persons[i]
		var text := _name(person)
		var rect := plate_rect(text, (to_hud * heads[i]).floor())
		# A pixel's gap, so two names never read as one.
		if not room.encloses(rect) or taken.any(func(other: Rect2) -> bool: return other.intersects(rect.grow(1))):
			continue
		taken.append(rect)
		var mark := "selected" if person == selected else ("hovered" if person == hovered else "")
		plates.append({"person": person, "text": text, "colour": COLOURS[relations[i]], "rect": rect, "mark": mark})
	queue_redraw()


## Returns where `text`, named over a head at `head`, stands with its outline, in the HUD's pixels:
## centred over the head, its foot a pixel above it.
func plate_rect(text: String, head: Vector2) -> Rect2:
	var outlined := Vector2(_width(text), _font.get_height(FONT_SIZE)) + Vector2(2, 2)
	return Rect2((head - Vector2(outlined.x / 2.0, outlined.y + 1)).floor(), outlined)


func _name(person: int) -> String:
	if not _names.has(person):
		_names[person] = PersonName.of(_world, person)
	return _names[person]


func _draw() -> void:
	for plate: Dictionary in plates:
		var rect: Rect2 = plate.rect
		var baseline := rect.position + Vector2(1, 1 + _font.get_ascent(FONT_SIZE))
		draw_string_outline(_font, baseline, plate.text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, 2, Palette.INK_1)
		draw_string(_font, baseline, plate.text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, plate.colour)
		match plate.mark:
			"selected":
				draw_rect(rect, Palette.GOLD_4, false, 1.0)
			"hovered":
				draw_line(Vector2(rect.position.x, rect.end.y), rect.end, Palette.PAPER_6, 1.0)
