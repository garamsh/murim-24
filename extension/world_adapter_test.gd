# Checks that a world driven through the adapter reaches the core's own hash
# for the same scenario and tick count, the core's hash coming from the
# world_hash reference program, and that the adapter refuses what the core,
# or the conversion to the core's types, refuses.
extends RefCounted

const SEED := 42
const OTHER_SEED := 43
const TICKS := 2000
const REFERENCE := "res://build/extension/world_hash"
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
	var core_hash: Variant = _core_hash()
	var adapter_hash := _adapter_hash(SEED)
	# Control: another seed must differ, or equality could come from a constant hash.
	var other_seed_hash := _adapter_hash(OTHER_SEED)
	var matches: bool = core_hash != null and adapter_hash == core_hash and other_seed_hash != core_hash
	print("world_adapter_test: %d ticks, %d armies, %d structures; core hash %s, adapter hash %d for seed %d, %d for seed %d: %s" % [
		TICKS, ARMIES.size(), STRUCTURES.size(), core_hash, adapter_hash, SEED, other_seed_hash,
		OTHER_SEED, "pass" if matches else "FAIL"])

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
	return matches and accepted and refused == invalid.size()


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


func _adapter_hash(seed: int) -> int:
	var world := _create(seed, ARMIES, RULES)
	for i in TICKS:
		world.tick()
	return world.hash()


# The scenario as world_hash reads it: fields in the core's order, each list after its length.
func _core_hash() -> Variant:
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
	fields += RULES.stage_floors
	var output := []
	if OS.execute(ProjectSettings.globalize_path(REFERENCE), [" ".join(fields.map(str))], output) != 0:
		return null
	return String(output[0]).strip_edges().to_int()
