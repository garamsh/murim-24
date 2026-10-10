# Checks the relief the view draws a region with: the levels it derives from the core's terrain
# kinds, water a level down, hills rising as slopes to two levels, mountains in three tall steps of
# rock and the rest flat; that the texture it gives the shader holds each cell's kind, level, face
# and slope, the same for the same cells; that no cliff drops between two points of ground the core
# lets a person cross, across or down, with a control where hills rise without a slope and do drop
# by one; that the point a screen pixel shows is the point drawn there, on flat ground and up a
# slope; and that a cell something is built on is marked in the texture to be drawn straight where
# the ground round it is flat, and not on the hills' edge, where it would drop by a cliff.
extends RefCounted

const SIZE := Vector2i(16, 14)
# A map of the core's terrain kinds: a lake top left, hills in the middle and a range bottom right,
# with the rest plain and a strip of forest.
const MAP: Array[String] = [
	"wwww............",
	"wwww............",
	"wwww............",
	"..........ffffff",
	"....hhhhh.......",
	"....hhhhh.......",
	"....hhhhh.......",
	"....hhhhh.......",
	"....hhhhh.......",
	"...........mmmmm",
	"...........mmmmm",
	"...........mmmmm",
	"...........mmmmm",
	"...........mmmmm",
]
const KINDS := {".": "plain", "f": "forest", "h": "hills", "w": "water", "m": "mountain"}


func run() -> bool:
	return _levels_pass() and _texture_pass() and _cliffs_pass() and _under_pass() and _built_pass()


static func _relief(slopes := true) -> Relief:
	var terrain := PackedStringArray()
	for row in MAP:
		for kind in row:
			terrain.append(KINDS[kind])
	var relief := Relief.from_terrain(SIZE, terrain)
	if not slopes:
		for y in SIZE.y:
			for x in SIZE.x:
				var at := Vector2i(x, y)
				if relief.slope_at(at):
					relief.lay(at, relief.kind_at(at), relief.level_at(at), relief.face_at(at), false)
	return relief


# Water a level down with a bank, plain and forest at 0, hills slopes of level 1 at their edge and 2
# within, and mountains rock with a rock face, 2 at their edge, 4 a cell and two in, and 5 from
# the fourth cell in, the map's edge counting as mountain. Control: the hills' edge and centre
# differ, so the depth is read.
func _levels_pass() -> bool:
	var relief := _relief()
	var faults: Array[String] = []
	var expect := func(at: Vector2i, kind: String, level: int, slope: bool) -> void:
		if relief.kind_at(at) != kind or relief.level_at(at) != level or relief.slope_at(at) != slope:
			faults.append("%s %s %d %s" % [at, relief.kind_at(at), relief.level_at(at), relief.slope_at(at)])
	expect.call(Vector2i(0, 0), "water", -1, false)
	expect.call(Vector2i(8, 0), "plain", 0, false)
	expect.call(Vector2i(12, 3), "forest", 0, false)
	expect.call(Vector2i(4, 4), "hills", 1, true)
	expect.call(Vector2i(6, 6), "hills", 2, true)
	expect.call(Vector2i(11, 9), "rock", 2, false)
	expect.call(Vector2i(12, 10), "rock", 4, false)
	expect.call(Vector2i(13, 11), "rock", 4, false)
	expect.call(Vector2i(14, 12), "rock", 5, false)
	var faces := relief.face_at(Vector2i(13, 11)) == "rock_face" and relief.face_at(Vector2i(2, 4)) == "bank_face"
	var passed := faults.is_empty() and faces and relief.highest() == 5
	print("relief_test: levels by kind and depth, faults %s; rock and bank faces %s; highest %d: %s" % [
		faults, faces, relief.highest(), "pass" if passed else "FAIL"])
	return passed


# Between every point and the next pixel south or east, the ground rises or drops by no more than
# half a level while both lie on cells the core lets a person cross, and their neighbours too, so
# the warp cannot reach a blocked cell. Control: the same map with hills rising without a slope drops at
# their edge, so the check sees a cliff where one is drawn.
# The texture holds, a texel a cell, the kind's index in red, the level plus one in green, the face's
# index in blue and alpha half for a slope, read at a cell of each kind; the same cells give the same
# texture twice. Control: one cell changed gives another.
func _texture_pass() -> bool:
	var image := _relief().cell_image()
	var again := _relief().cell_image()
	var changed := _relief()
	changed.lay(Vector2i(8, 0), "forest", 0, "bank_face", false)
	var data := image.get_data()
	var texel := func(at: Vector2i) -> Array:
		var i := (at.y * SIZE.x + at.x) * 4
		return [data[i], data[i + 1], data[i + 2], data[i + 3]]
	var faults: Array[String] = []
	for expect: Array in [[Vector2i(0, 0), [0, 0, 1, 255]], [Vector2i(8, 0), [1, 1, 1, 255]],
			[Vector2i(12, 3), [3, 1, 1, 255]], [Vector2i(6, 6), [2, 3, 1, 128]], [Vector2i(14, 12), [4, 6, 0, 255]]]:
		if texel.call(expect[0]) != expect[1]:
			faults.append("%s %s" % [expect[0], texel.call(expect[0])])
	var same := data == again.get_data()
	var other := changed.cell_image().get_data() != data
	var passed := image.get_size() == SIZE and faults.is_empty() and same and other
	print("relief_test: the cells' texture %s, faults %s; the same cells twice the same %s, a cell changed another %s: %s" % [
		image.get_size(), faults, same, other, "pass" if passed else "FAIL"])
	return passed


