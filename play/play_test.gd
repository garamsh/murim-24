# Checks that play.tscn turns input into orders for the person it steers: a left click on a passable
# cell moves the person toward it, and a click on a blocked cell is refused; key 1 orders the
# person's first skill, and key 9, a skill it lacks, is refused. Key C challenges the person's
# faction's leader: born into an Orthodox village, the core refuses it for having no duel and the
# HUD says so; born into a Cult village, it starts a duel the HUD shows, and a second C while it is
# fought is refused for that. When a village's residents march away, its territory follows them,
# and the world's borders and the HUD's map follow the territory the core derives, redrawn on that
# tick and not on every frame. W, A, S and D move the person their way through the orders port, and
# releasing them stops it. The scene opens on the title screen with no world; a new game starts the
# world from the seed typed; Escape pauses it, the pause panel's guide showing the keys the scene
# binds, and no tick runs while it is paused; the speed the settings choose runs that many ticks.
# Enter in the sect panel applies to the sect shown, joining it where the core admits the person and
# showing why where it refuses. A Cult body marched on an Orthodox village dies there, and the
# rebirth screen opens on it and the soul's new body, holding the ticks until Enter goes on.
extends RefCounted

const SCENE := "res://play/play.tscn"
# The seed every scene here starts its world from; MARCHERS and MARCHED_ON are of its region.
const SEED := 2026
const TICKS := 600
# How far from the person's home, in cells, to look for the cells to click.
const SEARCH := 64
# Ticks a move key is held for, and ticks after its release.
const WALK_TICKS := 60
const STOP_TICKS := 60
# The seed the new game is typed with.
const TYPED_SEED := 4817
# In the region the scene's seed makes, village 5's residents are army 4, of the Cult; ordered on
# village 7, of the Orthodox, they take village 5's seat, and its claim, with them.
const MARCHERS := 4
const MARCHED_ON := 6
# Born into village 1, of the Cult, the person may join the Heavenly Demon; born into village 2, of
# the Unorthodox, it lacks the proof the Black Fang asks.
const CULT_VILLAGE := 1
const UNORTHODOX_VILLAGE := 2
# Ticks a Cult body marched on the nearest Orthodox village has to die there.
const MARCH_LIMIT := 12000


func run() -> bool:
	var scene := _scene(0)
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
	return passed and _challenge_pass() and _genealogy_pass() and _sect_pass() and _territory_pass() \
			and _walk_pass() and _screens_pass() and _apply_pass() and _rebirth_pass()


# A move key pressed orders the person STRIDE cells its way, and it walks there: further its way
# than across it. Released, the key orders it to stop where it stands, and it moves less than a
# cell. Control: the same scene with no key pressed, ticked as long, leaves the person in place,
# so the walk is the key's. Setup: the way chosen is passable for STRIDE cells.
func _walk_pass() -> bool:
	var scene := _scene(0)
	var control := _scene(0)
	var rows: PackedStringArray = scene.world.map_rows()
	var home := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var keycode := KEY_NONE
	for candidate: Key in scene.MOVE_KEYS:
		var way: Vector2i = scene.MOVE_KEYS[candidate]
		var clear := true
		for stride in range(1, scene.STRIDE + 1):
			var cell := home + way * stride
			clear = clear and cell.y >= 0 and cell.y < rows.size() and cell.x >= 0 \
					and cell.x < rows[cell.y].length() and rows[cell.y][cell.x] == "."
		if keycode == KEY_NONE and clear:
			keycode = candidate
	if keycode == KEY_NONE:
		print("play_test: no way from %s is passable for %d cells: FAIL" % [home, scene.STRIDE])
		return false
	var direction := Vector2(scene.MOVE_KEYS[keycode])
	var from: Vector2 = scene.lead()
	var control_from: Vector2 = control.lead()
	var pressed: bool = scene.command(_key(keycode))
	for i in WALK_TICKS:
		scene.step()
		control.step()
	var walked: Vector2 = (scene.lead() - from) / WorldAdapter.CELL_SIZE
	var along := walked.dot(direction)
	var across := absf(walked.dot(Vector2(direction.y, direction.x)))
	var stood: float = (control.lead() - control_from).length() / WorldAdapter.CELL_SIZE
	var release := _key(keycode)
	release.pressed = false
	var stopped_at: Vector2 = scene.lead()
	var released: bool = scene.command(release)
	for i in STOP_TICKS:
		scene.step()
	var after_stop: float = (scene.lead() - stopped_at).length() / WorldAdapter.CELL_SIZE
	var passed := pressed and along >= 1.0 and along > across and stood < 0.5 and released and after_stop < 1.0
	print("play_test: %s from %s ordered %s, walked %.2f cells its way and %.2f across in %d ticks; released %s, then moved %.2f cells in %d; control without a key moved %.2f cells: %s" % [
		OS.get_keycode_string(keycode), home, pressed, along, across, WALK_TICKS, released, after_stop,
		STOP_TICKS, stood, "pass" if passed else "FAIL"])
	scene.free()
	control.free()
	return passed


