# Mockup of the 2.5D view proposed for #200: the world seen from Albion's 3/4 angle, drawn in the
# art under art/mockup/angle/, a cell 16 pixels wide and 12 deep, everything standing sorted by
# the bottom edge of its footprint and a person hidden by a building or a tree drawn over it as an
# outline in its faction's light accent. It draws SEED's region from the core, lived YEARS years
# with days of TICKS_PER_DAY ticks as evidence/pr-176 does, round the village VILLAGE names and
# the seat below it. Every position is the core's; the mockup only gives orders, through the
# order port every driver uses: Demonic people walk WALKS through the village and the seat, then
# Orthodox armies, the player's among them, gather at MUSTER and march on the village, and the
# Demonic people meet them.
# With valley it draws instead the Valley, a hand-composed map of levels, a village, a walled
# town, terraces and a riverside, its ground drawn by angle/ground.gdshader and baked per chunk at
# map build, walked by VALLEY_PEOPLE of the core, or CROWD with crowd; with a region of REGIONS,
# that region laid out in the valley's place.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://angle.gd -- <dir> frames [valley]
#     FRAMES frames, TICKS_PER_FRAME ticks apart, 640x360 at 1x without the HUD:
#     <dir>/frames/frame_<n>.png, and 4x crops of a person behind a house and in front of one,
#     depth_behind.png and depth_front.png.
#   godot --path . --script res://angle.gd -- <dir> stills [valley [crowd] | <region>]
#     Under the HUD of play.tscn, 1920x1080: the sequence at STILLS' frames, or the valley's
#     VALLEY_STILLS after VALLEY_TICKS ticks, in ko and en: <dir>/<still>_<locale>.png. A region
#     gives <dir>/<region>_ko.png at 2x and <dir>/<region>_village_ko.png at 3x.
#   godot --path . --script res://angle.gd -- <dir> level
#     The Leveller's skill taking a village down, LEVEL_SECONDS at LEVEL_FPS, 1920x1080:
#     <dir>/level/frame_<n>.png.
#   godot --path . --script res://angle.gd -- <dir> bench [valley [crowd] | <region> | m1 | level] [pan] [flat] [solid]
#     Frame time, vsync off, in a 1920x1080 window: the sequence, the valley or a region in real
#     time under the HUD; with m1 the core's M1 scenario of 500 people without it; with level the
#     Leveller's moment. pan moves the camera round PAN, or the valley's VALLEY_PAN, so chunks
#     stream in and out; flat turns the y-sort off and solid the outlines, to measure what each
#     costs. Prints p50, p99 and max over TIMED_FRAMES or BENCH_SECONDS, whichever ends first,
#     what streaming held and built, and how long the map build baked.
# Every mode quits on its own, past DEADLINES at the latest.
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
# The valley's stills, each at Valley.PLACES' cell, after VALLEY_TICKS ticks of its people
# walking; the whole valley pulled back to 2x, the rest at the PC's 3x. VALLEY_PEOPLE walk it in
# armies of VALLEY_ARMY, or CROWD people with crowd.
const VALLEY_STILLS := ["village", "town", "terraces", "riverside", "valley"]
# The regions of #200's screens. A region's stills are the region at 2x and its village or town at
# the PC's 3x, in Korean.
const REGIONS := ["central", "sichuan", "west", "jiangnan"]
const VALLEY_TICKS := 90
const VALLEY_PEOPLE := 80
const VALLEY_ARMY := 4
const CROWD := 500
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
# The valley's pan: round its places, in this order.
const VALLEY_PAN := ["village", "terraces", "riverside", "town"]
# A bench reports on what it has once its timed frames run this long, and any run quits after
# DEADLINES' seconds for its mode whatever it is doing, so no window is left open.
const BENCH_SECONDS := 40.0
const DEADLINES := {"frames": 300.0, "stills": 600.0, "level": 240.0, "bench": 150.0}
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
var _region := ""
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
var _bench_started := 0
var _driver: Driver
var _valley: Valley
var _villagers: Villagers
var _level: Leveller
var _level_ticks := 0
var _level_window := PackedFloat64Array()
var _worst := [0.0, 0.0]


