## The map window, drawn as evidence/map-mockup's approved window lays it out: the minimap's own
## image of what the player has seen, at the largest whole multiple at which it and its legend fit
## the HUD's room, in a frame sized to them; the player's place on it; and beside it the season and
## a legend of each faction's colour, villages seen in ruins, land not yet seen and the player.
## Every word comes from hud.csv through Godot's translations. It holds no game rule.
extends "res://hud/panel.gd"

## The colour unseen land is drawn in, on the map and the legend.
const FOG := Color("1d1a17")
## The least gap between the frame and the room's edge, the gap round the map and the legend, the
## legend's width, and the height of the frame's title bar and foot together.
const INSET := 12
const MARGIN := 10
const LEGEND_WIDTH := 132
const BARS := 42
const LINE := 14
const SWATCH := 14

var _map: Texture2D
## The cell the player stands on, or a point off the map while it has no body.
var player_cell := Vector2.ZERO
var _season := ""
var _factions: Array[Dictionary] = []


## Opens the window on `map`, the minimap's image a pixel a cell, with the player at `player`, a
## cell, or off the map for none, and `season`, as the HUD words it.
func show_map(world: WorldAdapter, map: Texture2D, player: Vector2, season: String) -> void:
	_world = world
	_map = map
	player_cell = player
	_season = season
	_factions.clear()
	for faction in world.faction_names().size():
		_factions.append({"name": Affiliation.faction_name(world, faction),
				"colour": Palette.ACCENTS[Affiliation.sprite(world, faction)]})
	visible = true
	queue_redraw()


## Updates the player's place and season while the window stays open.
func refresh(player: Vector2, season: String) -> void:
	if player_cell == player and _season == season:
		return
	player_cell = player
	_season = season
	queue_redraw()


## The whole multiple the map is drawn at: the largest at which it and its legend fit the room.
func map_scale() -> int:
	if _map == null:
		return 1
	var room := _room().size
	var across := (room.x - 2 * INSET - LEGEND_WIDTH - 3 * MARGIN) / _map.get_width()
	var down := (room.y - 2 * INSET - 2 * MARGIN - BARS) / _map.get_height()
	return maxi(floori(minf(across, down)), 1)


## The frame, in this control's pixels: sized to the map at map_scale() and the legend, and centred
## in the room.
func frame() -> Rect2:
	_font = get_theme_default_font() if _font == null else _font
	var shown := Vector2(_map.get_size()) * map_scale()
	var size := Vector2(shown.x + LEGEND_WIDTH + 3 * MARGIN, shown.y + 2 * MARGIN + BARS)
	size.x = maxf(size.x, _width(tr("MAP_KEYS")) + 2 * INSET)
	var room := _room()
	return Rect2((room.position + (room.size - size) / 2.0).floor(), size)


func key(event: InputEventKey) -> String:
	if event.keycode == toggle_key or event.keycode == KEY_ESCAPE:
		return "close"
	return ""


## Works the window with a tap anywhere: it closes. Returns "close".
func tap(_at: Vector2) -> String:
	return "close"


func _draw() -> void:
	if _map == null:
		return
	_font = get_theme_default_font() if _font == null else _font
	_dim()
	var outer := frame()
	_dialog(outer, tr("MAP_TITLE"), tr("MAP_CLOSE"), tr("MAP_KEYS"))
	var shown := Vector2(_map.get_size()) * map_scale()
	var at := outer.position + Vector2(MARGIN, 20 + MARGIN)
	draw_rect(Rect2(at - Vector2.ONE, shown + Vector2(2, 2)), Palette.GOLD_3, false)
	draw_texture_rect(_map, Rect2(at, shown), false)
	if Rect2(Vector2.ZERO, Vector2(_map.get_size())).has_point(player_cell):
		_you(at + (player_cell + Vector2(0.5, 0.5)) * map_scale())
	var x := at.x + shown.x + 2 * MARGIN
	var y := at.y
	_text(Vector2(x, y), _season, Palette.GOLD_6)
	y += 22
	_text(Vector2(x, y), tr("MAP_LEGEND"), Palette.STONE_4)
	y += 16
	for faction: Dictionary in _factions:
		_swatch(Vector2(x, y), faction.colour)
		_text(Vector2(x + SWATCH, y), fit(faction.name, LEGEND_WIDTH - SWATCH), Palette.PAPER_6)
		y += LINE
	y += 4
	_swatch(Vector2(x, y), Palette.STONE_3)
	_text(Vector2(x + SWATCH, y), fit(tr("MAP_RUINS"), LEGEND_WIDTH - SWATCH), Palette.PAPER_6)
	y += LINE
	_swatch(Vector2(x, y), FOG)
	_text(Vector2(x + SWATCH, y), fit(tr("MAP_UNSEEN"), LEGEND_WIDTH - SWATCH), Palette.PAPER_6)
	y += LINE
	_you(Vector2(x + 5, y + 6))
	_text(Vector2(x + SWATCH, y), fit(tr("MAP_YOU"), LEGEND_WIDTH - SWATCH), Palette.PAPER_6)


# The player's mark centred on `at`: gold ringed in ink.
func _you(at: Vector2) -> void:
	draw_rect(Rect2((at - Vector2(2.5, 2.5)).floor(), Vector2(5, 5)), Palette.INK_1)
	draw_rect(Rect2((at - Vector2(1.5, 1.5)).floor(), Vector2(3, 3)), Palette.GOLD_6)


func _swatch(at: Vector2, colour: Color) -> void:
	draw_rect(Rect2(at + Vector2(1, 2), Vector2(8, 8)), Palette.INK_1)
	draw_rect(Rect2(at + Vector2(2, 3), Vector2(6, 6)), colour)
