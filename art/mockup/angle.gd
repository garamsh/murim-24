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
#   godot --path . --script res://angle.gd -- <dir> bench [m1] [pan] [flat] [solid]
#     Frame time, vsync off, in a 1920x1080 window: the sequence in real time under the HUD, or
#     with m1 the core's M1 scenario of 500 people without it. pan moves the camera round PAN, so
#     chunks stream in and out; flat turns the y-sort off and solid the outlines, to measure what
#     each costs. Prints p50, p99 and max over TIMED_FRAMES, and what streaming held and built.
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
# The levelling skill: village 5 of the region, LEVEL_PEOPLE people round it in LEVEL_ARMIES
# armies, each of LEVEL_HEALTH, and a caster south of it. Each structure cell holds
# LEVEL_DURABILITY, its stages' floors LEVEL_FLOORS; the skill takes LEVEL_SKILL's damage from
# every person and structure within its radius, LEVEL_RADIUS cells, so three casts, at
# LEVEL_CASTS seconds, take the village from intact to rubble and its people from whole to dead.
const LEVEL_VILLAGE := 4
const LEVEL_PEOPLE := 60
const LEVEL_ARMIES := 6
const LEVEL_HEALTH := 100
const LEVEL_RADIUS := 11
const LEVEL_DURABILITY := 100
const LEVEL_FLOORS := [80, 50, 20]
const LEVEL_SKILL := {"range": 24 * 256, "radius": LEVEL_RADIUS * 256, "body_damage": 40, "structure_damage": 30,
		"cooldown": 15}
const IDLE_SKILL := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 0}
const LEVEL_CASTER := Vector2i(30, 64)
const LEVEL_CASTS := [2.3, 2.9, 3.5]
const LEVEL_BLADES := 40
const BLADE_FALL := 0.3
const BLADE_DROP := 140.0
const SHAKE := 4.0
const SHAKE_SECONDS := 0.35
# Where the camera centres, in cells, at 3x and pulled back to 2x.
const LEVEL_FOCUS := Vector2(31.5, 50.0)
const LEVEL_WIDE := Vector2(31.5, 55.0)
const LEVEL_FPS := 15
const LEVEL_SECONDS := 8.0
# The bench's pan: the camera round PAN's corners, in cells, at PAN_SPEED cells a second.
const PAN := [Vector2(26, 46), Vector2(100, 46), Vector2(100, 100), Vector2(26, 100)]
const PAN_SPEED := 20.0
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
var _pan_seconds := 0.0
var _driver: Driver
var _level: Leveller
var _level_ticks := 0
var _level_window := PackedFloat64Array()


