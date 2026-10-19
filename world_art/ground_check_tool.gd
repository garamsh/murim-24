## Checks what world_art/ground.gd draws, which needs a renderer, as docs/convention/stack-godot.md
## §Ground leaves the drawn pixels to captures. It is a tool, not a test: no test runs it. Run it by
## hand on a machine with a display, as
## `godot --path . --script res://world_art/ground_check_tool.gd`, and report what it prints in the
## pull request with the captures. It checks that the bake frees its viewport once every area is
## drawn; that the same relief bakes to the same pixels, detail and
## trees; that an edge between plain and forest running straight along the cells' sides is drawn
## wandering over them, where the same relief drawn without warp keeps it within a few pixels of the
## cells' side; that across cells something is built on, the same edge runs straight down the cells'
## side on every row; and that streaming shows the areas round the view, an area at most a frame
## once the first are shown, frees those it left, and shows an area's trees again where they were.
## Given a reach past the map's bottom before the bake, or after it, when it bakes the areas on that
## edge again and frees the viewport, every pixel from above the map's drawn bottom down to that
## reach is painted, beside raised ground on the last row as beside level ground (#400). It exits 0 when every check passes, 1 when one fails and 2 when it cannot draw.
extends SceneTree

const Ground := preload("res://world_art/ground.gd")
# Plain, with forest from column EDGE on and a block of mountain toward the bottom left, so a chunk
# holds raised strips; two chunks deep, so the bottom edge's chunks end with the map.
const SIZE := Vector2i(70, 64)
const EDGE := 24
const MOUNTAIN := Rect2i(2, 28, 10, 8)
# Rows of the image, from its top, the edge is read along: ground at level 0 well away from the
# mountain.
const ROWS := Vector2i(100, 360)
# How near, in pixels, the edge falls to the cells' side on a row where it runs along it, dithered
# by a pixel or two; and the shares of rows where it does, at most when warped and at least when
# not. Detail and tree shadows over the forest's first pixels move a row's reading now and then.
const NEAR_SIDE := 3
const WARPED_ALONG := 0.4
const BLOCKY_ALONG := 0.8
# The least share of rows on which the edge is read: four pixels running of the forest's ramp.
const READ := 0.9
# Cells built on across the edge, below ROWS and clear of the mountain, whose rows the edge must run
# straight down.
const BUILT := Rect2i(EDGE - 3, 25, 6, 4)
# Ground raised on the map's last rows, so the bottom edge runs beside raised ground; and how far
# past the bottom the view is given to show, covered() at 2048×1536 with the touch controls.
const RAISED_EDGE := Rect2i(44, 60, 8, 4)
const PAST := 87.0
const VIEW := Vector2(640, 360)
const FRAMES := 600

var _warped: Node2D
var _again: Node2D
var _blocky: Node2D
var _frame := 0
var _results: Array[bool] = []
var _phase := "bake"
var _first_trees := []
var _path := 0
var _shown_before := 0
var _most_new := 0
var _bakes := 0


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		printerr("ground_check: needs a display to draw")
		quit(2)
		return
	var relief := Relief.new(SIZE)
	for y in SIZE.y:
		for x in range(EDGE, SIZE.x):
			relief.cover(Vector2i(x, y), "forest")
	for y in range(MOUNTAIN.position.y, MOUNTAIN.end.y):
		for x in range(MOUNTAIN.position.x, MOUNTAIN.end.x):
			relief.lay(Vector2i(x, y), "rock", 2 if Rect2i(MOUNTAIN).grow(-1).has_point(Vector2i(x, y)) else 4,
					"rock_face", false)
	for y in range(RAISED_EDGE.position.y, RAISED_EDGE.end.y):
		for x in range(RAISED_EDGE.position.x, RAISED_EDGE.end.x):
			relief.lay(Vector2i(x, y), "rock", 2, "rock_face", false)
	for y in range(BUILT.position.y, BUILT.end.y):
		for x in range(BUILT.position.x, BUILT.end.x):
			relief.build(Vector2i(x, y))
	# No tree stands on the built cells, as world_art.gd keeps them off what is built.
	var open_at := func(cell: Vector2i) -> bool: return not BUILT.has_point(cell)
	_warped = _ground(relief, {"open_at": open_at})
	_again = _ground(relief, {"open_at": open_at})
	_blocky = _ground(relief, {"open_at": open_at, "shader": {"warp": 0.0}})
	# Given its reach past the bottom before it bakes, as the play scene gives it before the first.
	_blocky.past_bottom = PAST


