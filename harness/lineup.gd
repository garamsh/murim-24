extends SceneTree
# Every trade's frames from the production body sheets: a row a trade, each faction's idle, walk
# and work side by side, on ink, scaled 4x. Args: <out png>
const TRADES := ["merchant", "physician", "smith", "farmer", "innkeeper", "beggar", "monk"]
const FACTIONS := ["orthodox", "unorthodox", "demonic"]
func _init() -> void:
	var cell := Vector2i(17, 25)
	var per := 7
	var out := Image.create_empty((per * FACTIONS.size() + FACTIONS.size()) * cell.x, TRADES.size() * cell.y, false, Image.FORMAT_RGBA8)
	out.fill(Color8(28, 26, 40))
	for f in FACTIONS.size():
		var sheet := Image.load_from_file(ProjectSettings.globalize_path("res://art/bodies/%s.png" % FACTIONS[f]))
		sheet.convert(Image.FORMAT_RGBA8)
		var tags := {}
		for t in JSON.parse_string(FileAccess.get_file_as_string("res://art/bodies/%s.json" % FACTIONS[f])).meta.frameTags:
			tags[t.name] = int(t.from)
		for r in TRADES.size():
			var x := f * (per + 1)
			for motion in [["idle", 1], ["walk", 4], ["work", 2]]:
				for i in motion[1]:
					var frame: int = tags[TRADES[r] + "_" + motion[0]] + i
					out.blend_rect(sheet, Rect2i(frame * 16, 0, 16, 24), Vector2i(x * cell.x, r * cell.y))
					x += 1
	out.resize(out.get_width() * 4, out.get_height() * 4, Image.INTERPOLATE_NEAREST)
	out.save_png(OS.get_cmdline_user_args()[0])
	print("lineup ", out.get_size())
	quit()
