# Checks what world_art/ground.gd gives the shader and where it places what it scatters, as
# docs/convention/stack-godot.md §Ground asks; the pixels the shader draws are shown by captures
# and checked on a display by ground_check.gd. With no renderer, as under make test, the ground
# bakes nothing and says so. A stand-in drawing each cell flat in its kind takes the viewport's
# place for the rest: every area is baked once and no more however the view moves; streaming shows
# the areas round the view, one more a frame once the first are, frees those it left and shows an
# area's trees again where they were; the same relief and seeds give the same ground and trees, and
# no tree stands where the view says none may. Each area has a seed of its own, and Poisson-disc
# points from one seed are the same, none nearer another than the spacing.
extends RefCounted

const Ground := preload("res://world_art/ground.gd")
# Plain with forest from column FOREST on: four areas across and two down.
const SIZE := Vector2i(100, 40)
const FOREST := 20
# Cells no tree may stand on, inside the forest.
const CLEARED := Rect2i(24, 4, 6, 6)
const VIEW := Vector2(640, 360)
const IMAGE := Vector2i(Ground.CHUNK * 16, Ground.CHUNK * 12 + Ground.TOP_PAD + Ground.BOTTOM_PAD)

var _calls := 0


func run() -> bool:
	return _seeds_pass() and _poisson_pass() and _waits_pass() and _bake_pass() and _stream_pass() \
			and _placement_pass()


static func _relief() -> Relief:
	var relief := Relief.new(SIZE)
	for y in SIZE.y:
		for x in range(FOREST, SIZE.x):
			relief.cover(Vector2i(x, y), "forest")
	return relief


# The stand-in for the shader: each cell of the area's rows drawn flat over its own rectangle, in
# a colour of its kind, or, in the second pass, as what it is.
func _render(key: Vector2i, pass_kind: int, top: Vector2i, relief: Relief) -> Image:
	_calls += 1
	var image := Image.create_empty(IMAGE.x, IMAGE.y, false, Image.FORMAT_RGBA8)
	for y in range(key.y * Ground.CHUNK, mini((key.y + 1) * Ground.CHUNK, SIZE.y)):
		for x in range(key.x * Ground.CHUNK, mini((key.x + 1) * Ground.CHUNK, SIZE.x)):
			var kind := Relief.KINDS.find(relief.kind_at(Vector2i(x, y)))
			var colour := Color8(kind * 10, 120, 40) if pass_kind == 0 \
					else Color8(y - key.y * Ground.CHUNK + 1, 2, kind, 255)
			image.fill_rect(Rect2i(Vector2i(x * 16, y * 12) - top, Vector2i(16, 12)), colour)
	return image


func _ground(options := {}) -> Node2D:
	var relief := _relief()
	var ground := Ground.new()
	ground.standing = Node2D.new()
	ground.render = func(key: Vector2i, pass_kind: int, top: Vector2i) -> Image:
		return _render(key, pass_kind, top, relief)
	ground.prepare(relief, options)
	return ground


static func _free(ground: Node2D) -> void:
	ground.standing.free()
	ground.free()


# Every area of a 100 x 40-cell map has a seed no other has, and a second ground over the same map
# gives each the same. Control: the map's areas are more than one, so distinct seeds are read.
func _seeds_pass() -> bool:
	var first := Ground.new()
	var again := Ground.new()
	first.prepare(Relief.new(SIZE))
	again.prepare(Relief.new(SIZE))
	var seeds := {}
	var same := true
	for key in _keys():
		seeds[first.seed_of(key)] = true
		same = same and first.seed_of(key) == again.seed_of(key)
	var passed := _keys().size() > 1 and seeds.size() == _keys().size() and same
	print("ground_test: %d areas, %d distinct seeds, the same on a second ground %s: %s" % [
		_keys().size(), seeds.size(), same, "pass" if passed else "FAIL"])
	first.free()
	again.free()
	return passed


static func _keys() -> Array[Vector2i]:
	var keys: Array[Vector2i] = []
	for cx in ceili(float(SIZE.x) / Ground.CHUNK):
		for cy in ceili(float(SIZE.y) / Ground.CHUNK):
			keys.append(Vector2i(cx, cy))
	return keys


# The detail spacing's points from one seed, drawn twice, are the same, and no two lie nearer than
# the spacing. Control: another seed gives other points.
func _poisson_pass() -> bool:
	var size := Vector2i(512, 472)
	var points := _points(7, size)
	var again := _points(7, size)
	var other := _points(8, size)
	var nearest := INF
	for i in points.size():
		for j in range(i + 1, points.size()):
			nearest = minf(nearest, Vector2(points[i].x, points[i].y).distance_to(Vector2(points[j].x, points[j].y)))
	var passed := not points.is_empty() and points == again and points != other and nearest >= Ground.DETAIL_SPACING
	print("ground_test: %d points from seed 7, the same drawn again %s, other from seed 8 %s, nearest two %.2f apart against %.1f: %s" % [
		points.size(), points == again, points != other, nearest, Ground.DETAIL_SPACING, "pass" if passed else "FAIL"])
	return passed


