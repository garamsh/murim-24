## The ground of a region drawn off the cell grid (#212), baked once when the map is built and
## streamed by chunk round the camera. bake() draws every chunk of CHUNK x CHUNK cells with
## ground.gdshader into a viewport freed as soon as the last is drawn, scatters detail over it and
## decorative trees in clusters by Poisson-disc spacing from a seed the chunk's index gives, darkens
## the ground under each tree, and cuts the chunk into its ground at level 0 and below and a strip
## for each raised row and level. stream() then shows the chunks within STREAM_MARGIN of the screen,
## at most BUILDS_PER_FRAME a frame once the first are shown, and frees those a chunk further out:
## the ground under everything standing, and the strips and trees among what stands, each sorted by
## its row so whoever is behind a raised row is drawn behind it. It decides nothing.
extends Node2D

const SHADER := preload("res://world_art/ground.gdshader")
const TREES := "res://art/terrain/trees"
## Each kind's ramp of four colours, dark to light, a frame tagged by its name, then the earth
## bank's; and the detail scattered over the ground, drawn in the ink ramp where it takes the
## ground's ramp.
const RAMPS := "res://art/terrain/ramps"
const DETAIL := "res://art/terrain/detail"
const RAMP_TAGS := ["water", "plain", "hills", "forest", "rock", "bank"]
const DETAIL_ROOT := Vector2i(2, 3)
# The ink ramp, whose colours in a detail frame stand for steps 0 to 3 of the ground's ramp.
const INK := [Color("14101a"), Color("2a2433"), Color("3d3647"), Color("524a5e")]
## Cells a chunk is wide and deep: ADR 0042's areas.
const CHUNK := 32
## Pixels past the screen within which chunks are shown.
const STREAM_MARGIN := 64.0
## Chunks shown in one frame at most, nearest the screen first, once the first are.
const BUILDS_PER_FRAME := 1
## Rows of a chunk's image above its own, for the ground its raised rows lift, and below, for water
## and the faces that drop to it.
const TOP_PAD := 88
const BOTTOM_PAD := 40
## The colour shadows on the ground lean toward, and how far.
const SHADE := Color(0.078, 0.094, 0.157)
const SHADE_SHARE := 0.38
## Poisson spacing, in pixels, of detail and of trees, and the tries made for each per chunk.
const DETAIL_SPACING := 4.2
const DETAIL_TRIES := 2600
const TREE_SPACING := 8.0
const TREE_TRIES := 1400
## Where a tree's frame holds its foot.
const TREE_FOOT := Vector2(16, 36)
# A kind of the shader's what-pass from which a pixel is a face, not ground.
const FACE_KINDS_FROM := 20
# The kind whose ramp a detail takes, where it is not the ground's it lies on.
const DETAIL_RAMP := {"litter": "forest", "reed": "forest"}
# More than the levels the what-pass can encode, plus two, so a row offset and a level make one key.
const LEVELS := 16

## Chunks shown now, the most shown at once, and the bytes their images hold now.
var shown := 0
var most_shown := 0
var shown_bytes := 0
## How long each chunk took to bake, and to show as it streamed in, in microseconds.
var bake_usec := PackedInt64Array()
var show_usec := PackedInt64Array()
## Whether the bake has run, and its viewport been freed.
var baked := false
## The node strips and trees are sorted in, with everything else standing.
var standing: Node2D
## Draws an area as ground.gdshader does, given its key, the pass (0 colour, 1 what each pixel is)
## and the unraised pixel its image's top-left lies on, returning the image; when not set, the
## shader draws it in a viewport. A test gives one of its own, having no renderer.
var render := Callable()

var _relief: Relief
var _options := {}
var _chunks := {}
var _shown := {}
var _baker: SubViewport
var _material: ShaderMaterial
var _ramps: Array[Color] = []
var _trees := {}
# Each detail tag's frames, each a list of [offset from its root, a colour or the step of the
# ground's ramp it takes].
var _detail := {}
var _asked_at := -1