func _ground(relief: Relief, options: Dictionary) -> Node2D:
	var ground := Ground.new()
	ground.standing = Node2D.new()
	root.add_child(ground)
	root.add_child(ground.standing)
	ground.prepare(relief, options)
	return ground


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame > FRAMES:
		printerr("ground_check: ran out of frames in %s" % _phase)
		quit(1)
		return false
	match _phase:
		"bake":
			var view := Rect2(Vector2.ZERO, VIEW)
			if _warped.stream(view) and _again.stream(view) and _blocky.stream(view):
				_phase = "freed"
		"freed":
			# The viewports were queued for freeing as the bake ended; they are gone a frame later.
			_check("ground_check: after the bake the grounds hold %d viewports" % _viewports(), _viewports() == 0)
			_check_same()
			_check_edge()
			_check_built()
			_check_past(_blocky)
			_first_trees = _warped.trees_of(Vector2i.ZERO)
			_bakes = _again.bake_usec.size()
			_again.past_bottom = PAST
			_phase = "reach"
		"reach":
			if _again.stream(Rect2(Vector2.ZERO, VIEW)):
				_phase = "reached"
		"reached":
			var again: int = _again.bake_usec.size() - _bakes
			var across := ceili(float(SIZE.x) / Ground.CHUNK)
			_check("ground_check: given %.0f past the bottom, %d chunks baked again of the %d on the bottom edge, the grounds holding %d viewports" % [
				PAST, again, across, _viewports()], again == across and _viewports() == 0)
			_check_past(_again)
			_phase = "stream"
		"stream":
			_stream_step()
	return false


func _viewports() -> int:
	var count := 0
	for ground: Node2D in [_warped, _again, _blocky]:
		for child in ground.get_children():
			if child is SubViewport:
				count += 1
	return count


# Every chunk of the second ground has the same ground, strips and trees as the first's.
func _check_same() -> void:
	var chunks := 0
	var same := 0
	for cx in ceili(float(SIZE.x) / Ground.CHUNK):
		for cy in ceili(float(SIZE.y) / Ground.CHUNK):
			var key := Vector2i(cx, cy)
			var a: Dictionary = _warped.images_of(key)
			var b: Dictionary = _again.images_of(key)
			chunks += 1
			var strips_same: bool = a.strips.keys() == b.strips.keys()
			for strip: Vector2i in a.strips:
				strips_same = strips_same and b.strips.has(strip) \
						and a.strips[strip].image.get_data() == b.strips[strip].image.get_data()
			if a.ground.get_data() == b.ground.get_data() and strips_same and a.trees == b.trees:
				same += 1
	var raised: int = _warped.images_of(Vector2i(0, 1)).strips.size()
	_check("ground_check: %d of %d chunks baked to the same ground, strips and trees twice, the mountain's chunk holding %d strips" % [
		same, chunks, raised], same == chunks and raised > 0)


# Where the forest begins along each of ROWS on each ground: wandering when warped, straight
# along the cells' side when not.
func _check_edge() -> void:
	var warped := _edge(_warped, ROWS)
	var blocky := _edge(_blocky, ROWS)
	var side := EDGE * int(Relief.CELL.x)
	var along := func(xs: Array) -> float:
		return float(xs.filter(func(x: int) -> bool: return absi(x - side) <= NEAR_SIDE).size()) / maxi(xs.size(), 1)
	_check("ground_check: the forest's edge runs within %d pixels of the cells' side at x %d on %.2f of rows %s warped, from x %d to %d, and on %.2f without warp, %d and %d rows read" % [
		NEAR_SIDE, side, along.call(warped), ROWS, warped.min(), warped.max(), along.call(blocky), warped.size(), blocky.size()],
			warped.size() >= READ * (ROWS.y - ROWS.x) and blocky.size() >= READ * (ROWS.y - ROWS.x)
			and along.call(warped) <= WARPED_ALONG and along.call(blocky) >= BLOCKY_ALONG)


# On every row of the built cells the warped ground's forest begins exactly at the cells' side;
# the control is the same edge below them, which _check_edge() finds wandering.
func _check_built() -> void:
	# The chunk's image starts TOP_PAD rows above the ground's top.
	var rows := Vector2i(BUILT.position.y * int(Relief.CELL.y), BUILT.end.y * int(Relief.CELL.y)) + Vector2i.ONE * Ground.TOP_PAD
	var xs := _edge(_warped, rows)
	var side := EDGE * int(Relief.CELL.x)
	var straight := xs.filter(func(x: int) -> bool: return x == side).size()
	_check("ground_check: across the built cells the forest begins at x %d on %d of %d rows %s, from x %d to %d" % [
		side, straight, rows.y - rows.x, rows, xs.min() if not xs.is_empty() else -1, xs.max() if not xs.is_empty() else -1],
			xs.size() == rows.y - rows.x and straight == xs.size())


