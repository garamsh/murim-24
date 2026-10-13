extends SceneTree
func _initialize() -> void:
	var off := Image.load_from_file("res://build/issue310/evidence/off-ko.png")
	var failures := 0
	for name in ["day", "dusk", "night"]:
		var picture := Image.load_from_file("res://build/issue310/evidence/%s-ko.png" % name)
		var changed := 0
		for rect in [Rect2i(10, 10, 330, 118), Rect2i(10, 144, 330, 30), Rect2i(10, 186, 194, 32), Rect2i(1746, 10, 162, 118), Rect2i(1676, 144, 230, 228)]:
			for y in range(rect.position.y, rect.end.y):
				for x in range(rect.position.x, rect.end.x):
					if not picture.get_pixel(x, y).is_equal_approx(off.get_pixel(x, y)):
						changed += 1
		print("capture HUD control: %s changes %d pixels across status, map and shortcut panels" % [name, changed])
		if changed != 0:
			failures += 1
	quit(0 if failures == 0 else 1)
