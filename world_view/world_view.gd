## Shows the core's M1 benchmark scenario: every body drawn through one MultiMesh, structures
## coloured by stage, and the camera's centre set as the world's focus each tick. It ticks the
## world at the core's tick rate whatever the frame rate, and holds no game rule.
extends Node2D

const PIXELS_PER_CELL := 16.0
const AGENT_PIXELS := 3.0
const CAMERA_PIXELS_PER_SECOND := 800.0
# Ticks one frame may run to catch up; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4
const AGENT_COLOUR := Color(0.85, 0.85, 0.75)
const AGGREGATE_COLOUR := Color(0.95, 0.55, 0.2)
# Indexed by WorldAdapter.STAGE_*.
const STAGE_COLOURS: Array[Color] = [
	Color(0.45, 0.5, 0.6), Color(0.75, 0.6, 0.3), Color(0.6, 0.2, 0.15), Color(0.25, 0.25, 0.25),
]

var world: WorldAdapter
var ticks := 0
var bodies_drawn := 0

var _since_tick := 0.0

@onready var _bodies: MultiMeshInstance2D = $Bodies
@onready var _camera: Camera2D = $Camera


func _ready() -> void:
	world = WorldAdapter.create_m1()
	_camera.position = _to_pixels(Vector2(WorldAdapter.m1_focus()))
	var agents := 0
	for army in world.army_count():
		for count in world.body_counts(army):
			agents += count
	# A body holds at least one agent, so there are never more bodies than agents.
	_bodies.multimesh.instance_count = agents


func _process(delta: float) -> void:
	_camera.position += Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down") \
			* CAMERA_PIXELS_PER_SECOND * delta / _camera.zoom.x
	var step := 1.0 / WorldAdapter.TICKS_PER_SECOND
	_since_tick = minf(_since_tick + delta, step * MAX_TICKS_PER_FRAME)
	while _since_tick >= step:
		world.set_focus(Vector2i(_camera.position * WorldAdapter.CELL_SIZE / PIXELS_PER_CELL))
		world.tick()
		ticks += 1
		_since_tick -= step
	_draw_bodies()
	queue_redraw()


func _draw() -> void:
	for structure in world.structure_count():
		var corner := Vector2(world.structure_cell(structure)) * PIXELS_PER_CELL
		draw_rect(Rect2(corner, Vector2.ONE * PIXELS_PER_CELL),
				STAGE_COLOURS[world.structure_stage(structure)])


func _draw_bodies() -> void:
	var multimesh := _bodies.multimesh
	var instance := 0
	for army in world.army_count():
		var positions := world.body_positions(army)
		var counts := world.body_counts(army)
		for body in counts.size():
			var size := AGENT_PIXELS * sqrt(counts[body])
			multimesh.set_instance_transform_2d(instance,
					Transform2D(0.0, Vector2(size, size), 0.0, _to_pixels(positions[body])))
			multimesh.set_instance_color(instance,
					AGENT_COLOUR if counts[body] == 1 else AGGREGATE_COLOUR)
			instance += 1
	multimesh.visible_instance_count = instance
	bodies_drawn = instance


func _to_pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
