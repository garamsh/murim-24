## Draws a WorldAdapter's world in the approved art (docs/art-direction.md) at the 2.5D angle of
## ADR 0059, a cell CELL wide and deep and raised by the relief's levels: the ground drawn off the
## cell grid by ground.gd, baked once from the relief.gd derives and streamed round the screen,
## territory borders, buildings by stage, bodies by faction, the steered person's markers, a crimson ring round each body hostile
## to the person names are read around, as WorldAdapter.relation says, and what the fighting did
## through combat.gd, the struck drawn in the impact colour and knocked back and the dead falling
## and fading. Bodies are drawn between the positions of the last two ticks, as far between as the
## caller says the frame is, so they move on every frame; the core's positions stay the only truth.
## Each refresh also lists the people within NAME_REACH of that person, for their names to be drawn
## over them. Bodies are batched: one MultiMesh per row of cells on screen, sorted with buildings,
## trees and the raised rows of the ground by the row they stand on. It reads the world and holds no
## game rule.
extends Node2D

## Emitted after a tick changed the territory or the villages, once both are drawn anew.
signal region_changed

const Combat := preload("res://world_art/combat.gd")
const Ground := preload("res://world_art/ground.gd")
## A cell's width and depth on the ground, in pixels.
const CELL := Relief.CELL
const ART := "res://art/"
# Sprite sets by faction name; a world without faction names takes them in this order.
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
# The faction accents of docs/art-direction.md §Palette, in the order of FACTIONS.
const ACCENTS: Array[Color] = [Color("2f63b8"), Color("d63a2f"), Color("e8a830")]
const STAGES := ["intact", "damaged", "destroyed", "rubble"]
# The art for each kind of structure; its frames hold the bottom-left corner of the structure's
# footprint FOOTPRINT_INSET in from their left and bottom edges.
const BUILDINGS := {
	"house": "structures/house", "hall": "structures/hall", "well": "structures/well",
	"barrels": "structures/barrels", "fence": "structures/fence",
	"granary": "structures/granary", "storehouse": "structures/storehouse",
	"training_ground": "structures/training_ground", "manual_hall": "structures/manual_hall",
	"seat_gate": "structures/seat_gate",
	"smithy": "structures/smithy", "apothecary": "structures/apothecary",
}
# The kinds laid with the ground, over the terrain and under everything standing.
const GROUND_KINDS := ["training_ground"]
# The fields' tile for each FIELD_* state the core gives, in their order; in winter each takes
# its tag with WINTER after it.
const FIELD_TAGS := ["sown", "growing", "ripe", "fallow", "burnt"]
const WINTER := "_winter"
const WINTER_SEASON := 3
# The decorations of each village template, which never block or break: ground tiles over
# rectangles of cells and banners on cells, as offsets from the village's corner.
const DECORATIONS := "res://world_art/villages.json"
# The art of each kind of banner a template names, by the key it is named with.
const BANNERS := {"banner": "decorations/banner", "sect_banner": "decorations/sect_banner"}
const FOOTPRINT_INSET := 8
const WALK_FRAME_MS := 150
const CHEVRON_FRAME_MS := 400
# Pixels past the screen's edge that are still drawn, for sprites that reach across it.
const MARGIN := 64.0
# Frame positions in the body atlas built at startup, in pixels.
const BODY_FRAME := Vector2i(16, 24)
## The row of a body's frame its feet stand on, and how far above where a person stands its name's
## anchor, over its head, is, in pixels.
const FEET := 22
const HEAD := 23.0
## How long a body shows its strike frame after it struck, in milliseconds.
const STRIKE_MS := 200
# The frames of a faction's body sheet, `<tag>` and `<tag>_xray` for each of these, by its tags; and
# the atlas slot after the factions' frames that holds the shadow, centred in it.
const BODY_TAGS := ["idle", "walk", "back", "strike", "idle_hurt", "walk_hurt", "idle_maimed", "walk_maimed"]
# The suffix of a body's idle and walk tags by how wounded Combat.wound() says it is.
const WOUND_TAGS := ["", "_hurt", "_maimed"]
const SHADOW := "markers/shadow"
## Pixels a band's bodies are bucketed by for the test of what stands in front of them.
const BUCKET := 32.0
# The bucket of a place nothing can hide a body in.
const NO_OCCLUDERS: Array[Ground.Occluder] = []
## How far from the person names are read around another person's name is drawn, in cells.
const NAME_REACH := 8
## The relations, as WorldAdapter.relation gives them, of a person drawn ringed in RING.
const HOSTILE_RELATIONS := [WorldAdapter.RELATION_HOSTILE, WorldAdapter.RELATION_AT_WAR]
const RING := Color("d63a2f")
# Floats per body in a band's buffer: its 2D transform, its colour and its custom data.
const BODY_FLOATS := 16

