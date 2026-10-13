# Checks the blood and the wounds world_art.tscn draws from the core's record of a fight: on the
# tick a steered person's sweep strikes a foe beside it, a spray of blood flies from the foe and
# drops fall on the ground beyond it, where the same foe out of reach, struck by nothing, bleeds
# nothing; with the settings' blood off, the same hit draws no blood and still flashes; a foe left
# below half its health stands in its hurt frame and one below a quarter in its maimed frame, where
# one left above half stands as it was; a maimed foe drips as it stands, where a hurt one does
# not; and the steered person's hit holds the struck foe's drawn flash for its hit stop, where with
# the settings' hit stop off it fades, and a foe out of reach starts no hit stop; and the same hit
# shakes the screen by whole pixels, as far as its size, until it ends, where with the hit stop off
# or out of reach it does not. A hit stop lasts its frames whether or not its body is drawn, and
# holds no one dead; a world shown anew, as a load shows it, shows none of the last one's blood or
# hit stops; and the blood's rings keep their most, the newest in the oldest's place.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const Combat := preload("res://world_art/combat.gd")
const WorldArt := preload("res://world_art/world_art.gd")
const BloodPixels := preload("res://world_art/blood_pixels.gd")
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
# How far apart the two refreshes a hit stop must hold are, in milliseconds.
const HELD_MS := 60
# More view frames than the longest hit stop holds.
const HIT_STOP_FRAMES := 6
# A view far from the map, which draws none of it.
const ELSEWHERE := Rect2(Vector2(100000, 100000), Vector2(256, 80))


func run() -> bool:
	return _blood_pass() and _off_pass() and _cap_pass() and _wound_pass() and _drip_pass() \
			and _hit_stop_pass() and _shake_pass() and _held_off_screen_pass() and _held_dead_pass() \
			and _load_pass() and _ring_pass()


# On the hit tick the foe bleeds: one spray from its middle, and drops on the ground, two or more of
# them beyond its feet in the blow's direction, away from the striker, and two or more pooled just
# below them; given to the blood MultiMeshes as the spray's pixels and each drop's cluster. Control: the
# foe out of reach is struck by nothing and bleeds nothing.
func _blood_pass() -> bool:
	var near := _fight(BESIDE, 60, true)
	var far := _fight(OUT_OF_REACH, 60, true)
	var now := Time.get_ticks_msec()
	var combat: Combat = near.art.combat
	var sprays := combat.shown_sprays(now)
	var drops := combat.shown_drops(now)
	var foe_at: Vector2 = near.art.body_at(1, 0, 1.0)
	var beyond := drops.filter(func(drop: Dictionary) -> bool: return drop.at.x > foe_at.x + 3.0).size()
	var pooled := drops.filter(func(drop: Dictionary) -> bool:
		return absf(drop.at.x - foe_at.x) <= 4.0 and drop.at.y - foe_at.y >= 1.0 and drop.at.y - foe_at.y <= 4.0).size()
	var far_combat: Combat = far.art.combat
	# What the blood's MultiMeshes are given to draw: a pixel an instance.
	var drawn := [_pixels(near.art, "Sprays"), _pixels(near.art, "Drops")]
	var far_drawn := [_pixels(far.art, "Sprays"), _pixels(far.art, "Drops")]
	var dropped := Combat.HIT_DROPS + Combat.POOL_DROPS
	var passed: bool = near.tick >= 0 and sprays.size() == 1 and drops.size() == dropped and beyond >= 2 \
			and pooled >= 2 and (sprays[0].dir as Vector2).x > 0.0 \
			and drawn == [Combat.SPRAY_PIXELS, dropped * BloodPixels.DROP_PIXELS] and far.tick < 0 \
			and far_combat.shown_sprays(now).is_empty() and far_combat.shown_drops(now).is_empty() \
			and far_drawn == [0, 0]
	print("blood_test: on the hit tick %d, %d spray(s) toward %s and %d drop(s), %d beyond the foe's feet and %d pooled at them, given to draw as %d and %d pixels; out of reach, a use by tick %d, %d spray(s) and %d drop(s), %d and %d pixels: %s" % [
		near.tick, sprays.size(), sprays[0].dir if not sprays.is_empty() else Vector2.ZERO, drops.size(), beyond, pooled,
		drawn[0], drawn[1], far.tick, far_combat.shown_sprays(now).size(), far_combat.shown_drops(now).size(),
		far_drawn[0], far_drawn[1], "pass" if passed else "FAIL"])
	near.art.free()
	far.art.free()
	return passed