## The world in the 2.5D projection, drawn from a look (RegionLook over a generated region, or
## Valley, composed by hand) and streamed by chunks of CHUNK x CHUNK cells: a chunk is built when
## it comes within STREAM_MARGIN of the screen and freed once it is a chunk further away. Ground
## at level 0 and below is composed into one image a chunk; each row of a raised level is a strip,
## its top and the face where it drops to its south, sorted with everything standing by the
## row's north edge, so whoever is behind a terrace is drawn behind it. Shadows are baked into
## the ground as a chunk is built: each structure's, tree's and cliff's, cast to the lower right,
## two steps darker along the palette's ramps. Bodies get sprites only on screen, and the hidden
## parts of those are outlined over what hides them, each outline clipped to its occluder.
class AngleView extends Node2D:
	const ART := "res://art/mockup/angle/"
	const CELL := Vector2(16, 12)
	# grass 6 of docs/art-direction.md §Palette: a raised edge's lit lip.
	const LIP := Color("8fae5a")
	# stone 5: the lit edge of rock.
	const ROCK_LIP := Color("b4bcc4")
	const SUB := 256.0
	const GROUND_KINDS := ["training_ground"]
	const STAGES := ["intact", "damaged", "destroyed", "rubble"]
	const FACTIONS := ["orthodox", "demonic", "villager"]
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
	# Rows below a chunk's ground image for ground lower than level 0 and its faces.
	const LOW_PAD := 40
	# Where a shadow falls for each pixel of height: right and down, light being upper left.
	const SHADOW := Vector2(0.55, 0.25)

	var world: WorldAdapter
	## What is drawn: terrain, levels, structures and trees, by the calls RegionLook makes.
	var look: Object
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
	## bytes their ground images and strips hold now.
	var resident := 0
	var built := 0
	var freed := 0
	var most_resident := 0
	var ground_bytes := 0
	## How long each chunk took to build as it streamed in, and to bake, in microseconds.
	var build_usec := PackedInt64Array()
	var bake_usec := PackedInt64Array()
	## Effects drawn on the ground under everything standing, and over all of it.
	var under := Node2D.new()
	var over := Node2D.new()

	var _stage_of := Callable()
	var _sheets := {}
	var _size := Vector2i.ZERO
	var _px := 16
	var _shade := {}
	var _standing := Node2D.new()
	var _shadows := Node2D.new()
	var _ground := Node2D.new()
	var _chunks := {}
	var _baked := {}
	var _shadow_cache := {}
	var _buckets := {}
	# Structures by the chunk of their footprint's corner: {kind, corner, size, id}.
	var _structures_in := {}
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

	## A frame of an effect's sheet, loaded the first time it is asked for.
	func effect(sheet: String, tag: String, index: int) -> AtlasTexture:
		return _frame(sheet, tag, index)

	## Reads `shown`'s bodies, and draws `drawn_from`: a WorldAdapter's region (`shown`'s when null)
	## or a look of its own; a structure is drawn at the stage `stage_of` gives its entry, or the
	## look's.
	func show_world(shown: WorldAdapter, steered_army: int, sorted := true, drawn_from: Object = null,
			stage_of := Callable()) -> void:
		world = shown
		var source: Object = drawn_from if drawn_from != null else shown
		look = RegionLook.new(source) if source is WorldAdapter else source
		steered = steered_army
		_stage_of = stage_of
		_standing.y_sort_enabled = sorted
		_size = look.size()
		_px = look.level_px()
		var table: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(ART + "shade.json"))
		for hex: String in table:
			_shade[Color(hex).to_rgba32()] = Color(table[hex][1])
		for entry: Dictionary in look.structures():
			if not _has_sheet(entry.kind):
				print("angle: no art for %s at %s" % [entry.kind, entry.corner])
				continue
			var key: Vector2i = entry.corner / CHUNK
			if not _structures_in.has(key):
				_structures_in[key] = []
			_structures_in[key].append(entry)
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
		# The ground shader's viewport draws only from the next frame, so the shaded map is baked
		# at the first refresh after it; another look's is baked at once.
		if _shaded():
			_set_up_baker()
			_baker_frame = Engine.get_process_frames()
		elif look.baked_shadows():
			bake_all()

	## Where a point in sub-cell units is drawn: x by CELL.x, y by CELL.y, raised by its level.
	func to_screen(sub: Vector2) -> Vector2:
		return _ground_point(sub) - Vector2(0, _lift(sub))

	func _ground_point(sub: Vector2) -> Vector2:
		return Vector2(sub.x / SUB * CELL.x, sub.y / SUB * CELL.y)

	# How far a point is raised: its cell's level, and on stairs, part way down to the level
	# below them as it nears their foot.
	func _lift(sub: Vector2) -> float:
		var cell := Vector2i((sub / SUB).floor())
		var level: float = look.level_at(cell.x, cell.y)
		if look.stairs_at(cell.x, cell.y):
			level -= fposmod(sub.y / SUB, 1.0)
		return level * _px

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
		if _shaded() and _baked.is_empty():
			# Process frames, not frames drawn: a hidden window draws none, and the bake forces its own.
			if Engine.get_process_frames() <= _baker_frame + 1:
				return
			bake_all()
			# Every chunk is baked: the baker would otherwise draw its shader again every frame.
			_baker.queue_free()
			_baker = null
		_stream(view)
		# A structure that changed stage shows it at once, its shadow with it.
		for key: Vector2i in _chunks:
			for sprite: Sprite2D in _chunks[key].occluders:
				sprite.clip_children = CanvasItem.CLIP_CHILDREN_DISABLED
				if sprite.has_meta("stages"):
					var stage := _stage(sprite.get_meta("entry"))
					if stage != sprite.get_meta("stage"):
						sprite.set_meta("stage", stage)
						sprite.texture = sprite.get_meta("stages")[stage]
						var shadow: Sprite2D = sprite.get_meta("shadow")
						shadow.texture = _stage_shadow(sprite.get_meta("entry").size, stage)
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
			var sub := before.lerp(now, clampf(between, 0.0, 1.0))
			var foot := _ground_point(sub).round()
			var lift := roundf(_lift(sub))
			if not shown.has_point(foot - Vector2(0, lift)):
				continue
			on_screen[person] = true
			var sprite: Sprite2D = _bodies.get(person)
			if sprite == null:
				sprite = Sprite2D.new()
				sprite.centered = false
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
			# Sorted by where its feet are on the ground, drawn raised by its level.
			sprite.position = foot + Vector2(0, 0.25)
			sprite.offset = -FEET - Vector2(0, lift)
			_body_shadows[person].position = foot - Vector2(0, lift)
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
			var body: Sprite2D = _bodies[lead[0]]
			var at := body.position + body.offset + FEET
			_ring.position = at
			_chevron.position = at
			_chevron.texture = _chevron_frames[(now_ms / CHEVRON_MS) % 2]

	## Pixels the chunk at `key` covers on the ground, before raising.
	func chunk_rect(key: Vector2i) -> Rect2:
		return Rect2(Vector2(key) * CHUNK * CELL, CHUNK * CELL)

	# Builds the nearest missing chunks round `view`, at most BUILDS_PER_FRAME, and frees those a
	# chunk past the streamed ring.
	func _stream(view: Rect2) -> void:
		# Raised ground draws above its cells, so the ring reaches further down the screen.
		var reach := view.grow(STREAM_MARGIN).grow_individual(0, 0, 0, look.max_level() * _px)
		var wanted := _chunks_over(reach)
		var kept := _chunks_over(reach.grow(CHUNK * CELL.y))
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
		var last := Vector2i((_size.x - 1) / CHUNK, (_size.y - 1) / CHUNK)
		for cx in range(maxi(floori(rect.position.x / size.x), 0), mini(floori(rect.end.x / size.x), last.x) + 1):
			for cy in range(maxi(floori(rect.position.y / size.y), 0), mini(floori(rect.end.y / size.y), last.y) + 1):
				keys[Vector2i(cx, cy)] = true
		return keys

	func _level(x: int, y: int) -> int:
		return look.level_at(clampi(x, 0, _size.x - 1), clampi(y, 0, _size.y - 1))

	# The chunk: its ground at level 0 and below in one image, each raised row a strip, shadows
	# baked into both, then its structures and trees.
	# The ground shader's kinds and face materials, in the order its cell texture indexes them.
	const KINDS := ["water", "plain", "hills", "forest", "rock", "lane", "paving", "paddy", "garden", "steps", "earth",
			"sand", "snow", "wheat"]
	const FACES := ["rock_face", "bank_face", "stone_face"]
	# Each kind's ramp as hue in degrees, saturation and lightness, stepped dark to light by
	# shifting hue toward blue in the dark and toward yellow in the light (#212's ramp theory);
	# a kind given four colours instead is drawn with them as they are.
	const RAMPS := [[205, 0.45, 0.42], [100, 0.42, 0.42], [80, 0.36, 0.5], [130, 0.38, 0.27], [220, 0.12, 0.46],
			[33, 0.32, 0.5], [215, 0.06, 0.6], ["2b5770", "3d7a8f", "5fa3a8", "8fae5a"],
			["3a2c24", "6b523a", "4f7a3f", "8fae5a"], [215, 0.06, 0.6], [28, 0.35, 0.36], [40, 0.45, 0.64],
			[210, 0.16, 0.86], [44, 0.6, 0.55]]
	const SHADE_COLOUR := Color(0.078, 0.094, 0.157)
	# Rows above a chunk's own for the ground its raised rows lift, and below for water and faces.
	const TOP_PAD := 88
	const BOTTOM_PAD := 40
	# Poisson spacing, in pixels, of detail and of trees, and the tries made for each per chunk.
	const DETAIL_SPACING := 4.2
	const DETAIL_TRIES := 2600
	const TREE_SPACING := 8.0
	const TREE_TRIES := 1400

	var _baker: SubViewport
	var _baker_frame := 0
	var _bake_material: ShaderMaterial
	var _ramp_colours: Array[Color] = []

	func _shaded() -> bool:
		return look.has_method("ground_cells")

	# The viewport the ground shader draws a chunk into, and its cell texture and ramps.
	func _set_up_baker() -> void:
		_baker = SubViewport.new()
		_baker.size = Vector2i(CHUNK * int(CELL.x), CHUNK * int(CELL.y) + TOP_PAD + BOTTOM_PAD)
		_baker.transparent_bg = true
		_baker.disable_3d = true
		_baker.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		var shader := Shader.new()
		shader.code = FileAccess.get_file_as_string(ART + "ground.gdshader")
		_bake_material = ShaderMaterial.new()
		_bake_material.shader = shader
		var rect := ColorRect.new()
		rect.size = Vector2(_baker.size)
		rect.material = _bake_material
		_baker.add_child(rect)
		add_child(_baker)
		var cells: Image = look.ground_cells(KINDS, FACES)
		_bake_material.set_shader_parameter("cells", ImageTexture.create_from_image(cells))
		_bake_material.set_shader_parameter("map_size", Vector2i(cells.get_width(), cells.get_height()))
		_bake_material.set_shader_parameter("image_size", Vector2(_baker.size))
		var ramps := PackedVector3Array()
		# A region's look may recolour kinds, and set its air.
		var specs := RAMPS.duplicate()
		if look.has_method("ramps"):
			var changed: Dictionary = look.ramps()
			for kind: String in changed:
				specs[KINDS.find(kind)] = changed[kind]
		if look.has_method("air"):
			var air: Dictionary = look.air()
			for name: String in air:
				_bake_material.set_shader_parameter(name, air[name])
		for spec: Array in specs:
			for step in 4:
				var colour := Color(spec[step]) if spec[0] is String else _ramp_step(spec, step)
				_ramp_colours.append(colour)
				ramps.append(Vector3(colour.r, colour.g, colour.b))
		_bake_material.set_shader_parameter("ramps", ramps)

	func _ramp_step(spec: Array, step: int) -> Color:
		var rock: bool = spec[1] < 0.1
		var hue: float = spec[0] + [-22.0, -8.0, 6.0, 14.0][step] * (0.5 if rock else 1.0)
		var saturation: float = clampf(spec[1] + [0.04, 0.0, 0.02, -0.04][step], 0.0, 1.0)
		var lightness: float = clampf(spec[2] + [-0.13, -0.05, 0.04, 0.12][step], 0.0, 1.0)
		var q := lightness * (1.0 + saturation) if lightness < 0.5 else lightness + saturation - lightness * saturation
		var p := 2.0 * lightness - q
		var h := fposmod(hue, 360.0) / 360.0
		var channel := func(t: float) -> float:
			t = fposmod(t, 1.0)
			if t < 1.0 / 6.0:
				return p + (q - p) * 6.0 * t
			if t < 0.5:
				return q
			if t < 2.0 / 3.0:
				return p + (q - p) * (2.0 / 3.0 - t) * 6.0
			return p
		return Color(channel.call(h + 1.0 / 3.0), channel.call(h), channel.call(h - 1.0 / 3.0))

	func _ramp_colour(kind: int, step: int) -> Color:
		return _ramp_colours[kind * 4 + clampi(step, 0, 3)]

	# Draws the chunk at `key` with the ground shader, in colour or, with `pass_kind` 1, what each
	# pixel is; the image's top-left is `top` on the screen, unlifted.
	func _draw_ground(key: Vector2i, pass_kind: int, top: Vector2i) -> Image:
		var origin := key * CHUNK
		_bake_material.set_shader_parameter("origin", Vector2(top))
		_bake_material.set_shader_parameter("row_from", origin.y)
		_bake_material.set_shader_parameter("row_to", mini(origin.y + CHUNK, _size.y))
		_bake_material.set_shader_parameter("pass_kind", pass_kind)
		RenderingServer.force_draw(false)
		var image := _baker.get_texture().get_image()
		image.convert(Image.FORMAT_RGBA8)
		return image

	# The chunk drawn off the cell grid (#212): the shader's ground; detail scattered over it and
	# decorative trees clustered in it by Poisson-disc spacing from a seed the chunk's index
	# gives, the same on every build; contact shadows under trees and shadows cast by structures;
	# then cut into the ground at level 0 and below and a strip per raised row and level, as the
	# grid's chunks are, so people behind a raised row are hidden by it.
	func _compose_shaded(key: Vector2i) -> Dictionary:
		var origin := key * CHUNK
		var top := Vector2i(origin.x * int(CELL.x), origin.y * int(CELL.y) - TOP_PAD)
		var colour := _draw_ground(key, 0, top)
		var ids := _draw_ground(key, 1, top)
		var what := ids.get_data()
		var width := ids.get_width()
		var rng := RandomNumberGenerator.new()
		rng.seed = hash(key)
		_scatter_detail(colour, what, width, rng)
		var trees := _scatter_trees(colour, what, width, top, rng)
		for tree: Dictionary in look.trees_in(Rect2i(origin, Vector2i(CHUNK, CHUNK))):
			trees.append(tree)
		for tree: Dictionary in trees:
			_contact_shadow(colour, what, width, tree.foot - Vector2(0, tree.level * _px) - Vector2(top), tree.level)
		_shade_structures(key, colour, what, width, top)
		var pieces := _cut(colour, what, width, origin.y)
		var strips := {}
		var ground := Image.create_empty(1, 1, false, Image.FORMAT_RGBA8)
		var ground_at := Vector2(top)
		for piece_key: Variant in pieces:
			var piece: Dictionary = pieces[piece_key]
			if piece_key is String:
				ground = piece.image
				ground_at = Vector2(top + piece.at)
			else:
				strips[piece_key] = {"image": piece.image, "at": top + piece.at}
		return {"ground": ground, "ground_at": ground_at, "strips": strips, "trees": trees}

	# What the shader says a pixel is: [sort row offset, level, kind], kind 20 and up for a face;
	# a clear pixel has a sort row offset of -1.
	func _what(what: PackedByteArray, width: int, at: Vector2i) -> Vector3i:
		if at.x < 0 or at.y < 0 or at.x >= width or (at.y * width + at.x) * 4 + 3 >= what.size():
			return Vector3i(-1, 0, 0)
		var i := (at.y * width + at.x) * 4
		if what[i + 3] == 0:
			return Vector3i(-1, 0, 0)
		return Vector3i(what[i] - 1, what[i + 1] - 2, what[i + 2])

	func _poisson(rng: RandomNumberGenerator, size: Vector2i, spacing: float, tries: int) -> Array[Vector3]:
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

	# Tufts and flowers on grass, litter on the forest floor, pebbles on lanes and paving, reeds at
	# the water's edge: more of them near an edge between kinds, drawn only on their own ground.
	func _scatter_detail(colour: Image, what: PackedByteArray, width: int, rng: RandomNumberGenerator) -> void:
		var flowers := [Color("d63a2f"), Color("f5d27a"), Color("f4f1e6")]
		for point: Vector3 in _poisson(rng, colour.get_size(), DETAIL_SPACING, DETAIL_TRIES):
			var at := Vector2i(int(point.x), int(point.y))
			var here := _what(what, width, at)
			if here.x < 0 or here.z >= 20:
				continue
			var edge := false
			for d: Vector2i in [Vector2i(4, 0), Vector2i(-4, 0), Vector2i(0, 4), Vector2i(0, -4)]:
				var other := _what(what, width, at + d)
				if other.x >= 0 and other.z != here.z:
					edge = true
			if point.z > (0.9 if edge else 0.38):
				continue
			var marks := []
			var pick := rng.randf()
			match here.z:
				1, 2:
					if pick < 0.72:
						marks = [[Vector2i.ZERO, _ramp_colour(here.z, 0)], [Vector2i(-1, -1), _ramp_colour(here.z, 0)],
								[Vector2i(1, -1), _ramp_colour(here.z, 0)], [Vector2i(0, -1), _ramp_colour(here.z, 3)]]
					else:
						marks = [[Vector2i.ZERO, flowers[int(pick * 30) % 3]], [Vector2i(0, 1), _ramp_colour(here.z, 0)]]
				3:
					marks = [[Vector2i.ZERO, _ramp_colour(3, 0)], [Vector2i(1, -1), _ramp_colour(3, 0)],
							[Vector2i(-1, -1), _ramp_colour(3, 2)]]
				5, 6:
					marks = [[Vector2i.ZERO, _ramp_colour(4, 1)], [Vector2i(1, 0), _ramp_colour(4, 1)],
							[Vector2i(0, -1), _ramp_colour(4, 3)]]
				0:
					if edge:
						for t in 4:
							marks.append([Vector2i(0, -t), _ramp_colour(3, 3 if t == 3 else 1)])
				11:
					# Dry scrub and pebbles, sparse.
					if pick < 0.25:
						marks = [[Vector2i.ZERO, _ramp_colour(4, 0)], [Vector2i(-1, -1), _ramp_colour(4, 1)],
								[Vector2i(1, -1), _ramp_colour(4, 1)]]
					elif pick < 0.4:
						marks = [[Vector2i.ZERO, _ramp_colour(4, 1)], [Vector2i(1, 0), _ramp_colour(4, 3)]]
				13:
					# Ears standing above the rows.
					marks = [[Vector2i.ZERO, _ramp_colour(13, 3)], [Vector2i(0, 1), _ramp_colour(13, 1)]]
			for mark: Array in marks:
				var p: Vector2i = at + mark[0]
				var there := _what(what, width, p)
				if there.x >= 0 and there.y == here.y and there.z < 20:
					colour.set_pixelv(p, mark[1])

	# Decorative trees: dense inside forests, thinning toward their edges by noise, a lone tree
	# now and then in the open, each on a cell open to it. Trees the rules know come from the core.
	func _scatter_trees(colour: Image, what: PackedByteArray, width: int, top: Vector2i,
			rng: RandomNumberGenerator) -> Array:
		var trees := []
		for point: Vector3 in _poisson(rng, colour.get_size(), TREE_SPACING, TREE_TRIES):
			var at := Vector2i(int(point.x), int(point.y))
			var here := _what(what, width, at)
			if here.x < 0 or here.z >= 20:
				continue
			var foot := Vector2(top + at) + Vector2(0, here.y * _px)
			var cell := Vector2i(int(foot.x / CELL.x), int(foot.y / CELL.y))
			if not look.open_at(cell.x, cell.y):
				continue
			var density := _value_noise(foot / 30.0)
			var keep := (here.z == 3 and point.z < 0.55 + density * 0.6) or (here.z in [1, 2] and point.z < 0.02)
			if not keep:
				continue
			# A region names its own trees; the valley's are pines up high and broadleaf below.
			var pick: Array = look.tree_kind(here.y, rng) if look.has_method("tree_kind") else []
			if not pick.is_empty():
				trees.append({"foot": foot, "kind": pick[0], "variant": rng.randi_range(0, pick[1] - 1), "cell": cell,
						"level": here.y})
				continue
			var pine := here.y >= 2 or rng.randf() < 0.35
			trees.append({"foot": foot, "kind": "pine" if pine else "tree", "variant": rng.randi_range(0, 1 if pine else 2),
					"cell": cell, "level": here.y})
		return trees

	func _value_noise(p: Vector2) -> float:
		var i := p.floor()
		var f := p - i
		f = f * f * (Vector2(3, 3) - 2.0 * f)
		var corner := func(x: int, y: int) -> float: return float(_hash(x, y, 91) % 1000) / 1000.0
		var a: float = corner.call(int(i.x), int(i.y))
		var b: float = corner.call(int(i.x) + 1, int(i.y))
		var c: float = corner.call(int(i.x), int(i.y) + 1)
		var d: float = corner.call(int(i.x) + 1, int(i.y) + 1)
		return lerpf(lerpf(a, b, f.x), lerpf(c, d, f.x), f.y)

	# A tree's shadow on the ground at its foot, `at` in the image, on its own level.
	func _contact_shadow(colour: Image, what: PackedByteArray, width: int, at: Vector2, level: int) -> void:
		for dy in range(-3, 4):
			for dx in range(-10, 11):
				if pow(dx / 10.0, 2) + pow(dy / 3.0, 2) > 1.0:
					continue
				_darken_at(colour, what, width, Vector2i(int(at.x) + dx + 3, int(at.y) + dy + 1), level)

	# Shadows the structures on and round the chunk cast to the lower right, on their own level.
	func _shade_structures(key: Vector2i, colour: Image, what: PackedByteArray, width: int, top: Vector2i) -> void:
		var area := Rect2i(top, colour.get_size())
		for dx in range(-1, 2):
			for dy in range(-1, 2):
				for entry: Dictionary in _structures_in.get(key + Vector2i(dx, dy), []):
					if entry.kind in GROUND_KINDS or entry.has("tag") or _stage(entry) == 3:
						continue
					var level := _level(entry.corner.x, entry.corner.y + entry.size.y - 1)
					var rise: float = (_used(_entry_frame(entry, "intact")).size.y - entry.size.y * CELL.y) * (0.5 if _stage(entry) == 2 else 1.0)
					var lifted := Rect2i(Vector2i(area.position.x, area.position.y + level * _px), area.size)
					# The shadow's edge wanders a couple of pixels, so it is not a ruled line.
					_sweep(Rect2(Vector2(entry.corner) * CELL, Vector2(entry.size) * CELL).grow(2.0), rise, lifted,
							func(wx: int, gy: int) -> void:
								var wobble := Vector2(_value_noise(Vector2(wx, gy) / 5.0), _value_noise(Vector2(gy, wx) / 5.0)) * 4.0 - Vector2(2, 2)
								var inside := Rect2(Vector2(entry.corner) * CELL, Vector2(entry.size) * CELL)
								var swept := inside.merge(Rect2(inside.position + SHADOW * rise, inside.size))
								if swept.has_point(Vector2(wx, gy) + wobble) and not inside.has_point(Vector2(wx, gy) + wobble):
									_darken_at(colour, what, width, Vector2i(wx, gy - level * _px) - top, level))

	func _darken_at(colour: Image, what: PackedByteArray, width: int, at: Vector2i, level: int) -> void:
		var here := _what(what, width, at)
		if here.x < 0 or here.y != level or here.z >= 20:
			return
		var i := (at.y * width + at.x) * 4
		if what[i + 3] == 254:
			return
		what[i + 3] = 254
		colour.set_pixelv(at, colour.get_pixelv(at).lerp(SHADE_COLOUR, 0.38))

	# Cuts the chunk's image into its ground, level 0 and below, keyed "ground", and a piece for
	# each raised row and level, keyed by (row, level), each cropped to what it holds, with where
	# it stands in the image.
	func _cut(colour: Image, what: PackedByteArray, width: int, row_from: int) -> Dictionary:
		var height := colour.get_height()
		var data := colour.get_data()
		var bounds := {}
		for y in height:
			for x in width:
				var i := (y * width + x) * 4
				if what[i + 3] == 0:
					continue
				var level: int = what[i + 1] - 2
				var piece_key: Variant = "ground" if level <= 0 else Vector2i(row_from + what[i] - 1, level)
				var box: Variant = bounds.get(piece_key)
				if box == null:
					bounds[piece_key] = PackedInt32Array([x, y, x, y])
				else:
					var b: PackedInt32Array = box
					b[0] = mini(b[0], x)
					b[1] = mini(b[1], y)
					b[2] = maxi(b[2], x)
					b[3] = maxi(b[3], y)
					bounds[piece_key] = b
		var pieces := {}
		var images := {}
		var corners := {}
		for piece_key: Variant in bounds:
			var b: PackedInt32Array = bounds[piece_key]
			images[piece_key] = Image.create_empty(b[2] - b[0] + 1, b[3] - b[1] + 1, false, Image.FORMAT_RGBA8)
			corners[piece_key] = Vector2i(b[0], b[1])
		for y in height:
			for x in width:
				var i := (y * width + x) * 4
				if what[i + 3] == 0:
					continue
				var level: int = what[i + 1] - 2
				var piece_key: Variant = "ground" if level <= 0 else Vector2i(row_from + what[i] - 1, level)
				var at: Vector2i = corners[piece_key]
				(images[piece_key] as Image).set_pixel(x - at.x, y - at.y, Color8(data[i], data[i + 1], data[i + 2], 255))
		for piece_key: Variant in images:
			pieces[piece_key] = {"image": images[piece_key], "at": corners[piece_key]}
		return pieces

	# The chunk's images, as a map build bakes them: its ground at level 0 and below, a strip per
	# raised row, the edges between kinds of ground roughened, shadows baked in, and the trees
	# standing on it and round it.
	func _compose(key: Vector2i) -> Dictionary:
		if _shaded():
			return _compose_shaded(key)
		var origin := key * CHUNK
		var cells := Rect2i(origin, Vector2i(CHUNK, CHUNK)).intersection(Rect2i(Vector2i.ZERO, _size))
		var top_left := Vector2i(origin.x * int(CELL.x), origin.y * int(CELL.y))
		var ground := Image.create_empty(CHUNK * int(CELL.x), CHUNK * int(CELL.y) + LOW_PAD, false, Image.FORMAT_RGBA8)
		var strips := {}
		for y in range(cells.position.y, cells.end.y):
			for x in range(cells.position.x, cells.end.x):
				var level := _level(x, y)
				var drop := maxi(level - _level(x, y + 1), 0) * _px
				var image := ground
				var at := Vector2i(x * int(CELL.x), y * int(CELL.y) - level * _px) - top_left
				if level > 0:
					var strip_key := Vector2i(y, level)
					if not strips.has(strip_key):
						strips[strip_key] = Image.create_empty(CHUNK * int(CELL.x), int(CELL.y) + _strip_drop(y, level, cells),
								false, Image.FORMAT_RGBA8)
					image = strips[strip_key]
					at = Vector2i(x * int(CELL.x) - top_left.x, 0)
				var tile: String = look.tile_at(x, y)
				_blit_tile(image, "terrain", tile, x, y, at)
				if drop > 0:
					var face: String = "steps_face" if look.stairs_at(x, y) else look.face_at(x, y)
					var face_top := at + Vector2i(0, int(CELL.y))
					for row in range(0, drop, 8):
						_blit_tile(image, "faces", face, x, y + row, face_top + Vector2i(0, row))
					# The edge where the ground turns down into the face catches the light.
					image.fill_rect(Rect2i(face_top, Vector2i(int(CELL.x), 1)), ROCK_LIP if tile == "rock" else LIP)
					# Where the run of a face ends, its end turns away: shaded, its top corner cut.
					for side: int in [-1, 1]:
						var beside := _drop(x + side, y, level)
						if beside >= drop:
							continue
						var column := 0 if side < 0 else int(CELL.x) - 2
						for row in range(beside, drop):
							for dx in 2:
								_darken(image, face_top + Vector2i(column + dx, row))
						image.set_pixel(face_top.x + (0 if side < 0 else int(CELL.x) - 1), face_top.y + beside, Color(0, 0, 0, 0))
				if level > _level(x, y - 1):
					image.fill_rect(Rect2i(at, Vector2i(int(CELL.x), 1)), ROCK_LIP if tile == "rock" else LIP)
		if look.baked_shadows():
			_roughen(cells, top_left, ground, strips)
		var trees: Array = look.trees_in(cells.grow(4))
		_bake_shadows(key, cells, top_left, ground, strips, trees)
		return {"ground": ground, "strips": strips, "trees": trees}

	## Bakes every chunk's images once, as a map build would, so streaming a chunk in only
	## uploads them; `bake_usec` holds what each took. A look that bakes no structure shadows
	## (RegionLook) is composed as it streams instead.
	func bake_all() -> void:
		for cx in range((_size.x + CHUNK - 1) / CHUNK):
			for cy in range((_size.y + CHUNK - 1) / CHUNK):
				var started := Time.get_ticks_usec()
				_baked[Vector2i(cx, cy)] = _compose(Vector2i(cx, cy))
				bake_usec.append(Time.get_ticks_usec() - started)

	func _build_chunk(key: Vector2i) -> void:
		var started := Time.get_ticks_usec()
		var origin := key * CHUNK
		var cells := Rect2i(origin, Vector2i(CHUNK, CHUNK)).intersection(Rect2i(Vector2i.ZERO, _size))
		var top_left := Vector2i(origin.x * int(CELL.x), origin.y * int(CELL.y))
		var composed: Dictionary = _baked[key] if _baked.has(key) else _compose(key)
		var ground: Image = composed.ground
		var strips: Dictionary = composed.strips
		var trees: Array = composed.trees
		var occluders: Array[Sprite2D] = []
		var extras: Array[Sprite2D] = []
		var bytes := ground.get_data_size()
		var ground_sprite := Sprite2D.new()
		ground_sprite.centered = false
		ground_sprite.texture = ImageTexture.create_from_image(ground)
		ground_sprite.position = composed.get("ground_at", Vector2(top_left))
		_ground.add_child(ground_sprite)
		for strip_key: Vector2i in strips:
			# A strip is an image, or the shader's piece with where it stands on the screen.
			var piece: Variant = strips[strip_key]
			var image: Image = piece.image if piece is Dictionary else piece
			bytes += image.get_data_size()
			var strip := Sprite2D.new()
			strip.centered = false
			strip.texture = ImageTexture.create_from_image(image)
			# Sorted by its row's north edge, so the row's own people stand on it.
			strip.position = Vector2(top_left.x, strip_key.x * CELL.y - 0.5)
			strip.offset = Vector2(piece.at) - strip.position if piece is Dictionary else Vector2(0, 0.5 - strip_key.y * _px)
			_add_occluder(strip, "terrain", image.get_used_rect(), occluders)
		for tree: Dictionary in trees:
			if not cells.has_point(tree.cell):
				continue
			var sprite := Sprite2D.new()
			sprite.texture = _frame("trees", tree.kind, tree.variant)
			sprite.centered = false
			sprite.position = tree.foot
			sprite.offset = Vector2(-16, -36 - tree.level * _px)
			_add_occluder(sprite, "tree", _used(sprite.texture), occluders)
		for entry: Dictionary in _structures_in.get(key, []):
			var sprite := Sprite2D.new()
			var stages: Array[AtlasTexture] = []
			for stage: String in STAGES:
				stages.append(_entry_frame(entry, stage))
			sprite.set_meta("stages", stages)
			sprite.set_meta("entry", entry)
			sprite.set_meta("stage", _stage(entry))
			sprite.texture = stages[sprite.get_meta("stage")]
			sprite.centered = false
			var lift: int = _level(entry.corner.x, entry.corner.y + entry.size.y - 1) * _px
			var frame_size := sprite.texture.get_size()
			sprite.offset = Vector2(-8, -(frame_size.y - 8) - lift)
			sprite.position = Vector2(entry.corner.x * CELL.x, (entry.corner.y + entry.size.y) * CELL.y)
			var shadow := Sprite2D.new()
			shadow.centered = false
			shadow.texture = _stage_shadow(entry.size, sprite.get_meta("stage"))
			shadow.position = Vector2(entry.corner) * CELL - Vector2(0, lift)
			sprite.set_meta("shadow", shadow)
			extras.append(shadow)
			_shadows.add_child(shadow)
			if entry.kind in GROUND_KINDS:
				sprite.position += sprite.offset
				sprite.offset = Vector2.ZERO
				_ground.add_child(sprite)
				extras.append(sprite)
			else:
				_add_occluder(sprite, entry.kind, _used(sprite.texture), occluders)
		_chunks[key] = {"ground": ground_sprite, "extras": extras, "occluders": occluders, "bytes": bytes}
		ground_bytes += bytes
		built += 1
		build_usec.append(Time.get_ticks_usec() - started)

	# A structure's frame at `stage`: its sheet's tag for the stage, intact where the sheet has
	# none, or the one frame its entry names by `tag` and `variant`.
	func _entry_frame(entry: Dictionary, stage: String) -> AtlasTexture:
		if entry.has("tag"):
			return _frame(entry.kind, entry.tag, entry.variant)
		return _frame(entry.kind, stage if _sheet(entry.kind).tags.has(stage) else "intact", 0)

	# Where two kinds of ground meet on one level, each takes a ragged few pixels of the other,
	# by a seeded hash, so lanes, meadows and gardens have organic edges rather than cell edges.
	func _roughen(cells: Rect2i, top_left: Vector2i, ground: Image, strips: Dictionary) -> void:
		var sheet := _sheet("terrain")
		var band := 3
		for y in range(cells.position.y, cells.end.y):
			for x in range(cells.position.x, cells.end.x):
				var level := _level(x, y)
				var image: Image = ground if level <= 0 else strips.get(Vector2i(y, level))
				var at := Vector2i(x * int(CELL.x), y * int(CELL.y) - level * _px) - top_left
				if level > 0:
					at = Vector2i(x * int(CELL.x) - top_left.x, 0)
				for d: Vector2i in [Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(0, -1)]:
					var other := Vector2i(x, y) + d
					if not Rect2i(Vector2i.ZERO, _size).has_point(other) or _level(other.x, other.y) != level:
						continue
					var tag: String = look.tile_at(other.x, other.y)
					if tag == look.tile_at(x, y) or look.stairs_at(x, y) or look.stairs_at(other.x, other.y):
						continue
					var span: Array = sheet.tags[tag]
					var from: Rect2i = sheet.frames[span[0] + _hash(other.x, other.y, 1) % (span[1] - span[0] + 1)]
					for py in int(CELL.y):
						for px in int(CELL.x):
							var depth: int = [int(CELL.x) - 1 - px, int(CELL.y) - 1 - py, px, py][[Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(0, -1)].find(d)]
							if depth >= band:
								continue
							var wx := x * int(CELL.x) + px
							var wy := y * int(CELL.y) + py
							if float(_hash(wx, wy, 77) % 100) / 100.0 < float(band - depth) / (band + 1.0) * 0.8:
								image.set_pixel(at.x + px, at.y + py, sheet.image.get_pixel(from.position.x + px, from.position.y + py))

	# How far the cell at (x, y) drops to its south, if it stands on `level`; 0 off that level.
	func _drop(x: int, y: int, level: int) -> int:
		if _level(x, y) != level:
			return 0
		return maxi(level - _level(x, y + 1), 0) * _px

	# Darkens a pixel two steps along its ramp, if it is drawn.
	func _darken(image: Image, at: Vector2i) -> void:
		if not Rect2i(Vector2i.ZERO, image.get_size()).has_point(at):
			return
		var colour := image.get_pixel(at.x, at.y)
		if colour.a > 0.0:
			image.set_pixel(at.x, at.y, _shade.get(colour.to_rgba32(), colour))

	# The deepest face a strip of `level` in row `y` drops to its south.
	func _strip_drop(y: int, level: int, cells: Rect2i) -> int:
		var drop := 0
		for x in range(cells.position.x, cells.end.x):
			if _level(x, y) == level:
				drop = maxi(drop, (level - _level(x, y + 1)) * _px)
		return drop

	func _blit_tile(image: Image, sheet_name: String, tag: String, x: int, y: int, at: Vector2i) -> void:
		var sheet := _sheet(sheet_name)
		var span: Array = sheet.tags[tag]
		var frame: Rect2i = sheet.frames[span[0] + _hash(x, y, 1) % (span[1] - span[0] + 1)]
		image.blit_rect(sheet.image, frame, at)

	# Shadows cast to the lower right onto this chunk's ground: each structure's and tree's on the
	# level it stands on, and each raised cell's on the lower ground beside it. A pixel is
	# darkened two steps along its ramp once, however many shadows cover it.
	func _bake_shadows(key: Vector2i, cells: Rect2i, top_left: Vector2i, ground: Image, strips: Dictionary,
			trees: Array) -> void:
		var done := {}
		var area := Rect2i(top_left, Vector2i(cells.size) * Vector2i(CELL))
		var shade := func(wx: int, gy: int, highest: int, lowest: int) -> void:
			if not area.has_point(Vector2i(wx, gy)):
				return
			var cell := Vector2i(wx / int(CELL.x), gy / int(CELL.y))
			var level := _level(cell.x, cell.y)
			if level > highest or level < lowest:
				return
			var image := ground
			var at := Vector2i(wx, gy - level * _px) - top_left
			if level > 0:
				image = strips.get(Vector2i(cell.y, level))
				at = Vector2i(wx - top_left.x, gy - cell.y * int(CELL.y))
			var mark := Vector3i(at.x, at.y, level)
			if done.has(mark) or image == null:
				return
			done[mark] = true
			var colour := image.get_pixel(at.x, at.y)
			if colour.a > 0.0:
				image.set_pixel(at.x, at.y, _shade.get(colour.to_rgba32(), colour))
		for dx in range(-1, 2):
			for dy in range(-1, 2):
				for entry: Dictionary in _structures_in.get(key + Vector2i(dx, dy), []):
					if not look.baked_shadows() or entry.kind in GROUND_KINDS or _stage(entry) == 3:
						continue
					var level := _level(entry.corner.x, entry.corner.y + entry.size.y - 1)
					var rise: float = (_used(_entry_frame(entry, "intact")).size.y - entry.size.y * CELL.y) * (0.5 if _stage(entry) == 2 else 1.0)
					_sweep(Rect2(Vector2(entry.corner) * CELL, Vector2(entry.size) * CELL), rise, area,
							func(wx: int, gy: int) -> void: shade.call(wx, gy, level, level))
		for tree: Dictionary in trees:
			var foot: Vector2 = tree.foot
			var centre := foot + SHADOW * 26.0
			var level: int = tree.level
			for wx in range(int(centre.x) - 11, int(centre.x) + 12):
				for gy in range(int(centre.y) - 5, int(centre.y) + 6):
					if Vector2((wx - centre.x) / 11.0, (gy - centre.y) / 5.0).length() <= 1.0:
						shade.call(wx, gy, level, level)
		for y in range(cells.position.y - 3, cells.end.y):
			for x in range(cells.position.x - 3, cells.end.x):
				var level := _level(x, y)
				var below := mini(_level(x + 1, y), mini(_level(x, y + 1), _level(x + 1, y + 1)))
				if level <= below:
					continue
				_sweep(Rect2(Vector2(x, y) * CELL, CELL), (level - below) * _px, area,
						func(wx: int, gy: int) -> void: shade.call(wx, gy, level - 1, -99))
				# The foot of a face is in its shadow, all along it.
				if _level(x, y + 1) < level:
					for wx in range(x * int(CELL.x), (x + 1) * int(CELL.x)):
						for gy in range((y + 1) * int(CELL.y), (y + 1) * int(CELL.y) + 3):
							shade.call(wx, gy, level - 1, -99)

	# Calls `paint` for each ground pixel within `area` that `rect`, raised `rise` pixels, shades:
	# the rect swept along SHADOW times the rise.
	func _sweep(rect: Rect2, rise: float, area: Rect2i, paint: Callable) -> void:
		var v := SHADOW * rise
		var bound := Rect2i(rect.merge(Rect2(rect.position + v, rect.size))).intersection(area)
		for wx in range(bound.position.x, bound.end.x):
			for gy in range(bound.position.y, bound.end.y):
				var lo := 0.0
				var hi := 1.0
				if v.x > 0.0:
					lo = maxf(lo, (wx - rect.end.x) / v.x)
					hi = minf(hi, (wx - rect.position.x) / v.x)
				if v.y > 0.0:
					lo = maxf(lo, (gy - rect.end.y) / v.y)
					hi = minf(hi, (gy - rect.position.y) / v.y)
				if lo <= hi and not rect.has_point(Vector2(wx, gy)):
					paint.call(wx, gy)

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

	# Where a look's structures change stage as the core runs (RegionLook), a structure's shadow is
	# a sprite of its own, so a stage change swaps it without building the chunk again: a dithered
	# band past its right and bottom edges, shorter as it falls, none from rubble. A look that
	# bakes structure shadows (Valley) gets an empty one.
	func _stage_shadow(size: Vector2i, stage: int) -> ImageTexture:
		var key := "%d,%d,%d" % [size.x, size.y, stage]
		if not _shadow_cache.has(key):
			var reach: int = [6, 6, 3, 0][stage] if not look.baked_shadows() else 0
			var right := size.x * int(CELL.x)
			var bottom := size.y * int(CELL.y)
			var image := Image.create_empty(right + 6, bottom + 3, false, Image.FORMAT_RGBA8)
			for rect: Rect2i in [Rect2i(right, reach, reach, bottom - 2), Rect2i(reach, bottom, right, reach / 2)]:
				for x in range(rect.position.x, mini(rect.end.x, image.get_width())):
					for y in range(rect.position.y, mini(rect.end.y, image.get_height())):
						if reach > 0 and (x + y) % 2 == 0:
							image.set_pixel(x, y, Color("14101a"))
			_shadow_cache[key] = ImageTexture.create_from_image(image)
		return _shadow_cache[key]

	func _stage(entry: Dictionary) -> int:
		if _stage_of.is_valid():
			return _stage_of.call(entry)
		return look.stage(entry)

	# Draws the outline of `body` over every occluder sorted after it that its frame overlaps,
	# clipped to the occluder's pixels.
	func _outline(body: Sprite2D, key: String, army: int, person: int, used: Dictionary) -> void:
		var rect := Rect2(body.position + body.offset, BODY).grow(-2)
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
				occluder.add_child(made)
				pool.append(made)
			var outline: Sprite2D = pool[n]
			outline.texture = _frame(parts[0], parts[1] + "_xray", int(parts[2]))
			outline.flip_h = body.flip_h
			outline.position = body.position - occluder.position
			outline.offset = body.offset
			outline.visible = true
			occluder.clip_children = CanvasItem.CLIP_CHILDREN_AND_DRAW
			occluded += 1
			hidden_list.append([army, person, occluder.get_meta("kind"), body.position + body.offset + FEET,
					occluder.get_meta("solid").intersection(rect).size.y])

	func _note_front(body: Sprite2D, army: int, person: int) -> void:
		var rect := Rect2(body.position + body.offset, BODY).grow(-2)
		for occluder: Sprite2D in _near(rect):
			if occluder.position.y < body.position.y and not occluder.get_meta("kind") in ["tree", "terrain"] \
					and occluder.get_meta("solid").intersects(rect):
				front_list.append([army, person, occluder.get_meta("kind"), body.position + body.offset + FEET,
						occluder.get_meta("solid").intersection(rect).size.y])

	func _near(rect: Rect2) -> Array:
		var found := {}
		for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
			for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
				for occluder: Sprite2D in _buckets.get(Vector2i(bx, by), []):
					found[occluder] = true
		return found.keys()

	# Adds `sprite` to the sorted node as something that can hide a body, `used` being the
	# opaque part of its texture, and to `occluders`, its chunk's.
	func _add_occluder(sprite: Sprite2D, kind: String, used: Rect2i, occluders: Array[Sprite2D]) -> void:
		var drawn_at := sprite.position + sprite.offset
		sprite.set_meta("solid", Rect2(drawn_at + Vector2(used.position), used.size))
		sprite.set_meta("kind", kind)
		_standing.add_child(sprite)
		occluders.append(sprite)
		var rect := Rect2(drawn_at, sprite.texture.get_size())
		var keys: Array[Vector2i] = []
		for bx in range(floori(rect.position.x / BUCKET), floori(rect.end.x / BUCKET) + 1):
			for by in range(floori(rect.position.y / BUCKET), floori(rect.end.y / BUCKET) + 1):
				var key := Vector2i(bx, by)
				if not _buckets.has(key):
					_buckets[key] = []
				_buckets[key].append(sprite)
				keys.append(key)
		sprite.set_meta("buckets", keys)

	# The opaque part of an atlas frame, in the frame's pixels.
	func _used(texture: AtlasTexture) -> Rect2i:
		if not texture.has_meta("used"):
			var region := Rect2i(texture.region)
			var image: Image = texture.atlas.get_meta("image")
			texture.set_meta("used", image.get_region(region).get_used_rect())
		return texture.get_meta("used")

	func _hash(x: int, y: int, salt: int) -> int:
		var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
		h = (h ^ (h >> 13)) * 1274126177
		return absi(h ^ (h >> 16))

	func _has_sheet(name: String) -> bool:
		return _sheets.has(name) or FileAccess.file_exists(ART + name + ".json")

	func _sheet(name: String) -> Dictionary:
		if not _sheets.has(name):
			_sheets[name] = _load_from(ART + name)
		return _sheets[name]

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
		var texture := ImageTexture.create_from_image(image)
		texture.set_meta("image", image)
		return {"image": image, "texture": texture, "frames": frames, "tags": tags, "atlas": {}}

	func _frame(sheet: String, tag: String, index: int) -> AtlasTexture:
		return _frame_of(_sheet(sheet), tag, index)

	func _frame_of(sheet: Dictionary, tag: String, index: int) -> AtlasTexture:
		var key := "%s/%d" % [tag, index]
		var atlas: Dictionary = sheet.atlas
		if not atlas.has(key):
			var texture := AtlasTexture.new()
			texture.atlas = sheet.texture
			texture.region = Rect2(sheet.frames[sheet.tags[tag][0] + index])
			atlas[key] = texture
		return atlas[key]


