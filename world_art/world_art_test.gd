# Checks that world_art.tscn lays a world at the 2.5D angle: the relief it draws the ground with
# raises the cells the map blocks as rock and leaves the rest plain at level 0; it draws every body
# on screen and none off it; every structure of a region is drawn over its footprint with its
# stage's frame of its kind's art, a seat's training ground on the ground, every kind among them
# once the region has grown its workshops; each field is tiled with
# its state's tag; each village's ground decorations cover the cells its template's data names, a
# village the world founds as it runs included, and its banners stand on the cells it names; after the view jumps across the region, every body a
# band holds lies inside the bounds the renderer culls that band by; and between ticks a body is
# drawn between the positions of the last two; and it lists the windows lit at night of every
# structure on screen by its kind and stage, and the steered person's torch; and each of a region's
# arts rises in its discipline's colour (#184).
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const WorldArt := preload("res://world_art/world_art.gd")
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
# 3,660 days.
const SHORT_DAY := 5
const FOUNDING_DAYS := 4500
# Days of short days within which the region's growth builds a smithy and an apothecary: the
# apothecary came at day 10,980 once settlers walked into their villages by their army's field
# (#387). A stopgap tied to this seed's growth until #396 sets every kind up directly.
const WORKSHOP_DAYS := 12000
# How far in from its left and bottom edges every structure's frame holds its footprint's
# bottom-left corner, as docs/art-direction.md §Structures and damage stages gives it.
const INSET := 8