static func _points(from_seed: int, size: Vector2i) -> Array[Vector3]:
	var rng := RandomNumberGenerator.new()
	rng.seed = from_seed
	return Ground.poisson(rng, size, Ground.DETAIL_SPACING, Ground.DETAIL_TRIES)


# Without a renderer and with no stand-in, as under make test, the ground holds no viewport,
# stream() bakes nothing and shows nothing, and says so. Setup: the display is the headless one.
func _waits_pass() -> bool:
	var ground := Ground.new()
	ground.prepare(Relief.new(SIZE))
	var streamed := ground.stream(Rect2(Vector2.ZERO, VIEW))
	var headless := DisplayServer.get_name() == "headless"
	var passed := headless and not streamed and not ground.baked and ground.get_child_count() == 0 \
			and ground.shown == 0
	print("ground_test: on the %s display the ground streamed %s, baked %s, holds %d children and shows %d areas: %s" % [
		DisplayServer.get_name(), streamed, ground.baked, ground.get_child_count(), ground.shown, "pass" if passed else "FAIL"])
	ground.free()
	return passed


# The first stream bakes every area, two passes each; moving the view across the map and back
# draws none again. That the viewport is freed is the probe's to check: the stand-in needs none.
func _bake_pass() -> bool:
	_calls = 0
	var ground := _ground()
	var streamed: bool = ground.stream(Rect2(Vector2.ZERO, VIEW))
	var after_bake := _calls
	for at: Vector2 in [Vector2(800, 0), Vector2(1200, 300), Vector2.ZERO]:
		for frame in 4:
			ground.stream(Rect2(at, VIEW))
	var passed: bool = streamed and ground.baked and after_bake == 2 * _keys().size() and _calls == after_bake
	print("ground_test: the first stream drew %d passes for %d areas, %d after moving across and back: %s" % [
		after_bake, _keys().size(), _calls, "pass" if passed else "FAIL"])
	_free(ground)
	return passed


# The view at the map's corner shows the areas round it at once; moved across the map a step at a
# time, no step shows more than BUILDS_PER_FRAME areas it did not show the step before, the first
# area is freed once the view is far from it, and back at the corner it is shown again with its
# trees where they were. Setup: the first area has trees.
func _stream_pass() -> bool:
	var ground := _ground()
	ground.stream(Rect2(Vector2.ZERO, VIEW))
	var first: Array = ground.shown_chunks()
	var trees: Array = ground.trees_of(Vector2i.ZERO).duplicate(true)
	var most_new := 0
	var left := false
	for at: Vector2 in [Vector2(512, 0), Vector2(1100, 0), Vector2(1100, 120), Vector2(512, 0), Vector2.ZERO]:
		for frame in 3:
			var before: Array = ground.shown_chunks()
			ground.stream(Rect2(at, VIEW))
			var fresh: Array = ground.shown_chunks().filter(func(key: Vector2i) -> bool: return not before.has(key))
			most_new = maxi(most_new, fresh.size())
		if at.x >= 1100:
			left = left or not ground.shown_chunks().has(Vector2i.ZERO)
	var back: bool = ground.shown_chunks().has(Vector2i.ZERO) and ground.trees_of(Vector2i.ZERO) == trees
	var passed: bool = first.has(Vector2i.ZERO) and not trees.is_empty() and most_new <= Ground.BUILDS_PER_FRAME \
			and left and back
	print("ground_test: at the corner %s shown, the first area's %d trees; at most %d new a step; the first freed far away %s, shown again with the same trees %s: %s" % [
		first, trees.size(), most_new, left, back, "pass" if passed else "FAIL"])
	_free(ground)
	return passed


# Two grounds of the same relief bake the same image and trees for every area, and no tree stands
# on a cell the view closes to them. Control: with every cell open, trees stand there.
func _placement_pass() -> bool:
	var closed := func(cell: Vector2i) -> bool: return not CLEARED.has_point(cell)
	var first := _ground({"open_at": closed})
	var again := _ground({"open_at": closed})
	var open := _ground()
	for ground: Node2D in [first, again, open]:
		ground.stream(Rect2(Vector2.ZERO, VIEW))
	var same := 0
	for key in _keys():
		if first.images_of(key).ground.get_data() == again.images_of(key).ground.get_data() \
				and first.trees_of(key) == again.trees_of(key):
			same += 1
	var on_cleared := func(ground: Node2D) -> int:
		var count := 0
		for key in _keys():
			for tree: Dictionary in ground.trees_of(key):
				if CLEARED.has_point(Vector2i((tree.foot / Relief.CELL).floor())):
					count += 1
		return count
	var passed: bool = same == _keys().size() and on_cleared.call(first) == 0 and on_cleared.call(open) > 0
	print("ground_test: %d of %d areas the same ground and trees twice; trees on the cleared cells %d, %d with all open: %s" % [
		same, _keys().size(), on_cleared.call(first), on_cleared.call(open), "pass" if passed else "FAIL"])
	for ground: Node2D in [first, again, open]:
		_free(ground)
	return passed
