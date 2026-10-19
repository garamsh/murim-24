extends SceneTree
# Draws seed 2026's region at midday (tick 900 of a 1800-tick day) in the world art scene alone, the
# camera at 3x on each cell given, and saves the frame and a 360x200 crop round the cell at 4x.
# Args after --: <out prefix> <x,y>...
var art: Node2D
var camera: Camera2D
var world: WorldAdapter
var cells := []
var out: String
var frame := 0
var shot := 0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	out = args[0]
	for i in range(1, args.size()):
		cells.append(Vector2i(int(args[i].split(",")[0]), int(args[i].split(",")[1])))
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	root.size = Vector2i(1920, 1080)
	world = WorldAdapter.create_region(2026, 0)
	for t in 900:
		world.tick()
	art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(art)
	art.show_world(world, world.army_count() - 1)
	camera = Camera2D.new()
	camera.zoom = Vector2(3, 3)
	root.add_child(camera)
	camera.make_current()

func _process(_d: float) -> bool:
	frame += 1
	var cell: Vector2i = cells[shot]
	var ground := Vector2(cell.x * 16 + 8, cell.y * 12 + 6)
	var at := ground - Vector2(0, art.relief.lift_at(ground))
	camera.position = at
	art.refresh(Rect2(at - Vector2(320, 180), Vector2(640, 360)))
	if frame % 30 == 0 and art.get_node("Ground").baked:
		var image := root.get_texture().get_image()
		var name := "%s_%d_%d" % [out, cell.x, cell.y]
		image.save_png(name + ".png")
		var crop := image.get_region(Rect2i(Vector2i(960, 540) - Vector2i(180, 100), Vector2i(360, 200)))
		crop.resize(1440, 800, Image.INTERPOLATE_NEAREST)
		crop.save_png(name + "_crop4x.png")
		print("cellshot: %s at %s" % [name, at])
		shot += 1
		if shot == cells.size():
			quit()
	return frame > 3000