## The look of a region the core generated: its terrain kinds (hills raised one level of 8
## pixels, plain where the kind has no tile at the angle), the villages' road and paving from
## world_art/villages.json, its fields in the state they are in when read, its structures, and a
## tree on about half the forest cells outside the villages.
class RegionLook extends RefCounted:
	const HILLS_PX := 8
	var world: WorldAdapter
	var _size := Vector2i.ZERO
	var _kinds := PackedStringArray()
	var _laid := {}
	var _cleared: Array[Rect2i] = []
	var _structures: Array[Dictionary] = []

	func _init(source: WorldAdapter) -> void:
		world = source
		var rows := world.map_rows()
		_size = Vector2i(rows[0].length(), rows.size())
		var names := world.terrain_names()
		var terrain := world.terrain()
		for i in _size.x * _size.y:
			_kinds.append(names[terrain[i]] if terrain.size() == _size.x * _size.y else
					("plain" if rows[i / _size.x][i % _size.x] == "." else "mountain"))
		var layouts: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://world_art/villages.json"))
		for village: Dictionary in world.villages():
			_cleared.append(Rect2i(village.corner, village.area).grow(1))
			for item: Dictionary in layouts.get(village.kind, []):
				if not item.has("tile"):
					continue
				var r: Array = item.rect
				for dx in int(r[2]):
					for dy in int(r[3]):
						_laid[Vector2i(village.corner) + Vector2i(int(r[0]) + dx, int(r[1]) + dy)] = item.tile
		for structure in world.structure_count():
			var corner := world.structure_cell(structure)
			var size := world.structure_footprint(structure)
			var state := world.structure_field_state(structure)
			if state >= 0:
				for dx in size.x:
					for dy in size.y:
						_laid[corner + Vector2i(dx, dy)] = ["sown", "growing", "ripe", "fallow", "burnt"][state]
				continue
			_structures.append({"kind": world.structure_kind(structure), "corner": corner, "size": size, "id": structure})

	func size() -> Vector2i:
		return _size

	func baked_shadows() -> bool:
		return false

	func level_px() -> int:
		return HILLS_PX

	func max_level() -> int:
		return 1

	func level_at(x: int, y: int) -> int:
		return 1 if _kind(x, y) == "hills" else 0

	func stairs_at(_x: int, _y: int) -> bool:
		return false

	func tile_at(x: int, y: int) -> String:
		var laid: String = _laid.get(Vector2i(x, y), "")
		if not laid.is_empty():
			return laid
		var kind := _kind(x, y)
		return kind if kind in ["hills", "forest"] else "plain"

	func face_at(_x: int, _y: int) -> String:
		return "hills_face"

	func structures() -> Array[Dictionary]:
		return _structures

	func stage(entry: Dictionary) -> int:
		return clampi(world.structure_stage(entry.id), 0, 3)

	func trees_in(cells: Rect2i) -> Array:
		var trees := []
		for y in range(maxi(cells.position.y, 0), mini(cells.end.y, _size.y)):
			for x in range(maxi(cells.position.x, 0), mini(cells.end.x, _size.x)):
				if _kind(x, y) != "forest" or _hash(x, y, 3) % 100 >= 55:
					continue
				if _cleared.any(func(rect: Rect2i) -> bool: return rect.has_point(Vector2i(x, y))):
					continue
				var pine := _hash(x, y, 4) % 3 == 0
				trees.append({"foot": Vector2(x * 16 + 4 + _hash(x, y, 6) % 9, y * 12 + 3 + _hash(x, y, 7) % 7),
						"kind": "pine" if pine else "tree", "variant": _hash(x, y, 5) % (2 if pine else 3),
						"cell": Vector2i(x, y), "level": level_at(x, y)})
		return trees

	func _kind(x: int, y: int) -> String:
		return _kinds[clampi(y, 0, _size.y - 1) * _size.x + clampi(x, 0, _size.x - 1)]

	func _hash(x: int, y: int, salt: int) -> int:
		var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
		h = (h ^ (h >> 13)) * 1274126177
		return absi(h ^ (h >> 16))


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


