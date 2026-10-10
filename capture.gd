# Captures #232's target selection in the real play.tscn, in Korean at 1920x1080: a foe selected by
# a left click (select_ko.png), the next foe selected by Tab (cycle_ko.png), the attack intent set
# by Space and chased until the target is within the guard strike's reach, CHASE_TICKS ticks at most (attack_ko.png), and a person of the player's own
# faction clicked, which talks or, out of talk reach, is selected (own_ko.png). It prints what each image
# shows. Run, after make build, with this file copied to the project root:
#   godot --path . --script res://capture_232.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const KEPT := "user://capture_232.cfg"
const WINDOW := Vector2i(1920, 1080)
const APPROACH_LIMIT := 4000
const NEAR_CELLS := 4
const CHASE_TICKS := 200
# The guard strike's reach, a cell and a half, in sub-cell units.
const REACH := 384.0

var _dir := "."
var _frame := 0
var _step := 0
var _play: Node2D
var _world: WorldAdapter
var _person := -1


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_play = load("res://play/play.tscn").instantiate()
	_play.birth_village = 0
	root.add_child(_play)


func _pixels(person: int) -> Vector2:
	return _world.person_position(person) * _play.PIXELS_PER_CELL / WorldAdapter.CELL_SIZE


func _left(at: Vector2) -> InputEventMouseButton:
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	click.position = at
	return click


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _shoot(name: String) -> void:
	var path := "%s/%s_ko.png" % [_dir, name]
	root.get_viewport().get_texture().get_image().save_png(path)
	var hud: CanvasLayer = _play.get_node("Hud")
	print("capture: wrote %s; selected %d, intent %d, frame shown %s, name %s, health %s, notice %s" % [path,
		_play.selected, _world.target(_play.steered), hud.target_panel.visible, hud.labels.target_name.text,
		hud.labels.target_health.text, hud.labels.notice.text if hud.labels.notice.visible else "none"])


func _approach() -> bool:
	_person = _world.roster(_play.steered)[0]
	var here := Vector2i((_play.lead() / WorldAdapter.CELL_SIZE).floor())
	_world.order(_play.steered, here, PackedInt64Array(range(_world.skill_count(_play.steered))))
	var foes := -1
	var best := INF
	for army in _world.army_count():
		if army != _play.steered and not _world.roster(army).is_empty() \
				and _world.hostile(_world.army_faction(_play.steered), _world.army_faction(army)):
			var apart := _world.body_positions(army)[0].distance_to(_play.lead())
			if apart < best:
				best = apart
				foes = army
	_world.steer(foes, true)
	_world.order(foes, here, PackedInt64Array([0]))
	for i in APPROACH_LIMIT:
		var seen := _world.foes_in_sight(_person)
		if seen.size() >= 2 and _world.person_position(seen[0]).distance_to(_play.lead()) \
				<= NEAR_CELLS * WorldAdapter.CELL_SIZE:
			_world.order(foes, Vector2i((_world.body_positions(foes)[0] / WorldAdapter.CELL_SIZE).floor()),
					PackedInt64Array([0]))
			for j in 30:
				_play.step()
			print("capture: %d foes in sight after %d ticks" % [_world.foes_in_sight(_person).size(), i])
			return true
		_play.step()
	return false


func _process(_delta: float) -> bool:
	_frame += 1
	if _step >= 2:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", 0)
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(WINDOW)
		1:
			_play.start(SEED)
			_play.set_process(false)
			_world = _play.world
			if not _approach():
				print("capture: no foes came near")
				quit(1)
				return true
			hud.labels.notice.visible = false
		2:
			_play.command(_left(_pixels(_world.foes_in_sight(_person)[0])))
		3:
			_shoot("select")
			_play.command(_key(KEY_TAB))
		4:
			_shoot("cycle")
			_play.command(_key(KEY_SPACE))
			var aimed: int = _play.selected
			for i in CHASE_TICKS:
				_play.step()
				if _world.person_position(aimed).distance_to(_play.lead()) <= REACH:
					print("capture: the chase reached the target %d ticks after Space" % (i + 1))
					break
		5:
			_shoot("attack")
			var own := -1
			var nearest := INF
			for army in _world.army_count():
				for other in _world.roster(army):
					var apart := _world.person_position(other).distance_to(_play.lead())
					if other != _person and _world.person_faction(other) == _world.army_faction(_play.steered) \
							and apart < nearest:
						nearest = apart
						own = other
			_play.command(_key(KEY_ESCAPE))
			_play.command(_left(_pixels(own)))
		6:
			_shoot("own")
			print("capture: done")
			quit(0)
			return true
	_step += 1
	return false