func run() -> bool:
	var world := WorldAdapter.create(42, PackedStringArray(MAP_ROWS), ARMIES, STRUCTURES, RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, -1)
	# The wall down column 8 is rock above the plain, dropping by a rock face; every other cell is
	# plain at level 0. Control: a cell beside the wall is not raised, so the check tells them apart.
	var relief: Relief = art.relief
	var wall := 0
	var flat := 0
	for y in MAP_ROWS.size():
		for x in MAP_ROWS[y].length():
			var at := Vector2i(x, y)
			if MAP_ROWS[y][x] == "#":
				if relief.kind_at(at) == "rock" and relief.level_at(at) >= 2 and relief.face_at(at) == "rock_face" \
						and not relief.slope_at(at):
					wall += 1
			elif relief.kind_at(at) == "plain" and relief.level_at(at) == 0 and not relief.slope_at(at):
				flat += 1
	var walls: int = MAP_ROWS.reduce(func(count: int, row: String) -> int: return count + row.count("#"), 0)
	var cells := MAP_ROWS.size() * MAP_ROWS[0].length()
	var relief_right: bool = wall == walls and flat == cells - walls and relief.level_at(Vector2i(7, 0)) == 0
	print("world_art_test: %d of %d blocked cells raised as rock, %d of %d others plain at level 0: %s" % [
		wall, walls, flat, cells - walls, "pass" if relief_right else "FAIL"])

	for i in TICKS:
		world.tick()
		art.note_tick()
	var bodies := 0
	for army in world.army_count():
		bodies += world.body_positions(army).size()
	art.refresh(Rect2(Vector2.ZERO, Vector2(MAP_ROWS[0].length(), MAP_ROWS.size()) * WorldArt.CELL))
	var on_screen: int = art.drawn
	# Control: a view beside the map, over the same rows, draws nothing, so the count above is the
	# culling's.
	art.refresh(Rect2(Vector2(10000, 0), Vector2(480, 270)))
	var off_screen: int = art.drawn
	var bodies_right := bodies > world.army_count() and on_screen == bodies and off_screen == 0

	var bodies_line := "world_art_test: after %d ticks %d bodies, %d drawn on screen, %d off it" % [
		TICKS, bodies, on_screen, off_screen]
	print(bodies_line + ": " + ("pass" if bodies_right else "FAIL"))

	# Buildings: every region structure but a field is drawn as its kind's art, its footprint's
	# bottom-left on the ground on the sprite's position and on the frame's, INSET in from its left and
	# bottom edges, raised with the ground, holding each stage's frame and showing its stage's; a
	# training ground lies on the ground, under every body, and casts no shadow, and every other kind
	# stands, its shadow on the ground from its footprint's top-left. Setup: the region holds a seat's
	# training ground, manual hall and gate. Control: the structures above, made by create() with no
	# kind, are counted undrawn by the same count.
	var region := WorldAdapter.create_region(SEED)
	var region_art: Node2D = load(SCENE).instantiate()
	region_art.show_world(region, region.army_count() - 1)
	region_art.refresh(Rect2(Vector2.ZERO, Vector2(480, 270)))
	var drawn := _placed(region, region_art)
	var placed: int = drawn.placed if drawn.batched else 0
	var fields: int = drawn.fields
	var kinds: Dictionary = drawn.kinds
	var undrawn := _undrawn(region, region_art)
	var unkinded := _undrawn(world, art)
	var seat_kinds := ["training_ground", "manual_hall", "seat_gate"].all(func(kind: String) -> bool: return kinds.has(kind))
	var buildings_right := placed + fields == region.structure_count() and kinds.size() == 12 \
			and seat_kinds and undrawn == 0 and unkinded == world.structure_count() and unkinded > 0
	print("world_art_test: %d of %d region structures drawn as their kind %s over their footprints with their stage's frame and shadow, a seat's kinds among them %s, %d fields tiled, %d left undrawn; %d of %d structures without a kind left undrawn: %s" % [
		placed, region.structure_count(), kinds.keys(), seat_kinds, fields, undrawn, unkinded,
		world.structure_count(), "pass" if buildings_right else "FAIL"])

	# Banners: each village's banners stand on the cells its template's data names, a seat's in the
	# sect banner's art and every other's in the banner's, in its owner's colours, and every seat
	# has sect banners. Control: no sprite of either art stands anywhere else.
	var banners_named := 0
	var banners_placed := 0
	var seats_bannered := {}
	var banner_sprites := {}
	for sprite in region_art.get_node("Standing").get_children():
		if sprite is Sprite2D and sprite.texture is AtlasTexture:
			banner_sprites[sprite] = true
	var factions := ["orthodox", "demonic", "unorthodox"]
	var layouts: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/villages.json"))
	for village: Dictionary in region.villages():
		for item: Dictionary in layouts.get(village.kind, []):
			if item.has("tile"):
				continue
			var banner: String = item.keys()[0]
			var cell: Vector2i = village.corner + Vector2i(int(item[banner][0]), int(item[banner][1]))
			var sheet := _sheet("res://art/decorations/%s.json" % banner)
			var texture: Texture2D = load("res://art/decorations/%s.png" % banner)
			var tag: Array = sheet.tags[factions[int(village.faction)]]
			banners_named += 1
			for sprite: Sprite2D in banner_sprites:
				if sprite.texture.atlas == texture and sprite.position == Vector2(cell.x, cell.y + 1) * WorldArt.CELL \
						and sprite.texture.region.position.x == tag[0] * sheet.width:
					banners_placed += 1
					if village.kind == "seat" and banner == "sect_banner":
						seats_bannered[village.corner] = true
					banner_sprites.erase(sprite)
					break
	var stray := 0
	for sprite: Sprite2D in banner_sprites:
		var atlas: Texture2D = sprite.texture.atlas
		if atlas.resource_path.begins_with("res://art/decorations/"):
			stray += 1
	var seats := region.villages().filter(func(village: Dictionary) -> bool: return village.kind == "seat").size()
	var banners_right := seats > 0 and seats_bannered.size() == seats and banners_placed == banners_named \
			and stray == 0
	print("world_art_test: %d of %d banners the templates name stand on their cells in their owner's colours, sect banners at %d of %d seats; %d stand elsewhere: %s" % [
		banners_placed, banners_named, seats_bannered.size(), seats, stray, "pass" if banners_right else "FAIL"])

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
	# frame of the tag named, a seat's court and path among them; control: the cell west of the
	# village has none.
	var tiled := 0
	var expected := 0
	var seat_tiled := 0
	var outside_bare := true
	for village: Dictionary in region.villages():
		var counts := _decorated(region_art.built, village)
		tiled += counts.x
		expected += counts.y
		if village.kind == "seat":
			seat_tiled += counts.x
		outside_bare = outside_bare \
				and region_art.built.get_cell_source_id(village.corner + Vector2i(-1, 4)) == -1
	var decorations_right := expected > 0 and tiled == expected and seat_tiled > 0 and outside_bare
	print("world_art_test: %d of %d decorated cells carry their template's tile, %d of them a seat's; cells beside the villages bare %s: %s" % [
		tiled, expected, seat_tiled, outside_bare, "pass" if decorations_right else "FAIL"])

	region_art.free()
	var passed := relief_right and bodies_right and buildings_right and banners_right and decorations_right \
			and fields_right
	art.free()
	return _bands_pass() and _between_pass() and _founded_pass() and _every_kind_pass() and _lights_pass() \
			and _techniques_pass() and passed


