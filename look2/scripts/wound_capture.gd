# wound_capture.gd -- <out dir>: draws world_art.tscn at 4x over three Orthodox swordsmen, each
# beside one Cult soldier his single sweep leaves at 70, 40 and 20 of 100: above half, below half
# (hurt: stained and stooping) and below a quarter (maimed: stain spread, limping and dripping);
# each foe has an untouched Cult soldier two cells south of it as its control. It saves them
# standing a second after the hits (wounds_idle_4x.png), walking west after an order
# (wounds_walk_<k>_4x.png, a walk frame apart), and once the maimed has dripped
# (wounds_drip_4x.png). It runs at 60 frames a second and quits after MAX_FRAMES whatever happens.
extends SceneTree

const MAX_FRAMES := 3000
const NO_REACH := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}
const DAMAGE := [30, 60, 80]
const COLUMNS := [8, 15, 22]

var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _out := ""
var _hit := -1


func _initialize() -> void:
	Engine.max_fps = 60
	_out = OS.get_cmdline_user_args()[0]
	var rows := PackedStringArray()
	for y in 30:
		rows.append(".".repeat(40))
	var armies: Array[Dictionary] = []
	for k in 3:
		var x: int = COLUMNS[k]
		var sweep := {"range": 384, "radius": 0, "body_damage": DAMAGE[k], "structure_damage": 0,
			"cooldown": 100000, "shape": WorldAdapter.SHAPE_ARC}
		armies.append({"objective": Vector2i(x, 9), "faction": 0, "skills": [sweep],
			"members": [{"cell": Vector2i(x, 9), "health": 100}]})
		armies.append({"objective": Vector2i(x + 1, 9), "faction": 1, "skills": [NO_REACH],
			"members": [{"cell": Vector2i(x + 1, 9), "health": 100}]})
		armies.append({"objective": Vector2i(x + 1, 11), "faction": 1, "skills": [NO_REACH],
			"members": [{"cell": Vector2i(x + 1, 11), "health": 100}]})
	_world = WorldAdapter.create(5, rows, armies, [], {"separation": 256,
		"hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]})
	_viewport = SubViewport.new()
	_viewport.size = Vector2i(1920, 1080)
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_viewport.canvas_item_default_texture_filter = Viewport.DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST
	root.add_child(_viewport)
	_art = load("res://world_art/world_art.tscn").instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, 0)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame > MAX_FRAMES:
		print("wound_capture: out of frames")
		quit(1)
		return false
	var view := Rect2(Vector2(15.5, 10.5) * Vector2(_art.CELL) - Vector2(240, 135), Vector2(480, 270))
	_art.scale = Vector2(4, 4)
	_art.position = -view.position.round() * 4.0
	if _frame > 60 and _frame % 2 == 0:
		_world.tick()
		_art.note_tick()
		if _hit < 0 and not _world.strikes().is_empty():
			_hit = _frame
			print("wound_capture: hits at frame ", _frame)
	_art.refresh(view, 1.0)
	if _hit < 0:
		return false
	var since := _frame - _hit
	if since == 60:
		_save("wounds_idle_4x")
		print("wound_capture: health ", [70, 40, 20].map(func(_h: int) -> int: return 0), " wounds ",
			range(3).map(func(k: int) -> int: return _art.combat.wound(_world.roster(k * 3 + 1)[0])),
			" controls ", range(3).map(func(k: int) -> int: return _art.combat.wound(_world.roster(k * 3 + 2)[0])))
		for k in 3:
			for army in [k * 3 + 1, k * 3 + 2]:
				_world.order(army, Vector2i(COLUMNS[k] - 5, 9 + 2 * (army - k * 3 - 1)), PackedInt64Array([0]))
	for n in 4:
		if since == 80 + n * 9:
			_save("wounds_walk_%d_4x" % n)
	if since == 400:
		_save("wounds_drip_4x")
		quit()
	return false


func _save(name: String) -> void:
	_viewport.get_texture().get_image().save_png("%s/%s.png" % [_out, name])
