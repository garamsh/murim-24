## Lays the PNGs named after `--`, all of one size, four to a row, into the first argument's PNG.
extends SceneTree


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var frames: Array[Image] = []
	for path in args.slice(1):
		frames.append(Image.load_from_file(path))
	var size := frames[0].get_size()
	var rows := ceili(frames.size() / 4.0)
	var strip := Image.create_empty(size.x * 4, size.y * rows, false, Image.FORMAT_RGBA8)
	for n in frames.size():
		frames[n].convert(Image.FORMAT_RGBA8)
		strip.blit_rect(frames[n], Rect2i(Vector2i.ZERO, size), Vector2i(n % 4 * size.x, n / 4 * size.y))
	strip.save_png(args[0])
	print("strip: %d frames into %s" % [frames.size(), args[0]])
	quit(0)
