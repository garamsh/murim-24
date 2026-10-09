# Mockup of the 2.5D view proposed for #200: the world seen from Albion's 3/4 angle, drawn in the
# art under art/mockup/angle/, a cell 16 pixels wide and 12 deep, everything standing sorted by
# the bottom edge of its footprint and a person hidden by a building or a tree drawn over it as an
# outline in its faction's light accent. It draws SEED's region from the core, lived YEARS years
# with days of TICKS_PER_DAY ticks as evidence/pr-176 does, round the village VILLAGE names and
# the seat below it. Every position is the core's; the mockup only gives orders, through the
# order port every driver uses: Demonic people walk WALKS through the village and the seat, then
# Orthodox armies, the player's among them, gather at MUSTER and march on the village, and the
# Demonic people meet them.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://angle.gd -- <dir> frames
#     FRAMES frames, TICKS_PER_FRAME ticks apart, 640x360 at 1x without the HUD:
#     <dir>/frames/frame_<n>.png, and 4x crops of a person behind a house and in front of one,
#     depth_behind.png and depth_front.png.
#   godot --path . --script res://angle.gd -- <dir> stills
#     The sequence at STILLS' frames under the HUD of play.tscn, 1920x1080, in ko and en:
#     <dir>/<still>_<locale>.png.
#   godot --path . --script res://angle.gd -- <dir> bench [m1] [flat] [solid]
#     Frame time, vsync off, in a 1920x1080 window: the sequence in real time under the HUD, or
#     with m1 the core's M1 scenario of 500 people without it. flat turns the y-sort off and solid
#     the outlines, to measure what each costs. Prints p50, p99 and max over TIMED_FRAMES.
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
# The cells the camera centres on: village 5 at the forest's edge, the seat of the Heavenly Demon and
# its gate, and the southern houses of village 2.
const FOCUS := Vector2(26.0, 46.0)
const FRAMES := 180
const TICKS_PER_FRAME := 3
const FRAME_MS := 100
const STILLS := {"village": 40, "skirmish": 130}
const LOCALES := ["ko", "en"]
const KEPT := "user://angle_mockup.cfg"
const SETTLE_FRAMES := 8
# Rows of a body a house must cover, or the body cover of the house, for a depth crop.
const DEPTH_ROWS := 12
const WARM_UP_FRAMES := 120
const TIMED_FRAMES := 1200
# Waypoints each Demonic army walks in turn, by army, in cells: along the row behind village 5's
# southern houses and back, out of the seat's gate to village 5 and back, and down from village 2.
const WALKS := {
	4: [Vector2i(40, 50), Vector2i(22, 50)],
	8: [Vector2i(13, 42), Vector2i(22, 47), Vector2i(13, 35)],
	1: [Vector2i(6, 30), Vector2i(20, 36)],
}
const RAIDERS := [0, 3, 6, 7, 10]
const MUSTER := Vector2i(22, 64)
const TARGET := Vector2i(27, 49)
# Raiders within this many cells of TARGET bring the village's people out to meet them.
const ALARM := 12.0
# Ticks the raiders are given to gather before they march, whether all have or not.
const MUSTER_TICKS := 6000

var _dir := ""
var _mode := ""
var _flags := PackedStringArray()
var _world: WorldAdapter
var _view: AngleView
var _camera: Camera2D
var _play: Node2D
var _hud: CanvasLayer
var _frame := 0
var _step := 0
var _shots: Array[Dictionary] = []
var _shot_at := 0
var _sequence_frame := 0
var _depth := {}
var _times := PackedFloat64Array()
var _last_usec := 0
var _since_tick := 0.0
var _bench_frames := 0
var _driver: Driver