# A burst striking more than MOST_BLEEDS foes at once, none of them on or from the steered person,
# bleeds only MOST_BLEEDS of them. Control: the same burst made by the steered person bleeds all.
func _cap_pass() -> bool:
	var others := _burst(2)
	var own := _burst(0)
	var passed: bool = others.struck > Combat.MOST_BLEEDS and own.struck == others.struck \
			and others.sprays == Combat.MOST_BLEEDS and own.sprays == own.struck
	print("blood_test: a burst striking %d foes, by someone else, bled %d of them; by the steered person, striking %d, bled %d: %s" % [
		others.struck, others.sprays, own.struck, own.sprays, "pass" if passed else "FAIL"])
	return passed


# One tick of a burst at a block of nine foes, with `steered` the steered army: 0 the striker's, 2
# a bystander's far off. Returns how many were struck and how many sprays bled.
func _burst(steered: int) -> Dictionary:
	var burst := {"range": 768, "radius": 512, "body_damage": 10, "structure_damage": 0, "cooldown": 100000,
		"shape": WorldAdapter.SHAPE_AREA}
	var foes := []
	for y in 3:
		for x in 3:
			foes.append({"cell": Vector2i(4 + x, 1 + y), "health": HEALTH})
	var armies: Array[Dictionary] = [
		{"objective": STRIKER_CELL, "faction": 0, "skills": [burst], "members": [{"cell": STRIKER_CELL, "health": HEALTH}]},
		{"objective": Vector2i(5, 2), "faction": 1, "skills": [NO_REACH], "members": foes},
		{"objective": Vector2i(14, 4), "faction": 0, "skills": [NO_REACH], "members": [{"cell": Vector2i(14, 4), "health": HEALTH}]},
	]
	var world := WorldAdapter.create(7, PackedStringArray(MAP_ROWS), armies, [], RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, steered)
	# Drawn once before the first tick, as play draws a frame before it ticks, so combat knows the view.
	art.refresh(_view())
	var found := {"struck": 0, "sprays": 0}
	for i in TICKS:
		world.tick()
		art.note_tick()
		art.refresh(_view())
		if not world.strikes().is_empty():
			found.struck = world.strikes()[0].struck.size()
			found.sprays = art.combat.shown_sprays(Time.get_ticks_msec()).size()
			break
	art.free()
	return found


# The pixels the blood node `name` of `art` is given to draw.
func _pixels(art: WorldArt, name: String) -> int:
	var node: MultiMeshInstance2D = art.get_node(name)
	return 0 if node.multimesh == null else node.multimesh.visible_instance_count


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
		var bled := combat.drops_fallen
		for i in Combat.DRIP_TICKS:
			fight.world.tick()
			fight.art.note_tick()
			fight.art.refresh(_view())
		counts.append(combat.drops_fallen - bled)
		fight.art.free()
	var passed: bool = counts[0] > 0 and counts[1] == 0
	print("blood_test: over %d ticks a maimed foe dripped %d drop(s), a hurt one %d: %s" % [
		Combat.DRIP_TICKS, counts[0], counts[1], "pass" if passed else "FAIL"])
	return passed


