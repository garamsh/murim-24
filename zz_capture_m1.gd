## Captures the M1 battle through world_view.tscn at 1920x1080: every other frame of the first
## CAPTURED after a warm-up, the 640x360 at the screen's middle, logging what combat.gd shows.
## A second argument hides the damage numbers. Exits on its own after at most FRAME_CAP frames.
extends SceneTree

const SCENE := "res://world_view/world_view.tscn"
const WARM_UP := 3000
const CAPTURED := 150
const FRAME_CAP := 3400

var _view: Node
var _frame := 0
var _saved := 0
var _out := ""


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	_view = load(SCENE).instantiate()
	root.add_child(_view)
	if OS.get_cmdline_user_args().size() > 1:
		_view.get_node("WorldArt/Combat").damage_numbers = false


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame > WARM_UP and _frame % 2 == 0 and _saved < CAPTURED:
		var combat: Node2D = _view.get_node("WorldArt/Combat")
		var now := Time.get_ticks_msec()
		print("frame %d: tick %d, %d uses, %d numbers, %d dying" % [_saved, _view.ticks,
				combat.shown_strikes(now).size(), combat.shown_numbers(now).size(), combat.dying(now).size()])
		root.get_texture().get_image().get_region(Rect2i(640, 360, 640, 360)).save_png(
				"%s/m1_%03d.png" % [_out, _saved])
		_saved += 1
	if _saved >= CAPTURED or _frame >= FRAME_CAP:
		print("capture: %d frames saved in %d" % [_saved, _frame])
		quit(0)
	return false
