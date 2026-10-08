# Checks that play.tscn turns input into orders for the person it steers: a right click on a passable
# cell moves the person toward it, and a click on a blocked cell is refused; key 1 orders the
# person's first skill, and key 9, a skill it lacks, is refused. Key C challenges the person's
# faction's leader: born into an Orthodox village, the core refuses it for having no duel and the
# HUD says so; born into a Cult village, it starts a duel the HUD shows, and a second C while it is
# fought is refused for that. When a village's residents march away, its territory follows them,
# and the world's borders and the HUD's map follow the territory the core derives, redrawn on that
# tick and not on every frame. Holding the right button keeps ordering the person toward the cursor,
# and releasing it stops the following. Q casts the first skill at the cursor, on its press or, as
# the settings choose, on its release after showing the aim, which Q again or Escape cancels; a
# refused cast shows its reason. The scene opens on the title screen with no world; a new game starts the
# world from the seed typed; Escape pauses it, the pause panel's guide showing the keys the scene
# binds, and no tick runs while it is paused; the speed the settings choose runs that many ticks.
# Enter in the sect panel applies to the sect shown, joining it where the core admits the person and
# showing why where it refuses. A Cult body marched on an Orthodox village dies there, and the
# rebirth screen opens on it and the soul's new body, holding the ticks until Enter goes on.
# The pause panel's Save writes the world, and back on the title Continue resumes it, steering the
# same person, while with no save Continue cannot be taken and with a broken one the title names
# why; the world is saved again every AUTOSAVE_DAYS days. With the touch controls shown, the
# joystick walks the person the way it is pushed and stops it when let go; a skill button tapped
# casts at the hostile person the core aims at the way the person faces, dragged casts at the point
# it aims, dropped on the cancel zone casts nothing, and held casts on the person itself, while a
# tap the core refuses for its cooldown shows why; the screens take taps, and the HUD's buttons
# open the pause panel and the genealogy panel. K opens the arts panel, where a skill chosen is put
# on a slot with that slot's key and taken off with Delete; a person who knows an art beyond its
# army's skills, with it put on T, sees it on T's slot, aims at its range and casts it at that range
# from the fifth skill button, while an empty slot shows and casts nothing. The loadout is saved
# with the world, and Continue restores it.
extends RefCounted

const SCENE := "res://play/play.tscn"
# The seed every scene here starts its world from; MARCHERS and MARCHED_ON are of its region.
const SEED := 2026
const TICKS := 600
# How far from the person's home, in cells, to look for the cells to click.
const SEARCH := 64
# Ticks the person is given to walk toward each cell the cursor is held on.
const WALK_TICKS := 60
# Pixels east of the person the cursor is put for a cast, and further than any skill reaches.
const CAST_OFFSET := Vector2(32, 0)
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
# Ticks a Cult body marched on the nearest Orthodox village has to die there.
const MARCH_LIMIT := 12000
# A region whose growth forms an army after the player's within fifteen years of five-tick days.
const GROWN_SEED := 3
const GROWN_TICKS_PER_DAY := 5
const GROWN_TICKS := 9000
const TestSaves := preload("res://extension/test_saves.gd")
# Ticks a hostile army marched on the person has to come within its first skill's range, and ticks
# the person has to be ready to cast again.
const APPROACH_LIMIT := 4000
const READY_LIMIT := 120
# Ticks after the joystick is let go, and the four ways it is pushed to find one passable.
const STOP_TICKS := 60
const WAYS := [Vector2i.UP, Vector2i.LEFT, Vector2i.DOWN, Vector2i.RIGHT]
# Ticks the joystick is held for: long enough to walk past the STRIDE cells one order reaches.
const STICK_TICKS := 150


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
			and _move_pass() and _cast_pass() and _screens_pass() and _settings_button_pass() and _apply_pass() and _rebirth_pass() and _save_pass() \
			and _autosave_pass() and _stick_pass() and _touch_cast_pass() and _taps_pass() and _arts_pass() \
			and _art_slot_pass() and _loadout_save_pass()


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


# With the touch controls shown and a hostile person marched within the first skill's range: the
# joystick pushed toward it faces the person that way, and the first skill's button tapped casts
# at the person the core aims at that way. Once the person may cast again, the button dragged off
# its pad onto the cancel zone casts nothing, while the same drag let go short of it casts at the
# point it aims, the skill's range that way. The second skill's button held for a long press casts
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

	var ready_again := _ready_to_cast(scene, person, 0, Vector2.ZERO)
	var reach := float(world.skills(scene.steered)[0].range)
	scene.last_cast = {}
	scene.touch(_finger(1, main, true))
	scene.touch(_slide(1, main + Vector2(touch.SLOP + 4, 0)))
	scene.touch(_slide(1, touch.CANCEL.at))
	scene.touch(_finger(1, touch.CANCEL.at, false))
	var cancelled: Dictionary = scene.last_cast
	var at: Vector2 = world.person_position(person)
	scene.touch(_finger(1, main, true))
	scene.touch(_slide(1, main + Vector2(touch.PAD, 0)))
	scene.touch(_finger(1, main + Vector2(touch.PAD, 0), false))
	var dragged: Dictionary = scene.last_cast
	var drag_right: bool = ready_again and cancelled.is_empty() \
			and dragged == {"skill": 0, "target": (at + Vector2(reach, 0)).floor()}

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
	scene.navigate(_finger(0, hud.new_game._random_slip().get_center(), true))
	var typed: String = hud.new_game.typed
	scene.navigate(_finger(0, hud.new_game._start_slip().get_center(), true))
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


