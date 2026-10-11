# blood_cost.gd: headless, times world_art's refresh over the M1 battle, 600 ticks with blood on
# and 600 off on the same world alternately in blocks of 50 ticks, a frame per tick, so the
# machine's load falls on both alike. Prints the mean and 99th percentile refresh time of each.
extends SceneTree

const SCENE := "res://world_art/world_art.tscn"
const BLOCK := 50
const BLOCKS := 12

func _initialize() -> void:
	var world := WorldAdapter.create_m1()
	var art: Node2D = load(SCENE).instantiate()
	root.add_child(art)
	art.show_world(world, -1)
	var rows := world.map_rows()
	var whole := Rect2(Vector2.ZERO, Vector2(rows[0].length(), rows.size()) * Vector2(art.CELL))
	var view := Rect2(whole.get_center() - Vector2(320, 180), Vector2(640, 360))
	var times := {true: PackedFloat64Array(), false: PackedFloat64Array()}
	for b in BLOCKS:
		var blood := b % 2 == 0
		art.combat.blood = blood
		for t in BLOCK:
			world.tick()
			art.note_tick()
			var started := Time.get_ticks_usec()
			art.refresh(view, 1.0)
			times[blood].append((Time.get_ticks_usec() - started) / 1000.0)
	for blood: bool in [true, false]:
		var sorted: PackedFloat64Array = times[blood].duplicate()
		sorted.sort()
		var sum := 0.0
		for v in sorted:
			sum += v
		print("blood_cost: blood %s: mean %.3f ms, p99 %.3f ms over %d refreshes, %d drops fallen" % [
			blood, sum / sorted.size(), sorted[int(sorted.size() * 0.99)], sorted.size(), art.combat.drops_fallen])
	quit()
