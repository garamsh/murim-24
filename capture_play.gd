# Records play.tscn for the PR's evidence: the player is ordered from its birthplace into its
# village's square on the first frame, as a left click there would order it, so the camera shows
# the village; the HUD is in Korean, then, from frame SWITCH on, in English.
# Run: godot --path . --write-movie <dir>/play.png --quit-after 300 --script <this file>
extends SceneTree

const SWITCH := 240
# Where in the village's area to walk to, as cells from its corner.
const SQUARE := Vector2i(9, 5)
var _frame := 0
var _play: Node


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 2:
		var corner: Vector2i = _play.world.villages()[0].corner
		var click := InputEventMouseButton.new()
		click.button_index = MOUSE_BUTTON_LEFT
		click.pressed = true
		click.position = (Vector2(corner + SQUARE) + Vector2.ONE * 0.5) * _play.PIXELS_PER_CELL
		_play.command(click)
	if _frame == SWITCH:
		_play.get_node("Hud").switch_locale()
	return false
