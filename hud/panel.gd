## What the HUD's full-screen panels share, as art/mockup/panel.lua shares it among their mockups:
## the game behind a panel dimmed by a checker of ink, the panel's frame, its title bar and the keys
## along its foot, a person's card and portrait, and words drawn in the HUD's font, cut short where
## they would overrun. It holds no game rule.
extends Control

const ART := "res://art/"
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
const FONT_SIZE := 10
# The mockups' layout, in the native 480x270.
const PANEL := Rect2(20, 12, 440, 246)
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
const STONE := Color("8a94a0")
const STONE_DARK := Color("66707d")
const JADE := Color("4fc49a")
const JADE_DARK := Color("2f8c6e")
const GRASS := Color("3b6138")
const GRASS_LIGHT := Color("4f7a3f")
const AZURE_LIGHT := Color("5b9ae0")
const DEAD_TINT := Color(0.45, 0.45, 0.5)

var _world: WorldAdapter
var _font: Font
var _bodies := {}
var _scrim: ImageTexture


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	size = Vector2(480, 270)
	texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
	visible = false


func close() -> void:
	visible = false


# The dimmed game, the panel's frame, its title bar and `keys` along its foot.
func _chrome(keys: String) -> void:
	_font = get_theme_default_font() if _font == null else _font
	draw_texture_rect(_scrim_texture(), Rect2(Vector2.ZERO, size), true)
	_frame(PANEL, INK)
	draw_rect(Rect2(PANEL.position + Vector2(3, 3), Vector2(PANEL.size.x - 6, 16)), INK_DARK)
	draw_rect(Rect2(PANEL.position + Vector2(3, 19), Vector2(PANEL.size.x - 6, 1)), GOLD_DARK)
	draw_rect(Rect2(PANEL.position.x + 3, PANEL.end.y - 20, PANEL.size.x - 6, 1), GOLD_DARK)
	_text(Vector2(PANEL.position.x + (PANEL.size.x - _width(keys)) / 2.0, PANEL.end.y - 17), keys, STONE)


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
