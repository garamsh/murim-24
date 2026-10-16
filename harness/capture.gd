## Throwaway (#364, not committed): captures seed 2026 with the player born in village 7, which starts with
## a temple and an inn, in the play scene with the Korean HUD at summer noon and midnight at high,
## and checks both buildings stand in view.
extends SceneTree
var play: Node
var frame := 0
var shots := ["day", "night"]
var times := [0.5, 0.0]
var _view := Rect2()
func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	play = load("res://play/play.tscn").instantiate()
	root.add_child(play)
func _process(_delta: float) -> bool:
	frame += 1
	if frame == 3:
		play.birth_village = 6
		play.start(2026)
	if frame == 120:
		play.set_process(false)
		var art: Node2D = play.get_node("WorldArt")
		var camera: Camera2D = play.get_node("Camera")
		var lead: Vector2 = play.drawn_lead()
		# The birth village's temple and inn: those nearest the player.
		var nearest := {}
		for index in play.world.structure_count():
			var kind: String = play.world.structure_kind(index)
			var sprite: Sprite2D = art.structure_sprites[index]
			if kind in ["temple", "inn"] and sprite != null:
				var middle := sprite.position + sprite.offset + sprite.texture.get_size() / 2.0
				if not nearest.has(kind) or middle.distance_to(lead) < nearest[kind].distance_to(lead):
					nearest[kind] = middle
		if nearest.size() != 2:
			print("capture: temple or inn missing %s" % [nearest])
			quit(2)
			return false
		var screen := Vector2(root.size) / camera.zoom
		camera.position = play.camera_centre((nearest.temple + nearest.inn) / 2.0, screen, play._bounds, camera.zoom.x)
		_view = Rect2(camera.position - screen / 2.0, screen)
		var seen := 0
		for kind: String in nearest:
			if _view.has_point(nearest[kind]):
				seen += 1
		print("capture: seed 2026 birth village %s, core date %s, camera zoom %s, temple at %s and inn at %s, %d of 2 in view, renderer %s" % [play.world.villages()[play.birth_village].name, play.world.date(), camera.zoom, nearest.temple, nearest.inn, seen, RenderingServer.get_video_adapter_name()])
		if seen != 2:
			quit(2)
			return false
	if frame >= 120:
		var at := frame - 120
		var n := at / 8
		if n < shots.size():
			var grade: CanvasLayer = play.get_node("Grade")
			if at % 8 == 0:
				grade.quality = 2
				play.show_view(_view)
				grade.show_time(times[n], 1)
			if at % 8 == 7:
				var path := "res://build/issue364b/%s-ko.png" % shots[n]
				var error := root.get_texture().get_image().save_png(path)
				var p: Dictionary = grade.params()
				print("capture: %s saved %s gain %s lamp %s bloom %s lights %s" % [shots[n], error == OK, p.gain, p.get("lamp"), p.get("bloom"), p.get("light_count")])
		else:
			quit()
	if frame > 400:
		quit(2)
	return false