# After the steered person's hit, the struck foe is drawn with the same share of white on two
# refreshes HELD_MS apart, held, and a hit stop was started for it. Control: with the hit stop off,
# the share fades between them; out of reach, no hit stop starts.
func _hit_stop_pass() -> bool:
	var whites := []
	var stops := []
	for stop: bool in [true, false]:
		var fight := _fight(BESIDE, 60, true, stop)
		var first := _white_at(fight.art, fight.art.body_at(1, 0, 1.0))
		OS.delay_msec(HELD_MS)
		fight.art.refresh(_view())
		whites.append([first, _white_at(fight.art, fight.art.body_at(1, 0, 1.0))])
		stops.append(fight.art.combat.hit_stops.size())
		fight.art.free()
	var far := _fight(OUT_OF_REACH, 60, true, true)
	var far_stops: int = far.art.combat.hit_stops.size()
	far.art.free()
	var passed: bool = whites[0][0] > 0.5 and is_equal_approx(whites[0][0], whites[0][1]) and stops[0] == 2 \
			and whites[1][1] < whites[1][0] and stops[1] == 0 and far_stops == 0
	print("blood_test: held, the struck foe's white %.2f then %.2f, %d hit stop(s); hit stop off %.2f then %.2f, %d; out of reach %d: %s" % [
		whites[0][0], whites[0][1], stops[0], whites[1][0], whites[1][1], stops[1], far_stops,
		"pass" if passed else "FAIL"])
	return passed


# Right after the steered person's hit of 60, the screen is shaken by a step of the pattern 3 whole
# pixels far, and by nothing once Combat.SHAKE_MS has passed. Control: with the hit stop off, and
# out of reach, no shake.
func _shake_pass() -> bool:
	var shakes := []
	for case: Array in [[BESIDE, true], [BESIDE, false], [OUT_OF_REACH, true]]:
		var fight := _fight(case[0], 60, true, case[1])
		var now := Time.get_ticks_msec()
		shakes.append([fight.art.combat.shake(now), fight.art.combat.shake(now + Combat.SHAKE_MS)])
		fight.art.free()
	var first: Vector2i = shakes[0][0]
	var stepped := Combat.SHAKE_PATTERN.any(func(step: Vector2i) -> bool: return step * 3 == first)
	var passed: bool = first != Vector2i.ZERO and stepped and shakes[0][1] == Vector2i.ZERO \
			and shakes[1][0] == Vector2i.ZERO and shakes[2][0] == Vector2i.ZERO
	print("blood_test: the hit of 60 shook the screen %s, a step of the pattern 3 pixels far %s, then %s after %d ms; hit stop off %s; out of reach %s: %s" % [
		first, stepped, shakes[0][1], Combat.SHAKE_MS, shakes[1][0], shakes[2][0], "pass" if passed else "FAIL"])
	return passed


# After the steered person's hit, the striker walks away while the view looks elsewhere for more
# frames than the longest hit stop; brought back into view it is held no more and drawn where it now
# stands, a tick later further on. Control: the same hit with the hit stop off draws it the same;
# and brought back at once, before its stop ends, it is still held.
func _held_off_screen_pass() -> bool:
	var cases := []
	for case: Array in [[true, HIT_STOP_FRAMES], [false, HIT_STOP_FRAMES], [true, 0]]:
		var fight := _fight(BESIDE, 60, true, case[0])
		var striker: int = fight.world.roster(0)[0]
		var started: bool = fight.art.combat.bodies.held(striker)
		fight.world.order(0, Vector2i(STRIKER_CELL.x, MAP_ROWS.size() - 1), PackedInt64Array([0]))
		for i in case[1]:
			fight.world.tick()
			fight.art.note_tick()
			fight.art.refresh(ELSEWHERE)
		fight.art.refresh(_view())
		var held: bool = fight.art.combat.bodies.held(striker)
		var at: Vector2 = fight.art.body_at(0, 0, 1.0)
		var drawn := _drawn_near(fight.art, at)
		cases.append({"started": started, "held": held, "drawn": drawn})
		fight.art.free()
	var passed: bool = cases[0].started and not cases[0].held and cases[0].drawn \
			and not cases[1].started and not cases[1].held and cases[1].drawn \
			and cases[2].started and cases[2].held
	print("blood_test: a hit stop on the striker %s, after %d frames off screen held %s and drawn where it stands %s; hit stop off %s, %s, %s; back at once held %s: %s" % [
		cases[0].started, HIT_STOP_FRAMES, cases[0].held, cases[0].drawn, cases[1].started, cases[1].held,
		cases[1].drawn, cases[2].held, "pass" if passed else "FAIL"])
	return passed


