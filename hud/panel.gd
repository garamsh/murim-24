## What the HUD's full-screen panels share, as art/mockup/panel.lua shares it among their mockups:
## the game behind a panel dimmed by a checker of ink, the panel's frame, its title bar and the keys
## along its foot, a menu, a choice's slip, a key's cap, a person's card and portrait, and words
## drawn in the HUD's font, cut short where they would overrun. It holds no game rule.
extends Control

const ART := "res://art/"
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
const FONT_SIZE := 10
# The mockups' layout, in the native 480x270.
const PANEL := Rect2(20, 12, 440, 246)
# The native screen a panel is laid out in.
const SCREEN := Vector2(480, 270)
const CARD := Vector2(66, 40)
# Where a card's text starts.
const CARD_TEXT_X := 23
const ELLIPSIS := "…"
# docs/art-direction.md §Palette.
const INK := Color("2a2433")
const INK_DARK := Color("14101a")
const INK_LIGHT := Color("3d3647")
const GOLD_DARK := Color("8c6a2a")
const GOLD := Color("c99a3c")
const GOLD_LIGHT := Color("f0d080")
const LACQUER := Color("a83a2c")
const PAPER := Color("efe3c2")
const PAPER_DARK := Color("d9c9a3")
const STONE := Color("8a94a0")
const STONE_DARK := Color("66707d")
const JADE := Color("4fc49a")
const JADE_DARK := Color("2f8c6e")
const INK_PALE := Color("524a5e")
const GRASS := Color("3b6138")
const GRASS_LIGHT := Color("4f7a3f")
const AZURE_LIGHT := Color("5b9ae0")
const DEAD_TINT := Color(0.45, 0.45, 0.5)
# A menu's rows, and the height of a key's cap and a choice's slip.
const MENU_STEP := 18
const KEYCAP_HEIGHT := 13
const SLIP_HEIGHT := 14

var _world: WorldAdapter
var _font: Font
var _bodies := {}
var _scrim: ImageTexture


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	size = SCREEN
	texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
	visible = false


func close() -> void:
	visible = false


# The dimmed game, the panel's frame, its title bar and `keys` along its foot.
func _chrome(keys: String) -> void:
	_dim()
	_dialog(PANEL, "", "", keys)


# The game behind a panel, dimmed by a checker of ink over the whole room the panel stands in the
# middle of.
func _dim() -> void:
	_font = get_theme_default_font() if _font == null else _font
	draw_texture_rect(_scrim_texture(), _room(), true)


# The room the panel stands in the middle of, in its own pixels.
func _room() -> Rect2:
	return Rect2(-position, SCREEN + 2.0 * position)


# A panel in `rect` with `title` and `close` in its title bar and `keys` along its foot.
func _dialog(rect: Rect2, title: String, close: String, keys: String) -> void:
	_frame(rect, INK)
	draw_rect(Rect2(rect.position + Vector2(3, 3), Vector2(rect.size.x - 6, 16)), INK_DARK)
	draw_rect(Rect2(rect.position + Vector2(3, 19), Vector2(rect.size.x - 6, 1)), GOLD_DARK)
	draw_rect(Rect2(rect.position.x + 3, rect.end.y - 20, rect.size.x - 6, 1), GOLD_DARK)
	_text(rect.position + Vector2(8, 4), title, PAPER, true)
	_text(Vector2(rect.end.x - 8 - _width(close), rect.position.y + 4), close, STONE)
	_text(Vector2(rect.position.x + (rect.size.x - _width(keys)) / 2.0, rect.end.y - 17), keys, STONE)


# A menu of `items` from `top_left`, `width` wide, a row every MENU_STEP pixels. An item is a
# Dictionary of its `text` and, for an entry that cannot be taken yet, a `note` drawn beside it,
# greyed. Row `chosen` is lit and marked in jade.
func _menu(top_left: Vector2, width: float, items: Array[Dictionary], chosen: int) -> void:
	for n in items.size():
		var item := items[n]
		var y := top_left.y + n * MENU_STEP
		if n > 0:
			draw_rect(Rect2(top_left.x, y - 3, width, 1), INK_LIGHT)
		if n == chosen:
			draw_rect(Rect2(top_left.x, y - 2, width, 16), INK)
			draw_rect(Rect2(top_left.x, y - 2, 1, 16), JADE)
			for k in 4:
				draw_rect(Rect2(top_left.x + 4 + k, y + 2 + k, 1, 7 - 2 * k), GOLD_LIGHT)
		var note: String = item.get("note", "")
		var colour := STONE_DARK if not note.is_empty() else (GOLD_LIGHT if n == chosen else PAPER)
		_text(Vector2(top_left.x + 12, y), item.text, colour)
		if not note.is_empty():
			_text(Vector2(top_left.x + width - 4 - _width(note), y), note, STONE_DARK)