## Prepares to draw `relief` from the next frame on, where the renderer can draw; `options` may hold
## "shader", values of ground.gdshader's uniforms by name, and "open_at", a Callable taking a cell
## and returning whether a decorative tree may stand on it.
func prepare(relief: Relief, options := {}) -> void:
	_relief = relief
	_options = options
	_ramps.clear()
	var sheet: Image = load(RAMPS + ".png").get_image()
	var tags := _tags(RAMPS)
	for tag: String in RAMP_TAGS:
		for step in 4:
			_ramps.append(sheet.get_pixel(tags[tag] * 4 + step, 0))
	_read_detail()
	if render.is_valid():
		_asked_at = Engine.get_process_frames() - 2
		return
	if DisplayServer.get_name() == "headless":
		return
	_baker = SubViewport.new()
	_baker.size = Vector2i(CHUNK * int(Relief.CELL.x), CHUNK * int(Relief.CELL.y) + TOP_PAD + BOTTOM_PAD)
	_baker.transparent_bg = true
	_baker.disable_3d = true
	# The viewport draws only when bake() forces it, so it never draws again behind the bake's back.
	_baker.render_target_update_mode = SubViewport.UPDATE_DISABLED
	_material = ShaderMaterial.new()
	_material.shader = SHADER
	var rect := ColorRect.new()
	rect.size = Vector2(_baker.size)
	rect.material = _material
	_baker.add_child(rect)
	add_child(_baker)
	var cells: Image = relief.cell_image()
	_material.set_shader_parameter("cells", ImageTexture.create_from_image(cells))
	_material.set_shader_parameter("map_size", relief.size)
	_material.set_shader_parameter("image_size", Vector2(_baker.size))
	var ramps := PackedVector3Array()
	for colour in _ramps:
		ramps.append(Vector3(colour.r, colour.g, colour.b))
	_material.set_shader_parameter("ramps", ramps)
	var uniforms: Dictionary = options.get("shader", {})
	for name: String in uniforms:
		_material.set_shader_parameter(name, uniforms[name])
	_asked_at = Engine.get_process_frames()


## Colour `step`, 0 darkest, of kind `kind`'s ramp, an index of Relief.KINDS.
func ramp_colour(kind: int, step: int) -> Color:
	return _ramps[kind * 4 + clampi(step, 0, 3)]


## Shows the chunks round `view`, the screen in pixels, baking every chunk first once the
## renderer can draw them; returns whether the ground is baked.
func stream(view: Rect2) -> bool:
	if not baked:
		# The viewport draws from the frame after it entered the tree.
		if (_baker == null and not render.is_valid()) or Engine.get_process_frames() <= _asked_at + 1:
			return false
		_bake()
	var reach := view.grow(STREAM_MARGIN).grow_individual(0, 0, 0, _relief.highest() * Relief.LEVEL_PX)
	var wanted := _chunks_over(reach)
	var kept := _chunks_over(reach.grow(CHUNK * Relief.CELL.y))
	for key: Vector2i in _shown.keys():
		if not kept.has(key):
			_hide(key)
	var missing: Array = wanted.keys().filter(func(key: Vector2i) -> bool: return not _shown.has(key))
	var centre := view.get_center()
	missing.sort_custom(func(a: Vector2i, b: Vector2i) -> bool:
			return chunk_rect(a).get_center().distance_squared_to(centre) \
					< chunk_rect(b).get_center().distance_squared_to(centre))
	for key: Vector2i in missing.slice(0, BUILDS_PER_FRAME if not _shown.is_empty() else missing.size()):
		_show(key)
	shown = _shown.size()
	most_shown = maxi(most_shown, shown)
	return true


## Pixels the chunk at `key` covers on the unraised ground.
func chunk_rect(key: Vector2i) -> Rect2:
	return Rect2(Vector2(key) * CHUNK * Relief.CELL, CHUNK * Relief.CELL)


## The keys of the chunks shown now.
func shown_chunks() -> Array:
	return _shown.keys()


