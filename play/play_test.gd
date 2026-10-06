# Checks that play.tscn turns input into orders for the person it steers: a left click on a passable
# cell moves the person toward it, and a click on a blocked cell is refused; key 1 orders the
# person's first skill, and key 9, a skill it lacks, is refused.
extends RefCounted

const SCENE := "res://play/play.tscn"
const TICKS := 600
# How far from the person's home, in cells, to look for the cells to click.
const SEARCH := 40


func run() -> bool:
	var scene: Node2D = load(SCENE).instantiate()
	# The runner has no running tree to add the scene to, so it is readied directly.
	scene.notification(Node.NOTIFICATION_READY)
	var world: WorldAdapter = scene.world
	var home_cell := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var target := _cell_near(scene.world.map_rows(), home_cell, ".")
	var wall := _cell_near(scene.world.map_rows(), home_cell, "#")
	# Setup: both cells were found, so a refusal below is the wall's, not a missing cell.
	var aimed := target != home_cell and wall != home_cell
	var target_click := _click(target, scene.PIXELS_PER_CELL)
	var wall_click := _click(wall, scene.PIXELS_PER_CELL)
	var wall_refused: bool = not scene.command(wall_click)
	var target_accepted: bool = scene.command(target_click)
	var before := _distance(scene.lead(), target)
	for i in TICKS:
		scene.step()
	var after := _distance(scene.lead(), target)
	var moved := after < before
	var key_one: bool = scene.command(_key(KEY_1))
	var key_nine_refused: bool = not scene.command(_key(KEY_9))
	var passed := aimed and wall_refused and target_accepted and moved and key_one \
			and key_nine_refused and world.skill_count(scene.steered) < 9
	print("play_test: cells found %s; blocked cell refused %s; cell %s accepted %s, distance %.0f to %.0f; key 1 accepted %s, key 9 refused %s: %s" % [
		aimed, wall_refused, target, target_accepted, before, after, key_one, key_nine_refused,
		"pass" if passed else "FAIL"])
	scene.free()
	return passed


# The nearest cell to `home` within SEARCH cells, at least two away, whose tile is `tile`.
func _cell_near(rows: PackedStringArray, home: Vector2i, tile: String) -> Vector2i:
	for reach in range(2, SEARCH + 1):
		for y in range(home.y - reach, home.y + reach + 1):
			for x in range(home.x - reach, home.x + reach + 1):
				if y >= 0 and y < rows.size() and x >= 0 and x < rows[y].length() and rows[y][x] == tile:
					return Vector2i(x, y)
	return home


# A left click at the centre of `cell`, in the scene's pixels of `pixels_per_cell` to a cell.
func _click(cell: Vector2i, pixels_per_cell: float) -> InputEventMouseButton:
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	click.position = (Vector2(cell) + Vector2.ONE * 0.5) * pixels_per_cell
	return click


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _distance(position: Vector2, cell: Vector2i) -> float:
	return position.distance_to((Vector2(cell) + Vector2.ONE * 0.5) * WorldAdapter.CELL_SIZE)