var world: WorldAdapter
var steered := -1
## The army whose lead names are read around and relations read toward, or -1 for none.
var viewer := -1
## The people within NAME_REACH of the viewer's lead in the last refresh, the lead itself left out,
## nearest first, each as `person`, `head`, where its head is drawn in pixels, `relation`, how its
## faction stands toward the viewer's, and `cells`, how far it stands from the lead.
var near: Array[Dictionary] = []
## Bodies drawn in the last refresh, after culling to the screen.
var drawn := 0
## The territory the borders were last drawn from, as WorldAdapter.territory() gives it.
var owners := PackedInt64Array()

## The relief the ground is drawn with, and bodies and buildings raised by.
var relief: Relief

var _width := 0
var _height := 0
var _sprite_of_faction := PackedInt64Array()
var _atlas: ImageTexture
var _bands: Array[MultiMeshInstance2D] = []
# The first frame of each of BODY_TAGS, and of its outline, in a faction's sheet; the atlas's width
# for one faction; and where the shadow's slot lies in the atlas.
var _body_frames := {}
var _faction_width := 0
var _shadow_x := 0
# When each person last struck, in milliseconds, by person; never, for one past its end.
var _struck_at := PackedInt64Array()
## Bodies drawn over what stands in front of them as an outline in the last refresh.
var outlined := 0
# What can stand in front of a body, by BUCKET, rebuilt when the ground shows other areas, a
# structure is laid or a structure's frame changes; and how many times a structure's frame changed.
var _occluders: Dictionary[Vector2i, Array] = {}
var _occluders_for := Vector3i(-1, -1, -1)
var _structure_frames := 0
# Bodies change only on a tick, so they are read once per tick: positions now and a tick ago,
# whether each army's roster is the one it had a tick ago, and each army's sprite set. Every body
# is one person.
var _tick_positions: Array[PackedVector2Array] = []
var _last_positions: Array[PackedVector2Array] = []
var _rosters: Array[PackedInt64Array] = []
var _same_roster: Array[bool] = []
var _army_sprites := PackedInt64Array()
# How each army's faction stands toward the viewer's, read once a tick.
var _relations := PackedInt64Array()
## The layer of the villages' ground decorations, laid over the ground: paving and road.
var built: TileMapLayer
# How many villages have their decorations laid, the world's order kept.
var _decorated := 0
var _layouts := {}
var _ground_sheet := {}
var _banner_sheets := {}
var _colours: Array[Color] = []
## The layer of the fields, each cell tiled by its field's state and the season.
var fields: TileMapLayer
## One sprite per structure, in the world's order; null for a structure with no art.
var structure_sprites: Array[Sprite2D] = []
# Each building's frames by footprint, one per stage in the order of STAGES.
var _stage_frames := {}
var _field_sheet := {}
# The tag each field's cells were last tiled with, by structure.
var _field_tags := {}
var _chevron_frames: Array[AtlasTexture] = []
var _material: ShaderMaterial

var _laid: Node2D
var _territory: Territory
var _standing: Node2D
## What the fighting of the last ticks did: uses, hits, deaths and the steered person's target.
var combat: Combat
var _ring: Sprite2D
var _chevron: Sprite2D


## Lays out `shown`'s terrain, landforms and buildings, and remembers `steered_army`, the army
## whose lead body carries the player's markers, or -1 for none.
func show_world(shown: WorldAdapter, steered_army: int) -> void:
	# Read here rather than on ready: a test readies only the scene it instantiates.
	_laid = $Laid
	_territory = $Territory
	_standing = $Standing
	_ring = $Ring
	_chevron = $Chevron
	combat = $Combat
	world = shown
	steered = steered_army
	var rows := world.map_rows()
	_height = rows.size()
	_width = rows[0].length() if _height > 0 else 0
	relief = Relief.from_terrain(Vector2i(_width, _height), _cell_kinds(rows))
	var names := world.faction_names()
	_sprite_of_faction.clear()
	for faction in maxi(names.size(), FACTIONS.size()):
		var name: String = names[faction].to_lower() if faction < names.size() else FACTIONS[faction % FACTIONS.size()]
		_sprite_of_faction.append(maxi(FACTIONS.find(name), 0))
	_colours.clear()
	for faction in _sprite_of_faction:
		_colours.append(ACCENTS[faction])
	owners = world.territory()
	_territory.show_borders(_borders(), _colours)
	for kind: String in BUILDINGS:
		var sheet := _sheet(BUILDINGS[kind])
		var frames: Array[AtlasTexture] = []
		for stage: String in STAGES:
			frames.append(_region(sheet, stage, 0))
		_stage_frames[kind] = frames
	_lay_decorations()
	_lay_fields()
	_mark_built()
	_lay_structures()
	_lay_ground()
	_build_body_atlas()
	var player := _sheet("markers/player_marker")
	_ring.texture = _region(player, "ring", 0)
	_chevron_frames.append(_region(player, "chevron", 0))
	_chevron_frames.append(_region(player, "chevron", 1))
	_ring.visible = steered >= 0
	_chevron.visible = steered >= 0
	combat.to_screen = to_screen
	combat.begin(world)
	_add_armies()
	name_around(steered)


