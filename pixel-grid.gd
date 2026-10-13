extends SceneTree
var frame := 0
var grade: CanvasLayer
func _initialize() -> void:
	root.size = Vector2i(1440, 810)
	var white := ColorRect.new()
	white.size = Vector2(root.size)
	white.color = Color.WHITE
	root.add_child(white)
	grade = load("res://grade/grade.tscn").instantiate()
	root.add_child(grade)
	grade.art_pixel = 3.0
	grade.show_time(0.5, 1)
func _process(_delta: float) -> bool:
	frame += 1
	if frame == 10:
		var picture := root.get_texture().get_image()
		var split := 0
		var dark := 0
		var bright := 0
		for y in range(0, 810, 3):
			for x in range(0, 1440, 3):
				var colour := picture.get_pixel(x, y)
				if colour.r < 0.95:
					dark += 1
				else:
					bright += 1
				var different := false
				for dy in 3:
					for dx in 3:
						different = different or not picture.get_pixel(x + dx, y + dy).is_equal_approx(colour)
				if different:
					split += 1
		print("pixel grid: %d 3x3 art cells, %d dark %d bright, %d split cells" % [480 * 270, dark, bright, split])
		quit(0 if split == 0 and dark > 0 and bright > 0 else 1)
	return false