# The scene opens on the title with no world; Enter on a new game opens the new-game panel, and the
# digits typed and Enter start the world from that seed, closing both. Escape opens the pause
# panel on that seed, its guide showing the move keys, the first and last skill keys and the key
# for all skills as the scene binds them, and no tick runs while it is open. Control: once resumed,
# the same time runs its ticks, and at the settings' fourfold speed at least four times as many.
func _screens_pass() -> bool:
	var scene: Node2D = load(SCENE).instantiate()
	scene.notification(Node.NOTIFICATION_READY)
	var hud: CanvasLayer = scene.get_node("Hud")
	var titled: bool = scene.world == null and hud.screen() == hud.title_screen
	scene.navigate(_key(KEY_ENTER))
	var new_game_open: bool = hud.screen() == hud.new_game
	for digit in str(TYPED_SEED):
		scene.navigate(_key(KEY_0 + int(digit)))
	scene.navigate(_key(KEY_ENTER))
	var started: bool = scene.world != null and scene.world_seed == TYPED_SEED and hud.screen() == null
	var same_world := false
	if started:
		var again := WorldAdapter.create_region(TYPED_SEED, 0)
		same_world = scene.world.hash() == again.hash() \
				and scene.world.hash() != WorldAdapter.create_region(SEED, 0).hash()

	var paused: bool = scene.navigate(_key(scene.PAUSE_KEY)) and hud.screen() == hud.pause \
			and hud.pause.world_seed == TYPED_SEED
	var caps := []
	for control: Dictionary in hud.pause.controls:
		caps.append_array(control.keys)
	var skills := "%s-%s" % [OS.get_keycode_string(scene.SKILL_KEYS.front()), OS.get_keycode_string(scene.SKILL_KEYS.back())]
	var guided: bool = caps.has(skills) and caps.has(OS.get_keycode_string(scene.ALL_SKILLS_KEY)) \
			and scene.MOVE_KEYS.keys().all(func(keycode: Key) -> bool: return caps.has(OS.get_keycode_string(keycode)))
	# Three and a half ticks' time, so that the half left over is not lost to rounding.
	var span := 3.5 / WorldAdapter.TICKS_PER_SECOND
	var before: int = scene.ticks
	scene.advance(span)
	var held: bool = scene.ticks == before
	scene.navigate(_key(KEY_ESCAPE))
	scene.advance(span)
	var ran: int = scene.ticks - before
	hud.settings.chosen.speed = hud.settings.SPEEDS.find(4)
	scene.advance(span)
	var ran_fast: int = scene.ticks - before - ran
	var passed := titled and new_game_open and started and same_world and paused and guided and held \
			and ran == 3 and ran_fast >= 4 * ran
	print("play_test: opens on the title with no world %s; Enter opens the new game %s; %d typed and Enter starts that world %s, the same as the core makes from it and not seed %d's %s; Escape pauses it on that seed %s, the guide showing %s %s; no tick while paused %s; resumed, 3.5 ticks' time runs %d ticks, at 4x %d: %s" % [
		titled, new_game_open, TYPED_SEED, started, SEED, same_world, paused, caps, guided, held, ran, ran_fast,
		"pass" if passed else "FAIL"])
	scene.free()
	return passed


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