## Reads names around army `army`'s lead and relations toward its faction from now on, or neither
## for an army of -1.
func name_around(army: int) -> void:
	viewer = army
	_read_relations()


func _read_relations() -> void:
	_relations.resize(_tick_positions.size())
	var faction := world.army_faction(viewer) if viewer >= 0 else -1
	for army in _relations.size():
		_relations[army] = world.relation(faction, world.army_faction(army)) if faction >= 0 else -1


## Records the bodies' positions as a tick leaves them, so a body that moved since the last
## tick is drawn walking, facing the way it went; and redraws the borders and lays the new
## villages' decorations when the tick changed the territory or founded a village.
func note_tick() -> void:
	for army in _tick_positions.size():
		_last_positions[army] = _tick_positions[army]
		_tick_positions[army] = world.body_positions(army)
		var roster := world.roster(army)
		_same_roster[army] = roster == _rosters[army]
		_rosters[army] = roster
	_add_armies()
	_read_relations()
	var ms := Time.get_ticks_msec()
	combat.note_tick(world, steered, ms, _rosters, _tick_positions)
	for strike: Dictionary in combat.strikes:
		if strike.ms == ms:
			if strike.person >= _struck_at.size():
				_struck_at.resize(strike.person + 1)
			_struck_at[strike.person] = ms
	var now := world.territory()
	var villages := world.villages()
	if now == owners and villages.size() == _decorated:
		return
	owners = now
	_territory.show_borders(_borders(), _colours)
	_decorate(villages)
	region_changed.emit()


# Reads the armies the world formed since they were last read, settlers and raiding parties
# among them: their positions and their sprite set.
func _add_armies() -> void:
	for army in range(_tick_positions.size(), world.army_count()):
		_tick_positions.append(world.body_positions(army))
		_last_positions.append(_tick_positions[army])
		_rosters.append(world.roster(army))
		_same_roster.append(true)
		_army_sprites.append(_sprite_of_faction[world.army_faction(army) % _sprite_of_faction.size()])
		combat.accents.append(ACCENTS[_army_sprites[army]])


## Returns where body `body` of army `army` stands as drawn, its feet, in pixels: `between` of the
## way from where the tick before last left it to where the last tick did, so a body moves on every
## frame and not only on a tick, raised with the ground under it. While the army's roster changed on
## the last tick, the bodies are drawn where it left them, since a body's index may then name
## another person than a tick before.
func body_at(army: int, body: int, between: float) -> Vector2:
	return to_screen(_sub_at(army, body, between))


func _sub_at(army: int, body: int, between: float) -> Vector2:
	var now := _tick_positions[army][body]
	var before := _last_positions[army]
	if _same_roster[army] and body < before.size():
		now = before[body].lerp(now, clampf(between, 0.0, 1.0))
	return now


## Where a point in sub-cell units lies on the unraised ground, in pixels.
func ground_point(sub: Vector2) -> Vector2:
	return sub * CELL / WorldAdapter.CELL_SIZE


## Where a point in sub-cell units is drawn, in pixels: on the ground, raised with it.
func to_screen(sub: Vector2) -> Vector2:
	var on_ground := ground_point(sub)
	return on_ground - Vector2(0, relief.lift_at(on_ground))


## The point in sub-cell units whose ground is drawn at `screen`, in pixels: what a click there
## points at.
func to_sub(screen: Vector2) -> Vector2:
	return relief.ground_under(screen) / CELL * WorldAdapter.CELL_SIZE


## The cell whose ground is drawn at `screen`, in pixels.
func cell_at(screen: Vector2) -> Vector2i:
	return Vector2i((to_sub(screen) / WorldAdapter.CELL_SIZE).floor())


