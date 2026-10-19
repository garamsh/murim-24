extends SceneTree
# Counts a frame's pixels of one colour, the void's, read from a probe pixel of a reference frame:
# <reference png> <probe x> <probe y> <png...>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var colour := Image.load_from_file(a[0]).get_pixel(int(a[1]), int(a[2]))
	for i in range(3, a.size()):
		var image := Image.load_from_file(a[i])
		var count := 0
		var rows := {}
		for y in image.get_height():
			for x in image.get_width():
				if image.get_pixel(x, y).is_equal_approx(colour):
					count += 1
					rows[y] = true
		print("void: %s of %s: %d pixels on %d rows%s" % [colour.to_html(false), a[i].get_file(), count, rows.size(),
				", rows %d to %d" % [rows.keys().min(), rows.keys().max()] if count > 0 else ""])
	quit()
