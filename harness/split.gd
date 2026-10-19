extends SceneTree
# Splits seed 2026's built cells not drawn straight into those with a slope within FLAT_REACH and
# those with only other levels (drops) round them.
func _init() -> void:
	var world := WorldAdapter.create_region(2026, 0)
	var art: Node2D = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(art)
	art.show_world(world, world.army_count() - 1)
	var r: Relief = art.relief
	var sloped := []
	var dropped := []
	for y in r.size.y:
		for x in r.size.x:
			var i := y * r.size.x + x
			if not r.built_at(Vector2i(x, y)) or r._straight(i):
				continue
			var any_slope := false
			for dy in range(-Relief.FLAT_REACH.y, Relief.FLAT_REACH.y + 1):
				for dx in range(-Relief.FLAT_REACH.x, Relief.FLAT_REACH.x + 1):
					any_slope = any_slope or r.slope_at(Vector2i(x + dx, y + dy))
			(sloped if any_slope else dropped).append(Vector2i(x, y))
	print("split: %d by a slope %s; %d by a drop only %s" % [sloped.size(), sloped, dropped.size(), dropped])
	quit()
