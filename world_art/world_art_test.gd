# Checks that world_art.tscn lays a world as docs/convention/stack-godot.md §Terrain layers sets
# out: the plain on every cell, and a kind's dual-grid tile carrying the corner mask of the four
# cells around it; that it draws every body on screen and none off it; that a structure is drawn
# over its footprint with its stage's frame of its kind's art; that each field is tiled with its
# state's tag; that each village's ground decorations cover the cells its template's data
# names, a village the world founds as it runs included; that no landform stands on a buildable
# cell or over a structure; and that after the view jumps across the region, every body a band
# holds lies inside the bounds the renderer culls that band by.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const SEED := 2026
const TICKS := 2000
# A wall of blocked cells, drawn as mountain, down column 8 from row 0 to row 7.
const MAP_ROWS: Array[String] = [
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"................",
]
const BLAST := {"range": 768, "radius": 512, "body_damage": 2, "structure_damage": 9, "cooldown": 64}
# Twelve people from (0, 0) and ten from (12, 6), in rows of four, each bound for the other's corner.
const ARMIES: Array[Dictionary] = [
	{"objective": Vector2i(14, 7), "faction": 0, "skills": [BLAST], "members": [
		{"cell": Vector2i(0, 0), "health": 100}, {"cell": Vector2i(1, 0), "health": 100}, {"cell": Vector2i(2, 0), "health": 100}, {"cell": Vector2i(3, 0), "health": 100}, {"cell": Vector2i(0, 1), "health": 100}, {"cell": Vector2i(1, 1), "health": 100}, {"cell": Vector2i(2, 1), "health": 100}, {"cell": Vector2i(3, 1), "health": 100}, {"cell": Vector2i(0, 2), "health": 100}, {"cell": Vector2i(1, 2), "health": 100}, {"cell": Vector2i(2, 2), "health": 100}, {"cell": Vector2i(3, 2), "health": 100}]},
	{"objective": Vector2i(1, 1), "faction": 1, "skills": [BLAST], "members": [
		{"cell": Vector2i(12, 6), "health": 101}, {"cell": Vector2i(13, 6), "health": 101}, {"cell": Vector2i(14, 6), "health": 101}, {"cell": Vector2i(15, 6), "health": 101}, {"cell": Vector2i(12, 7), "health": 101}, {"cell": Vector2i(13, 7), "health": 101}, {"cell": Vector2i(14, 7), "health": 101}, {"cell": Vector2i(15, 7), "health": 101}, {"cell": Vector2i(12, 8), "health": 101}, {"cell": Vector2i(13, 8), "health": 101}]},
]
const STRUCTURES: Array[Dictionary] = [
	{"cell": Vector2i(7, 7), "durability": 100},
	{"cell": Vector2i(9, 7), "durability": 60},
]
const RULES := {"separation": 256, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}
const VIEW := Vector2(480, 270)
# Days of five ticks, as the core's region test lives them: marching and raising run far faster
# against the calendar than in play, so the region SEED makes founds its first village within
# FOUNDING_DAYS in a few tens of thousands of ticks: from one household a village, its first after
# 2,490 days.
const SHORT_DAY := 5
const FOUNDING_DAYS := 3000


