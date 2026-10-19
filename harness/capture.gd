extends SceneTree
# Captures a new game born in village 0 of seed 2026, Korean HUD, at each tick given; then, with the
# grade and the HUD hidden, counts the frame's pixels of the viewport's clear colour: those no ground,
# structure or body paints, which the camera shows past the map's drawn edge where nothing is drawn.
# Args after --: <out prefix> <width>x<height> <touch 0|1> <ticks...>
var scene: Node2D


func _init() -> void:
	var args := OS.get_cmdline_user_args()
	var out: String = args[0]
	var size := Vector2i(int(args[1].split("x")[0]), int(args[1].split("x")[1]))
	var touch := args[2] == "1"
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	TranslationServer.set_locale("ko")
	scene = load("res://play/play.tscn").instantiate()
	scene.birth_village = 0
	scene.save_directory = OS.get_user_data_dir().path_join("capture400")
	root.add_child(scene)
	await process_frame
	DisplayServer.window_set_size(size)
	root.size = size
	for f in 5:
		await process_frame
	var hud: CanvasLayer = scene.get_node("Hud")
	if touch:
		hud.settings.chosen.touch = hud.settings.TOUCH_MODES.find("on")
	scene.start(2026)
	scene.fit_view()
	var clear := RenderingServer.get_default_clear_color()
	var done := 0
	for i in range(3, args.size()):
		var tick := int(args[i])
		while done < tick:
			scene.step()
			done += 1
		for f in 10:
			await process_frame
		var image := root.get_texture().get_image()
		image.save_png("%s_t%d.png" % [out, tick])
		var grade: CanvasLayer = scene.get_node("Grade")
		grade.visible = false
		hud.visible = false
		for f in 3:
			await process_frame
		var bare := root.get_texture().get_image()
		bare.convert(Image.FORMAT_RGBA8)
		var unpainted := 0
		var lowest := -1
		for y in bare.get_height():
			for x in bare.get_width():
				var pixel := bare.get_pixel(x, y)
				if absf(pixel.r - clear.r) < 0.003 and absf(pixel.g - clear.g) < 0.003 and absf(pixel.b - clear.b) < 0.003:
					unpainted += 1
					lowest = maxi(lowest, y)
		bare.save_png("%s_t%d_bare.png" % [out, tick])
		var camera: Camera2D = scene.get_node("Camera")
		print("capture: %s touch %s tick %d zoom %s camera %s covered %.1f; frame %s, pixels of the clear colour %s: %d, the lowest on row %d" % [
			size, touch, tick, camera.zoom, camera.position, scene.covered(Vector2(size)) if scene.has_method("covered") else -1.0,
			image.get_size(), clear, unpainted, lowest])
		grade.visible = true
		hud.visible = true
	quit()
