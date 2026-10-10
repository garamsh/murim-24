# Checks how world_art.tscn draws bodies at the 2.5D angle: a person walking away from the viewer
# shows its back and one walking across its walk; a person who just struck shows its strike; every
# body drawn has a shadow under it in its band; and a person standing just north of a house, which
# hides it, is outlined over the house, while one standing just south of it, in front, is not.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const WorldArt := preload("res://world_art/world_art.gd")
const FACTIONS := ["orthodox", "demonic", "unorthodox"]
const MAP_ROWS: Array[String] = [
	"............",
	"............",
	"............",
	"............",
	"............",
	"............",
	"............",
	"............",
]
const STEP := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}
const SWEEP := {"range": 384, "radius": 0, "body_damage": 10, "structure_damage": 0, "cooldown": 30,
	"shape": WorldAdapter.SHAPE_ARC}
const RULES := {"separation": 256, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}
const VIEW := Rect2(0, 0, 480, 270)
const REGION_SEED := 2026
const WALK_TICKS := 600


func run() -> bool:
	return _facing_pass() and _strike_pass() and _outline_pass()


# One person walks north and one east, from the same row: after two ticks the first shows a frame
# of the back tag and the second of the walk tag, and each drawn body has one shadow in its band.
func _facing_pass() -> bool:
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(2, 0), "faction": 0, "skills": [STEP], "members": [{"cell": Vector2i(2, 6), "health": 100}]},
		{"objective": Vector2i(11, 6), "faction": 0, "skills": [STEP], "members": [{"cell": Vector2i(6, 6), "health": 100}]},
	]
	var world := WorldAdapter.create(1, PackedStringArray(MAP_ROWS), armies, [], {"separation": 256,
		"hostile": [[false]], "stage_floors": [75, 25, 1]})
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, -1)
	for tick in 2:
		world.tick()
		art.note_tick()
	art.refresh(VIEW, 1.0)
	var north := _tag_at(art, art.body_at(0, 0, 1.0))
	var east := _tag_at(art, art.body_at(1, 0, 1.0))
	var shadows := _shadows(art)
	var passed: bool = north == "back" and east == "walk" and art.drawn == 2 and shadows == art.drawn
	print("bodies_test: walking north drawn as %s, east as %s; %d bodies drawn, %d shadows: %s" % [
		north, east, art.drawn, shadows, "pass" if passed else "FAIL"])
	art.free()
	return passed


# On the tick a person sweeps a foe beside it, the striker is drawn in a frame of its strike.
# Control: the foe, which struck no one, is not.
func _strike_pass() -> bool:
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(4, 4), "faction": 0, "skills": [SWEEP], "members": [{"cell": Vector2i(3, 4), "health": 100}]},
		{"objective": Vector2i(4, 4), "faction": 1, "skills": [STEP], "members": [{"cell": Vector2i(4, 4), "health": 100}]},
	]
	var world := WorldAdapter.create(1, PackedStringArray(MAP_ROWS), armies, [], RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, 0)
	var struck := false
	for tick in 60:
		world.tick()
		art.note_tick()
		if not world.strikes().filter(func(strike: Dictionary) -> bool: return strike.body_damage > 0).is_empty():
			struck = true
			break
	art.refresh(VIEW, 1.0)
	var striker := _tag_at(art, art.body_at(0, 0, 1.0))
	var foe := _tag_at(art, art.body_at(1, 0, 1.0))
	var passed := struck and striker == "strike" and foe != "strike" and foe != ""
	print("bodies_test: on the tick of a sweep, %s, the striker drawn as %s and the foe as %s: %s" % [
		struck, striker, foe, "pass" if passed else "FAIL"])
	art.free()
	return passed


# The steered person, walked to the cell just north of the house nearest it, is outlined over it;
# walked to the cell just south of it, it is not. Setup: it reaches each cell.
func _outline_pass() -> bool:
	var behind := _walked_beside_house(-1)
	var front := _walked_beside_house(1)
	var passed: bool = behind.reached and front.reached and behind.hiders == [behind.house] and behind.outlined \
			and front.hiders.is_empty() and not front.outlined
	print("bodies_test: north of house %d at %s reached %s, hidden by %s, outlined %s; south of it at %s reached %s, hidden by %s, outlined %s: %s" % [
		behind.house, behind.cell, behind.reached, behind.hiders, behind.outlined, front.cell, front.reached, front.hiders,
		front.outlined, "pass" if passed else "FAIL"])
	return passed