## The decorative trees of the chunk at `key`, each {foot, kind, variant, level}, once baked.
func trees_of(key: Vector2i) -> Array:
	return _chunks.get(key, {}).get("trees", [])


## The baked images of the chunk at `key`: "ground", its image of the ground at level 0 and below,
## "ground_at", where its top-left is drawn, and "strips", {Vector2i(row, level): {image, at}}.
func images_of(key: Vector2i) -> Dictionary:
	return _chunks.get(key, {})


func _chunks_over(rect: Rect2) -> Dictionary:
	var keys := {}
	var size := CHUNK * Relief.CELL
	var last := Vector2i((_relief.size.x - 1) / CHUNK, (_relief.size.y - 1) / CHUNK)
	for cx in range(maxi(floori(rect.position.x / size.x), 0), mini(floori(rect.end.x / size.x), last.x) + 1):
		for cy in range(maxi(floori(rect.position.y / size.y), 0), mini(floori(rect.end.y / size.y), last.y) + 1):
			keys[Vector2i(cx, cy)] = true
	return keys


# Bakes every chunk once, then frees the viewport, so nothing draws the shader again.
func _bake() -> void:
	for cx in range((_relief.size.x + CHUNK - 1) / CHUNK):
		for cy in range((_relief.size.y + CHUNK - 1) / CHUNK):
			var started := Time.get_ticks_usec()
			_chunks[Vector2i(cx, cy)] = _compose(Vector2i(cx, cy))
			bake_usec.append(Time.get_ticks_usec() - started)
	if _baker != null:
		_baker.queue_free()
		_baker = null
	baked = true


# Draws the chunk at `key` with the shader, in colour or, with `pass_kind` 1, what each pixel is;
# the image's top-left is `top` on the unraised ground.
func _render(key: Vector2i, pass_kind: int, top: Vector2i) -> Image:
	if render.is_valid():
		return render.call(key, pass_kind, top)
	var origin := key * CHUNK
	_material.set_shader_parameter("origin", Vector2(top))
	_material.set_shader_parameter("row_from", origin.y)
	_material.set_shader_parameter("row_to", mini(origin.y + CHUNK, _relief.size.y))
	_material.set_shader_parameter("pass_kind", pass_kind)
	_baker.render_target_update_mode = SubViewport.UPDATE_ONCE
	RenderingServer.force_draw(false)
	var image := _baker.get_texture().get_image()
	image.convert(Image.FORMAT_RGBA8)
	return image


# The chunk at `key`: the shader's ground, detail and trees scattered over it from the chunk's seed,
# shadows under the trees, cut into its ground and its raised strips.
func _compose(key: Vector2i) -> Dictionary:
	var origin := key * CHUNK
	var top := Vector2i(origin.x * int(Relief.CELL.x), origin.y * int(Relief.CELL.y) - TOP_PAD)
	var colour := _render(key, 0, top)
	var what := _render(key, 1, top).get_data()
	var width := colour.get_width()
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_of(key)
	_scatter_detail(colour, what, width, rng)
	var trees := _scatter_trees(colour.get_size(), what, width, top, rng)
	for tree: Dictionary in trees:
		_contact_shadow(colour, what, width, tree.foot - Vector2(0, tree.level * Relief.LEVEL_PX) - Vector2(top),
				tree.level)
	var composed := _cut(colour, what, width, origin.y, top)
	composed.trees = trees
	return composed


## The seed the chunk at `key` scatters its detail and trees from: its index among the areas.
func seed_of(key: Vector2i) -> int:
	var across := (_relief.size.x + CHUNK - 1) / CHUNK
	return (key.y * across + key.x) * 2654435761 + 1


# What the shader's second pass says a pixel is: [sort row offset, level, kind], a kind of
# FACE_KINDS_FROM and up for a face; a clear pixel has a sort row offset of -1.
func _what(what: PackedByteArray, width: int, at: Vector2i) -> Vector3i:
	if at.x < 0 or at.y < 0 or at.x >= width or (at.y * width + at.x) * 4 + 3 >= what.size():
		return Vector3i(-1, 0, 0)
	var i := (at.y * width + at.x) * 4
	if what[i + 3] == 0:
		return Vector3i(-1, 0, 0)
	return Vector3i(what[i] - 1, what[i + 1] - 2, what[i + 2])


