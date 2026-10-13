# Checks that play.tscn turns input into orders for the person it steers: a right click on a passable
# cell moves the person toward it, and a click on a blocked cell is refused; key 1 orders the
# person's first skill, and key 9, a skill it lacks, is refused. Key C challenges the person's
# faction's leader: born into an Orthodox village, the core refuses it for having no duel and the
# HUD says so; born into a Cult village, it starts a duel the HUD shows, and a second C while it is
# fought is refused for that. When a village's residents march away, its territory follows them,
# and the world's borders and the HUD's map follow the territory the core derives, redrawn on that
# tick and not on every frame. Holding the right button keeps ordering the person toward the cursor,
# and releasing it stops the following; pressed on a cell down a diagonal, it walks the person along
# the diagonal. Q casts the first skill at the cursor, on its press or, as
# the settings choose, on its release after showing the aim, which Q again or Escape cancels; a
# refused cast shows its reason. The scene opens on the title screen with no world; a new game starts the
# world from the seed typed; Escape pauses it, the pause panel's guide showing the keys the scene
# binds, and no tick runs while it is paused; the speed the settings choose runs that many ticks.
# Enter in the sect panel applies to the sect shown, joining it where the core admits the person and
# showing why where it refuses. A Cult body marched on the largest hostile army dies there, and the
# rebirth screen opens on it and the soul's new body, holding the ticks until Enter goes on.
# The pause panel's Save writes the world, and back on the title Continue resumes it, steering the
# same person, while with no save Continue cannot be taken and with a broken one the title names
# why; the world is saved again every AUTOSAVE_DAYS days. With the touch controls shown, the
# joystick walks the person the way it is pushed, a diagonal included, and stops it when let go; a
# skill button tapped
# casts at the hostile person the core aims at the way the person faces, dragged casts at the point
# it aims, dropped on the cancel zone casts nothing, and held casts on the person itself, while a
# tap the core refuses for its cooldown shows why; the screens take taps, and the HUD's buttons
# open the pause panel and the genealogy panel. K opens the arts panel, where a skill chosen is put
# on a slot with that slot's key and taken off with Delete; a person who knows an art beyond its
# army's skills, with it put on T, sees it on T's slot, aims at its range and casts it at that range
# from the fifth skill button, while an empty slot shows and casts nothing. The loadout is saved
# with the world, and Continue restores it. Walked up to a person of an allied faction, the prompt
# names it and C opens the dialogue window on it, as a left click on it does, which takes the keys:
# Enter lists its news, Escape goes back, then ends the talk. Once its faction is at war with the
# person's, the prompt shows its refusal, C opens nothing, and an open window closes; control: until
# then it stays open. A left click on a foe in sight selects it without moving the person, and
# Space then sets an attack intent on it, which the person chases, until a move order ends it; Tab
# selects the next foe and Escape clears the selection; Space on an ally is refused, and with no
# foe in sight does nothing; the touch controls' next and attack buttons do what Tab and Space do. The people near the person are named over their heads, the person itself and the one the
# talk prompt names left out, and no name shows while a panel is open. The camera stops at the
# map's bounds, its bottom raised under raised ground, and centres a map narrower than the screen.
extends RefCounted

const SCENE := "res://play/play.tscn"
# The seed every scene here starts its world from; MARCHERS and MARCHED_ON are of its region.
const SEED := 2026
const TICKS := 600
# How far from the person's home, in cells, to look for the cells to click.
const SEARCH := 64
# Ticks the person is given to walk toward each cell the cursor is held on.
const WALK_TICKS := 60
# Pixels east of the person the cursor is put for a cast, within arm's length, and further than any skill
# reaches.
const CAST_OFFSET := Vector2(16, 0)
const FAR_OFFSET := Vector2(400, 0)
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
# Ticks a Cult body marched on the largest hostile army has to die there.
const MARCH_LIMIT := 12000
# A region whose growth forms an army after the player's within fifteen years of five-tick days.
const GROWN_SEED := 3
const GROWN_TICKS_PER_DAY := 5
const GROWN_TICKS := 9000
const TestSaves := preload("res://extension/test_saves.gd")
const ViewScale := preload("res://view_scale.gd")
const WorldArt := preload("res://world_art/world_art.gd")
# Ticks a hostile army marched on the person has to come within its first skill's range, and ticks
# the person has to be ready to cast again.
const APPROACH_LIMIT := 4000
const READY_LIMIT := 120
# Cells from the person within which the hostile army halts in the attack intent's pass, within the
# rules' sight of 6 cells, and ticks each of its steps runs.
const INTENT_APART := 4
const INTENT_TICKS := 30
# Ticks a person ordered off an attack intent is given to reach the cell it was ordered to.
const MOVE_LIMIT := 200
# Ticks after the joystick is let go, and the four ways it is pushed to find one passable.
const STOP_TICKS := 60
const WAYS := [Vector2i.UP, Vector2i.LEFT, Vector2i.DOWN, Vector2i.RIGHT]
# Ticks the joystick is held for: long enough to walk past the STRIDE cells one order reaches.
const STICK_TICKS := 150
# Ticks the person is given to walk up to a person of an allied faction.
const TALK_WALK_LIMIT := 6000
# In the region SEED makes, the person born into village 7 has a diagonal open from where it is
# born, and the four ways it is walked along.
const DIAGONAL_VILLAGE := 7
const DIAGONALS := [Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1)]


func run() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var home_cell := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var target := _cell_near(scene.world.map_rows(), home_cell, ".")
	var wall := _cell_near(scene.world.map_rows(), home_cell, "#")
	# Setup: both cells were found, so a refusal below is the wall's, not a missing cell.
	var aimed := target != home_cell and wall != home_cell
	var target_click := _click(target, scene)
	var wall_click := _click(wall, scene)
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
	return passed and _challenge_pass() and _factions_pass() and _genealogy_pass() and _sect_pass() and _territory_pass() \
			and _move_pass() and _cast_pass() and _screens_pass() and _shortcuts_pass() and _numbers_pass() and _apply_pass() and _rebirth_pass() and _save_pass() \
			and _autosave_pass() and _stick_pass() and _stick_diagonal_pass() and _move_diagonal_pass() \
			and _touch_cast_pass() and _taps_pass() and _arts_pass() and _art_slot_pass() and _loadout_save_pass() \
			and _talk_pass() and _trade_pass() and _names_pass() and _intent_pass() and _camera_pass()


# The camera stops at the map's bounds: centred on a person at the map's corner, or at its bottom
# edge under raised ground, it shows no pixel past them, and on a map narrower than the screen it
# shows the map's middle. Control: on a person in the map's middle it centres on the person. The
# bounds are drawn_bounds(): a map whose last row is raised four levels ends 64 pixels higher.
# Control: a flat one ends at its edge.
func _camera_pass() -> bool:
	var Play := preload("res://play/play.gd")
	var screen := Vector2(640, 360)
	var size := Vector2(128, 128) * Relief.CELL
	var flat := Relief.new(Vector2i(128, 128))
	var raised := Relief.new(Vector2i(128, 128))
	for x in 128:
		raised.lay(Vector2i(x, 127), "rock", 4, "rock_face", false)
	var flat_bounds: Rect2 = Play.drawn_bounds(size, flat)
	var raised_bounds: Rect2 = Play.drawn_bounds(size, raised)
	var corner := Rect2(Play.view_centre(Vector2(3, 3), screen, raised_bounds).round() - screen / 2.0, screen)
	var bottom := Rect2(Play.view_centre(Vector2(1000, size.y), screen, raised_bounds).round() - screen / 2.0, screen)
	var middle: Vector2 = Play.view_centre(Vector2(1000, 700), screen, raised_bounds)
	var small: Vector2 = Play.view_centre(Vector2(10, 10), screen, Rect2(0, 0, 320, 200))
	var passed: bool = flat_bounds == Rect2(Vector2.ZERO, size) \
			and raised_bounds == Rect2(Vector2.ZERO, size - Vector2(0, 4 * Relief.LEVEL_PX)) \
			and raised_bounds.encloses(corner) and raised_bounds.encloses(bottom) \
			and middle == Vector2(1000, 700) and small == Vector2(160, 100)
	print("play_test: bounds flat %s, raised %s; the view at the corner %s and at the bottom %s; centred on the middle at %s, on a small map at %s: %s" % [
		flat_bounds, raised_bounds, corner, bottom, middle, small, "pass" if passed else "FAIL"])
	return passed


# With the touch controls shown, a thumb on the joystick held pushed a way passable for three times
# STRIDE cells walks the person past the STRIDE cells one order reaches, as it is ordered on from
# each new cell, further its way than across it, and let go, stops it. Control: the same
# scene with the touch controls shown and no touch, ticked as long, leaves the person in place.
# Setup: the touch controls are shown in both.
func _stick_pass() -> bool:
	var scene := _scene(0)
	var control := _scene(0)
	scene.get_node("Hud").use_touch(true)
	control.get_node("Hud").use_touch(true)
	var touch: Control = scene.get_node("Hud").touch
	var shown: bool = touch.visible and control.get_node("Hud").touch.visible
	var way := _clear_way(scene, 3 * scene.STRIDE)
	if way == Vector2i.ZERO:
		print("play_test: no way is passable for %d cells: FAIL" % (3 * scene.STRIDE))
		return false
	var direction := Vector2(way)
	var from: Vector2 = scene.lead()
	var control_from: Vector2 = control.lead()
	var thumb: Vector2 = touch.STICK
	var took: bool = scene.touch(_finger(0, thumb, true)) and scene.touch(_slide(0, thumb + direction * touch.STICK_RADIUS))
	for i in STICK_TICKS:
		scene.step()
		control.step()
	var walked: Vector2 = (scene.lead() - from) / WorldAdapter.CELL_SIZE
	var along := walked.dot(direction)
	var across := absf(walked.dot(Vector2(direction.y, direction.x)))
	var stood: float = (control.lead() - control_from).length() / WorldAdapter.CELL_SIZE
	var stopped_at: Vector2 = scene.lead()
	var released: bool = scene.touch(_finger(0, thumb + direction * touch.STICK_RADIUS, false))
	for i in STOP_TICKS:
		scene.step()
	var after_stop: float = (scene.lead() - stopped_at).length() / WorldAdapter.CELL_SIZE
	var passed: bool = shown and took and along > scene.STRIDE + 1 and along > across and stood < 0.5 and released \
			and after_stop < 1.0
	print("play_test: touch shown %s; the joystick pushed %s taken %s, walked %.2f cells its way, past the %d one order reaches, and %.2f across in %d ticks; let go %s, then moved %.2f cells in %d; control with no touch moved %.2f cells: %s" % [
		shown, way, took, along, scene.STRIDE, across, STICK_TICKS, released, after_stop, STOP_TICKS, stood,
		"pass" if passed else "FAIL"])
	scene.free()
	control.free()
	return passed