func run() -> bool:
	var world := WorldAdapter.create(42, PackedStringArray(MAP_ROWS), ARMIES, STRUCTURES, RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, -1)
	var ground: Array[Node] = art.get_node("Ground").get_children()
	var plain: TileMapLayer = ground[0]
	var cells := MAP_ROWS.size() * MAP_ROWS[0].length()
	var plain_full := plain.get_used_cells().size() == cells
	var mountain := _layer_with_tiles(ground)
	var transitions := _tags("res://art/terrain/transitions.json")
	var from: int = transitions.mountain[0]
	# Dual-grid tile (8, 1) has the wall on its right corners (2 + 8); (9, 4) on its left (1 + 4);
	# (2, 2) touches no wall and is empty.
	var masks_right := mountain != null \
			and mountain.get_cell_atlas_coords(Vector2i(8, 1)) == Vector2i(from + 10, 0) \
			and mountain.get_cell_atlas_coords(Vector2i(9, 4)) == Vector2i(from + 5, 0) \
			and mountain.get_cell_source_id(Vector2i(2, 2)) == -1
	print("world_art_test: plain on %d of %d cells; mountain tiles at (8, 1) %s, (9, 4) %s, (2, 2) source %d: %s" % [
		plain.get_used_cells().size(), cells,
		mountain.get_cell_atlas_coords(Vector2i(8, 1)) if mountain else "none",
		mountain.get_cell_atlas_coords(Vector2i(9, 4)) if mountain else "none",
		mountain.get_cell_source_id(Vector2i(2, 2)) if mountain else -2,
		"pass" if plain_full and masks_right else "FAIL"])

	for i in TICKS:
		world.tick()
		art.note_tick()
	var bodies := 0
	for army in world.army_count():
		bodies += world.body_positions(army).size()
	art.refresh(Rect2(Vector2.ZERO, Vector2(MAP_ROWS[0].length(), MAP_ROWS.size()) * 16))
	var on_screen: int = art.drawn
	# Control: a view beside the map, over the same rows, draws nothing, so the count above is the
	# culling's.
	art.refresh(Rect2(Vector2(10000, 0), Vector2(480, 270)))
	var off_screen: int = art.drawn
	var bodies_right := bodies > world.army_count() and on_screen == bodies and off_screen == 0

	var bodies_line := "world_art_test: after %d ticks %d bodies, %d drawn on screen, %d off it" % [
		TICKS, bodies, on_screen, off_screen]
	print(bodies_line + ": " + ("pass" if bodies_right else "FAIL"))

	# Buildings: each region structure of a kind world_art draws is drawn as its kind's art, its
	# footprint's bottom-left on the sprite's position, showing its stage's frame; a sect seat's
	# training ground, manual hall and gate, which it does not draw yet, get no sprite. Control:
	# the structures above, made by create() with no kind, get no sprite.
	var region := WorldAdapter.create_region(SEED)
	var region_art: Node2D = load(SCENE).instantiate()
	region_art.show_world(region, region.army_count() - 1)
	region_art.refresh(Rect2(Vector2.ZERO, Vector2(480, 270)))
	var stages := ["intact", "damaged", "destroyed", "rubble"]
	var placed := 0
	var fields := 0
	var undrawn := 0
	var kinds := {}
	for structure in region.structure_count():
		var sprite: Sprite2D = region_art.structure_sprites[structure]
		var footprint := region.structure_footprint(structure)
		var corner := region.structure_cell(structure)
		var kind := region.structure_kind(structure)
		if kind == "field":
			fields += 1
			continue
		if not region_art.BUILDINGS.has(kind):
			undrawn += 1 if sprite == null else 0
			continue
		var sheet := _sheet("res://art/structures/%s.json" % kind)
		if sprite == null or sheet.is_empty():
			continue
		var frame := (sprite.texture as AtlasTexture).region
		var stage: String = stages[region.structure_stage(structure)]
		if sprite.position == Vector2(corner.x, corner.y + footprint.y) * 16 \
				and frame.position.x == sheet.tags[stage][0] * sheet.width \
				and frame.size.x == sheet.width and sheet.width == footprint.x * 16 + 16:
			placed += 1
			kinds[kind] = true
	var unkinded_skipped: bool = art.structure_sprites.all(func(sprite: Sprite2D) -> bool: return sprite == null)
	var buildings_right := placed + fields + undrawn == region.structure_count() and kinds.size() == 7 \
			and undrawn > 0 and unkinded_skipped
	print("world_art_test: %d of %d region structures drawn as their kind %s over their footprints with their stage's frame, %d fields tiled, %d of a seat's kinds left undrawn; structures without a kind left undrawn %s: %s" % [
		placed, region.structure_count(), kinds.keys(), fields, undrawn, unkinded_skipped, "pass" if buildings_right else "FAIL"])

	# Fields: every cell of each field carries a frame of its state's tag, sown in spring as the
	# world begins. Control: the same cells carry no frame of the burnt tag, so the check tells
	# the states apart.
	var field_tiles := _sheet("res://art/decorations/fields.json")
	var tiled_right := fields > 0
	var burnt_apart := true
	for structure in region.structure_count():
		var state := region.structure_field_state(structure)
		if state < 0:
			continue
		tiled_right = tiled_right and state == WorldAdapter.FIELD_SOWN \
				and _field_tagged(region_art.fields, region, structure, field_tiles.tags["sown"])
		burnt_apart = burnt_apart \
				and not _field_tagged(region_art.fields, region, structure, field_tiles.tags["burnt"])
	var fields_right := tiled_right and burnt_apart
	print("world_art_test: %d fields tiled with the sown frames %s, none with the burnt %s: %s" % [
		fields, tiled_right, burnt_apart, "pass" if fields_right else "FAIL"])

	# Decorations: each village's ground tiles cover the cells its template's data names, with a
	# frame of the tag named; control: the cell west of the village has none.
	var tiled := 0
	var expected := 0
	var outside_bare := true
	for village: Dictionary in region.villages():
		var counts := _decorated(region_art.built, village)
		tiled += counts.x
		expected += counts.y
		outside_bare = outside_bare \
				and region_art.built.get_cell_source_id(village.corner + Vector2i(-1, 4)) == -1
	var decorations_right := expected > 0 and tiled == expected and outside_bare
	print("world_art_test: %d of %d decorated cells carry their template's tile; cells beside the villages bare %s: %s" % [
		tiled, expected, outside_bare, "pass" if decorations_right else "FAIL"])

	# Landforms: each stands only on cells the map marks rough or blocked, which nothing is built
	# on, so none covers a structure. Control: every structure stands on buildable cells, marked
	# ".", so the check tells the two apart.
	var landform_path := "res://art/terrain/landforms.json"
	var landform_tags := _tags(landform_path)
	var landform_width: int = _sheet(landform_path).width
	var landform_texture: Texture2D = load("res://art/terrain/landforms.png")
	var rows := region.map_rows()
	var built_on := {}
	var structures_buildable := true
	for structure in region.structure_count():
		for cell in _cells(region.structure_cell(structure), region.structure_footprint(structure)):
			built_on[cell] = true
			structures_buildable = structures_buildable and rows[cell.y][cell.x] == "."
	var landforms := 0
	var landforms_unbuildable := true
	var landforms_clear := true
	for sprite in region_art.get_node("Standing").get_children():
		if not (sprite is Sprite2D and sprite.texture is AtlasTexture \
				and sprite.texture.atlas == landform_texture):
			continue
		landforms += 1
		var frame := int(sprite.texture.region.position.x) / landform_width
		var size := Vector2i.ZERO
		for tag: String in landform_tags:
			if frame >= landform_tags[tag][0] and frame <= landform_tags[tag][1]:
				var parts := tag.split("_")[1].split("x")
				size = Vector2i(int(parts[0]), int(parts[1]))
		var corner := Vector2i(int(sprite.position.x) / 16, int(sprite.position.y) / 16 - size.y)
		for cell in _cells(corner, size):
			landforms_unbuildable = landforms_unbuildable and rows[cell.y][cell.x] != "."
			landforms_clear = landforms_clear and not built_on.has(cell)
	var landforms_right := landforms > 0 and landforms_unbuildable and landforms_clear \
			and structures_buildable
	print("world_art_test: %d landforms, all on unbuildable cells %s, none over a structure %s; every structure on buildable cells %s: %s" % [
		landforms, landforms_unbuildable, landforms_clear, structures_buildable,
		"pass" if landforms_right else "FAIL"])
	region_art.free()
	var passed := plain_full and masks_right and bodies_right and buildings_right \
			and decorations_right and fields_right and landforms_right
	art.free()
	return _bands_pass() and _founded_pass() and passed


