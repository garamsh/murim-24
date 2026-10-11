# Checks that a cast passes through the adapter as the core takes it: a tap's aim favours the
# facing and reaches no farther than the skill, a cast at a person lands on it, one at a point
# past the range lands at the range, and one at itself centres on the caster. Control: a cast on
# cooldown, out of range or at a target of another type is refused, and the nearer people a cast
# passes over are untouched. tick_count gives the tick the caster's readiness is counted against.
extends RefCounted

const CASTER := 0
const NEAR_FOE := 1
const FAR_FOE := 2
const BEHIND_FOE := 3
const HEALTH := 1000
const IDLE := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 0}
const BOLT := {"range": 1024, "radius": 0, "body_damage": 10, "structure_damage": 0, "cooldown": 30}
const SHORT_BOLT := {"range": 768, "radius": 0, "body_damage": 10, "structure_damage": 0, "cooldown": 30}
const NOVA := {"range": 0, "radius": 384, "body_damage": 5, "structure_damage": 0, "cooldown": 30}
const RULES := {"separation": 0, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}


func run() -> bool:
	var aimed := _world()
	var ahead := aimed.aim(CASTER, 1, Vector2(1, 0))
	var behind := aimed.aim(CASTER, 1, Vector2(-1, 0))
	# The aim reaches no farther than the skill: the idle skill, of no range, aims at no one, though
	# every foe stands within the bolt's.
	var idle := aimed.aim(CASTER, 0, Vector2(1, 0))
	var skill: Dictionary = aimed.skills(CASTER)[1]
	var aim_right: bool = ahead == NEAR_FOE and behind == BEHIND_FOE and idle == -1
	print("cast_test: the bolt aims east at %d, west at %d; the idle skill east at %d: %s" % [
		ahead, behind, idle, "pass" if aim_right else "FAIL"])

	var at_person := _world()
	var ticks_before := at_person.tick_count()
	var may := at_person.cast_refusal(CASTER, 1, FAR_FOE)
	var out_of_range := at_person.cast_refusal(CASTER, 2, FAR_FOE)
	var wrong_type := not at_person.cast(CASTER, 1, "east")
	var cast := at_person.cast(CASTER, 1, FAR_FOE)
	at_person.tick()
	var counted: bool = ticks_before == 0 and at_person.tick_count() == 1 \
			and at_person.body_ready(CASTER)[0] == at_person.tick_count() + skill["cooldown"]
	print("cast_test: tick_count %d before the cast's tick and %d after, the caster ready at %d: %s" % [
		ticks_before, at_person.tick_count(), at_person.body_ready(CASTER)[0], "pass" if counted else "FAIL"])
	var cooling := at_person.cast_refusal(CASTER, 1, FAR_FOE)
	var cooling_refused := not at_person.cast(CASTER, 1, FAR_FOE)
	var person_right: bool = may == WorldAdapter.CAST_NONE and out_of_range == WorldAdapter.CAST_OUT_OF_RANGE \
			and wrong_type and cast and at_person.person_health(FAR_FOE) == HEALTH - 10 \
			and at_person.person_health(NEAR_FOE) == HEALTH and cooling == WorldAdapter.CAST_ON_COOLDOWN \
			and cooling_refused
	print("cast_test: at a person %s, the far foe at %d and the near at %d; out of range %d, on cooldown %d: %s" % [
		cast, at_person.person_health(FAR_FOE), at_person.person_health(NEAR_FOE), out_of_range,
		cooling, "pass" if person_right else "FAIL"])

	var at_point := _world()
	var point_cast := at_point.cast(CASTER, 1, Vector2(15 * 256 + 128, 8 * 256 + 128))
	at_point.tick()
	var at_self := _world()
	var self_cast := at_self.cast(CASTER, 3, null)
	at_self.tick()
	var area_right: bool = point_cast and at_point.person_health(FAR_FOE) == HEALTH - 10 \
			and at_point.person_health(NEAR_FOE) == HEALTH and self_cast \
			and at_self.person_health(BEHIND_FOE) == HEALTH - 5 and at_self.person_health(CASTER) == HEALTH \
			and at_self.person_health(NEAR_FOE) == HEALTH
	print("cast_test: past the range the far foe at %d; at itself the one behind at %d, the caster at %d: %s" % [
		at_point.person_health(FAR_FOE), at_self.person_health(BEHIND_FOE),
		at_self.person_health(CASTER), "pass" if area_right else "FAIL"])
	return aim_right and counted and person_right and area_right


# A caster at (6, 8) striking with IDLE alone, and three foes: two cells east, four cells east and
# one cell west.
func _world() -> WorldAdapter:
	var rows := PackedStringArray()
	for y in 16:
		rows.append(".".repeat(16))
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(6, 8), "members": [{"cell": Vector2i(6, 8), "health": HEALTH}], "faction": 0,
			"skills": [IDLE, BOLT, SHORT_BOLT, NOVA]},
	]
	for cell in [Vector2i(8, 8), Vector2i(10, 8), Vector2i(5, 8)]:
		armies.append({"objective": cell, "members": [{"cell": cell, "health": HEALTH}], "faction": 1, "skills": [IDLE]})
	var world := WorldAdapter.create(1, rows, armies, [], RULES)
	world.order(CASTER, Vector2i(6, 8), PackedInt64Array([0]))
	return world
