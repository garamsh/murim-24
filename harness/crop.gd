extends SceneTree
# Crops 360x200 round a point of a capture, scaled 4x by nearest neighbour: <png> <x> <y> <out>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	if true:
		var path: String = a[0]
		var image := Image.load_from_file(path)
		var crop := image.get_region(Rect2i(Vector2i(int(a[1]), int(a[2])) - Vector2i(180, 120), Vector2i(360, 200)))
		crop.resize(1440, 800, Image.INTERPOLATE_NEAREST)
		crop.save_png(a[3])
	quit()
