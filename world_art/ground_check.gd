## Checks what world_art/ground.gd draws, which needs a renderer, as docs/convention/stack-godot.md
## §Ground leaves the drawn pixels to captures. It is a tool, not a test: no test runs it. Run it by
## hand on a machine with a display, as `godot --path . --script res://world_art/ground_check.gd`,
## and report what it prints in the pull request with the captures. It checks that the bake frees
## its viewport once every area is drawn; that the same relief bakes to the same pixels, detail and
## trees; that an edge between plain and forest running straight along the cells' sides is drawn
## wandering over them, where the same relief drawn without warp keeps it within a few pixels of the
## cells' side; and that streaming shows the areas round the view, an area at most a frame once the
## first are shown, frees those it left, and shows an area's trees again where they were. It exits 0
## when every check passes, 1 when one fails and 2 when it cannot draw.
extends SceneTree

const Ground := preload("res://world_art/ground.gd")
# Plain, with forest from column EDGE on and a block of mountain toward the bottom left, so a chunk
# holds raised strips.
const SIZE := Vector2i(70, 40)
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
	_warped = _ground(relief, {})
	_again = _ground(relief, {})
	_blocky = _ground(relief, {"shader": {"warp": 0.0}})


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
			_first_trees = _warped.trees_of(Vector2i.ZERO)
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
	var warped := _edge(_warped)
	var blocky := _edge(_blocky)
	var side := EDGE * int(Relief.CELL.x)
	var along := func(xs: Array) -> float:
		return float(xs.filter(func(x: int) -> bool: return absi(x - side) <= NEAR_SIDE).size()) / maxi(xs.size(), 1)
	_check("ground_check: the forest's edge runs within %d pixels of the cells' side at x %d on %.2f of rows %s warped, from x %d to %d, and on %.2f without warp, %d and %d rows read" % [
		NEAR_SIDE, side, along.call(warped), ROWS, warped.min(), warped.max(), along.call(blocky), warped.size(), blocky.size()],
			warped.size() >= READ * (ROWS.y - ROWS.x) and blocky.size() >= READ * (ROWS.y - ROWS.x)
			and along.call(warped) <= WARPED_ALONG and along.call(blocky) >= BLOCKY_ALONG)


# For each row of ROWS of `ground`'s first chunk, the first x from which four pixels running are of
# the forest's ramp.
func _edge(ground: Node2D) -> Array:
	var image: Image = ground.images_of(Vector2i.ZERO).ground
	var forest := Relief.KINDS.find("forest")
	var ramp := []
	for step in 4:
		ramp.append(ground.ramp_colour(forest, step))
	var of_forest := func(x: int, y: int) -> bool:
		var colour := image.get_pixel(x, y)
		return ramp.any(func(c: Color) -> bool:
				return absf(c.r - colour.r) < 0.01 and absf(c.g - colour.g) < 0.01 and absf(c.b - colour.b) < 0.01)
	var xs := []
	for y in range(ROWS.x, ROWS.y):
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
