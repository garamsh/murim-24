# corner_capture.gd -- <out dir> <seed> <cell x> <cell y> <name>: plays a game of <seed> in Korean
# under the HUD at 1920 x 1080, orders the player toward the cell given, and saves the screen as
# <name>_ko.png once it stands within 3 cells of it or has walked MAX_FRAMES frames, printing where
# the camera's view lies against the map's bounds.
extends SceneTree

const MAX_FRAMES := 3000
var _play: Node
var _frame := 0
var _args: PackedStringArray
var _target := Vector2i.ZERO


func _initialize() -> void:
	_args = OS.get_cmdline_user_args()
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_d: float) -> bool:
	_frame += 1
	if _frame == 3:
		_play.start(int(_args[1]))
		_target = Vector2i(int(_args[2]), int(_args[3]))
		_play.world.order(_play.steered, _target, PackedInt64Array([0]))
	if _frame > 3:
		for k in 4:
			_play.step()
		var at: Vector2 = Vector2(_play.lead()) / WorldAdapter.CELL_SIZE
		if at.distance_to(Vector2(_target)) < 3.0 or _frame > MAX_FRAMES:
			if _frame % 30 == 0:
				var camera: Camera2D = _play.get_node("Camera")
				var screen := Vector2(root.size) / camera.zoom
				var view := Rect2(camera.get_screen_center_position() - screen / 2.0, screen)
				var rows: PackedStringArray = _play.world.map_rows()
				print("corner_capture: at ", at, " view ", view, " map ", Vector2(rows[0].length() * 16, rows.size() * 12))
				root.get_texture().get_image().save_png("%s/%s_ko.png" % [_args[0], _args[4]])
				quit()
	return false
