## Draws a WorldAdapter's world in the approved art (docs/art-direction.md): terrain tiles laid
## as docs/convention/stack-godot.md §Terrain layers sets out, territory borders, landforms,
## buildings by stage, bodies by faction and the steered person's markers. Bodies are batched:
## one MultiMesh per row of cells on screen, sorted with buildings and landforms by the bottom
## edge of what they stand on. It reads the world and holds no game rule.
extends Node2D

const CELL := 16
const ART := "res://art/"
# Sprite sets by faction name; a world without faction names takes them in this order.
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
# The faction accents of docs/art-direction.md §Palette, in the order of FACTIONS.
const ACCENTS: Array[Color] = [Color("2f63b8"), Color("d63a2f"), Color("e8a830")]
const STAGES := ["intact", "damaged", "destroyed", "rubble"]
# The art for each kind of structure; its frames hold the structure's footprint at
# (FOOTPRINT_INSET, FOOTPRINT_INSET).
const BUILDINGS := {
	"house": "structures/house", "hall": "structures/hall", "well": "structures/well",
	"barrels": "structures/barrels", "fence": "structures/fence",
}
# The decorations of each village template, which never block or break: ground tiles over
# rectangles of cells and banners on cells, as offsets from the village's corner.
const DECORATIONS := "res://world_art/villages.json"
const FOOTPRINT_INSET := 8
# How often a free cell is left without a landform: mountains mostly covered, hills open.
const LANDFORM_SKIP := {"mountain": 0.05, "hills": 0.3}
const WALK_FRAME_MS := 150
const CHEVRON_FRAME_MS := 400
# Pixels past the screen's edge that are still drawn, for sprites that reach across it.
const MARGIN := 64.0
# Frame positions in the body atlas built at startup, in pixels.
const BODY_FRAME := Vector2i(16, 24)

var world: WorldAdapter
var steered := -1
## Bodies drawn in the last refresh, after culling to the screen.
var drawn := 0

var _width := 0
var _height := 0
var _kinds := PackedStringArray()
var _sprite_of_faction := PackedInt64Array()
var _atlas: ImageTexture
var _bands: Array[MultiMeshInstance2D] = []
# Bodies change only on a tick, so they are read once per tick: positions now and a tick ago,
# and each army's sprite set. Every body is one person.
var _tick_positions: Array[PackedVector2Array] = []
var _last_positions: Array[PackedVector2Array] = []
var _army_sprites := PackedInt64Array()
## The layer of the villages' ground decorations: paving, road and fields.
var built: TileMapLayer
## One sprite per structure, in the world's order; null for a structure with no art.
var structure_sprites: Array[Sprite2D] = []
# Each building's frames by footprint, one per stage in the order of STAGES.
var _stage_frames := {}
var _chevron_frames: Array[AtlasTexture] = []
var _material: ShaderMaterial

var _ground: Node2D
var _territory: Territory
var _standing: Node2D
var _ring: Sprite2D
var _chevron: Sprite2D


## Lays out `shown`'s terrain, landforms and buildings, and remembers `steered_army`, the army
## whose lead body carries the player's markers, or -1 for none.
func show_world(shown: WorldAdapter, steered_army: int) -> void:
	# Read here rather than on ready: a test readies only the scene it instantiates.
	_ground = $Ground
	_territory = $Territory
	_standing = $Standing
	_ring = $Ring
	_chevron = $Chevron
	world = shown
	steered = steered_army
	var rows := world.map_rows()
	_height = rows.size()
	_width = rows[0].length() if _height > 0 else 0
	_kinds = _cell_kinds(rows)
	var names := world.faction_names()
	_sprite_of_faction.clear()
	for faction in maxi(names.size(), FACTIONS.size()):
		var name: String = names[faction].to_lower() if faction < names.size() else FACTIONS[faction % FACTIONS.size()]
		_sprite_of_faction.append(maxi(FACTIONS.find(name), 0))
	_lay_terrain()
	var colours: Array[Color] = []
	for faction in _sprite_of_faction:
		colours.append(ACCENTS[faction])
	_territory.show_borders(_borders(), colours)
	_lay_landforms()
	for kind: String in BUILDINGS:
		var sheet := _sheet(BUILDINGS[kind])
		var frames: Array[AtlasTexture] = []
		for stage: String in STAGES:
			frames.append(_region(sheet, stage, 0))
		_stage_frames[kind] = frames
	_lay_decorations()
	_lay_structures()
	_build_body_atlas()
	var player := _sheet("markers/player_marker")
	_ring.texture = _region(player, "ring", 0)
	_chevron_frames.append(_region(player, "chevron", 0))
	_chevron_frames.append(_region(player, "chevron", 1))
	_ring.visible = steered >= 0
	_chevron.visible = steered >= 0
	for army in world.army_count():
		_tick_positions.append(world.body_positions(army))
		_last_positions.append(_tick_positions[army])
		_army_sprites.append(_sprite_of_faction[world.army_faction(army) % _sprite_of_faction.size()])


