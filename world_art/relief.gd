## The relief the view draws a region with, cell by cell: a kind of ground, a level, whether the
## ground rises to it as a slope, and the material of the face where it drops; and the height of
## the ground at any point, as world_art/ground.gdshader draws it. Until the core's map carries
## levels (ADR 0059), from_terrain() derives them from the core's terrain kinds, and draws a cliff
## only where the core blocks movement: hills rise as slopes a person walks over, mountains in
## three tall steps of rock, and water lies a level below the land round it. It decides nothing.
class_name Relief
extends RefCounted

## The kinds of ground the shader draws, in the order its cell texture indexes them.
const KINDS := ["water", "plain", "hills", "forest", "rock"]
## The materials of faces, in the order the cell texture indexes them: rock, and an earth bank.
const FACES := ["rock_face", "bank_face"]
## The ground kind of each of the core's terrain kinds.
const OF_TERRAIN := {"plain": "plain", "forest": "forest", "hills": "hills", "water": "water", "mountain": "rock"}
## A level's height in pixels, and a cell's width and depth on the ground.
const LEVEL_PX := 16.0
const CELL := Vector2(16, 12)
## The highest and lowest levels the shader looks for.
const TOP := 5
const LOW := -1
## How far, in pixels, noise moves the point a pixel reads its cells from.
const WARP := 32.0
## Pixels the warp's noise varies over, as ground.gdshader's WARP_SCALE.
const WARP_SCALE := 48.0
## How many cells across and down a point on a cell can read its height from: the warp's reach,
## WARP over CELL rounded up, and the cell beside it the interpolation takes.
const FLAT_REACH := Vector2i(3, 4)
## The levels of mountain cells by how many cells they lie from the nearest cell that is not one,
## 1 for the first entry: three tall steps, the last for every cell further in.
const MOUNTAIN_LEVELS := [2, 4, 4, 5]
## The levels of hills cells by the same distance: their edge, then every cell further in.
const HILLS_LEVELS := [1, 2]

var size := Vector2i.ZERO
var _kind := PackedByteArray()
var _level := PackedInt32Array()
var _face := PackedByteArray()
var _slope := PackedByteArray()
# Per cell, 1 where something built stands on it, a structure or paving: where the ground round it
# is flat, the ground there is drawn straight, without warp or dither, so it meets what is built
# along the cell's sides. On or beside a slope or a drop it is drawn as any ground, so the straight
# ground never stands off the slope by a cliff the core does not block.
var _built := PackedByteArray()
# Per cell, its level plus one where every cell height_at() can read from a point on it is of that
# level and none a slope, so the ground on it is flat; 0 where it is not. Built when first read
# after a cell changes.
var _flat := PackedInt32Array()
# The highest level, or -1 until it is read after a cell changes.
var _highest := -1


## A flat relief of `cells` of plain, at level 0, banked where it drops.
func _init(cells: Vector2i) -> void:
	size = cells
	var count := cells.x * cells.y
	_kind.resize(count)
	_kind.fill(KINDS.find("plain"))
	_level.resize(count)
	_face.resize(count)
	_face.fill(FACES.find("bank_face"))
	_slope.resize(count)
	_built.resize(count)


## The relief of a region whose cells are of the core's terrain kinds `terrain`, row by row, in a
## map `cells` across.
static func from_terrain(cells: Vector2i, terrain: PackedStringArray) -> Relief:
	var relief := Relief.new(cells)
	var mountain := PackedByteArray()
	var hills := PackedByteArray()
	mountain.resize(terrain.size())
	hills.resize(terrain.size())
	for i in terrain.size():
		mountain[i] = 1 if terrain[i] == "mountain" else 0
		hills[i] = 1 if terrain[i] == "hills" else 0
	var into_mountain := _depth(cells, mountain)
	var into_hills := _depth(cells, hills)
	for i in terrain.size():
		var at := Vector2i(i % cells.x, i / cells.x)
		var kind: String = OF_TERRAIN.get(terrain[i], "plain")
		match terrain[i]:
			"water":
				relief.lay(at, kind, -1, "bank_face", false)
			"mountain":
				relief.lay(at, kind, MOUNTAIN_LEVELS[mini(into_mountain[i], MOUNTAIN_LEVELS.size()) - 1], "rock_face", false)
			"hills":
				relief.lay(at, kind, HILLS_LEVELS[mini(into_hills[i], HILLS_LEVELS.size()) - 1], "bank_face", true)
			_:
				relief.lay(at, kind, 0, "bank_face", false)
	return relief