## The world in the 2.5D projection, streamed by chunks of CHUNK x CHUNK cells: a chunk's ground
## is composed into one image and its structures and trees made as sprites when it comes within
## STREAM_MARGIN of the screen, and all of it freed once it is a chunk further away. Everything
## standing is a sprite under one y-sorted node; bodies get sprites only on screen, and the hidden
## parts of those are outlined over what hides them, each outline clipped to its occluder's pixels.
class AngleView extends Node2D:
	const ART := "res://art/mockup/angle/"
	const CELL := Vector2(16, 12)
	const HILL_LIFT := 8
	# grass 6 and grass 2 of docs/art-direction.md §Palette: the hills' lit lip and dark edge.
	const LIP := Color("8fae5a")
	const EDGE := Color("2c4a32")
	const INK := Color("14101a")
	const SUB := 256.0
	const BUILDINGS := ["house", "hall", "storehouse", "manual_hall", "granary", "well", "seat_gate",
			"barrels", "fence", "training_ground"]
	const GROUND_KINDS := ["training_ground"]
	const FIELD_TAGS := ["sown", "growing", "ripe", "fallow", "burnt"]
	const STAGES := ["intact", "damaged", "destroyed", "rubble"]
	const FACTIONS := ["orthodox", "demonic"]
	const BODY := Vector2(16, 24)
	const FEET := Vector2(8, 22)
	const WALK_MS := 150
	const STRIKE_MS := 200
	const CHEVRON_MS := 400
	# Occluders are bucketed by this many pixels, so a body tests only those near it.
	const BUCKET := 64
	## Cells a chunk is wide and deep: ADR 0042's areas.
	const CHUNK := 32
	## Pixels past the screen within which chunks are streamed in.
	const STREAM_MARGIN := 64.0
	## Chunks built in one frame at most, nearest the screen first.
	const BUILDS_PER_FRAME := 1
	# Pixels past the screen within which bodies get sprites.
	const BODY_MARGIN := 32.0

	var world: WorldAdapter
	## The world whose terrain, villages and structures are drawn; `world` unless show_world is
	## given another, whose map has the same cells.
	var look: WorldAdapter
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
	## Chunks resident now, chunks built and freed so far, the most resident at once, and the
	## bytes their ground images hold now.
	var resident := 0
	var built := 0
	var freed := 0
	var most_resident := 0
	var ground_bytes := 0
	## Chunks built again since a structure on them changed stage.
	var rebuilt := 0
	## How long each chunk took to build, in microseconds.
	var build_usec := PackedInt64Array()

	## Effects drawn on the ground under everything standing, and over all of it.
	var under := Node2D.new()
	var over := Node2D.new()

	var _stage_of := Callable()
	var _sheets := {}
	var _kinds := PackedStringArray()
	var _width := 0
	var _height := 0
	var _standing := Node2D.new()
	var _shadows := Node2D.new()
	var _ground := Node2D.new()
	var _chunks := {}
	var _buckets := {}
	# The cells the villages' decorations and the fields cover, with their tile.
	var _laid := {}
	# Structures by the chunk of their footprint's corner: {kind, corner, size, id}.
	var _structures_in := {}
	var _cleared: Array[Rect2i] = []
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
		_ground.y_sort_enabled = true
		add_child(_ground)
		add_child(_shadows)
		add_child(under)
		_standing.y_sort_enabled = true
		add_child(_standing)
		add_child(over)
		add_child(_chevron)

	## A frame of one of EFFECTS' sheets, loaded the first time it is asked for.
	func effect(sheet: String, tag: String, index: int) -> AtlasTexture:
		if not _sheets.has(sheet):
			_sheets[sheet] = _load(sheet)
		return _frame(sheet, tag, index)

	## Reads `shown`'s bodies, and `drawn_from`'s terrain, villages and structures (`shown`'s when
	## null); a structure is drawn at the stage `stage_of` gives its entry, or the world's.
	func show_world(shown: WorldAdapter, steered_army: int, sorted := true, drawn_from: WorldAdapter = null,
			stage_of := Callable()) -> void:
		world = shown
		look = drawn_from if drawn_from != null else shown
		steered = steered_army
		_stage_of = stage_of
		_standing.y_sort_enabled = sorted
		for name: String in ["terrain", "faces", "trees", "orthodox", "demonic", "marks"] + BUILDINGS:
			_sheets[name] = _load(name)
		var rows := look.map_rows()
		_height = rows.size()
		_width = rows[0].length()
		var names := look.terrain_names()
		var terrain := look.terrain()
		_kinds.clear()
		for i in _width * _height:
			_kinds.append(names[terrain[i]] if terrain.size() == _width * _height else
					("plain" if rows[i / _width][i % _width] == "." else "mountain"))
		_index_layout()
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

	## Streams the chunks round `view`, the screen in world pixels, and draws the bodies on it
	## `between` of the way from the tick before last to the last, at `now_ms`.
	func refresh(view: Rect2, between: float, now_ms: int) -> void:
		_stream(view)
		# A chunk whose structures changed stage is built again, its shadows with them.
		var restaged := {}
		for key: Vector2i in _chunks:
			for sprite: Sprite2D in _chunks[key].occluders:
				if sprite.has_meta("stages") and _stage(sprite.get_meta("entry")) != sprite.get_meta("stage"):
					for next: Vector2i in [key, key + Vector2i(1, 0), key + Vector2i(0, 1)]:
						if _chunks.has(next):
							restaged[next] = true
		for key: Vector2i in restaged:
			_free_chunk(key)
			_build_chunk(key)
			rebuilt += 1
		for key: Vector2i in _chunks:
			for sprite: Sprite2D in _chunks[key].occluders:
				sprite.clip_children = CanvasItem.CLIP_CHILDREN_DISABLED
		for pool: Array in _xray_pool.values():
			for sprite: Sprite2D in pool:
				sprite.visible = false
		drawn = 0
		occluded = 0
		hidden_list.clear()
		front_list.clear()
		var used := {}
		var shown := view.grow(BODY_MARGIN)
		var on_screen := {}
		for person: int in _now:
			var army: int = _now[person][0]
			var now: Vector2 = _now[person][1]
			var before: Vector2 = _last[person][1] if _last.has(person) else now
			var at := to_screen(before.lerp(now, clampf(between, 0.0, 1.0))).round()
			if not shown.has_point(at):
				continue
			on_screen[person] = true
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
		# Bodies off the screen, or no longer in the world, give up their sprites.
		for person: int in _bodies.keys():
			if not on_screen.has(person):
				_bodies[person].queue_free()
				_body_shadows[person].queue_free()
				_bodies.erase(person)
				_body_shadows.erase(person)
		var lead := world.roster(steered) if steered >= 0 else PackedInt64Array()
		var alive := not lead.is_empty() and _bodies.has(lead[0])
		_ring.visible = alive
		_chevron.visible = alive
		if alive:
			_ring.position = _bodies[lead[0]].position
			_chevron.position = _bodies[lead[0]].position
			_chevron.texture = _chevron_frames[(now_ms / CHEVRON_MS) % 2]

	## Pixels the chunk at `key` covers in the world.
	func chunk_rect(key: Vector2i) -> Rect2:
		return Rect2(Vector2(key) * CHUNK * CELL - Vector2(0, HILL_LIFT), CHUNK * CELL + Vector2(0, HILL_LIFT))

	# Builds the nearest missing chunks round `view`, at most BUILDS_PER_FRAME, and frees those a
	# chunk past the streamed ring.
	func _stream(view: Rect2) -> void:
		var wanted := _chunks_over(view.grow(STREAM_MARGIN))
		var kept := _chunks_over(view.grow(STREAM_MARGIN + CHUNK * CELL.y))
		for key: Vector2i in _chunks.keys():
			if not kept.has(key):
				_free_chunk(key)
		var missing: Array = wanted.keys().filter(func(key: Vector2i) -> bool: return not _chunks.has(key))
		var centre := view.get_center()
		missing.sort_custom(func(a: Vector2i, b: Vector2i) -> bool:
				return chunk_rect(a).get_center().distance_squared_to(centre) \
						< chunk_rect(b).get_center().distance_squared_to(centre))
		for key: Vector2i in missing.slice(0, BUILDS_PER_FRAME if not _chunks.is_empty() else missing.size()):
			_build_chunk(key)
		resident = _chunks.size()
		most_resident = maxi(most_resident, resident)

	func _chunks_over(rect: Rect2) -> Dictionary:
		var keys := {}
		var size := CHUNK * CELL
		var last := Vector2i((_width - 1) / CHUNK, (_height - 1) / CHUNK)
		for cx in range(maxi(floori(rect.position.x / size.x), 0), mini(floori(rect.end.x / size.x), last.x) + 1):
			for cy in range(maxi(floori(rect.position.y / size.y), 0), mini(floori(rect.end.y / size.y), last.y) + 1):
				keys[Vector2i(cx, cy)] = true
		return keys

	# The chunk's ground image: terrain, the villages' road and paving, the fields in the state
	# they hold when it is built, the hills raised with their front step, and the dithered shadows
	# of what stands on it; then its structures and trees.
	func _build_chunk(key: Vector2i) -> void:
		var started := Time.get_ticks_usec()
		var origin := key * CHUNK
		var top_left := Vector2i(origin.x * int(CELL.x), origin.y * int(CELL.y) - HILL_LIFT)
		var image := Image.create_empty(CHUNK * int(CELL.x), CHUNK * int(CELL.y) + HILL_LIFT, false,
				Image.FORMAT_RGBA8)
		var sheet: Dictionary = _sheets.terrain
		var faces: Dictionary = _sheets.faces
		for y in range(origin.y, mini(origin.y + CHUNK, _height)):
			for x in range(origin.x, mini(origin.x + CHUNK, _width)):
				var tag := _tile(x, y)
				var span: Array = sheet.tags[tag]
				var frame: Rect2i = sheet.frames[span[0] + _hash(x, y, 1) % (span[1] - span[0] + 1)]
				var lift := HILL_LIFT if _kind(x, y) == "hills" else 0
				var at := Vector2i(x * int(CELL.x), y * int(CELL.y) - lift) - top_left
				image.blit_rect(sheet.image, frame, at)
				if lift == 0:
					continue
				if _kind(x, y + 1) != "hills":
					var face: Rect2i = faces.frames[faces.tags.hills_face[0] + _hash(x, y, 2) % 2]
					image.blit_rect(faces.image, face, Vector2i(at.x, at.y + int(CELL.y)))
				# The raised edge catches the light from the upper left and falls dark to the right.
				if _kind(x, y - 1) != "hills":
					image.fill_rect(Rect2i(at, Vector2i(int(CELL.x), 1)), LIP)
				if _kind(x - 1, y) != "hills":
					image.fill_rect(Rect2i(at, Vector2i(1, int(CELL.y))), LIP)
				if _kind(x + 1, y) != "hills":
					image.fill_rect(Rect2i(at + Vector2i(int(CELL.x) - 1, 0), Vector2i(1, int(CELL.y) + HILL_LIFT)), EDGE)
		# Shadows reach a little past a footprint, so the neighbours' structures cast them too.
		for dx in range(-1, 2):
			for dy in range(-1, 2):
				for entry: Dictionary in _structures_in.get(key + Vector2i(dx, dy), []):
					# The shadow shortens as the structure falls, and rubble casts none.
					var reach: int = [6, 6, 3, 0][_stage(entry)]
					if entry.kind in GROUND_KINDS or reach == 0:
						continue
					var left: int = entry.corner.x * int(CELL.x)
					var top: int = entry.corner.y * int(CELL.y)
					var right: int = (entry.corner.x + entry.size.x) * int(CELL.x)
					var bottom: int = (entry.corner.y + entry.size.y) * int(CELL.y)
					_dither(image, Rect2i(right, top + reach, reach, bottom - top - 2), top_left)
					_dither(image, Rect2i(left + reach, bottom, right - left, reach / 2), top_left)
		var occluders: Array[Sprite2D] = []
		var trees := _trees(Rect2i(origin - Vector2i.ONE, Vector2i(CHUNK + 2, CHUNK + 2)))
		for tree: Array in trees:
			var foot: Vector2 = tree[0]
			_dither(image, Rect2i(int(foot.x) - 2, int(foot.y) - 2, 12, 4), top_left)
			if Rect2i(origin, Vector2i(CHUNK, CHUNK)).has_point(Vector2i(tree[2])):
				var sprite := Sprite2D.new()
				sprite.texture = tree[1]
				sprite.centered = false
				sprite.offset = Vector2(-16, -36)
				sprite.position = foot
				_add_occluder(sprite, "tree", _sheets.trees, occluders)
		var ground := Sprite2D.new()
		ground.centered = false
		ground.texture = ImageTexture.create_from_image(image)
		ground.position = Vector2(top_left)
		_ground.add_child(ground)
		var extras: Array[Sprite2D] = []
		for entry: Dictionary in _structures_in.get(key, []):
			var sprite := Sprite2D.new()
			var stages: Array[AtlasTexture] = []
			for stage: String in STAGES:
				stages.append(_frame(entry.kind, stage if _sheets[entry.kind].tags.has(stage) else "intact", 0))
			sprite.set_meta("stages", stages)
			sprite.set_meta("entry", entry)
			sprite.set_meta("stage", _stage(entry))
			sprite.texture = stages[sprite.get_meta("stage")]
			sprite.centered = false
			var frame_size := sprite.texture.get_size()
			sprite.offset = Vector2(-8, -(frame_size.y - 8))
			sprite.position = Vector2(entry.corner.x * CELL.x, (entry.corner.y + entry.size.y) * CELL.y)
			if entry.kind in GROUND_KINDS:
				sprite.position += sprite.offset
				sprite.offset = Vector2.ZERO
				_ground.add_child(sprite)
				extras.append(sprite)
			else:
				_add_occluder(sprite, entry.kind, _sheets[entry.kind], occluders)
		_chunks[key] = {"ground": ground, "extras": extras, "occluders": occluders,
				"bytes": image.get_data_size()}
		ground_bytes += image.get_data_size()
		built += 1
		build_usec.append(Time.get_ticks_usec() - started)

	func _free_chunk(key: Vector2i) -> void:
		var chunk: Dictionary = _chunks[key]
		for sprite: Sprite2D in chunk.occluders:
			for bucket: Vector2i in sprite.get_meta("buckets"):
				_buckets[bucket].erase(sprite)
				if _buckets[bucket].is_empty():
					_buckets.erase(bucket)
			_xray_pool.erase(sprite)
			sprite.queue_free()
		for sprite: Sprite2D in chunk.extras:
			sprite.queue_free()
		chunk.ground.queue_free()
		ground_bytes -= chunk.bytes
		_chunks.erase(key)
		freed += 1

	func _stage(entry: Dictionary) -> int:
		if _stage_of.is_valid():
			return _stage_of.call(entry)
		return clampi(look.structure_stage(entry.id), 0, STAGES.size() - 1)

	# Indexes once what the chunks draw from: the cells decorations and fields cover, the
	# structures by chunk, and the villages trees keep clear of.
	func _index_layout() -> void:
		var layouts: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/villages.json"))
		for village: Dictionary in look.villages():
			_cleared.append(Rect2i(village.corner, village.area).grow(1))
			for item: Dictionary in layouts.get(village.kind, []):
				if not item.has("tile"):
					continue
				var r: Array = item.rect
				for dx in int(r[2]):
					for dy in int(r[3]):
						_laid[Vector2i(village.corner) + Vector2i(int(r[0]) + dx, int(r[1]) + dy)] = item.tile
		for structure in look.structure_count():
			var corner := look.structure_cell(structure)
			var size := look.structure_footprint(structure)
			var state := look.structure_field_state(structure)
			if state >= 0:
				for dx in size.x:
					for dy in size.y:
						_laid[corner + Vector2i(dx, dy)] = FIELD_TAGS[state]
				continue
			var kind := look.structure_kind(structure)
			if not _sheets.has(kind):
				print("angle: no art for %s at %s" % [kind, corner])
				continue
			var key := corner / CHUNK
			if not _structures_in.has(key):
				_structures_in[key] = []
			_structures_in[key].append({"kind": kind, "corner": corner, "size": size, "id": structure})

	func _tile(x: int, y: int) -> String:
		var laid: String = _laid.get(Vector2i(x, y), "")
		if not laid.is_empty():
			return laid
		var kind := _kind(x, y)
		return kind if kind in ["hills", "forest"] else "plain"

	# The trees on the forest cells of `cells`: on about half of them outside the villages, the
	# foot jittered within the cell, broadleaf or pine. Each is [foot, frame, cell].
	func _trees(cells: Rect2i) -> Array:
		var trees := []
		for y in range(maxi(cells.position.y, 0), mini(cells.end.y, _height)):
			for x in range(maxi(cells.position.x, 0), mini(cells.end.x, _width)):
				if _kind(x, y) != "forest" or _hash(x, y, 3) % 100 >= 55:
					continue
				if _cleared.any(func(rect: Rect2i) -> bool: return rect.has_point(Vector2i(x, y))):
					continue
				var pine := _hash(x, y, 4) % 3 == 0
				trees.append([Vector2(x * CELL.x + 4 + _hash(x, y, 6) % 9, y * CELL.y + 3 + _hash(x, y, 7) % 7),
						_frame("trees", "pine" if pine else "tree", _hash(x, y, 5) % (2 if pine else 3)),
						Vector2i(x, y)])
		return trees

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
	# opaque pixels in the world, and to `occluders`, its chunk's.
	func _add_occluder(sprite: Sprite2D, kind: String, sheet: Dictionary, occluders: Array[Sprite2D]) -> void:
		var region := Rect2i((sprite.texture as AtlasTexture).region)
		var used := (sheet.image as Image).get_region(region).get_used_rect()
		sprite.set_meta("solid", Rect2(sprite.position + sprite.offset + Vector2(used.position), used.size))
		sprite.set_meta("kind", kind)
		_standing.add_child(sprite)
		occluders.append(sprite)
		var rect := Rect2(sprite.position + sprite.offset, sprite.texture.get_size())
		var keys: Array[Vector2i] = []
		for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
			for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
				var key := Vector2i(bx, by)
				if not _buckets.has(key):
					_buckets[key] = []
				_buckets[key].append(sprite)
				keys.append(key)
		sprite.set_meta("buckets", keys)

	func _kind(x: int, y: int) -> String:
		return _kinds[clampi(y, 0, _height - 1) * _width + clampi(x, 0, _width - 1)]

	func _hash(x: int, y: int, salt: int) -> int:
		var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
		h = (h ^ (h >> 13)) * 1274126177
		return absi(h ^ (h >> 16))

	# Dithers ink over every other pixel of `rect`, given in the world, on `image`, whose top-left
	# is `origin` in the world.
	func _dither(image: Image, rect: Rect2i, origin: Vector2i) -> void:
		var area := Rect2i(rect.position - origin, rect.size).intersection(Rect2i(Vector2i.ZERO, image.get_size()))
		for x in range(area.position.x, area.end.x):
			for y in range(area.position.y, area.end.y):
				if (x + origin.x + y + origin.y) % 2 == 0:
					image.set_pixel(x, y, INK)

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