## The world in the 2.5D projection: ground tiles composed into one image, structures, trees and
## bodies as sprites under one y-sorted node, the hidden parts of bodies outlined over what hides
## them, each outline clipped to its occluder's pixels.
class AngleView extends Node2D:
	const ART := "res://art/mockup/angle/"
	const CELL := Vector2(16, 12)
	const HILL_LIFT := 8
	# grass 6 and grass 2 of docs/art-direction.md §Palette: the hills' lit lip and dark edge.
	const LIP := Color("8fae5a")
	const EDGE := Color("2c4a32")
	const SUB := 256.0
	const BUILDINGS := ["house", "hall", "storehouse", "manual_hall", "granary", "well", "seat_gate",
			"barrels", "fence", "training_ground"]
	const GROUND_KINDS := ["training_ground"]
	const FIELD_TAGS := ["sown", "growing", "ripe", "fallow", "burnt"]
	const FACTIONS := ["orthodox", "demonic"]
	const BODY := Vector2(16, 24)
	const FEET := Vector2(8, 22)
	const WALK_MS := 150
	const STRIKE_MS := 200
	const CHEVRON_MS := 400
	# Occluders are bucketed by this many pixels, so a body tests only those near it.
	const BUCKET := 64

	var world: WorldAdapter
	var steered := -1
	var xray := true
	## Bodies drawn in the last refresh, and how many of them something hid.
	var drawn := 0
	var occluded := 0
	## Per hidden body in the last refresh: [army, person, occluder's kind, body's feet, rows of
	## the body the occluder's opaque bounds cover].
	var hidden_list: Array = []
	## Per body standing in front of a structure it overlaps: [army, person, kind, feet, rows].
	var front_list: Array = []

	var _sheets := {}
	var _kinds := PackedStringArray()
	var _width := 0
	var _height := 0
	var _standing := Node2D.new()
	var _shadows := Node2D.new()
	var _ground := Node2D.new()
	var _occluders: Array[Sprite2D] = []
	var _buckets := {}
	var _bodies := {}
	var _body_shadows := {}
	var _last := {}
	var _now := {}
	var _ready_at := {}
	var _strike_until := {}
	var _ring := Sprite2D.new()
	var _chevron := Sprite2D.new()
	var _chevron_frames: Array[AtlasTexture] = []
	var _xray_pool := {}

	func _init() -> void:
		add_child(_ground)
		add_child(_shadows)
		_standing.y_sort_enabled = true
		add_child(_standing)
		add_child(_chevron)

	## Lays out `shown`'s ground, structures and trees, and reads its bodies.
	func show_world(shown: WorldAdapter, steered_army: int, sorted := true) -> void:
		world = shown
		steered = steered_army
		_standing.y_sort_enabled = sorted
		for name: String in ["terrain", "faces", "trees", "orthodox", "demonic", "marks"] + BUILDINGS:
			_sheets[name] = _load(name)
		var rows := world.map_rows()
		_height = rows.size()
		_width = rows[0].length()
		var names := world.terrain_names()
		var terrain := world.terrain()
		_kinds.clear()
		for i in _width * _height:
			_kinds.append(names[terrain[i]] if terrain.size() == _width * _height else
					("plain" if rows[i / _width][i % _width] == "." else "mountain"))
		_lay_ground()
		_lay_structures()
		_lay_trees()
		_ring.texture = _frame("marks", "ring", 0)
		_ring.centered = false
		_ring.offset = Vector2(-8, -4)
		_shadows.add_child(_ring)
		var marker := _load_from("res://art/markers/player_marker")
		for n in 2:
			_chevron_frames.append(_frame_of(marker, "chevron", n))
		_chevron.centered = false
		_chevron.offset = Vector2(-8, -38)
		note_tick()
		note_tick()

	## Where a point in sub-cell units is drawn: x by CELL.x, y by CELL.y, raised on hills.
	func to_screen(sub: Vector2) -> Vector2:
		var cell := Vector2i((sub / SUB).floor())
		var at := Vector2(sub.x / SUB * CELL.x, sub.y / SUB * CELL.y)
		if _kind(cell.x, cell.y) == "hills":
			at.y -= HILL_LIFT
		return at

	## Reads the bodies as the last tick left them.
	func note_tick() -> void:
		_last = _now
		_now = {}
		for army in world.army_count():
			var roster := world.roster(army)
			var positions := world.body_positions(army)
			var ready := world.body_ready(army)
			for n in roster.size():
				var person := roster[n]
				_now[person] = [army, positions[n]]
				if _ready_at.has(person) and ready[n] > _ready_at[person]:
					_strike_until[person] = -1
				_ready_at[person] = ready[n]

	## Draws the bodies `between` of the way from the tick before last to the last, at `now_ms`.
	func refresh(between: float, now_ms: int) -> void:
		drawn = 0
		occluded = 0
		hidden_list.clear()
		front_list.clear()
		for occluder in _occluders:
			occluder.clip_children = CanvasItem.CLIP_CHILDREN_DISABLED
		for pool: Array in _xray_pool.values():
			for sprite: Sprite2D in pool:
				sprite.visible = false
		var used := {}
		for person: int in _bodies.keys():
			if not _now.has(person):
				_bodies[person].queue_free()
				_body_shadows[person].queue_free()
				_bodies.erase(person)
				_body_shadows.erase(person)
		for person: int in _now:
			var army: int = _now[person][0]
			var now: Vector2 = _now[person][1]
			var before: Vector2 = _last[person][1] if _last.has(person) else now
			var at := to_screen(before.lerp(now, clampf(between, 0.0, 1.0))).round()
			var sprite: Sprite2D = _bodies.get(person)
			if sprite == null:
				sprite = Sprite2D.new()
				sprite.centered = false
				sprite.offset = -FEET
				_standing.add_child(sprite)
				_bodies[person] = sprite
				var shadow := Sprite2D.new()
				shadow.texture = _frame("marks", "shadow", 0)
				shadow.centered = false
				shadow.offset = Vector2(-8, -4)
				_shadows.add_child(shadow)
				_body_shadows[person] = shadow
			var faction: String = FACTIONS[world.army_faction(army) % FACTIONS.size()]
			var heading := now - before
			var tag := "idle"
			var index := 0
			if _strike_until.get(person, 0) == -1:
				_strike_until[person] = now_ms + STRIKE_MS
			if _strike_until.get(person, 0) > now_ms:
				tag = "strike"
			elif heading != Vector2.ZERO:
				tag = "back" if heading.y < 0 and absf(heading.y) > absf(heading.x) else "walk"
				index = (now_ms / WALK_MS) % 4
			if heading.x != 0:
				sprite.flip_h = heading.x < 0
			sprite.texture = _frame(faction, tag, index)
			sprite.position = at
			_body_shadows[person].position = at
			drawn += 1
			if xray:
				_outline(sprite, faction + "|" + tag + "|" + str(index), army, person, used)
			_note_front(sprite, army, person)
		var lead := world.roster(steered) if steered >= 0 else PackedInt64Array()
		var alive := not lead.is_empty() and _bodies.has(lead[0])
		_ring.visible = alive
		_chevron.visible = alive
		if alive:
			_ring.position = _bodies[lead[0]].position
			_chevron.position = _bodies[lead[0]].position
			_chevron.texture = _chevron_frames[(now_ms / CHEVRON_MS) % 2]

	# Draws the outline of `body` over every occluder sorted after it that its frame overlaps,
	# clipped to the occluder's pixels.
	func _outline(body: Sprite2D, key: String, army: int, person: int, used: Dictionary) -> void:
		var rect := Rect2(body.position - FEET, BODY).grow(-2)
		var parts := key.split("|")
		for occluder: Sprite2D in _near(rect):
			if occluder.position.y <= body.position.y:
				continue
			if not occluder.get_meta("solid").intersects(rect):
				continue
			var pool: Array = _xray_pool.get(occluder, [])
			_xray_pool[occluder] = pool
			var n: int = used.get(occluder, 0)
			used[occluder] = n + 1
			if n >= pool.size():
				var made := Sprite2D.new()
				made.centered = false
				made.offset = -FEET
				occluder.add_child(made)
				pool.append(made)
			var outline: Sprite2D = pool[n]
			outline.texture = _frame(parts[0], parts[1] + "_xray", int(parts[2]))
			outline.flip_h = body.flip_h
			outline.position = body.position - occluder.position
			outline.visible = true
			occluder.clip_children = CanvasItem.CLIP_CHILDREN_AND_DRAW
			occluded += 1
			hidden_list.append([army, person, occluder.get_meta("kind"), body.position,
					occluder.get_meta("solid").intersection(rect).size.y])

	func _note_front(body: Sprite2D, army: int, person: int) -> void:
		var rect := Rect2(body.position - FEET, BODY).grow(-2)
		for occluder: Sprite2D in _near(rect):
			if occluder.position.y < body.position.y and occluder.get_meta("kind") != "tree" \
					and occluder.get_meta("solid").intersects(rect):
				front_list.append([army, person, occluder.get_meta("kind"), body.position,
						occluder.get_meta("solid").intersection(rect).size.y])

	func _near(rect: Rect2) -> Array:
		var found := {}
		for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
			for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
				for occluder: Sprite2D in _buckets.get(Vector2i(bx, by), []):
					found[occluder] = true
		return found.keys()

	# Adds `sprite` to the sorted node as something that can hide a body, with the bounds of its
	# opaque pixels in the world.
	func _add_occluder(sprite: Sprite2D, kind: String, sheet: Dictionary) -> void:
		var region := Rect2i((sprite.texture as AtlasTexture).region)
		var used := (sheet.image as Image).get_region(region).get_used_rect()
		sprite.set_meta("solid", Rect2(sprite.position + sprite.offset + Vector2(used.position), used.size))
		sprite.set_meta("kind", kind)
		_standing.add_child(sprite)
		_occluders.append(sprite)
		var rect := Rect2(sprite.position + sprite.offset, sprite.texture.get_size())
		for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
			for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
				var key := Vector2i(bx, by)
				if not _buckets.has(key):
					_buckets[key] = []
				_buckets[key].append(sprite)

	func _kind(x: int, y: int) -> String:
		return _kinds[clampi(y, 0, _height - 1) * _width + clampi(x, 0, _width - 1)]

	func _hash(x: int, y: int, salt: int) -> int:
		var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
		h = (h ^ (h >> 13)) * 1274126177
		return absi(h ^ (h >> 16))

	# The ground of every cell composed into one image: terrain, the villages' road and paving,
	# the fields in the state they are in now, the hills raised with their front step, and the
	# dithered shadows of what stands on it.
	func _lay_ground() -> void:
		var image := Image.create_empty(_width * int(CELL.x), _height * int(CELL.y), false, Image.FORMAT_RGBA8)
		var tiles := {}
		for x in _width:
			for y in _height:
				var kind := _kind(x, y)
				tiles[Vector2i(x, y)] = kind if kind in ["hills", "forest"] else "plain"
		var layouts: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/villages.json"))
		for village: Dictionary in world.villages():
			for item: Dictionary in layouts.get(village.kind, []):
				if not item.has("tile"):
					continue
				var r: Array = item.rect
				for dx in int(r[2]):
					for dy in int(r[3]):
						tiles[Vector2i(village.corner) + Vector2i(int(r[0]) + dx, int(r[1]) + dy)] = item.tile
		for structure in world.structure_count():
			var state := world.structure_field_state(structure)
			if state < 0:
				continue
			var corner := world.structure_cell(structure)
			var size := world.structure_footprint(structure)
			for dx in size.x:
				for dy in size.y:
					tiles[corner + Vector2i(dx, dy)] = FIELD_TAGS[state]
		var sheet: Dictionary = _sheets.terrain
		var faces: Dictionary = _sheets.faces
		for y in _height:
			for x in _width:
				var tag: String = tiles[Vector2i(x, y)]
				var span: Array = sheet.tags[tag]
				var frame: Rect2i = sheet.frames[span[0] + _hash(x, y, 1) % (span[1] - span[0] + 1)]
				var lift := HILL_LIFT if _kind(x, y) == "hills" else 0
				var at := Vector2i(x * int(CELL.x), y * int(CELL.y) - lift)
				image.blit_rect(sheet.image, frame, at)
				if lift == 0:
					continue
				if _kind(x, y + 1) != "hills":
					var face: Rect2i = faces.frames[faces.tags.hills_face[0] + _hash(x, y, 2) % 2]
					image.blit_rect(faces.image, face, Vector2i(at.x, (y + 1) * int(CELL.y) - HILL_LIFT))
				# The raised edge catches the light from the upper left and falls dark to the right.
				if _kind(x, y - 1) != "hills":
					image.fill_rect(Rect2i(at, Vector2i(int(CELL.x), 1)), LIP)
				if _kind(x - 1, y) != "hills":
					image.fill_rect(Rect2i(at, Vector2i(1, int(CELL.y))), LIP)
				if _kind(x + 1, y) != "hills":
					image.fill_rect(Rect2i(at + Vector2i(int(CELL.x) - 1, 0), Vector2i(1, int(CELL.y) + HILL_LIFT)), EDGE)
		for structure in world.structure_count():
			if world.structure_field_state(structure) >= 0:
				continue
			var corner := world.structure_cell(structure)
			var size := world.structure_footprint(structure)
			var left := corner.x * int(CELL.x)
			var top := corner.y * int(CELL.y)
			var right := (corner.x + size.x) * int(CELL.x)
			var bottom := (corner.y + size.y) * int(CELL.y)
			_dither(image, Rect2i(right, top + 6, 6, bottom - top - 2))
			_dither(image, Rect2i(left + 6, bottom, right - left, 3))
		var sprite := Sprite2D.new()
		sprite.centered = false
		sprite.texture = ImageTexture.create_from_image(image)
		_ground.add_child(sprite)
		_ground_image = image

	var _ground_image: Image

	func _dither(image: Image, rect: Rect2i) -> void:
		var ink := Color("14101a")
		for x in range(rect.position.x, rect.end.x):
			for y in range(rect.position.y, rect.end.y):
				if (x + y) % 2 == 0 and x >= 0 and y >= 0 and x < image.get_width() and y < image.get_height():
					image.set_pixel(x, y, ink)

	func _lay_structures() -> void:
		for structure in world.structure_count():
			var kind := world.structure_kind(structure)
			if world.structure_field_state(structure) >= 0:
				continue
			if not _sheets.has(kind):
				print("angle: no art for %s at %s" % [kind, world.structure_cell(structure)])
				continue
			var corner := world.structure_cell(structure)
			var size := world.structure_footprint(structure)
			var sprite := Sprite2D.new()
			sprite.texture = _frame(kind, "intact", 0)
			sprite.centered = false
			var frame_size := sprite.texture.get_size()
			sprite.offset = Vector2(-8, -(frame_size.y - 8))
			sprite.position = Vector2(corner.x * CELL.x, (corner.y + size.y) * CELL.y)
			if kind in GROUND_KINDS:
				sprite.position += sprite.offset
				sprite.offset = Vector2.ZERO
				_ground.add_child(sprite)
			else:
				_add_occluder(sprite, kind, _sheets[kind])

	# A tree on about half the forest cells outside the villages, its foot jittered within the
	# cell, broadleaf or pine.
	func _lay_trees() -> void:
		var cleared: Array[Rect2i] = []
		for village: Dictionary in world.villages():
			cleared.append(Rect2i(village.corner, village.area).grow(1))
		for y in _height:
			for x in _width:
				if _kind(x, y) != "forest" or _hash(x, y, 3) % 100 >= 55:
					continue
				if cleared.any(func(rect: Rect2i) -> bool: return rect.has_point(Vector2i(x, y))):
					continue
				var sprite := Sprite2D.new()
				var pine := _hash(x, y, 4) % 3 == 0
				sprite.texture = _frame("trees", "pine" if pine else "tree", _hash(x, y, 5) % (2 if pine else 3))
				sprite.centered = false
				sprite.offset = Vector2(-16, -36)
				sprite.position = Vector2(x * CELL.x + 4 + _hash(x, y, 6) % 9, y * CELL.y + 3 + _hash(x, y, 7) % 7)
				_dither(_ground_image, Rect2i(int(sprite.position.x) - 2, int(sprite.position.y) - 2, 12, 4))
				_add_occluder(sprite, "tree", _sheets.trees)
		(_ground.get_child(0) as Sprite2D).texture = ImageTexture.create_from_image(_ground_image)

	func _load(name: String) -> Dictionary:
		return _load_from(ART + name)

	func _load_from(path: String) -> Dictionary:
		var image := Image.load_from_file(ProjectSettings.globalize_path(path + ".png"))
		var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path + ".json"))
		var frames: Array[Rect2i] = []
		for f: Dictionary in data.frames:
			var r: Dictionary = f.frame
			frames.append(Rect2i(int(r.x), int(r.y), int(r.w), int(r.h)))
		var tags := {}
		for t: Dictionary in data.meta.frameTags:
			tags[t.name] = [int(t.from), int(t.to)]
		return {"image": image, "texture": ImageTexture.create_from_image(image), "frames": frames,
				"tags": tags, "atlas": {}}

	func _frame(sheet: String, tag: String, index: int) -> AtlasTexture:
		return _frame_of(_sheets[sheet], tag, index)

	func _frame_of(sheet: Dictionary, tag: String, index: int) -> AtlasTexture:
		var key := "%s/%d" % [tag, index]
		var atlas: Dictionary = sheet.atlas
		if not atlas.has(key):
			var texture := AtlasTexture.new()
			texture.atlas = sheet.texture
			texture.region = Rect2(sheet.frames[sheet.tags[tag][0] + index])
			atlas[key] = texture
		return atlas[key]


