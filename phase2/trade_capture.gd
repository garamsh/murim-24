# Captures of trade as play.tscn draws it (PR #181, phase 2), in each locale on PC at 1920x1080:
# - menu: the dialogue on the nearest seller of the herb, the wares row chosen.
# - trade: its wares, as Enter on that row shows them.
# - bought: Enter on the herb: bought, the silver and the belt following.
# - pill: down and Enter on the pill: bought.
# - full: the window closed, Z at full health: refused, nothing consumed.
# - hud: the player, walked into a hostile person until wounded, with the herb and pill it holds.
# - used: Z: the herb consumed, health up and the notice.
# And touch: the touch HUD with the silver strip and no belt.
# The seller is found and walked up to as art/mockup/trade.gd does, among people for whom the core's
# sells() holds. Every action goes through the scene's own talk(), command() and the core.
# Run after make build, copied to the root:
#   godot --path . --script res://trade_capture.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const TALK_REACH := 2
const LOCALES := ["ko", "en"]
const KEPT := "user://trade_capture.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true},
]
const VIEWS := ["menu", "trade", "bought", "pill", "full", "hud", "used"]
const TOUCH_VIEWS := ["touch"]
# Wounded once the player's health falls to this share of its full.
const WOUNDED := 0.8

var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _worlds := {}
var _seller := -1


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		_worlds[locale] = _lived()
		for run: Dictionary in RUNS:
			for view: String in (TOUCH_VIEWS if run.touch else VIEWS):
				_shots.append({"locale": locale, "run": run, "view": view})
	for locale: String in LOCALES:
		var world: WorldAdapter = _worlds[locale]
		var player := world.roster(world.player_army())[0]
		_seller = _nearest_seller(world, player)
		if _seller < 0 or not _approach(world, player, _seller):
			push_error("trade_capture: no seller reached")
			quit(1)
			return
		print("trade_capture: %s: player %d of %s, %d silver, beside seller %d of %s, %d cells apart" % [locale,
				player, world.faction_names()[world.army_faction(world.player_army())], world.person_silver(player),
				_seller, world.faction_names()[world.person_faction(_seller)], _apart(world, player, _seller)])
	_open()


func _lived() -> WorldAdapter:
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		world.tick()
	return world


func _cell(world: WorldAdapter, person: int) -> Vector2i:
	return Vector2i((world.person_position(person) / WorldAdapter.CELL_SIZE).floor())


func _apart(world: WorldAdapter, one: int, other: int) -> int:
	var apart := (_cell(world, one) - _cell(world, other)).abs()
	return maxi(apart.x, apart.y)


func _hostile(world: WorldAdapter, army: int) -> bool:
	var relation := world.relation(world.army_faction(world.player_army()), world.army_faction(army))
	return relation == WorldAdapter.RELATION_HOSTILE or relation == WorldAdapter.RELATION_AT_WAR


func _nearest_seller(world: WorldAdapter, player: int) -> int:
	var best := -1
	for army in world.army_count():
		if _hostile(world, army):
			continue
		for person in world.roster(army):
			if person != player and world.sells(person, 0) \
					and (best < 0 or _apart(world, player, person) < _apart(world, player, best)):
				best = person
	return best


# Walks the player up to `person`, held where it stands, until the core lets them talk.
func _approach(world: WorldAdapter, player: int, person: int) -> bool:
	for army in world.army_count():
		if world.roster(army).has(person):
			world.steer(army, true)
			world.order(army, _cell(world, person), PackedInt64Array(range(world.skill_count(army))))
	var mine := world.player_army()
	for i in 3 * DAYS_PER_YEAR * TICKS_PER_DAY:
		if world.talk_refusal(player, person) == WorldAdapter.TALK_NONE:
			return true
		if i % 20 == 0:
			world.order(mine, _cell(world, person), PackedInt64Array(range(world.skill_count(mine))))
		world.tick()
	return false


# Walks the player into the nearest hostile person until its health falls to WOUNDED of its full,
# then orders it to stand where it is.
func _wound(world: WorldAdapter) -> void:
	var mine := world.player_army()
	var player := world.roster(mine)[0]
	var full := float(world.army_strength(mine))
	var foe := -1
	for army in world.army_count():
		if _hostile(world, army):
			for person in world.roster(army):
				if foe < 0 or _apart(world, player, person) < _apart(world, player, foe):
					foe = person
	var skills := PackedInt64Array(range(world.skill_count(mine)))
	for i in 2 * DAYS_PER_YEAR * TICKS_PER_DAY:
		if world.person_health(player) <= WOUNDED * full or world.person_health(foe) == 0:
			break
		if i % 20 == 0:
			world.order(mine, _cell(world, foe), skills)
		world.tick()
	world.order(mine, _cell(world, player), skills)
	print("trade_capture: wounded to %d of %d by person %d" % [world.person_health(player), full, foe])


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
	var world: WorldAdapter = _worlds[shot.locale]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if shot.run.touch else "off"))
			DisplayServer.window_set_size(shot.run.window)
		1:
			if shot.view == "hud":
				_wound(world)
			_play._play(world)
			_play.set_process(false)
			_play._process(0.0)
			hud.labels.notice.visible = false
		2:
			_act(shot.view, hud)
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("trade_capture: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


# Does what view `view` shows, through the scene's keys.
func _act(view: String, hud: CanvasLayer) -> void:
	var world: WorldAdapter = _play.world
	var player := world.roster(world.player_army())[0]
	if view in ["menu", "trade", "bought", "pill"]:
		_play.talk_to = _seller
		_play.talk(_play.TALK_KEY)
		_play.talk(KEY_DOWN)
		if view != "menu":
			_play.talk(KEY_ENTER)
		if view == "bought":
			_play.talk(KEY_ENTER)
		elif view == "pill":
			_play.talk(KEY_DOWN)
			_play.talk(KEY_ENTER)
	elif view in ["full", "used"]:
		_play.command(_key(KEY_Z))
	print("trade_capture: %s: silver %d, held %s, health %d, line \"%s\", notice \"%s\"" % [view,
			world.person_silver(player), world.person_held(player), world.person_health(player),
			hud.dialogue.line, hud.labels.notice.text if hud.labels.notice.visible else ""])


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _save(shot: Dictionary) -> void:
	var name: String = "%s_%s_%s" % [shot.view, shot.run.name, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("trade_capture: %s.png %dx%d, sha256 %s" % [name, image.get_width(), image.get_height(),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
