# Records world_art.tscn over the region of seed 2026 with a camera on the player's village, then
# at JUMP on the village farthest from it, refreshing the bands every frame without recreating
# them. Prints how many bodies world_art counts as drawn and how many pixels of the body atlas's
# outline ink the frame shows inside the view.
# Run: godot --path . --write-movie <dir>/jump.png --quit-after 40 --script <this file>
extends SceneTree

const JUMP := 20
const VIEW := Vector2(480, 270)
var _art: Node2D
var _camera: Camera2D
var _far := Vector2.ZERO
var _frame := 0


func _initialize() -> void:
	var world := WorldAdapter.create_region(2026)
	_art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(_art)
	var steered := world.army_count() - 1
	_art.show_world(world, steered)
	var home := world.body_positions(steered)[0] * 16 / WorldAdapter.CELL_SIZE
	_far = home
	for village: Dictionary in world.villages():
		var corner := Vector2(village.corner) * 16
		if corner.distance_to(home) > _far.distance_to(home):
			_far = corner
	_camera = Camera2D.new()
	_camera.position = home
	root.add_child(_camera)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == JUMP:
		_camera.position = _far
	_art.refresh(Rect2(_camera.position - VIEW / 2, VIEW))
	if _frame == JUMP - 1 or _frame == JUMP + 10:
		print("capture_jump: frame %d, camera %s, %d bodies counted drawn" % [_frame, _camera.position, _art.drawn])
	return false
