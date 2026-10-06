# Checks that a world driven through the adapter reaches the core's own hash for the same seed and
# tick count, the core's hash coming from the world_hash reference program.
extends RefCounted

const SEED := 42
const OTHER_SEED := 43
const TICKS := 10000
const REFERENCE := "res://build/extension/world_hash"


func run() -> bool:
	var output := []
	var code := OS.execute(ProjectSettings.globalize_path(REFERENCE), [str(SEED), str(TICKS)], output)
	var core_hash := String(output[0]).strip_edges().to_int() if code == 0 else 0
	var adapter_hash := _adapter_hash(SEED)
	# Control: another seed must differ, or equality could come from a constant hash.
	var other_seed_hash := _adapter_hash(OTHER_SEED)
	var passed := code == 0 and adapter_hash == core_hash and other_seed_hash != core_hash
	print("world_adapter_test: %d ticks; core hash %d (reference exit %d); adapter hash %d for seed %d, %d for seed %d: %s" % [
		TICKS, core_hash, code, adapter_hash, SEED, other_seed_hash, OTHER_SEED, "pass" if passed else "FAIL"])
	return passed


func _adapter_hash(seed: int) -> int:
	var world := WorldAdapter.create(seed)
	for i in TICKS:
		world.tick()
	return world.hash()