# The steered person's hit kills the foe: both are held on the tick, and after the next refresh the
# dead foe is held no more. Control: the living striker still is.
func _held_dead_pass() -> bool:
	var fight := _fight(BESIDE, HEALTH, true)
	var foe: int = fight.victim
	var striker: int = fight.world.roster(0)[0]
	var died: bool = fight.world.roster(1).is_empty()
	var stops: Array = fight.art.combat.hit_stops.map(func(stop: Dictionary) -> int: return stop.person)
	fight.art.refresh(_view())
	var foe_held: bool = fight.art.combat.bodies.held(foe)
	var striker_held: bool = fight.art.combat.bodies.held(striker)
	fight.art.free()
	var passed := died and stops.has(foe) and not foe_held and striker_held
	print("blood_test: the foe killed %s and stopped %s, held after the next refresh %s; the striker held %s: %s" % [
		died, stops.has(foe), foe_held, striker_held, "pass" if passed else "FAIL"])
	return passed


# A world shown anew mid-battle, as play.gd shows a loaded world on the same art, shows no blood, no
# hit stop and no held body, as the same world shown on a fresh art does. Setup: the battle left
# blood and hit stops to clear.
func _load_pass() -> bool:
	var fight := _fight(BESIDE, 60, true)
	var before := _leftovers(fight.art)
	var loaded := WorldAdapter.create(7, PackedStringArray(MAP_ROWS), _calm_armies(), [], RULES)
	fight.art.show_world(loaded, 0)
	var after := _leftovers(fight.art)
	var fresh: Node2D = load(SCENE).instantiate()
	fresh.show_world(WorldAdapter.create(7, PackedStringArray(MAP_ROWS), _calm_armies(), [], RULES), 0)
	var fresh_left := _leftovers(fresh)
	fight.art.free()
	fresh.free()
	var setup: bool = before.drops > 0 and before.pixels > 0 and before.stops > 0 and before.held > 0
	var passed: bool = setup and after == fresh_left and after.drops == 0 and after.pixels == 0
	print("blood_test: mid-battle %s, shown anew %s, a fresh art %s: %s" % [before, after, fresh_left,
		"pass" if passed else "FAIL"])
	return passed


# Writing one drop and one spray pixel past each ring's most keeps the most, the newest in the
# oldest's place, and given to draw so: the drops' first instance at the newest drop. Control: one
# under the most, nothing is overwritten.
func _ring_pass() -> bool:
	var results := []
	for past: int in [1, -1]:
		var combat := Combat.new()
		for n in Combat.MOST_DROPS + past:
			combat._drop(Vector2(n, 0), 0)
		for n in Combat.MOST_SPRAY_PIXELS + past:
			combat._throw(Vector2(n, 0), Vector2.ZERO, 0)
		var drops := BloodPixels.new()
		drops.rebuild(combat)
		var first_drawn := Vector2(drops.multimesh.buffer[3], drops.multimesh.buffer[7]) - Vector2(0.5, 0.5)
		results.append({"drops": [combat.drop_at.size(), combat.drops_fallen, combat.drop_at[0].x],
			"pixels": [combat.spray_pixel_at.size(), combat.spray_pixels_thrown, combat.spray_pixel_at[0].x],
			"drawn": [drops.multimesh.visible_instance_count, first_drawn.x]})
		drops.free()
		combat.free()
	var over: Dictionary = results[0]
	var under: Dictionary = results[1]
	var passed: bool = over.drops == [Combat.MOST_DROPS, Combat.MOST_DROPS + 1, float(Combat.MOST_DROPS)] \
			and over.pixels == [Combat.MOST_SPRAY_PIXELS, Combat.MOST_SPRAY_PIXELS + 1, float(Combat.MOST_SPRAY_PIXELS)] \
			and over.drawn == [Combat.MOST_DROPS * BloodPixels.DROP_PIXELS, float(Combat.MOST_DROPS)] \
			and under.drops == [Combat.MOST_DROPS - 1, Combat.MOST_DROPS - 1, 0.0] \
			and under.pixels == [Combat.MOST_SPRAY_PIXELS - 1, Combat.MOST_SPRAY_PIXELS - 1, 0.0] \
			and under.drawn == [(Combat.MOST_DROPS - 1) * BloodPixels.DROP_PIXELS, 0.0]
	print("blood_test: one past the rings' most %s, one under %s: %s" % [over, under, "pass" if passed else "FAIL"])
	return passed