## Updates buildings and bodies within `view`, the part of the world on screen, in pixels, the
## bodies `between` of the way from the tick before last to the last, as body_at() places them.
func refresh(view: Rect2, between := 1.0) -> void:
	var now := Time.get_ticks_msec()
	$Ground.stream(view)
	_tile_fields()

	_lay_structures()
	for structure in structure_sprites.size():
		var sprite := structure_sprites[structure]
		if sprite != null:
			var stage := world.structure_stage(structure)
			var texture: Texture2D = sprite.get_meta("frames")[stage]
			if sprite.texture != texture:
				sprite.texture = texture
				_structure_frames += 1
				if not GROUND_KINDS.has(world.structure_kind(structure)):
					_shadow_of(structure)
	var shown := view.grow(MARGIN)
	# Raised ground draws its rows above where they lie, so the rows on screen reach further down.
	var first_row := maxi(int(shown.position.y / CELL.y), 0)
	var last_row := mini(int((shown.end.y + relief.highest() * Relief.LEVEL_PX) / CELL.y) + 1, _height + 1)
	var buffers: Array[PackedFloat32Array] = []
	buffers.resize(maxi(last_row - first_row + 1, 0))
	# Each band's shadows, drawn before its bodies, and the outlines of the bodies something hides.
	var shadows: Array[PackedFloat32Array] = []
	shadows.resize(buffers.size())
	var outlines := PackedFloat32Array()
	_gather_occluders()
	var walk := (now / WALK_FRAME_MS) % 4
	drawn = 0
	outlined = 0
	near.clear()
	var named := viewer >= 0 and not _tick_positions[viewer].is_empty()
	var named_from := _sub_at(viewer, 0, between) if named else Vector2.ZERO
	var share := clampf(between, 0.0, 1.0)
	var pixels := CELL / WorldAdapter.CELL_SIZE
	for army in _tick_positions.size():
		var positions := _tick_positions[army]
		var before := _last_positions[army]
		var roster := _rosters[army]
		var sprite := _army_sprites[army]
		var tint := _tint(army)
		var moving := _same_roster[army]
		for body in positions.size():
			var person := roster[body]
			# As _sub_at() gives it, written out: this loop runs for every body on every frame.
			var sub := before[body].lerp(positions[body], share) if moving and body < before.size() else positions[body]
			var on_ground := sub * pixels
			var at := on_ground - Vector2(0, relief.lift_at(on_ground)) + combat.knock(person, now)
			if not shown.has_point(at):
				continue
			var cells := (sub - named_from).length() / WorldAdapter.CELL_SIZE
			if named and cells <= NAME_REACH and (army != viewer or body > 0):
				near.append({"person": person, "head": at - Vector2(0, HEAD),
					"relation": _relations[army], "cells": cells})
			var row := clampi(int(on_ground.y / CELL.y), first_row, last_row)
			if buffers.is_empty():
				continue
			var heading := positions[body] - before[body] if body < before.size() else Vector2.ZERO
			var tag := "idle"
			var index := 0
			var struck := _struck_at[person] if person < _struck_at.size() else -STRIKE_MS
			if struck > 0 and now - struck < STRIKE_MS:
				tag = "strike"
				index = 0 if now - struck < STRIKE_MS / 2 else 1
			elif heading != Vector2.ZERO:
				tag = "back" if heading.y < 0.0 and -heading.y > absf(heading.x) else "walk"
				index = walk
			# Facing the viewer, standing or walking, a wounded body stoops and is stained.
			if tag == "idle" or tag == "walk":
				tag += WOUND_TAGS[combat.wound(person)]
			var centre := Vector2(at.x, at.y - FEET + BODY_FRAME.y / 2.0)
			var frame := Rect2(Vector2(sprite * _faction_width + (_body_frames[tag] + index) * BODY_FRAME.x, 0), BODY_FRAME)
			var band := row - first_row
			var origin := Vector2(0, _band_y(row))
			# Rounded before the band's origin is taken off, so a body stands on the same whole pixel in
			# every band.
			_pack(shadows[band], Rect2(Vector2(_shadow_x, 0), BODY_FRAME), at.round() - origin, false, Color.WHITE)
			var white := combat.flash(person, now)
			_pack(buffers[band], frame, centre.round() - origin, heading.x < 0.0, tint, white)
			if _hidden(Rect2(centre - Vector2(BODY_FRAME) / 2.0, BODY_FRAME).grow(-2), origin.y):
				var outline := frame
				outline.position.x += (_body_frames[tag + "_xray"] - _body_frames[tag]) * BODY_FRAME.x
				_pack(outlines, outline, centre, heading.x < 0.0, Color.WHITE, white, 1.0, 0.0, frame.position)
				outlined += 1
			drawn += 1
	# The dead fall a quarter turn about their feet and fade where they last stood, flashing as
	# their killing blow lands.
	for death: Dictionary in combat.dying(now):
		var at: Vector2 = death.at
		if not shown.has_point(at) or buffers.is_empty():
			continue
		var row := clampi(int(death.ground.y / CELL.y), first_row, last_row)
		_pack(buffers[row - first_row], Rect2(Vector2(_army_sprites[death.army] * _faction_width, 0), BODY_FRAME),
				Vector2(at.x, at.y - FEET + BODY_FRAME.y / 2.0).round() - Vector2(0, _band_y(row)), false, _tint(death.army),
				death.flash, death.alpha, death.fallen * PI / 2.0)
	for band in buffers.size():
		shadows[band].append_array(buffers[band])
	_fill_bands(shadows, first_row)
	# With nothing outlined the pass is not drawn, so the screen is not copied for it.
	var outlining := _outlines()
	outlining.visible = outlined > 0
	_fill(outlining, outlines)
	near.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a.cells < b.cells)
	# The markers follow the steered person while it lives.
	var alive := steered >= 0 and not _tick_positions[steered].is_empty()
	_ring.visible = alive
	_chevron.visible = alive
	if alive:
		var lead := body_at(steered, 0, between).round()
		_ring.position = lead
		_chevron.position = lead
		_chevron.texture = _chevron_frames[(now / CHEVRON_FRAME_MS) % 2]
	var aimed: Vector2i = combat.place_of(combat.target)
	combat.show_at(view, now, body_at(aimed.x, aimed.y, between) if aimed.x >= 0 else Vector2.ZERO)
	$Drops.show_drops(combat.shown_drops(now))