# A way of WAYS passable for `cells` cells from where the scene's person stands, or none.
func _clear_way(scene: Node2D, cells: int) -> Vector2i:
	var rows: PackedStringArray = scene.world.map_rows()
	var home := Vector2i((scene.lead() / WorldAdapter.CELL_SIZE).floor())
	for way: Vector2i in WAYS:
		var clear := true
		for stride in range(1, cells + 1):
			var cell := home + way * stride
			clear = clear and cell.y >= 0 and cell.y < rows.size() and cell.x >= 0 \
					and cell.x < rows[cell.y].length() and rows[cell.y][cell.x] == "."
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
	var pressed: bool = scene.command(_click(first, scene.PIXELS_PER_CELL))
	var first_from := _distance(scene.lead(), first)
	for i in WALK_TICKS:
		scene.step()
		control.step()
	var first_to := _distance(scene.lead(), first)
	var held_on: bool = scene.command(_drag(second, scene.PIXELS_PER_CELL, MOUSE_BUTTON_MASK_RIGHT))
	var second_from := _distance(scene.lead(), second)
	for i in WALK_TICKS:
		scene.step()
		control.step()
	var second_to := _distance(scene.lead(), second)
	var release := _click(second, scene.PIXELS_PER_CELL)
	release.pressed = false
	var released: bool = not scene.command(release)
	var after_release: bool = not scene.command(_drag(home, scene.PIXELS_PER_CELL, 0))
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


# Q casts the first skill, a bolt, at the cursor through the cast port. Setup: key 1 leaves the
# person striking with the bolt alone, which finds no foe at home, so its ready tick moves only for
# a cast. Cast on press: Q's press is accepted and the ready tick moves on the next tick; control,
# the same scene given no Q keeps it. A second Q at once is refused as on cooldown and E, a skill
# the person lacks, as no such skill, each with its reason on the HUD, where the first Q was
# accepted. Cast on release: Q's press casts nothing but shows the aim, the bolt's range round the
# person and its area at the cursor, or at the ring for a cursor beyond it; its release casts.
# Q pressed again while aiming, or Escape, which then pauses nothing, cancels it, and the release
# casts nothing.
func _cast_pass() -> bool:
	var pressing := _cast_scene(false)
	var control := _cast_scene(false)
	var setup: bool = pressing.world.skill_count(pressing.steered) == 2 and _ready(pressing) == _ready(control)
	var ready_from := _ready(pressing)
	var cast: bool = pressing.command(_key(KEY_Q))
	pressing.step()
	control.step()
	var cast_moved := _ready(pressing) > ready_from and _ready(control) == ready_from
	var notice: Label = pressing.get_node("Hud").labels.notice
	var cooling: bool = not pressing.command(_key(KEY_Q)) and notice.text == "CAST_ON_COOLDOWN" and notice.visible
	var lacking: bool = not pressing.command(_key(KEY_E)) and notice.text == "CAST_NO_SKILL"
	var pressed := setup and cast and cast_moved and cooling and lacking
	print("play_test: cast on press, Q accepted %s and the ready tick moved %d to %d, the control's staying %s; Q again refused as on cooldown with its reason %s; E refused as no skill %s (setup %s): %s" % [
		cast, ready_from, _ready(pressing), _ready(control) == ready_from, cooling, lacking, setup,
		"pass" if pressed else "FAIL"])

	var releasing := _cast_scene(true)
	var aim: Node2D = releasing.get_node("Aim")
	var person: Vector2 = releasing.lead() * releasing.PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	var reach: float = releasing.world.skills(releasing.steered)[0]["range"] * releasing.PIXELS_PER_CELL \
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
	var let_go := _key(KEY_Q)
	let_go.pressed = false
	var released: bool = releasing.command(let_go) and not aim.visible
	releasing.step()
	var cast_on_release := _ready(releasing) > before

	var again := _cast_scene(true)
	again.command(_key(KEY_Q))
	var cancelled_again: bool = not again.command(_key(KEY_Q)) and again.aiming() == KEY_NONE \
			and not again.get_node("Aim").visible
	var again_released: bool = not again.command(let_go)
	var again_before := _ready(again)
	again.step()
	var again_still := _ready(again) == again_before

	var escaped := _cast_scene(true)
	var escape_hud: CanvasLayer = escaped.get_node("Hud")
	escaped.command(_key(KEY_Q))
	var not_paused: bool = not escaped.navigate(_key(KEY_ESCAPE)) and escape_hud.screen() == null
	escaped.command(_key(KEY_ESCAPE))
	var escape_cancelled: bool = escaped.aiming() == KEY_NONE and not escaped.get_node("Aim").visible
	var escape_released: bool = not escaped.command(let_go)
	var escape_before := _ready(escaped)
	escaped.step()
	var escape_still := _ready(escaped) == escape_before
	var on_release := held and shown and at_ring and nothing_on_press and released and cast_on_release \
			and cancelled_again and again_released and again_still and not_paused and escape_cancelled \
			and escape_released and escape_still
	print("play_test: cast on release, Q's press aims %s, the aim at the cursor with range %.0f px %s and at the ring past it %s, nothing cast while held %s; the release casts %s and the ready tick moved %s; Q again cancels %s, its release casts nothing %s %s; Escape pauses nothing %s, cancels %s, the release casts nothing %s %s: %s" % [
		held, reach, shown, at_ring, nothing_on_press, released, cast_on_release, cancelled_again,
		again_released, again_still, not_paused, escape_cancelled, escape_released, escape_still,
		"pass" if on_release else "FAIL"])
	for scene: Node2D in [pressing, control, releasing, again, escaped]:
		scene.free()
	return pressed and on_release


