extends SceneTree

const REACH := 6.0
const Play := preload("res://play/play.gd")
# Captures the play scene, Korean, born into a village, at hours of a day.
# Args after --: <out prefix> <width>x<height> <save dir> <unused> <hour>...: loads the save, then captures each hour of the next day.
func _init() -> void:
	var args := OS.get_cmdline_user_args()
	var out: String = args[0]
	var size := Vector2i(int(args[1].split("x")[0]), int(args[1].split("x")[1]))
	TranslationServer.set_locale("ko")
	var scene: Node2D = load("res://play/play.tscn").instantiate()
	scene.save_directory = args[2]
	root.add_child(scene)
	await process_frame
	DisplayServer.window_set_size(size)
	root.size = size
	for f in 5:
		await process_frame
	print("resumed ", scene.resume_saved())
	scene.fit_view()
	var world: WorldAdapter = scene.world
	var day_ticks: int = world.date().ticks_per_day
	var day := int(args[3])
	for i in range(4, args.size()):
		var hour := int(args[i])
		var target := (world.tick_count() / day_ticks + (1 if hour * day_ticks / 24 < world.tick_count() % day_ticks or i == 4 else 0)) * day_ticks + hour * day_ticks / 24
		while world.tick_count() < target:
			scene.step()
		for f in 6:
			await process_frame
		# The camera on the village's people rather than the player: their mean position within
		# REACH cells of the village, the scene's own frame stopped so it stays.
		var village: Dictionary = world.villages()[int(args[3])]
		var area := Rect2(Vector2(village.corner), Vector2(village.area)).grow(REACH)
		var sum := Vector2.ZERO
		var counted := 0
		for army in world.army_count():
			for person in world.roster(army):
				var cell := world.person_position(person) / WorldAdapter.CELL_SIZE
				if area.has_point(cell):
					sum += scene.get_node("WorldArt").to_screen(world.person_position(person))
					counted += 1
		scene.set_process(true)
		var camera: Camera2D = scene.get_node("Camera")
		var screen := Vector2(size) / camera.zoom
		var rows := world.map_rows()
		var bounds: Rect2 = Play.drawn_bounds(Vector2(rows[0].length(), rows.size()) * Relief.CELL, scene.get_node("WorldArt").relief)
		camera.position = Play.camera_centre(sum / maxi(counted, 1), screen, bounds, camera.zoom.x)
		scene.set_process(false)
		for f in 6:
			scene.show_view(Rect2(camera.position - screen / 2.0, screen))
			scene.show_talk()
			scene.show_names()
			await process_frame
		var counts := {}
		for army in world.army_count():
			for person in world.roster(army):
				var key := "%d/%d" % [world.person_occupation(person), world.person_activity(person)]
				counts[key] = counts.get(key, 0) + 1
		var names := world.occupation_names()
		var placed := []
		for army in world.army_count():
			for person in world.roster(army):
				if area.has_point(world.person_position(person) / WorldAdapter.CELL_SIZE):
					var o := world.person_occupation(person)
					var on_screen: Vector2 = (scene.get_node("WorldArt").to_screen(world.person_position(person)) - camera.position) * camera.zoom + Vector2(size) / 2.0
					placed.append("%d %s activity %d at %s" % [person, names[o] if o >= 0 else "none", world.person_activity(person), on_screen.round()])
		print("people: ", placed)
		var image := root.get_texture().get_image()
		image.save_png("%s_%02d.png" % [out, hour])
		scene.set_process(true)
		print("captured %s village %s, %d people near, day %d hour %d tick %d size %s; occupation/activity counts %s" % [out.get_file(), args[3], counted, day, hour,
				world.tick_count(), image.get_size(), counts])
	quit()