## A valley composed by hand for the revised look (PR #204's review), WIDTH x HEIGHT cells, in
## levels of LEVEL_PX: a range to the north in tiers up to level 5, capped by peaks; the main
## village on the valley floor, entered from the south through a gate and a guardian pavilion,
## lanes winding to a square with a well, its ancestral hall, a shrine up stairs on a terrace, an
## inn, stalls, tiled houses in the core and thatched ones at the edges; a walled market town to
## the west; a village of terraces climbing the hillside to the east; a riverside village on
## stilts along the river to the south. Every choice is seeded, so it is the same each run.
## It answers the calls AngleView reads a look with, and gives the core the cells people can
## walk (map_rows) and the cells each village's people walk between (lanes).
class Valley extends RefCounted:
	const WIDTH := 128
	const HEIGHT := 96
	const LEVEL_PX := 16
	const TOP := 5
	## The cells each still centres on.
	const PLACES := {"village": Vector2(50, 48), "town": Vector2(19, 48), "terraces": Vector2(100, 40),
			"riverside": Vector2(80, 70), "valley": Vector2(64, 24)}
	const VILLAGE := Rect2i(24, 32, 50, 32)
	const TOWN := Rect2i(4, 36, 25, 25)
	const TERRACES := Rect2i(86, 22, 36, 34)
	const RIVERSIDE := Rect2i(60, 60, 40, 18)
	const SQUARE := Rect2i(45, 42, 11, 7)

	var _level := PackedInt32Array()
	var _tile := PackedStringArray()
	var _face := PackedStringArray()
	var _stairs := PackedByteArray()
	var _taken := PackedByteArray()
	var _blocked := PackedByteArray()
	var _region := PackedInt32Array()
	var _built := PackedByteArray()
	var _slope := PackedByteArray()
	# Cells a bridge or pier carries people over, open though they lie on a cliff's edge.
	var _crossings := {}
	var _structures: Array[Dictionary] = []
	var _trees := {}
	var _lanes := {"village": [], "town": [], "terraces": [], "riverside": []}
	var _rng := RandomNumberGenerator.new()
	## The region laid out in place of the valley (#200's region screens), or "" for the valley.
	var region := ""
	var _places: Dictionary = PLACES

	func _init(name := "") -> void:
		region = name
		_rng.seed = 2026
		var cells := WIDTH * HEIGHT
		_level.resize(cells)
		_tile.resize(cells)
		_face.resize(cells)
		_stairs.resize(cells)
		_taken.resize(cells)
		_blocked.resize(cells)
		_built.resize(cells)
		_slope.resize(cells)
		match region:
			"central":
				_lay_central()
			"sichuan":
				_lay_sichuan()
			"west":
				_lay_west()
			"jiangnan":
				_lay_jiangnan()
			_:
				_lay_ground()
				_lay_range()
				_lay_river()
				_lay_terraces()
				_lay_town()
				_lay_village()
				_lay_riverside()
				_lay_woods()
		_block_cliffs()

	## The cells each still centres on.
	func places() -> Dictionary:
		return _places

	## The places the people are spread over, by their lanes, one army after another.
	func people_places() -> Array:
		return {"central": ["village", "village", "fields", "shaolin"], "sichuan": ["clan", "clan", "village", "terraces"],
				"west": ["village", "village", "pass", "fortress"], "jiangnan": ["town", "town", "town", "fields"]}.get(
				region, ["village", "village", "town", "terraces", "riverside"])

	## Kinds of ground the region recolours, as the view's RAMPS gives them.
	func ramps() -> Dictionary:
		return {
			"central": {"water": ["8c6d4a", "9c7048", "b08d60", "d1b787"], "plain": [78, 0.36, 0.44], "hills": [66, 0.38, 0.5]},
			"sichuan": {"plain": [110, 0.42, 0.4], "hills": [96, 0.38, 0.47], "forest": [140, 0.36, 0.25],
					"water": [190, 0.35, 0.4], "paddy": ["284536", "3b6138", "4f7a3f", "5fa3a8"]},
			"west": {"rock": [28, 0.14, 0.46], "earth": [34, 0.3, 0.52], "plain": [85, 0.4, 0.42], "hills": [70, 0.38, 0.5],
					"lane": [36, 0.32, 0.6], "water": [185, 0.4, 0.42], "forest": [95, 0.35, 0.3], "paving": [20, 0.08, 0.45]},
			"jiangnan": {"water": [170, 0.32, 0.42], "plain": [105, 0.42, 0.42], "paving": [210, 0.05, 0.62]},
		}.get(region, {})

	## The region's air, as the ground shader's uniforms: mist, moss on rock, slopes lit.
	func air() -> Dictionary:
		return {
			"central": {"relief": true, "flow": true},
			"sichuan": {"relief": true, "flow": true, "mist": Color(0.86, 0.89, 0.9, 0.32)},
			"west": {"relief": true, "moss": false},
			"jiangnan": {"relief": true, "flow": true},
		}.get(region, {})

	## The kind of a decorative tree on ground of `level`, and how many variants it has; none for
	## the valley, whose view picks its own.
	func tree_kind(level: int, rng: RandomNumberGenerator) -> Array:
		match region:
			"central":
				return ["pine", 2] if level >= 2 or rng.randf() < 0.3 else ["tree", 3]
			"sichuan":
				if level <= 1:
					return ["bamboo", 2] if rng.randf() < 0.6 else ["tree", 3]
				return ["pine", 2] if rng.randf() < 0.5 else ["tree", 3]
			"west":
				return ["snow_pine", 2] if level >= 2 else ["poplar", 2]
			"jiangnan":
				return ["willow", 2] if level == 0 and rng.randf() < 0.4 else ["tree", 3]
		return []

	func size() -> Vector2i:
		return Vector2i(WIDTH, HEIGHT)

	func baked_shadows() -> bool:
		return true

	func level_px() -> int:
		return LEVEL_PX

	func max_level() -> int:
		return TOP

	func level_at(x: int, y: int) -> int:
		return _level[_i(x, y)]

	func stairs_at(x: int, y: int) -> bool:
		return _stairs[_i(x, y)] == 1

	func tile_at(x: int, y: int) -> String:
		return _tile[_i(x, y)]

	func face_at(x: int, y: int) -> String:
		return _face[_i(x, y)]

	func structures() -> Array[Dictionary]:
		return _structures

	func stage(_entry: Dictionary) -> int:
		return 0

	## The cells as the ground shader reads them, one texel a cell: the ground kind's index in
	## `kinds` in red, the level plus one in green, in blue the material of the face the cell drops
	## by, an index in `faces`, or 3 for a stair, and in alpha half where its ground rises as a
	## slope and not by a cliff.
	func ground_cells(kinds: Array, faces: Array) -> Image:
		var image := Image.create_empty(WIDTH, HEIGHT, false, Image.FORMAT_RGBA8)
		for y in HEIGHT:
			for x in WIDTH:
				var kind := kinds.find(tile_at(x, y))
				if kind < 0:
					kind = 1
				var face := 3 if stairs_at(x, y) else maxi(faces.find(face_at(x, y)), 0)
				image.set_pixel(x, y, Color8(kind, level_at(x, y) + 1, face, 128 if _slope[_i(x, y)] == 1 else 255))
		return image

	## Whether a decorative tree may stand on the cell: open natural ground, nothing built on it
	## and nothing laid there.
	func open_at(x: int, y: int) -> bool:
		if not _inside(Vector2i(x, y)):
			return false
		var i := _i(x, y)
		return _blocked[i] == 0 and _built[i] == 0 and _tile[i] in ["plain", "hills", "forest", "rock"] \
				and not _trees.has(Vector2i(x, y))

	func trees_in(cells: Rect2i) -> Array:
		var found := []
		for y in range(maxi(cells.position.y, 0), mini(cells.end.y, HEIGHT)):
			for x in range(maxi(cells.position.x, 0), mini(cells.end.x, WIDTH)):
				if _trees.has(Vector2i(x, y)):
					found.append(_trees[Vector2i(x, y)])
		return found

	## The map the core walks people on: "." where they may stand.
	func map_rows() -> PackedStringArray:
		var rows := PackedStringArray()
		for y in HEIGHT:
			var row := ""
			for x in WIDTH:
				row += "#" if _blocked[_i(x, y)] == 1 else "."
			rows.append(row)
		return rows

	## The lane cells of `place` its people walk between: those the core lets them stand on, in
	## the one stretch of open ground holding most of them, so every one reaches every other.
	func lanes(place: String) -> Array[Vector2i]:
		if _region.is_empty():
			_find_regions()
		var count := {}
		for cell: Vector2i in _lanes[place]:
			var region: int = _region[_i(cell.x, cell.y)]
			if region >= 0:
				count[region] = count.get(region, 0) + 1
		var best := -1
		for region: int in count:
			if best < 0 or count[region] > count[best]:
				best = region
		var out: Array[Vector2i] = []
		if best < 0:
			return out
		for cell: Vector2i in _lanes[place]:
			if _region[_i(cell.x, cell.y)] == best and not out.has(cell):
				out.append(cell)
		return out

	## The cells people of `place` may start on, each once: its lanes first, then every other open
	## cell of the same stretch of ground within ten cells of them.
	func room(place: String) -> Array:
		var lanes := lanes(place)
		var cells: Array = lanes.duplicate()
		if lanes.is_empty():
			return cells
		var stretch: int = _region[_i(lanes[0].x, lanes[0].y)]
		var near := Rect2i(lanes[0], Vector2i.ONE)
		for cell: Vector2i in lanes:
			near = near.expand(cell)
		near = near.grow(10)
		var seen := {}
		for cell: Vector2i in lanes:
			seen[cell] = true
		for y in range(near.position.y, near.end.y):
			for x in range(near.position.x, near.end.x):
				var cell := Vector2i(x, y)
				if _inside(cell) and not seen.has(cell) and _blocked[_i(x, y)] == 0 and _region[_i(x, y)] == stretch:
					cells.append(cell)
		return cells

	# Numbers each stretch of open ground the core's people can cross, four ways.
	func _find_regions() -> void:
		_region.resize(WIDTH * HEIGHT)
		_region.fill(-1)
		var next := 0
		for start in WIDTH * HEIGHT:
			if _blocked[start] == 1 or _region[start] >= 0:
				continue
			var queue: Array[int] = [start]
			_region[start] = next
			while not queue.is_empty():
				var i: int = queue.pop_back()
				var x := i % WIDTH
				var y := i / WIDTH
				for d: Vector2i in [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]:
					var other := Vector2i(x, y) + d
					if not _inside(other):
						continue
					var j := _i(other.x, other.y)
					if _blocked[j] == 0 and _region[j] < 0:
						_region[j] = next
						queue.append(j)
			next += 1

	func _i(x: int, y: int) -> int:
		return clampi(y, 0, HEIGHT - 1) * WIDTH + clampi(x, 0, WIDTH - 1)

	func _inside(cell: Vector2i) -> bool:
		return cell.x >= 0 and cell.y >= 0 and cell.x < WIDTH and cell.y < HEIGHT

	func _noise(x: float, y: float, scale: float, salt: int) -> float:
		var fx := x / scale
		var fy := y / scale
		var x0 := floori(fx)
		var y0 := floori(fy)
		var tx := fx - x0
		var ty := fy - y0
		tx = tx * tx * (3.0 - 2.0 * tx)
		ty = ty * ty * (3.0 - 2.0 * ty)
		var a := _hash(x0, y0, salt)
		var b := _hash(x0 + 1, y0, salt)
		var c := _hash(x0, y0 + 1, salt)
		var d := _hash(x0 + 1, y0 + 1, salt)
		return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), ty)

	func _hash(x: int, y: int, salt: int) -> float:
		var h := (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)
		h = (h ^ (h >> 13)) * 1274126177
		return float(absi(h ^ (h >> 16)) % 100003) / 100003.0

	func _lay_tile(cell: Vector2i, tile: String) -> void:
		if _inside(cell):
			_tile[_i(cell.x, cell.y)] = tile

	# Grass in patches of two tones, meadow lighter than the plain.
	func _lay_ground() -> void:
		for y in HEIGHT:
			for x in WIDTH:
				_tile[_i(x, y)] = "hills" if _noise(x, y, 9.0, 1) > 0.62 else "plain"
				_face[_i(x, y)] = "bank_face"

	# The range: levels rising northward in tiers whose edges wander, smoothed so no tier is a
	# lone cell; grass and forest floor on the lower tops, rock above, rock faces from the second
	# level up, peaks on the top tier, and pines on the slopes.
	func _lay_range() -> void:
		var raw := PackedInt32Array()
		raw.resize(WIDTH * HEIGHT)
		for y in HEIGHT:
			for x in WIDTH:
				var rise := (38.0 - y) / 4.0 + (_noise(x, y, 12.0, 2) - 0.5) * 2.0 + (_noise(x, y, 30.0, 3) - 0.5) * 3.0
				# The village and the town lie on the valley's floor, the ground falling to them
				# over a few cells all round, so no level ends along a straight line.
				for floor: Array in [[Vector2(50, 46), Vector2(28, 17)], [Vector2(16, 48), Vector2(16, 16)]]:
					var reach: float = ((Vector2(x, y) - floor[0]) / floor[1]).length()
					rise -= clampf((1.35 - reach) * 6.0, 0.0, 99.0)
				# Fewer, taller steps: the range climbs by two levels at a time, then to the top.
				raw[_i(x, y)] = [0, 0, 2, 2, 4, 5][clampi(floori(rise), 0, TOP)]
		# Each cell takes the level most of its neighbours have, twice over.
		for pass_count in 2:
			var smooth := raw.duplicate()
			for y in range(1, HEIGHT - 1):
				for x in range(1, WIDTH - 1):
					var count := {}
					for dy in range(-1, 2):
						for dx in range(-1, 2):
							var l: int = raw[_i(x + dx, y + dy)]
							count[l] = count.get(l, 0) + 1
					var best: int = raw[_i(x, y)]
					for l: int in count:
						if count[l] > count.get(best, 0):
							best = l
					smooth[_i(x, y)] = best
			raw = smooth
		for y in HEIGHT:
			for x in WIDTH:
				var level: int = raw[_i(x, y)]
				if level <= 0:
					continue
				var i := _i(x, y)
				_level[i] = level
				# Here and there the mountainside rises as a slope, with no cliff.
				if level < TOP and _noise(x, y, 9.0, 41) > 0.68:
					_slope[i] = 1
				# Ground by noise, not by level, so its kinds meet in blobs and not along the levels'
				# edges: meadow and forest floor on the lower tiers, forest and rock higher up.
				var patch := _noise(x, y, 7.0, 4)
				var rock := (level - 2.5) / 2.5 + (patch - 0.5)
				_tile[i] = "rock" if rock > 0.45 else ("forest" if patch > 0.55 - level * 0.05 else ("hills" if patch > 0.3 else "plain"))
				_face[i] = "rock_face" if level >= 2 else "bank_face"
				_taken[i] = 1
		# Peaks stand on the front of the top tier, as far south as it reaches.
		var peaks := 0
		for x in range(4, WIDTH - 8, 11):
			for y in range(HEIGHT / 3, -1, -1):
				if level_at(x, y) == TOP and level_at(x + 6, y) == TOP and peaks < 12:
					_structures.append({"kind": "mountains", "tag": "peak", "variant": peaks % 3,
							"corner": Vector2i(x, y - 2), "size": Vector2i(7, 1), "id": -1})
					peaks += 1
					# Pines gather round its footing, so it rises out of the forest.
					for fx in range(x - 2, x + 9):
						for fy in range(y - 1, y + 2):
							var foot := Vector2i(fx, fy)
							if _inside(foot) and level_at(fx, fy) == TOP and _hash(fx, fy, 17) > 0.35:
								_taken[_i(fx, fy)] = 0
								_add_tree(foot, "pine", 2)
								_taken[_i(fx, fy)] = 1
					break
		# Forest clothes the slopes where the noise is dense; its trees are scattered as the
		# ground is drawn, not set on cells.
		for y in HEIGHT:
			for x in WIDTH:
				var level := level_at(x, y)
				if level >= 1 and level <= 4 and _noise(x, y, 6.0, 12) > 0.5 and not TERRACES.has_point(Vector2i(x, y)):
					_tile[_i(x, y)] = "forest"

	# The river: five cells wide, a level below the plain, winding west along the valley's foot;
	# its north bank drops into it in earth.
	func _lay_river() -> void:
		for x in WIDTH:
			var centre := 74.0 + sin(x / 14.0) * 4.0 + (_noise(x, 0, 20.0, 6) - 0.5) * 3.0
			for y in range(int(centre) - 2, int(centre) + 3):
				var i := _i(x, y)
				_level[i] = -1
				_tile[i] = "water"
				_taken[i] = 1
				_blocked[i] = 1

	# Terraces climbing the eastern hillside, a level each six rows, faced in stone, planted with
	# paddies and gardens, houses on their lips, and a stair winding up through them.
	func _lay_terraces() -> void:
		var r := TERRACES
		for y in range(r.position.y, r.end.y):
			for x in range(r.position.x, r.end.x):
				# The terraced hill's own edges wander, so its fields meet the meadow raggedly.
				if x < r.position.x + _noise(0, y, 6.0, 14) * 6.0 or x >= r.end.x - _noise(1, y, 6.0, 15) * 4.0 \
						or y < r.position.y + _noise(x, 0, 6.0, 16) * 4.0:
					continue
				var wobble := (_noise(x, 0, 11.0, 8) - 0.5) * 7.0
				var level := clampi(1 + floori((r.end.y - y + wobble) / 6.0), 1, 5)
				var i := _i(x, y)
				if _level[i] > level:
					continue
				_level[i] = level
				_face[i] = "stone_face"
				_tile[i] = "paddy" if level <= 2 else ("garden" if _noise(x, y, 5.0, 10) > 0.45 else "hills")
				_taken[i] = 0
		# The stair: up the middle, a cell to the side at each landing.
		var x := r.position.x + 16
		for y in range(r.end.y + 1, r.position.y, -1):
			var cell := Vector2i(x, y)
			if level_at(x, y) > level_at(x, y + 1):
				_stairs[_i(x, y)] = 1
				_tile[_i(x, y)] = "steps"
			else:
				_lay_tile(cell, "lane")
			_lanes.terraces.append(cell)
			_taken[_i(x, y)] = 1
			if y % 6 == 0:
				for k in range(-8, 9):
					var along := Vector2i(x + k, y - 1)
					if level_at(along.x, along.y) == level_at(x, y - 1) and not stairs_at(along.x, along.y):
						_lay_tile(along, "lane")
						_lanes.terraces.append(along)
						_taken[_i(along.x, along.y)] = 1
		# Houses along every terrace, thatched and small, each on a run of one level, a haystack
		# or crocks beside some.
		var k := 0
		for hx in range(r.position.x + 1, r.end.x - 3, 5):
			for y in range(r.position.y + 1, r.end.y - 2):
				var kind: String = ["thatched_house", "thatched_small", "tiled_house"][k % 3]
				var size := Vector2i(2, 2) if kind == "thatched_small" else Vector2i(3, 2)
				if level_at(hx, y - 1) > level_at(hx, y) and _place(kind, Vector2i(hx, y), size, false, true):
					_place(["haystack", "crocks", "firewood"][k % 3], Vector2i(hx + size.x, y + 1), Vector2i.ONE, false)
					k += 1
		for y in range(r.position.y, r.end.y):
			for xx in range(r.position.x, r.end.x):
				if _taken[_i(xx, y)] == 0 and _tile[_i(xx, y)] == "hills" and _hash(xx, y, 9) > 0.8:
					_add_tree(Vector2i(xx, y), "bamboo", 2)

	# The walled market town: a wall round it, its gate tower on the south, a street up from the
	# gate to an east-west market street lined with stalls, shops, an inn and watchtowers.
	func _lay_town() -> void:
		var r := TOWN
		for x in range(r.position.x, r.end.x):
			for y in range(r.position.y, r.end.y):
				_lay_tile(Vector2i(x, y), "paving" if (y == r.position.y + 11 or y == r.position.y + 12 or x == r.position.x + 12) else "lane")
		for x in range(r.position.x, r.end.x):
			_place("wall_h", Vector2i(x, r.position.y), Vector2i.ONE, false)
			if x < r.position.x + 10 or x > r.position.x + 13:
				_place("wall_h", Vector2i(x, r.end.y - 1), Vector2i.ONE, false)
		for y in range(r.position.y + 1, r.end.y - 1):
			_place("wall_v", Vector2i(r.position.x, y), Vector2i.ONE, false)
			if y < r.position.y + 10 or y > r.position.y + 13:
				_place("wall_v", Vector2i(r.end.x - 1, y), Vector2i.ONE, false)
		_place("town_gate", Vector2i(r.position.x + 10, r.end.y - 2), Vector2i(4, 2), false)
		_place("watchtower", Vector2i(r.position.x + 1, r.position.y + 1), Vector2i(2, 2), false)
		_place("watchtower", Vector2i(r.end.x - 3, r.position.y + 1), Vector2i(2, 2), false)
		var street := r.position.y + 11
		for x in range(r.position.x + 1, r.end.x + 6):
			_lanes.town.append(Vector2i(x, street + 1))
			if x >= r.end.x - 1:
				_lay_tile(Vector2i(x, street + 1), "lane")
				_lay_tile(Vector2i(x, street), "lane")
		for y in range(street, r.end.y + 2):
			_lanes.town.append(Vector2i(r.position.x + 12, y))
		_place("inn", Vector2i(r.position.x + 14, street - 4), Vector2i(4, 3), false)
		var shops := ["tiled_house_big", "tiled_house", "tiled_house", "tiled_house_big", "tiled_house"]
		var x := r.position.x + 2
		for kind: String in shops:
			var size := Vector2i(4, 3) if kind == "tiled_house_big" else Vector2i(3, 2)
			if x + size.x <= r.position.x + 12:
				_place(kind, Vector2i(x, street - size.y - 1), size, false)
			x += size.x + 1
		x = r.position.x + 2
		for kind: String in ["tiled_house", "tiled_house_big", "tiled_house"]:
			var size := Vector2i(4, 3) if kind == "tiled_house_big" else Vector2i(3, 2)
			_place(kind, Vector2i(x, street + 3), size, false)
			_place(kind, Vector2i(x + 13, street + 3), size, false)
			x += size.x + 1
		var stalls := ["stall_red", "stall_blue", "stall_amber"]
		for k in 6:
			_place(stalls[k % 3], Vector2i(r.position.x + 2 + k * 4, street - 1), Vector2i(2, 1), false)
			_place(stalls[(k + 1) % 3], Vector2i(r.position.x + 2 + k * 4, street + 2), Vector2i(2, 1), false)
		_place("cart", Vector2i(r.position.x + 14, street + 2), Vector2i(2, 1), false)
		for y in range(r.position.y + 2, r.end.y - 2, 3):
			_place("lantern", Vector2i(r.position.x + 11, y), Vector2i.ONE, false)

	# The main village: the gate and guardians at the south, the main lane up to the square and on
	# to the shrine's stairs, a cross lane winding east and west, lanes off them, and houses,
	# gardens and the things of daily life along them.
	func _lay_village() -> void:
		# The shrine's terrace, a level up at the village's head, and its stairs.
		for y in range(31, 36):
			for x in range(44, 57):
				_level[_i(x, y)] = 1
				_face[_i(x, y)] = "stone_face"
				_tile[_i(x, y)] = "paving"
		for y in range(28, 31):
			for x in range(42, 59):
				_level[_i(x, y)] = maxi(_level[_i(x, y)], 2)
		_stairs[_i(50, 35)] = 1
		_tile[_i(50, 35)] = "steps"
		_stairs[_i(51, 35)] = 1
		_tile[_i(51, 35)] = "steps"
		_place("shrine", Vector2i(49, 31), Vector2i(2, 2), false)
		_place("lantern", Vector2i(47, 33), Vector2i.ONE, false)
		_place("lantern", Vector2i(54, 33), Vector2i.ONE, false)
		for y in range(SQUARE.position.y, SQUARE.end.y):
			for x in range(SQUARE.position.x, SQUARE.end.x):
				_lay_tile(Vector2i(x, y), "paving")
		var main := [Vector2i(50, 66), Vector2i(51, 60), Vector2i(50, 54), Vector2i(50, 48), Vector2i(50, 41), Vector2i(50, 36)]
		var cross := [Vector2i(26, 52), Vector2i(32, 49), Vector2i(39, 47), Vector2i(45, 45), Vector2i(56, 45),
				Vector2i(62, 43), Vector2i(68, 45), Vector2i(76, 44)]
		var side := [[Vector2i(36, 48), Vector2i(35, 54), Vector2i(40, 59), Vector2i(46, 61)],
				[Vector2i(62, 44), Vector2i(64, 51), Vector2i(60, 57), Vector2i(54, 60)],
				[Vector2i(41, 46), Vector2i(41, 39), Vector2i(46, 36)],
				[Vector2i(60, 43), Vector2i(61, 37), Vector2i(55, 36)]]
		for lane: Array in [main, cross] + side:
			_lay_lane(lane, "village")
		_place("well_square", Vector2i(49, 44), Vector2i(2, 2), false)
		_place("ancestral_hall", Vector2i(47, 37), Vector2i(5, 3), false)
		_place("inn", Vector2i(56, 38), Vector2i(4, 3), false)
		_place("village_gate", Vector2i(49, 63), Vector2i(3, 1), false)
		_place("guardian", Vector2i(47, 64), Vector2i.ONE, false)
		_place("guardian", Vector2i(53, 64), Vector2i.ONE, false)
		_place("pavilion", Vector2i(54, 61), Vector2i(2, 2), false)
		for k in 3:
			_place(["stall_red", "stall_blue", "stall_amber"][k], Vector2i(46 + k * 3, 49), Vector2i(2, 1), false)
		_place("cart", Vector2i(56, 49), Vector2i(2, 1), false)
		_place("laundry", Vector2i(42, 49), Vector2i(2, 1), false)
		# Houses along every lane: tiled and larger toward the square, thatched and smaller
		# toward the edges, each with a garden behind it and something of daily life beside it.
		var centre := Vector2(SQUARE.get_center())
		for lane: Array in [main, cross] + side:
			for n in lane.size() - 1:
				var a: Vector2 = lane[n]
				var b: Vector2 = lane[n + 1]
				var steps := int(a.distance_to(b) / 4.0) + 1
				for t in steps:
					var at := a.lerp(b, float(t) / steps)
					var across := (b - a).normalized().orthogonal()
					for sign: int in [-1, 1]:
						var spot: Vector2 = at + across * sign * _rng.randf_range(2.5, 4.0)
						var far := spot.distance_to(centre) / 24.0
						var kind := _village_kind(far)
						var size := _size_of(kind)
						var corner := Vector2i(int(spot.x) - size.x / 2, int(spot.y) - size.y / 2)
						if _place(kind, corner, size, false, true):
							_dress(corner, size, far)
		# Brush fences and field-stone walls along parts of the edge.
		for x in range(VILLAGE.position.x + 2, VILLAGE.end.x - 2):
			if _hash(x, 0, 21) > 0.4:
				_place("brush_fence" if x < 50 else "stone_wall", Vector2i(x, VILLAGE.position.y + 1), Vector2i.ONE, false)

	func _village_kind(far: float) -> String:
		var roll := _rng.randf()
		if far < 0.6:
			return "tiled_house_big" if roll < 0.4 else "tiled_house"
		if far < 1.0:
			return "tiled_house" if roll < 0.5 else ("thatched_house" if roll < 0.85 else "tiled_house_big")
		return "thatched_house" if roll < 0.6 else "thatched_small"

	func _size_of(kind: String) -> Vector2i:
		return {"tiled_house_big": Vector2i(4, 3), "tiled_house": Vector2i(3, 2), "thatched_house": Vector2i(3, 2),
				"thatched_small": Vector2i(2, 2)}[kind]

	# A kitchen garden behind a house and things of daily life beside it.
	func _dress(corner: Vector2i, size: Vector2i, far: float) -> void:
		for x in range(corner.x, corner.x + size.x):
			for y in range(corner.y - 2, corner.y):
				var cell := Vector2i(x, y)
				if _inside(cell) and _taken[_i(x, y)] == 0 and level_at(x, y) == 0 and far > 0.5:
					_lay_tile(cell, "garden")
					_taken[_i(x, y)] = 1
		var props := ["crocks", "firewood", "haystack", "crocks", "laundry", "bush"]
		var kind: String = props[_rng.randi_range(0, props.size() - 1)]
		var at := corner + Vector2i(size.x if _rng.randf() < 0.5 else -1, size.y - 1)
		_place(kind, at, Vector2i(2, 1) if kind == "laundry" else Vector2i.ONE, false)
		if _rng.randf() < 0.5:
			_add_tree(corner + Vector2i(-1 if _rng.randf() < 0.5 else size.x, -1), "tree", 3)

	# The riverside village: stilt houses standing in the water off the north bank, piers and
	# boats between them, willows and nets along the bank, a lane behind.
	func _lay_riverside() -> void:
		var r := RIVERSIDE
		for x in range(r.position.x, r.end.x):
			var bank := 0
			for y in range(r.position.y, r.end.y):
				if level_at(x, y) < 0:
					bank = y
					break
			if bank == 0:
				continue
			_lay_tile(Vector2i(x, bank - 2), "lane")
			_lanes.riverside.append(Vector2i(x, bank - 2))
			if x % 6 == 0 and x + 3 < r.end.x:
				_place("stilt_house", Vector2i(x, bank), Vector2i(3, 2), true)
			elif x % 6 == 4:
				for y in range(bank, bank + 3):
					_place("pier", Vector2i(x, y), Vector2i.ONE, true)
				_place("boat", Vector2i(x + 1, bank + 3), Vector2i(2, 1), true)
			if x % 5 == 2:
				_add_tree(Vector2i(x, bank - 1), "willow", 2)
			if x % 9 == 1:
				_place(["laundry", "crocks", "firewood"][x % 3], Vector2i(x, bank - 4), Vector2i(2, 1) if x % 3 == 0 else Vector2i.ONE, false)

	# Woods round the valley: broadleaf and pine on the floor and the lower slopes where nothing
	# is built, in clumps by noise, and lone trees and bushes in the open.
	func _lay_woods() -> void:
		for y in HEIGHT:
			for x in WIDTH:
				var cell := Vector2i(x, y)
				if _taken[_i(x, y)] == 1 or level_at(x, y) < 0 or level_at(x, y) > 2:
					continue
				if VILLAGE.has_point(cell) or TOWN.grow(1).has_point(cell) or TERRACES.has_point(cell):
					continue
				var wood := _noise(x, y, 8.0, 31)
				if wood > 0.6:
					_tile[_i(x, y)] = "forest"
				elif _hash(x, y, 34) > 0.985:
					_place("bush" if _hash(x, y, 35) > 0.4 else "boulder", cell, Vector2i.ONE, false)

	# A cliff's top cell blocks the core's people unless a stair leads down from it, so nobody
	# walks off a level; a stair's foot and head stay open.
	func _block_cliffs() -> void:
		for y in HEIGHT:
			for x in WIDTH:
				var level := level_at(x, y)
				if stairs_at(x, y) or _crossings.has(Vector2i(x, y)):
					continue
				for d: Vector2i in [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]:
					var other := Vector2i(x, y) + d
					if not _inside(other):
						continue
					var down := level_at(other.x, other.y) != level
					var by_stair := stairs_at(other.x, other.y) and d.y != 0
					if down and not by_stair:
						_blocked[_i(x, y)] = 1

	func _lay_lane(points: Array, place: String) -> void:
		for n in points.size() - 1:
			var a: Vector2 = points[n]
			var b: Vector2 = points[n + 1]
			var steps := int(a.distance_to(b) * 2.0) + 1
			for t in steps + 1:
				var at := a.lerp(b, float(t) / steps)
				var wobble := Vector2((_noise(at.x, at.y, 3.0, 41) - 0.5) * 1.2, 0)
				var cell := Vector2i((at + wobble).floor())
				for c: Vector2i in [cell, cell + Vector2i(1, 0)]:
					if _inside(c) and level_at(c.x, c.y) >= 0 and (region != "" or not SQUARE.has_point(c)) \
							and not stairs_at(c.x, c.y):
						_lay_tile(c, "lane")
						_taken[_i(c.x, c.y)] = 1
				_lanes[place].append(cell)

	# Places a structure on a free run of one level, every cell of it untaken, or none at all;
	# over water when `wet`; with `apart`, a cell clear of any other building all round. Returns
	# whether it was placed.
	func _place(kind: String, corner: Vector2i, size: Vector2i, wet: bool, apart := false) -> bool:
		if kind == "arch_bridge":
			return _bridge(corner, size)
		var level := level_at(corner.x, corner.y + size.y - 1)
		if apart:
			for x in range(corner.x - 1, corner.x + size.x + 1):
				for y in range(corner.y - 1, corner.y + size.y + 1):
					if _inside(Vector2i(x, y)) and _built[_i(x, y)] == 1:
						return false
		for x in range(corner.x, corner.x + size.x):
			for y in range(corner.y, corner.y + size.y):
				var cell := Vector2i(x, y)
				if not _inside(cell) or _taken[_i(x, y)] == 1 and not wet or level_at(x, y) != level:
					return false
				if (level < 0) != wet and kind != "stilt_house":
					return false
		for x in range(corner.x, corner.x + size.x):
			for y in range(corner.y, corner.y + size.y):
				_taken[_i(x, y)] = 1
				if size.x > 1 or size.y > 1:
					_built[_i(x, y)] = 1
					# A building stands on dry ground, not in a paddy or a garden.
					if _tile[_i(x, y)] in ["paddy", "garden"]:
						_tile[_i(x, y)] = "plain"
				if kind != "pier":
					_blocked[_i(x, y)] = 1
				else:
					_blocked[_i(x, y)] = 0
		if region != "" and kind == "pier":
			_crossings[corner] = true
		# A region's gates are walked through, under their arch.
		if region != "" and kind == "town_gate":
			for y in range(corner.y, corner.y + size.y):
				_blocked[_i(corner.x + 1, y)] = 0
				_blocked[_i(corner.x + 2, y)] = 0
		_structures.append({"kind": kind, "corner": corner, "size": size, "id": _structures.size()})
		return true

	# A bridge spans a canal from bank to bank, so its cells are of two levels; people cross it.
	func _bridge(corner: Vector2i, size: Vector2i) -> bool:
		for x in range(corner.x, corner.x + size.x):
			for y in range(corner.y, corner.y + size.y):
				if not _inside(Vector2i(x, y)) or _built[_i(x, y)] == 1:
					return false
		for x in range(corner.x, corner.x + size.x):
			for y in range(corner.y, corner.y + size.y):
				_taken[_i(x, y)] = 1
				_blocked[_i(x, y)] = 0
				_crossings[Vector2i(x, y)] = true
		_structures.append({"kind": "arch_bridge", "corner": corner, "size": size, "id": _structures.size()})
		return true

	func _add_tree(cell: Vector2i, kind: String, variants: int) -> void:
		if not _inside(cell) or _taken[_i(cell.x, cell.y)] == 1 or _trees.has(cell) or level_at(cell.x, cell.y) < 0:
			return
		_trees[cell] = {"foot": Vector2(cell.x * 16 + 4 + int(_hash(cell.x, cell.y, 6) * 9),
				cell.y * 12 + 3 + int(_hash(cell.x, cell.y, 7) * 7)), "kind": kind,
				"variant": int(_hash(cell.x, cell.y, 5) * variants) % variants, "cell": cell, "level": level_at(cell.x, cell.y)}

	# -- The regions' screens (#200): each a region of the map plan laid out by hand on the
	# valley's grid, with the valley's means.

	# Levels from `rise`, a height in steps at each cell, taken through `steps`, then each cell
	# the level most of its neighbours have, twice over. Raised cells are taken, faced in rock from
	# the second level up and in earth below, and where the noise passes `slopes` rise as a slope.
	func _raise(rise: Callable, steps: Array, slopes: float) -> void:
		var raw := PackedInt32Array()
		raw.resize(WIDTH * HEIGHT)
		for y in HEIGHT:
			for x in WIDTH:
				raw[_i(x, y)] = steps[clampi(floori(rise.call(x, y)), 0, steps.size() - 1)]
		for pass_count in 2:
			var smooth := raw.duplicate()
			for y in range(1, HEIGHT - 1):
				for x in range(1, WIDTH - 1):
					var count := {}
					for dy in range(-1, 2):
						for dx in range(-1, 2):
							var l: int = raw[_i(x + dx, y + dy)]
							count[l] = count.get(l, 0) + 1
					var best: int = raw[_i(x, y)]
					for l: int in count:
						if count[l] > count.get(best, 0):
							best = l
					smooth[_i(x, y)] = best
			raw = smooth
		for y in HEIGHT:
			for x in WIDTH:
				var level: int = raw[_i(x, y)]
				if level <= _level[_i(x, y)]:
					continue
				var i := _i(x, y)
				_level[i] = level
				_face[i] = "rock_face" if level >= 2 else "bank_face"
				_taken[i] = 1
				if level < TOP and _noise(x, y, 9.0, 41) > slopes:
					_slope[i] = 1

	# Ground of one level over `r`, faced in `face`, open to building.
	func _flat(r: Rect2i, level: int, face: String, tile: String) -> void:
		for y in range(r.position.y, r.end.y):
			for x in range(r.position.x, r.end.x):
				var i := _i(x, y)
				_level[i] = level
				_face[i] = face
				_tile[i] = tile
				_taken[i] = 0
				_slope[i] = 0

	func _water(x: int, y: int) -> void:
		if not _inside(Vector2i(x, y)):
			return
		var i := _i(x, y)
		_level[i] = -1
		_tile[i] = "water"
		_taken[i] = 1
		_blocked[i] = 1
		_slope[i] = 0

	# A stair down from the cell at (x, y) to the one below it.
	func _stair(x: int, y: int) -> void:
		_stairs[_i(x, y)] = 1
		_tile[_i(x, y)] = "steps"
		_taken[_i(x, y)] = 1

	# Fields between balks, warped so no balk is ruled: each field a crop drawn from `crops`, a
	# list of [tile, share], on the open level ground of `r`.
	func _fields(r: Rect2i, crops: Array, salt: int) -> void:
		for y in range(r.position.y, r.end.y):
			for x in range(r.position.x, r.end.x):
				var i := _i(x, y)
				if _taken[i] == 1 or _level[i] != 0:
					continue
				var fx := x + (_noise(x, y, 10.0, salt) - 0.5) * 4.0
				var fy := y + (_noise(x, y, 10.0, salt + 1) - 0.5) * 3.0
				if fposmod(fx, 7.0) < 0.9 or fposmod(fy, 5.0) < 0.8:
					_tile[i] = "plain"
					continue
				var roll := _hash(floori(fx / 7.0), floori(fy / 5.0), salt + 2)
				for crop: Array in crops:
					roll -= crop[1]
					if roll < 0.0:
						_tile[i] = crop[0]
						break

	# Peaks of the sheet's `tag` on the front of the top tier, and trees of `kind` round each foot.
	func _crown(tag: String, kind: String, x_from: int, x_to: int) -> void:
		var peaks := 0
		for x in range(x_from, x_to - 7, 11):
			for y in range(HEIGHT / 2, -1, -1):
				if level_at(x, y) == TOP and level_at(x + 6, y) == TOP:
					_structures.append({"kind": "mountains", "tag": tag, "variant": peaks % 3,
							"corner": Vector2i(x, y - 2), "size": Vector2i(7, 1), "id": -1})
					peaks += 1
					for fx in range(x - 2, x + 9):
						for fy in range(y - 1, y + 2):
							if _inside(Vector2i(fx, fy)) and level_at(fx, fy) == TOP and _hash(fx, fy, 17) > 0.35:
								_taken[_i(fx, fy)] = 0
								_add_tree(Vector2i(fx, fy), kind, 2)
								_taken[_i(fx, fy)] = 1
					break

	# Houses along `lane`'s points on both sides, by `kinds`, dressed as the valley's are.
	func _line_houses(lane: Array, kinds: Array, gap: float, dress: bool) -> void:
		for n in lane.size() - 1:
			var a: Vector2 = lane[n]
			var b: Vector2 = lane[n + 1]
			var steps := int(a.distance_to(b) / gap) + 1
			for t in steps:
				var at := a.lerp(b, float(t) / steps)
				var across := (b - a).normalized().orthogonal()
				for sign: int in [-1, 1]:
					var spot: Vector2 = at + across * sign * _rng.randf_range(2.5, 3.5)
					var kind: String = kinds[_rng.randi_range(0, kinds.size() - 1)]
					var size: Vector2i = {"tiled_house_big": Vector2i(4, 3), "water_house_big": Vector2i(4, 3),
							"thatched_small": Vector2i(2, 2), "adobe_small": Vector2i(2, 2)}.get(kind, Vector2i(3, 2))
					var corner := Vector2i(int(spot.x) - size.x / 2, int(spot.y) - size.y / 2)
					if _place(kind, corner, size, false, true) and dress:
						_dress(corner, size, 1.0)

	# A walled compound over `r`: walls round it, a gate of `gate`'s width in the south wall at
	# `gate_x`, a tower in each front corner.
	func _walls(r: Rect2i, gate_x: int) -> void:
		for x in range(r.position.x, r.end.x):
			_place("wall_h", Vector2i(x, r.position.y), Vector2i.ONE, false)
			if x < gate_x or x > gate_x + 3:
				_place("wall_h", Vector2i(x, r.end.y - 1), Vector2i.ONE, false)
		for y in range(r.position.y + 1, r.end.y - 1):
			_place("wall_v", Vector2i(r.position.x, y), Vector2i.ONE, false)
			_place("wall_v", Vector2i(r.end.x - 1, y), Vector2i.ONE, false)
		_place("town_gate", Vector2i(gate_x, r.end.y - 2), Vector2i(4, 2), false)
		_place("watchtower", Vector2i(r.position.x + 1, r.end.y - 4), Vector2i(2, 2), false)
		_place("watchtower", Vector2i(r.end.x - 3, r.end.y - 4), Vector2i(2, 2), false)

	func _lane_cells(cells: Array, place: String) -> void:
		for cell: Vector2i in cells:
			if _inside(cell) and _blocked[_i(cell.x, cell.y)] == 0:
				_lanes[place].append(cell)

	# 중원, the Central Plains: wheat to the horizon between balks, the Yellow River across the
	# north with silt beaches and willows, Luoyang's south wall and gate with its streets behind,
	# a farming hamlet on the road east, and Song Shan standing alone with Shaolin's terrace,
	# halls and pagodas on its south face, and a forest of pagodas at its foot.
	func _lay_central() -> void:
		_lanes = {"village": [], "fields": [], "shaolin": []}
		_places = {"central": Vector2(58, 39), "central_village": Vector2(44, 52), "village": Vector2(44, 52)}
		for y in HEIGHT:
			for x in WIDTH:
				_tile[_i(x, y)] = "plain"
				_face[_i(x, y)] = "bank_face"
		var shan := Vector2(80, 27)
		var rise := func(x: int, y: int) -> float:
			var d := (Vector2(x, y) - shan) / Vector2(16.0, 11.0)
			return (1.25 - d.length()) * 6.0 + (_noise(x, y, 8.0, 54) - 0.5) * 2.5
		_raise(rise, [0, 0, 2, 2, 4, 5], 0.62)
		for y in HEIGHT:
			for x in WIDTH:
				var level := level_at(x, y)
				if level > 0:
					var patch := _noise(x, y, 7.0, 4)
					_tile[_i(x, y)] = "rock" if level >= 4 and patch > 0.3 else ("forest" if patch > 0.38 else "hills")
		_crown("peak", "pine", 66, 96)
		_fields(Rect2i(0, 0, WIDTH, HEIGHT), [["wheat", 0.62], ["hills", 0.18], ["garden", 0.2]], 51)
		# Shaolin's terrace, two levels up the mountain's face, its stair down the middle.
		_flat(Rect2i(70, 37, 16, 7), 2, "stone_face", "paving")
		_flat(Rect2i(64, 44, 26, 4), 0, "bank_face", "plain")
		_stair(77, 43)
		_stair(78, 43)
		_place("ancestral_hall", Vector2i(75, 37), Vector2i(5, 3), false)
		_place("pagoda", Vector2i(71, 38), Vector2i(2, 2), false)
		_place("pagoda", Vector2i(83, 38), Vector2i(2, 2), false)
		_place("shrine", Vector2i(72, 41), Vector2i(2, 2), false)
		_place("pavilion", Vector2i(82, 41), Vector2i(2, 2), false)
		_place("lantern", Vector2i(75, 41), Vector2i.ONE, false)
		_place("lantern", Vector2i(80, 41), Vector2i.ONE, false)
		for x in range(70, 86):
			if x < 76 or x > 79:
				_place("stone_wall", Vector2i(x, 43), Vector2i.ONE, false)
		_place("village_gate", Vector2i(76, 46), Vector2i(3, 1), false)
		_place("guardian", Vector2i(75, 46), Vector2i.ONE, false)
		_place("guardian", Vector2i(79, 46), Vector2i.ONE, false)
		# The forest of pagodas, the monks' graves, at the foot.
		for spot: Vector2i in [Vector2i(66, 45), Vector2i(69, 46), Vector2i(85, 45)]:
			_place("pagoda", spot, Vector2i(2, 2), false)
		var shaolin := []
		for y in range(40, 50):
			shaolin.append(Vector2i(77, y))
			shaolin.append(Vector2i(78, y))
		for x in range(72, 84):
			shaolin.append(Vector2i(x, 40))
		_lane_cells(shaolin, "shaolin")
		# The Yellow River, bending north under the mountain, silt beaches on its south bank.
		for x in WIDTH:
			var centre := 25.0 + sin(x / 16.0) * 2.5 + (_noise(x, 0, 20.0, 56) - 0.5) * 3.0 - maxf(0.0, x - 56.0) * 0.4
			for y in range(int(centre) - 3, int(centre) + 4):
				_water(x, y)
			for y in range(int(centre) + 4, int(centre) + 6):
				if _inside(Vector2i(x, y)) and level_at(x, y) == 0 and _noise(x, y, 4.0, 57) > 0.4:
					_tile[_i(x, y)] = "sand"
			if _hash(x, 0, 58) > 0.72:
				_add_tree(Vector2i(x, int(centre) + 6), "willow", 2)
		# Luoyang: the south wall and its gate, the main street up from it, a cross street, and
		# the city's houses, its yamen, an inn and stalls.
		var city := Rect2i(24, 44, 36, 15)
		for y in range(city.position.y, city.end.y):
			for x in range(city.position.x, city.end.x):
				_tile[_i(x, y)] = "paving" if x == 41 or x == 42 or y == 50 or y == 51 else "lane"
		_walls(city, 40)
		_place("ancestral_hall", Vector2i(44, 45), Vector2i(5, 3), false)
		_place("inn", Vector2i(44, 52), Vector2i(4, 3), false)
		for k in 3:
			_place(["stall_red", "stall_blue", "stall_amber"][k], Vector2i(35 + k * 2, 52), Vector2i(2, 1), false)
		for y in range(46, 58, 4):
			_place("lantern", Vector2i(40, y), Vector2i.ONE, false)
			_place("lantern", Vector2i(43, y), Vector2i.ONE, false)
		for by in [46, 53]:
			for bx in range(26, 58, 4):
				var kind: String = "tiled_house_big" if _hash(bx, by, 59) > 0.6 else "tiled_house"
				var size := Vector2i(4, 3) if kind == "tiled_house_big" else Vector2i(3, 2)
				_place(kind, Vector2i(bx, by), size, false, true)
		var street := []
		for y in range(45, 66):
			street.append(Vector2i(41, y))
			street.append(Vector2i(42, y))
		for x in range(26, 58):
			street.append(Vector2i(x, 50))
		_lane_cells(street, "village")
		_lay_lane([Vector2(41, 58), Vector2(41, 70), Vector2(39, 95)], "village")
		_lay_lane([Vector2(43, 60), Vector2(58, 61), Vector2(66, 55), Vector2(72, 50), Vector2(77, 47)], "fields")
		# The hamlet on the road east: farmhouses with their gardens, a threshing floor, stacks.
		_flat(Rect2i(66, 61, 4, 3), 0, "bank_face", "paving")
		_line_houses([Vector2(58, 63), Vector2(66, 57), Vector2(76, 56), Vector2(86, 52)], ["thatched_house", "thatched_house",
				"thatched_small", "tiled_house"], 4.0, true)
		for spot: Vector2i in [Vector2i(66, 60), Vector2i(70, 64), Vector2i(64, 64)]:
			_place("haystack", spot, Vector2i.ONE, false)
		_place("cart", Vector2i(70, 61), Vector2i(2, 1), false)
		for y in HEIGHT:
			for x in WIDTH:
				if _taken[_i(x, y)] == 0 and level_at(x, y) == 0 and not city.grow(2).has_point(Vector2i(x, y)) \
						and _noise(x, y, 9.0, 55) > 0.74:
					_tile[_i(x, y)] = "forest"

	# 사천, Sichuan: a misty basin ringed by peaks, terraced paddies climbing its north side, a
	# gorge cut through the eastern range with its river running out across the basin, bamboo
	# everywhere, the Tang clan's walled compound on the floor, and a village by the river.
	func _lay_sichuan() -> void:
		_lanes = {"village": [], "clan": [], "terraces": []}
		_places = {"sichuan": Vector2(56, 37), "sichuan_village": Vector2(53, 55), "village": Vector2(53, 55)}
		for y in HEIGHT:
			for x in WIDTH:
				_tile[_i(x, y)] = "hills" if _noise(x, y, 9.0, 1) > 0.6 else "plain"
				_face[_i(x, y)] = "bank_face"
		var rise := func(x: int, y: int) -> float:
			var height := (26.0 - y) / 3.0 + (_noise(x, y, 12.0, 2) - 0.5) * 2.0 + (_noise(x, y, 30.0, 3) - 0.5) * 3.0
			height += maxf(0.0, x - 92.0) / 4.0
			# A ridge south of the gorge, so the river runs between two walls.
			if x > 72.0 + _noise(0, y, 4.0, 79) * 6.0 and y >= 35 and y <= 44:
				height = maxf(height, 4.2 + _noise(x, y, 6.0, 60))
			return height
		_raise(rise, [0, 0, 2, 2, 4, 5], 0.64)
		# The ridge falls to the basin by cliffs, too tall to rise as a slope.
		for y in range(33, 47):
			for x in range(72, WIDTH):
				_slope[_i(x, y)] = 0
		for y in HEIGHT:
			for x in WIDTH:
				var level := level_at(x, y)
				if level > 0:
					var patch := _noise(x, y, 7.0, 4)
					_tile[_i(x, y)] = "rock" if level >= 4 and patch > 0.45 else ("forest" if patch > 0.35 else "hills")
		_crown("peak", "pine", 4, 124)
		# The terraces: a level each five rows up the north hillside, earth-banked paddies.
		var terraces := Rect2i(14, 22, 40, 22)
		for y in range(terraces.position.y, terraces.end.y):
			for x in range(terraces.position.x, terraces.end.x):
				if x < terraces.position.x + _noise(0, y, 6.0, 14) * 5.0 or x >= terraces.end.x - _noise(1, y, 6.0, 15) * 5.0:
					continue
				var wobble := (_noise(x, 0, 11.0, 8) - 0.5) * 7.0
				var level := clampi(1 + floori((terraces.end.y - y + wobble) / 5.0), 1, 4)
				var i := _i(x, y)
				if _level[i] > level:
					continue
				_level[i] = level
				_face[i] = "bank_face"
				_tile[i] = "paddy" if _noise(x, y, 5.0, 10) > 0.3 else "garden"
				_taken[i] = 0
				_slope[i] = 0
		var climb := []
		var sx := 33
		for y in range(terraces.end.y + 1, terraces.position.y, -1):
			if level_at(sx, y) > level_at(sx, y + 1):
				_stair(sx, y)
			else:
				_lay_tile(Vector2i(sx, y), "lane")
				_taken[_i(sx, y)] = 1
			climb.append(Vector2i(sx, y))
		_lane_cells(climb, "terraces")
		var k := 0
		for hx in range(terraces.position.x + 2, terraces.end.x - 3, 6):
			for y in range(terraces.position.y + 1, terraces.end.y - 2):
				var kind: String = ["thatched_house", "thatched_small", "tiled_house"][k % 3]
				var size := Vector2i(2, 2) if kind == "thatched_small" else Vector2i(3, 2)
				if level_at(hx, y - 1) > level_at(hx, y) and _place(kind, Vector2i(hx, y), size, false, true):
					k += 1
		# The gorge's river, out of the east between its walls and south across the basin.
		for x in range(64, WIDTH):
			var centre := 39.0 + sin(x / 9.0) * 1.2
			for y in range(int(centre) - 1, int(centre) + 2):
				_water(x, y)
		for y in range(38, HEIGHT):
			var centre := 66.0 + sin(y / 8.0) * 2.5
			for x in range(int(centre) - 1, int(centre) + 2):
				_water(x, y)
		_fields(Rect2i(0, 46, WIDTH, HEIGHT - 46), [["paddy", 0.6], ["hills", 0.15], ["garden", 0.25]], 61)
		# The Tang clan: a walled compound of halls on the basin floor, crocks of its poisons
		# standing about, bamboo crowding its walls.
		var clan := Rect2i(42, 48, 22, 15)
		_flat(clan, 0, "bank_face", "lane")
		_flat(Rect2i(42, 63, 22, 3), 0, "bank_face", "plain")
		_walls(clan, 51)
		_place("ancestral_hall", Vector2i(50, 50), Vector2i(5, 3), false)
		_place("tiled_house_big", Vector2i(44, 51), Vector2i(4, 3), false)
		_place("tiled_house_big", Vector2i(57, 51), Vector2i(4, 3), false)
		for x in [44, 48, 56, 60]:
			_place("tiled_house", Vector2i(x, 56), Vector2i(3, 2), false)
		for spot: Vector2i in [Vector2i(49, 59), Vector2i(55, 59), Vector2i(48, 54), Vector2i(56, 54)]:
			_place("crocks", spot, Vector2i.ONE, false)
		_place("lantern", Vector2i(51, 54), Vector2i.ONE, false)
		_place("lantern", Vector2i(54, 54), Vector2i.ONE, false)
		for y in range(53, 61):
			_lay_tile(Vector2i(52, y), "paving")
			_lay_tile(Vector2i(53, y), "paving")
		var court := []
		for y in range(54, 66):
			court.append(Vector2i(52, y))
			court.append(Vector2i(53, y))
		for x in range(44, 62):
			court.append(Vector2i(x, 55))
		_lane_cells(court, "clan")
		# The village by the river: houses along a lane between paddies.
		var lane := [Vector2(70, 48), Vector2(73, 55), Vector2(79, 59), Vector2(88, 57), Vector2(96, 60)]
		_lay_lane(lane, "village")
		_line_houses(lane, ["thatched_house", "thatched_house", "thatched_small", "tiled_house"], 4.0, true)
		_place("pier", Vector2i(67, 56), Vector2i.ONE, true)
		_place("boat", Vector2i(65, 60), Vector2i(2, 1), true)
		for y in HEIGHT:
			for x in WIDTH:
				var cell := Vector2i(x, y)
				if _taken[_i(x, y)] == 1 or level_at(x, y) != 0 or clan.has_point(cell):
					continue
				var near_clan := clan.grow(6).has_point(cell) and _noise(x, y, 5.0, 62) > 0.42
				if near_clan or _noise(x, y, 8.0, 63) > 0.66:
					_tile[_i(x, y)] = "forest"

	# 서역, the Western Regions: sand and gravel under the Tianshan's snow, the Cult's fortress on a
	# terrace of the range with its hall, towers and a long stair, the wall of Jiayu Pass across
	# the corridor with its gate, an oasis of poplars and adobe houses round a pool, and dunes
	# running away east.
	func _lay_west() -> void:
		_lanes = {"village": [], "pass": [], "fortress": []}
		_places = {"west": Vector2(60, 31), "west_village": Vector2(44, 46), "village": Vector2(44, 46)}
		for y in HEIGHT:
			for x in WIDTH:
				_tile[_i(x, y)] = "earth" if _noise(x, y, 8.0, 64) > 0.66 else "sand"
				_face[_i(x, y)] = "bank_face"
		var rise := func(x: int, y: int) -> float:
			return (30.0 - y) / 3.0 + (_noise(x, y, 12.0, 2) - 0.5) * 2.0 + (_noise(x, y, 30.0, 3) - 0.5) * 3.0
		_raise(rise, [0, 0, 2, 2, 4, 5], 0.66)
		for y in HEIGHT:
			for x in WIDTH:
				var level := level_at(x, y)
				if level > 0:
					var patch := _noise(x, y, 6.0, 65)
					_tile[_i(x, y)] = "snow" if level >= 4 or patch > 0.62 else ("forest" if level == 2 and patch < 0.25 else "rock")
		_crown("snow", "snow_pine", 4, 124)
		# The fortress: a terrace four levels up, a lower landing, and a stair down both.
		_flat(Rect2i(62, 14, 26, 16), 4, "rock_face", "paving")
		# The landing: a rounded shelf of rock under the gate, its stair cut down its front.
		_flat(Rect2i(66, 30, 20, 10), 0, "bank_face", "sand")
		for y in range(30, 36):
			for x in range(66, 86):
				var d := ((Vector2(x, y) - Vector2(75.5, 30.5)) / Vector2(9.0, 5.5)).length() + (_noise(x, y, 4.0, 78) - 0.5) * 0.35
				if d < 1.0 or (x >= 74 and x <= 77):
					_level[_i(x, y)] = 2
					_face[_i(x, y)] = "rock_face"
					_tile[_i(x, y)] = "rock"
		for y in [29, 35]:
			_stair(75, y)
			_stair(76, y)
		for x in range(63, 87):
			if x < 73 or x > 76:
				_place("wall_h", Vector2i(x, 28), Vector2i.ONE, false)
		for y in range(15, 28):
			_place("wall_v", Vector2i(63, y), Vector2i.ONE, false)
			_place("wall_v", Vector2i(86, y), Vector2i.ONE, false)
		_place("town_gate", Vector2i(73, 27), Vector2i(4, 2), false)
		_place("cult_hall", Vector2i(72, 18), Vector2i(5, 3), false)
		_place("tiled_house_big", Vector2i(65, 20), Vector2i(4, 3), false)
		_place("tiled_house_big", Vector2i(81, 20), Vector2i(4, 3), false)
		_place("watchtower", Vector2i(64, 25), Vector2i(2, 2), false)
		_place("watchtower", Vector2i(84, 25), Vector2i(2, 2), false)
		for spot: Vector2i in [Vector2i(71, 22), Vector2i(78, 22), Vector2i(72, 25), Vector2i(78, 25)]:
			_place("lantern", spot, Vector2i.ONE, false)
		var fortress := []
		for y in range(22, 40):
			fortress.append(Vector2i(75, y))
			fortress.append(Vector2i(76, y))
		for x in range(66, 85):
			fortress.append(Vector2i(x, 24))
		_lane_cells(fortress, "fortress")
		# Jiayu Pass: the wall across the corridor, its gate tower and watchtowers.
		_flat(Rect2i(18, 36, 38, 5), 0, "bank_face", "earth")
		for x in range(20, 53):
			if x < 34 or x > 37:
				_place("wall_h", Vector2i(x, 39), Vector2i.ONE, false)
		_place("town_gate", Vector2i(34, 38), Vector2i(4, 2), false)
		_place("watchtower", Vector2i(24, 37), Vector2i(2, 2), false)
		_place("watchtower", Vector2i(48, 37), Vector2i(2, 2), false)
		_lay_lane([Vector2(35.5, 31), Vector2(35.5, 43), Vector2(42, 48), Vector2(60, 52), Vector2(96, 56)], "pass")
		_lay_lane([Vector2(75.5, 39), Vector2(70, 37), Vector2(52, 34), Vector2(36, 33)], "pass")
		# The oasis: a pool among grass, poplars and willows, adobe houses round it, a caravan.
		var pool := Vector2(46, 51)
		for y in range(41, 66):
			for x in range(30, 68):
				var d := ((Vector2(x, y) - pool) / Vector2(13.0, 8.0)).length() + (_noise(x, y, 4.0, 66) - 0.5) * 0.3
				if _taken[_i(x, y)] == 1:
					continue
				if d < 0.42:
					_water(x, y)
				elif d < 0.75:
					_tile[_i(x, y)] = "forest" if _noise(x, y, 4.0, 67) > 0.55 else "plain"
				elif d < 1.0:
					_tile[_i(x, y)] = "hills" if _noise(x, y, 4.0, 68) > 0.5 else "plain"
		for x in range(40, 54):
			if _hash(x, 1, 69) > 0.6:
				_add_tree(Vector2i(x, 54), "willow", 2)
		var ring := []
		for k in 24:
			var at := pool + Vector2(cos(k * TAU / 24.0) * 10.0, sin(k * TAU / 24.0) * 6.0)
			ring.append(at)
		ring.append(ring[0])
		_lay_lane(ring, "village")
		_line_houses(ring, ["adobe_house", "adobe_house", "adobe_small"], 5.0, false)
		_place("cart", Vector2i(36, 46), Vector2i(2, 1), false)
		_place("cart", Vector2i(38, 45), Vector2i(2, 1), false)
		for spot: Vector2i in [Vector2i(35, 49), Vector2i(55, 45), Vector2i(58, 52)]:
			_place("crocks", spot, Vector2i.ONE, false)
		# Dunes: long ridges of sand rising as slopes, never by a cliff.
		for y in range(40, HEIGHT):
			for x in range(60, WIDTH):
				var i := _i(x, y)
				if _taken[i] == 1 or level_at(x, y) != 0:
					continue
				var ridge := sin((x * 0.45 + y * 0.9) / 3.2 + _noise(x, y, 14.0, 70) * 4.0)
				var height := (ridge + 0.2) * 1.4 * clampf((x - 60) / 12.0, 0.0, 1.0)
				var level := clampi(roundi(height), 0, 2)
				_tile[i] = "sand"
				if level > 0:
					_level[i] = level
					_slope[i] = 1
					_taken[i] = 1

	# 강남, Jiangnan: a white-walled water town on a grid of canals faced in stone, arched bridges
	# over them, boats, houses opening on the water; Tai Lake to the west, tea on the low hills,
	# and rice paddies and a hamlet south of the town.
	func _lay_jiangnan() -> void:
		_lanes = {"town": [], "fields": [], "village": []}
		_places = {"jiangnan": Vector2(60, 46), "jiangnan_village": Vector2(60, 43), "village": Vector2(60, 43)}
		for y in HEIGHT:
			for x in WIDTH:
				_tile[_i(x, y)] = "hills" if _noise(x, y, 9.0, 1) > 0.62 else "plain"
				_face[_i(x, y)] = "bank_face"
		# Tea on low hills to the north-east, rising as slopes.
		for y in HEIGHT:
			for x in WIDTH:
				var h := _noise(x, y, 10.0, 71) * 2.6 + (x - 86.0) / 10.0 - (y - 30.0) / 10.0
				if x > 84 and y < 40 and h > 1.0:
					var i := _i(x, y)
					_level[i] = clampi(floori(h), 1, 2)
					_slope[i] = 1
					_taken[i] = 1
					_tile[i] = "garden" if _noise(x, y, 6.0, 72) > 0.35 else "forest"
		# Tai Lake's shore.
		for y in HEIGHT:
			for x in WIDTH:
				if x < 18 + _noise(0, y, 8.0, 73) * 10.0:
					_water(x, y)
		var town := Rect2i(30, 30, 56, 32)
		_flat(town, 0, "bank_face", "plain")
		# The canals: one east and west to the lake, two north and south, faced in stone.
		var canals := []
		for x in range(10, 90):
			for y in range(44, 48):
				canals.append(Vector2i(x, y))
		for cx in [50, 70]:
			for y in range(town.position.y, town.end.y + 2):
				for x in range(cx, cx + 3):
					canals.append(Vector2i(x, y))
		for cell: Vector2i in canals:
			_water(cell.x, cell.y)
		for cell: Vector2i in canals:
			for d: Vector2i in [Vector2i(0, -1), Vector2i(-1, 0), Vector2i(1, 0), Vector2i(0, 1)]:
				var bank := cell + d
				if _inside(bank) and level_at(bank.x, bank.y) == 0:
					_face[_i(bank.x, bank.y)] = "stone_face"
					if town.has_point(bank):
						_tile[_i(bank.x, bank.y)] = "paving"
		_place("arch_bridge", Vector2i(49, 38), Vector2i(5, 1), true)
		_place("arch_bridge", Vector2i(69, 54), Vector2i(5, 1), true)
		_place("arch_bridge", Vector2i(49, 56), Vector2i(5, 1), true)
		_place("arch_bridge", Vector2i(69, 36), Vector2i(5, 1), true)
		for x in [60, 80, 38]:
			for y in range(44, 48):
				_place("pier", Vector2i(x, y), Vector2i.ONE, true)
		# Houses on the water: rows along the canal banks, their fronts on the streets.
		# Stone streets along the canals and alleys between the rows, grass in the courts.
		for x in range(town.position.x, town.end.x):
			for y in [37, 42, 49, 54]:
				_lay_tile(Vector2i(x, y), "paving")
		for y in range(town.position.y, town.end.y):
			for x in [48, 54, 68, 74]:
				_lay_tile(Vector2i(x, y), "paving")
		for row: int in [34, 39, 51, 56]:
			var x := town.position.x + 1 + int(_hash(row, 0, 76) * 2.0)
			while x < town.end.x - 3:
				var big := _hash(x, row, 74) > 0.7
				var size := Vector2i(4, 3) if big else Vector2i(3, 2)
				var corner := Vector2i(x, row - (1 if big else 0))
				# Now and then a court is left open, with a tree or a well in it.
				if _hash(x, row, 77) < 0.16:
					_add_tree(Vector2i(x + 1, row + 1), "tree", 3)
					x += 3
				elif _place("water_house_big" if big else "water_house", corner, size, false, true):
					x += size.x + 1
				else:
					x += 1
		for spot: Vector2i in [Vector2i(56, 45), Vector2i(76, 46), Vector2i(42, 44), Vector2i(50, 32), Vector2i(71, 58)]:
			_place("boat", spot, Vector2i(2, 1), true)
		for k in 3:
			_place(["stall_red", "stall_blue", "stall_amber"][k], Vector2i(57 + k * 3, 41), Vector2i(2, 1), false)
		for x in range(33, 86, 6):
			_place("lantern", Vector2i(x, 42), Vector2i.ONE, false)
		for x in range(31, 86, 7):
			_add_tree(Vector2i(x, 49), "willow", 2)
		var streets := []
		for x in range(31, 86):
			streets.append(Vector2i(x, 42))
			streets.append(Vector2i(x, 49))
		for y in range(31, 61):
			streets.append(Vector2i(48, y))
			streets.append(Vector2i(54, y))
		_lane_cells(streets, "town")
		# Paddies and a hamlet south of the town.
		_fields(Rect2i(18, 63, WIDTH - 18, HEIGHT - 63), [["paddy", 0.7], ["garden", 0.15], ["hills", 0.15]], 75)
		var lane := [Vector2(40, 72), Vector2(55, 70), Vector2(70, 73), Vector2(88, 70)]
		_lay_lane(lane, "fields")
		_line_houses(lane, ["water_house", "thatched_house", "thatched_small"], 5.0, true)
		_lay_lane([Vector2(60, 61), Vector2(58, 70)], "village")
		_lane_cells(streets.slice(0, 40), "village")