# The ring army `army`'s bodies are drawn in: RING while it is hostile to the person names are read
# around, and otherwise white, no ring.
func _tint(army: int) -> Color:
	return RING if _relations[army] in HOSTILE_RELATIONS else Color.WHITE


# Each cell's terrain kind name: the region's, or for a world without one, plain where the map
# lets bodies pass and mountain where it does not.
func _cell_kinds(rows: PackedStringArray) -> PackedStringArray:
	var kinds := PackedStringArray()
	var names := world.terrain_names()
	var terrain := world.terrain()
	for y in _height:
		for x in _width:
			if terrain.size() == _width * _height:
				kinds.append(names[terrain[y * _width + x]])
			else:
				kinds.append("plain" if rows[y][x] == "." else "mountain")
	return kinds


# A hash of a cell, for the seeded choice of a variant.
func _hash(x: int, y: int, salt: int) -> int:
	var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
	h = (h ^ (h >> 13)) * 1274126177
	return absi(h ^ (h >> 16))


# Marks in the relief every cell a structure stands on and every cell paved or a road, so the
# ground there is drawn straight up to what is built on it.
func _mark_built() -> void:
	for structure in world.structure_count():
		var corner := world.structure_cell(structure)
		var footprint := world.structure_footprint(structure)
		for y in range(corner.y, mini(corner.y + footprint.y, _height)):
			for x in range(corner.x, mini(corner.x + footprint.x, _width)):
				relief.build(Vector2i(x, y))
	for cell in built.get_used_cells():
		if cell.x >= 0 and cell.y >= 0 and cell.x < _width and cell.y < _height:
			relief.build(cell)


# Prepares the ground to be baked from the relief, with a decorative tree on no village and no
# structure.
func _lay_ground() -> void:
	var cleared: Array[Rect2i] = []
	for village: Dictionary in world.villages():
		cleared.append(Rect2i(village.corner, village.area).grow(1))
	for structure in world.structure_count():
		cleared.append(Rect2i(world.structure_cell(structure), world.structure_footprint(structure)))
	var ground: Node2D = $Ground
	ground.standing = _standing
	ground.prepare(relief, {"open_at": func(cell: Vector2i) -> bool:
			return not cleared.any(func(rect: Rect2i) -> bool: return rect.has_point(cell))})


# Each edge between two cells in different territories, as [from, to, faction of the cell on
# its left or top, faction on its right or bottom], -1 for a cell in none, on the unraised ground.
func _borders() -> Array:
	var borders := []
	if owners.size() != _width * _height:
		return borders
	for y in _height:
		for x in _width:
			var here := owners[y * _width + x]
			if x + 1 < _width and owners[y * _width + x + 1] != here:
				borders.append([Vector2(x + 1, y) * CELL, Vector2(x + 1, y + 1) * CELL,
						here, owners[y * _width + x + 1]])
			if y + 1 < _height and owners[(y + 1) * _width + x] != here:
				borders.append([Vector2(x, y + 1) * CELL, Vector2(x + 1, y + 1) * CELL,
						here, owners[(y + 1) * _width + x]])
	return borders


# One sprite per structure whose kind has art, the bottom edge of its footprint as its
# position, raised with the ground there, for each structure laid since they were last laid, those
# the world's growth lays among them; a kind of GROUND_KINDS on the ground, the rest standing. A
# structure with no kind, as from WorldAdapter.create(), is not drawn.
func _lay_structures() -> void:
	var laid := structure_sprites.size()
	structure_sprites.resize(world.structure_count())
	for structure in range(laid, world.structure_count()):
		var corner := world.structure_cell(structure)
		var footprint := world.structure_footprint(structure)
		var kind := world.structure_kind(structure)
		if not _stage_frames.has(kind):
			continue
		var frames: Array[AtlasTexture] = _stage_frames[kind]
		var sprite := Sprite2D.new()
		sprite.centered = false
		sprite.position = Vector2(corner.x, corner.y + footprint.y) * CELL
		var lift := relief.lift_at(sprite.position + Vector2(footprint.x * CELL.x / 2.0, -CELL.y / 2.0))
		sprite.offset = -Vector2(FOOTPRINT_INSET, frames[0].get_size().y - FOOTPRINT_INSET) - Vector2(0, lift)
		sprite.set_meta("frames", frames)
		if GROUND_KINDS.has(kind):
			sprite.position += sprite.offset
			sprite.offset = Vector2.ZERO
			_laid.add_child(sprite)
		else:
			_standing.add_child(sprite)
		structure_sprites[structure] = sprite
		if not GROUND_KINDS.has(kind):
			_shadow_of(structure)


