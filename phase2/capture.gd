# Captures the built nameplates (#164, phase 2) in the real play.tscn over the crowd the mockup
# (art/mockup/nameplates.gd at 9bde82c) staged, in each locale, at each window in WINDOWS:
# names_<window>_<locale>.png. The world then stands still, so every body is drawn in its standing
# frame. It prints, for each image, the names shown by relation and how many people stood within
# reach.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://capture.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const TICKS_PER_DAY := 5
const LOCALES := ["ko", "en"]
const KEPT := "user://nameplates_capture.cfg"
const WINDOWS: Array[Vector2i] = [Vector2i(1280, 720), Vector2i(1920, 1080), Vector2i(2560, 1440)]
## Where each relation's armies are walked to, in cells from the player.
const POSTS := {
	WorldAdapter.RELATION_OWN: Vector2i(0, 3),
	WorldAdapter.RELATION_NEUTRAL: Vector2i(-5, 0),
	WorldAdapter.RELATION_ALLY: Vector2i(5, 0),
}
## Armies walked to each post.
const ARMIES_PER_POST := 5
const GATHER_TICKS := 6000


var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array[Dictionary] = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _player := -1


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for window: Vector2i in WINDOWS:
			_shots.append({"locale": locale, "window": window})
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	_player = _world.player_army()
	# Peace between every two factions hostile by stance first, so no one fights on the way; war once
	# every army stands at its post.
	for faction in _world.faction_count():
		for other in _world.faction_count():
			if _world.relation(faction, other) == WorldAdapter.RELATION_HOSTILE:
				_world.declare(_world.leader(faction), other, WorldAdapter.DECLARATION_PEACE)
	if not _gather():
		quit(1)
		return
	_declare(WorldAdapter.RELATION_ALLY, WorldAdapter.DECLARATION_WAR)
	_open()


# Has the player's faction's leader declare `what` toward each faction standing `relation` toward it.
func _declare(relation: int, what: int) -> void:
	var mine := _world.army_faction(_player)
	for faction in _world.faction_count():
		if _world.relation(mine, faction) == relation:
			_world.declare(_world.leader(mine), faction, what)
		print("nameplates: %s stands %d toward %s" % [_world.faction_names()[mine],
				_world.relation(mine, faction), _world.faction_names()[faction]])


# Walks ARMIES_PER_POST armies of each relation in POSTS, the nearest to the player, to their post,
# steered so the rules for people on their own leave them there, the faction to be at war still
# allied.
func _gather() -> bool:
	var mine := _world.army_faction(_player)
	var home := Vector2i(_cell(_world.person_position(_world.roster(_player)[0])))
	var walked := {}
	for relation: int in POSTS:
		var armies := []
		for army in _world.army_count():
			if army != _player and not _world.roster(army).is_empty() \
					and _world.relation(mine, _world.army_faction(army)) == relation:
				armies.append(army)
		armies.sort_custom(func(a: int, b: int) -> bool:
			return _cell(_world.person_position(_world.roster(a)[0])).distance_to(Vector2(home)) \
					< _cell(_world.person_position(_world.roster(b)[0])).distance_to(Vector2(home)))
		if armies.size() < 1:
			push_error("nameplates: no army stands %d toward the player" % relation)
			return false
		for army: int in armies.slice(0, ARMIES_PER_POST):
			_world.steer(army, true)
			walked[army] = _post(army, home + POSTS[relation])
	for i in GATHER_TICKS:
		if walked.keys().all(func(army: int) -> bool: return _world.roster(army).is_empty() \
				or _cell(_world.person_position(_world.roster(army)[0])).distance_to(Vector2(walked[army])) <= 1.0):
			break
		if i % 20 == 0:
			_world.order(_player, home, _skills(_player))
			for army: int in walked:
				if not _world.roster(army).is_empty():
					_world.order(army, walked[army], _skills(army))
		_world.tick()
	for army: int in walked:
		print("nameplates: army %d of %s, %d people, lead %s from its post" % [army,
				_world.faction_names()[_world.army_faction(army)], _world.roster(army).size(),
				_cell(_world.person_position(_world.roster(army)[0])) - Vector2(walked[army])
				if not _world.roster(army).is_empty() else "dead"])
	return true


# The cell nearest `cell` that `army` may be ordered to, which takes it there.
func _post(army: int, cell: Vector2i) -> Vector2i:
	for reach in 6:
		for y in range(cell.y - reach, cell.y + reach + 1):
			for x in range(cell.x - reach, cell.x + reach + 1):
				if maxi(absi(x - cell.x), absi(y - cell.y)) == reach and _world.order(army, Vector2i(x, y), _skills(army)):
					return Vector2i(x, y)
	return cell


func _skills(army: int) -> PackedInt64Array:
	return PackedInt64Array(range(_world.skill_count(army)))


func _cell(at: Vector2) -> Vector2:
	return (at / WorldAdapter.CELL_SIZE).floor()


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
	var shot := _shots[_shot_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(shot.window)
		1:
			_play._play(_world)
			_play.set_process(false)
			# Two reads of the same positions: no body moved since the last, so each stands.
			_play._art.note_tick()
			_play._art.note_tick()
			_play._process(0.0)
			hud.labels.notice.visible = false
		2:
			var counts := {}
			for plate: Dictionary in hud.nameplates.plates:
				var relation := _world.relation(_world.army_faction(_player),
						_world.person_faction(plate.person))
				counts[relation] = counts.get(relation, 0) + 1
			print("nameplates: %s %s: %d within reach, named by relation %s" % [shot.window, shot.locale,
					_play._art.near.size(), counts])
		3:
			var path := "%s/names_%dx%d_%s.png" % [_dir, shot.window.x, shot.window.y, shot.locale]
			root.get_viewport().get_texture().get_image().save_png(path)
			print("capture: wrote ", path)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("nameplates: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false
