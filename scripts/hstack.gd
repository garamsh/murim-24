extends SceneTree
# hstack.gd -- <out.png> <in.png> ...: lays the images side by side, 8 pixels apart, on ink.
func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	var images: Array[Image] = []
	var w := 0; var h := 0
	for p in a.slice(1):
		var img := Image.load_from_file(p); img.convert(Image.FORMAT_RGBA8); images.append(img)
		w += img.get_width() + 8; h = maxi(h, img.get_height())
	var out := Image.create_empty(w - 8, h, false, Image.FORMAT_RGBA8)
	out.fill(Color("1b1823"))
	var x := 0
	for img in images:
		out.blit_rect(img, Rect2i(Vector2i.ZERO, img.get_size()), Vector2i(x, 0)); x += img.get_width() + 8
	out.save_png(a[0])
	quit()