# The scene with a world started from SEED, its player born into `village`.
func _scene(village: int) -> Node2D:
	var scene: Node2D = load(SCENE).instantiate()
	scene.birth_village = village
	# The runner has no running tree to add the scene to, so it is readied directly.
	scene.notification(Node.NOTIFICATION_READY)
	scene.start(SEED)
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


# M opens the sect panel on the Cult person's sect, and Enter makes it a member, the panel saying it
# was admitted. Control: born into an Unorthodox village, Enter is refused for want of proof, the
# panel says so, and the person stays out of every sect.
func _apply_pass() -> bool:
	var scene := _scene(CULT_VILLAGE)
	var world: WorldAdapter = scene.world
	var panel: Control = scene.get_node("Hud").sect
	var person := world.roster(scene.steered)[0]
	var opened: bool = scene.command(_key(KEY_M)) and panel.sect >= 0 and world.person_sect(person) == -1
	var entered: bool = scene.command(_key(KEY_ENTER))
	var joined: bool = world.person_sect(person) == panel.sect and panel.verdict == "APPLICATION_NONE"
	scene.free()
	var refused_scene := _scene(UNORTHODOX_VILLAGE)
	var other: WorldAdapter = refused_scene.world
	var other_panel: Control = refused_scene.get_node("Hud").sect
	var other_person := other.roster(refused_scene.steered)[0]
	refused_scene.command(_key(KEY_M))
	var other_entered: bool = refused_scene.command(_key(KEY_ENTER))
	var refused: bool = other_entered and other_panel.verdict == "APPLICATION_UNPROVEN" \
			and other.person_sect(other_person) == -1
	refused_scene.free()
	var passed := opened and entered and joined and refused
	print("play_test: M opens the sect panel %s; Enter applies %s and joins %s; from an Unorthodox village Enter is refused as unproven %s: %s" % [
		opened, entered, joined, refused, "pass" if passed else "FAIL"])
	return passed


# A Cult body ordered on the nearest Orthodox village's residents dies among them and its soul is
# born again: on that tick the rebirth screen opens on the body that died and the new one, and no
# tick runs while it is open; Enter closes it and the ticks run again. Setup: the screen was not
# open before the death, and the dead body and the new one differ.
func _rebirth_pass() -> bool:
	var scene := _scene(CULT_VILLAGE)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var body := world.roster(scene.steered)[0]
	var home := world.person_position(body)
	var target := Vector2i(-1, -1)
	for army in world.army_count() - 1:
		var roster := world.roster(army)
		if world.army_faction(army) == 0 and not roster.is_empty():
			var cell := Vector2i((world.person_position(roster[0]) / WorldAdapter.CELL_SIZE).floor())
			if target.x < 0 or Vector2(cell).distance_to(home / WorldAdapter.CELL_SIZE) \
					< Vector2(target).distance_to(home / WorldAdapter.CELL_SIZE):
				target = cell
	var ordered := target.x >= 0 and world.order(scene.steered, target, PackedInt64Array([0]))
	var closed_before := true
	var ticks := 0
	while ordered and not hud.rebirth.visible and ticks < MARCH_LIMIT:
		closed_before = closed_before and hud.screen() == null
		scene.step()
		ticks += 1
	var reborn: int = world.roster(scene.steered)[0] if not world.roster(scene.steered).is_empty() else -1
	var shown: bool = hud.rebirth.visible and hud.screen() == hud.rebirth and hud.rebirth.past == body \
			and hud.rebirth.next == reborn and reborn != body and world.person_health(body) == 0
	var before: int = scene.ticks
	scene.advance(1.0)
	var held: bool = scene.ticks == before
	scene.navigate(_key(KEY_ENTER))
	scene.advance(1.0)
	var resumed: bool = not hud.rebirth.visible and scene.ticks > before
	scene.free()
	var passed := ordered and closed_before and shown and held and resumed
	print("play_test: Cult body %d ordered on %s %s; after %d ticks the rebirth screen shows %d dead and %d born %s, not open before %s; no tick while open %s; Enter goes on and ticks run %s: %s" % [
		body, target, ordered, ticks, body, reborn, shown, closed_before, held, resumed, "pass" if passed else "FAIL"])
	return passed