# With the touch controls shown, the joystick pushed down a diagonal open for three times STRIDE
# cells walks the person along it, past the STRIDE cells one order reaches along each axis, and as
# far along one as the other to within a cell. Setup: the touch controls are shown and the
# diagonal is open.
func _stick_diagonal_pass() -> bool:
	var scene := _scene(DIAGONAL_VILLAGE)
	var stride: int = scene.STRIDE
	scene.get_node("Hud").use_touch(true)
	var touch: Control = scene.get_node("Hud").touch
	var shown := touch.visible
	var way := _clear_way(scene, 3 * stride, DIAGONALS)
	var from: Vector2 = scene.lead()
	var thumb: Vector2 = touch.STICK
	var took: bool = scene.touch(_finger(0, thumb, true)) \
			and scene.touch(_slide(0, thumb + Vector2(way).normalized() * touch.STICK_RADIUS))
	for i in STICK_TICKS:
		scene.step()
	var walked: Vector2 = (scene.lead() - from) / WorldAdapter.CELL_SIZE * Vector2(way)
	scene.free()
	var passed: bool = shown and way != Vector2i.ZERO and took and walked.x > stride + 1 \
			and walked.y > stride + 1 and absf(walked.x - walked.y) < 1.0
	print("play_test: touch shown %s; the joystick pushed down the diagonal %s taken %s, walked %.2f and %.2f cells along each axis its way in %d ticks: %s" % [
		shown, way, took, walked.x, walked.y, STICK_TICKS, "pass" if passed else "FAIL"])
	return passed


# The right button pressed on a cell three times STRIDE cells down a diagonal open that far walks
# the person along the diagonal, more than STRIDE cells along each axis, and as far along one as
# the other to within a cell. Setup: the diagonal is open and the order accepted.
func _move_diagonal_pass() -> bool:
	var scene := _scene(DIAGONAL_VILLAGE)
	var stride: int = scene.STRIDE
	var way := _clear_way(scene, 3 * stride, DIAGONALS)
	var home := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var target := home + way * 3 * stride
	var from: Vector2 = scene.lead()
	var pressed: bool = scene.command(_click(target, scene))
	for i in STICK_TICKS:
		scene.step()
	var walked: Vector2 = (scene.lead() - from) / WorldAdapter.CELL_SIZE * Vector2(way)
	scene.free()
	var passed: bool = way != Vector2i.ZERO and pressed and walked.x > stride and walked.y > stride \
			and absf(walked.x - walked.y) < 1.0
	print("play_test: right button on %s, down the diagonal %s from %s, ordered %s, walked %.2f and %.2f cells along each axis its way in %d ticks: %s" % [
		target, way, home, pressed, walked.x, walked.y, STICK_TICKS, "pass" if passed else "FAIL"])
	return passed


