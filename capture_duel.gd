# Records play.tscn with the player born into village VILLAGE (after `--`, default 1, the region's
# first Cult village) pressing C at frame CHALLENGE, as the key would, and saves stills when the
# challenge starts, every second of play the duel is fought (duel_sNN.png, NN seconds after the
# challenge), and once it is settled. Prints the ticks
# from the challenge to the settlement. Quits AFTER frames past the settlement.
# Run: godot --path . --write-movie <dir>/duel.avi --script <this file> -- <village> <dir>
extends SceneTree

const CHALLENGE := 30
const AFTER := 120
var _frame := 0
var _play: Node
var _dir := ""
var _faction := -1
var _from_tick := -1
var _settled_frame := -1
var _shots := {}


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	var args := OS.get_cmdline_user_args()
	_play.birth_village = int(args[0]) if args.size() > 0 else 1
	_dir = args[1] if args.size() > 1 else "."
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	var world: WorldAdapter = _play.world
	if _frame == CHALLENGE:
		_faction = world.army_faction(_play.steered)
		var key := InputEventKey.new()
		key.keycode = KEY_C
		key.pressed = true
		_play.command(key)
		_from_tick = _play.ticks
		print("capture_duel: challenge at frame %d, tick %d, duel %s" % [_frame, _from_tick, world.duel(_faction)])
	if _frame == CHALLENGE + 10:
		_shot("duel_started")
	if _from_tick >= 0 and _settled_frame < 0 and world.duel(_faction).is_empty() and _frame > CHALLENGE:
		_settled_frame = _frame
		var ticks: int = _play.ticks - _from_tick
		print("capture_duel: settled at frame %d, %d ticks (%.1f s) after the challenge; leader %d" % [
			_frame, ticks, ticks / float(WorldAdapter.TICKS_PER_SECOND), world.leader(_faction)])
	var since: int = _play.ticks - _from_tick
	if _from_tick >= 0 and _settled_frame < 0 and since > 0 \
			and since % WorldAdapter.TICKS_PER_SECOND == 0 and not _shots.has(since):
		_shots[since] = true
		_shot("duel_s%02d" % (since / WorldAdapter.TICKS_PER_SECOND))
	if _settled_frame > 0 and _frame == _settled_frame + 20:
		_shot("duel_settled")
	return _settled_frame > 0 and _frame >= _settled_frame + AFTER or _frame > 6000


func _shot(name: String) -> void:
	var image := root.get_texture().get_image()
	image.save_png("%s/%s.png" % [_dir, name])
	print("capture_duel: %s.png at frame %d, tick %d" % [name, _frame, _play.ticks])
