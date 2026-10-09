# Compares each PNG in <before> with the same name in <after>, pixel by pixel, printing the count
# of differing pixels and their bounding box.
extends SceneTree


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	for name in DirAccess.get_files_at(args[0]):
		if not name.ends_with(".png"):
			continue
		var a := Image.load_from_file(args[0].path_join(name))
		var b := Image.load_from_file(args[1].path_join(name))
		var count := 0
		var box := Rect2i()
		for y in a.get_height():
			for x in a.get_width():
				if a.get_pixel(x, y) != b.get_pixel(x, y):
					box = Rect2i(x, y, 1, 1) if count == 0 else box.expand(Vector2i(x, y))
					count += 1
		print("%s %d %s" % [name, count, box if count > 0 else ""])
	quit(0)
