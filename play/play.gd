## Lets the player steer one agent born into the generated region, through the same orders every
## army follows: a left click orders it toward the cell clicked, keys 1 to 9 to strike with that
## skill, and 0 with all its skills. Before each tick the focus is set on the agent's lead body, and
## the camera follows it. It ticks at the core's tick rate, draws the world through world_art.tscn
## and the HUD through hud.tscn, and holds no game rule.
extends Node2D

const SEED := 2026
const PIXELS_PER_CELL := 16.0
# Ticks one frame may run to catch up; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4

var world: WorldAdapter
var steered := 0
var ticks := 0

var _objective := Vector2i.ZERO
var _skills := PackedInt64Array()
var _since_tick := 0.0

@onready var _camera: Camera2D = $Camera
@onready var _art: Node2D = $WorldArt
@onready var _hud: CanvasLayer = $Hud


func _ready() -> void:
	world = WorldAdapter.create_region(SEED)
	steered = world.army_count() - 1
	_objective = Vector2i((lead() / WorldAdapter.CELL_SIZE).floor())
	_skills = _all_skills()
	_camera.position = _to_pixels(lead()).floor()
	_art.show_world(world, steered)
	_hud.show_player(world, steered)
	# The camera stops at the region's edge rather than showing past it.
	var rows := world.map_rows()
	_camera.limit_left = 0
	_camera.limit_top = 0
	_camera.limit_right = int(rows[0].length() * PIXELS_PER_CELL)
	_camera.limit_bottom = int(rows.size() * PIXELS_PER_CELL)


func _unhandled_input(event: InputEvent) -> void:
	command(make_input_local(event))


func _process(delta: float) -> void:
	var step_seconds := 1.0 / WorldAdapter.TICKS_PER_SECOND
	_since_tick = minf(_since_tick + delta, step_seconds * MAX_TICKS_PER_FRAME)
	while _since_tick >= step_seconds:
		step()
		_since_tick -= step_seconds
	_camera.position = _to_pixels(lead()).floor()
	var screen := get_viewport_rect().size / _camera.zoom
	_art.refresh(Rect2(_camera.get_screen_center_position() - screen / 2.0, screen))
	_hud.refresh(world, steered, _skills)


## Turns `event`, its position in this node's pixels, into an order for the steered agent, keeping
## the part of its last order the event does not change. Returns whether it gave an order and the
## core accepted it.
func command(event: InputEvent) -> bool:
	var objective := _objective
	var skills := _skills
	var mouse := event as InputEventMouseButton
	var key := event as InputEventKey
	if mouse != null and mouse.pressed and mouse.button_index == MOUSE_BUTTON_LEFT:
		objective = Vector2i((mouse.position / PIXELS_PER_CELL).floor())
	elif key != null and key.pressed and key.keycode >= KEY_0 and key.keycode <= KEY_9:
		var number := key.keycode - KEY_0
		skills = _all_skills() if number == 0 else PackedInt64Array([number - 1])
	else:
		return false
	if not world.order(steered, objective, skills):
		return false
	_objective = objective
	_skills = skills
	return true


## Advances the world one tick, with the focus on the steered agent's lead body.
func step() -> void:
	world.set_focus(Vector2i(lead()))
	world.tick()
	_art.note_tick()
	ticks += 1


## Returns the steered agent's lead body position, in sub-cell units.
func lead() -> Vector2:
	return world.body_positions(steered)[0]


func _all_skills() -> PackedInt64Array:
	var skills := PackedInt64Array()
	for skill in world.skill_count(steered):
		skills.append(skill)
	return skills


func _to_pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
