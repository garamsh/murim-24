# Checks that world_art.tscn lays a world as docs/convention/stack-godot.md §Terrain layers sets
# out: the plain on every cell, and a kind's dual-grid tile carrying the corner mask of the four
# cells around it; that it draws every body on screen and none off it; and that a building shows
# the frame of its stage.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
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
	{"cell": Vector2i(8, 8), "durability": 100},
	{"cell": Vector2i(7, 8), "durability": 60},
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

	var house := _tags("res://art/structures/house.json")
	var stages := ["intact", "damaged", "destroyed", "rubble"]
	var broken := 0
	var frames_right := true
	for structure in world.structure_count():
		var stage := world.structure_stage(structure)
		broken += 1 if stage != WorldAdapter.STAGE_INTACT else 0
		var sprite: Sprite2D = art.structure_sprites[structure]
		var region := (sprite.texture as AtlasTexture).region
		frames_right = frames_right and region.position.x == house[stages[stage]][0] * 64
	var passed := plain_full and masks_right and bodies_right and broken > 0 and frames_right
	print("world_art_test: after %d ticks %d bodies, %d drawn on screen, %d off it; %d of %d structures broken, frames match stages %s: %s" % [
		TICKS, bodies, on_screen, off_screen, broken, world.structure_count(), frames_right,
		"pass" if passed else "FAIL"])
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