# The lights of the region SEED makes: over the whole map, every window lights.json names for each
# structure's kind at its stage, at the sprite's frame, and nothing else, and the steered person's
# torch at its feet beside them. Over a view round one lit structure, the windows reaching into it,
# from only the structures near it (#336). Setup: the map holds lit windows and the steered person
# lives. Controls: a view beside the map lists none and visits none, with no one steered there is no
# torch, and the version holds for a list unchanged and moves on for one changed.
func _lights_pass() -> bool:
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/lights.json"))
	var region := WorldAdapter.create_region(SEED)
	var steered := region.army_count() - 1
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(region, steered)
	var rows := region.map_rows()
	var whole := Rect2(Vector2.ZERO, Vector2(rows[0].length(), rows.size()) * WorldArt.CELL)
	art.refresh(whole)
	var shown := _entries(art.lights(whole))
	var whole_version: int = art.lights_version
	var whole_visited: int = art.lights_visited
	var again := _entries(art.lights(whole))
	var steady: bool = art.lights_version == whole_version and again == shown
	var expected := _windows(art, region, data, whole)
	var drawn := 0
	var lit_at := Vector2.INF
	for sprite: Sprite2D in art.structure_sprites:
		if sprite != null:
			drawn += 1
			if lit_at == Vector2.INF and not sprite.get_meta("lights").get("intact", []).is_empty():
				lit_at = sprite.position + sprite.offset
	var near := Rect2(lit_at - VIEW / 2.0, VIEW)
	var near_shown := _entries(art.lights(near))
	var near_expected := _windows(art, region, data, near)
	var near_visited: int = art.lights_visited
	var moved: bool = art.lights_version == whole_version + 1
	var feet: Vector2 = art.body_at(steered, 0, 1.0)
	var torch: Array = data.torch
	var torch_pass: bool = art.torch(whole) == PackedFloat32Array([feet.x + torch[0], feet.y + torch[1], torch[2], torch[3], torch[4]])
	var beside_view := Rect2(Vector2(-10000, 0), VIEW)
	var beside: PackedFloat32Array = art.lights(beside_view)
	var beside_visited: int = art.lights_visited
	var beside_torch: PackedFloat32Array = art.torch(beside_view)
	art.free()
	var unsteered: Node2D = load(SCENE).instantiate()
	unsteered.show_world(region, -1)
	unsteered.refresh(whole)
	var untorched: PackedFloat32Array = unsteered.torch(whole)
	unsteered.free()
	var passed := expected.size() > 0 and shown == expected and whole_visited == drawn and steady \
			and near_shown.size() > 0 and near_shown == near_expected and near_visited < drawn and moved and torch_pass \
			and beside.is_empty() and beside_visited == 0 and beside_torch.is_empty() and untorched.is_empty()
	print("world_art_test: %d windows listed over the region against %d expected, visiting %d of %d structures, the version held when unchanged %s; %d round a lit structure against %d, visiting %d, the version moved on %s; the torch at %s %s; %d beside the map visiting %d, no torch there %s or with no one steered %s: %s" % [
		shown.size(), expected.size(), whole_visited, drawn, steady, near_shown.size(), near_expected.size(), near_visited,
		moved, feet, torch_pass, beside.size() / WorldArt.LIGHT_FLOATS, beside_visited, beside_torch.is_empty(),
		untorched.is_empty(), "pass" if passed else "FAIL"])
	return passed


# Each window lit at night of every structure `art` draws of `region`, by `data`, whose light reaches
# into `view`, as _entries() gives them: every structure looked at.
func _windows(art: Node2D, region: WorldAdapter, data: Dictionary, view: Rect2) -> Array:
	var stages := ["intact", "damaged", "destroyed", "rubble"]
	var listed := PackedFloat32Array()
	for structure in region.structure_count():
		var sprite: Sprite2D = art.structure_sprites[structure]
		if sprite == null:
			continue
		var of_kind: Dictionary = data.structures.get(region.structure_kind(structure), {})
		for light: Array in of_kind.get(stages[region.structure_stage(structure)], []):
			var rect := Rect2(sprite.position + sprite.offset + Vector2(light[0], light[1]), Vector2(light[2], light[3]))
			if rect.grow(light[4]).intersects(view):
				listed.append_array([rect.position.x, rect.position.y, rect.size.x, rect.size.y, light[4]])
	return _entries(listed)


