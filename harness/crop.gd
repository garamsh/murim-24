extends SceneTree
# CROP="in.png out.png x y w h scale"
func _initialize() -> void:
	var a := OS.get_environment("CROP").split(" ")
	var img := Image.load_from_file(a[0])
	var out := img.get_region(Rect2i(int(a[2]), int(a[3]), int(a[4]), int(a[5])))
	out.resize(out.get_width() * int(a[6]), out.get_height() * int(a[6]), Image.INTERPOLATE_NEAREST)
	out.save_png(a[1])
	quit()