# A village the region founds is drawn as its template is: its structures stand at the offsets a
# generated village of that template has from its corner, and its ground decorations cover the
# cells the template's data names. Setup: the region, on short days, founds a village within
# FOUNDING_DAYS. Control:
# a corner a cell off matches no structure layout, and the cell west of the village is bare.
func _founded_pass() -> bool:
	var region := WorldAdapter.create_region(SEED, 0, SHORT_DAY)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(region, region.army_count() - 1)
	var generated := region.villages()
	var created := region.structure_count()
	var ticks := 0
	while region.villages().size() == generated.size() and ticks < FOUNDING_DAYS * SHORT_DAY:
		region.tick()
		ticks += 1
	art.note_tick()
	var villages := region.villages()
	var founded: Dictionary = villages[-1] if villages.size() > generated.size() else {}
	var model := {}
	for village: Dictionary in generated:
		if not founded.is_empty() and village.kind == founded.kind:
			model = village
			break
	var set_up := not founded.is_empty() and not model.is_empty()
	var layout := _layout(region, model, 0, created) if set_up else []
	var laid := _layout(region, founded, created, region.structure_count()) if set_up else []
	var off := founded.duplicate()
	off.corner = founded.get("corner", Vector2i.ZERO) + Vector2i(1, 0)
	var laid_off := _layout(region, off, created, region.structure_count()) if set_up else []
	var at_offsets := set_up and layout.all(func(placed: String) -> bool: return laid.has(placed))
	var off_apart := not layout.all(func(placed: String) -> bool: return laid_off.has(placed))
	var structures_right := at_offsets and off_apart
	var counts := _decorated(art.built, founded) if set_up else Vector2i.ZERO
	var outside_bare: bool = set_up \
			and art.built.get_cell_source_id(founded.corner + Vector2i(-1, 4)) == -1
	var passed := structures_right and counts.y > 0 and counts.x == counts.y and outside_bare
	print("world_art_test: after %d days a village founded %s at %s, laid out as %s at %s: its %d structures at the template's offsets %s, a cell off not %s; %d of %d decorated cells carry their template's tile, the cell beside it bare %s: %s" % [
		ticks / SHORT_DAY, founded.get("kind", "none"), founded.get("corner", "none"),
		model.get("name", "none"), model.get("corner", "none"), layout.size(), at_offsets,
		off_apart, counts.x, counts.y, outside_bare, "pass" if passed else "FAIL"])
	art.free()
	return passed