# Sets the shadow standing structure `structure` casts at its stage, from its footprint's top-left
# raised with the ground, in the one node that draws every structure's shadow.
func _shadow_of(structure: int) -> void:
	var corner := world.structure_cell(structure)
	var footprint := world.structure_footprint(structure)
	var bottom := Vector2(corner.x, corner.y + footprint.y) * CELL
	var lift := relief.lift_at(bottom + Vector2(footprint.x * CELL.x / 2.0, -CELL.y / 2.0))
	$Shadows.set_shadow(structure, Vector2(corner) * CELL - Vector2(0, lift), footprint, world.structure_stage(structure))


# A layer of tiles a cell each, from the 16 x 12 frames of `sheet`.
func _tile_layer(sheet: Dictionary) -> TileMapLayer:
	var tiles := TileSet.new()
	tiles.tile_size = Vector2i(CELL)
	var source := TileSetAtlasSource.new()
	source.texture = sheet.texture
	source.texture_region_size = Vector2i(CELL)
	for frame in sheet.texture.get_width() / int(CELL.x):
		source.create_tile(Vector2i(frame, 0))
	tiles.add_source(source)
	var layer := TileMapLayer.new()
	layer.tile_set = tiles
	_laid.add_child(layer)
	return layer


# Lays the layer the fields are tiled on, one tile of fields.png per cell, under the standing
# sprites and over the ground.
func _lay_fields() -> void:
	_field_sheet = _sheet("decorations/fields")
	fields = _tile_layer(_field_sheet)
	_field_tags.clear()
	_tile_fields()


# Tiles every field whose state or season changed since it was last tiled: each cell of its
# footprint with a variant of its tag by a seeded hash.
func _tile_fields() -> void:
	var winter: bool = world.date().season == WINTER_SEASON
	for structure in world.structure_count():
		var state := world.structure_field_state(structure)
		if state < 0:
			continue
		var tag: String = FIELD_TAGS[state] + (WINTER if winter else "")
		if _field_tags.get(structure, "") == tag:
			continue
		_field_tags[structure] = tag
		var tagged: Array = _field_sheet.tags[tag]
		var corner := world.structure_cell(structure)
		var footprint := world.structure_footprint(structure)
		for y in footprint.y:
			for x in footprint.x:
				var cell := corner + Vector2i(x, y)
				var variant: int = tagged[0] + _hash(cell.x, cell.y, 7) % (tagged[1] - tagged[0] + 1)
				fields.set_cell(cell, 0, Vector2i(variant, 0))


# Lays the layer the villages' ground decorations are tiled on, over the ground, and each village's
# decorations.
func _lay_decorations() -> void:
	_layouts = JSON.parse_string(FileAccess.get_file_as_string(DECORATIONS))
	_ground_sheet = _sheet("decorations/ground")
	for banner: String in BANNERS:
		_banner_sheets[banner] = _sheet(BANNERS[banner])
	built = _tile_layer(_ground_sheet)
	_decorated = 0
	_decorate(world.villages())


# Lays the decorations from DECORATIONS by its template of each of `villages` laid since they
# were last laid, those the world's growth founds among them: ground tiles, a variant per cell by a
# seeded hash, and banners in the owner's colours, standing with everything else.
func _decorate(villages: Array[Dictionary]) -> void:
	for village: Dictionary in villages.slice(_decorated):
		var corner: Vector2i = village.corner
		for item: Dictionary in _layouts.get(village.kind, []):
			if item.has("tile"):
				var tagged: Array = _ground_sheet.tags[item.tile]
				var rect: Array = item.rect
				for y in range(int(rect[1]), int(rect[1]) + int(rect[3])):
					for x in range(int(rect[0]), int(rect[0]) + int(rect[2])):
						var cell := corner + Vector2i(x, y)
						var variant: int = tagged[0] + _hash(cell.x, cell.y, 6) % (tagged[1] - tagged[0] + 1)
						built.set_cell(cell, 0, Vector2i(variant, 0))
			else:
				# Any other item is a banner, keyed by its kind.
				var banner: String = item.keys()[0]
				var sheet: Dictionary = _banner_sheets[banner]
				var cell := corner + Vector2i(int(item[banner][0]), int(item[banner][1]))
				var sprite := Sprite2D.new()
				sprite.texture = _region(sheet, FACTIONS[_sprite_of_faction[int(village.faction)]], 0)
				sprite.centered = false
				sprite.position = Vector2(cell.x, cell.y + 1) * CELL
				sprite.offset = Vector2(0, -sheet.frame.y)
				_standing.add_child(sprite)
	_decorated = villages.size()