## Points spaced at least `spacing` apart over `size`, from `tries` draws of `rng`, each with a third
## draw for what it becomes.
static func poisson(rng: RandomNumberGenerator, size: Vector2i, spacing: float, tries: int) -> Array[Vector3]:
	var points: Array[Vector3] = []
	var cell := spacing / sqrt(2.0)
	var grid := {}
	for n in tries:
		var p := Vector2(rng.randf() * size.x, rng.randf() * size.y)
		var g := Vector2i((p / cell).floor())
		var free := true
		for gy in range(g.y - 2, g.y + 3):
			for gx in range(g.x - 2, g.x + 3):
				var other: Variant = grid.get(Vector2i(gx, gy))
				if other != null and (other as Vector2).distance_squared_to(p) < spacing * spacing:
					free = false
		if free:
			grid[g] = p
			points.append(Vector3(p.x, p.y, rng.randf()))
	return points


# Tufts and flowers on grass, litter on the forest floor and reeds at the water's edge, from the
# detail sheet: more of them near an edge between kinds, each drawn only on its own ground, taking
# the ramp DETAIL_RAMP names.
func _scatter_detail(colour: Image, what: PackedByteArray, width: int, rng: RandomNumberGenerator) -> void:
	for point: Vector3 in poisson(rng, colour.get_size(), DETAIL_SPACING, DETAIL_TRIES):
		var at := Vector2i(int(point.x), int(point.y))
		var here := _what(what, width, at)
		if here.x < 0 or here.z >= FACE_KINDS_FROM:
			continue
		var edge := false
		for d: Vector2i in [Vector2i(4, 0), Vector2i(-4, 0), Vector2i(0, 4), Vector2i(0, -4)]:
			var other := _what(what, width, at + d)
			if other.x >= 0 and other.z != here.z:
				edge = true
		if point.z > (0.9 if edge else 0.38):
			continue
		var kind: String = Relief.KINDS[here.z]
		var pick := rng.randf()
		var tag := ""
		match kind:
			"plain", "hills":
				tag = "tuft" if pick < 0.72 else "flower"
			"forest":
				tag = "litter"
			"water":
				tag = "reed" if edge else ""
		if tag.is_empty():
			continue
		var frames: Array = _detail[tag]
		var ramp := Relief.KINDS.find(DETAIL_RAMP.get(tag, kind))
		for mark: Array in frames[int(pick * 30) % frames.size()]:
			var p: Vector2i = at + mark[0]
			var there := _what(what, width, p)
			if there.x >= 0 and there.y == here.y and there.z < FACE_KINDS_FROM:
				colour.set_pixelv(p, ramp_colour(ramp, mark[1]) if mark[1] is int else mark[1])


# Decorative trees: dense inside forests, thinning toward their edges by noise, a lone tree now
# and then in the open, each on a cell open to it. Trees the rules know come from the core.
func _scatter_trees(size: Vector2i, what: PackedByteArray, width: int, top: Vector2i,
		rng: RandomNumberGenerator) -> Array:
	var trees := []
	var open_at: Callable = _options.get("open_at", func(_cell: Vector2i) -> bool: return true)
	var forest := Relief.KINDS.find("forest")
	var open := [Relief.KINDS.find("plain"), Relief.KINDS.find("hills")]
	for point: Vector3 in poisson(rng, size, TREE_SPACING, TREE_TRIES):
		var at := Vector2i(int(point.x), int(point.y))
		var here := _what(what, width, at)
		if here.x < 0 or here.z >= FACE_KINDS_FROM:
			continue
		var foot := Vector2(top + at) + Vector2(0, here.y * Relief.LEVEL_PX)
		var cell := Vector2i(int(foot.x / Relief.CELL.x), int(foot.y / Relief.CELL.y))
		if not open_at.call(cell):
			continue
		var density := _value_noise(foot / 30.0)
		var keep := (here.z == forest and point.z < 0.55 + density * 0.6) or (here.z in open and point.z < 0.02)
		if not keep:
			continue
		var pine := here.y >= 2 or rng.randf() < 0.35
		trees.append({"foot": foot, "kind": "pine" if pine else "tree", "variant": rng.randi_range(0, 1 if pine else 2),
				"level": here.y})
	return trees