# The scene born into the first village, casting on release or on press as `on_release` says,
# striking with its first skill alone and the cursor CAST_OFFSET east of the person.
func _cast_scene(on_release: bool) -> Node2D:
	var scene := _scene(0)
	scene.get_node("Hud").settings.chosen.cast = 1 if on_release else 0
	scene.command(_key(KEY_1))
	scene.command(_motion(scene.lead() * scene.PIXELS_PER_CELL / WorldAdapter.CELL_SIZE + CAST_OFFSET, 0))
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


# The HUD's settings button opens the settings over the world, and no tick runs while they are
# open; Escape goes back to the world, which runs again. Control: the settings opened from the
# pause panel go back to it, so going back to the world is the button's own. Setup: the world is
# played and no screen is open before the button is pressed.
func _settings_button_pass() -> bool:
	var scene := _scene(0)
	var hud: CanvasLayer = scene.get_node("Hud")
	var span := 3.5 / WorldAdapter.TICKS_PER_SECOND
	var set_up: bool = scene.world != null and hud.screen() == null and hud.settings_button.visible
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	hud.settings_button.gui_input.emit(click)
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
	scene.free()
	var passed := set_up and opened and held and back_to_world and ran and from_pause and back_to_pause
	print("play_test: the settings button opens the settings %s, no tick while open %s; Escape goes back to the world %s, which runs %s; control: opened from the pause panel %s, Escape goes back to it %s (setup %s): %s" % [
		opened, held, back_to_world, ran, from_pause, back_to_pause, set_up, "pass" if passed else "FAIL"])
	return passed


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
	var grown_past := player >= 0 and grown.army_count() > player + 1 and grown.save(0, TestSaves.DIRECTORY)
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
	var loaded := WorldAdapter.load(scene.SAVE_SLOT, TestSaves.DIRECTORY)
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
	scene._play(lived)
	scene.steered = army
	scene._body = lived.roster(army)[0]
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
	var pixels: float = scene.PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	var art_reach: float = skills[art_skill].range * pixels
	var army_reach: float = skills[0].range * pixels
	var aim: Node2D = scene.get_node("Aim")
	scene.command(_motion(scene.lead() * pixels + CAST_OFFSET, 0))
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
	scene.save_directory = TestSaves.DIRECTORY
	# The runner has no running tree to add the scene to, so it is readied directly.
	scene.notification(Node.NOTIFICATION_READY)
	scene.start(SEED)
	return scene


# The scene on the title screen, as it opens or as leaving for the title reloads it, its saves in
# the tests' own directory.
func _title() -> Node2D:
	var scene: Node2D = load(SCENE).instantiate()
	scene.save_directory = TestSaves.DIRECTORY
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


# A right click at the centre of `cell`, in the scene's pixels of `pixels_per_cell` to a cell.
func _click(cell: Vector2i, pixels_per_cell: float) -> InputEventMouseButton:
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_RIGHT
	click.pressed = true
	click.position = (Vector2(cell) + Vector2.ONE * 0.5) * pixels_per_cell
	return click


# The cursor moved to the centre of `cell`, with the buttons of `mask` held.
func _drag(cell: Vector2i, pixels_per_cell: float, mask: int) -> InputEventMouseMotion:
	return _motion((Vector2(cell) + Vector2.ONE * 0.5) * pixels_per_cell, mask)


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