# Each light of `lights`, as lights() gives them, as an array of its floats, sorted, as lights() lists
# them by area rather than by structure.
func _entries(lights: PackedFloat32Array) -> Array:
	var entries := []
	for at in range(0, lights.size(), WorldArt.LIGHT_FLOATS):
		entries.append(Array(lights.slice(at, at + WorldArt.LIGHT_FLOATS)))
	entries.sort()
	return entries


func _every_kind_pass() -> bool:
	var region := WorldAdapter.create_region(SEED, 0, SHORT_DAY)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(region, region.army_count() - 1)
	var workshops := ["smithy", "apothecary"]
	var held := func() -> Array:
		var found := []
		for structure in region.structure_count():
			var kind := region.structure_kind(structure)
			if kind in workshops and not kind in found:
				found.append(kind)
		return found
	var at_start: Array = held.call()
	var ticks := 0
	while held.call().size() < workshops.size() and ticks < WORKSHOP_DAYS * SHORT_DAY:
		for step in 50:
			region.tick()
		ticks += 50
	art.note_tick()
	art.refresh(Rect2(Vector2.ZERO, Vector2(480, 270)))
	var drawn := _placed(region, art)
	var every: bool = WorldArt.BUILDINGS.keys().all(func(kind: String) -> bool: return drawn.kinds.has(kind))
	# Each hearth lights.json names for a structure's kind at its stage, a light with no width or height,
	# is counted as one, over the whole map and beside it none.
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/lights.json"))
	var stages := ["intact", "damaged", "destroyed", "rubble"]
	var rows := region.map_rows()
	var whole := Rect2(Vector2.ZERO, Vector2(rows[0].length(), rows.size()) * WorldArt.CELL)
	var hearths := 0
	for structure in region.structure_count():
		var of_kind: Dictionary = data.structures.get(region.structure_kind(structure), {})
		for light: Array in of_kind.get(stages[region.structure_stage(structure)], []):
			if light[2] == 0 and light[3] == 0:
				hearths += 1
	art.lights(whole)
	var counted: int = art.hearths
	art.lights(Rect2(Vector2(-10000, 0), Vector2(480, 270)))
	var hearth_pass: bool = hearths > 0 and counted == hearths and art.hearths == 0
	var passed: bool = at_start.is_empty() and held.call().size() == workshops.size() \
			and drawn.placed + drawn.fields == region.structure_count() and every and drawn.batched and hearth_pass
	print("world_art_test: after %d days, the workshops at the start %s and now %s; %d of %d structures drawn as their kind, of every kind world_art.gd draws %s (%s); %d hearths counted of %d lights.json names, none beside the map %s: %s" % [
		ticks / SHORT_DAY, at_start, held.call(), drawn.placed + drawn.fields, region.structure_count(), every,
		drawn.kinds.keys(), counted, hearths, art.hearths == 0, "pass" if passed else "FAIL"])
	art.free()
	return passed


