## Measures how the steered body moves on screen in play.tscn while the joystick holds it one way:
## each frame, at a fixed frame rate, it reads where WorldArt drew the body (from its MultiMesh
## bands) and where the camera stands, and prints the counts below. Given a capture directory, it
## holds the camera where the walk began, so the walk crosses the screen, and saves there every
## SHEET_EVERY-th frame, a sheet of them at 1/SHEET_SHRINK of the window, and the last frame with
## every frame's drawn body centre marked in red. Capturing needs a display. Arguments after `--`:
## <way: straight|diagonal|click> <fps> <birth village> [capture_dir]
## `click` presses and lets go the right button on the cell RUN cells down a clear diagonal,
## instead of the joystick.
extends SceneTree

const SCENE := "res://play/play.tscn"
const SEED := 2026
const RUN := 14
const FRAMES_SECONDS := 4.0
const WARM_TICKS := 15
const SHEET_EVERY := 30
const SHEET_COLUMNS := 4
# Each frame on the sheet is the window's size over this.
const SHEET_SHRINK := 2

var _scene: Node2D
var _fps := 60
var _straight := false
var _click := false
var _village := 0
var _way := Vector2i.ZERO
var _frame := 0
var _frames := 0
var _capture_dir := ""
var _drawn: Array[Vector2] = []
var _on_screen: Array[Vector2] = []
var _ticks: Array[int] = []
var _held_camera := Vector2.INF
var _frames_saved: Array[Image] = []
var _last_image: Image


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	_straight = args[0] == "straight"
	_click = args[0] == "click"
	_fps = int(args[1])
	_village = int(args[2])
	if args.size() > 3:
		_capture_dir = args[3]
		DirAccess.make_dir_recursive_absolute(_capture_dir)
	_frames = int(FRAMES_SECONDS * _fps)
	_scene = load(SCENE).instantiate()
	_scene.save_directory = "user://probe_saves"
	_scene.birth_village = _village
	root.add_child(_scene)


# Starts the walk once the scene is ready in the running tree.
func _begin() -> void:
	_scene.set_process(false)
	_scene.start(SEED)
	_way = _clear_way(_straight)
	if _way == Vector2i.ZERO:
		printerr("motion probe: no clear way")
		quit(2)
		return
	if _click:
		var press := InputEventMouseButton.new()
		press.button_index = MOUSE_BUTTON_RIGHT
		press.pressed = true
		var home := Vector2i((_scene.lead() / WorldAdapter.CELL_SIZE).floor())
		press.position = (Vector2(home + _way * RUN) + Vector2.ONE * 0.5) * _scene.PIXELS_PER_CELL
		if not _scene.command(press):
			printerr("motion probe: the click was refused")
			quit(2)
		# Let go at once, or each frame would order the person toward the real cursor.
		var release := press.duplicate()
		release.pressed = false
		_scene.command(release)
		for i in WARM_TICKS:
			_scene.step()
		return
	_scene.get_node("Hud").use_touch(true)
	var touch: Control = _scene.get_node("Hud").touch
	var thumb: Vector2 = touch.STICK
	var pushed: bool = _scene.touch(_finger(thumb, true)) \
			and _scene.touch(_slide(thumb + Vector2(_way).normalized() * touch.STICK_RADIUS))
	if not pushed:
		printerr("motion probe: the joystick was not taken")
		quit(2)
		return
	for i in WARM_TICKS:
		_scene.step()


func _process(_delta: float) -> bool:
	if _frames == 0:
		return false
	if _scene.world == null:
		_begin()
		return false
	_scene._process(1.0 / _fps)
	var camera: Camera2D = _scene.get_node("Camera")
	var at := _body()
	_drawn.append(at)
	_on_screen.append(at - camera.position)
	_ticks.append(_scene.ticks)
	if _capture_dir != "":
		if _held_camera == Vector2.INF:
			_held_camera = camera.position
		camera.position = _held_camera
		# The texture holds the frame drawn before this one.
		var image := root.get_texture().get_image()
		if image != null and _frame > 0:
			_last_image = image
			if (_frame - 1) % SHEET_EVERY == 0:
				image.save_png(_capture_dir.path_join("frame_%04d.png" % (_frame - 1)))
				_frames_saved.append(image)
	_frame += 1
	if _frame >= _frames:
		_report()
		if _capture_dir != "":
			_save_sheet()
		_frames = 0
		quit(0)
	return false


# The centre WorldArt drew the steered body at this frame: the drawn body nearest its ring.
func _body() -> Vector2:
	var art: Node2D = _scene.get_node("WorldArt")
	var ring: Vector2 = art.get_node("Ring").position
	var best := Vector2.INF
	for band: MultiMeshInstance2D in art._bands:
		var mm := band.multimesh
		var buffer := mm.buffer
		for i in mm.visible_instance_count:
			var centre := Vector2(buffer[i * 12 + 3], buffer[i * 12 + 7]) + band.position
			if centre.distance_to(ring) < best.distance_to(ring):
				best = centre
	return best