# Puts the bodies' frames in one texture, so every band draws from it: the three factions' body
# sheets side by side, then the shadow, centred in a slot of its own.
func _build_body_atlas() -> void:
	var first := _sheet("bodies/" + FACTIONS[0])
	for tag in BODY_TAGS:
		_body_frames[tag] = first.tags[tag][0]
		_body_frames[tag + "_xray"] = first.tags[tag + "_xray"][0]
	_faction_width = first.texture.get_width()
	_shadow_x = FACTIONS.size() * _faction_width
	var image := Image.create_empty(_shadow_x + BODY_FRAME.x, BODY_FRAME.y, false, Image.FORMAT_RGBA8)
	for faction in FACTIONS.size():
		var source: Image = _sheet("bodies/" + FACTIONS[faction]).texture.get_image()
		source.convert(Image.FORMAT_RGBA8)
		image.blit_rect(source, Rect2i(Vector2i.ZERO, source.get_size()), Vector2i(faction * _faction_width, 0))
	var shadow: Image = _sheet(SHADOW).texture.get_image()
	shadow.convert(Image.FORMAT_RGBA8)
	image.blit_rect(shadow, Rect2i(Vector2i.ZERO, shadow.get_size()),
			Vector2i(_shadow_x, (BODY_FRAME.y - shadow.get_height()) / 2))
	_atlas = ImageTexture.create_from_image(image)


# Adds one sprite to `buffer`: `frame` is its rectangle in the atlas, BODY_FRAME in size, `centre`
# its centre in the pixels of the node the buffer is drawn in, `flip` mirrors it, and a `tint`
# other than white rings it in that colour; it is drawn `white` of the way to white, `alpha`
# opaque, and turned `turn` radians about its bottom edge's middle. `outline_of`, for an outline, is
# where the body it outlines is in the atlas, and `white` is how far to white that body is drawn.
func _pack(buffer: PackedFloat32Array, frame: Rect2, centre: Vector2, flip: bool, tint: Color, white := 0.0,
		alpha := 1.0, turn := 0.0, outline_of := Vector2(-1, -1)) -> void:
	var size := _atlas.get_size()
	var width := frame.size.x * (-1.0 if flip else 1.0)
	var height := frame.size.y
	var cos_turn := cos(turn)
	var sin_turn := sin(turn)
	var local := centre.round() + Vector2(height / 2.0 * sin_turn, height / 2.0 * (1.0 - cos_turn))
	# A QuadMesh's top is +y; the negative height turns it to the canvas's downward y. Then the
	# transform's rows, the ring's colour, and the rectangle's position with the shares of white and
	# of the body shown; or, for an outline, the body's share of white in the colour's red, and
	# where the body it outlines is. Written in place, so no array is made per sprite.
	var body := outline_of.x < 0.0
	var custom := Vector2(white, alpha) if body else outline_of / size
	var colour := tint if body else Color(white, 0.0, 0.0, 1.0)
	var at := buffer.size()
	buffer.resize(at + BODY_FLOATS)
	buffer[at] = width * cos_turn
	buffer[at + 1] = height * sin_turn
	buffer[at + 3] = local.x
	buffer[at + 4] = width * sin_turn
	buffer[at + 5] = -height * cos_turn
	buffer[at + 7] = local.y
	buffer[at + 8] = colour.r
	buffer[at + 9] = colour.g
	buffer[at + 10] = colour.b
	buffer[at + 11] = colour.a
	buffer[at + 12] = frame.position.x / size.x
	buffer[at + 13] = frame.position.y / size.y
	buffer[at + 14] = custom.x
	buffer[at + 15] = custom.y


# Whether something standing in front of a body, sorted after `sort_y`, covers part of `rect`,
# the body's frame on screen.
func _hidden(rect: Rect2, sort_y: float) -> bool:
	for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
		for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
			var bucket: Array = _occluders.get(Vector2i(bx, by), NO_OCCLUDERS)
			for occluder: Ground.Occluder in bucket:
				if occluder.sort_y > sort_y and occluder.rect.intersects(rect):
					return true
	return false


# Buckets what can stand in front of a body: the ground's raised strips and trees shown now, and
# every standing structure's frame, again whenever either changes.
func _gather_occluders() -> void:
	var ground: Node2D = $Ground
	var now := Vector3i(ground.shows, structure_sprites.size(), _structure_frames)
	if now == _occluders_for:
		return
	_occluders_for = now
	_occluders.clear()
	var all: Array[Ground.Occluder] = ground.occluders()
	for sprite in structure_sprites:
		if sprite != null and sprite.get_parent() == _standing:
			all.append(Ground.Occluder.new(Rect2(sprite.position + sprite.offset, sprite.texture.get_size()), sprite.position.y))
	for occluder in all:
		var rect := occluder.rect
		for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
			for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
				var key := Vector2i(bx, by)
				if not _occluders.has(key):
					_occluders[key] = []
				_occluders[key].append(occluder)


