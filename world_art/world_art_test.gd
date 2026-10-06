# Checks that world_art.tscn lays a world as docs/convention/stack-godot.md §Terrain layers sets
# out: the plain on every cell, and a kind's dual-grid tile carrying the corner mask of the four
# cells around it; that it draws every body on screen and none off it; and that a building is
# drawn over its footprint with its stage's frame of the art for that footprint.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const SEED := 2026
const TICKS := 2000
const FOCUS := Vector2i(4 * 256, 4 * 256)
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
const ARMIES: Array[Dictionary] = [
	{"start": Vector2i(1, 1), "objective": Vector2i(14, 7), "count": 12, "strength": 1200,
		"faction": 0, "skills": [BLAST]},
	{"start": Vector2i(14, 7), "objective": Vector2i(1, 1), "count": 10, "strength": 1010,
		"faction": 1, "skills": [BLAST]},
]
const STRUCTURES: Array[Dictionary] = [
	{"cell": Vector2i(7, 7), "durability": 100},
	{"cell": Vector2i(9, 7), "durability": 60},
]
const RULES := {"separation": 72, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}


func run() -> bool:
	var world := WorldAdapter.create(42, PackedStringArray(MAP_ROWS), ARMIES, STRUCTURES, RULES)
	world.set_focus(FOCUS)
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
		bodies += world.body_counts(army).size()
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

	# Buildings: a region's structures have footprints with art, each drawn with its footprint's
	# bottom-left on the sprite's position, showing its stage's frame of the art for its width.
	# Control: the structures above, one cell each, have no art and get no sprite.
	var region := WorldAdapter.create_region(SEED)
	var region_art: Node2D = load(SCENE).instantiate()
	region_art.show_world(region, region.army_count() - 1)
	region_art.refresh(Rect2(Vector2.ZERO, Vector2(480, 270)))
	var sheets := {3: _tags("res://art/structures/house.json"), 5: _tags("res://art/structures/hall.json")}
	var widths := {3: 64, 5: 96}
	var stages := ["intact", "damaged", "destroyed", "rubble"]
	var placed := 0
	for structure in region.structure_count():
		var sprite: Sprite2D = region_art.structure_sprites[structure]
		var footprint := region.structure_footprint(structure)
		var corner := region.structure_cell(structure)
		if sprite == null or not sheets.has(footprint.x):
			continue
		var frame := (sprite.texture as AtlasTexture).region
		var stage: String = stages[region.structure_stage(structure)]
		if sprite.position == Vector2(corner.x, corner.y + footprint.y) * 16 \
				and frame.position.x == sheets[footprint.x][stage][0] * widths[footprint.x] \
				and frame.size.x == widths[footprint.x]:
			placed += 1
	var one_cell_skipped: bool = art.structure_sprites.all(func(sprite: Sprite2D) -> bool: return sprite == null)
	var buildings_right := placed == region.structure_count() and placed > 0 and one_cell_skipped
	print("world_art_test: %d of %d region structures drawn over their footprints with their stage's frame; one-cell structures left undrawn %s: %s" % [
		placed, region.structure_count(), one_cell_skipped, "pass" if buildings_right else "FAIL"])
	region_art.free()
	var passed := plain_full and masks_right and bodies_right and buildings_right
	art.free()
	return passed


# The first kind layer with any tile: here, the only kind beside the plain is mountain.
func _layer_with_tiles(ground: Array[Node]) -> TileMapLayer:
	for layer: TileMapLayer in ground.slice(1):
		if not layer.get_used_cells().is_empty():
			return layer
	return null


func _tags(path: String) -> Dictionary:
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	var tags := {}
	for tag: Dictionary in data.meta.frameTags:
		tags[tag.name] = [int(tag.from), int(tag.to)]
	return tags
