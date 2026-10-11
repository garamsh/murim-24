extends SceneTree
# count_blood.gd -- <png>...: counts the pixels of the two blood colours in each image.
func _initialize() -> void:
	for path in OS.get_cmdline_user_args():
		var img := Image.load_from_file(path)
		var n := 0
		for y in img.get_height():
			for x in img.get_width():
				var c := img.get_pixel(x, y).to_html(false)
				if c == "9c1f24" or c == "5c1418":
					n += 1
		print(path.get_file(), " ", n)
	quit()