func _report() -> void:
	var moves := 0
	var skips := 0
	var screen_twitches := 0
	var gaps := {}
	var last_move := -1
	var ticked_frames := 0
	for f in range(1, _drawn.size()):
		var d := _drawn[f] - _drawn[f - 1]
		if _ticks[f] != _ticks[f - 1]:
			ticked_frames += 1
		if d != Vector2.ZERO:
			moves += 1
			if absf(d.x) > 1.0 or absf(d.y) > 1.0:
				skips += 1
			if last_move >= 0:
				gaps[f - last_move] = gaps.get(f - last_move, 0) + 1
			last_move = f
		if _on_screen[f] != _on_screen[f - 1]:
			screen_twitches += 1
	# Furthest any drawn point lies off the straight line from the first to the last.
	var start := _drawn[0]
	var line := (_drawn[-1] - start).normalized()
	var off := 0.0
	for p in _drawn:
		var rel := p - start
		off = maxf(off, absf(rel.x * line.y - rel.y * line.x))
	var walked := _drawn[-1] - start
	var keys := gaps.keys()
	keys.sort()
	var histogram := []
	for k in keys:
		histogram.append("%d:%d" % [k, gaps[k]])
	print("motion probe: way %s at %d fps over %d frames (%d ticks, %d frames with a tick); walked %s px" % [
		_way, _fps, _drawn.size(), _ticks[-1] - _ticks[0], ticked_frames, walked])
	print("  frames the body moved: %d; moves of more than 1 px on an axis: %d; frames between moves {gap:count} %s" % [
		moves, skips, ", ".join(histogram)])
	print("  furthest off the straight line: %.2f px; frames the body shifted against the camera: %d" % [
		off, screen_twitches])


# Saves the frames kept side by side, and the last frame with each frame's body centre in red.
func _save_sheet() -> void:
	var full := _frames_saved[0].get_size()
	var size := full / SHEET_SHRINK
	for frame in _frames_saved:
		frame.resize(size.x, size.y, Image.INTERPOLATE_NEAREST)
	var rows := ceili(_frames_saved.size() / float(SHEET_COLUMNS))
	var sheet := Image.create_empty(size.x * SHEET_COLUMNS, size.y * rows, false, _frames_saved[0].get_format())
	for i in _frames_saved.size():
		sheet.blit_rect(_frames_saved[i], Rect2i(Vector2i.ZERO, size), Vector2i(i % SHEET_COLUMNS * size.x, i / SHEET_COLUMNS * size.y))
	sheet.save_png(_capture_dir.path_join("sheet.png"))
	var trail := _last_image.duplicate()
	var camera: Camera2D = _scene.get_node("Camera")
	# The camera stops at the region's edge, so its screen centre may differ from its position;
	# its zoom is the scale ViewScale gives the window.
	var origin: Vector2 = camera.get_screen_center_position() - Vector2(full) / camera.zoom / 2.0
	for at in _drawn:
		var pixel := Vector2i(((at - origin) * camera.zoom).round())
		trail.fill_rect(Rect2i(pixel - Vector2i(2, 2), Vector2i(5, 5)).intersection(Rect2i(Vector2i.ZERO, full)), Color.RED)
	trail.save_png(_capture_dir.path_join("trail.png"))


func _clear_way(straight: bool) -> Vector2i:
	var rows: PackedStringArray = _scene.world.map_rows()
	var home := Vector2i((_scene.lead() / WorldAdapter.CELL_SIZE).floor())
	var ways := [Vector2i.RIGHT, Vector2i.DOWN, Vector2i.LEFT, Vector2i.UP] if straight \
			else [Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1)]
	for way: Vector2i in ways:
		var clear := true
		for stride in range(0, RUN + 1):
			for cell: Vector2i in [home + way * stride, home + way * stride + Vector2i(way.x, 0), home + way * stride + Vector2i(0, way.y)]:
				clear = clear and cell.y >= 0 and cell.y < rows.size() and cell.x >= 0 \
						and cell.x < rows[cell.y].length() and rows[cell.y][cell.x] == "." \
						and _scene.world.order(_scene.steered, cell, _scene._skills)
		_scene.world.order(_scene.steered, home, _scene._skills)
		if clear:
			return way
	return Vector2i.ZERO


func _finger(at: Vector2, pressed: bool) -> InputEventScreenTouch:
	var event := InputEventScreenTouch.new()
	event.index = 0
	event.position = at
	event.pressed = pressed
	return event


func _slide(at: Vector2) -> InputEventScreenDrag:
	var event := InputEventScreenDrag.new()
	event.index = 0
	event.position = at
	return event
