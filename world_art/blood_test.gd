# Checks the blood and the wounds world_art.tscn draws from the core's record of a fight: on the
# tick a steered person's sweep strikes a foe beside it, a spray of blood flies from the foe and
# drops fall on the ground beyond it, where the same foe out of reach, struck by nothing, bleeds
# nothing; with the settings' blood off, the same hit draws no blood and still flashes; a foe left
# below half its health stands in its hurt frame and one below a quarter in its maimed frame, where
# one left above half stands as it was; and a maimed foe drips as it stands, where a hurt one does
# not.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const Combat := preload("res://world_art/combat.gd")
const WorldArt := preload("res://world_art/world_art.gd")
const MAP_ROWS: Array[String] = [
	"................",
	"................",
	"................",
	"................",
	"................",
]
# The foe's one skill reaches no one, so only the steered person ever strikes.
const NO_REACH := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}
const HEALTH := 100
const STRIKER_CELL := Vector2i(2, 2)
const BESIDE := Vector2i(3, 2)
const OUT_OF_REACH := Vector2i(12, 2)
const RULES := {"separation": 256, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}
const TICKS := 90


func run() -> bool:
	return _blood_pass() and _off_pass() and _wound_pass() and _drip_pass()


# On the hit tick the foe bleeds: one spray from its middle and three drops beyond its feet in the
# blow's direction, away from the striker. Control: the foe out of reach is struck by nothing and
# bleeds nothing.
func _blood_pass() -> bool:
	var near := _fight(BESIDE, 60, true)
	var far := _fight(OUT_OF_REACH, 60, true)
	var now := Time.get_ticks_msec()
	var combat: Combat = near.art.combat
	var sprays := combat.shown_sprays(now)
	var drops := combat.shown_drops(now)
	var foe_at: Vector2 = near.art.body_at(1, 0, 1.0)
	var beyond := drops.all(func(drop: Dictionary) -> bool: return drop.at.x > foe_at.x - 3.0)
	var far_combat: Combat = far.art.combat
	var passed: bool = near.tick >= 0 and sprays.size() == 1 and drops.size() == 3 and beyond \
			and (sprays[0].dir as Vector2).x > 0.0 and far.tick < 0 \
			and far_combat.shown_sprays(now).is_empty() and far_combat.shown_drops(now).is_empty()
	print("blood_test: on the hit tick %d, %d spray(s) toward %s and %d drop(s), beyond the foe's feet %s; out of reach, a use by tick %d, %d spray(s) and %d drop(s): %s" % [
		near.tick, sprays.size(), sprays[0].dir if not sprays.is_empty() else Vector2.ZERO, drops.size(), beyond,
		far.tick, far_combat.shown_sprays(now).size(), far_combat.shown_drops(now).size(), "pass" if passed else "FAIL"])
	near.art.free()
	far.art.free()
	return passed


# With the settings' blood off the same hit draws no spray and no drop. Control: it still flashes.
func _off_pass() -> bool:
	var off := _fight(BESIDE, 60, false)
	var now := Time.get_ticks_msec()
	var combat: Combat = off.art.combat
	var passed: bool = off.tick >= 0 and not combat.blood and combat.shown_sprays(now).is_empty() \
			and combat.shown_drops(now).is_empty() and combat.flash(off.victim, now) > 0.5
	print("blood_test: with blood off, the hit on tick %d drew %d spray(s) and %d drop(s), the foe flashing %.2f: %s" % [
		off.tick, combat.shown_sprays(now).size(), combat.shown_drops(now).size(), combat.flash(off.victim, now),
		"pass" if passed else "FAIL"])
	off.art.free()
	return passed


# A foe a sweep leaves at 40 of 100 stands in idle_hurt, one left at 20 in idle_maimed. Control: one
# left at 70 stands in idle. Setup: each is left at the health asked for.
func _wound_pass() -> bool:
	var faults := []
	for case: Array in [[30, "idle"], [60, "idle_hurt"], [80, "idle_maimed"]]:
		var fight := _fight(BESIDE, case[0], true)
		fight.art.refresh(_view())
		var health: int = fight.world.person_health(fight.victim)
		var tag := _tag_at(fight.art, fight.art.body_at(1, 0, 1.0))
		if health != HEALTH - case[0] or tag != case[1]:
			faults.append("left at %d drawn as %s" % [health, tag])
		fight.art.free()
	var passed := faults.is_empty()
	print("blood_test: foes left at 70, 40 and 20 of 100 drawn idle, idle_hurt and idle_maimed, faults %s: %s" % [
		faults, "pass" if passed else "FAIL"])
	return passed


# Over the Combat.DRIP_TICKS ticks after the hit, a maimed foe drips at least once, a drop falling
# after its hit's at its feet. Control: a foe left hurt, not maimed, drips none.
func _drip_pass() -> bool:
	var counts := []
	for damage: int in [80, 60]:
		var fight := _fight(BESIDE, damage, true)
		var combat: Combat = fight.art.combat
		var bled := combat.drops.size()
		for i in Combat.DRIP_TICKS:
			fight.world.tick()
			fight.art.note_tick()
			fight.art.refresh(_view())
		counts.append(combat.drops.size() - bled)
		fight.art.free()
	var passed: bool = counts[0] > 0 and counts[1] == 0
	print("blood_test: over %d ticks a maimed foe dripped %d drop(s), a hurt one %d: %s" % [
		Combat.DRIP_TICKS, counts[0], counts[1], "pass" if passed else "FAIL"])
	return passed


# The steered person, with a sweep of `damage` it uses once, and a foe of HEALTH at `foe_cell`,
# drawn by world_art with blood `blood`, ticked until the first use the core records, at most TICKS.
func _fight(foe_cell: Vector2i, damage: int, blood: bool) -> Dictionary:
	var sweep := {"range": 384, "radius": 0, "body_damage": damage, "structure_damage": 0, "cooldown": 100000,
		"shape": WorldAdapter.SHAPE_ARC}
	var armies: Array[Dictionary] = [
		{"objective": STRIKER_CELL, "faction": 0, "skills": [sweep], "members": [{"cell": STRIKER_CELL, "health": HEALTH}]},
		{"objective": foe_cell, "faction": 1, "skills": [NO_REACH], "members": [{"cell": foe_cell, "health": HEALTH}]},
	]
	var world := WorldAdapter.create(7, PackedStringArray(MAP_ROWS), armies, [], RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, 0)
	art.combat.blood = blood
	var first := -1
	for i in TICKS:
		world.tick()
		art.note_tick()
		art.refresh(_view())
		if not world.strikes().is_empty():
			first = i
			break
	return {"world": world, "art": art, "victim": world.roster(1)[0], "tick": first}


# The tag of the frame a band draws the body standing at `at` with, within the knock a hit gives
# it, or "" for none.
func _tag_at(art: Node2D, at: Vector2) -> String:
	var centre := Vector2(at.x, at.y - WorldArt.FEET + WorldArt.BODY_FRAME.y / 2.0).round()
	var sheet: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://art/bodies/orthodox.json"))
	var faction_width: int = int(sheet.frames[0].frame.w) * sheet.frames.size()
	var atlas_width := faction_width * 3 + WorldArt.BODY_FRAME.x
	for child in art.get_node("Standing").get_children():
		var band := child as MultiMeshInstance2D
		if band == null:
			continue
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


func _view() -> Rect2:
	return Rect2(Vector2.ZERO, Vector2(MAP_ROWS[0].length(), MAP_ROWS.size()) * WorldArt.CELL)
