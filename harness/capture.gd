## Captures seed 2026's birth village at summer morning, noon, dusk and midnight at standard, noon
## at low, then the same morning and noon over the mountain nearest the player, where high ground
## rises above the fog. Runs on main, part 1 and part 2 alike (grade params missing on older trees
## print as null). TAG names the tree.
extends SceneTree
var play: Node
var frame := 0
var shots := ["day", "dusk", "night", "rain-day", "rain-night"]
var times := [0.5, 0.76, 0.0, 0.5, 0.0]
const EACH := 16
var mountain := Vector2.ZERO


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
		mountain = _nearest_mountain()
		print("capture: seed 2026 birth village, core date %s, camera zoom %s, mountain at %s, renderer %s" % [
			play.world.date(), play.get_node("Camera").zoom, mountain, RenderingServer.get_video_adapter_name()])
	if frame >= 120:
		var at := frame - 120
		var n := at / EACH
		if n < shots.size():
			var grade: CanvasLayer = play.get_node("Grade")
			var camera: Camera2D = play.get_node("Camera")
			if shots[n].begins_with("mountain"):
				camera.position = mountain
			var screen := Vector2(root.size) / camera.zoom
			play.show_view(Rect2(camera.position - screen / 2.0, screen))
			grade.quality = 0 if shots[n].ends_with("low") else 1
			# show_view shows the core's time; the shot's replaces it.
			grade.show_time(times[n], 1)
			grade.raining = shots[n].contains("rain")
			if at % EACH == EACH - 1:
				var path := "res://build/issue209/close/%s-%s-ko.png" % [OS.get_environment("TAG"), shots[n]]
				var error := root.get_texture().get_image().save_png(path)
				var p: Dictionary = grade.params()
				print("capture: %s saved %s gain %s vignette %s lamp %s bloom %s fog %s clouds %s rain %s" % [
					shots[n], error == OK, p.gain, p.vignette, p.get("lamp"), p.get("bloom"), p.get("fog"), p.get("clouds"), p.get("rain")])
		else:
			quit()
	if frame > 600:
		quit(2)
	return false


# The middle of the screen nearest the player that centres a mountain's highest cells, the camera
# kept a whole pixel.
func _nearest_mountain() -> Vector2:
	var art: Node = play.get_node("WorldArt")
	var relief = art.relief
	var camera: Camera2D = play.get_node("Camera")
	var best := Vector2.ZERO
	var nearest := INF
	for y in relief.size.y:
		for x in relief.size.x:
			if relief.level_at(Vector2i(x, y)) >= 4:
				var at := Vector2(x, y) * Relief.CELL
				var d := at.distance_squared_to(camera.position)
				if d < nearest:
					nearest = d
					best = at
	# Seed 2026's mountains lie on the map's edge: the camera is kept a half screen in from it, so the
	# shot shows map all round with the peak toward its side.
	var half := Vector2(root.size) / camera.zoom / 2.0
	var map := Vector2(relief.size) * Relief.CELL
	return best.clamp(half, map - half).round()