# Of `world`'s structures as `art` draws them, {placed, fields, kinds}: how many but fields stand
# as their kind's art over their footprint, its bottom-left on the ground on the sprite's position
# and on the frame's, INSET in from its left and bottom edges, raised with the ground, holding each
# stage's frame and showing its stage's, a training ground on the ground with no shadow and every
# other kind standing with its shadow on the ground from its footprint's top-left; how many are
# fields; the kinds placed; and whether every shadow is drawn by the Shadows node from one texture,
# nothing but the ground's kinds laid beside it.
func _placed(world: WorldAdapter, art: Node2D) -> Dictionary:
	var stages := ["intact", "damaged", "destroyed", "rubble"]
	var placed := 0
	var fields := 0
	var kinds := {}
	for structure in world.structure_count():
		var sprite: Sprite2D = art.structure_sprites[structure]
		var footprint := world.structure_footprint(structure)
		var corner := world.structure_cell(structure)
		var kind := world.structure_kind(structure)
		if kind == "field":
			fields += 1
			continue
		var sheet := _sheet("res://art/structures/%s.json" % kind)
		if sprite == null or sheet.is_empty():
			continue
		var frame := (sprite.texture as AtlasTexture).region
		var stage: String = stages[world.structure_stage(structure)]
		var layer := "Laid" if kind == "training_ground" else "Standing"
		var staged := true
		for index in stages.size():
			var held: AtlasTexture = sprite.get_meta("frames")[index]
			staged = staged and held.region.position.x == sheet.tags[stages[index]][0] * sheet.width
		var bottom := Vector2(corner.x, corner.y + footprint.y) * WorldArt.CELL
		var lift: float = art.relief.lift_at(bottom + Vector2(footprint.x * WorldArt.CELL.x / 2.0,
				-WorldArt.CELL.y / 2.0))
		var standing := sprite.position if kind != "training_ground" else bottom
		var shadow: Dictionary = art.get_node("Shadows").shadow_of(structure)
		var shadowed: bool = shadow.is_empty() if kind == "training_ground" else \
				not shadow.is_empty() and shadow.at == Vector2(corner) * WorldArt.CELL - Vector2(0, lift) \
				and shadow.footprint == footprint and shadow.stage == world.structure_stage(structure)
		if standing == bottom and shadowed \
				and (sprite.position + sprite.offset + Vector2(INSET, frame.size.y - INSET)).is_equal_approx(bottom - Vector2(0, lift)) \
				and sprite.get_parent().name == layer and staged \
				and frame.position.x == sheet.tags[stage][0] * sheet.width \
				and frame.size.x == sheet.width and sheet.width == footprint.x * 16 + 16:
			placed += 1
			kinds[kind] = true
	# Every shadow is drawn by the one node, from one texture: no sprite is laid with the ground but
	# a structure of the ground's kinds.
	var laid_others := art.get_node("Laid").get_children().filter(func(node: Node) -> bool:
			return node is Sprite2D and (node as Sprite2D).get_meta("frames", []) == [])
	var batched: bool = art.get_node("Shadows").textures() == (1 if placed > 0 else 0) and laid_others.is_empty()
	return {"placed": placed, "fields": fields, "kinds": kinds, "batched": batched}


# A village the region founds is drawn as a generated one is: every structure it lays has a
# sprite, and its square, the land the core says it was founded on, carries the ground
# decorations its template's data names, from the square's corner. Setup: the region, on short
# days, founds a village within FOUNDING_DAYS. Control: the cell west of its square is bare.
func _founded_pass() -> bool:
	var region := WorldAdapter.create_region(SEED, 0, SHORT_DAY)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(region, region.army_count() - 1)
	var generated := region.villages()
	var ticks := 0
	while region.villages().size() == generated.size() and ticks < FOUNDING_DAYS * SHORT_DAY:
		region.tick()
		ticks += 1
	art.note_tick()
	# A refresh lays the sprites of the structures laid since.
	art.refresh(Rect2(Vector2.ZERO, Vector2(480, 270)))
	var villages := region.villages()
	var founded: Dictionary = villages[-1] if villages.size() > generated.size() else {}
	var set_up := not founded.is_empty()
	var undrawn := _undrawn(region, art) if set_up else -1
	var counts := _decorated(art.built, founded) if set_up else Vector2i.ZERO
	var outside_bare: bool = set_up \
			and art.built.get_cell_source_id(founded.corner + Vector2i(-1, 0)) == -1
	var passed := undrawn == 0 and counts.y > 0 and counts.x == counts.y and outside_bare
	print("world_art_test: after %d days a village founded %s on its square at %s of %s: %d structures without a sprite; %d of %d decorated cells carry their template's tile, the cell west of it bare %s: %s" % [
		ticks / SHORT_DAY, founded.get("kind", "none"), founded.get("corner", "none"),
		founded.get("area", "none"), undrawn, counts.x, counts.y, outside_bare,
		"pass" if passed else "FAIL"])
	art.free()
	return passed


# How many of `world`'s structures other than fields `art` gives no sprite.
func _undrawn(world: WorldAdapter, art: Node2D) -> int:
	var undrawn := 0
	for structure in world.structure_count():
		if world.structure_kind(structure) != "field" and art.structure_sprites[structure] == null:
			undrawn += 1
	return undrawn


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
	var home: Vector2 = art.ground_point(region.body_positions(steered)[0])
	var far := home
	for village: Dictionary in region.villages():
		var corner := Vector2(village.corner) * WorldArt.CELL
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