## Walks the valley's people: each army goes to a lane cell of its place, and on arriving, to
## another, picked by a seeded draw, so a run replays exactly.
class Villagers extends RefCounted:
	var world: WorldAdapter
	var valley: Valley
	var _place := {}
	var _goal := {}
	var _rng := RandomNumberGenerator.new()

	## A world of `people` people in armies of `per_army`, spread over the places by their lanes.
	func _init(on: Valley, people: int, per_army: int) -> void:
		valley = on
		_rng.seed = 7
		var places := valley.people_places()
		var armies: Array[Dictionary] = []
		var room := {}
		for place: String in places:
			room[place] = valley.room(place)
		for n in people / per_army:
			var place: String = places[n % places.size()]
			var members: Array[Dictionary] = []
			for k in per_army:
				# Each person on a cell of its own: lane cells first, then the open ground round them.
				var free: Array = room[place]
				assert(not free.is_empty(), "no open cell left in %s for person %d" % [place, n * per_army + k])
				var cell: Vector2i = free.pop_at(_rng.randi_range(0, free.size() - 1))
				members.append({"cell": cell, "health": 100})
			armies.append({"objective": members[0].cell, "faction": [2, 2, 0, 2, 1][n % 5],
					"skills": [IDLE_SKILL], "members": members})
			_place[n] = place
		world = WorldAdapter.create(SEED, valley.map_rows(), armies, [], {"separation": 0,
				"hostile": [[false, false, false], [false, false, false], [false, false, false]],
				"stage_floors": [75, 25, 1]})
		assert(world != null, "the core refused the valley's world")
		for army in world.army_count():
			_walk(army)

	func step() -> void:
		for army in world.army_count():
			var positions := world.body_positions(army)
			if positions.is_empty():
				continue
			if (positions[0] / float(WorldAdapter.CELL_SIZE)).distance_to(Vector2(_goal[army]) + Vector2(0.5, 0.5)) <= 1.5:
				_walk(army)

	func _walk(army: int) -> void:
		var lanes := valley.lanes(_place[army])
		_goal[army] = lanes[_rng.randi_range(0, lanes.size() - 1)]
		world.order(army, _goal[army], PackedInt64Array([0]))


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2 or not args[1] in ["frames", "stills", "bench", "level"]:
		printerr("angle: usage: -- <dir> frames|stills|level|bench [valley|central|sichuan|west|jiangnan] [crowd] [m1|level] [pan] [flat] [solid]")
		quit(2)
		return
	_dir = args[0]
	_mode = args[1]
	_flags = args.slice(2)
	for flag: String in _flags:
		if flag in REGIONS:
			_region = flag
	if _region != "" and not "valley" in _flags:
		_flags.append("valley")
	create_timer(DEADLINES[_mode]).timeout.connect(func() -> void:
		printerr("angle: %s ran past its %.0f s deadline; quitting" % [args[1], DEADLINES[args[1]]])
		quit(3))
	DirAccess.make_dir_recursive_absolute(_dir.path_join("frames"))
	if _mode == "frames" and "valley" in _flags:
		DisplayServer.window_set_size(Vector2i(640, 360))
		_start_valley()
		_add_view(root, -1)
		_camera.position = (_valley.places().village * AngleView.CELL).round()
	elif _mode == "frames":
		DisplayServer.window_set_size(Vector2i(640, 360))
		_start_world()
		_add_view(root, _world.player_army())
	elif _mode == "stills" and _region != "":
		_shots.append({"still": _region, "locale": "ko"})
		_shots.append({"still": _region + "_village", "locale": "ko"})
	elif _mode == "stills" and "valley" in _flags:
		for still: String in VALLEY_STILLS:
			for locale: String in LOCALES:
				_shots.append({"still": still, "locale": locale})
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


