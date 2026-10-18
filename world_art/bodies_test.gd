# Checks how world_art.tscn draws bodies at the 2.5D angle: a person walking away from the viewer
# shows its back and one walking across its walk; on the tick a sweep hits, the striker swings and the
# struck recoils and flashes (#184), and with no strike neither does; every
# body drawn has a shadow under it in its band; a person standing just north of a house, which
# hides it, is outlined over the house, while one standing just south of it, in front, is not; and
# an outlined body's outline carries the share of white its body flashes with as it is struck, for
# outline.gdshader to tell the body's pixels from what hides it (world_art/outline_check_tool.gd
# checks the pixels it then draws, which need a renderer); every base-pack occupation has a look
# drawn, and a resident at work is drawn working in it.
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
# An hour the base pack's trades are at work.
const WORK_HOUR := 10
# Ticks the steered person has to walk to the house it is sent beside: at about 16 ticks a cell,
# past half the region's width, since every house of its own village can have the temple, the inn or
# rock before it.
const WALK_TICKS := 3000
# Ticks of the M1 battle searched for a body outlined while it flashes.
const BATTLE_TICKS := 400
# Rows before a house that nothing else stands on, so whatever stands south of it rises over no one
# walked to its front.
const FRONT_CLEAR := 6


func run() -> bool:
	return _facing_pass() and _strike_pass() and _outline_pass() and _flash_pass() and _looks_pass() \
			and _work_pass()


# Every occupation of the base pack finds its look's `<id>_idle`, `<id>_walk` and `<id>_work`
# tags, and their outlines, in every faction's body sheet, so a trade left undrawn fails here rather
# than falling back unseen (ADR 0081). Control: an id no sheet tags falls back, six -1s. Setup: the
# base pack names occupations.
func _looks_pass() -> bool:
	var names := WorldAdapter.create_region(REGION_SEED, 0).occupation_names()
	var missing: Array[String] = []
	for faction: String in FACTIONS:
		var tags := {}
		var sheet: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://art/bodies/%s.json" % faction))
		for tag: Dictionary in sheet.meta.frameTags:
			tags[tag.name] = [int(tag.from), int(tag.to)]
		var looks := WorldArt.trade_looks(names, tags)
		for occupation in names.size():
			if looks.slice(occupation * 6, occupation * 6 + 6).has(-1):
				missing.append("%s %s" % [faction, names[occupation]])
		var none := WorldArt.trade_looks(PackedStringArray(["nonesuch"]), tags)
		if none != PackedInt64Array([-1, -1, -1, -1, -1, -1]):
			missing.append("%s nonesuch found %s" % [faction, none])
	var passed := not names.is_empty() and missing.is_empty()
	print("bodies_test: the base pack's occupations %s each find their look in every faction's sheet, and an id with none falls back; missing: %s: %s" % [
		names, missing if not missing.is_empty() else "none", "pass" if passed else "FAIL"])
	return passed


# In seed 2026's region at WORK_HOUR, a resident the core reads as working at its workplace is drawn
# in its occupation's work motion. Control: once its army is steered, it follows no schedule and the
# same person, read not working, is drawn in its look but not working. Control: the player's body,
# though it holds an occupation, is drawn as its faction's. Setup: someone with an occupation is
# working, using no skill on either tick, whose motion would be drawn over its look, and the player
# holds an occupation.
func _work_pass() -> bool:
	var world := WorldAdapter.create_region(REGION_SEED, 0)
	var day: int = world.date().ticks_per_day
	while world.date().tick * 24 / day != WORK_HOUR:
		world.tick()
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, world.player_army())
	art.note_tick()
	var found := _working(world)
	if found.is_empty():
		print("bodies_test: no one working at hour %d: FAIL" % WORK_HOUR)
		art.free()
		return false
	var names := world.occupation_names()
	var trade: String = names[world.person_occupation(found.person)]
	var working := _drawn(art, found.army, found.body)
	world.steer(found.army, true)
	world.tick()
	art.note_tick()
	var steered := _drawn(art, found.army, found.body)
	var still := not _striking(world, found.person)
	var player := world.roster(world.player_army())[0]
	var player_trade := world.person_occupation(player)
	var player_drawn := _drawn(art, world.player_army(), 0)
	var passed := still and player_trade >= 0 and working == trade + "_work" \
			and steered in [trade + "_idle", trade + "_walk"] and player_drawn in ["idle", "walk", "back"]
	print("bodies_test: at hour %d person %d, a %s working, drawn as %s; its army steered, read %d, as %s, using no skill %s; the player, a %s, drawn as %s: %s" % [
		WORK_HOUR, found.person, trade, working, world.person_activity(found.person), steered, still,
		names[player_trade] if player_trade >= 0 else "none", player_drawn, "pass" if passed else "FAIL"])
	art.free()
	return passed


# The tag `art` draws body `body` of army `army` in, the view centred on it.
func _drawn(art: Node2D, army: int, body: int) -> String:
	var at: Vector2 = art.body_at(army, body, 1.0)
	art.refresh(Rect2(at - VIEW.size / 2.0, VIEW.size), 1.0)
	return _tag_at(art, at)


# The first person of `world` with an occupation reading ACTIVITY_WORKING and using no skill on the
# last tick: its `person`, `army` and `body` in the army's roster; or empty.
func _working(world: WorldAdapter) -> Dictionary:
	for army in world.army_count():
		var roster := world.roster(army)
		for body in roster.size():
			if world.person_occupation(roster[body]) >= 0 and not _striking(world, roster[body]) \
					and world.person_activity(roster[body]) == WorldAdapter.ACTIVITY_WORKING:
				return {"person": roster[body], "army": army, "body": body}
	return {}