# What of the last fight `art` still shows or holds: drops and spray pixels given to draw, hit stops
# and held bodies.
func _leftovers(art: Node2D) -> Dictionary:
	return {"drops": art.get_node("Drops").multimesh.visible_instance_count,
		"pixels": art.get_node("Sprays").multimesh.visible_instance_count,
		"stops": art.combat.hit_stops.size(), "held": art.combat.bodies.held_count()}


# Two armies standing apart with nothing that reaches, as a world loaded after a battle might be.
func _calm_armies() -> Array[Dictionary]:
	return [
		{"objective": STRIKER_CELL, "faction": 0, "skills": [NO_REACH], "members": [{"cell": STRIKER_CELL, "health": HEALTH}]},
		{"objective": OUT_OF_REACH, "faction": 1, "skills": [NO_REACH], "members": [{"cell": OUT_OF_REACH, "health": HEALTH}]},
	]


# Whether a band draws a body within a pixel of where one standing at `at`, its feet, is drawn.
func _drawn_near(art: Node2D, at: Vector2) -> bool:
	var centre := Vector2(at.x, at.y - WorldArt.FEET + WorldArt.BODY_FRAME.y / 2.0).round()
	for child in art.get_node("Standing").get_children():
		var band := child as MultiMeshInstance2D
		if band == null:
			continue
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			var base := i * WorldArt.BODY_FLOATS
			if buffer[base + 14] >= 0.0 and buffer[base + 15] == 1.0 \
					and (Vector2(buffer[base + 3], buffer[base + 7]) + band.position).distance_to(centre) <= 1.0:
				return true
	return false


# The share of white the band instance nearest `at`, the feet of a body, draws, or -1 for none.
func _white_at(art: Node2D, at: Vector2) -> float:
	var centre := Vector2(at.x, at.y - WorldArt.FEET + WorldArt.BODY_FRAME.y / 2.0).round()
	var best := INF
	var white := -1.0
	for child in art.get_node("Standing").get_children():
		var band := child as MultiMeshInstance2D
		if band == null:
			continue
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			var base := i * WorldArt.BODY_FLOATS
			var d := (Vector2(buffer[base + 3], buffer[base + 7]) + band.position).distance_to(centre)
			if d < best and d <= 4.0:
				best = d
				white = buffer[base + 14]
	return white


# The steered person, with a sweep of `damage` it uses once, and a foe of HEALTH at `foe_cell`,
# drawn by world_art with blood `blood` and hit stop `stop`, ticked until the first use the core
# records, at most TICKS.
func _fight(foe_cell: Vector2i, damage: int, blood: bool, stop := true) -> Dictionary:
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
	art.combat.hit_stop = stop
	var victim := world.roster(1)[0]
	var first := -1
	for i in TICKS:
		world.tick()
		art.note_tick()
		art.refresh(_view())
		if not world.strikes().is_empty():
			first = i
			break
	return {"world": world, "art": art, "victim": victim, "tick": first}


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
