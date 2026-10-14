## Captures seed 2026's birth village at summer dusk, the camera moved 0, 1 and 2 screen pixels right
## (0, 1/3 and 2/3 of a world pixel at 3x), held still, to show the world scrolling a screen pixel
## at a time on one grid with the bodies and the grade. TAG names the tree.
extends SceneTree
var play: Node
var frame := 0
const EACH := 12
var base := Vector2.ZERO


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	play = load("res://play/play.tscn").instantiate()
	root.add_child(play)


func _process(_delta: float) -> bool:
	frame += 1
	if frame == 3:
		play.start(2026)
	if frame == 120:
		play.set_process(false)
		base = play.get_node("Camera").position.round()
	if frame >= 120:
		var n := (frame - 120) / EACH
		if n < 3:
			var camera: Camera2D = play.get_node("Camera")
			camera.position = base + Vector2(n / 3.0, 0)
			var screen := Vector2(root.size) / camera.zoom
			play.show_view(Rect2(camera.position - screen / 2.0, screen))
			play.get_node("Grade").show_time(0.76, 1)
			if (frame - 120) % EACH == EACH - 1:
				root.get_texture().get_image().save_png("res://build/issue209/3b/%s-shift%d-ko.png" % [OS.get_environment("TAG"), n])
				print("capture: shift %d saved, camera %s" % [n, camera.position])
		else:
			quit()
	if frame > 400:
		quit(2)
	return false
