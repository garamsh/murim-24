extends SceneTree
# Compares the regenerated body sheets with the old ones (frames before the trades) and the mockup.
func _init() -> void:
	var s: String = OS.get_cmdline_user_args()[0]
	var ok := true
	for n in ["orthodox", "demonic", "unorthodox"]:
		var old := Image.load_from_file(s + "/old/" + n + ".png")
		var now := Image.load_from_file("res://art/bodies/" + n + ".png".replace("res://", ""))
		now = Image.load_from_file(ProjectSettings.globalize_path("res://art/bodies/" + n + ".png"))
		var tags: Dictionary = {}
		for t in JSON.parse_string(FileAccess.get_file_as_string("res://art/bodies/" + n + ".json")).meta.frameTags:
			tags[t.name] = [int(t.from), int(t.to)]
		var oldtags: Dictionary = {}
		# old: 34 body frames then 34 xray, same order
		var same := true
		for f in 34:
			if old.get_region(Rect2i(f * 16, 0, 16, 24)).get_data() != now.get_region(Rect2i(f * 16, 0, 16, 24)).get_data():
				same = false
		for f in 34:
			var xf: int = tags["idle_xray"][0] + f
			if old.get_region(Rect2i((34 + f) * 16, 0, 16, 24)).get_data() != now.get_region(Rect2i(xf * 16, 0, 16, 24)).get_data():
				same = false
		var mock_same := true
		for t in ["merchant", "physician", "smith", "farmer", "innkeeper", "beggar", "monk"]:
			var m := Image.load_from_file(s + "/mock/" + t + ".png")
			var mt: Dictionary = {}
			for g in JSON.parse_string(FileAccess.get_file_as_string(s + "/mock/" + t + ".json")).meta.frameTags:
				mt[g.name] = int(g.from)
			for kind in [["idle", 1], ["walk", 4], ["work", 2]]:
				for f in kind[1]:
					var a := m.get_region(Rect2i((mt[n + "_" + kind[0]] + f) * 16, 0, 16, 24))
					var b := now.get_region(Rect2i((tags[t + "_" + kind[0]][0] + f) * 16, 0, 16, 24))
					a.convert(Image.FORMAT_RGBA8); b.convert(Image.FORMAT_RGBA8)
					if a.get_data() != b.get_data():
						mock_same = false
						print(n, " ", t, " ", kind[0], " ", f, " differs from the mockup")
		print(n, ": old frames unchanged ", same, "; trade frames equal the mockup's ", mock_same)
	quit()
