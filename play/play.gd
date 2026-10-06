## Lets the player steer one person born into the generated region, through the same orders every
## army follows: a left click orders it toward the cell clicked, keys 1 to 9 to strike with that
## skill, and 0 with all its skills; C challenges its faction's leader to a duel, with the same
## order, when the core allows it. The camera follows it. It ticks at the core's tick rate, draws
## the world through world_art.tscn and the HUD through hud.tscn, and holds no game rule.
extends Node2D

const SEED := 2026
const PIXELS_PER_CELL := 16.0
# Ticks one frame may run to catch up; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4

## The village the player is born into, set before the scene is ready.
var birth_village := 0
var world: WorldAdapter
var steered := 0
var ticks := 0

var _objective := Vector2i.ZERO
var _skills := PackedInt64Array()
var _since_tick := 0.0
# Where the steered person last stood, kept for the camera once it has died.
var _last_lead := Vector2.ZERO

@onready var _camera: Camera2D = $Camera
@onready var _art: Node2D = $WorldArt
@onready var _hud: CanvasLayer = $Hud


func _ready() -> void:
	world = WorldAdapter.create_region(SEED, birth_village)
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


## Turns `event`, its position in this node's pixels, into an order for the steered person, keeping
## the part of its last order the event does not change. Returns whether it gave an order and the
## core accepted it.
func command(event: InputEvent) -> bool:
	var objective := _objective
	var skills := _skills
	var mouse := event as InputEventMouseButton
	var key := event as InputEventKey
	if mouse != null and mouse.pressed and mouse.button_index == MOUSE_BUTTON_LEFT:
		objective = Vector2i((mouse.position / PIXELS_PER_CELL).floor())
	elif key != null and key.pressed and key.keycode == KEY_C:
		return challenge() == WorldAdapter.CHALLENGE_NONE
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


## Has the steered person challenge its faction's leader, marching on the leader with its current
## skills, and shows the outcome on the HUD. Returns the core's verdict, one of the
## WorldAdapter.CHALLENGE_* constants; CHALLENGE_NONE means the duel began.
func challenge() -> int:
	var faction := world.army_faction(steered)
	var refusal := world.challenge_refusal(steered, faction)
	if refusal == WorldAdapter.CHALLENGE_NONE:
		var seat := Vector2i((world.person_position(world.leader(faction)) / WorldAdapter.CELL_SIZE).floor())
		if world.order(steered, seat, _skills, faction):
			_objective = seat
	_hud.show_challenge(refusal)
	return refusal


## Advances the world one tick.
func step() -> void:
	world.tick()
	_art.note_tick()
	ticks += 1


## Returns the steered person's position, in sub-cell units, or where it last stood while it has
## no body: once dead, until its soul is born again.
func lead() -> Vector2:
	var positions := world.body_positions(steered)
	if not positions.is_empty():
		_last_lead = positions[0]
	return _last_lead


func _all_skills() -> PackedInt64Array:
	var skills := PackedInt64Array()
	for skill in world.skill_count(steered):
		skills.append(skill)
	return skills


func _to_pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
