# Records play.tscn with the player born into village 1, the region's first Cult village: the HUD
# in Korean, F2 at ENGLISH switching it to English, G at GENEALOGY opening the genealogy panel on
# the player, and F2 at KOREAN switching back.
# Run: godot --path . --write-movie <dir>/fit.png --quit-after 160 --script <this file>
extends SceneTree

const ENGLISH := 40
const GENEALOGY := 80
const KOREAN := 120
var _frame := 0
var _play: Node


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	_play.birth_village = 1
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == ENGLISH or _frame == KOREAN:
		_play.get_node("Hud").switch_locale()
	if _frame == GENEALOGY:
		var key := InputEventKey.new()
		key.keycode = KEY_G
		key.pressed = true
		_play.command(key)
	return false
