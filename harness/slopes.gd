extends SceneTree
# Counts the built cells of seed 2026's region drawn straight and those not (on or beside a slope or
# a drop), by what is built there, and lists the village each unstraight one lies in.
func _init() -> void:
	var world := WorldAdapter.create_region(2026, 0)
	var art: Node2D = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(art)
	art.show_world(world, world.army_count() - 1)
	var relief: Relief = art.relief
	var data := relief.cell_image() if relief.has_method("cell_image") else null
	var straight := 0
	var bent := 0
	var by_village := {}
	for y in relief.size.y:
		for x in relief.size.x:
			var at := Vector2i(x, y)
			if not relief.built_at(at):
				continue
			if relief._straight(y * relief.size.x + x):
				straight += 1
			else:
				bent += 1
				for v in world.villages().size():
					var vill: Dictionary = world.villages()[v]
					if Rect2i(vill.corner, vill.area).grow(4).has_point(at):
						by_village[v] = by_village.get(v, []) + [at]
						break
	print("slopes: %d built cells straight, %d not" % [straight, bent])
	for v in by_village:
		var vill: Dictionary = world.villages()[v]
		print("slopes: village %d %s at %s area %s: %d unstraight, first %s" % [v, vill.kind, vill.corner, vill.area, by_village[v].size(), by_village[v].slice(0, 6)])
	quit()
