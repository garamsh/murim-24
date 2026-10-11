# Checks that every world sprite the production set exports is outlined sel-out (ADR 0069): no
# pixel on a silhouette's edge is the ink ramp's darkest step unless what it borders is ink, such
# as hair, and no pixel anywhere is the former ink outline's colour. The sheets checked are the
# bodies, the structures, the banners, the trees and the army marker; the icons, the player's
# markers and the title keep an ink outline. Control: the same orthodox sheet with its edge
# repainted in that ink is refused, so the check finds a black outline where there is one.
extends RefCounted

const Palette := preload("res://hud/palette.gd")
const SHEETS: Array[String] = [
	"bodies/orthodox", "bodies/demonic", "bodies/unorthodox",
	"structures/house", "structures/hall", "structures/well", "structures/barrels", "structures/fence",
	"structures/granary", "structures/storehouse", "structures/training_ground", "structures/manual_hall",
	"structures/seat_gate", "structures/smithy", "structures/apothecary", "structures/town_wall_south",
	"structures/town_wall_north", "structures/town_wall_west", "structures/town_wall_east",
	"structures/town_corner_tower", "structures/town_gate_tower", "structures/town_steps",
	"structures/town_temple", "structures/town_market_stall", "structures/town_courtyard_house",
	"decorations/banner", "decorations/sect_banner", "terrain/trees", "markers/army_marker",
]
# The ink an outline was drawn in before the restyle.
const FORMER_INK := Color("14101a")
const INKS: Array[Color] = [Palette.INK_1, Palette.INK_2, Palette.INK_4, Palette.INK_5]
const SIDES: Array[Vector2i] = [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]


func run() -> bool:
	var edges := 0
	var faults: Array[String] = []
	for sheet in SHEETS:
		var image := Image.load_from_file(ProjectSettings.globalize_path("res://art/%s.png" % sheet))
		var found := _check(image)
		edges += found.edges
		if found.edges == 0:
			faults.append("%s: no edge" % sheet)
		if found.black > 0 or found.former > 0:
			faults.append("%s: %d edge pixels in ink, %d in the former ink" % [sheet, found.black, found.former])
	var control := Image.load_from_file(ProjectSettings.globalize_path("res://art/bodies/orthodox.png"))
	var repainted := _check(_inked(control))
	var passed: bool = faults.is_empty() and edges > 0 and repainted.black > 0
	print("selout_test: %d sheets, %d edge pixels, faults %s; the orthodox sheet with its edge in ink: %d of %d edge pixels refused: %s" % [
		SHEETS.size(), edges, faults, repainted.black, repainted.edges, "pass" if passed else "FAIL"])
	return passed


# Counts `image`'s edge pixels, those of them in the darkest ink bordering no ink, and its pixels in
# the former ink.
func _check(image: Image) -> Dictionary:
	var found := {"edges": 0, "black": 0, "former": 0}
	for y in image.get_height():
		for x in image.get_width():
			var colour := image.get_pixel(x, y)
			if colour.a == 0.0:
				continue
			if colour.is_equal_approx(FORMER_INK):
				found.former += 1
			if not _edge(image, Vector2i(x, y)):
				continue
			found.edges += 1
			if colour.is_equal_approx(Palette.INK_1) and not _borders_ink(image, Vector2i(x, y)):
				found.black += 1
	return found


# Whether the opaque pixel at `at` has a clear pixel, or the image's border, beside it.
func _edge(image: Image, at: Vector2i) -> bool:
	for side in SIDES:
		var p := at + side
		if p.x < 0 or p.y < 0 or p.x >= image.get_width() or p.y >= image.get_height() or image.get_pixelv(p).a == 0.0:
			return true
	return false


# Whether a pixel beside `at` inside the silhouette, not on its edge, is ink.
func _borders_ink(image: Image, at: Vector2i) -> bool:
	for side in SIDES:
		var p := at + side
		if p.x < 0 or p.y < 0 or p.x >= image.get_width() or p.y >= image.get_height():
			continue
		var colour := image.get_pixelv(p)
		if colour.a > 0.0 and not _edge(image, p) and INKS.any(func(ink: Color) -> bool: return colour.is_equal_approx(ink)):
			return true
	return false


# `image` with every edge pixel repainted in the darkest ink.
func _inked(image: Image) -> Image:
	var out := image.duplicate()
	for y in image.get_height():
		for x in image.get_width():
			if image.get_pixel(x, y).a > 0.0 and _edge(image, Vector2i(x, y)):
				out.set_pixel(x, y, Palette.INK_1)
	return out