## Gives the staged orders: each walking Demonic army its next waypoint when it reaches one, the
## raiders MUSTER then TARGET, and every Demonic army the nearest raider once raiders come within
## ALARM of TARGET. It decides from the world's state alone, so a run replays exactly.
class Driver extends RefCounted:
	var world: WorldAdapter
	var marching := false
	var alarmed := false
	var _waypoint := {}
	var _since_muster := 0

	func _init(driven: WorldAdapter) -> void:
		world = driven
		for army: int in WALKS:
			world.steer(army, true)
			_waypoint[army] = 0
			_order(army, WALKS[army][0])
		for army: int in RAIDERS:
			world.steer(army, true)
			_order(army, MUSTER)

	func step() -> void:
		if not marching:
			_since_muster += 1
			var gathered := true
			for army: int in RAIDERS:
				if _lead(army).distance_to(Vector2(MUSTER)) > 4.0:
					gathered = false
			if gathered or _since_muster >= MUSTER_TICKS:
				marching = true
				for army: int in RAIDERS:
					_order(army, TARGET)
		elif not alarmed:
			for army: int in RAIDERS:
				if _lead(army).distance_to(Vector2(TARGET)) <= ALARM:
					alarmed = true
		for army: int in WALKS:
			if world.roster(army).is_empty():
				continue
			if alarmed:
				_order(army, _nearest_raider(_lead(army)))
				continue
			var route: Array = WALKS[army]
			if _lead(army).distance_to(Vector2(route[_waypoint[army]]) + Vector2(0.5, 0.5)) <= 1.0:
				_waypoint[army] = (_waypoint[army] + 1) % route.size()
				_order(army, route[_waypoint[army]])

	func _lead(army: int) -> Vector2:
		var positions := world.body_positions(army)
		if positions.is_empty():
			return Vector2.INF
		return positions[0] / float(WorldAdapter.CELL_SIZE)

	func _nearest_raider(from: Vector2) -> Vector2i:
		var best := TARGET
		var distance := INF
		for army: int in RAIDERS:
			var lead := _lead(army)
			if lead.distance_to(from) < distance:
				distance = lead.distance_to(from)
				best = Vector2i(lead.floor())
		return best

	func _order(army: int, cell: Vector2i) -> void:
		var skills := PackedInt64Array()
		for skill in world.skill_count(army):
			skills.append(skill)
		world.order(army, cell, skills)


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2 or not args[1] in ["frames", "stills", "bench"]:
		printerr("angle: usage: -- <dir> frames|stills|bench [m1] [flat] [solid]")
		quit(2)
		return
	_dir = args[0]
	_mode = args[1]
	_flags = args.slice(2)
	DirAccess.make_dir_recursive_absolute(_dir.path_join("frames"))
	if _mode == "frames":
		DisplayServer.window_set_size(Vector2i(640, 360))
		_start_world()
		_add_view(root, _world.player_army())
	elif _mode == "stills":
		for still: String in STILLS:
			for locale: String in LOCALES:
				_shots.append({"still": still, "locale": locale})
	elif _mode == "bench":
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		DisplayServer.window_set_size(Vector2i(1920, 1080))
		if "m1" in _flags:
			_world = WorldAdapter.create_m1()
			_add_view(root, -1)
			_camera.position = _view.to_screen(Vector2(WorldAdapter.m1_centre())).round()
			_camera.zoom = Vector2(3, 3)
		else:
			# The HUD is set up as for a still, which then hands over to the bench.
			_mode = "stills"
			_shots.append({"still": "bench", "locale": "ko"})


