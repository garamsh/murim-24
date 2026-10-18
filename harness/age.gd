extends SceneTree
# Args: <village> <years> <save dir>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var world := WorldAdapter.create_region(2026, int(a[0]))
	var start := Time.get_ticks_msec()
	var years := int(a[1])
	var y0: int = world.date().year
	while world.date().year < y0 + years:
		world.tick()
	var living := 0
	for army in world.army_count():
		living += world.roster(army).size()
	var occ := {}
	for army in world.army_count():
		for p in world.roster(army):
			var o := world.person_occupation(p)
			occ[o] = occ.get(o, 0) + 1
	print("aged %d years in %.1f s to tick %d; %d living; by occupation %s; player army %d holds %d; saved %s" % [years,
			(Time.get_ticks_msec() - start) / 1000.0, world.tick_count(), living, occ, world.player_army(),
			world.roster(world.player_army()).size() if world.player_army() >= 0 else -1, world.save(0, a[2])])
	var near := {}
	var vs := world.villages()
	for army in world.army_count():
		for p in world.roster(army):
			var at: Vector2 = world.person_position(p) / WorldAdapter.CELL_SIZE
			for i in vs.size():
				var v: Dictionary = vs[i]
				if Rect2(Vector2(v.corner), Vector2(v.area)).grow(4).has_point(at):
					near[i] = near.get(i, 0) + 1
	print("people within 4 cells of each village ", near)
	quit()