# Each structure among `from` to `to` of `world` inside `village`'s area, as its kind and its
# cell's offset from the village's corner.
func _layout(world: WorldAdapter, village: Dictionary, from: int, to: int) -> Array[String]:
	var placed: Array[String] = []
	var area := Rect2i(village.corner, village.area)
	for structure in range(from, to):
		var cell := world.structure_cell(structure)
		if area.has_point(cell):
			placed.append("%s at %s" % [world.structure_kind(structure), cell - area.position])
	return placed


# How many of the cells `village`'s template's data names carry a frame of their tile on `built`,
# and how many it names.
func _decorated(built: TileMapLayer, village: Dictionary) -> Vector2i:
	var layouts: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/villages.json"))
	var ground_tiles := _sheet("res://art/decorations/ground.json")
	var corner: Vector2i = village.corner
	var counts := Vector2i.ZERO
	for item: Dictionary in layouts.get(village.kind, []):
		if not item.has("tile"):
			continue
		var rect: Array = item.rect
		for y in range(int(rect[1]), int(rect[1]) + int(rect[3])):
			for x in range(int(rect[0]), int(rect[0]) + int(rect[2])):
				var at: int = built.get_cell_atlas_coords(corner + Vector2i(x, y)).x
				# Paving laid after the road over the same cell is the one that shows.
				var last := _last_tile(layouts[village.kind], Vector2i(x, y))
				counts.y += 1
				if at >= ground_tiles.tags[last][0] and at <= ground_tiles.tags[last][1]:
					counts.x += 1
	return counts


# A band's bodies lie inside the bounds the renderer culls it by after the view jumps from the
# player's village to the village farthest from it, so the renderer draws them. Godot 4.7.2 takes a
# MultiMeshInstance2D's bounds from its MultiMesh's AABB when the band is first drawn, and keeps
# them when the buffer is refilled: the bounds are the band's custom_aabb where it has one, and
# otherwise what the band held when first filled. Control: refreshed again over the same view, the
# bands' bodies lie inside the same bounds. Setup: both views draw bodies, a screen's width apart.
func _bands_pass() -> bool:
	var region := WorldAdapter.create_region(SEED)
	var art: Node2D = load(SCENE).instantiate()
	var steered := region.army_count() - 1
	art.show_world(region, steered)
	var home := region.body_positions(steered)[0] * 16 / WorldAdapter.CELL_SIZE
	var far := home
	for village: Dictionary in region.villages():
		var corner := Vector2(village.corner) * 16
		if corner.distance_to(home) > far.distance_to(home):
			far = corner
	art.refresh(Rect2(home - VIEW / 2, VIEW))
	var home_drawn: int = art.drawn
	var first := {}
	for band in _bands(art):
		first[band] = _extent(band)
	art.refresh(Rect2(home - VIEW / 2, VIEW))
	var unmoved := _outside(art, first)
	art.refresh(Rect2(far - VIEW / 2, VIEW))
	var far_drawn: int = art.drawn
	var jumped := _outside(art, first)
	art.free()
	var set_up := home_drawn > 0 and far_drawn > 0 and absf(far.x - home.x) >= VIEW.x
	var passed := set_up and unmoved.is_empty() and jumped.is_empty()
	print("world_art_test: %d bodies drawn at the player's village %s, %d at the village at %s; bodies outside their band's bounds: %d refreshed in place, %d after the jump %s: %s" % [
		home_drawn, home, far_drawn, far, unmoved.size(), jumped.size(), jumped.slice(0, 3), "pass" if passed else "FAIL"])
	return passed