## A high-realm skill levelling a whole village, resolved by the core. The world is made with
## WorldAdapter.create from SEED's region: its map, a structure on every cell of each building's
## footprint, LEVEL_PEOPLE Demonic people in LEVEL_ARMIES armies on the free cells round village
## LEVEL_VILLAGE, and an Orthodox caster at LEVEL_CASTER whose skill reaches LEVEL_RADIUS cells
## round the village's centre. It casts at each of LEVEL_CASTS, and the core resolves each cast on
## the next tick: the people it reaches lose health, and each structure durability, its stage
## following LEVEL_FLOORS. The view draws a building at the stage most of its cells are in, the
## worse on a tie. Around the casts it pulls the camera back from 3x to 2x, raises a pillar of qi
## over the village, drops blades of sword qi on it, and at each impact rings the ground, raises
## dust and throws debris from each building the cast staged down, and shakes the camera.
class Leveller extends RefCounted:
	var region: WorldAdapter
	var world: WorldAdapter
	var view: AngleView
	var target := Vector2.ZERO
	var caster := 0
	## The people of the village, by id, with their health before the first cast.
	var villagers := {}
	var casts_given := 0
	var _cells_of := {}
	var _ring: RingFx
	var _pillar: Sprite2D
	var _blades: Array = []
	var _bursts: Array = []
	var _impacts: Array[float] = []
	var _stages := {}

	func _init() -> void:
		region = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
		var village: Dictionary = region.villages()[LEVEL_VILLAGE]
		var corner: Vector2i = village.corner
		var area: Vector2i = village.area
		target = (Vector2(corner) + Vector2(area) / 2.0) * WorldAdapter.CELL_SIZE
		var rows := region.map_rows()
		var taken := {}
		var structures: Array[Dictionary] = []
		for structure in region.structure_count():
			if region.structure_field_state(structure) >= 0:
				continue
			var cells: Array[int] = []
			var at := region.structure_cell(structure)
			var size := region.structure_footprint(structure)
			for dx in size.x:
				for dy in size.y:
					taken[at + Vector2i(dx, dy)] = true
					cells.append(structures.size())
					structures.append({"cell": at + Vector2i(dx, dy), "durability": LEVEL_DURABILITY})
			_cells_of[structure] = cells
		var free: Array[Vector2i] = []
		var around := Rect2i(corner - Vector2i(2, 2), area + Vector2i(4, 4))
		for y in range(around.position.y, around.end.y):
			for x in range(around.position.x, around.end.x):
				if rows[y][x] == "." and not taken.has(Vector2i(x, y)):
					free.append(Vector2i(x, y))
		var armies: Array[Dictionary] = [{"objective": LEVEL_CASTER, "faction": 0, "skills": [IDLE_SKILL, LEVEL_SKILL],
				"members": [{"cell": LEVEL_CASTER, "health": 1000}]}]
		var stride := maxf(1.0, float(free.size()) / LEVEL_PEOPLE)
		for n in LEVEL_ARMIES:
			var members: Array[Dictionary] = []
			for k in LEVEL_PEOPLE / LEVEL_ARMIES:
				var cell := free[int((k * LEVEL_ARMIES + n) * stride) % free.size()]
				members.append({"cell": cell, "health": LEVEL_HEALTH})
			armies.append({"objective": members[0].cell, "faction": 1, "skills": [IDLE_SKILL], "members": members})
		world = WorldAdapter.create(SEED, rows, armies, structures, {"separation": 0,
				"hostile": [[false, true], [true, false]], "stage_floors": LEVEL_FLOORS})
		caster = world.roster(0)[0]
		for army in world.army_count():
			world.order(army, Vector2i(world.body_positions(army)[0] / WorldAdapter.CELL_SIZE), PackedInt64Array([0]))
		# The village's people go about it before the strike: each army to a cell of its own.
		for army in range(1, world.army_count()):
			world.order(army, free[(army * 37) % free.size()], PackedInt64Array([0]))
		for army in range(1, world.army_count()):
			for person in world.roster(army):
				villagers[person] = world.person_health(person)

	## Lays the view out over the region's look, the stages read from this world.
	func show(on: AngleView) -> void:
		view = on
		view.show_world(world, 0, true, region, _stage_of)
		_ring = RingFx.new()
		view.under.add_child(_ring)
		_pillar = Sprite2D.new()
		_pillar.centered = false
		_pillar.offset = Vector2(-24, -186)
		_pillar.position = view.to_screen(target).round()
		_pillar.visible = false
		view.over.add_child(_pillar)
		var rng := RandomNumberGenerator.new()
		rng.seed = SEED
		for landing: float in LEVEL_CASTS:
			for n in LEVEL_BLADES:
				var angle := rng.randf() * TAU
				var reach := sqrt(rng.randf()) * LEVEL_RADIUS
				var at := view.to_screen(target + Vector2(cos(angle), sin(angle)) * reach * WorldAdapter.CELL_SIZE).round()
				var blade := Sprite2D.new()
				blade.centered = false
				blade.offset = Vector2(-8, -44)
				blade.visible = false
				view.over.add_child(blade)
				_blades.append({"sprite": blade, "at": at, "land": landing - rng.randf() * 0.12})

	## Whether a cast should be given before the tick that lands at `seconds`, and gives it.
	func cast_before(seconds: float) -> void:
		if casts_given < LEVEL_CASTS.size() and seconds >= LEVEL_CASTS[casts_given]:
			world.cast(caster, 1, target)
			casts_given += 1

	## After a tick: an impact for each building the tick staged down, and the shake.
	func note_tick(seconds: float) -> void:
		var staged := []
		for structure: int in _cells_of:
			var stage := _stage_of({"id": structure})
			if stage > _stages.get(structure, 0):
				staged.append(structure)
			_stages[structure] = stage
		if staged.is_empty():
			return
		_impacts.append(seconds)
		var rng := RandomNumberGenerator.new()
		rng.seed = SEED + _impacts.size()
		for structure: int in staged:
			var at := region.structure_cell(structure)
			var size := region.structure_footprint(structure)
			var foot := Vector2((at.x + size.x / 2.0) * AngleView.CELL.x, (at.y + size.y) * AngleView.CELL.y)
			var dust := Sprite2D.new()
			dust.centered = false
			dust.offset = Vector2(-24, -28)
			dust.position = foot
			view.over.add_child(dust)
			_bursts.append({"sprite": dust, "from": seconds, "kind": "dust"})
			for n in 2 + size.x:
				var shard := Sprite2D.new()
				shard.centered = false
				shard.offset = Vector2(-4, -4)
				view.over.add_child(shard)
				_bursts.append({"sprite": shard, "from": seconds, "kind": "shard",
						"at": foot - Vector2(rng.randf_range(-size.x, size.x) * 6, 20),
						"velocity": Vector2(rng.randf_range(-70, 70), rng.randf_range(-150, -80))})

	## Draws the effects at `seconds` into the sequence, and returns the camera: [zoom, centre].
	func update(seconds: float) -> Array:
		var rise := 1.2
		var fall := 4.4
		_pillar.visible = seconds >= rise and seconds < fall + 0.5
		if _pillar.visible:
			var index := mini(int((seconds - rise) / 0.08), 5)
			if seconds >= fall:
				index = maxi(5 - int((seconds - fall) / 0.08), 0)
			elif index == 5:
				index = 4 + int(seconds / 0.08) % 2
			_pillar.texture = view.effect("pillar", "rise", index)
		for blade: Dictionary in _blades:
			var sprite: Sprite2D = blade.sprite
			var left: float = blade.land - seconds
			sprite.visible = left >= 0.0 and left < BLADE_FALL
			if sprite.visible:
				sprite.position = blade.at - Vector2(0, round(BLADE_DROP * left / BLADE_FALL))
				sprite.texture = view.effect("qi", "fall", int(seconds / 0.06) % 4)
		for burst: Dictionary in _bursts:
			var sprite: Sprite2D = burst.sprite
			var age: float = seconds - burst.from
			if burst.kind == "dust":
				sprite.visible = age < 0.6
				if sprite.visible:
					sprite.texture = view.effect("dust", "puff", mini(int(age / 0.12), 4))
			else:
				sprite.visible = age < 0.6
				if sprite.visible:
					var v: Vector2 = burst.velocity
					sprite.position = (burst.at + v * age + Vector2(0, 360) * age * age).round()
					sprite.texture = view.effect("debris", "shard", int(age / 0.08) % 4)
		var ring_age := INF
		var shake := 0.0
		for impact in _impacts:
			if seconds >= impact:
				ring_age = minf(ring_age, seconds - impact)
				shake = maxf(shake, SHAKE * maxf(0.0, 1.0 - (seconds - impact) / SHAKE_SECONDS))
		_ring.show_ring(view.to_screen(target).round(), Vector2(LEVEL_RADIUS * AngleView.CELL.x,
				LEVEL_RADIUS * AngleView.CELL.y), ring_age)
		var pull := smoothstep(1.0, 1.8, seconds) - smoothstep(5.0, 5.8, seconds)
		var zoom := lerpf(3.0, 2.0, pull)
		var centre := LEVEL_FOCUS.lerp(LEVEL_WIDE, pull) * AngleView.CELL
		var offset := Vector2(round(sin(seconds * 97.0) * shake), round(cos(seconds * 71.0) * shake))
		return [zoom, centre.round() + offset]

	## People of the village the casts reached, and those of them dead.
	func hit() -> Array[int]:
		var reached := 0
		var dead := 0
		for person: int in villagers:
			var health := world.person_health(person)
			if health < villagers[person]:
				reached += 1
			if health <= 0:
				dead += 1
		return [reached, dead]

	func stages() -> Dictionary:
		var counts := {}
		for structure: int in _cells_of:
			var key: String = AngleView.STAGES[_stage_of({"id": structure})]
			counts[key] = counts.get(key, 0) + 1
		return counts

	func _stage_of(entry: Dictionary) -> int:
		var count := [0, 0, 0, 0]
		for cell: int in _cells_of.get(entry.id, []):
			count[clampi(world.structure_stage(cell), 0, 3)] += 1
		var best := 0
		for stage in 4:
			if count[stage] >= count[best]:
				best = stage
		return best