# The valley and its people, VALLEY_PEOPLE or with crowd CROWD.
func _start_valley() -> void:
	_valley = Valley.new(_region)
	var crowd := "crowd" in _flags
	_villagers = Villagers.new(_valley, CROWD if crowd else VALLEY_PEOPLE, 10 if crowd else VALLEY_ARMY)
	_world = _villagers.world
	var people := 0
	for army in _world.army_count():
		people += _world.roster(army).size()
	print("angle: %s %dx%d cells, %d structures, %d people in %d armies" % [_region if _region != "" else "valley",
			Valley.WIDTH, Valley.HEIGHT,
			_valley.structures().size(), people, _world.army_count()])


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
	_view.show_world(_world, steered, not "flat" in _flags, _valley)
	if parent == root:
		_camera = Camera2D.new()
		root.add_child(_camera)
		_camera.position = (FOCUS * AngleView.CELL).round()


# The cell a still centres on; in the valley the bench's is the village's.
func _still_focus(still: String) -> Vector2:
	return _valley.places().get(still, _valley.places().village) if _valley != null else FOCUS


# The point `distance` cells along PAN's loop.
func _pan_at(distance: float) -> Vector2:
	var path: Array = PAN if _valley == null else VALLEY_PAN.map(func(place: String) -> Vector2: return _valley.places()[place])
	var length := 0.0
	for n in path.size():
		length += (path[n] as Vector2).distance_to(path[(n + 1) % path.size()])
	var left := fmod(distance, length)
	for n in path.size():
		var from: Vector2 = path[n]
		var to: Vector2 = path[(n + 1) % path.size()]
		if left <= from.distance_to(to):
			return from.move_toward(to, left)
		left -= from.distance_to(to)
	return path[0]


