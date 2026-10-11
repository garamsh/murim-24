# m1_capture.gd -- <out.png>: renders world_view.tscn, the M1 battle of 500 people, in a 1440 x 810
# window (3x the native screen) and saves it on frame SHOT_FRAME. It quits by frame SHOT_FRAME + 5
# whatever happens.
extends SceneTree

const SHOT_FRAME := 180
var _frame := 0


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1440, 810)
	root.add_child(load("res://world_view/world_view.tscn").instantiate())


func _process(_d: float) -> bool:
	_frame += 1
	if _frame == SHOT_FRAME:
		root.get_texture().get_image().save_png(OS.get_cmdline_user_args()[0])
		print("m1_capture: saved on frame ", _frame)
	return _frame >= SHOT_FRAME + 5