## The impact's ring on the ground: an ellipse of azure pixels growing to the skill's reach,
## thinning by dithering as it ages.
class RingFx extends Node2D:
	const COLOURS := [Color("f4f1e6"), Color("a8d0f5"), Color("5b9ae0")]
	var _centre := Vector2.ZERO
	var _radii := Vector2.ZERO
	var _age := INF

	func show_ring(centre: Vector2, radii: Vector2, age: float) -> void:
		_centre = centre
		_radii = radii
		_age = age
		queue_redraw()

	func _draw() -> void:
		if _age > 0.5:
			return
		var grow := minf(1.0, _age / 0.2)
		var thin := maxf(0.0, (_age - 0.2) / 0.3)
		for band in 3:
			var r := _radii * grow * (1.0 - band * 0.04)
			var steps := maxi(int(TAU * r.x), 16)
			for n in steps:
				var angle := TAU * n / steps
				var at := (_centre + Vector2(cos(angle) * r.x, sin(angle) * r.y)).floor()
				if posmod(int(at.x) * 7 + int(at.y) * 3, 10) >= int(thin * 10):
					draw_rect(Rect2(at, Vector2(2, 1)), COLOURS[band])


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2 or not args[1] in ["frames", "stills", "bench", "level"]:
		printerr("angle: usage: -- <dir> frames|stills|level|bench [m1|level] [pan] [flat] [solid]")
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
	elif _mode == "level" or (_mode == "bench" and "level" in _flags):
		if _mode == "bench":
			DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		DisplayServer.window_set_size(Vector2i(1920, 1080))
		DirAccess.make_dir_recursive_absolute(_dir.path_join("level"))
		_level = Leveller.new()
		_view = AngleView.new()
		root.add_child(_view)
		_level.show(_view)
		_camera = Camera2D.new()
		root.add_child(_camera)
		_place_camera(_level.update(0.0))
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


