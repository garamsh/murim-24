# Checks that play.tscn turns input into orders for the person it steers: a left click on a passable
# cell moves the person toward it, and a click on a blocked cell is refused; key 1 orders the
# person's first skill, and key 9, a skill it lacks, is refused. Key C challenges the person's
# faction's leader: born into an Orthodox village, the core refuses it for having no duel and the
# HUD says so; born into a Cult village, it starts a duel the HUD shows, and a second C while it is
# fought is refused for that. When a village's residents march away, its territory follows them,
# and the world's borders and the HUD's map follow the territory the core derives, redrawn on that
# tick and not on every frame.
extends RefCounted

const SCENE := "res://play/play.tscn"
const TICKS := 600
# How far from the person's home, in cells, to look for the cells to click.
const SEARCH := 64
# In the region the scene's seed makes, village 5's residents are army 4, of the Cult; ordered on
# village 7, of the Orthodox, they take village 5's seat, and its claim, with them.
const MARCHERS := 4
const MARCHED_ON := 6


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
	return passed and _challenge_pass() and _genealogy_pass() and _sect_pass() and _territory_pass()


# The borders and the map follow the territory as it moves. Setup: a tick with no order moves no
# territory, and the tick after the march is ordered finds it moved. Before and after, the borders
# and the map's border pixels match the territory the core gives then; control: the territory after no
# longer matches the borders before, so the match after is the redraw's. The tick that moved
# nothing, and frames after the move, keep the same borders and map.
func _territory_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var art: Node2D = scene.get_node("WorldArt")
	var hud: CanvasLayer = scene.get_node("Hud")
	var territory: Node2D = art.get_node("Territory")
	var before := world.territory()
	var borders_before: Array = territory.borders
	var map_before: ImageTexture = hud.map_texture
	var drawn_before := _borders_follow(borders_before, before, world) \
			and _map_follows(hud, before, world)
	scene.step()
	var unchanged_tick: bool = world.territory() == before \
			and is_same(territory.borders, borders_before) and is_same(hud.map_texture, map_before)
	var target: Dictionary = world.villages()[MARCHED_ON]
	var ordered := world.order(MARCHERS, target.corner + target.area / 2,
			PackedInt64Array(range(world.skill_count(MARCHERS))))
	scene.step()
	var after := world.territory()
	var moved := after != before
	var borders_after: Array = territory.borders
	var map_after: ImageTexture = hud.map_texture
	for frame in 3:
		_frame(scene)
	var frames_kept: bool = is_same(territory.borders, borders_after) \
			and is_same(hud.map_texture, map_after) and not is_same(map_after, map_before)
	var drawn_after := _borders_follow(borders_after, after, world) and _map_follows(hud, after, world)
	var old_stale := not _borders_follow(borders_before, after, world)
	var passed := unchanged_tick and ordered and moved and drawn_before and drawn_after \
			and old_stale and frames_kept
	print("play_test: a tick with no order keeps the territory, borders and map %s; army %d ordered on village %d %s, the territory moved by the next tick %s; borders and map follow the territory before %s and after %s, the borders before stale after %s; kept over frames %s: %s" % [
		unchanged_tick, MARCHERS, MARCHED_ON, ordered, moved, drawn_before, drawn_after,
		old_stale, frames_kept, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# Draws a frame as the scene does after its ticks, over the view around the steered person.
func _frame(scene: Node2D) -> void:
	var view := Vector2(480, 270)
	scene.get_node("WorldArt").refresh(Rect2(scene.lead() * 16 / WorldAdapter.CELL_SIZE - view / 2, view))
	scene.get_node("Hud").refresh(scene.world, scene.steered, PackedInt64Array([0]))


# Whether `borders` are exactly the edges between neighbouring cells in different territories of
# `owners`, each carrying the two cells' owners, left or top first.
func _borders_follow(borders: Array, owners: PackedInt64Array, world: WorldAdapter) -> bool:
	var width := world.map_rows()[0].length()
	var edges := 0
	for i in owners.size():
		if (i + 1) % width != 0 and owners[i] != owners[i + 1]:
			edges += 1
		if i + width < owners.size() and owners[i] != owners[i + width]:
			edges += 1
	for border: Array in borders:
		var cell := Vector2i(border[0]) / 16
		var vertical: bool = border[0].x == border[1].x
		var first := cell - (Vector2i(1, 0) if vertical else Vector2i(0, 1))
		if border[2] != owners[first.y * width + first.x] or border[3] != owners[cell.y * width + cell.x] \
				or border[2] == border[3]:
			return false
	return borders.size() == edges and edges > 0


# Whether the HUD's map marks with a faction's accent exactly the cells, outside every village's
# square, whose right or lower neighbour lies in another territory of `owners`.
func _map_follows(hud: CanvasLayer, owners: PackedInt64Array, world: WorldAdapter) -> bool:
	var image: Image = hud.map_texture.get_image()
	var width := world.map_rows()[0].length()
	var accents := []
	for accent: Color in hud.ACCENTS:
		accents.append(accent.to_html(false))
	var squares: Array[Rect2i] = []
	for village: Dictionary in world.villages():
		squares.append(Rect2i(village.corner, village.area))
	for i in owners.size():
		var cell := Vector2i(i % width, i / width)
		if squares.any(func(square: Rect2i) -> bool: return square.has_point(cell)):
			continue
		var border := ((i + 1) % width != 0 and owners[i] != owners[i + 1]) \
				or (i + width < owners.size() and owners[i] != owners[i + width])
		if accents.has(image.get_pixel(cell.x, cell.y).to_html(false)) != border:
			return false
	return true


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


# M opens the sect panel on the first sect of the steered person's faction, the person belonging
# to none as the world begins, and takes the keys while it is open, so a digit gives no order; Tab
# shows the next faction, and M again closes it. Control: with it closed, the same digit orders.
func _sect_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var panel: Control = scene.get_node("Hud").sect
	var faction := world.army_faction(scene.steered)
	var opened: bool = scene.command(_key(KEY_M)) and panel.visible and panel.faction == faction \
			and panel.sect >= 0 and world.sect_faction(panel.sect) == faction
	var digit_taken: bool = not scene.command(_key(KEY_1))
	var tabbed: bool = scene.command(_key(KEY_TAB)) and panel.faction != faction
	var closed: bool = scene.command(_key(KEY_M)) and not panel.visible
	var digit_orders: bool = scene.command(_key(KEY_1))
	var passed := opened and digit_taken and tabbed and closed and digit_orders
	print("play_test: M opens the sect panel on the steered person's faction %s, a digit gives no order while open %s, Tab shows the next faction %s, M closes it %s, then the digit orders %s: %s" % [
		opened, digit_taken, tabbed, closed, digit_orders, "pass" if passed else "FAIL"])
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
