# Sets the genealogy panel's details column of <before>/genealogy_<locale>.png beside the same of
# <after>, before on the left.
extends SceneTree

const COLUMN := Rect2i(1096, 344, 292, 396)


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var pair := Image.create_empty(2 * COLUMN.size.x + 24, COLUMN.size.y + 16, false, Image.FORMAT_RGBA8)
	pair.fill(Color("14101a"))
	for n in 2:
		var shot := Image.load_from_file(args[n].path_join("genealogy_%s.png" % args[2]))
		shot.convert(Image.FORMAT_RGBA8)
		pair.blit_rect(shot, COLUMN, Vector2i(8 + n * (COLUMN.size.x + 8), 8))
	pair.save_png(args[3])
	quit(0)