# The point `distance` cells along PAN's loop.
func _pan_at(distance: float) -> Vector2:
	var length := 0.0
	for n in PAN.size():
		length += PAN[n].distance_to(PAN[(n + 1) % PAN.size()])
	var left := fmod(distance, length)
	for n in PAN.size():
		var from: Vector2 = PAN[n]
		var to: Vector2 = PAN[(n + 1) % PAN.size()]
		if left <= from.distance_to(to):
			return from.move_toward(to, left)
		left -= from.distance_to(to)
	return PAN[0]


# The part of the world on screen, in world pixels; the camera has no smoothing and its limits
# lie outside what it shows.
func _view_rect() -> Rect2:
	var size := root.get_visible_rect().size / _camera.zoom
	return Rect2(_camera.position - size / 2.0, size)


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
		"level":
			return _level_frames()
		"bench":
			return _level_bench(delta) if _level != null else _bench(delta)
	return false


func _place_camera(placed: Array) -> void:
	_camera.zoom = Vector2(placed[0], placed[0])
	_camera.position = placed[1]


# Runs the levelling world's ticks up to `seconds`, a cast given before the tick it lands on.
func _level_to(seconds: float) -> void:
	while _level_ticks < int(seconds * WorldAdapter.TICKS_PER_SECOND):
		var at := float(_level_ticks + 1) / WorldAdapter.TICKS_PER_SECOND
		_level.cast_before(at + 1.0 / WorldAdapter.TICKS_PER_SECOND)
		_level.world.tick()
		_level_ticks += 1
		_view.note_tick()
		_level.note_tick(at)


