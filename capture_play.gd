# Records play.tscn for the PR's evidence: the HUD in Korean, then, from frame SWITCH on, in
# English. Run: godot --path . --write-movie <dir>/play.png --quit-after 180 --script <this file>
extends SceneTree

const SWITCH := 120
var _frame := 0
var _play: Node


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == SWITCH:
		_play.get_node("Hud").switch_locale()
	return false
