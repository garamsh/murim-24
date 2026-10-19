extends SceneTree
# Shows seed 2026's region in the world art scene and, once every area is baked, prints the bake's
# time summed over the areas, and the villages near seed 2026's built cells on a slope.
var art: Node2D
var world: WorldAdapter
var frames := 0
func _initialize() -> void:
	world = WorldAdapter.create_region(2026, 0)
	art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(art)
	art.show_world(world, world.army_count() - 1)
	for v in world.villages().size():
		var vill: Dictionary = world.villages()[v]
		for cell in [Vector2i(19, 8), Vector2i(72, 104)]:
			if Rect2i(vill.corner, vill.area).grow(6).has_point(cell):
				print("baketime: village %d %s at %s holds %s" % [v, vill.kind, vill.corner, cell])
func _process(_d: float) -> bool:
	frames += 1
	art.refresh(Rect2(Vector2.ZERO, Vector2(480, 270)))
	var ground: Node2D = art.get_node("Ground")
	if ground.baked or frames > 3000:
		var total := 0
		for u in ground.bake_usec:
			total += u
		print("baketime: baked %s after %d frames, %d areas, %.1f ms in all" % [ground.baked, frames, ground.bake_usec.size(), total / 1000.0])
		quit()
	return false
