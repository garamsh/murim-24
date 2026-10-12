# bake_time.gd: draws world_art.tscn for the region of seed 2026 in a window and, once the ground has
# baked, prints how long its areas took: their count, total, mean and slowest, from ground.gd's
# bake_usec. It quits after 2,000 frames whatever happens.
extends SceneTree
var _art: Node2D
var _frame := 0
func _initialize() -> void:
	root.size = Vector2i(1920, 1080)
	_art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(_art)
	_art.show_world(WorldAdapter.create_region(2026, 0), -1)
func _process(_d: float) -> bool:
	_frame += 1
	_art.refresh(Rect2(0, 0, 640, 360))
	var ground: Node = _art.get_node("Ground")
	if ground.baked or _frame > 2000:
		var times: PackedInt64Array = ground.bake_usec
		var total := 0
		var most := 0
		for t in times:
			total += t
			most = maxi(most, t)
		print("bake_time: baked %s, %d areas, total %.1f ms, mean %.2f ms, slowest %.2f ms" % [ground.baked, times.size(), total / 1000.0, total / 1000.0 / maxi(1, times.size()), most / 1000.0])
		return true
	return false
