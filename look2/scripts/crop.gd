extends SceneTree
# crop.gd -- <in> <out> <x> <y> <w> <h> <scale>
func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	var img := Image.load_from_file(a[0])
	var r := img.get_region(Rect2i(int(a[2]), int(a[3]), int(a[4]), int(a[5])))
	r.resize(r.get_width() * int(a[6]), r.get_height() * int(a[6]), Image.INTERPOLATE_NEAREST)
	r.save_png(a[1])
	quit()