# For each cell marked in `marked`, how many cells it lies from the nearest unmarked one, 1 for a
# cell beside one, counting the eight ways; 0 for an unmarked cell. The map's edge counts as
# marked, so ground running off the map keeps its height.
static func _depth(cells: Vector2i, marked: PackedByteArray) -> PackedInt32Array:
	var depth := PackedInt32Array()
	depth.resize(marked.size())
	var frontier: Array[Vector2i] = []
	for i in marked.size():
		if marked[i] == 0:
			continue
		var at := Vector2i(i % cells.x, i / cells.x)
		for d: Vector2i in [Vector2i(-1, -1), Vector2i(0, -1), Vector2i(1, -1), Vector2i(-1, 0), Vector2i(1, 0),
				Vector2i(-1, 1), Vector2i(0, 1), Vector2i(1, 1)]:
			var beside := at + d
			if Rect2i(Vector2i.ZERO, cells).has_point(beside) and marked[beside.y * cells.x + beside.x] == 0:
				depth[i] = 1
				frontier.append(at)
				break
	var reached := 1
	while not frontier.is_empty():
		var next: Array[Vector2i] = []
		reached += 1
		for at: Vector2i in frontier:
			for d: Vector2i in [Vector2i(-1, -1), Vector2i(0, -1), Vector2i(1, -1), Vector2i(-1, 0), Vector2i(1, 0),
					Vector2i(-1, 1), Vector2i(0, 1), Vector2i(1, 1)]:
				var beside := at + d
				if not Rect2i(Vector2i.ZERO, cells).has_point(beside):
					continue
				var i := beside.y * cells.x + beside.x
				if marked[i] == 1 and depth[i] == 0:
					depth[i] = reached
					next.append(beside)
		frontier = next
	# A marked stretch with no unmarked cell at all lies as deep as the deepest level goes.
	for i in marked.size():
		if marked[i] == 1 and depth[i] == 0:
			depth[i] = 1 << 20
	return depth


## Lays cell `at` as ground of `kind`, of KINDS, at `level`, dropping by a face of `face`, of FACES,
## and rising to its level as a slope when `slope`.
func lay(at: Vector2i, kind: String, level: int, face: String, slope: bool) -> void:
	var i := at.y * size.x + at.x
	_kind[i] = KINDS.find(kind)
	_level[i] = level
	_face[i] = FACES.find(face)
	_slope[i] = 1 if slope else 0
	_flat.clear()
	_highest = -1


## Lays cell `at` as ground of `kind`, keeping its level, face and slope.
func cover(at: Vector2i, kind: String) -> void:
	_kind[at.y * size.x + at.x] = KINDS.find(kind)


## Marks cell `at` as one something built stands on.
func build(at: Vector2i) -> void:
	_built[at.y * size.x + at.x] = 1


## Whether something built stands on cell `at`.
func built_at(at: Vector2i) -> bool:
	return _built[_index(at)] == 1


## The kind of cell `at`, a name of KINDS; cells past the edge are the edge's.
func kind_at(at: Vector2i) -> String:
	return KINDS[_kind[_index(at)]]


## The level of cell `at`; cells past the edge are the edge's.
func level_at(at: Vector2i) -> int:
	return _level[_index(at)]


## Whether cell `at` rises to its level as a slope.
func slope_at(at: Vector2i) -> bool:
	return _slope[_index(at)] == 1


## The face material of cell `at`, a name of FACES.
func face_at(at: Vector2i) -> String:
	return FACES[_face[_index(at)]]


## The highest level of any cell, 0 at least.
func highest() -> int:
	if _highest < 0:
		_highest = 0
		for level in _level:
			_highest = maxi(_highest, level)
	return _highest


## The cells as the shader reads them, one texel a cell: its kind in red, its level plus one in
## green, its face material in blue, plus 128 where something is built on it and the ground round
## it is flat, and alpha half where it rises as a slope.
func cell_image() -> Image:
	if _flat.is_empty():
		_find_flat()
	var data := PackedByteArray()
	data.resize(size.x * size.y * 4)
	for i in size.x * size.y:
		data[i * 4] = _kind[i]
		data[i * 4 + 1] = _level[i] + 1
		data[i * 4 + 2] = _face[i] + (128 if _straight(i) else 0)
		data[i * 4 + 3] = 128 if _slope[i] == 1 else 255
	return Image.create_from_data(size.x, size.y, false, Image.FORMAT_RGBA8, data)


## The height of the ground, in levels, at `point`, in pixels on the unraised ground, as
## ground.gdshader's height_at() gives it: its cells' levels read through coordinates warped by
## noise and interpolated, then taken to the nearest level one of them has, except toward cells
## that rise as a slope; on a cell drawn straight, its own level.
func height_at(point: Vector2) -> float:
	if _flat.is_empty():
		_find_flat()
	var on := _index(Vector2i((point / CELL).floor()))
	if _flat[on] > 0:
		return float(_flat[on] - 1)
	# As the shader draws a straight cell; one is always flat, so this answers what the line above
	# does, kept so the two read the same rule.
	if _straight(on):
		return float(_level[on])
	var q := _warped(point)
	var uv := Vector2(q.x / CELL.x, q.y / CELL.y) - Vector2(0.5, 0.5)
	var c := Vector2i(uv.floor())
	var f := uv - uv.floor()
	var corners := [c, c + Vector2i(1, 0), c + Vector2i(0, 1), c + Vector2i(1, 1)]
	var levels: Array[float] = []
	var slopes: Array[float] = []
	for corner: Vector2i in corners:
		levels.append(float(level_at(corner)))
		slopes.append(1.0 if slope_at(corner) else 0.0)
	var level := lerpf(lerpf(levels[0], levels[1], f.x), lerpf(levels[2], levels[3], f.x), f.y)
	var slope := lerpf(lerpf(slopes[0], slopes[1], f.x), lerpf(slopes[2], slopes[3], f.x), f.y)
	var best := levels[0]
	for n in range(1, 4):
		var here := absf(levels[n] - level)
		var there := absf(best - level)
		if here < there or (here == there and levels[n] > best):
			best = levels[n]
	return lerpf(best, level, smoothstep(0.0, 0.5, slope))