func _value_noise(p: Vector2) -> float:
	var i := p.floor()
	var f := p - i
	f = f * f * (Vector2(3, 3) - 2.0 * f)
	var corner := func(x: int, y: int) -> float: return float(_hash(x, y, 91) % 1000) / 1000.0
	return lerpf(lerpf(corner.call(int(i.x), int(i.y)), corner.call(int(i.x) + 1, int(i.y)), f.x),
			lerpf(corner.call(int(i.x), int(i.y) + 1), corner.call(int(i.x) + 1, int(i.y) + 1), f.x), f.y)


func _hash(x: int, y: int, salt: int) -> int:
	var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
	h = (h ^ (h >> 13)) * 1274126177
	return absi(h ^ (h >> 16))


# A tree's shadow on the ground round its foot, `at` in the image, on its own level.
func _contact_shadow(colour: Image, what: PackedByteArray, width: int, at: Vector2, level: int) -> void:
	for dy in range(-3, 4):
		for dx in range(-10, 11):
			if pow(dx / 10.0, 2) + pow(dy / 3.0, 2) > 1.0:
				continue
			var p := Vector2i(int(at.x) + dx + 3, int(at.y) + dy + 1)
			var here := _what(what, width, p)
			if here.x < 0 or here.y != level or here.z >= FACE_KINDS_FROM:
				continue
			# A pixel two shadows cover is darkened once: the what-pass's alpha marks it.
			var i := (p.y * width + p.x) * 4
			if what[i + 3] == 254:
				continue
			what[i + 3] = 254
			colour.set_pixelv(p, colour.get_pixelv(p).lerp(SHADE, SHADE_SHARE))


# Cuts the chunk's image, whose top-left is `top`, into its ground at level 0 and below and a piece
# for each raised row and level, keyed by Vector2i(row, level), each cropped to what it holds. It
# works by runs of pixels along each line that belong to one piece.
func _cut(colour: Image, what: PackedByteArray, width: int, row_from: int, top: Vector2i) -> Dictionary:
	var height := colour.get_height()
	# Each run as [key, y, from, to], a key being a raised piece's row offset times LEVELS plus its
	# level; and each key's bounds.
	var runs: Array[PackedInt32Array] = []
	var bounds := {}
	for y in height:
		var x := 0
		while x < width:
			var i := (y * width + x) * 4
			if what[i + 3] == 0 or what[i + 1] <= 2:
				x += 1
				continue
			var key: int = what[i] * LEVELS + what[i + 1]
			var from := x
			x += 1
			while x < width:
				var j := (y * width + x) * 4
				if what[j + 3] == 0 or what[j] * LEVELS + what[j + 1] != key:
					break
				x += 1
			runs.append(PackedInt32Array([key, y, from, x]))
			var b: Variant = bounds.get(key)
			bounds[key] = Rect2i(from, y, x - from, 1) if b == null else (b as Rect2i).merge(Rect2i(from, y, x - from, 1))
	var composed := {"ground": colour, "ground_at": Vector2(top), "strips": {}}
	if runs.is_empty():
		return composed
	var ground := colour.duplicate() as Image
	var images := {}
	for key: int in bounds:
		images[key] = Image.create_empty(bounds[key].size.x, bounds[key].size.y, false, Image.FORMAT_RGBA8)
	for run in runs:
		var at: Vector2i = bounds[run[0]].position
		var line := Rect2i(run[2], run[1], run[3] - run[2], 1)
		(images[run[0]] as Image).blit_rect(colour, line, line.position - at)
		ground.fill_rect(line, Color(0, 0, 0, 0))
	composed.ground = ground
	for key: int in images:
		var strip := Vector2i(row_from + key / LEVELS - 1, key % LEVELS - 2)
		composed.strips[strip] = {"image": images[key], "at": Vector2(top + bounds[key].position)}
	return composed