func _cliffs_pass() -> bool:
	var drops := _passable_drops(_relief())
	var control := _passable_drops(_relief(false))
	var passed := drops == 0 and control > 0
	print("relief_test: cliffs between passable points %d, with hills unsloped %d: %s" % [
		drops, control, "pass" if passed else "FAIL"])
	return passed


static func _passable_drops(relief: Relief) -> int:
	var drops := 0
	for x in range(0, SIZE.x * int(Relief.CELL.x), 3):
		for y in range(0, SIZE.y * int(Relief.CELL.y) - 1):
			var here := Vector2(x + 0.5, y + 0.5)
			if not _crossable(relief, here):
				continue
			for next: Vector2 in [here + Vector2(0, 1), here + Vector2(1, 0)]:
				if _crossable(relief, next) and absf(relief.height_at(here) - relief.height_at(next)) > 0.5:
					drops += 1
	return drops


# Whether every cell within the warp's reach of `point` is one a person crosses.
static func _crossable(relief: Relief, point: Vector2) -> bool:
	var from := Vector2i(((point - Vector2(Relief.WARP, Relief.WARP)) / Relief.CELL - Vector2(0.5, 0.5)).floor())
	var to := Vector2i(((point + Vector2(Relief.WARP, Relief.WARP)) / Relief.CELL - Vector2(0.5, 0.5)).floor()) + Vector2i.ONE
	for y in range(from.y, to.y + 1):
		for x in range(from.x, to.x + 1):
			if relief.kind_at(Vector2i(x, y)) in ["water", "rock"]:
				return false
	return true


# The point a pixel shows is the point drawn there: on the plain, and on the hills' slopes, where
# the ground is raised. Setup: the slope point is raised by more than a pixel, so finding it takes
# the column search. Control: on a slope the pixel straight above the unraised point shows other
# ground.
func _under_pass() -> bool:
	var relief := _relief()
	var flat := Vector2(130.5, 20.5)
	var slope := Vector2(6.5 * Relief.CELL.x, 6.5 * Relief.CELL.y)
	var raised := slope - Vector2(0, relief.lift_at(slope))
	var setup := relief.lift_at(slope) > 1.0 and relief.lift_at(flat) == 0.0
	var found_flat := relief.ground_under(flat).distance_to(flat) < 1.0
	var found_slope := relief.ground_under(raised).distance_to(slope) < 1.0
	var control := relief.ground_under(slope).distance_to(slope) >= 1.0
	var passed := setup and found_flat and found_slope and control
	print("relief_test: under %s found %s; the slope's %s raised %.1f to %s found %s; its unraised pixel shows %s (setup %s): %s" % [
		flat, relief.ground_under(flat), slope, relief.lift_at(slope), raised, relief.ground_under(raised),
		relief.ground_under(slope), setup, "pass" if passed else "FAIL"])
	return passed


# Cells built on along the hills' edge, where a straight ground at their own level would stand off
# the slope by a cliff, are not marked straight, and no cliff drops between passable points.
# Control: a cell built on in the open plain is marked, 128 more in its texel's blue, its
# neighbour not.
func _built_pass() -> bool:
	var edge: Array[Vector2i] = [Vector2i(4, 6), Vector2i(8, 8), Vector2i(6, 9)]
	var open := Vector2i(13, 1)
	var built := _relief()
	for cell in edge:
		built.build(cell)
	built.build(open)
	var data := built.cell_image().get_data()
	var unbuilt := _relief().cell_image().get_data()
	var blue := func(bytes: PackedByteArray, cell: Vector2i) -> int: return bytes[(cell.y * SIZE.x + cell.x) * 4 + 2]
	var edge_marked := edge.filter(func(cell: Vector2i) -> bool: return blue.call(data, cell) != blue.call(unbuilt, cell))
	var drops := _passable_drops(built)
	var beside := open + Vector2i(1, 0)
	var open_marked: bool = blue.call(data, open) == blue.call(unbuilt, open) + 128 \
			and blue.call(data, beside) == blue.call(unbuilt, beside)
	var passed: bool = edge.all(func(cell: Vector2i) -> bool: return built.built_at(cell)) and edge_marked.is_empty() \
			and drops == 0 and open_marked
	print("relief_test: built on the hills' edge %s, marked straight %s, cliffs between passable points %d; built on the plain at %s marked %s: %s" % [
		edge, edge_marked, drops, open, open_marked, "pass" if passed else "FAIL"])
	return passed