# Walks the steered person of REGION_SEED's region to the cell beside the house nearest it, north
# of it for `side` -1 and south for 1, and reads whether it is outlined there, and what stands in
# front of it and covers its frame: the structures, by index, and -1 for anything of the ground.
func _walked_beside_house(side: int) -> Dictionary:
	var world := WorldAdapter.create_region(REGION_SEED)
	var art: Node2D = load(SCENE).instantiate()
	var steered := world.army_count() - 1
	art.show_world(world, steered)
	var lead := world.body_positions(steered)[0] / WorldAdapter.CELL_SIZE
	var house := -1
	for structure in world.structure_count():
		if world.structure_kind(structure) == "house" and (house < 0
				or Vector2(world.structure_cell(structure)).distance_to(lead)
				< Vector2(world.structure_cell(house)).distance_to(lead)):
			house = structure
	var corner := world.structure_cell(house)
	var size := world.structure_footprint(house)
	var cell := corner + Vector2i(1, -1 if side < 0 else size.y)
	world.order(steered, cell, PackedInt64Array([0]))
	var reached := false
	for tick in WALK_TICKS:
		world.tick()
		art.note_tick()
		if Vector2i((world.body_positions(steered)[0] / WorldAdapter.CELL_SIZE).floor()) == cell:
			reached = true
			break
	var at: Vector2 = art.body_at(steered, 0, 1.0)
	art.refresh(Rect2(at - VIEW.size / 2.0, VIEW.size), 1.0)
	var outlines: MultiMeshInstance2D = art.get_node("Outlines")
	var centre := Vector2(at.x, at.y - WorldArt.FEET + WorldArt.BODY_FRAME.y / 2.0).round()
	var outlined := false
	var buffer := outlines.multimesh.buffer
	for i in outlines.multimesh.visible_instance_count:
		if Vector2(buffer[i * WorldArt.BODY_FLOATS + 3], buffer[i * WorldArt.BODY_FLOATS + 7]) == centre:
			outlined = true
	var body := Rect2(centre - Vector2(WorldArt.BODY_FRAME) / 2.0, WorldArt.BODY_FRAME).grow(-2)
	var sort_y := cell.y * WorldArt.CELL.y + WorldArt.CELL.y / 2.0
	var hiders := []
	for structure in art.structure_sprites.size():
		var sprite: Sprite2D = art.structure_sprites[structure]
		if sprite != null and sprite.position.y > sort_y \
				and Rect2(sprite.position + sprite.offset, sprite.texture.get_size()).intersects(body):
			hiders.append(structure)
	for occluder: WorldArt.Ground.Occluder in art.get_node("Ground").occluders():
		if occluder.sort_y > sort_y and occluder.rect.intersects(body):
			hiders.append(-1)
	art.free()
	return {"cell": cell, "reached": reached, "outlined": outlined, "house": house, "hiders": hiders}


# The tag of the frame a band draws the body standing at `at` with, within the knock a hit gives
# it, or "" for none.
func _tag_at(art: Node2D, at: Vector2) -> String:
	var centre := Vector2(at.x, at.y - WorldArt.FEET + WorldArt.BODY_FRAME.y / 2.0).round()
	var sheet: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://art/bodies/orthodox.json"))
	var faction_width: int = int(sheet.frames[0].frame.w) * sheet.frames.size()
	var atlas_width := faction_width * FACTIONS.size() + WorldArt.BODY_FRAME.x
	for band in _bands(art):
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			var base := i * WorldArt.BODY_FLOATS
			if (Vector2(buffer[base + 3], buffer[base + 7]) + band.position).distance_to(centre) > 3.0:
				continue
			if is_equal_approx(buffer[base + 12], 1.0 - float(WorldArt.BODY_FRAME.x) / atlas_width):
				continue
			var frame := (roundi(buffer[base + 12] * atlas_width) % faction_width) / WorldArt.BODY_FRAME.x
			for tag: Dictionary in sheet.meta.frameTags:
				if frame >= int(tag.from) and frame <= int(tag.to):
					return tag.name
	return ""


# How many shadows the bands hold: instances drawn from the atlas's last slot.
func _shadows(art: Node2D) -> int:
	var count := 0
	for band in _bands(art):
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			if is_equal_approx(buffer[i * WorldArt.BODY_FLOATS + 12],
					1.0 - float(WorldArt.BODY_FRAME.x) / art.get_node("Outlines").texture.get_width()):
				count += 1
	return count


func _bands(art: Node2D) -> Array[MultiMeshInstance2D]:
	var bands: Array[MultiMeshInstance2D] = []
	for child in art.get_node("Standing").get_children():
		if child is MultiMeshInstance2D:
			bands.append(child)
	return bands