# Shows the chunk at `key`: its ground under everything standing, and its strips and trees among
# what stands.
func _show(key: Vector2i) -> void:
	var started := Time.get_ticks_usec()
	var composed: Dictionary = _chunks[key]
	var nodes: Array[Node2D] = []
	var ground := Sprite2D.new()
	ground.centered = false
	ground.texture = ImageTexture.create_from_image(composed.ground)
	ground.position = composed.ground_at
	add_child(ground)
	nodes.append(ground)
	var bytes: int = composed.ground.get_data_size()
	for strip_key: Vector2i in composed.strips:
		var piece: Dictionary = composed.strips[strip_key]
		var strip := Sprite2D.new()
		strip.centered = false
		strip.texture = ImageTexture.create_from_image(piece.image)
		# Sorted by its row's north edge, so the row's own people stand on it.
		strip.position = Vector2(piece.at.x, strip_key.x * Relief.CELL.y - 0.5)
		strip.offset = Vector2(0, piece.at.y - strip.position.y)
		standing.add_child(strip)
		nodes.append(strip)
		bytes += piece.image.get_data_size()
	for tree: Dictionary in composed.trees:
		var sprite := Sprite2D.new()
		sprite.texture = _tree_frame(tree.kind, tree.variant)
		sprite.centered = false
		sprite.position = tree.foot
		sprite.offset = -TREE_FOOT - Vector2(0, tree.level * Relief.LEVEL_PX)
		standing.add_child(sprite)
		nodes.append(sprite)
	_shown[key] = {"nodes": nodes, "bytes": bytes}
	shown_bytes += bytes
	show_usec.append(Time.get_ticks_usec() - started)


func _hide(key: Vector2i) -> void:
	for node: Node2D in _shown[key].nodes:
		node.queue_free()
	shown_bytes -= _shown[key].bytes
	_shown.erase(key)


func _tree_frame(kind: String, variant: int) -> AtlasTexture:
	if _trees.is_empty():
		var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(TREES + ".json"))
		_trees.texture = load(TREES + ".png")
		_trees.frames = data.frames
		_trees.tags = {}
		for tag: Dictionary in data.meta.frameTags:
			_trees.tags[tag.name] = int(tag.from)
		_trees.made = {}
	var key := "%s/%d" % [kind, variant]
	if not _trees.made.has(key):
		var frame: Dictionary = _trees.frames[_trees.tags[kind] + variant].frame
		var texture := AtlasTexture.new()
		texture.atlas = _trees.texture
		texture.region = Rect2(frame.x, frame.y, frame.w, frame.h)
		_trees.made[key] = texture
	return _trees.made[key]


# Reads the detail sheet's frames as marks: each pixel's offset from the root, with the step of the
# ground's ramp an ink pixel stands for, or its own colour.
func _read_detail() -> void:
	if not _detail.is_empty():
		return
	var image: Image = load(DETAIL + ".png").get_image()
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(DETAIL + ".json"))
	for tag: Dictionary in data.meta.frameTags:
		var frames := []
		for index in range(int(tag.from), int(tag.to) + 1):
			var frame: Dictionary = data.frames[index].frame
			var marks := []
			for y in int(frame.h):
				for x in int(frame.w):
					var pixel := image.get_pixel(int(frame.x) + x, int(frame.y) + y)
					if pixel.a == 0.0:
						continue
					var step := INK.find(Color(pixel, 1.0))
					marks.append([Vector2i(x, y) - DETAIL_ROOT, step if step >= 0 else pixel])
			frames.append(marks)
		_detail[tag.name] = frames


# A sheet's tags by name, each its first frame.
static func _tags(sheet: String) -> Dictionary:
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(sheet + ".json"))
	var tags := {}
	for tag: Dictionary in data.meta.frameTags:
		tags[tag.name] = int(tag.from)
	return tags