# The part of the world on screen, in world pixels; the camera has no smoothing and its limits
# lie outside what it shows.
func _view_rect() -> Rect2:
	var size := root.get_visible_rect().size / _camera.zoom
	return Rect2(_camera.position - size / 2.0, size)


func _tick() -> void:
	if _villagers != null:
		_villagers.step()
	else:
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
	if seconds >= 1.0 and time > _worst[0]:
		_worst = [time, seconds]
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
	print("angle bench: levelling, worst frame from the pull-back on %.3f ms at %.2f s" % _worst)
	print("angle bench: levelling, chunks %d resident now, %d at most, %d built; ground images %d bytes" % [
			_view.resident, _view.most_resident, _view.built, _view.ground_bytes])
	quit(0)
	return true


# One frame of the sequence each process frame, drawn before it is read, so a capture holds it
# even while the window is hidden and the loop would not draw.
func _frames() -> bool:
	# The first frames bake the map and stream the chunks in; what they add draws from the next.
	if _frame <= 2:
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


# Saves, the first time it happens, a 4x crop round a person hidden by a building and round a
# person standing in front of one.
func _note_depth(image: Image) -> void:
	for pair: Array in [["depth_behind", _view.hidden_list], ["depth_front", _view.front_list]]:
		if _depth.has(pair[0]):
			continue
		for seen: Array in pair[1]:
			if seen[2] in ["tree", "terrain"] or seen[4] < DEPTH_ROWS:
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
			if "valley" in _flags:
				# The HUD is the generated region's player's; the valley is drawn in its place.
				_play._play(WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY))
				_start_valley()
			else:
				_start_world()
				_play._play(_world)
			_play.set_process(false)
			_play.get_node("WorldArt").visible = false
			_play.get_node("Sound").process_mode = Node.PROCESS_MODE_DISABLED
			_view = AngleView.new()
			_view.xray = not "solid" in _flags
			_play.add_child(_view)
			_play.move_child(_view, 1)
			_view.show_world(_world, _world.player_army() if _valley == null else -1, not "flat" in _flags, _valley)
			_camera = _play.get_node("Camera")
			_camera.limit_left = -100000
			_camera.limit_top = -100000
			_camera.limit_right = 100000
			_camera.limit_bottom = 100000
			_camera.position = (_still_focus(shot.still) * AngleView.CELL).round()
			var target: int = VALLEY_TICKS if _valley != null else STILLS.get(shot.still, 0) * TICKS_PER_FRAME
			for i in target:
				_tick()
			if shot.still == "bench":
				_since_tick = 0.0
				_mode = "bench"
				return false
		3:
			_play.fit_view()
			if shot.still == "valley" or shot.still in REGIONS:
				_camera.zoom = Vector2(2, 2)
			_view.refresh(_view_rect(), 1.0, STILLS.get(shot.still, 0) * FRAME_MS)
			_hud.refresh(_play.world, _play.steered, _play._skills)
			if _frame % SETTLE_FRAMES != 0:
				return false
		4:
			var name := "%s_%s" % [shot.still, shot.locale]
			if shot.still == "valley" or shot.still in REGIONS:
				_camera.zoom = Vector2(2, 2)
				_view.refresh(_view_rect(), 1.0, 0)
				RenderingServer.force_draw(false)
			RenderingServer.force_draw(false)
			var image := root.get_texture().get_image()
			image.save_png(_dir.path_join(name + ".png"))
			print("angle: %s.png %dx%d at frame %d, tick %d, %d bodies drawn, %d hidden" % [name,
					image.get_width(), image.get_height(), STILLS.get(shot.still, 0), _world.tick_count(), _view.drawn,
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
		if _driver != null or _villagers != null:
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
		_hud.refresh(_play.world, _play.steered, _play._skills)
	_bench_frames += 1
	# A window behind others is paced at the display's refresh rather than drawn as fast as it can.
	if _bench_frames == 1:
		DisplayServer.window_move_to_foreground()
	if _bench_frames == WARM_UP_FRAMES:
		_bench_started = Time.get_ticks_msec()
	if _bench_frames > WARM_UP_FRAMES:
		_times.append((now - _last_usec) / 1000.0)
	_last_usec = now
	var out_of_time := _bench_started > 0 and Time.get_ticks_msec() - _bench_started > BENCH_SECONDS * 1000.0
	if _times.size() < TIMED_FRAMES and not (out_of_time and not _times.is_empty()):
		return false
	var sorted := _times.duplicate()
	sorted.sort()
	var scene := "M1 scenario" if "m1" in _flags else ("valley under the HUD" if _valley != null else "village sequence under the HUD")
	print("angle bench: %s, flags %s, window %s, vsync off, %s, Godot %s" % [scene, _flags,
		DisplayServer.window_get_size(), RenderingServer.get_video_adapter_name(), Engine.get_version_info().string])
	print("angle bench: %d bodies drawn, %d hidden behind something in the last frame" % [_view.drawn, _view.occluded])
	print("angle bench: frame time p50 %.3f ms, p99 %.3f ms, max %.3f ms over %d frames" % [
		sorted[sorted.size() / 2], sorted[ceili(sorted.size() * 0.99) - 1], sorted[-1], sorted.size()])
	if absf(sorted[sorted.size() / 2] - 1000.0 / 144.0) < 0.05:
		print("angle bench: PACED: the p50 is a 144 Hz display's period, so the window was paced, not measured")
	var builds := Array(_view.build_usec)
	builds.sort()
	print("angle bench: chunks of %d cells; %d resident now, %d at most; %d built, %d freed; ground images %d bytes now (%d a chunk)" % [
		AngleView.CHUNK, _view.resident, _view.most_resident, _view.built, _view.freed, _view.ground_bytes,
		_view.ground_bytes / maxi(_view.resident, 1)])
	print("angle bench: chunk build p50 %.3f ms, max %.3f ms; camera travelled %.0f cells" % [
		builds[builds.size() / 2] / 1000.0, builds[-1] / 1000.0, _pan_seconds * PAN_SPEED])
	if not _view.bake_usec.is_empty():
		var bakes := Array(_view.bake_usec)
		bakes.sort()
		var total := 0
		for usec: int in bakes:
			total += usec
		print("angle bench: map build baked %d chunks in %.0f ms, p50 %.1f ms, max %.1f ms a chunk" % [bakes.size(),
				total / 1000.0, bakes[bakes.size() / 2] / 1000.0, bakes[-1] / 1000.0])
	print("angle bench: texture memory %.1f MB, video memory %.1f MB, static memory %.1f MB, %d nodes" % [
		Performance.get_monitor(Performance.RENDER_TEXTURE_MEM_USED) / 1048576.0,
		Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED) / 1048576.0,
		Performance.get_monitor(Performance.MEMORY_STATIC) / 1048576.0,
		Performance.get_monitor(Performance.OBJECT_NODE_COUNT)])
	quit(0)
	return true