## How far, in pixels, the ground at `point` on the unraised ground is drawn above it.
func lift_at(point: Vector2) -> float:
	if _flat.is_empty():
		_find_flat()
	# On flat ground, as most of it is, this is height_at()'s first answer, read without a call.
	var on := clampi(int(point.y / CELL.y), 0, size.y - 1) * size.x + clampi(int(point.x / CELL.x), 0, size.x - 1)
	if _flat[on] > 0:
		return (_flat[on] - 1) * LEVEL_PX
	return height_at(point) * LEVEL_PX


## The point on the unraised ground drawn at `screen`, in the view's pixels: down the column from
## the highest ground that could show there, the first point drawn at or above it, as the shader
## finds what a pixel shows.
func ground_under(screen: Vector2) -> Vector2:
	for step in (TOP - LOW) * int(LEVEL_PX) + 1:
		var y := screen.y + TOP * LEVEL_PX - step
		if y - lift_at(Vector2(screen.x, y)) <= screen.y + 0.5:
			return Vector2(screen.x, y)
	return screen


# Marks each cell whose ground is flat: every cell within FLAT_REACH of it of its level, and none a
# slope. A point on the cell reads, through the warp and the interpolation, only cells that near.
# The lowest and highest level and any slope round each cell are found along the rows, then down
# the columns of those.
func _find_flat() -> void:
	var count := size.x * size.y
	var low := PackedInt32Array()
	var high := PackedInt32Array()
	var sloped := PackedByteArray()
	low.resize(count)
	high.resize(count)
	sloped.resize(count)
	for y in size.y:
		for x in size.x:
			var i := y * size.x + x
			low[i] = _level[i]
			high[i] = _level[i]
			sloped[i] = _slope[i]
			for dx in range(-FLAT_REACH.x, FLAT_REACH.x + 1):
				var j := y * size.x + clampi(x + dx, 0, size.x - 1)
				low[i] = mini(low[i], _level[j])
				high[i] = maxi(high[i], _level[j])
				sloped[i] = sloped[i] | _slope[j]
	_flat.resize(count)
	for y in size.y:
		for x in size.x:
			var level := _level[y * size.x + x]
			var flat := true
			for dy in range(-FLAT_REACH.y, FLAT_REACH.y + 1):
				var j := clampi(y + dy, 0, size.y - 1) * size.x + x
				if low[j] != level or high[j] != level or sloped[j] == 1:
					flat = false
					break
			_flat[y * size.x + x] = level + 1 if flat else 0


# Whether the ground on cell `i` is drawn straight: something is built on it and the ground round it
# is flat.
func _straight(i: int) -> bool:
	return _built[i] == 1 and _flat[i] > 0


func _index(at: Vector2i) -> int:
	return clampi(at.y, 0, size.y - 1) * size.x + clampi(at.x, 0, size.x - 1)


func _warped(p: Vector2) -> Vector2:
	return p + (Vector2(_fbm(p / WARP_SCALE + Vector2(7.0, 0.0), 11), _fbm(p / WARP_SCALE + Vector2(0.0, 5.0), 12))
			- Vector2(0.5, 0.5)) * 2.0 * WARP


func _fbm(p: Vector2, salt: int) -> float:
	var sum := 0.0
	var amp := 0.5
	for i in 3:
		sum += amp * _vnoise(p, salt + i)
		p *= 2.03
		amp *= 0.5
	return sum / 0.875


func _vnoise(p: Vector2, salt: int) -> float:
	var i := p.floor()
	var f := p - i
	f = f * f * (Vector2(3, 3) - 2.0 * f)
	var x := int(i.x)
	var y := int(i.y)
	return lerpf(lerpf(_hash01(x, y, salt), _hash01(x + 1, y, salt), f.x),
			lerpf(_hash01(x, y + 1, salt), _hash01(x + 1, y + 1, salt), f.x), f.y)


## The shader's hash of (x, y, salt) as a share of 1, on its unsigned 32-bit arithmetic.
static func _hash01(x: int, y: int, salt: int) -> float:
	var n := ((x & 0xffffffff) * 374761393 + (y & 0xffffffff) * 668265263 + (salt & 0xffffffff) * 2246822519) \
			& 0xffffffff
	n = ((n ^ (n >> 13)) * 1274126177) & 0xffffffff
	n = n ^ (n >> 16)
	return float(n & 0xffff) / 65536.0