# Whether `person` used a skill on `world`'s last tick.
func _striking(world: WorldAdapter, person: int) -> bool:
	return world.strikes().any(func(strike: Dictionary) -> bool: return strike.person == person)


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


# On the tick the core records a person's sweep hitting a foe beside it, the striker is drawn in a
# frame of its swing and the foe in a frame of its recoil, flashing (#184). Control: the same two,
# the first given no skill that strikes, are drawn in neither, and the second does not flash.
func _strike_pass() -> bool:
	var hit := _first_strike(SWEEP)
	var calm := _first_strike(STEP)
	var passed: bool = hit.struck and hit.striker == "swing" and hit.foe == "recoil" and hit.flash > 0.0 \
			and not calm.struck and not calm.striker in ["swing", "recoil"] and calm.foe != "recoil" \
			and calm.flash == 0.0
	print("bodies_test: on the tick of a sweep, %s, the striker drawn as %s and the foe as %s flashing %.2f; with no strike, %s, drawn as %s and %s flashing %.2f: %s" % [
		hit.struck, hit.striker, hit.foe, hit.flash, calm.struck, calm.striker, calm.foe, calm.flash,
		"pass" if passed else "FAIL"])
	return passed


# Two people beside each other, the first with `skill`, the second unable to strike, ticked until a
# use of the first hits or for 60 ticks, then drawn: whether a use hit, the tags each is drawn in and
# how white the second flashes.
func _first_strike(skill: Dictionary) -> Dictionary:
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(4, 4), "faction": 0, "skills": [skill], "members": [{"cell": Vector2i(3, 4), "health": 100}]},
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
	var found := {"struck": struck, "striker": _tag_at(art, art.body_at(0, 0, 1.0)),
		"foe": _tag_at(art, art.body_at(1, 0, 1.0)),
		"flash": art.combat.flash(world.roster(1)[0], Time.get_ticks_msec())}
	art.free()
	return found


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


# Walks the steered person of REGION_SEED's region to the cell beside the house nearest it with
# nothing standing in the FRONT_CLEAR rows before it, north of it for `side` -1 and south for 1, and
# reads whether it is outlined there, and what stands in front of it and covers its frame: the
# structures, by index, and -1 for anything of the ground.
func _walked_beside_house(side: int) -> Dictionary:
	var world := WorldAdapter.create_region(REGION_SEED)
	var art: Node2D = load(SCENE).instantiate()
	var steered := world.army_count() - 1
	art.show_world(world, steered)
	var lead := world.body_positions(steered)[0] / WorldAdapter.CELL_SIZE
	var house := -1
	for structure in world.structure_count():
		if world.structure_kind(structure) == "house" and _clear_before(world, structure) and (house < 0
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


# Over the M1 battle, drawn whole, every outlined body's outline carries the share of white its
# body is drawn with in its band, and at some refresh an outlined body is flashing, struck, while
# another is not, the control. Setup: bodies are outlined, and each outline has its body in a band.
func _flash_pass() -> bool:
	var world := WorldAdapter.create_m1()
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, -1)
	var rows := world.map_rows()
	var whole := Rect2(Vector2.ZERO, Vector2(rows[0].length(), rows.size()) * WorldArt.CELL)
	var outlines: MultiMeshInstance2D = art.get_node("Outlines")
	var checked := 0
	var unmatched := 0
	var differing := 0
	var flashing := 0
	var still := 0
	for tick in BATTLE_TICKS:
		world.tick()
		art.note_tick()
		art.refresh(whole, 1.0)
		var bodies := {}
		for band in _bands(art):
			var buffer := band.multimesh.buffer
			for i in band.multimesh.visible_instance_count:
				var base := i * WorldArt.BODY_FLOATS
				bodies[[Vector2(buffer[base + 3], buffer[base + 7]) + band.position,
						Vector2(buffer[base + 12], buffer[base + 13])]] = buffer[base + 14]
		var buffer := outlines.multimesh.buffer
		for i in outlines.multimesh.visible_instance_count:
			var base := i * WorldArt.BODY_FLOATS
			var key := [Vector2(buffer[base + 3], buffer[base + 7]), Vector2(buffer[base + 14], buffer[base + 15])]
			checked += 1
			if not bodies.has(key):
				unmatched += 1
			elif not is_equal_approx(buffer[base + 8], bodies[key]):
				differing += 1
			elif bodies[key] > 0.0:
				flashing += 1
			else:
				still += 1
		if flashing > 0 and still > 0:
			break
	art.free()
	var passed := checked > 0 and unmatched == 0 and differing == 0 and flashing > 0 and still > 0
	print("bodies_test: over the M1 battle %d outlines read, %d without their body in a band, %d carrying another share of white than their body's; %d of a flashing body and %d of one not: %s" % [
		checked, unmatched, differing, flashing, still, "pass" if passed else "FAIL"])
	return passed


# Whether the FRONT_CLEAR rows before `structure`, two cells either side of it, are open ground of
# the map and no other structure but a field stands in them.
static func _clear_before(world: WorldAdapter, structure: int) -> bool:
	var corner := world.structure_cell(structure)
	var size := world.structure_footprint(structure)
	var before := Rect2i(corner.x - 2, corner.y + size.y, size.x + 4, FRONT_CLEAR)
	var rows := world.map_rows()
	for y in range(before.position.y, before.end.y):
		for x in range(before.position.x, before.end.x):
			if y < 0 or y >= rows.size() or x < 0 or x >= rows[y].length() or rows[y][x] != ".":
				return false
	for other in world.structure_count():
		if other != structure and world.structure_field_state(other) < 0 \
				and before.intersects(Rect2i(world.structure_cell(other), world.structure_footprint(other))):
			return false
	return true


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
