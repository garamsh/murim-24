## Shows the core's M1 benchmark scenario in the approved art, through world_art.tscn, with a
## camera the arrow keys move. It ticks the world at the core's tick rate whatever the frame rate,
## and holds no game rule.
extends Node2D

const PIXELS_PER_CELL := 16.0
const CAMERA_PIXELS_PER_SECOND := 200.0
# Ticks one frame may run to catch up; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4

var world: WorldAdapter
var ticks := 0
## Bodies the world holds after the last frame, on screen or not.
var bodies := 0
## Bodies drawn in the last frame, the ones on screen.
var bodies_drawn := 0

var _since_tick := 0.0

@onready var _art: Node2D = $WorldArt
@onready var _camera: Camera2D = $Camera


func _ready() -> void:
	world = WorldAdapter.create_m1()
	_camera.position = _to_pixels(Vector2(WorldAdapter.m1_centre()))
	_art.show_world(world, -1)


func _process(delta: float) -> void:
	_camera.position += Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down") \
			* CAMERA_PIXELS_PER_SECOND * delta / _camera.zoom.x
	var step := 1.0 / WorldAdapter.TICKS_PER_SECOND
	_since_tick = minf(_since_tick + delta, step * MAX_TICKS_PER_FRAME)
	while _since_tick >= step:
		world.tick()
		_art.note_tick()
		ticks += 1
		_since_tick -= step
	var screen := get_viewport_rect().size / _camera.zoom
	_art.refresh(Rect2(_camera.position - screen / 2.0, screen))
	bodies_drawn = _art.drawn
	bodies = 0
	for army in world.army_count():
		bodies += world.body_positions(army).size()


func _to_pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
