extends SceneTree
# newred.gd -- <before.png> <after.png> <scale>: prints the native pixels in 9c1f24 in after not in before.
func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	var b := Image.load_from_file(a[0]); var c := Image.load_from_file(a[1]); var k := int(a[2])
	var out := []
	for y in range(0, c.get_height(), k):
		for x in range(0, c.get_width(), k):
			if c.get_pixel(x, y).to_html(false) == "9c1f24" and b.get_pixel(x, y).to_html(false) != "9c1f24":
				out.append(Vector2i(x / k, y / k))
	print(out.size(), " ", out)
	quit()