# The region lived YEARS years, the orders given, and run up to the raiders' march.
func _start_world() -> void:
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		_world.tick()
	_driver = Driver.new(_world)
	var ticks := 0
	while not _driver.marching:
		_driver.step()
		_world.tick()
		ticks += 1
	print("angle: %s, raiders gathered after %d ticks; relation Orthodox to Demonic %d" % [
		_world.date(), ticks, _world.relation(0, 1)])
	for army in _world.army_count():
		print("angle: army %d of faction %d, %d people" % [army, _world.army_faction(army), _world.roster(army).size()])


func _add_view(parent: Node, steered: int) -> void:
	_view = AngleView.new()
	_view.xray = not "solid" in _flags
	parent.add_child(_view)
	_view.show_world(_world, steered, not "flat" in _flags)
	if parent == root:
		_camera = Camera2D.new()
		root.add_child(_camera)
		_camera.position = (FOCUS * AngleView.CELL).round()


func _tick() -> void:
	_driver.step()
	_world.tick()
	_view.note_tick()


func _process(delta: float) -> bool:
	_frame += 1
	match _mode:
		"frames":
			return _frames()
		"stills":
			return _stills()
		"bench":
			return _bench(delta)
	return false


# One frame of the sequence each process frame, drawn before it is read, so a capture holds it
# even while the window is hidden and the loop would not draw.
func _frames() -> bool:
	if _sequence_frame > 0:
		for i in TICKS_PER_FRAME:
			_tick()
	_view.refresh(1.0, _sequence_frame * FRAME_MS)
	RenderingServer.force_draw(false)
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join("frames/frame_%03d.png" % _sequence_frame))
	_note_depth(image)
	print("angle: frame %d tick %d, %d bodies drawn, %d hidden" % [_sequence_frame, _world.tick_count(),
			_view.drawn, _view.occluded])
	_sequence_frame += 1
	if _sequence_frame >= FRAMES:
		print("angle: depth crops %s" % [_depth.keys()])
		quit(0)
		return true
	return false


