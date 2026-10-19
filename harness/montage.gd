extends SceneTree
# Lays the given frames side by side, each cropped to its middle `width` x `height`, into one image.
# Args after --: <out> <width>x<height> <png>...
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var size := Vector2i(int(a[1].split("x")[0]), int(a[1].split("x")[1]))
	var shots := a.slice(2)
	var out := Image.create_empty(size.x * shots.size() + 8 * (shots.size() - 1), size.y, false, Image.FORMAT_RGBA8)
	out.fill(Color("171229"))
	for i in shots.size():
		var shot := Image.load_from_file(shots[i])
		var from := (shot.get_size() - size) / 2
		out.blit_rect(shot, Rect2i(from, size), Vector2i(i * (size.x + 8), 0))
	out.save_png(a[0])
	print("montage: %s of %d frames" % [a[0], shots.size()])
	quit()
