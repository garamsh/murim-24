# Captures of the dialogue card as play.tscn draws it (PR #368), in each locale on PC at 1920x1080:
# - seller: the dialogue open on the nearest person who sells the herb; its card names its occupation.
# - plain: the dialogue open on the nearest person who sells nothing; its card names its sect or none.
# Each person is found among those the player's faction is not hostile to, and the player is walked up
# to it until the core lets them talk. Every action goes through the scene's own talk_with() and the
# core.
# Run after make build, copied to the root:
#   godot --path . --script res://card_capture.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const DAYS_PER_YEAR := 120
const TICKS_PER_DAY := 5
const LOCALES := ["ko", "en"]
const VIEWS := ["seller", "plain"]
const WINDOW := Vector2i(1920, 1080)
const KEPT := "user://card_capture.cfg"

var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for view: String in VIEWS:
			var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
			var player := world.roster(world.player_army())[0]
			var person := _nearest(world, player, view == "seller")
			if person < 0 or not _approach(world, player, person):
				push_error("card_capture: no %s reached" % view)
				quit(1)
				return
			var occupation := world.person_occupation(person)
			print("card_capture: %s %s: person %d, occupation %s, sells the herb %s, %d cells from the player" % [
					locale, view, person, world.occupation_names()[occupation] if occupation >= 0 else "none",
					world.sells(person, 0), _apart(world, player, person)])
			_shots.append({"locale": locale, "view": view, "world": world, "person": person})
	_open()


func _cell(world: WorldAdapter, person: int) -> Vector2i:
	return Vector2i((world.person_position(person) / WorldAdapter.CELL_SIZE).floor())


func _apart(world: WorldAdapter, one: int, other: int) -> int:
	var apart := (_cell(world, one) - _cell(world, other)).abs()
	return maxi(apart.x, apart.y)


func _hostile(world: WorldAdapter, army: int) -> bool:
	var relation := world.relation(world.army_faction(world.player_army()), world.army_faction(army))
	return relation == WorldAdapter.RELATION_HOSTILE or relation == WorldAdapter.RELATION_AT_WAR


# The nearest person, not the player and not of a hostile army, who sells the herb when `selling`, or
# sells nothing otherwise.
func _nearest(world: WorldAdapter, player: int, selling: bool) -> int:
	var best := -1
	for army in world.army_count():
		if _hostile(world, army):
			continue
		for person in world.roster(army):
			var sells := false
			for ware in world.wares().size():
				sells = sells or world.sells(person, ware)
			if person != player and sells == selling \
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
	for i in DAYS_PER_YEAR * TICKS_PER_DAY:
		if world.talk_refusal(player, person) == WorldAdapter.TALK_NONE:
			return true
		if i % 20 == 0:
			world.order(mine, _cell(world, person), PackedInt64Array(range(world.skill_count(mine))))
		world.tick()
	return false


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(WINDOW)
		1:
			_play._play(shot.world)
			_play.set_process(false)
			_play._process(0.0)
			hud.labels.notice.visible = false
		2:
			if not _play.talk_with(shot.person):
				push_error("card_capture: talk_with refused person %d" % shot.person)
			print("card_capture: %s %s: card %s" % [shot.locale, shot.view, hud.dialogue.particulars()])
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("card_capture: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _save(shot: Dictionary) -> void:
	var name: String = "%s_pc_1920x1080_%s" % [shot.view, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("card_capture: %s.png %dx%d, sha256 %s" % [name, image.get_width(), image.get_height(),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