# Between ticks a body is drawn the share of the way refresh() is given from where the tick before
# last left it to where the last tick did, as body_at() places it, and the ring stands on the
# steered body drawn so; at a share of 1 it is drawn where the last tick left it. Control: after a
# tick that changed an army's roster, its bodies are drawn where that tick left them at any share,
# since a body's index may then name someone else. Setup: the body moved on the last tick, and a
# tick changed a roster with a body at some index standing elsewhere than that index's a tick
# before, so drawing between the two would have moved it.
func _between_pass() -> bool:
	var world := WorldAdapter.create(42, PackedStringArray(MAP_ROWS), ARMIES, STRUCTURES, RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, 0)
	var scale := WorldArt.CELL / WorldAdapter.CELL_SIZE
	world.tick()
	art.note_tick()
	var before := world.body_positions(0)[0] * scale
	world.tick()
	art.note_tick()
	var after := world.body_positions(0)[0] * scale
	var halfway := before.lerp(after, 0.5)
	var view := Rect2(Vector2.ZERO, Vector2(MAP_ROWS[0].length(), MAP_ROWS.size()) * WorldArt.CELL)
	art.refresh(view, 0.5)
	var drawn_halfway := _drawn_at(art, Vector2(halfway.x, halfway.y - WorldArt.FEET + WorldArt.BODY_FRAME.y / 2.0).round())
	var ring_halfway: bool = art.get_node("Ring").position == halfway.round()
	var placed: bool = art.body_at(0, 0, 0.5) == halfway and art.body_at(0, 0, 1.0) == after \
			and art.body_at(0, 0, 0.0) == before
	var changed := -1
	var prior: Array[PackedVector2Array] = []
	for i in TICKS:
		prior = [world.body_positions(0), world.body_positions(1)]
		world.tick()
		art.note_tick()
		for army in 2:
			if world.body_positions(army).size() != prior[army].size() and changed < 0:
				changed = army
		if changed >= 0:
			break
	var held := changed >= 0
	var shifted := false
	if held:
		var left := world.body_positions(changed)
		for body in left.size():
			held = held and art.body_at(changed, body, 0.5) == left[body] * scale
			shifted = shifted or (body < prior[changed].size() and prior[changed][body] != left[body])
	art.free()
	var passed := before != after and placed and drawn_halfway and ring_halfway and held and shifted
	print("world_art_test: a body moved from %s to %s on the last tick is drawn at %s halfway to the next %s, its ring there %s, and at either end at a share of 0 and 1 %s; army %d's roster changed, an index's body elsewhere than a tick before %s, and its bodies are drawn where that tick left them %s: %s" % [
		before, after, halfway, drawn_halfway, ring_halfway, placed, changed, shifted, held, "pass" if passed else "FAIL"])
	return passed


# Whether a band holds a body whose centre is at `centre`, in pixels.
func _drawn_at(art: Node2D, centre: Vector2) -> bool:
	for band in _bands(art):
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			if Vector2(buffer[i * WorldArt.BODY_FLOATS + 3], buffer[i * WorldArt.BODY_FLOATS + 7]) + band.position == centre:
				return true
	return false


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
		var at := i * WorldArt.BODY_FLOATS
		var size := Vector2(absf(buffer[at]), absf(buffer[at + 5]))
		var quad := Rect2(Vector2(buffer[at + 3], buffer[at + 7]) - size / 2, size)
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


# A region's arts rise in their disciplines' colours from techniques.json, by the names the world
# gives them: Plum Blossom in the sword's azure, Heavenly Demon in the saber's violet, Black Fang in
# hidden weapons' amber. Control: an art of a discipline techniques.json does not name rises in paper.
func _techniques_pass() -> bool:
	var region := WorldAdapter.create_region(SEED)
	var colours := WorldArt.technique_colours(region.arts(), region.discipline_names())
	var named := Array(colours).map(func(colour: Color) -> String: return colour.to_html(false))
	var unnamed := WorldArt.technique_colours([{"discipline": 0}], PackedStringArray(["staff"]))
	var passed: bool = named == ["9ac4cd", "cb88bd", "e9c36d"] and unnamed[0] == WorldArt.Combat.PAPER
	print("world_art_test: the arts rise in %s; an art of an unnamed discipline in %s: %s" % [
		named, unnamed[0].to_html(false), "pass" if passed else "FAIL"])
	return passed