# Every pixel of `ground`'s chunks on the map's bottom edge, its ground and its strips together, is
# painted from the highest the last row is raised above the map's bottom down to PAST below it,
# across the map; read beside RAISED_EDGE apart, where the issue saw the strip (#400).
func _check_past(ground: Node2D) -> void:
	var bottom := SIZE.y * int(Relief.CELL.y)
	var rows := Vector2i(bottom - 2 * Relief.LEVEL_PX, bottom + int(PAST))
	var clear := 0
	var beside := 0
	for cx in ceili(float(SIZE.x) / Ground.CHUNK):
		var key := Vector2i(cx, (SIZE.y - 1) / Ground.CHUNK)
		var images: Dictionary = ground.images_of(key)
		var at: Vector2 = images.ground_at
		var whole := (images.ground as Image).duplicate() as Image
		for strip: Vector2i in images.strips:
			var piece: Dictionary = images.strips[strip]
			var image: Image = piece.image
			whole.blend_rect(image, Rect2i(Vector2i.ZERO, image.get_size()), Vector2i(piece.at - at))
		for y in range(rows.x, rows.y):
			for x in whole.get_width():
				var column := int(at.x) + x
				if column >= SIZE.x * int(Relief.CELL.x):
					break
				var inside := y - int(at.y) < whole.get_height()
				if not inside or whole.get_pixel(x, y - int(at.y)).a < 1.0:
					clear += 1
					if column >= RAISED_EDGE.position.x * int(Relief.CELL.x) - 16 \
							and column < RAISED_EDGE.end.x * int(Relief.CELL.x) + 16:
						beside += 1
	_check("ground_check: from y %d to %d across the map's bottom edge, %d pixels unpainted, %d of them beside the raised ground on the last rows" % [
		rows.x, rows.y, clear, beside], clear == 0)


# For each row of `rows` of `ground`'s first chunk, the first x from which four pixels running are
# of the forest's ramp.
func _edge(ground: Node2D, rows: Vector2i) -> Array:
	var image: Image = ground.images_of(Vector2i.ZERO).ground
	var forest := Relief.KINDS.find("forest")
	var ramp := []
	for step in ground.STEPS:
		ramp.append(ground.ramp_colour(forest, step))
	var of_forest := func(x: int, y: int) -> bool:
		var colour := image.get_pixel(x, y)
		return ramp.any(func(c: Color) -> bool:
				return absf(c.r - colour.r) < 0.01 and absf(c.g - colour.g) < 0.01 and absf(c.b - colour.b) < 0.01)
	var xs := []
	for y in range(rows.x, rows.y):
		for x in range(EDGE * int(Relief.CELL.x) - 40, EDGE * int(Relief.CELL.x) + 40):
			if range(4).all(func(d: int) -> bool: return of_forest.call(x + d, y)):
				xs.append(x)
				break
	return xs


# The view moves down and right a chunk a frame across the map and back: no frame shows more than
# BUILDS_PER_FRAME chunks it did not show the frame before, the first chunk is freed once left, and
# shown again with its trees where they were.
func _stream_step() -> void:
	var stops := [Vector2.ZERO, Vector2(512, 0), Vector2(1024, 384), Vector2(1024, 384), Vector2(512, 0), Vector2.ZERO]
	var at: Vector2 = stops[mini(_path / 20, stops.size() - 1)]
	var before: Array = _warped.shown_chunks()
	_warped.stream(Rect2(at, VIEW))
	var now: Array = _warped.shown_chunks()
	var fresh := now.filter(func(key: Vector2i) -> bool: return not before.has(key)).size()
	if _path > 0:
		_most_new = maxi(_most_new, fresh)
	if _path == 3 * 20 - 1:
		_check("ground_check: at %s the first chunk is shown %s" % [at, now.has(Vector2i.ZERO)], not now.has(Vector2i.ZERO))
	_path += 1
	if _path == stops.size() * 20:
		var back: bool = now.has(Vector2i.ZERO) and _warped.trees_of(Vector2i.ZERO) == _first_trees
		_check("ground_check: back at the start the first chunk is shown with its %d trees where they were %s; at most %d new chunks a frame" % [
			_first_trees.size(), back, _most_new], back and not _first_trees.is_empty() and _most_new <= Ground.BUILDS_PER_FRAME)
		var passed := _results.all(func(result: bool) -> bool: return result)
		print("ground_check: %d of %d checks passed" % [_results.count(true), _results.size()])
		quit(0 if passed else 1)


func _check(line: String, passed: bool) -> void:
	_results.append(passed)
	print(line + ": " + ("pass" if passed else "FAIL"))
