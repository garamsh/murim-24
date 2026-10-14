## Stitches each staged run's eight frames into one strip, each cropped round the screen's middle.
extends SceneTree
func _initialize() -> void:
	for kind: String in ["sweep", "dart", "area"]:
		var w := 120 if kind != "dart" else 420
		var count := 16 if kind != "dart" else 6
		var strip := Image.create_empty(count * w, 140, false, Image.FORMAT_RGBA8)
		for n in count:
			var image := Image.load_from_file("res://build/issue184/seq/%s%d-ko.png" % [kind, n * (16 / count)])
			var crop := image.get_region(Rect2i(Vector2i(960 - w / 2, 540 - 90), Vector2i(w, 140)))
			strip.blit_rect(crop, Rect2i(Vector2i.ZERO, crop.get_size()), Vector2i(n * w, 0))
		strip.save_png("res://build/issue184/seq/strip-%s-ko.png" % kind)
	quit()