# With the touch controls shown and a hostile person marched within the first skill's range: the
# joystick pushed toward it faces the person that way, and the first skill's button tapped casts
# at the person the core aims at that way. Once the person may cast again, the button dragged off
# its pad onto the cancel zone casts nothing, while the same drag toward the foe let go short of it
# casts the targeted strike at the person the core aims at the way it points. The second skill's button held for a long press casts
# on the person itself, while held a little less it casts as a tap, at a point. Once the cast has
# struck, a tap the core refuses for its cooldown shows that reason. Control: the taps before it
# were accepted and showed none. Setup: a hostile person stands within range and the person is
# ready before each cast.
func _touch_cast_pass() -> bool:
	var scene := _scene(0)
	var hud: CanvasLayer = scene.get_node("Hud")
	hud.use_touch(true)
	var world: WorldAdapter = scene.world
	var touch: Control = hud.touch
	var now := [0]
	touch.clock = func() -> int: return now[0]
	var person: int = world.roster(scene.steered)[0]
	var here := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var foes := _nearest_hostile(world, scene.steered)
	world.order(foes, here, PackedInt64Array([0]))
	var waited := 0
	while world.aim(person, 0, Vector2.ZERO) < 0 and waited < APPROACH_LIMIT:
		scene.step()
		waited += 1
	var foe := world.aim(person, 0, Vector2.ZERO)
	var in_range := foe >= 0 and _ready_to_cast(scene, person, 0, foe)
	if not in_range:
		print("play_test: no hostile person came within range in %d ticks: FAIL" % APPROACH_LIMIT)
		scene.free()
		return false
	var main: Vector2 = touch.SKILLS[0].at
	var toward := (world.person_position(foe) - world.person_position(person)).normalized()
	scene.touch(_finger(0, touch.STICK, true))
	scene.touch(_slide(0, touch.STICK + toward * touch.STICK_RADIUS))
	scene.touch(_finger(0, touch.STICK + toward * touch.STICK_RADIUS, false))
	hud.labels.notice.visible = false
	scene.last_cast = {}
	_tap(scene, 0, main, true)
	var aimed := world.aim(person, 0, (scene.facing.normalized() * WorldAdapter.CELL_SIZE).round())
	var tap_cast: Dictionary = scene.last_cast
	var tap_right: bool = aimed >= 0 and tap_cast == {"skill": 0, "target": aimed} \
			and world.hostile(world.army_faction(scene.steered), world.person_faction(aimed)) \
			and not hud.labels.notice.visible
	scene.step()

	var ready_again := _ready_to_cast(scene, person, 0, foe)
	var reach := float(world.skills(scene.steered)[0].range)
	scene.last_cast = {}
	scene.touch(_finger(1, main, true))
	scene.touch(_slide(1, main + Vector2(touch.SLOP + 4, 0)))
	scene.touch(_slide(1, touch.CANCEL.at))
	scene.touch(_finger(1, touch.CANCEL.at, false))
	var cancelled: Dictionary = scene.last_cast
	var way: Vector2 = (world.person_position(foe) - world.person_position(person)).normalized()
	var pointed := world.aim(person, 0, (way * reach).round())
	scene.touch(_finger(1, main, true))
	scene.touch(_slide(1, main + way * touch.PAD))
	scene.touch(_finger(1, main + way * touch.PAD, false))
	var dragged: Dictionary = scene.last_cast
	var drag_right: bool = ready_again and cancelled.is_empty() and pointed >= 0 \
			and dragged == {"skill": 0, "target": pointed}

	var second: Vector2 = touch.SKILLS[1].at
	scene.touch(_finger(2, second, true))
	now[0] += touch.LONG_PRESS_MS - 100
	scene.touch(_finger(2, second, false))
	var short: Dictionary = scene.last_cast
	scene.touch(_finger(2, second, true))
	now[0] += touch.LONG_PRESS_MS
	scene.touch(_finger(2, second, false))
	var held: Dictionary = scene.last_cast
	var self_right: bool = short.get("skill", -1) == 1 and short.target is Vector2 \
			and held == {"skill": 1, "target": null} and not hud.labels.notice.visible
	scene.step()

	scene.last_cast = {}
	_tap(scene, 0, main, false)
	var refusal := world.cast_refusal(person, 0, aimed)
	var refused: bool = refusal == WorldAdapter.CAST_ON_COOLDOWN and scene.last_cast.is_empty() \
			and hud.labels.notice.visible and hud.labels.notice.text == "CAST_ON_COOLDOWN"
	var passed := tap_right and drag_right and self_right and refused
	print("play_test: hostile army %d marched %d ticks, foe %d in range; a tap facing %s cast %s at the aimed %d %s; dragged onto the cancel zone cast %s, short of it %s %s; held %d ms %s, %d ms %s %s; a tap on cooldown refused %d, the notice %s shown %s: %s" % [
		foes, waited, foe, scene.facing, tap_cast, aimed, tap_right, cancelled, dragged, drag_right,
		touch.LONG_PRESS_MS - 100, short, touch.LONG_PRESS_MS, held, self_right, refusal, hud.labels.notice.text,
		refused, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# The title's new game tapped opens the new-game panel, where the random slip tapped draws a seed
# into the field and start tapped starts the world from it. With the touch controls shown, the
# pause button tapped pauses the world and resume tapped on the panel goes on; the genealogy
# button tapped opens that panel on the person, and a tap closes it. Control: with the touch
# controls hidden, a tap on the pause button's place pauses nothing.
func _taps_pass() -> bool:
	var scene := _title()
	var hud: CanvasLayer = scene.get_node("Hud")
	var title: Control = hud.title_screen
	var menu: Vector2 = title.MENU.position + title.MENU_INSET
	scene.navigate(_finger(0, menu + Vector2(20, 5), true))
	var new_game_open: bool = hud.screen() == hud.new_game
	scene.navigate(_finger(0, hud.new_game.random_slip_rect().get_center(), true))
	var typed: String = hud.new_game.typed
	scene.navigate(_finger(0, hud.new_game.start_slip_rect().get_center(), true))
	var started: bool = scene.world != null and typed != "" and scene.world_seed == int(typed) \
			and hud.screen() == null
	var pause_at: Vector2 = hud.touch.PAUSE.get_center()
	var hidden_ignored: bool = not scene.touch(_finger(0, pause_at, true)) and hud.screen() == null
	hud.use_touch(true)
	scene.touch(_finger(0, pause_at, true))
	scene.touch(_finger(0, pause_at, false))
	var paused: bool = hud.screen() == hud.pause
	var pause_menu := Vector2(hud.pause.DIALOG.position.x + 10, hud.pause.DIALOG.position.y + 32)
	scene.navigate(_finger(0, pause_menu + Vector2(20, 5), true))
	var resumed: bool = hud.screen() == null
	scene.touch(_finger(0, hud.touch.BUTTONS.genealogy.get_center(), true))
	scene.touch(_finger(0, hud.touch.BUTTONS.genealogy.get_center(), false))
	var family_open: bool = hud.genealogy.visible and hud.genealogy.centre == scene.world.roster(scene.steered)[0]
	scene.touch(_finger(0, Vector2(240, 135), true))
	scene.touch(_finger(0, Vector2(240, 135), false))
	var family_closed: bool = not hud.genealogy.visible
	var passed := new_game_open and started and hidden_ignored and paused and resumed and family_open and family_closed
	print("play_test: new game tapped %s; random tapped drew %s, start tapped started it %s; the pause button with the touch controls hidden ignored %s, shown paused %s, resume tapped %s; the genealogy button opened the panel %s, a tap closed it %s: %s" % [
		new_game_open, typed, started, hidden_ignored, paused, resumed, family_open, family_closed,
		"pass" if passed else "FAIL"])
	scene.free()
	return passed


# The first of `ways` passable for `cells` cells from where the scene's person stands, the cells
# beside each diagonal step included, or none.
func _clear_way(scene: Node2D, cells: int, ways := WAYS) -> Vector2i:
	var rows: PackedStringArray = scene.world.map_rows()
	var home := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	for way: Vector2i in ways:
		var clear := true
		for stride in range(1, cells + 1):
			var cell := home + way * stride
			for passed: Vector2i in [cell, cell - Vector2i(way.x, 0), cell - Vector2i(0, way.y)]:
				clear = clear and passed.y >= 0 and passed.y < rows.size() and passed.x >= 0 \
						and passed.x < rows[passed.y].length() and rows[passed.y][passed.x] == "."
		if clear:
			return way
	return Vector2i.ZERO


# The army nearest army `army` whose faction its own is hostile to.
func _nearest_hostile(world: WorldAdapter, army: int) -> int:
	var here: Vector2 = world.body_positions(army)[0]
	var nearest := -1
	var best := INF
	for other in world.army_count():
		if other == army or world.roster(other).is_empty() \
				or not world.hostile(world.army_faction(army), world.army_faction(other)):
			continue
		var distance := world.body_positions(other)[0].distance_to(here)
		if distance < best:
			best = distance
			nearest = other
	return nearest


# Ticks the scene until `person` may cast skill `skill` at `target`, for READY_LIMIT ticks at most.
# Returns whether it may.
func _ready_to_cast(scene: Node2D, person: int, skill: int, target: Variant) -> bool:
	for i in READY_LIMIT:
		if scene.world.cast_refusal(person, skill, target) == WorldAdapter.CAST_NONE:
			return true
		scene.step()
	return scene.world.cast_refusal(person, skill, target) == WorldAdapter.CAST_NONE


# A tap on the skill button at `at`; when `wait` is set, the scene is first ticked until the
# person may cast its first skill at whom its aim finds.
func _tap(scene: Node2D, slot: int, at: Vector2, wait: bool) -> void:
	if wait:
		var person: int = scene.world.roster(scene.steered)[0]
		_ready_to_cast(scene, person, slot, scene.world.aim(person, slot, Vector2.ZERO))
	scene.touch(_finger(9, at, true))
	scene.touch(_finger(9, at, false))


func _finger(index: int, at: Vector2, pressed: bool) -> InputEventScreenTouch:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	return event


func _slide(index: int, at: Vector2) -> InputEventScreenDrag:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = at
	return event


# The right button pressed on a passable cell orders the person toward it, and it walks closer.
# Held, the cursor moved to a second cell orders it there, and it walks closer to that. Released,
# the cursor moved again orders nothing. Control: the same scene with no input, ticked as long,
# leaves the person in place, so the walking is the button's. Setup: both cells are passable and
# differ, and the second is not the person's cell when the cursor reaches it.
func _move_pass() -> bool:
	var scene := _scene(0)
	var control := _scene(0)
	var rows: PackedStringArray = scene.world.map_rows()
	var home := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var first := _cell_near(rows, home, ".")
	var second := _cell_near(rows, first, ".")
	var control_from: Vector2 = control.lead()
	var pressed: bool = scene.command(_click(first, scene))
	var first_from := _distance(scene.lead(), first)
	for i in WALK_TICKS:
		scene.step()
		control.step()
	var first_to := _distance(scene.lead(), first)
	var held_on: bool = scene.command(_drag(second, scene, MOUSE_BUTTON_MASK_RIGHT))
	var second_from := _distance(scene.lead(), second)
	for i in WALK_TICKS:
		scene.step()
		control.step()
	var second_to := _distance(scene.lead(), second)
	var release := _click(second, scene)
	release.pressed = false
	var released: bool = not scene.command(release)
	var after_release: bool = not scene.command(_drag(home, scene, 0))
	var stood: float = (control.lead() - control_from).length() / WorldAdapter.CELL_SIZE
	var setup := first != home and second != first and second_from > 0.0
	var passed := setup and pressed and first_to < first_from and held_on and second_to < second_from \
			and released and after_release and stood < 0.5
	print("play_test: right button on %s from %s ordered %s, %.0f to %.0f away in %d ticks; held and moved to %s ordered %s, %.0f to %.0f away; released %s, then moved to %s ordered nothing %s; control without input moved %.2f cells: %s" % [
		first, home, pressed, first_from, first_to, WALK_TICKS, second, held_on, second_from, second_to,
		released, home, after_release, stood, "pass" if passed else "FAIL"])
	scene.free()
	control.free()
	return passed


# Q holds the army's first skill, its targeted strike, and W its second, an area round the person
# that shares the army's cooldown. Setup: key 1 leaves the person striking with the first alone,
# which finds no foe at home, so its ready tick moves only for a cast. Cast on press: Q, with no one
# selected and no foe in reach, is refused as no target, its reason on the HUD, and the ready tick
# stays; W's press is accepted and the ready tick moves on the next tick; control, the same scene
# given no key keeps it. A second W at once is refused as on cooldown and E, a skill the person
# lacks, as no such skill, each with its reason on the HUD. Cast on release: Q's press casts nothing
# but shows the aim, the strike's range round the person and its area at the cursor, or at the ring
# for a cursor beyond it, and its release, with no target, casts nothing; W's press and release
# casts. W pressed again while aiming, or Escape, which then pauses nothing, cancels it, and the
# release casts nothing.
func _cast_pass() -> bool:
	var pressing := _cast_scene(false)
	var control := _cast_scene(false)
	var setup: bool = pressing.world.skill_count(pressing.steered) == 2 and _ready(pressing) == _ready(control)
	var ready_from := _ready(pressing)
	var notice: Label = pressing.get_node("Hud").labels.notice
	var untargeted: bool = not pressing.command(_key(KEY_Q)) and notice.text == "CAST_NO_TARGET" and notice.visible
	var cast: bool = pressing.command(_key(KEY_W))
	pressing.step()
	control.step()
	var cast_moved := _ready(pressing) > ready_from and _ready(control) == ready_from
	var cooling: bool = not pressing.command(_key(KEY_W)) and notice.text == "CAST_ON_COOLDOWN" and notice.visible
	var lacking: bool = not pressing.command(_key(KEY_E)) and notice.text == "CAST_NO_SKILL"
	var pressed := setup and untargeted and cast and cast_moved and cooling and lacking
	print("play_test: cast on press, Q with no target refused with its reason %s; W accepted %s and the ready tick moved %d to %d, the control's staying %s; W again refused as on cooldown with its reason %s; E refused as no skill %s (setup %s): %s" % [
		untargeted, cast, ready_from, _ready(pressing), _ready(control) == ready_from, cooling, lacking, setup,
		"pass" if pressed else "FAIL"])

	var releasing := _cast_scene(true)
	var aim: Node2D = releasing.get_node("Aim")
	var person: Vector2 = releasing.get_node("WorldArt").to_screen(releasing.lead())
	var reach: float = releasing.world.skills(releasing.steered)[0]["range"] * WorldArt.CELL.x \
			/ WorldAdapter.CELL_SIZE
	var held: bool = not releasing.command(_key(KEY_Q)) and releasing.aiming() == KEY_Q
	var shown: bool = aim.visible and aim.point == (person + CAST_OFFSET).floor() and aim.reach == reach
	releasing.command(_motion(person + FAR_OFFSET, 0))
	# The aim is drawn on whole pixels, so within a pixel of the ring.
	var at_ring: bool = absf(aim.point.distance_to(person.floor()) - reach) <= 1.0
	releasing.command(_motion(person + CAST_OFFSET, 0))
	var before := _ready(releasing)
	releasing.step()
	var nothing_on_press := _ready(releasing) == before
	var let_go_q := _key(KEY_Q)
	let_go_q.pressed = false
	var untargeted_release: bool = not releasing.command(let_go_q) and not aim.visible
	releasing.step()
	var nothing_untargeted := _ready(releasing) == before
	releasing.command(_key(KEY_W))
	var let_go := _key(KEY_W)
	let_go.pressed = false
	var released: bool = releasing.command(let_go) and not aim.visible
	releasing.step()
	var cast_on_release := _ready(releasing) > before

	var again := _cast_scene(true)
	again.command(_key(KEY_W))
	var cancelled_again: bool = not again.command(_key(KEY_W)) and again.aiming() == KEY_NONE \
			and not again.get_node("Aim").visible
	var again_released: bool = not again.command(let_go)
	var again_before := _ready(again)
	again.step()
	var again_still := _ready(again) == again_before

	var escaped := _cast_scene(true)
	var escape_hud: CanvasLayer = escaped.get_node("Hud")
	escaped.command(_key(KEY_W))
	var not_paused: bool = not escaped.navigate(_key(KEY_ESCAPE)) and escape_hud.screen() == null
	escaped.command(_key(KEY_ESCAPE))
	var escape_cancelled: bool = escaped.aiming() == KEY_NONE and not escaped.get_node("Aim").visible
	var escape_released: bool = not escaped.command(let_go)
	var escape_before := _ready(escaped)
	escaped.step()
	var escape_still := _ready(escaped) == escape_before
	var on_release := held and shown and at_ring and nothing_on_press and untargeted_release \
			and nothing_untargeted and released and cast_on_release and cancelled_again and again_released \
			and again_still and not_paused and escape_cancelled and escape_released and escape_still
	print("play_test: cast on release, Q's press aims %s, the aim at the cursor with range %.0f px %s and at the ring past it %s, nothing cast while held %s; its release with no target casts nothing %s %s; W's release casts %s and the ready tick moved %s; W again cancels %s, its release casts nothing %s %s; Escape pauses nothing %s, cancels %s, the release casts nothing %s %s: %s" % [
		held, reach, shown, at_ring, nothing_on_press, untargeted_release, nothing_untargeted, released,
		cast_on_release, cancelled_again, again_released, again_still, not_paused, escape_cancelled,
		escape_released, escape_still, "pass" if on_release else "FAIL"])
	for scene: Node2D in [pressing, control, releasing, again, escaped]:
		scene.free()
	return pressed and on_release


# The scene born into the first village, casting on release or on press as `on_release` says,
# striking with its first skill alone and the cursor CAST_OFFSET east of the person.
func _cast_scene(on_release: bool) -> Node2D:
	var scene := _scene(0)
	scene.get_node("Hud").settings.chosen.cast = 1 if on_release else 0
	scene.command(_key(KEY_1))
	scene.command(_motion(scene.get_node("WorldArt").to_screen(scene.lead()) + CAST_OFFSET, 0))
	return scene


# The tick from which the steered person may strike or cast again.
func _ready(scene: Node2D) -> int:
	return scene.world.body_ready(scene.steered)[0]


# The scene opens on the title with no world; Enter on a new game opens the new-game panel, and the
# digits typed and Enter start the world from that seed, closing both. Escape opens the pause
# panel on that seed, its guide showing the move button, the cast keys, the first and last skill
# keys and the key for all skills as the scene binds them, and no tick runs while it is open. Control: once resumed,
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
	var casts := "".join(scene.CAST_KEYS.map(func(keycode: Key) -> String: return OS.get_keycode_string(keycode)))
	var guided: bool = caps.has(skills) and caps.has(OS.get_keycode_string(scene.ALL_SKILLS_KEY)) \
			and caps.has("CONTROLS_CLICK") and caps.has(casts) and casts == "QWERT"
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


# A click on each row of the HUD's shortcut rail does what its key does: from the world with no
# panel open, the panel or screen a click on a row of SHORTCUTS leaves open is the row's own, and the
# one its key, pressed in the same state, leaves open; with the genealogy panel open, each row's
# click leaves open what its key does, so the genealogy row closes the panel and the sect row leaves
# it open. The settings, which no key opens, open as open_settings() opens them, no tick runs while
# they are open, and Escape goes back to the world, which runs again; opened from the pause panel,
# they go back to it. The cursor over a row marks it, and off the rail marks none. Control: a left
# click on the room's centre and one just left of the rail open nothing and give no order, and with
# the touch controls shown, the rail hidden, a click on a row opens nothing. Setup: the world is
# played, the rail shown with every row of SHORTCUTS, and nothing open before each click and key.
# The rail lights the row whose panel or screen is open, and shows each row's key and word as the
# pause panel's guide lists it, Esc for Escape, the settings' row, which has no key, unlisted.
func _shortcuts_pass() -> bool:
	var scene := _scene(0)
	var hud: CanvasLayer = scene.get_node("Hud")
	var rail: Control = hud.shortcuts
	var set_up: bool = scene.world != null and rail.visible and rail.rows.size() == scene.SHORTCUTS.size()
	var faults: Array[String] = []
	for state: String in ["", "genealogy"]:
		for n in scene.SHORTCUTS.size():
			var row: Dictionary = scene.SHORTCUTS[n]
			set_up = set_up and _open_from(scene, state) == state
			scene.click(_left(rail.row_rect(n).get_center()))
			var clicked := _open_now(scene)
			if rail.lit != clicked:
				faults.append("from \"%s\", %s's row clicked lit \"%s\" over \"%s\" open" % [state, row.action, rail.lit, clicked])
			set_up = set_up and _open_from(scene, state) == state
			if row.key == KEY_NONE:
				scene.open_settings()
			elif not scene.navigate(_key(row.key)):
				scene.command(_key(row.key))
			var keyed := _open_now(scene)
			if clicked != keyed or (state == "" and clicked != row.action):
				faults.append("from \"%s\", %s's row left \"%s\" open, its key \"%s\"" % [state, row.action, clicked, keyed])
	var guide: Array[Dictionary] = scene.controls()
	for n in scene.SHORTCUTS.size():
		var row: Dictionary = scene.SHORTCUTS[n]
		var word: String = hud.key_word(row.key) if row.key != KEY_NONE else ""
		var listed := guide.has({"keys": [word], "does": row.label})
		if rail.rows[n].key != word or rail.rows[n].label != row.label or listed != (row.key != KEY_NONE):
			faults.append("%s: the rail shows %s, the guide lists it %s" % [row.action, rail.rows[n], listed])
		if row.key == KEY_ESCAPE and word != "Esc":
			faults.append("%s: the rail and the guide show %s, not Esc" % [row.action, word])
	_open_from(scene, "")
	var span := 3.5 / WorldAdapter.TICKS_PER_SECOND
	scene.click(_left(rail.row_rect(scene.SHORTCUTS.find_custom(func(row: Dictionary) -> bool:
			return row.action == "settings")).get_center()))
	var opened: bool = hud.screen() == hud.settings
	var before: int = scene.ticks
	scene.advance(span)
	var held: bool = scene.ticks == before
	scene.navigate(_key(KEY_ESCAPE))
	var back_to_world: bool = hud.screen() == null
	scene.advance(span)
	var ran: bool = scene.ticks > before
	scene.navigate(_key(KEY_ESCAPE))
	for n in hud.pause.ENTRIES.find("settings"):
		scene.navigate(_key(KEY_DOWN))
	scene.navigate(_key(KEY_ENTER))
	var from_pause: bool = hud.screen() == hud.settings
	scene.navigate(_key(KEY_ESCAPE))
	var back_to_pause: bool = hud.screen() == hud.pause
	_open_from(scene, "")
	scene.click(_motion(rail.row_rect(2).get_center(), 0))
	var marked: bool = rail.hovered == 2
	scene.click(_motion(hud.room / 2.0, 0))
	var unmarked: bool = rail.hovered == -1
	var objective: Vector2i = scene.world.objective(scene.steered)
	var elsewhere := []
	for at: Vector2 in [hud.room / 2.0, rail.rect().position + Vector2(-2, 8)]:
		elsewhere.append(scene.click(_left(at)) or scene.command(_left(at)) or _open_now(scene) != "")
	var no_order: bool = scene.world.objective(scene.steered) == objective
	hud.use_touch(true)
	var hidden_taken: bool = scene.click(_left(rail.row_rect(0).get_center())) or _open_now(scene) != ""
	scene.free()
	var passed := set_up and faults.is_empty() and opened and held and back_to_world and ran and from_pause \
			and back_to_pause and marked and unmarked and elsewhere == [false, false] and no_order and not hidden_taken
	print("play_test: each shortcut row clicked against its key, from the world and from the genealogy panel: faults %s; the settings row opens them %s, no tick while open %s, Escape back to the world %s, which runs %s; from the pause panel %s, back to it %s; the cursor marks a row %s, off it none %s; control: clicks on the centre and beside the rail took %s, no order %s, a row with the rail hidden taken %s (setup %s): %s" % [
		faults, opened, held, back_to_world, ran, from_pause, back_to_pause, marked, unmarked, elsewhere, no_order,
		hidden_taken, set_up, "pass" if passed else "FAIL"])
	return passed


# Closes every panel and screen over `scene`'s world, then opens the panel `state` names by its key,
# or none for "". Returns what is open then, as _open_now names it.
func _open_from(scene: Node2D, state: String) -> String:
	var hud: CanvasLayer = scene.get_node("Hud")
	for over: Control in [hud.genealogy, hud.sect, hud.factions, hud.arts, hud.map_window, hud.dialogue, hud.settings,
			hud.pause]:
		over.visible = false
	for row: Dictionary in scene.SHORTCUTS:
		if row.action == state:
			scene.command(_key(row.key))
	return _open_now(scene)


# The action of the row of SHORTCUTS whose panel or screen is open over `scene`'s world, the screen
# first, or "" while none is, or the open one's script where no row names it.
func _open_now(scene: Node2D) -> String:
	var hud: CanvasLayer = scene.get_node("Hud")
	var open: Control = hud.screen() if hud.screen() != null else hud.panel()
	if open == null:
		return ""
	for row: Dictionary in scene.SHORTCUTS:
		if hud.get(row.action) == open:
			return row.action
	return open.get_script().resource_path


# A left click at `at`, in the HUD's pixels.
func _left(at: Vector2) -> InputEventMouseButton:
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	click.position = at
	return click


# A new game saved from the pause panel, which says so, is resumed by Continue on a fresh title, as
# leaving for the title reloads the scene: the same hash, seed and steered person, the title closed.
# Control: with no save, the title opens with Continue unavailable, and down from a new game
# passes over it. A slot of bytes no save begins with leaves Continue available, and taking it
# starts no world and names why on the title. A world whose growth formed armies after the
# player's continues steering the army the player's soul serves in, not the last. Setup: the world
# ran before it was saved, so a fresh world from the same seed has another hash, and the grown
# world has an army after the player's.
func _save_pass() -> bool:
	var game_slot := TestSaves.game_slot()
	TestSaves.clear()
	var bare := _title()
	var bare_hud: CanvasLayer = bare.get_node("Hud")
	bare.navigate(_key(KEY_DOWN))
	var passed_over: bool = bare_hud.title_screen.unavailable.has("continue") \
			and bare_hud.title_screen.ENTRIES[bare_hud.title_screen.chosen] == "settings"
	bare.free()

	var scene := _scene(0)
	for i in TICKS:
		scene.step()
	var hud: CanvasLayer = scene.get_node("Hud")
	scene.navigate(_key(scene.PAUSE_KEY))
	scene.navigate(_key(KEY_DOWN))
	var chose_save: bool = hud.pause.ENTRIES[hud.pause.chosen] == "save"
	scene.navigate(_key(KEY_ENTER))
	var noted: bool = hud.pause.notice == "PAUSE_SAVED" and hud.screen() == hud.pause
	var saved_hash: int = scene.world.hash()
	var saved_person: int = scene.world.roster(scene.steered)[0]
	var ran := saved_hash != WorldAdapter.create_region(SEED, 0).hash()
	scene.free()

	var resumed := _title()
	var resumed_hud: CanvasLayer = resumed.get_node("Hud")
	resumed.navigate(_key(KEY_DOWN))
	var on_continue: bool = resumed_hud.title_screen.ENTRIES[resumed_hud.title_screen.chosen] == "continue"
	resumed.navigate(_key(KEY_ENTER))
	var same: bool = resumed.world != null and resumed.world.hash() == saved_hash \
			and resumed.world_seed == SEED and resumed_hud.screen() == null \
			and resumed.world.roster(resumed.steered)[0] == saved_person
	resumed.free()

	var file := FileAccess.open(TestSaves.path(0), FileAccess.WRITE)
	file.store_string("not a save")
	file.close()
	var broken := _title()
	var broken_hud: CanvasLayer = broken.get_node("Hud")
	broken.navigate(_key(KEY_DOWN))
	broken.navigate(_key(KEY_ENTER))
	var refused: bool = broken.world == null and broken_hud.screen() == broken_hud.title_screen \
			and broken_hud.title_screen.notice == "TITLE_LOAD_NOT_A_SAVE"
	broken.free()

	# A world whose growth formed armies after the player's, so the last army is not the player's.
	var grown := WorldAdapter.create_region(GROWN_SEED, 0, GROWN_TICKS_PER_DAY)
	for i in GROWN_TICKS:
		grown.tick()
	var player := grown.player_army()
	var grown_past := player >= 0 and grown.army_count() > player + 1 and grown.save(0, TestSaves.directory)
	var continued := _title()
	continued.navigate(_key(KEY_DOWN))
	continued.navigate(_key(KEY_ENTER))
	var found: bool = continued.world != null and continued.steered == player \
			and continued.world.roster(continued.steered) == grown.roster(player)
	continued.free()
	TestSaves.clear()
	# Every save above went to the tests' own directory: the game's slot 0 is as it was.
	var untouched := TestSaves.game_slot() == game_slot
	var passed := passed_over and chose_save and noted and ran and on_continue and same and refused \
			and grown_past and found and untouched
	print("play_test: with no save, Continue passed over %s; saved from the pause panel %s with its notice %s, after ticks that changed the world %s; on a fresh title Continue chosen %s resumes the same hash, seed %d and person %d %s; a broken save refused on the title with its reason %s; a world grown past the player's army %d saved %s, and Continue steers that army %s; the game's slot 0 untouched %s: %s" % [
		passed_over, chose_save, noted, ran, on_continue, SEED, saved_person, same, refused, player,
		grown_past, found, untouched, "pass" if passed else "FAIL"])
	return passed


# The world is saved on the tick the AUTOSAVE_DAYS-th day begins, and the save loads as the world
# then is. Control: on every tick before it the slot holds no save.
func _autosave_pass() -> bool:
	var game_slot := TestSaves.game_slot()
	TestSaves.clear()
	var scene := _scene(0)
	var every: int = scene.AUTOSAVE_DAYS
	var date: Dictionary = scene.world.date()
	var days := 0
	var early := false
	while not FileAccess.file_exists(TestSaves.path(0)) and days <= every:
		scene.step()
		if scene.world.date() != date:
			date = scene.world.date()
			days += 1
		early = early or (days < every and FileAccess.file_exists(TestSaves.path(0)))
	var loaded := WorldAdapter.load(scene.SAVE_SLOT, TestSaves.directory)
	var saved: bool = loaded != null and loaded.hash() == scene.world.hash()
	var ticks: int = scene.ticks
	scene.free()
	TestSaves.clear()
	var untouched := TestSaves.game_slot() == game_slot
	var passed := days == every and not early and saved and untouched
	print("play_test: autosaved as day %d began, after %d ticks, every %d days %s; none earlier %s; it loads as the world then was %s; the game's slot 0 untouched %s: %s" % [
		days, ticks, every, days == every, not early, saved, untouched,
		"pass" if passed else "FAIL"])
	return passed


# The borders follow the territory as it moves, and the map follows the player's chart (ADR 0073).
# Setup: a tick with no order moves no territory, and the tick after the march is ordered finds it
# moved. Before and after, the borders match the territory the core gives then, and the map's
# pixels the territory as the player remembers it, its borders against seen cells only, in fog where
# it has seen nothing; control: the
# territory after no longer matches the borders before, so the match after is the redraw's. The
# map is drawn again exactly when the chart's revision moves, and frames after keep it.
func _territory_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var art: Node2D = scene.get_node("WorldArt")
	var hud: CanvasLayer = scene.get_node("Hud")
	var territory: Node2D = art.get_node("Territory")
	# The player sees its surroundings on the first tick, and the HUD draws them on the next frame.
	scene.step()
	_frame(scene)
	var before := world.territory()
	var borders_before: Array = territory.borders
	var map_before: ImageTexture = hud.map_texture
	var revision_before := world.chart_revision()
	var drawn_before := _borders_follow(borders_before, before, world) and _map_follows(hud, world)
	scene.step()
	_frame(scene)
	var unchanged_tick: bool = world.territory() == before and is_same(territory.borders, borders_before) \
			and is_same(hud.map_texture, map_before) == (world.chart_revision() == revision_before)
	map_before = hud.map_texture
	revision_before = world.chart_revision()
	var target: Dictionary = world.villages()[MARCHED_ON]
	var ordered := world.order(MARCHERS, target.corner + target.area / 2,
			PackedInt64Array(range(world.skill_count(MARCHERS))))
	scene.step()
	_frame(scene)
	var after := world.territory()
	var moved := after != before
	var borders_after: Array = territory.borders
	var map_after: ImageTexture = hud.map_texture
	var redrawn_on_revision := is_same(map_after, map_before) == (world.chart_revision() == revision_before)
	for frame in 3:
		_frame(scene)
	var frames_kept: bool = is_same(territory.borders, borders_after) and is_same(hud.map_texture, map_after) \
			and redrawn_on_revision
	var drawn_after := _borders_follow(borders_after, after, world) and _map_follows(hud, world)
	var old_stale := not _borders_follow(borders_before, after, world)
	var passed := unchanged_tick and ordered and moved and drawn_before and drawn_after \
			and old_stale and frames_kept
	print("play_test: a tick with no order keeps the territory and borders, and redraws the map only on a new chart revision %s; army %d ordered on village %d %s, the territory moved by the next tick %s; borders follow the territory and the map the chart before %s and after %s, the borders before stale after %s; kept over frames %s: %s" % [
		unchanged_tick, MARCHERS, MARCHED_ON, ordered, moved, drawn_before, drawn_after,
		old_stale, frames_kept, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# Walked up to the nearest person of a faction allied to its own, the person is prompted to talk to it
# and C opens the dialogue window on it, where a digit gives no order, Enter lists its own news
# and Escape goes back to the menu and then ends the talk, without pausing. A left click on the ally
# opens the window too, and selects no one, and so does a tap on it with the touch controls shown.
# With the window open again, a tick leaves it open; once the person's faction's leader declares war
# on the other's, the next tick closes it, the prompt shows the refusal and C opens nothing.
func _talk_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var player: int = world.roster(scene.steered)[0]
	var own := world.army_faction(scene.steered)
	var ally := -1
	var ally_army := -1
	for army in world.army_count():
		var theirs := world.army_faction(army)
		for person in world.roster(army):
			if world.relation(own, theirs) == WorldAdapter.RELATION_ALLY and (ally < 0 or _apart(world, player, person)
					< _apart(world, player, ally)):
				ally = person
				ally_army = army
	if ally < 0:
		print("play_test: no person of an allied faction: FAIL")
		scene.free()
		return false
	# The ally waits where it stands while the person walks up to it.
	world.steer(ally_army, true)
	var waiting_at := Vector2i((world.person_position(ally) / WorldAdapter.CELL_SIZE).floor())
	world.order(ally_army, waiting_at, PackedInt64Array(range(world.skill_count(ally_army))))
	var ticks := 0
	while ticks < TALK_WALK_LIMIT and scene.talk_target().person != ally:
		if ticks % 20 == 0:
			world.order(scene.steered, waiting_at, PackedInt64Array(range(world.skill_count(scene.steered))))
		scene.step()
		ticks += 1
	scene.show_talk()
	var prompt: Control = hud.talk_prompt
	var dialogue: Control = hud.dialogue
	var prompted: bool = scene.talk_to == ally and prompt.visible and prompt.person == ally and not prompt.refused
	var opened: bool = scene.command(_key(scene.TALK_KEY)) and dialogue.visible and dialogue.person == ally
	var digit_taken: bool = not scene.command(_key(KEY_1))
	var news_shown: bool = scene.command(_key(KEY_ENTER)) and dialogue.showing_news \
			and dialogue.news.size() == world.rumours(ally).size()
	var not_paused: bool = not scene.navigate(_key(KEY_ESCAPE)) and hud.screen() == null
	var back: bool = scene.command(_key(KEY_ESCAPE)) and dialogue.visible and not dialogue.showing_news
	var ended: bool = scene.command(_key(KEY_ESCAPE)) and not dialogue.visible
	var clicked: bool = scene.command(_left(_pixels_of(scene, ally))) and dialogue.visible \
			and dialogue.person == ally and scene.selected < 0
	var closed_first: bool = scene.command(_key(KEY_ESCAPE)) and not dialogue.visible
	# With the touch controls shown, a tap where the ally stands, in the HUD's pixels, opens it too.
	hud.use_touch(true)
	var on_hud: Vector2 = scene.to_hud_pixels(_pixels_of(scene, ally))
	scene.touch(_finger(0, on_hud, true))
	scene.touch(_finger(0, on_hud, false))
	var tapped: bool = closed_first and dialogue.visible and dialogue.person == ally and scene.selected < 0
	hud.use_touch(false)
	# Control: a tick with nothing changed leaves the window open.
	scene.step()
	scene.show_talk()
	var kept: bool = dialogue.visible and dialogue.person == ally
	var leader := world.leader(own)
	var declared := leader >= 0 and world.declare(leader, world.army_faction(ally_army), WorldAdapter.DECLARATION_WAR)
	scene.step()
	scene.show_talk()
	var closed: bool = not dialogue.visible and world.talk_refusal(player, ally) == WorldAdapter.TALK_HOSTILE
	var refused: bool = scene.talk_to == ally and scene.talk_hostile and prompt.visible and prompt.refused
	var nothing_opened: bool = not scene.command(_key(scene.TALK_KEY)) and not dialogue.visible
	var passed := prompted and opened and digit_taken and news_shown and not_paused and back and ended and clicked \
			and tapped and kept and declared and closed and refused and nothing_opened
	print("play_test: walked %d ticks up to person %d of an allied faction, prompted %s; C opened the window %s, a digit taken %s, Enter its %d rumours %s, Escape no pause %s, back %s, ended %s; a left click on it opened it, selecting no one, %s, and so did a tap with the touch controls shown %s; a tick kept it open %s; war declared %s closed it %s, the prompt refused %s, C opened nothing %s: %s" % [
		ticks, ally, prompted, opened, digit_taken, world.rumours(ally).size(), news_shown, not_paused, back, ended,
		clicked, tapped, kept, declared, closed, refused, nothing_opened, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# Walked up to the nearest person of a faction not hostile to its own who sells the herb, the steered
# person opens the dialogue on it with C, chooses its wares, and buys a herb with Enter, paying
# its price, the purse following; Enter on the pill it cannot pay for is refused, nothing paid; Escape
# goes back to the menu and then ends the talk. Z at full health is refused as full and X, holding no
# pill, as none held, each noticed, nothing consumed; control: a digit, the window closed, orders.
func _trade_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var player: int = world.roster(scene.steered)[0]
	var own := world.army_faction(scene.steered)
	var seller := -1
	var seller_army := -1
	for army in world.army_count():
		var relation := world.relation(own, world.army_faction(army))
		if relation == WorldAdapter.RELATION_HOSTILE or relation == WorldAdapter.RELATION_AT_WAR:
			continue
		for person in world.roster(army):
			if person != player and world.sells(person, 0) \
					and (seller < 0 or _apart(world, player, person) < _apart(world, player, seller)):
				seller = person
				seller_army = army
	if seller < 0:
		print("play_test: no seller of a faction not hostile: FAIL")
		scene.free()
		return false
	world.steer(seller_army, true)
	var waiting_at := Vector2i((world.person_position(seller) / WorldAdapter.CELL_SIZE).floor())
	world.order(seller_army, waiting_at, PackedInt64Array(range(world.skill_count(seller_army))))
	var ticks := 0
	while ticks < TALK_WALK_LIMIT and world.talk_refusal(player, seller) != WorldAdapter.TALK_NONE:
		if ticks % 20 == 0:
			world.order(scene.steered, waiting_at, PackedInt64Array(range(world.skill_count(scene.steered))))
		scene.step()
		ticks += 1
	# The prompt names the nearest person; the seller is named here, within the reach.
	scene.talk_to = seller
	var dialogue: Control = hud.dialogue
	var silver := world.person_silver(player)
	var opened: bool = scene.command(_key(scene.TALK_KEY)) and dialogue.rows.has(WorldAdapter.OFFER_TRADE)
	scene.command(_key(KEY_DOWN))
	var wares_shown: bool = scene.command(_key(KEY_ENTER)) and dialogue.trading and dialogue.buyer == player
	var bought: bool = scene.command(_key(KEY_ENTER)) and world.person_silver(player) == silver - world.wares()[0].price \
			and world.person_held(player) == PackedInt64Array([1, 0]) and not dialogue.refused \
			and hud.purse.silver == world.person_silver(player)
	scene.command(_key(KEY_DOWN))
	var dear: bool = not scene.command(_key(KEY_ENTER)) and dialogue.refused and dialogue.line == hud.tr("BUY_NO_SILVER") \
			and world.person_silver(player) == silver - world.wares()[0].price
	var back: bool = scene.command(_key(KEY_ESCAPE)) and dialogue.visible and not dialogue.trading
	var ended: bool = scene.command(_key(KEY_ESCAPE)) and not dialogue.visible
	var full: bool = not scene.command(_key(KEY_Z)) and world.person_held(player)[0] == 1 \
			and hud.labels.notice.text == "CONSUME_FULL_HEALTH"
	var none_held: bool = not scene.command(_key(KEY_X)) \
			and hud.labels.notice.text == hud.tr("CONSUME_NONE_HELD") % hud.dialogue.ware_name(world.wares()[1])
	var digit: bool = scene.command(_key(KEY_1))
	var passed := opened and wares_shown and bought and dear and back and ended and full and none_held and digit
	print("play_test: walked %d ticks up to seller %d; C opened its menu with wares %s; Enter showed them %s; bought a herb %s, %d silver left; the pill refused %s; back %s, ended %s; Z at full health refused %s, X with no pill %s; a digit then ordered %s: %s" % [
		ticks, seller, opened, wares_shown, bought, world.person_silver(player), dear, back, ended, full, none_held,
		digit, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# How many cells apart two people stand, along the farther axis, as the core measures a reach.
func _apart(world: WorldAdapter, one: int, other: int) -> int:
	var apart := (Vector2i((world.person_position(one) / WorldAdapter.CELL_SIZE).floor())
			- Vector2i((world.person_position(other) / WorldAdapter.CELL_SIZE).floor())).abs()
	return maxi(apart.x, apart.y)


# Draws a frame as the scene does after its ticks, over the view around the steered person.
func _frame(scene: Node2D) -> void:
	var view := Vector2(480, 270)
	scene.get_node("WorldArt").refresh(Rect2(scene.get_node("WorldArt").to_screen(scene.lead()) - view / 2, view))
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
		var cell := Vector2i((border[0] / WorldArt.CELL).round())
		var vertical: bool = border[0].x == border[1].x
		var first := cell - (Vector2i(1, 0) if vertical else Vector2i(0, 1))
		if border[2] != owners[first.y * width + first.x] or border[3] != owners[cell.y * width + cell.x] \
				or border[2] == border[3]:
			return false
	return borders.size() == edges and edges > 0


# Whether the HUD's map shows `world`'s chart: apart from the squares of the villages it remembers,
# clipped to seen cells, fog on every cell the player has not seen, and on the rest a faction's accent
# exactly where the remembered territory has a border with a seen neighbour.
func _map_follows(hud: CanvasLayer, world: WorldAdapter) -> bool:
	var image: Image = hud.map_texture.get_image()
	var width := world.map_rows()[0].length()
	var owners := world.remembered_territory()
	var seen := world.explored()
	var fog := preload("res://hud/map_window.gd").FOG.to_html(false)
	var accents := []
	for accent: Color in preload("res://hud/palette.gd").ACCENTS:
		accents.append(accent.to_html(false))
	var squares: Array[Rect2i] = []
	for village: Dictionary in world.remembered_villages():
		squares.append(Rect2i(village.corner, village.area))
	if seen.size() != owners.size() or not seen.has(1):
		return false
	for i in owners.size():
		var cell := Vector2i(i % width, i / width)
		if seen[i] == 0:
			if image.get_pixel(cell.x, cell.y).to_html(false) != fog:
				return false
			continue
		if squares.any(func(square: Rect2i) -> bool: return square.has_point(cell)):
			continue
		var border := ((i + 1) % width != 0 and seen[i + 1] != 0 and owners[i] != owners[i + 1]) \
				or (i + width < owners.size() and seen[i + width] != 0 and owners[i] != owners[i + width])
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


# Walked up to the nearest person of its faction, the steered person sees the names of the people
# world_art.gd finds within its reach, at most NAME_CAP, each one of them, and not its own, with the
# person the talk prompt names left out while the prompt shows; G's panel open, no name shows.
# Control: with the panel closed again, the names show again. Setup: people stand within the reach
# and the prompt names one.
func _names_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var art: Node2D = scene.get_node("WorldArt")
	# The person walks up to the nearest other person of its faction, who waits for it, until the
	# prompt may name someone.
	var player: int = world.roster(scene.steered)[0]
	var nearest := -1
	var waiting := -1
	for army in world.army_count():
		for person in world.roster(army):
			if person != player and world.army_faction(army) == world.army_faction(scene.steered) \
					and (nearest < 0 or _apart(world, player, person) < _apart(world, player, nearest)):
				nearest = person
				waiting = army
	if nearest >= 0:
		world.steer(waiting, true)
		var at := Vector2i((world.person_position(nearest) / WorldAdapter.CELL_SIZE).floor())
		world.order(waiting, at, PackedInt64Array(range(world.skill_count(waiting))))
		var ticks := 0
		while ticks < TALK_WALK_LIMIT and scene.talk_target().person < 0:
			if ticks % 20 == 0:
				world.order(scene.steered, at, PackedInt64Array(range(world.skill_count(scene.steered))))
			scene.step()
			ticks += 1
	var window := Vector2i(1920, 1080)
	var zoom := ViewScale.world_scale(window, false)
	scene.get_node("Camera").zoom = Vector2(zoom, zoom)
	hud.fit(window, false)
	var view := Rect2(art.to_screen(scene.lead()) - Vector2(window) / zoom / 2.0, Vector2(window) / zoom)
	scene.show_view(view)
	scene.show_talk()
	scene.show_names()
	var near: Array = Array(art.near.person)
	var named: Array = hud.nameplates.plates.map(func(plate: Dictionary) -> int: return plate.person)
	var setup: bool = not near.is_empty() and scene.talk_to >= 0 and hud.talk_prompt.visible
	var within := named.all(func(person: int) -> bool: return near.has(person))
	var shown: bool = not named.is_empty() and named.size() <= hud.nameplates.NAME_CAP \
			and not named.has(world.roster(scene.steered)[0]) and not named.has(scene.talk_to)
	scene.command(_key(KEY_G))
	scene.show_names()
	var hidden: bool = hud.genealogy.visible and hud.nameplates.plates.is_empty()
	scene.command(_key(KEY_G))
	scene.show_names()
	var back: bool = not hud.genealogy.visible and hud.nameplates.plates.size() == named.size()
	var passed := setup and within and shown and hidden and back
	print("play_test: %d people within reach, %d named %s, all of them within %s, neither the player nor person %d the prompt names %s; G's panel hides them %s, closed shows them %s (setup %s): %s" % [
		near.size(), named.size(), named, within, scene.talk_to, shown, hidden, back, setup, "pass" if passed else "FAIL"])
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


# Key C alone challenges no one: the HUD's "C 도전" line is gone. F opens the faction window, where
# C from an Orthodox village (the region's first, owned by faction 0) is refused with its reason's
# word in the window, and from a Cult one (its second, owned by faction 1) begins the duel and
# closes the window; asked again there, it is refused as the duel is fought. Control: the Orthodox
# refusal shows its reason, so the Cult's acceptance is the duel rule's and not the key's.
func _challenge_pass() -> bool:
	var orthodox := _scene(0)
	var panel: Control = orthodox.get_node("Hud").factions
	var alone: bool = not orthodox.command(_key(KEY_C)) and orthodox.world.duel(0).is_empty()
	var opened: bool = orthodox.command(_key(KEY_F)) and panel.visible
	var refused: bool = not orthodox.command(_key(KEY_C))
	var reason: String = panel.verdict
	orthodox.free()

	var cult := _scene(1)
	var world: WorldAdapter = cult.world
	var hud: CanvasLayer = cult.get_node("Hud")
	var faction := world.army_faction(cult.steered)
	var leader_before := world.leader(faction)
	cult.command(_key(KEY_F))
	var accepted: bool = cult.command(_key(KEY_C)) and not hud.factions.visible
	hud.refresh(world, cult.steered, PackedInt64Array([0]))
	var shown: bool = hud.duel_panel.visible and not world.duel(faction).is_empty()
	cult.step()
	cult.command(_key(KEY_F))
	var again: bool = not cult.command(_key(KEY_C))
	var again_reason: String = hud.factions.verdict
	var passed := alone and opened and refused and reason == "CHALLENGE_NO_SUCCESSION" and accepted and shown \
			and again and again_reason == "CHALLENGE_DUEL_IN_PROGRESS" and world.leader(faction) == leader_before
	print("play_test: key C alone challenges no one %s; F opens the faction window %s, where C from an Orthodox village is refused %s, the window saying %s; from a Cult village accepted, closing it, %s, the duel shown %s; again refused %s, the window saying %s: %s" % [
		alone, opened, refused, reason, accepted, shown, again, again_reason, "pass" if passed else "FAIL"])
	cult.free()
	return passed


# In the faction window, a person who does not lead its faction gives no order: Enter declares
# nothing. Control: steering its faction's leader, Enter asks war on an ally again, Escape takes it
# back with nothing declared, and Enter twice declares it, the core then holding the war, declared by
# the leader's faction; while the window is open a digit gives no order, and F closes it.
func _factions_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var panel: Control = scene.get_node("Hud").factions
	var own := world.army_faction(scene.steered)
	scene.command(_key(KEY_F))
	var ally := -1
	for n in range(1, panel.rows.size()):
		if panel.rows[n].relation == WorldAdapter.RELATION_ALLY and ally < 0:
			ally = n
	while panel.chosen < ally:
		scene.command(_key(KEY_DOWN))
	var target: int = panel.chosen_faction()
	scene.command(_key(KEY_ENTER))
	scene.command(_key(KEY_ENTER))
	var member_none: bool = not panel.leads and world.declaration(own, target).is_empty() and panel.confirming == -1
	scene.command(_key(KEY_F))

	for army in world.army_count():
		var roster := world.roster(army)
		if not roster.is_empty() and roster[0] == world.leader(own):
			scene.steered = army
	scene.command(_key(KEY_F))
	while panel.chosen < ally:
		scene.command(_key(KEY_DOWN))
	scene.command(_key(KEY_ENTER))
	var asked: bool = panel.leads and panel.confirming == WorldAdapter.DECLARATION_WAR
	scene.command(_key(KEY_ESCAPE))
	var taken_back: bool = panel.visible and panel.confirming == -1 and world.declaration(own, target).is_empty()
	var digit_taken: bool = not scene.command(_key(KEY_1))
	scene.command(_key(KEY_ENTER))
	scene.command(_key(KEY_ENTER))
	var made := world.declaration(own, target)
	var declared: bool = world.relation(own, target) == WorldAdapter.RELATION_AT_WAR and made.get("by", -1) == own
	var closed: bool = scene.command(_key(KEY_F)) and not panel.visible
	var passed := ally > 0 and member_none and asked and taken_back and digit_taken and declared and closed
	print("play_test: in the faction window a member declares nothing %s; its leader is asked war on ally %d again %s, Escape takes it back %s, a digit gives no order %s, Enter twice declares it %s, F closes it %s: %s" % [
		member_none, target, asked, taken_back, digit_taken, declared, closed, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# K opens the arts panel on the steered person, the first of its skills chosen, and while it is
# open a digit gives no order and Escape pauses nothing; the arrow down chooses the next skill, and
# Q puts it on the first slot, the HUD's first slot showing it; Delete takes it off, the slot then
# showing none, and Escape closes the panel. Control: with it closed, the same digit orders.
func _arts_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var panel: Control = hud.arts
	var person: int = world.roster(scene.steered)[0]
	var skills := world.person_skills(person)
	var opened: bool = scene.command(_key(scene.ARTS_KEY)) and panel.visible \
			and panel.selected == {"art": -1, "skill": 0}
	var digit_taken: bool = not scene.command(_key(KEY_1))
	var not_paused: bool = not scene.navigate(_key(KEY_ESCAPE)) and hud.screen() == null
	var moved: bool = scene.command(_key(KEY_DOWN)) and panel.selected == {"art": -1, "skill": 1}
	var assigned: bool = scene.command(_key(KEY_Q)) and hud.loadout.slots[0] == {"art": -1, "skill": 1} \
			and hud.loadout.slots[1].is_empty() and hud.slot_skills[0] == skills[1]
	var cleared: bool = scene.command(_key(scene.CLEAR_KEY)) and hud.loadout.slots[0].is_empty() \
			and hud.slot_skills[0].is_empty() and hud.slot_icons[0].texture == null
	var closed: bool = scene.command(_key(KEY_ESCAPE)) and not panel.visible
	var digit_orders: bool = scene.command(_key(KEY_1))
	var passed := skills.size() == 2 and opened and digit_taken and not_paused and moved and assigned \
			and cleared and closed and digit_orders
	print("play_test: K opens the arts panel on the first skill %s, a digit gives no order %s and Escape pauses nothing %s while open; down chooses the second %s, Q puts it on the first slot, leaving its own %s, Delete takes it off %s; Escape closes it %s, then the digit orders %s: %s" % [
		opened, digit_taken, not_paused, moved, assigned, cleared, closed, digit_orders, "pass" if passed else "FAIL"])
	scene.free()
	return passed


# In a region of five-tick days lived ten years, steering the first person who knows an art skill
# beyond its army's skills, with that skill put on T from the arts panel: T's slot on the technique
# bar shows it, its key's cap reads T, T held to aim shows its range, and the fifth skill button
# dragged to the pad's edge casts it at its full range, and tapped casts it at whom the aim finds
# within that range or at the point that far the way the person faces. Control: R's slot, empty,
# shows and casts nothing, from the key and from the button, and Q held aims at its army skill's
# range, which differs. Setup: the art skill's range differs from the army skill's, and the person
# may cast it.
func _art_slot_pass() -> bool:
	var lived := WorldAdapter.create_region(SEED, 0, GROWN_TICKS_PER_DAY)
	for i in 10 * 120 * GROWN_TICKS_PER_DAY:
		lived.tick()
	var army := -1
	for each in lived.army_count():
		var roster := lived.roster(each)
		if army < 0 and not roster.is_empty() and lived.person_skills(roster[0]).size() > lived.skills(each).size():
			army = each
	if army < 0:
		print("play_test: no person knows an art after ten years: FAIL")
		return false
	var scene := _title()
	var hud: CanvasLayer = scene.get_node("Hud")
	# The scene plays the lived region as Continue resumes it, then steers that person.
	TestSaves.clear()
	if not lived.save(scene.SAVE_SLOT, TestSaves.directory) or not scene.resume_saved():
		print("play_test: the region lived ten years was not saved and resumed: FAIL")
		scene.free()
		return false
	lived = scene.world
	scene.steered = army
	hud.show_player(lived, army)
	hud.settings.chosen.cast = 1
	var person: int = lived.roster(army)[0]
	var skills := lived.person_skills(person)
	var art_skill := lived.skills(army).size()
	var held := {"art": int(skills[art_skill].art), "skill": int(skills[art_skill].skill)}
	scene.command(_key(scene.ARTS_KEY))
	while hud.arts.selected != held and scene.command(_key(KEY_DOWN)):
		pass
	scene.command(_key(KEY_T))
	scene.command(_key(scene.ARTS_KEY))
	hud.refresh(lived, army, PackedInt64Array([0]))
	var key: Label = hud.slot_icons[4].get_parent().get_child(1)
	var shown: bool = hud.slot_skills[4] == skills[art_skill] and hud.slot_icons[4].texture != null \
			and key.text == OS.get_keycode_string(scene.CAST_KEYS[4])
	var pixels: float = WorldArt.CELL.x / WorldAdapter.CELL_SIZE
	var art_reach: float = skills[art_skill].range * pixels
	var army_reach: float = skills[0].range * pixels
	var aim: Node2D = scene.get_node("Aim")
	scene.command(_motion(scene.get_node("WorldArt").to_screen(scene.lead()) + CAST_OFFSET, 0))
	scene.command(_key(KEY_T))
	var aimed: bool = aim.visible and aim.reach == art_reach
	scene.command(_key(KEY_T))
	scene.command(_key(KEY_Q))
	var army_aimed: bool = aim.visible and aim.reach == army_reach
	scene.command(_key(KEY_Q))
	var empty_slot: bool = hud.slot_skills[3].is_empty() and hud.slot_icons[3].texture == null
	scene.last_cast = {}
	var r_refused: bool = scene.cast_at(3, scene.lead() * pixels + CAST_OFFSET) == WorldAdapter.CAST_NO_SKILL \
			and scene.last_cast.is_empty()
	hud.use_touch(true)
	var touch: Control = hud.touch
	var fifth: Vector2 = touch.SKILLS[4].at
	var at: Vector2 = lived.person_position(person)
	var ready := lived.cast_refusal(person, art_skill, null) == WorldAdapter.CAST_NONE
	scene.touch(_finger(1, fifth, true))
	scene.touch(_slide(1, fifth + Vector2(touch.SLOP + 4, 0)))
	scene.touch(_slide(1, fifth + Vector2(touch.PAD, 0)))
	scene.touch(_finger(1, fifth + Vector2(touch.PAD, 0), false))
	var dragged: Dictionary = scene.last_cast
	var reached: bool = dragged == {"skill": art_skill, "target": (at + Vector2(skills[art_skill].range, 0)).floor()}
	# Tapped, it casts at whom the core's aim finds the way the person faces, within the art's range,
	# or at the point that way at that range.
	scene.last_cast = {}
	scene.touch(_finger(1, fifth, true))
	scene.touch(_finger(1, fifth, false))
	var tapped: Dictionary = scene.last_cast
	var reach := float(skills[art_skill].range)
	var tap_target: Variant = tapped.get("target", null)
	var tap_reached: bool = tapped.get("skill", -1) == art_skill and (
			(tap_target is Vector2 and absf(at.distance_to(tap_target) - reach) <= 2.0)
			or (tap_target is int and lived.person_position(tap_target).distance_to(at) <= reach))
	reached = reached and tap_reached
	scene.last_cast = {}
	scene.touch(_finger(2, touch.SKILLS[3].at, true))
	scene.touch(_finger(2, touch.SKILLS[3].at, false))
	var button_empty: bool = scene.last_cast.is_empty() and touch.reach[3].is_empty() and touch.skill_count == 5
	var set_up: bool = art_reach != army_reach and ready
	var passed := set_up and shown and aimed and army_aimed and empty_slot and r_refused and reached and button_empty
	print("play_test: army %d's person %d knows %d skills past its army's %d; the art skill %s on T shown %s; T aims at %.0f px %s, Q at %.0f px %s; R's empty slot shows none %s, casts none %s; the fifth button dragged cast %s and tapped %s at range %d %s; the fourth, empty, cast none %s (setup %s): %s" % [
		army, person, skills.size() - art_skill, art_skill, held, shown, art_reach, aimed, army_reach, army_aimed,
		empty_slot, r_refused, dragged, tapped, skills[art_skill].range, reached, button_empty, set_up,
		"pass" if passed else "FAIL"])
	scene.free()
	return passed


# A loadout changed on the arts panel is saved with the world from the pause panel, and Continue on
# a fresh title restores it. Control: a new game from the same seed starts from the person's first
# skills.
func _loadout_save_pass() -> bool:
	var game_slot := TestSaves.game_slot()
	TestSaves.clear()
	var scene := _scene(0)
	var hud: CanvasLayer = scene.get_node("Hud")
	scene.command(_key(scene.ARTS_KEY))
	scene.command(_key(KEY_DOWN))
	scene.command(_key(KEY_T))
	scene.command(_key(scene.ARTS_KEY))
	var changed: Array = hud.loadout.slots.duplicate()
	var saved: bool = scene.save()
	scene.free()
	var resumed := _title()
	resumed.navigate(_key(KEY_DOWN))
	resumed.navigate(_key(KEY_ENTER))
	var restored: Array = resumed.get_node("Hud").loadout.slots.duplicate()
	var restored_shown: Dictionary = resumed.get_node("Hud").slot_skills[4]
	resumed.free()
	var fresh := _scene(0)
	var first: Array = fresh.get_node("Hud").loadout.slots.duplicate()
	fresh.free()
	TestSaves.clear()
	var untouched := TestSaves.game_slot() == game_slot
	var passed: bool = saved and changed[4] == {"art": -1, "skill": 1} and changed[1].is_empty() \
			and restored == changed and not restored_shown.is_empty() and first != changed \
			and first[1] == {"art": -1, "skill": 1} and untouched
	print("play_test: the loadout %s saved %s, restored by Continue %s, T showing %s; a new game starts from %s; the game's slot 0 untouched %s: %s" % [
		changed, saved, restored, not restored_shown.is_empty(), first, untouched, "pass" if passed else "FAIL"])
	return passed


# The scene with a world started from SEED, its player born into `village`, saving to the tests'
# own directory.
func _scene(village: int) -> Node2D:
	var scene: Node2D = load(SCENE).instantiate()
	scene.birth_village = village
	scene.save_directory = TestSaves.directory
	# The runner has no running tree to add the scene to, so it is readied directly.
	scene.notification(Node.NOTIFICATION_READY)
	scene.start(SEED)
	return scene


# The scene on the title screen, as it opens or as leaving for the title reloads it, its saves in
# the tests' own directory.
func _title() -> Node2D:
	var scene: Node2D = load(SCENE).instantiate()
	scene.save_directory = TestSaves.directory
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


# A right click where `scene` draws the centre of `cell`.
func _click(cell: Vector2i, scene: Node2D) -> InputEventMouseButton:
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_RIGHT
	click.pressed = true
	click.position = _centre(cell, scene)
	return click


# The cursor moved to where `scene` draws the centre of `cell`, with the buttons of `mask` held.
func _drag(cell: Vector2i, scene: Node2D, mask: int) -> InputEventMouseMotion:
	return _motion(_centre(cell, scene), mask)


# Where `scene` draws the centre of `cell`, in its pixels.
func _centre(cell: Vector2i, scene: Node2D) -> Vector2:
	return scene.get_node("WorldArt").to_screen((Vector2(cell) + Vector2.ONE * 0.5) * WorldAdapter.CELL_SIZE)


# The cursor moved to `at`, in the scene's pixels, with the buttons of `mask` held.
func _motion(at: Vector2, mask: int) -> InputEventMouseMotion:
	var motion := InputEventMouseMotion.new()
	motion.position = at
	motion.button_mask = mask
	return motion


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _distance(position: Vector2, cell: Vector2i) -> float:
	return position.distance_to((Vector2(cell) + Vector2.ONE * 0.5) * WorldAdapter.CELL_SIZE)


# The settings' damage numbers row reaches the world's drawing: as the scene starts, the world art
# floats the player's own number and not another fight's; set to everyone, both; set off, neither.
# Setup: both numbers lie within the view the art shows.
func _numbers_pass() -> bool:
	var scene := _scene(CULT_VILLAGE)
	var settings: Control = scene.get_node("Hud").settings
	var combat: Node2D = scene.get_node("WorldArt/Combat")
	var now := Time.get_ticks_msec()
	combat.show_at(Rect2(0, 0, 64, 64), now, Vector2.ZERO)
	for mine: bool in [true, false]:
		combat.bodies.add_number(now, "7" if mine else "9", Vector2(32, 32), mine, mine)
	var texts := func() -> Array: return combat.shown_numbers(now).map(
			func(number: Dictionary) -> String: return number.text)
	var at_start: Array = texts.call()
	settings.chosen.numbers = settings.NUMBERS_MODES.find("everyone")
	settings.apply()
	var everyone: Array = texts.call()
	settings.chosen.numbers = settings.NUMBERS_MODES.find("off")
	settings.apply()
	var off: Array = texts.call()
	scene.free()
	var passed := at_start == ["7"] and everyone == ["7", "9"] and off.is_empty()
	print("play_test: damage numbers shown as the scene starts %s, for everyone %s, off %s: %s" % [
		at_start, everyone, off, "pass" if passed else "FAIL"])
	return passed


# M opens the sect panel on the Cult person's sect, and Enter makes it a member, the panel saying it
# was admitted. Control: born into an Unorthodox village, Enter is refused for want of proof, the
# panel says so, and the person stays out of every sect.
func _apply_pass() -> bool:
	var scene := _scene(CULT_VILLAGE)
	var world: WorldAdapter = scene.world
	var panel: Control = scene.get_node("Hud").sect
	var person := world.roster(scene.steered)[0]
	var cult_born: bool = world.faction_names()[world.army_faction(scene.steered)] == "Demonic"
	var opened: bool = scene.command(_key(KEY_M)) and panel.sect >= 0 and world.person_sect(person) == -1
	var entered: bool = scene.command(_key(KEY_ENTER))
	var joined: bool = world.person_sect(person) == panel.sect and panel.verdict == "APPLICATION_NONE"
	scene.free()
	var refused_scene := _scene(UNORTHODOX_VILLAGE)
	var other: WorldAdapter = refused_scene.world
	var other_panel: Control = refused_scene.get_node("Hud").sect
	var other_person := other.roster(refused_scene.steered)[0]
	# Setup: the person was born Demonic in CULT_VILLAGE and Unorthodox in UNORTHODOX_VILLAGE.
	var set_up: bool = cult_born and other.faction_names()[other.army_faction(refused_scene.steered)] == "Unorthodox"
	refused_scene.command(_key(KEY_M))
	var other_entered: bool = refused_scene.command(_key(KEY_ENTER))
	var refused: bool = other_entered and other_panel.verdict == "APPLICATION_UNPROVEN" \
			and other.person_sect(other_person) == -1
	refused_scene.free()
	var passed := set_up and opened and entered and joined and refused
	print("play_test: M opens the sect panel %s; Enter applies %s and joins %s; from an Unorthodox village Enter is refused as unproven %s (setup %s): %s" % [
		opened, entered, joined, refused, set_up, "pass" if passed else "FAIL"])
	return passed


# A Cult body ordered on the largest army hostile to it dies among its people and its soul is
# born again: on that tick the rebirth screen opens on the body that died and the new one, and no
# tick runs while it is open; Enter closes it and the ticks run again. Setup: the screen was not
# open before the death, and the dead body and the new one differ.
func _rebirth_pass() -> bool:
	var scene := _scene(CULT_VILLAGE)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var body := world.roster(scene.steered)[0]
	# The largest, so that a lone body loses whatever the balance of a fight between equals.
	var target := Vector2i(-1, -1)
	var largest := 0
	for army in world.army_count() - 1:
		var roster := world.roster(army)
		if world.hostile(world.army_faction(scene.steered), world.army_faction(army)) \
				and roster.size() > largest:
			largest = roster.size()
			target = Vector2i((world.person_position(roster[0]) / WorldAdapter.CELL_SIZE).floor())
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


# Where person `person` of `scene`'s world stands, in the scene's pixels, where a click lands on its
# body.
func _pixels_of(scene: Node2D, person: int) -> Vector2:
	return scene.get_node("WorldArt").to_screen(scene.world.person_position(person))


# The nearest hostile army, steered so that it does not close on the person on its own, marches on
# the person until two of it are in sight and the nearest within INTENT_APART cells, and halts
# there; the person stands where it is.
# A left click on the nearest foe in sight selects it and sets no intent; ticked INTENT_TICKS, the
# person comes no closer to it (the control). Space then sets the intent on it, and as many ticks
# bring the person closer by more than a cell. A click on the other foe moves the intent to it. A
# right click, a move order, replaces the intent, and the person reaches the cell clicked within
# MOVE_LIMIT ticks. Space again sets an intent; once the person has chased it off that cell, Escape
# ends it and the person holds where it stands, not walking back; control, a right click on the
# old cell then walks it back there. Tab selects the next foe in
# sight and Escape clears the selection. Space on a person of the person's
# own faction is refused as not a foe, the reason on the HUD, and sets no intent. The touch
# controls' next button selects a foe and their attack button sets the intent on it. In a fresh
# scene, with no foe in sight, Space does nothing and shows nothing.
func _intent_pass() -> bool:
	var scene := _scene(0)
	var world: WorldAdapter = scene.world
	var hud: CanvasLayer = scene.get_node("Hud")
	var person: int = world.roster(scene.steered)[0]
	var here := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	world.order(scene.steered, here, PackedInt64Array(range(world.skill_count(scene.steered))))
	var foes := _nearest_hostile(world, scene.steered)
	world.steer(foes, true)
	world.order(foes, here, PackedInt64Array([0]))
	var waited := 0
	var near := func() -> bool:
		var seen := world.foes_in_sight(person)
		return seen.size() >= 2 and world.person_position(seen[0]).distance_to(scene.lead()) \
				<= INTENT_APART * WorldAdapter.CELL_SIZE
	while not near.call() and waited < APPROACH_LIMIT:
		scene.step()
		waited += 1
	# Halted where it stands, the army comes no closer.
	world.order(foes, Vector2i((world.body_positions(foes)[0] / WorldAdapter.CELL_SIZE).floor()), PackedInt64Array([0]))
	for i in INTENT_TICKS:
		scene.step()
	var seen := world.foes_in_sight(person)
	if seen.size() < 2:
		print("play_test: fewer than two foes came within sight in %d ticks: FAIL" % APPROACH_LIMIT)
		scene.free()
		return false
	var foe := seen[0]
	var selected: bool = scene.command(_left(_pixels_of(scene, foe))) and scene.selected == foe \
			and world.target(scene.steered) < 0
	var before: float = scene.lead().distance_to(world.person_position(foe))
	for i in INTENT_TICKS:
		scene.step()
	var stayed: bool = scene.lead().distance_to(world.person_position(foe)) >= before - WorldAdapter.CELL_SIZE / 2.0
	var set: bool = scene.command(_key(KEY_SPACE)) and world.target(scene.steered) == foe
	var from: float = scene.lead().distance_to(world.person_position(foe))
	for i in INTENT_TICKS:
		scene.step()
	var chased := world.target(scene.steered) == foe or world.person_health(foe) == 0
	var closed: bool = scene.lead().distance_to(world.person_position(foe)) < from - WorldAdapter.CELL_SIZE
	var second := -1
	for each in world.foes_in_sight(person):
		if each != foe:
			second = each
	var retargeted: bool = second >= 0 and scene.command(_left(_pixels_of(scene, second))) \
			and scene.selected == second and world.target(scene.steered) == second
	scene.command(_click(here, scene))
	var ended := world.target(scene.steered) < 0
	var walked := 0
	while Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor()) != here and walked < MOVE_LIMIT:
		scene.step()
		walked += 1
	var reached := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor()) == here
	var again: bool = scene.command(_key(KEY_SPACE)) and world.target(scene.steered) >= 0
	for i in INTENT_TICKS:
		scene.step()
	var there := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	scene.command(_key(KEY_ESCAPE))
	var escaped: bool = again and there != here and world.target(scene.steered) < 0 and scene.selected < 0
	for i in INTENT_TICKS:
		scene.step()
	var stood := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor()) == there
	# Control: a right click on the old objective still walks the person back to it.
	scene.command(_click(here, scene))
	var back := 0
	while Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor()) != here and back < MOVE_LIMIT:
		scene.step()
		back += 1
	var returned := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor()) == here
	seen = world.foes_in_sight(person)
	var expected := seen[(seen.find(scene.selected) + 1) % seen.size()]
	var tabbed: bool = scene.command(_key(KEY_TAB)) and scene.selected == expected
	scene.command(_key(KEY_ESCAPE))
	var cleared: bool = scene.selected < 0 and hud.screen() == null
	var own := -1
	for army in world.army_count():
		for other in world.roster(army):
			if other != person and world.person_faction(other) == world.army_faction(scene.steered):
				own = other
	scene.select(own)
	hud.labels.notice.visible = false
	var refused: bool = own >= 0 and not scene.command(_key(KEY_SPACE)) and world.target(scene.steered) < 0 \
			and hud.labels.notice.visible and hud.labels.notice.text == "ATTACK_NOT_A_FOE"
	scene.select(-1)
	hud.use_touch(true)
	var touch: Control = hud.touch
	scene.touch(_finger(0, touch.BUTTONS.cycle.get_center(), true))
	scene.touch(_finger(0, touch.BUTTONS.cycle.get_center(), false))
	var cycled: bool = world.foes_in_sight(person).has(scene.selected)
	scene.touch(_finger(0, touch.BUTTONS.attack.get_center(), true))
	scene.touch(_finger(0, touch.BUTTONS.attack.get_center(), false))
	var tapped: bool = cycled and world.target(scene.steered) == scene.selected
	scene.free()

	var alone := _scene(0)
	var alone_hud: CanvasLayer = alone.get_node("Hud")
	var nobody: bool = alone.world.foes_in_sight(alone.world.roster(alone.steered)[0]).is_empty()
	var nothing: bool = nobody and not alone.command(_key(KEY_SPACE)) and alone.world.target(alone.steered) < 0 \
			and not alone_hud.labels.notice.visible
	alone.free()
	var passed: bool = selected and stayed and set and chased and closed and retargeted and ended and reached \
			and escaped and stood and returned and tabbed and cleared and refused and tapped and nothing
	print("play_test: %d foes in sight after %d ticks; a click selected foe %d with no intent %s, the person kept its distance %s; Space set the intent %s, chased %s and closed in %s; a click on foe %d moved it %s; a move order replaced it %s and the person reached its cell %s in %d ticks; Space again, a chase to %s and Escape ended it %s, the person holding there %s, a right click walking it back %s; Tab selected the next %s; Escape cleared %s; Space on one of its own faction refused %s; touch next and attack %s; with no foe in sight Space did nothing %s: %s" % [
		seen.size(), waited, foe, selected, stayed, set, chased, closed, second, retargeted, ended, reached, walked,
		there, escaped, stood, returned, tabbed, cleared, refused, tapped, nothing, "pass" if passed else "FAIL"])
	return passed

