# Checks that every world sprite the production set exports is outlined sel-out (ADR 0069), and
# that no pixel anywhere is the former ink outline's colour. On the structures, the banners, the
# trees and the army marker, no pixel on a silhouette's edge is the ink ramp's darkest step unless
# what it borders is ink, such as hair. On a body sheet no edge pixel of a frame but the x-ray ones
# is a step of the skin, whose sel-out read as stains by the face and arm: a body's ring is its
# garment's darkest step. The icons, the player's markers and the title keep an ink outline.
# Controls: the house sheet with its edge repainted in ink is refused, so the check finds a black
# outline where there is one; and the orthodox sheet with one edge pixel by the face repainted in
# skin is refused, so the body check finds skin where there is some.
extends RefCounted

const Palette := preload("res://hud/palette.gd")
const BODIES: Array[String] = ["bodies/orthodox", "bodies/demonic", "bodies/unorthodox"]
const SHEETS: Array[String] = [
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
# art/pixel.lua's skin ramp, dark to light, and the step a face is drawn in, art/sprites.lua's `S`.
const SKIN_RAMP: Array[Color] = [Color("622854"), Color("85394b"), Color("ab5747"), Color("c7895f"),
	Color("d4ac84"), Color("e3cba6"), Color("f2e5c8")]
const SKIN := Color("e3cba6")
const SIDES: Array[Vector2i] = [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]


func run() -> bool:
	var edges := 0
	var faults: Array[String] = []
	for sheet in SHEETS:
		var image := _load(sheet)
		var found := _check(image)
		edges += found.edges
		if found.edges == 0:
			faults.append("%s: no edge" % sheet)
		if found.black > 0 or found.former > 0:
			faults.append("%s: %d edge pixels in ink, %d in the former ink" % [sheet, found.black, found.former])
	for sheet in BODIES:
		var image := _load(sheet)
		var colours := _ring_colours(image)
		edges += colours.values().reduce(func(a: int, b: int) -> int: return a + b, 0)
		if _skin(colours) > 0 or _check(image).former > 0:
			faults.append("%s: %d edge pixels in skin" % [sheet, _skin(colours)])
	var repainted := _check(_inked(_load("structures/house")))
	var spotted := _skin(_ring_colours(_spotted(_load("bodies/orthodox"))))
	var passed: bool = faults.is_empty() and edges > 0 and repainted.black > 0 and spotted == 1
	print("selout_test: %d sheets and %d bodies, %d edge pixels, faults %s; the house with its edge in ink: %d of %d edge pixels refused; the orthodox body with one edge pixel by the face in skin: %d refused: %s" % [
		SHEETS.size(), BODIES.size(), edges, faults, repainted.black, repainted.edges, spotted,
		"pass" if passed else "FAIL"])
	return passed


func _load(sheet: String) -> Image:
	return Image.load_from_file(ProjectSettings.globalize_path("res://art/%s.png" % sheet))


# The colours of a body sheet's edge pixels in its frames before the x-ray ones, its left half,
# each with how many pixels it holds.
func _ring_colours(image: Image) -> Dictionary:
	var colours := {}
	for y in image.get_height():
		for x in image.get_width() / 2:
			if image.get_pixel(x, y).a > 0.0 and _edge(image, Vector2i(x, y)):
				var hex := image.get_pixel(x, y).to_html(false)
				colours[hex] = colours.get(hex, 0) + 1
	return colours


# How many of the edge pixels `colours` counts are a step of the skin.
func _skin(colours: Dictionary) -> int:
	var n := 0
	for hex: String in colours:
		if SKIN_RAMP.any(func(skin: Color) -> bool: return skin.is_equal_approx(Color(hex))):
			n += colours[hex]
	return n


# `image` with the first edge pixel beside skin, the face's, repainted in that skin.
func _spotted(image: Image) -> Image:
	var out := image.duplicate()
	for y in image.get_height():
		for x in image.get_width() / 2:
			if image.get_pixel(x, y).a == 0.0 or not _edge(image, Vector2i(x, y)):
				continue
			for side in SIDES:
				var p := Vector2i(x, y) + side
				if p.x >= 0 and p.y >= 0 and p.x < image.get_width() and p.y < image.get_height() \
						and image.get_pixelv(p).is_equal_approx(SKIN):
					out.set_pixel(x, y, SKIN)
					return out
	return out


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
