# Tiles the captures named in VIEWS for one locale, each scaled to a quarter through mipmaps, four across, in order.
extends SceneTree

const VIEWS := ["title", "new_game", "settings", "hud", "genealogy", "genealogy_moved", "genealogy_entered",
	"genealogy_kp_entered", "sect", "sect_tab", "arts", "arts_down", "factions", "factions_down", "dialogue",
	"dialogue_news", "pause", "rebirth", "closed", "hud_touch", "genealogy_touch", "arts_touch", "factions_touch",
	"sect_touch"]


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var sheet := Image.create_empty(4 * 480 + 5 * 8, 6 * 270 + 7 * 8, false, Image.FORMAT_RGBA8)
	sheet.fill(Color("14101a"))
	for n in VIEWS.size():
		var shot := Image.load_from_file(args[0].path_join("%s_%s.png" % [VIEWS[n], args[1]]))
		shot.convert(Image.FORMAT_RGBA8)
		shot.resize(480, 270, Image.INTERPOLATE_TRILINEAR)
		sheet.blit_rect(shot, Rect2i(0, 0, 480, 270), Vector2i(8 + (n % 4) * 488, 8 + (n / 4) * 278))
	sheet.save_png(args[2])
	quit(0)
