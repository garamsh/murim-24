# town_capture.gd -- <out dir>: draws world_art.tscn over a plain world with town_layout.gd's walled
# town laid on it, the player standing at its south gate and townspeople in its streets, and saves
# the whole town at 2x and the player at the gate at 3x, each 1920 x 1080.
extends SceneTree

const TownLayout := preload("town_layout.gd")
const MAP := Vector2i(70, 64)
const CORNER := Vector2i(13, 6)
const STEP := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}

var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _out := ""
var _shots := []


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	var rows := PackedStringArray()
	for y in MAP.y:
		rows.append(".".repeat(MAP.x))
	var gate := CORNER + TownLayout.SOUTH_GATE + Vector2i(0, 2)
	var folk := []
	for k in 24:
		var street := CORNER + Vector2i(21 + k % 3, 8 + k)
		if k >= 22:
			street = CORNER + Vector2i(10 + k - 22, 18)
		folk.append({"cell": street, "health": 100})
	var armies: Array[Dictionary] = [
		{"objective": CORNER + Vector2i(13, 20), "faction": 0, "skills": [STEP], "members": folk},
		{"objective": gate, "faction": 0, "skills": [STEP], "members": [{"cell": gate, "health": 100}]},
	]
	_world = WorldAdapter.create(1, rows, armies, [], {"separation": 256, "hostile": [[false]],
		"stage_floors": [75, 25, 1]})
	_viewport = SubViewport.new()
	_viewport.size = Vector2i(1920, 1080)
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_viewport.canvas_item_default_texture_filter = Viewport.DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST
	root.add_child(_viewport)
	_art = load("res://world_art/world_art.tscn").instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, 1)
	TownLayout.lay(_art, CORNER)
	TownLayout.rebake(_art, CORNER)
	var town_centre := (Vector2(CORNER) + Vector2(TownLayout.SIZE) / 2.0) * Vector2(_art.CELL)
	var at_gate := (Vector2(gate) + Vector2(0.5, -1.0)) * Vector2(_art.CELL)
	_shots = [["walled_town_2x", town_centre - Vector2(0, 20), 2.0], ["walled_town_gate_3x", at_gate, 3.0]]


func _process(_delta: float) -> bool:
	_frame += 1
	var shot: Array = _shots[0]
	var scale: float = shot[2]
	var size := Vector2(1920, 1080) / scale
	var view := Rect2(shot[1] - size / 2.0, size)
	_art.scale = Vector2(scale, scale)
	_art.position = -view.position.round() * scale
	if _frame % 3 == 0:
		_world.tick()
		_art.note_tick()
	_art.refresh(view, 1.0)
	if _frame % 120 == 0:
		_viewport.get_texture().get_image().save_png(_out + "/" + shot[0] + ".png")
		print("town_capture: saved ", shot[0], " at ", view)
		_shots.pop_front()
		if _shots.is_empty():
			quit()
	return false
