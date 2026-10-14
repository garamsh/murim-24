## Captures seed 2026's birth village at summer noon, dusk and midnight at standard, midnight at low,
## and a smithy laid beside the player as a fixture at midnight (its hearth's emitters; the fixture
## is not a structure, so it carries no light pool). Runs on main and on the branch alike.
extends SceneTree
var play: Node
var frame := 0
var shots := ["day", "dusk", "night", "night-low", "smithy-night", "smithy-night-low"]
var times := [0.5, 0.76, 0.0, 0.0, 0.0, 0.0]
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
		print("capture: seed 2026 birth village, core date %s, camera zoom %s, renderer %s" % [play.world.date(), play.get_node("Camera").zoom, RenderingServer.get_video_adapter_name()])
	if frame >= 120:
		var at := frame - 120
		var n := at / 8
		if n < shots.size():
			var grade: CanvasLayer = play.get_node("Grade")
			if at % 8 == 0:
				grade.quality = 0 if shots[n].ends_with("low") else 1
				grade.show_time(times[n], 1)
				if shots[n] == "smithy-night":
					var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://art/structures/smithy.json"))
					var region := AtlasTexture.new()
					region.atlas = load("res://art/structures/smithy.png")
					var f: Dictionary = data.frames[0].frame
					region.region = Rect2(f.x, f.y, f.w, f.h)
					var sprite := Sprite2D.new()
					sprite.texture = region
					sprite.centered = false
					sprite.position = (play.get_node("Camera").position + Vector2(-110, -30)).round()
					play.get_node("WorldArt/Standing").add_child(sprite)
			if at % 8 == 7:
				var path := "res://build/issue209/%s-%s-ko.png" % [OS.get_environment("TAG"), shots[n]]
				var error := root.get_texture().get_image().save_png(path)
				var p: Dictionary = grade.params()
				print("capture: %s saved %s gain %s vignette %s lamp %s bloom %s lights %s" % [shots[n], error == OK, p.gain, p.vignette, p.get("lamp"), p.get("bloom"), p.get("light_count")])
		else:
			quit()
	if frame > 400:
		quit(2)
	return false
