# Captures every HUD panel and screen as play.tscn draws it, driven only through the scene's
# _unhandled_input, so the same script runs before and after the HUD refactor (#191).
# Run after make build, copied to the root:
#   godot --path . --script res://hud_capture.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const LOCALES := ["ko", "en"]
const KEPT := "user://hud_capture.cfg"
const WINDOW := Vector2i(1920, 1080)
# Each view: its name, whether on the touch layout, whether on the title screen, and the keys pressed.
const VIEWS := [
	["title", false, true, []],
	["new_game", false, true, [KEY_ENTER, KEY_1, KEY_2]],
	["settings", false, true, [KEY_DOWN, KEY_DOWN, KEY_ENTER, KEY_DOWN]],
	["hud", false, false, []],
	["genealogy", false, false, [KEY_G]],
	["genealogy_moved", false, false, [KEY_G, KEY_DOWN]],
	["genealogy_entered", false, false, [KEY_G, KEY_DOWN, KEY_ENTER]],
	["genealogy_kp_entered", false, false, [KEY_G, KEY_DOWN, KEY_KP_ENTER]],
	["sect", false, false, [KEY_M]],
	["sect_tab", false, false, [KEY_M, KEY_TAB, KEY_DOWN]],
	["arts", false, false, [KEY_K]],
	["arts_down", false, false, [KEY_K, KEY_DOWN, KEY_DOWN]],
	["factions", false, false, [KEY_F]],
	["factions_down", false, false, [KEY_F, KEY_DOWN, KEY_RIGHT]],
	["dialogue", false, false, [KEY_SPACE, KEY_DOWN]],
	["dialogue_news", false, false, [KEY_SPACE, KEY_ENTER]],
	["pause", false, false, [KEY_ESCAPE, KEY_DOWN]],
	["rebirth", false, false, []],
	["closed", false, false, [KEY_G, KEY_ESCAPE, KEY_M, KEY_M, KEY_K, KEY_ESCAPE, KEY_F, KEY_F]],
	["hud_touch", true, false, []],
	["genealogy_touch", true, false, [KEY_G]],
	["arts_touch", true, false, [KEY_K]],
	["factions_touch", true, false, [KEY_F]],
	["sect_touch", true, false, [KEY_M]],
]

var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _worlds := {}
var _talked := {}
var _kin := {}


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
		for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
			world.tick()
		_worlds[locale] = world
		_talked[locale] = _approach(world)
		_kin[locale] = _kin_army(world)
		print("hud_capture: %s: talks to %d" % [locale, _talked[locale]])
		for view: Array in VIEWS:
			_shots.append({"locale": locale, "view": view})
	_open()


# The army whose person has children, preferring one with parents and a sect too.
func _kin_army(world: WorldAdapter) -> int:
	var best := -1
	var best_score := -1
	for army in world.army_count():
		var roster := world.roster(army)
		if roster.is_empty() or world.person_children(roster[0]).is_empty():
			continue
		var person := roster[0]
		var score := int(world.person_parents(person).size() > 0) + int(world.person_sect(person) >= 0) * 2
		if score > best_score:
			best = army
			best_score = score
	print("hud_capture: kin army %d score %d" % [best, best_score])
	return best


func _cell(world: WorldAdapter, person: int) -> Vector2i:
	return Vector2i((world.person_position(person) / WorldAdapter.CELL_SIZE).floor())


# Walks the player up to the nearest person it may talk to, held where it stands. Returns that person.
func _approach(world: WorldAdapter) -> int:
	var mine := world.player_army()
	var player := world.roster(mine)[0]
	var best := -1
	for army in world.army_count():
		var relation := world.relation(world.army_faction(mine), world.army_faction(army))
		if army == mine or relation == WorldAdapter.RELATION_HOSTILE or relation == WorldAdapter.RELATION_AT_WAR:
			continue
		for person in world.roster(army):
			if best < 0 or (_cell(world, person) - _cell(world, player)).length() < (_cell(world, best) - _cell(world, player)).length():
				best = person
	for army in world.army_count():
		if world.roster(army).has(best):
			world.steer(army, true)
			world.order(army, _cell(world, best), PackedInt64Array(range(world.skill_count(army))))
	for i in 3 * DAYS_PER_YEAR * TICKS_PER_DAY:
		if world.talk_refusal(player, best) == WorldAdapter.TALK_NONE:
			return best
		if i % 20 == 0:
			world.order(mine, _cell(world, best), PackedInt64Array(range(world.skill_count(mine))))
		world.tick()
	return -1


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	if _step >= 2 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
	var view: Array = shot.view
	var world: WorldAdapter = _worlds[shot.locale]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if view[1] else "off"))
			DisplayServer.window_set_size(WINDOW)
		1:
			if not view[2]:
				_play._play(world)
				_play.set_process(false)
				_play._process(0.0)
				_play.show_talk()
				hud.labels.notice.visible = false
		2:
			if view[0] == "rebirth":
				var player := world.roster(world.player_army())[0]
				hud.rebirth.open(world, player, _talked[shot.locale])
			if view[0].begins_with("genealogy") and not view[1]:
				_play.steered = _kin[shot.locale]
			if view[0].begins_with("dialogue"):
				_play.talk_to = _talked[shot.locale]
			for keycode: Key in view[3]:
				var key := InputEventKey.new()
				key.keycode = keycode
				key.pressed = true
				_play._unhandled_input(key)
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("hud_capture: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _save(shot: Dictionary) -> void:
	var name: String = "%s_%s" % [shot.view[0], shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("hud_capture: %s %dx%d %s" % [name, image.get_width(), image.get_height(),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