## Records the bodies' positions as a tick leaves them, so a body that moved since the last
## tick is drawn walking, facing the way it went.
func note_tick() -> void:
	for army in world.army_count():
		_last_positions[army] = _tick_positions[army]
		_tick_positions[army] = world.body_positions(army)


## Updates buildings and bodies within `view`, the part of the world on screen, in pixels.
func refresh(view: Rect2) -> void:
	var now := Time.get_ticks_msec()
	for structure in structure_sprites.size():
		var sprite := structure_sprites[structure]
		if sprite != null:
			sprite.texture = sprite.get_meta("frames")[world.structure_stage(structure)]
	var shown := view.grow(MARGIN)
	var first_row := maxi(int(shown.position.y / CELL), 0)
	var last_row := mini(int(shown.end.y / CELL) + 2, _height + 2)
	var buffers: Array[PackedFloat32Array] = []
	buffers.resize(maxi(last_row - first_row + 1, 0))
	var walk := (now / WALK_FRAME_MS) % 4
	drawn = 0
	for army in _tick_positions.size():
		var positions := _tick_positions[army]
		var before := _last_positions[army]
		var sprite := _army_sprites[army]
		for body in positions.size():
			var at := positions[body] * CELL / WorldAdapter.CELL_SIZE
			if not shown.has_point(at):
				continue
			var row := clampi(int(at.y / CELL) + 1, first_row, last_row)
			if buffers.is_empty():
				continue
			var heading := 0.0
			if body < before.size():
				heading = positions[body].x - before[body].x
				if heading == 0.0 and positions[body].y == before[body].y:
					heading = NAN
			var frame := 0 if is_nan(heading) else 1 + walk
			_append(buffers, row - first_row, row, Rect2(Vector2(sprite * 80 + frame * BODY_FRAME.x, 0),
					BODY_FRAME), Vector2(at.x, at.y + CELL / 2.0 - BODY_FRAME.y / 2.0), heading < 0.0)
			drawn += 1
	_fill_bands(buffers, first_row)
	if steered >= 0:
		var lead := _tick_positions[steered][0] * CELL / WorldAdapter.CELL_SIZE
		_ring.position = lead.floor()
		_chevron.position = lead.floor()
		_chevron.texture = _chevron_frames[(now / CHEVRON_FRAME_MS) % 2]


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


func _kind(x: int, y: int) -> String:
	return _kinds[clampi(y, 0, _height - 1) * _width + clampi(x, 0, _width - 1)]


# A hash of a cell, for the seeded choices of a variant or a landform.
func _hash(x: int, y: int, salt: int) -> int:
	var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
	h = (h ^ (h >> 13)) * 1274126177
	return absi(h ^ (h >> 16))


func _lay_terrain() -> void:
	var terrain := _sheet("terrain/terrain")
	var transitions := _sheet("terrain/transitions")
	var tiles := TileSet.new()
	tiles.tile_size = Vector2i(CELL, CELL)
	for sheet: Dictionary in [terrain, transitions]:
		var source := TileSetAtlasSource.new()
		source.texture = sheet.texture
		source.texture_region_size = Vector2i(CELL, CELL)
		for frame in sheet.texture.get_width() / CELL:
			source.create_tile(Vector2i(frame, 0))
		tiles.add_source(source)
	var plain := TileMapLayer.new()
	plain.tile_set = tiles
	_ground.add_child(plain)
	for y in _height:
		for x in _width:
			plain.set_cell(Vector2i(x, y), 0, Vector2i(terrain.tags.plain[0] + _hash(x, y, 1) % 4, 0))
	for kind: String in transitions.tags:
		var layer := TileMapLayer.new()
		layer.tile_set = tiles
		layer.position = Vector2(-CELL / 2.0, -CELL / 2.0)
		_ground.add_child(layer)
		for y in _height + 1:
			for x in _width + 1:
				var mask := (1 if _kind(x - 1, y - 1) == kind else 0) + (2 if _kind(x, y - 1) == kind else 0) \
						+ (4 if _kind(x - 1, y) == kind else 0) + (8 if _kind(x, y) == kind else 0)
				if mask == 15:
					layer.set_cell(Vector2i(x, y), 0, Vector2i(terrain.tags[kind][0] + _hash(x, y, 2) % 4, 0))
				elif mask > 0:
					layer.set_cell(Vector2i(x, y), 1, Vector2i(transitions.tags[kind][0] + mask, 0))


