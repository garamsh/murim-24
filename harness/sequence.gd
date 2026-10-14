## Captures the M1 battle's frames in a run of eight, Korean names over it, once warmed up: every
## frame saved whole, and a 4x crop round the use nearest the screen's middle at the run's start.
extends SceneTree
var view: Node
var frame := 0
var centre := Vector2.ZERO
const START := 150
const RUN := 8


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	view = load("res://world_view/world_view.tscn").instantiate()
	root.add_child(view)


func _process(_delta: float) -> bool:
	frame += 1
	if frame == START:
		var combat: Node2D = view.get_node("WorldArt/Combat")
		var camera: Camera2D = view.get_node("Camera")
		var screen := Vector2(root.size) / camera.zoom
		var middle := camera.position
		var best := INF
		for strike: Dictionary in combat.shown_strikes(Time.get_ticks_msec()):
			var at: Vector2 = view.get_node("WorldArt").to_screen(strike.origin)
			if at.distance_to(middle) < best:
				best = at.distance_to(middle)
				centre = (at - (middle - screen / 2.0)) * camera.zoom.x
		print("sequence: centre %s, %d uses shown" % [centre, combat.shown_strikes(Time.get_ticks_msec()).size()])
	if frame >= START and frame < START + RUN:
		var image := root.get_texture().get_image()
		image.save_png("res://build/issue184/seq/frame%d-ko.png" % (frame - START))
		var crop := image.get_region(Rect2i(Vector2i(centre) - Vector2i(120, 80), Vector2i(240, 160)))
		crop.resize(960, 640, Image.INTERPOLATE_NEAREST)
		crop.save_png("res://build/issue184/seq/crop%d-ko.png" % (frame - START))
	if frame >= START + RUN:
		quit()
	if frame > 600:
		quit(2)
	return false