# A choice's slip `width` wide from `top_left`, its word centred: jade when `on`, ink when not.
func _slip(top_left: Vector2, width: float, text: String, on: bool) -> void:
	if on:
		draw_rect(Rect2(top_left, Vector2(width, SLIP_HEIGHT)), JADE_DARK)
		draw_rect(Rect2(top_left, Vector2(width, 1)), JADE)
	else:
		draw_rect(Rect2(top_left, Vector2(width, SLIP_HEIGHT)), INK_LIGHT)
		draw_rect(Rect2(top_left + Vector2(0, SLIP_HEIGHT - 1), Vector2(width, 1)), INK_PALE)
	_text(top_left + Vector2(floorf((width - _width(text)) / 2.0), 1), text, PAPER if on else STONE)


# A key's cap from `top_left` with `text` on it; returns its width.
func _keycap(top_left: Vector2, text: String) -> float:
	var w := keycap_width(text)
	draw_rect(Rect2(top_left, Vector2(w, KEYCAP_HEIGHT)), INK_PALE)
	draw_rect(Rect2(top_left, Vector2(w, KEYCAP_HEIGHT - 1)), INK_LIGHT)
	_text(top_left + Vector2(floorf((w - _width(text)) / 2.0), 0), text, PAPER)
	return w


# A person's card in `rect`: their portrait on a cell of ground, `name` cut to the card and `line`
# under it, greyed when `dead`, framed in `border` over `inner`.
func _person_card(rect: Rect2, person: int, name: String, line: String, dead: bool, border: Color,
		inner: Color) -> void:
	draw_rect(rect, border)
	draw_rect(rect.grow(-1), inner)
	draw_rect(rect.grow(-2), INK)
	# The body stands on a cell of ground, as it does in the world.
	draw_rect(Rect2(rect.position + Vector2(4, 20), Vector2(16, 16)), GRASS)
	draw_rect(Rect2(rect.position + Vector2(4, 20), Vector2(16, 1)), GRASS_LIGHT)
	_portrait(person, Rect2(rect.position + Vector2(4, 12), Vector2(16, 24)), dead)
	# A card's name stays within its 2-pixel frame.
	var room := CARD.x - CARD_TEXT_X - 2
	_text(rect.position + Vector2(CARD_TEXT_X, 6), fit(name, room), STONE if dead else PAPER)
	_text(rect.position + Vector2(CARD_TEXT_X, 21), line, STONE_DARK if dead else STONE)


# A word on a filled slip whose top-right is `top_right`, its text in paper; returns its width.
func _tag(top_right: Vector2, text: String, fill: Color) -> float:
	var w := _width(text) + 6
	draw_rect(Rect2(top_right - Vector2(w, 0), Vector2(w, 11)), fill)
	_text(top_right - Vector2(w - 3, 0), text, PAPER)
	return w


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


# Whether `at` falls on the close word `close` in `rect`'s title bar, or the bar's end beside it, so a
# finger finds it.
func _close_hit(rect: Rect2, close: String, at: Vector2) -> bool:
	_font = get_theme_default_font() if _font == null else _font
	var w := _width(close) + 16
	return Rect2(rect.end.x - w, rect.position.y, w, 20).has_point(at)


# The row of a menu `count` entries long, drawn by _menu from `top_left` and `width` wide, that
# `at` falls on, or -1 for none.
func _menu_row(top_left: Vector2, width: float, count: int, at: Vector2) -> int:
	var row := floori((at.y - top_left.y + 3) / MENU_STEP)
	if at.x < top_left.x or at.x >= top_left.x + width or row < 0 or row >= count:
		return -1
	return row


## Returns the width of a key's cap bearing `text`.
func keycap_width(text: String) -> float:
	return maxf(_width(text) + 6, KEYCAP_HEIGHT)


## Uses `font` for every word the panel draws.
func use_font(font: Font) -> void:
	_font = font


## Returns `text` as the panel draws it in `room` pixels: whole where it fits, and otherwise cut
## short with an ellipsis.
func fit(text: String, room: float) -> String:
	if _width(text) <= room:
		return text
	var kept := text
	while not kept.is_empty() and _width(kept + ELLIPSIS) > room:
		kept = kept.left(-1)
	return kept.strip_edges(false, true) + ELLIPSIS