# Each edge between two cells owned by different factions, as [from, to, faction of the cell
# on its left or top, faction on its right or bottom].
func _borders() -> Array:
	var owners := world.territory()
	var borders := []
	if owners.size() != _width * _height:
		return borders
	for y in _height:
		for x in _width:
			var here := owners[y * _width + x]
			if x + 1 < _width and owners[y * _width + x + 1] != here:
				borders.append([Vector2((x + 1) * CELL, y * CELL), Vector2((x + 1) * CELL, (y + 1) * CELL),
						here, owners[y * _width + x + 1]])
			if y + 1 < _height and owners[(y + 1) * _width + x] != here:
				borders.append([Vector2(x * CELL, (y + 1) * CELL), Vector2((x + 1) * CELL, (y + 1) * CELL),
						here, owners[(y + 1) * _width + x]])
	return borders


# Places landforms greedily: cells in a seeded order, each taking the first footprint, from a
# seeded start among the kind's, whose cells are all of the kind and free.
func _lay_landforms() -> void:
	var sheet := _sheet("terrain/landforms")
	var sizes := {}
	for tag: String in sheet.tags:
		var parts := tag.split("_")
		var size := parts[1].split("x")
		if not sizes.has(parts[0]):
			sizes[parts[0]] = []
		sizes[parts[0]].append([Vector2i(int(size[0]), int(size[1])), tag])
	var order := range(_width * _height)
	order.sort_custom(func(a: int, b: int) -> bool: return _hash(a, 0, 3) < _hash(b, 0, 3))
	var taken := PackedByteArray()
	taken.resize(_width * _height)
	for cell: int in order:
		var x := cell % _width
		var y := cell / _width
		var kind := _kinds[cell]
		if not sizes.has(kind) or taken[cell] == 1 or (_hash(x, y, 4) % 1000) < LANDFORM_SKIP[kind] * 1000:
			continue
		var options: Array = sizes[kind]
		var start := _hash(x, y, 5) % options.size()
		for t in options.size():
			var option: Array = options[(start + t) % options.size()]
			var size: Vector2i = option[0]
			if _fits(x, y, size, kind, taken):
				for r in size.y:
					for q in size.x:
						taken[(y + r) * _width + x + q] = 1
				var sprite := Sprite2D.new()
				sprite.texture = _region(sheet, option[1], 0)
				sprite.centered = false
				sprite.position = Vector2(x * CELL, (y + size.y) * CELL)
				sprite.offset = Vector2(0, -sheet.frame.y)
				_standing.add_child(sprite)
				break


func _fits(x: int, y: int, size: Vector2i, kind: String, taken: PackedByteArray) -> bool:
	if x + size.x > _width or y + size.y > _height:
		return false
	for r in size.y:
		for q in size.x:
			var cell := (y + r) * _width + x + q
			if _kinds[cell] != kind or taken[cell] == 1:
				return false
	return true


# One sprite per structure whose kind has art, the bottom edge of its footprint as its
# position. A structure with no kind, as from WorldAdapter.create(), is not drawn.
func _lay_structures() -> void:
	structure_sprites.resize(world.structure_count())
	for structure in world.structure_count():
		var corner := world.structure_cell(structure)
		var footprint := world.structure_footprint(structure)
		var kind := world.structure_kind(structure)
		if not _stage_frames.has(kind):
			continue
		var frames: Array[AtlasTexture] = _stage_frames[kind]
		var sprite := Sprite2D.new()
		sprite.centered = false
		sprite.position = Vector2(corner.x * CELL, (corner.y + footprint.y) * CELL)
		sprite.offset = -Vector2(FOOTPRINT_INSET, FOOTPRINT_INSET + footprint.y * CELL)
		sprite.set_meta("frames", frames)
		_standing.add_child(sprite)
		structure_sprites[structure] = sprite


