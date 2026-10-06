# Records play.tscn with the player born into village VILLAGE (after `--`, default 1, the region's
# first Cult village) pressing C at frame CHALLENGE, as the key would, and again at AGAIN; the
# HUD is in Korean.
# Run: godot --path . --write-movie <dir>/duel.png --quit-after 240 --script <this file> -- <village>
extends SceneTree

const CHALLENGE := 30
const AGAIN := 180
var _frame := 0
var _play: Node


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	var args := OS.get_cmdline_user_args()
	_play.birth_village = int(args[0]) if args.size() > 0 else 1
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == CHALLENGE or _frame == AGAIN:
		var key := InputEventKey.new()
		key.keycode = KEY_C
		key.pressed = true
		_play.command(key)
	return false