# The levelling sequence, LEVEL_FPS frames a second for LEVEL_SECONDS, at 1920x1080 without the
# HUD: <dir>/level/frame_<n>.png.
func _level_frames() -> bool:
	if _frame == 1:
		_view.refresh(_view_rect(), 1.0, 0)
		return false
	var n := _frame - 2
	var seconds := float(n) / LEVEL_FPS
	_level_to(seconds)
	_place_camera(_level.update(seconds))
	_view.refresh(_view_rect(), 1.0, int(seconds * 1000))
	RenderingServer.force_draw(false)
	root.get_texture().get_image().save_png(_dir.path_join("level/frame_%03d.png" % n))
	if n % 15 == 0:
		print("angle: level frame %d at %.2f s, zoom %.2f, %d chunks, villagers reached and dead %s, stages %s" % [
				n, seconds, _camera.zoom.x, _view.resident, _level.hit(), _level.stages()])
	if seconds >= LEVEL_SECONDS:
		print("angle: level done, %d casts, villagers reached and dead %s, stages %s" % [_level.casts_given,
				_level.hit(), _level.stages()])
		quit(0)
		return true
	return false


# The levelling sequence in real time, vsync off, every frame timed; the frames from the
# pull-back to the camera's return are timed again on their own.
func _level_bench(delta: float) -> bool:
	var now := Time.get_ticks_usec()
	_bench_frames += 1
	if _bench_frames <= 2:
		_view.refresh(_view_rect(), 1.0, 0)
		_last_usec = now
		return false
	_pan_seconds += delta
	var seconds := _pan_seconds
	_level_to(seconds)
	_place_camera(_level.update(seconds))
	_view.refresh(_view_rect(), fmod(seconds * WorldAdapter.TICKS_PER_SECOND, 1.0), int(seconds * 1000))
	var time := (now - _last_usec) / 1000.0
	_last_usec = now
	_times.append(time)
	if seconds >= 1.0 and seconds < 6.0:
		_level_window.append(time)
	if seconds < LEVEL_SECONDS:
		return false
	for pair: Array in [["whole sequence", _times], ["pull-back to return, 1.0 to 6.0 s", _level_window]]:
		var sorted: PackedFloat64Array = pair[1].duplicate()
		sorted.sort()
		print("angle bench: levelling, %s: %d frames, p50 %.3f ms, p99 %.3f ms, max %.3f ms" % [pair[0],
				sorted.size(), sorted[sorted.size() / 2], sorted[ceili(sorted.size() * 0.99) - 1], sorted[-1]])
	print("angle bench: levelling, window %s, vsync off, %s, Godot %s; %d casts; villagers reached and dead %s of %d; stages %s" % [
			DisplayServer.window_get_size(), RenderingServer.get_video_adapter_name(),
			Engine.get_version_info().string, _level.casts_given, _level.hit(), _level.villagers.size(),
			_level.stages()])
	print("angle bench: levelling, chunks %d resident now, %d at most, %d built, %d of them again on a stage change; ground images %d bytes" % [
			_view.resident, _view.most_resident, _view.built, _view.rebuilt, _view.ground_bytes])
	quit(0)
	return true


