# Checks that a world driven through the adapter reaches the core's own hash
# and body positions and counts for the same scenario, focus and tick count,
# the core's coming from the world_hash reference program, and that the
# adapter refuses what the core, or the conversion to the core's types,
# refuses.
extends RefCounted

const SEED := 42
const OTHER_SEED := 43
const TICKS := 2000
const REFERENCE := "res://build/extension/world_hash"
# Sub-cell units: the focus among the armies, and one beyond every tier's reach but the far one's.
const FOCUS := Vector2i(4 * 256, 4 * 256)
const FAR_FOCUS := Vector2i(1000000, 1000000)
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
const STRIKE := {"range": 512, "radius": 256, "body_damage": 1, "structure_damage": 4, "cooldown": 24}
const BLAST := {"range": 768, "radius": 512, "body_damage": 2, "structure_damage": 9, "cooldown": 64}
const ARMIES: Array[Dictionary] = [
	{"start": Vector2i(1, 1), "objective": Vector2i(14, 7), "count": 12, "strength": 1200,
		"faction": 0, "skills": [STRIKE, BLAST]},
	{"start": Vector2i(14, 7), "objective": Vector2i(1, 1), "count": 10, "strength": 1010,
		"faction": 1, "skills": [STRIKE]},
]
const STRUCTURES: Array[Dictionary] = [
	{"cell": Vector2i(8, 8), "durability": 100},
	{"cell": Vector2i(7, 8), "durability": 60},
]
const RULES := {"separation": 72, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}


func run() -> bool:
	var core := _core_run(FOCUS)
	var adapter := _adapter_run(SEED, FOCUS)
	# Control: another seed must differ, or equality could come from a constant hash.
	var other_seed := _adapter_run(OTHER_SEED, FOCUS)
	var hashes_match := core.size() > 0 and adapter[0] == core[0] and other_seed[0] != core[0]
	print("world_adapter_test: %d ticks, %d armies, %d structures; core hash %s, adapter hash %s for seed %d, %s for seed %d: %s" % [
		TICKS, ARMIES.size(), STRUCTURES.size(), core[0] if core.size() > 0 else "none", adapter[0], SEED,
		other_seed[0], OTHER_SEED, "pass" if hashes_match else "FAIL"])

	# Setup: the focus expanded the armies into bodies, so the comparison covers agents, not
	# only aggregates. Control: from a far focus the bodies differ, so equality is not a constant.
	var far := _adapter_run(SEED, FAR_FOCUS)
	var bodies := adapter.slice(1)
	var expanded := bodies.size() > ARMIES.size()
	var bodies_match := expanded and core.slice(1) == bodies and far.slice(1) != core.slice(1)
	print("world_adapter_test: %d bodies through the adapter, %d from the core, %d from a far focus; positions and counts equal: %s" % [
		bodies.size(), core.size() - 1, far.size() - 1, "pass" if bodies_match else "FAIL"])

	var strike_without_cooldown := STRIKE.duplicate()
	strike_without_cooldown.erase("cooldown")
	# Refused by the core, then by the conversion to the core's types.
	var invalid: Array[Array] = [
		[_with_first_army("start", Vector2i(8, 3)), RULES],
		[_with_first_army("skills", []), RULES],
		[_with_first_army("count", -1), RULES],
		[_with_first_army("count", 4294967297), RULES],
		[_with_first_army("strength", -1), RULES],
		[_with_first_army("skills", [strike_without_cooldown]), RULES],
		[ARMIES, _with_rules("stage_floors", [75, 25])],
	]
	var refused := 0
	for scenario in invalid:
		if _create(SEED, scenario[0], scenario[1]) == null:
			refused += 1
	# Control: the valid scenario is accepted, so a null above is the refusal under test.
	var accepted := _create(SEED, ARMIES, RULES) != null
	print("world_adapter_test: valid scenario accepted %s; %d of %d invalid scenarios refused: %s" % [
		accepted, refused, invalid.size(), "pass" if accepted and refused == invalid.size() else "FAIL"])
	return hashes_match and bodies_match and accepted and refused == invalid.size()


func _create(seed: int, armies: Array[Dictionary], rules: Dictionary) -> WorldAdapter:
	return WorldAdapter.create(seed, PackedStringArray(MAP_ROWS), armies, STRUCTURES, rules)


func _with_first_army(key: String, value: Variant) -> Array[Dictionary]:
	var armies := ARMIES.duplicate(true)
	armies[0][key] = value
	return armies


func _with_rules(key: String, value: Variant) -> Dictionary:
	var rules := RULES.duplicate(true)
	rules[key] = value
	return rules


# The run's hash, then one line `army x y count` per body, as world_hash prints them.
func _adapter_run(seed: int, focus: Vector2i) -> PackedStringArray:
	var world := _create(seed, ARMIES, RULES)
	world.set_focus(focus)
	for i in TICKS:
		world.tick()
	var lines := PackedStringArray([str(world.hash())])
	for army in world.army_count():
		var counts := world.body_counts(army)
		var positions := world.body_positions(army)
		for body in counts.size():
			lines.append("%d %d %d %d" % [army, int(positions[body].x), int(positions[body].y), counts[body]])
	return lines


# world_hash's output for the scenario, its fields in the core's order, each list after its length, then the focus.
func _core_run(focus: Vector2i) -> PackedStringArray:
	var fields: Array = [SEED, TICKS, MAP_ROWS.size()] + MAP_ROWS + [ARMIES.size()]
	for army in ARMIES:
		fields += [army.start.x, army.start.y, army.objective.x, army.objective.y, army.count,
			army.strength, army.faction, army.skills.size()]
		for skill in army.skills:
			fields += [skill.range, skill.radius, skill.body_damage, skill.structure_damage, skill.cooldown]
	fields.append(STRUCTURES.size())
	for structure in STRUCTURES:
		fields += [structure.cell.x, structure.cell.y, structure.durability]
	fields += [RULES.separation, RULES.hostile.size()]
	for row in RULES.hostile:
		fields.append(row.size())
		for hostile in row:
			fields.append(1 if hostile else 0)
	fields += RULES.stage_floors + [focus.x, focus.y]
	var output := []
	if OS.execute(ProjectSettings.globalize_path(REFERENCE), [" ".join(fields.map(str))], output) != 0:
		return PackedStringArray()
	return String(output[0]).strip_edges().split("\n")
