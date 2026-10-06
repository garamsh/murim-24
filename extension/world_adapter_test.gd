# Checks that a world driven through the adapter reaches the core's own hash
# for the same scenario and tick count, the core's hash coming from the
# world_hash reference program, and that the adapter refuses what the core
# refuses.
extends RefCounted

const SEED := 42
const OTHER_SEED := 43
const TICKS := 2000
const SEPARATION := 72
const REFERENCE := "res://build/extension/world_hash"
const MAP_ROWS: Array[String] = [
	"................",
	"................",
	"................",
	"......####......",
	"......####......",
	"................",
	"................",
	"................",
]
const ARMIES: Array[Dictionary] = [
	{"start": Vector2i(1, 1), "objective": Vector2i(14, 6), "count": 24, "strength": 240},
	{"start": Vector2i(14, 1), "objective": Vector2i(1, 6), "count": 7, "strength": 77},
]


func run() -> bool:
	var core_hash: Variant = _core_hash()
	var adapter_hash := _adapter_hash(SEED)
	# Control: another seed must differ, or equality could come from a constant hash.
	var other_seed_hash := _adapter_hash(OTHER_SEED)
	var matches: bool = core_hash != null and adapter_hash == core_hash and other_seed_hash != core_hash
	print("world_adapter_test: %d ticks, %d armies; core hash %s, adapter hash %d for seed %d, %d for seed %d: %s" % [
		TICKS, ARMIES.size(), core_hash, adapter_hash, SEED, other_seed_hash, OTHER_SEED,
		"pass" if matches else "FAIL"])

	# Refused by the core, and by the conversion to the core's integer types.
	var invalid: Array[Array] = [
		_with_first_army("start", Vector2i(7, 3)),
		_with_first_army("count", -1),
		_with_first_army("count", 4294967297),
		_with_first_army("strength", -1),
	]
	var refused := 0
	for armies in invalid:
		if _create(SEED, armies) == null:
			refused += 1
	# Control: the valid scenario is accepted, so a null above is the refusal under test.
	var accepted := _create(SEED, ARMIES) != null
	print("world_adapter_test: valid scenario accepted %s; %d of %d invalid scenarios refused: %s" % [
		accepted, refused, invalid.size(), "pass" if accepted and refused == invalid.size() else "FAIL"])
	return matches and accepted and refused == invalid.size()


func _create(seed: int, armies: Array[Dictionary]) -> WorldAdapter:
	return WorldAdapter.create(seed, PackedStringArray(MAP_ROWS), armies, SEPARATION)


func _with_first_army(key: String, value: Variant) -> Array[Dictionary]:
	var armies := ARMIES.duplicate(true)
	armies[0][key] = value
	return armies


func _adapter_hash(seed: int) -> int:
	var world := _create(seed, ARMIES)
	for i in TICKS:
		world.tick()
	return world.hash()


func _core_hash() -> Variant:
	var armies := PackedStringArray()
	for army in ARMIES:
		armies.append("%d,%d,%d,%d,%d,%d" % [army.start.x, army.start.y, army.objective.x,
			army.objective.y, army.count, army.strength])
	var args := [str(SEED), str(TICKS), str(SEPARATION), ";".join(armies)] + MAP_ROWS
	var output := []
	if OS.execute(ProjectSettings.globalize_path(REFERENCE), args, output) != 0:
		return null
	return String(output[0]).strip_edges().to_int()
