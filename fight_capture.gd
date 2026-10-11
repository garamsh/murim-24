# fight_capture.gd -- <out dir>: draws world_art.tscn over one Orthodox swordsman beside four Cult
# soldiers, his one sweep doing 80 of 100 to each it reaches, at 4x: the frames right after the first
# hits (sprays), a second later (drops, the struck maimed), and once the Cult is ordered back west,
# the maimed limping away. It quits after MAX_FRAMES whatever happens.
extends SceneTree

const MAX_FRAMES := 3000
const SWEEP := {"range": 384, "radius": 0, "body_damage": 80, "structure_damage": 0, "cooldown": 100000,
	"shape": WorldAdapter.SHAPE_ARC}
const NO_REACH := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}

var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _out := ""
var _hit := -1
var _shots := 0


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	var rows := PackedStringArray()
	for y in 30:
		rows.append(".".repeat(40))
	var orthodox := []
	var cult := []
	orthodox.append({"cell": Vector2i(18, 9), "health": 100})
	for k in 4:
		cult.append({"cell": Vector2i(19, 8 + k), "health": 100})
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(18, 9), "faction": 0, "skills": [SWEEP], "members": orthodox},
		{"objective": Vector2i(19, 9), "faction": 1, "skills": [NO_REACH], "members": cult},
	]
	_world = WorldAdapter.create(5, rows, armies, [], {"separation": 256, "hostile": [[false, true], [true, false]],
		"stage_floors": [75, 25, 1]})
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
		print("fight_capture: out of frames, saved ", _shots)
		quit(1)
		return false
	var view := Rect2(Vector2(18.5, 9.5) * Vector2(_art.CELL) - Vector2(240, 135), Vector2(480, 270))
	_art.scale = Vector2(4, 4)
	_art.position = -view.position.round() * 4.0
	if _frame > 60 and _frame % 2 == 0:
		_world.tick()
		_art.note_tick()
		if _hit < 0 and not _world.strikes().is_empty():
			_hit = _frame
			print("fight_capture: first hits at frame ", _frame)
	_art.refresh(view, 1.0)
	if _hit >= 0:
		for at: int in [1, 40, 200]:
			if _frame == _hit + at:
				_viewport.get_texture().get_image().save_png("%s/fight_%d_4x.png" % [_out, _shots])
				_shots += 1
		if _frame == _hit + 60:
			_world.order(1, Vector2i(4, 9), PackedInt64Array([0]))
		if _shots == 3:
			quit()
	return false