# Saves, the first time it happens, a 4x crop round a person hidden by a house and round a
# person standing in front of one.
func _note_depth(image: Image) -> void:
	for pair: Array in [["depth_behind", _view.hidden_list], ["depth_front", _view.front_list]]:
		if _depth.has(pair[0]):
			continue
		for seen: Array in pair[1]:
			if seen[2] != "house" or seen[4] < DEPTH_ROWS:
				continue
			var feet: Vector2 = seen[3]
			var on_screen := feet - (_camera.position - Vector2(320, 180))
			var crop := Rect2i(Vector2i(on_screen) - Vector2i(48, 56), Vector2i(96, 72))
			if not Rect2i(Vector2i.ZERO, image.get_size()).encloses(crop):
				continue
			var part := image.get_region(crop)
			part.resize(part.get_width() * 4, part.get_height() * 4, Image.INTERPOLATE_NEAREST)
			part.save_png(_dir.path_join(pair[0] + ".png"))
			_depth[pair[0]] = _sequence_frame
			print("angle: %s.png from frame %d, army %d person %d at %s" % [pair[0], _sequence_frame,
					seen[0], seen[1], feet])
			break


# A still under the real HUD: the sequence replayed to the still's frame, play.tscn's HUD shown
# for the player, the angle view in place of play's world art, at 1920x1080.
func _stills() -> bool:
	if _frame % SETTLE_FRAMES != 0 and _step != 3:
		return false
	var shot: Dictionary = _shots[_shot_at]
	match _step:
		0:
			if _play != null:
				_play.queue_free()
			_play = load("res://play/play.tscn").instantiate()
			root.add_child(_play)
		1:
			_hud = _play.get_node("Hud")
			_hud.settings.path = KEPT
			_hud.settings.choose("language", LOCALES.find(shot.locale))
			_hud.settings.choose("touch", _hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(Vector2i(1920, 1080))
		2:
			_start_world()
			_play._play(_world)
			_play.set_process(false)
			_play.get_node("WorldArt").visible = false
			_play.get_node("Sound").process_mode = Node.PROCESS_MODE_DISABLED
			_view = AngleView.new()
			_view.xray = not "solid" in _flags
			_play.add_child(_view)
			_play.move_child(_view, 1)
			_view.show_world(_world, _world.player_army(), not "flat" in _flags)
			_camera = _play.get_node("Camera")
			_camera.limit_right = 100000
			_camera.limit_bottom = 100000
			_camera.position = (FOCUS * AngleView.CELL).round()
			var target: int = STILLS.get(shot.still, 0) * TICKS_PER_FRAME
			for i in target:
				_tick()
			if shot.still == "bench":
				_since_tick = 0.0
				_mode = "bench"
				return false
		3:
			_play.fit_view()
			_view.refresh(1.0, STILLS.get(shot.still, 0) * FRAME_MS)
			_hud.refresh(_world, _play.steered, _play._skills)
			if _frame % SETTLE_FRAMES != 0:
				return false
		4:
			var name := "%s_%s" % [shot.still, shot.locale]
			RenderingServer.force_draw(false)
			var image := root.get_texture().get_image()
			image.save_png(_dir.path_join(name + ".png"))
			print("angle: %s.png %dx%d at frame %d, tick %d, %d bodies drawn, %d hidden" % [name,
					image.get_width(), image.get_height(), STILLS[shot.still], _world.tick_count(), _view.drawn,
					_view.occluded])
			_shot_at += 1
			_step = 0
			if _shot_at >= _shots.size():
				quit(0)
				return true
			return false
	_step += 1
	return false


# Real time: ticks at the core's rate, bodies drawn between ticks on every frame, and the frame
# times of TIMED_FRAMES after WARM_UP_FRAMES.
func _bench(delta: float) -> bool:
	var now := Time.get_ticks_usec()
	var step := 1.0 / WorldAdapter.TICKS_PER_SECOND
	_since_tick = minf(_since_tick + delta, step * 4)
	while _since_tick >= step:
		if _driver != null:
			_tick()
		else:
			_world.tick()
			_view.note_tick()
		_since_tick -= step
	_view.refresh(_since_tick / step, Time.get_ticks_msec())
	if _hud != null:
		_play.fit_view()
		_hud.refresh(_world, _play.steered, _play._skills)
	_bench_frames += 1
	if _bench_frames > WARM_UP_FRAMES:
		_times.append((now - _last_usec) / 1000.0)
	_last_usec = now
	if _times.size() < TIMED_FRAMES:
		return false
	var sorted := _times.duplicate()
	sorted.sort()
	print("angle bench: %s, flags %s, window %s, vsync off, %s, Godot %s" % [
		"M1 scenario" if "m1" in _flags else "village sequence under the HUD", _flags,
		DisplayServer.window_get_size(), RenderingServer.get_video_adapter_name(), Engine.get_version_info().string])
	print("angle bench: %d bodies drawn, %d hidden behind something in the last frame" % [_view.drawn, _view.occluded])
	print("angle bench: frame time p50 %.3f ms, p99 %.3f ms, max %.3f ms over %d frames" % [
		sorted[sorted.size() / 2], sorted[ceili(sorted.size() * 0.99) - 1], sorted[-1], sorted.size()])
	quit(0)
	return true
