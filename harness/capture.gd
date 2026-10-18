extends SceneTree
# Args after --: <out prefix> <ticks...>
var scene: Node2D
func _init() -> void:
	var args := OS.get_cmdline_user_args()
	var out: String = args[0]
	var size := Vector2i(int(args[1].split("x")[0]), int(args[1].split("x")[1]))
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	TranslationServer.set_locale("ko")
	scene = load("res://play/play.tscn").instantiate()
	scene.birth_village = 0
	scene.save_directory = OS.get_user_data_dir().path_join("capture398")
	root.add_child(scene)
	await process_frame
	DisplayServer.window_set_size(size)
	root.size = size
	for f in 5:
		await process_frame
	scene.start(2026)
	scene.fit_view()
	var done := 0
	for i in range(2, args.size()):
		var tick := int(args[i])
		while done < tick:
			scene.step()
			done += 1
		for f in 10:
			await process_frame
		var image := root.get_texture().get_image()
		image.save_png("%s_t%d.png" % [out, tick])
		print("win ", DisplayServer.window_get_size(), " root ", root.size, " visible ", root.get_visible_rect().size, " screen ", DisplayServer.screen_get_size())
		print("captured ", out, " tick ", tick, " size ", image.get_size(), " talk ", scene.talk_to, " prompt ", scene.get_node("Hud").talk_prompt.slip_rect())
	quit()