func _bands(art: Node2D) -> Array[MultiMeshInstance2D]:
	var bands: Array[MultiMeshInstance2D] = []
	for child in art.get_node("Standing").get_children():
		if child is MultiMeshInstance2D:
			bands.append(child)
	return bands


# The rectangle the bodies band `band` holds now cover, in its own coordinates; an empty Rect2 for
# none. Each instance is a unit quad scaled by its transform's diagonal and centred on its origin.
func _extent(band: MultiMeshInstance2D) -> Rect2:
	var buffer := band.multimesh.buffer
	var extent := Rect2()
	for i in band.multimesh.visible_instance_count:
		var size := Vector2(absf(buffer[i * 12]), absf(buffer[i * 12 + 5]))
		var quad := Rect2(Vector2(buffer[i * 12 + 3], buffer[i * 12 + 7]) - size / 2, size)
		extent = quad if i == 0 else extent.merge(quad)
	return extent


# Each band whose bodies reach past the bounds the renderer culls it by: its custom_aabb, or
# else its extent in `first`, when it was first filled; a band first filled now is drawn now.
func _outside(art: Node2D, first: Dictionary) -> Array[String]:
	var outside: Array[String] = []
	for band in _bands(art):
		var extent := _extent(band)
		if band.multimesh.visible_instance_count == 0:
			continue
		var aabb := band.multimesh.custom_aabb
		var bounds: Rect2 = Rect2(aabb.position.x, aabb.position.y, aabb.size.x, aabb.size.y) \
				if aabb.size.x > 0.0 and aabb.size.y > 0.0 else first.get(band, extent)
		if not bounds.encloses(extent):
			outside.append("%s outside %s" % [extent, bounds])
	return outside


# The first kind layer with any tile: here, the only kind beside the plain is mountain.
func _layer_with_tiles(ground: Array[Node]) -> TileMapLayer:
	for layer: TileMapLayer in ground.slice(1):
		if not layer.get_used_cells().is_empty():
			return layer
	return null


# Whether every cell of field `structure` of `world` carries a frame within `tag` on `layer`.
func _field_tagged(layer: TileMapLayer, world: WorldAdapter, structure: int, tag: Array) -> bool:
	var corner := world.structure_cell(structure)
	var footprint := world.structure_footprint(structure)
	for y in footprint.y:
		for x in footprint.x:
			var at := layer.get_cell_atlas_coords(corner + Vector2i(x, y)).x
			if at < tag[0] or at > tag[1]:
				return false
	return true


# The cells of a `size` footprint from `corner`, row by row.
func _cells(corner: Vector2i, size: Vector2i) -> Array[Vector2i]:
	var cells: Array[Vector2i] = []
	for y in size.y:
		for x in size.x:
			cells.append(corner + Vector2i(x, y))
	return cells


# The last tile a layout lays on `cell`, which is the one left showing.
func _last_tile(layout: Array, cell: Vector2i) -> String:
	var last := ""
	for item: Dictionary in layout:
		if item.has("tile") and Rect2i(int(item.rect[0]), int(item.rect[1]), int(item.rect[2]),
				int(item.rect[3])).has_point(cell):
			last = item.tile
	return last


# An exported sheet's tags and frame width, or empty when there is no such sheet.
func _sheet(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		return {}
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	return {"tags": _tags(path), "width": int(data.frames[0].frame.w)}


func _tags(path: String) -> Dictionary:
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	var tags := {}
	for tag: Dictionary in data.meta.frameTags:
		tags[tag.name] = [int(tag.from), int(tag.to)]
	return tags