func _fill_bands(buffers: Array[PackedFloat32Array], first_row: int) -> void:
	while _bands.size() < buffers.size():
		var band := MultiMeshInstance2D.new()
		band.multimesh = MultiMesh.new()
		band.multimesh.transform_format = MultiMesh.TRANSFORM_2D
		band.multimesh.use_colors = true
		band.multimesh.use_custom_data = true
		var quad := QuadMesh.new()
		quad.size = Vector2.ONE
		band.multimesh.mesh = quad
		# The renderer keeps a band's bounds from its MultiMesh's AABB when the band is first drawn,
		# and culls a band refilled elsewhere by them; bounds over every body the band's row can
		# hold, across the whole region, stay right wherever the view goes.
		var rise := relief.highest() * Relief.LEVEL_PX + BODY_FRAME.y + CELL.y
		band.multimesh.custom_aabb = AABB(Vector3(-BODY_FRAME.x, -rise, 0),
				Vector3(_width * CELL.x + 2 * BODY_FRAME.x, 2 * rise, 0))
		band.texture = _atlas
		band.material = _band_material()
		_standing.add_child(band)
		_bands.append(band)
	for band in _bands.size():
		_bands[band].position = Vector2(0, _band_y(first_row + band))
		_fill(_bands[band], buffers[band] if band < buffers.size() else PackedFloat32Array())


# Draws the sprites `data` holds in `instance`'s MultiMesh, growing it as it needs.
func _fill(instance: MultiMeshInstance2D, data: PackedFloat32Array) -> void:
	var multimesh := instance.multimesh
	var count := data.size() / BODY_FLOATS
	if multimesh.instance_count < count:
		multimesh.instance_count = count + 64
	if count > 0:
		data.resize(multimesh.instance_count * BODY_FLOATS)
		multimesh.buffer = data
	multimesh.visible_instance_count = count


# The MultiMesh the outlines are drawn in, over everything standing, set up the first time.
func _outlines() -> MultiMeshInstance2D:
	var outlines: MultiMeshInstance2D = $Outlines
	if outlines.multimesh == null:
		outlines.multimesh = MultiMesh.new()
		outlines.multimesh.transform_format = MultiMesh.TRANSFORM_2D
		outlines.multimesh.use_colors = true
		outlines.multimesh.use_custom_data = true
		var quad := QuadMesh.new()
		quad.size = Vector2.ONE
		outlines.multimesh.mesh = quad
		outlines.multimesh.custom_aabb = AABB(Vector3(-BODY_FRAME.x, -BODY_FRAME.y - relief.highest() * Relief.LEVEL_PX, 0),
				Vector3(_width * CELL.x + 2 * BODY_FRAME.x, _height * CELL.y + 2 * BODY_FRAME.y + relief.highest() * Relief.LEVEL_PX, 0))
		outlines.texture = _atlas
		var material := ShaderMaterial.new()
		material.shader = preload("res://world_art/outline.gdshader")
		material.set_shader_parameter("frame_size", Vector2(BODY_FRAME) / _atlas.get_size())
		outlines.material = material
	return outlines


# Where the band of row `row` is sorted: the row's middle on the ground, after the raised strip of
# its own row and before whatever stands on a footprint ending at the row's south edge.
func _band_y(row: int) -> float:
	return row * CELL.y + CELL.y / 2.0


func _band_material() -> ShaderMaterial:
	if _material == null:
		_material = ShaderMaterial.new()
		_material.shader = preload("res://world_art/body_band.gdshader")
		_material.set_shader_parameter("frame_size", Vector2(BODY_FRAME) / _atlas.get_size())
	return _material


# Reads an exported sheet: its texture, its frame size and its tags as {name: [from, to]}.
func _sheet(name: String) -> Dictionary:
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(ART + name + ".json"))
	var tags := {}
	for tag: Dictionary in data.meta.frameTags:
		tags[tag.name] = [int(tag.from), int(tag.to)]
	var first: Dictionary = data.frames[0].frame
	return {"texture": load(ART + name + ".png"), "frame": Vector2i(int(first.w), int(first.h)), "tags": tags}


# Frame `index` of tag `tag` in `sheet`, as a texture of its own.
func _region(sheet: Dictionary, tag: String, index: int) -> AtlasTexture:
	var region := AtlasTexture.new()
	region.atlas = sheet.texture
	region.region = Rect2(Vector2((sheet.tags[tag][0] + index) * sheet.frame.x, 0), sheet.frame)
	return region