# Lays each village's decorations from DECORATIONS by its template: ground tiles on a layer
# above the terrain, a variant per cell by a seeded hash, and banners in the owner's colours,
# standing with everything else.
func _lay_decorations() -> void:
	var layouts: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(DECORATIONS))
	var ground := _sheet("decorations/ground")
	var banners := _sheet("decorations/banner")
	var tiles := TileSet.new()
	tiles.tile_size = Vector2i(CELL, CELL)
	var source := TileSetAtlasSource.new()
	source.texture = ground.texture
	source.texture_region_size = Vector2i(CELL, CELL)
	for frame in ground.texture.get_width() / CELL:
		source.create_tile(Vector2i(frame, 0))
	tiles.add_source(source)
	built = TileMapLayer.new()
	built.tile_set = tiles
	_ground.add_child(built)
	for village: Dictionary in world.villages():
		var corner: Vector2i = village.corner
		for item: Dictionary in layouts.get(village.kind, []):
			if item.has("tile"):
				var tagged: Array = ground.tags[item.tile]
				var rect: Array = item.rect
				for y in range(int(rect[1]), int(rect[1]) + int(rect[3])):
					for x in range(int(rect[0]), int(rect[0]) + int(rect[2])):
						var cell := corner + Vector2i(x, y)
						var variant: int = tagged[0] + _hash(cell.x, cell.y, 6) % (tagged[1] - tagged[0] + 1)
						built.set_cell(cell, 0, Vector2i(variant, 0))
			elif item.has("banner"):
				var cell := corner + Vector2i(int(item.banner[0]), int(item.banner[1]))
				var sprite := Sprite2D.new()
				sprite.texture = _region(banners, FACTIONS[_sprite_of_faction[int(village.faction)]], 0)
				sprite.centered = false
				sprite.position = Vector2(cell.x * CELL, (cell.y + 1) * CELL)
				sprite.offset = Vector2(0, -banners.frame.y)
				_standing.add_child(sprite)


# Puts the bodies' frames in one texture, so every band draws from it: the three factions' body
# sheets side by side.
func _build_body_atlas() -> void:
	var image := Image.create_empty(256, BODY_FRAME.y, false, Image.FORMAT_RGBA8)
	for faction in FACTIONS.size():
		var sheet := _sheet("bodies/" + FACTIONS[faction])
		var source: Image = sheet.texture.get_image()
		source.convert(Image.FORMAT_RGBA8)
		image.blit_rect(source, Rect2i(Vector2i.ZERO, source.get_size()), Vector2i(faction * 80, 0))
	_atlas = ImageTexture.create_from_image(image)


# Adds one sprite to band `band`, the row of cells whose bottom edge is at `row`: `frame` is its
# rectangle in the atlas, `centre` its centre in world pixels, and `flip` mirrors it.
func _append(buffers: Array[PackedFloat32Array], band: int, row: int, frame: Rect2, centre: Vector2,
		flip: bool) -> void:
	var size := _atlas.get_size()
	var local := centre.round() - Vector2(0, row * CELL)
	# A QuadMesh's top is +y; the negative height turns it to the canvas's downward y.
	buffers[band].append_array(PackedFloat32Array([
		frame.size.x * (-1.0 if flip else 1.0), 0.0, 0.0, local.x,
		0.0, -frame.size.y, 0.0, local.y,
		frame.position.x / size.x, frame.position.y / size.y, frame.size.x / size.x, frame.size.y / size.y]))


func _fill_bands(buffers: Array[PackedFloat32Array], first_row: int) -> void:
	while _bands.size() < buffers.size():
		var band := MultiMeshInstance2D.new()
		band.multimesh = MultiMesh.new()
		band.multimesh.transform_format = MultiMesh.TRANSFORM_2D
		band.multimesh.use_custom_data = true
		var quad := QuadMesh.new()
		quad.size = Vector2.ONE
		band.multimesh.mesh = quad
		band.texture = _atlas
		band.material = _band_material()
		_standing.add_child(band)
		_bands.append(band)
	for band in _bands.size():
		var multimesh := _bands[band].multimesh
		var data: PackedFloat32Array = buffers[band] if band < buffers.size() else PackedFloat32Array()
		var count := data.size() / 12
		_bands[band].position = Vector2(0, (first_row + band) * CELL)
		if multimesh.instance_count < count:
			multimesh.instance_count = count + 64
		if count > 0:
			data.resize(multimesh.instance_count * 12)
			multimesh.buffer = data
		multimesh.visible_instance_count = count


func _band_material() -> ShaderMaterial:
	if _material == null:
		_material = ShaderMaterial.new()
		_material.shader = preload("res://world_art/body_band.gdshader")
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
