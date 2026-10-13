extends SceneTree
const Grade := preload("res://grade/grade.gd")
var play: Node
var frame := 0
var shots := ["off", "day", "dusk", "night", "low", "settings"]
func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	play = load("res://play/play.tscn").instantiate()
	root.add_child(play)
func _process(_delta: float) -> bool:
	frame += 1
	if frame == 3:
		play.start(2026)
	if frame == 120:
		play.set_process(false)
		print("capture: seed 2026 birth village, core date %s, camera zoom %s, renderer %s" % [play.world.date(), play.get_node("Camera").zoom, RenderingServer.get_video_adapter_name()])
	if frame >= 120:
		var at := frame - 120
		var n := at / 6
		if n < shots.size():
			var grade: CanvasLayer = play.get_node("Grade")
			if at % 6 == 0:
				grade.quality = Grade.Quality.STANDARD
				grade.show_time([0.5, 0.5, 0.76, 0.0, 0.5, 0.5][n], 1)
				grade.visible = shots[n] != "off"
				if shots[n] == "low":
					grade.quality = Grade.Quality.LOW
				if shots[n] == "settings":
					play.open_settings()
			if at % 6 == 5:
				var path := "res://build/issue310/evidence/%s-ko.png" % shots[n]
				var error := root.get_texture().get_image().save_png(path)
				print("capture: %s saved %s params %s" % [shots[n], error == OK, grade.params()])
				if error != OK:
					quit(1)
		else:
			quit()
	if frame > 300:
		quit(2)
	return false