# One frame of the sequence each process frame, drawn before it is read, so a capture holds it
# even while the window is hidden and the loop would not draw.
func _frames() -> bool:
	# The first frame streams the chunks in; what it adds draws from the next.
	if _frame == 1:
		_view.refresh(_view_rect(), 1.0, 0)
		return false
	if _sequence_frame > 0:
		for i in TICKS_PER_FRAME:
			_tick()
	_view.refresh(_view_rect(), 1.0, _sequence_frame * FRAME_MS)
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
			_view.refresh(_view_rect(), 1.0, STILLS.get(shot.still, 0) * FRAME_MS)
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
	if "pan" in _flags:
		_pan_seconds += delta
		_camera.position = (_pan_at(_pan_seconds * PAN_SPEED) * AngleView.CELL).round()
	_view.refresh(_view_rect(), _since_tick / step, Time.get_ticks_msec())
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
	var builds := Array(_view.build_usec)
	builds.sort()
	print("angle bench: chunks of %d cells; %d resident now, %d at most; %d built, %d freed; ground images %d bytes now (%d a chunk)" % [
		AngleView.CHUNK, _view.resident, _view.most_resident, _view.built, _view.freed, _view.ground_bytes,
		_view.ground_bytes / maxi(_view.resident, 1)])
	print("angle bench: chunk build p50 %.3f ms, max %.3f ms; camera travelled %.0f cells" % [
		builds[builds.size() / 2] / 1000.0, builds[-1] / 1000.0, _pan_seconds * PAN_SPEED])
	print("angle bench: texture memory %.1f MB, video memory %.1f MB, static memory %.1f MB, %d nodes" % [
		Performance.get_monitor(Performance.RENDER_TEXTURE_MEM_USED) / 1048576.0,
		Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED) / 1048576.0,
		Performance.get_monitor(Performance.MEMORY_STATIC) / 1048576.0,
		Performance.get_monitor(Performance.OBJECT_NODE_COUNT)])
	quit(0)
	return true
