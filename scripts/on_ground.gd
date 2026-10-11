extends SceneTree
# on_ground.gd -- <out.png> <bg hex> <scale> <sheet.png> <frame w> <frame h> <frames> [<sheet> ...]:
# draws the first <frames> frames of each sheet, a row a sheet, over a flat ground colour, scaled.
func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	var bg := Color(a[1]); var k := int(a[2])
	var rows := []
	var i := 3
	while i + 3 < a.size():
		rows.append([a[i], int(a[i + 1]), int(a[i + 2]), int(a[i + 3])]); i += 4
	var w := 0; var h := 0
	for r in rows:
		w = maxi(w, (r[1] + 4) * r[3]); h += r[2] + 4
	var img := Image.create_empty(w, h, false, Image.FORMAT_RGBA8)
	img.fill(bg)
	var y := 2
	for r in rows:
		var sheet := Image.load_from_file(r[0]); sheet.convert(Image.FORMAT_RGBA8)
		for n in r[3]:
			var src := Rect2i(n * r[1], 0, r[1], r[2])
			img.blend_rect(sheet, src, Vector2i(2 + n * (r[1] + 4), y))
		y += r[2] + 4
	img.resize(w * k, h * k, Image.INTERPOLATE_NEAREST)
	img.save_png(a[0])
	quit()
