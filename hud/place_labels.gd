## The names of the places near the player (#185), so a sect's seat never reads as a village: a
## village's name over its grounds in paper, outlined in ink; a seat's on a plaque framed in gold, its
## sect's name in its faction's text colour over the faction's name. None over another, a place the
## HUD keeps clear or the room's edge. The play scene says which places are near and where.
## It holds no game rule.
extends "res://hud/panel.gd"

## The padding inside a seat's plaque, in the HUD's pixels.
const PLAQUE_PAD := Vector2(4, 2)

## The labels shown, each as `village`, `seat`, whether it names a sect's seat, `lines`, each a
## [text, colour], and `rect`, where it stands with its plaque or outline in the HUD's pixels.
var labels: Array[Dictionary] = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


## Names each village of `places`, a list of `village` indices into `world`'s villages() with the
## `anchor` where the foot of its label stands, which `to_hud` takes to the HUD's pixels: a sect's
## seat by its sect and faction, any other village by its kind and number. A label that would cover
## one already placed, any of `clear` or the edge of the room is left out.
func show_places(world: WorldAdapter, places: Array[Dictionary], clear: Array[Rect2],
		to_hud := Transform2D.IDENTITY) -> void:
	_font = get_theme_default_font() if _font == null else _font
	var seats := {}
	for sect in world.sect_count():
		seats[world.sect_seat(sect)] = sect
	var villages := world.villages()
	var room := Rect2(Vector2.ZERO, size)
	var taken := clear.duplicate()
	labels.clear()
	for place: Dictionary in places:
		var village: int = place.village
		var seat: bool = seats.has(village)
		var lines: Array = _lines(world, villages[village], village, seats.get(village, -1))
		var rect := label_rect(lines, seat, (to_hud * (place.anchor as Vector2)).floor())
		if not room.encloses(rect) or taken.any(func(other: Rect2) -> bool: return other.intersects(rect.grow(1))):
			continue
		taken.append(rect)
		labels.append({"village": village, "seat": seat, "lines": lines, "rect": rect})
	queue_redraw()


## Returns where a label of `lines` stands, its foot centred on `foot` in the HUD's pixels: on a
## plaque when it names a `seat`, outlined otherwise.
func label_rect(lines: Array, seat: bool, foot: Vector2) -> Rect2:
	var inner := Vector2(0, lines.size() * _font.get_height(FONT_SIZE))
	for line: Array in lines:
		inner.x = maxf(inner.x, _width(line[0]))
	var outer := inner + (PLAQUE_PAD * 2.0 if seat else Vector2(2, 2))
	return Rect2((foot - Vector2(outer.x / 2.0, outer.y)).floor(), outer.ceil())


# A seat's sect and faction, or a village's kind and number, as [text, colour] lines.
func _lines(world: WorldAdapter, village: Dictionary, index: int, sect: int) -> Array:
	if sect < 0:
		return [[tr("PLACE_" + String(village.kind).to_upper()) % (index + 1), Palette.PAPER_6]]
	var faction := world.sect_faction(sect)
	var colour := Palette.FACTION_TEXT[Affiliation.sprite(world, faction)]
	return [[Affiliation.sect_name(world, sect), colour], [Affiliation.faction_name(world, faction), Palette.PAPER_4]]


func _draw() -> void:
	for label: Dictionary in labels:
		var rect: Rect2 = label.rect
		var pad := PLAQUE_PAD if label.seat else Vector2(1, 1)
		if label.seat:
			draw_rect(rect, Palette.INK_2)
			draw_rect(rect, Palette.GOLD_4, false, 1.0)
		var line_height := _font.get_height(FONT_SIZE)
		for i in label.lines.size():
			var line: Array = label.lines[i]
			var x := rect.position.x + (rect.size.x - _width(line[0])) / 2.0
			var baseline := Vector2(x, rect.position.y + pad.y + i * line_height + _font.get_ascent(FONT_SIZE))
			if not label.seat:
				draw_string_outline(_font, baseline, line[0], HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, 2, Palette.INK_1)
			draw_string(_font, baseline, line[0], HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, line[1])
