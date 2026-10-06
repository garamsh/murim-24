# Checks that play.tscn turns input into orders for the person it steers: a left click on a passable
# cell moves the person toward it, and a click on a blocked cell is refused; key 1 orders the
# person's first skill, and key 9, a skill it lacks, is refused. Key C challenges the person's
# faction's leader: born into an Orthodox village, the core refuses it for having no duel and the
# HUD says so; born into a Cult village, it starts a duel the HUD shows, and a second C while it is
# fought is refused for that.
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
	return passed and _challenge_pass() and _genealogy_pass()


# G opens the genealogy panel on the steered person and takes the keys while it is open, so a
# digit gives no order; G again closes it. Control: with it closed, the same digit orders.
func _genealogy_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var panel: Control = scene.get_node("Hud").genealogy
	var opened: bool = scene.command(_key(KEY_G)) and panel.visible \
			and panel.centre == world.roster(scene.steered)[0]
	var digit_taken: bool = not scene.command(_key(KEY_1))
	var closed: bool = scene.command(_key(KEY_G)) and not panel.visible
	var digit_orders: bool = scene.command(_key(KEY_1))
	var passed := opened and digit_taken and closed and digit_orders
	print("play_test: G opens the genealogy on the steered person %s, a digit gives no order while open %s, G closes it %s, then the digit orders %s: %s" % [
		opened, digit_taken, closed, digit_orders, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# Key C from an Orthodox village (the region's first, owned by faction 0) and from a Cult one (its
# second, owned by faction 1). Control: the Orthodox refusal shows its reason, so the Cult's
# acceptance is the duel rule's and not the key's.
func _challenge_pass() -> bool:
	var orthodox := _scene(0)
	var refused: bool = not orthodox.command(_key(KEY_C))
	orthodox.get_node("Hud").refresh(orthodox.world, orthodox.steered, PackedInt64Array([0]))
	var reason: String = orthodox.get_node("Hud").labels.notice.text
	orthodox.free()

	var cult := _scene(1)
	var world: WorldAdapter = cult.world
	var faction := world.army_faction(cult.steered)
	var leader_before := world.leader(faction)
	var accepted: bool = cult.command(_key(KEY_C))
	var hud: CanvasLayer = cult.get_node("Hud")
	hud.refresh(world, cult.steered, PackedInt64Array([0]))
	var shown: bool = hud.duel_panel.visible and not world.duel(faction).is_empty()
	cult.step()
	var again: bool = not cult.command(_key(KEY_C))
	var again_reason: String = hud.labels.notice.text
	var passed := refused and reason == "CHALLENGE_NO_SUCCESSION" and accepted and shown and again \
			and again_reason == "CHALLENGE_DUEL_IN_PROGRESS" and world.leader(faction) == leader_before
	print("play_test: key C from an Orthodox village refused %s, the HUD saying %s; from a Cult village accepted %s, the duel shown %s; again refused %s, the HUD saying %s: %s" % [
		refused, reason, accepted, shown, again, again_reason, "pass" if passed else "FAIL"])
	cult.free()
	return passed


func _scene(village: int) -> Node2D:
	var scene: Node2D = load(SCENE).instantiate()
	scene.birth_village = village
	scene.notification(Node.NOTIFICATION_READY)
	return scene


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
