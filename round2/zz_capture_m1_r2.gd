## Captures the M1 battle through world_view.tscn at 3d20657, in Korean, at 1920x1080, where no one is steered
## and everyone who fights is a bystander: after a warm-up, every other frame of the next CAPTURED,
## the 640x360 at the screen's middle, logging what combat.gd shows. The damage numbers are at their
## default, "mine", for the first half, and "everyone" for the second, the control that the same
## fight has losses to number. Exits on its own after at most FRAME_CAP frames.
extends SceneTree

const SCENE := "res://world_view/world_view.tscn"
const WARM_UP := 3000
const CAPTURED := 120
const FRAME_CAP := 3400

var _view: Node
var _frame := 0
var _saved := 0
var _out := ""


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	TranslationServer.set_locale("ko")
	_view = load(SCENE).instantiate()
	root.add_child(_view)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame > WARM_UP and _frame % 2 == 0 and _saved < CAPTURED:
		var combat: Node2D = _view.get_node("WorldArt/Combat")
		combat.damage_numbers = "mine" if _saved < CAPTURED / 2 else "everyone"
		var now := Time.get_ticks_msec()
		print("frame %d: tick %d, %s, %d uses, %d numbers, %d dying" % [_saved, _view.ticks,
				combat.damage_numbers, combat.shown_strikes(now).size(), combat.shown_numbers(now).size(),
				combat.dying(now).size()])
		root.get_texture().get_image().get_region(Rect2i(640, 360, 640, 360)).save_png(
				"%s/m1_%03d.png" % [_out, _saved])
		_saved += 1
	if _saved >= CAPTURED or _frame >= FRAME_CAP:
		print("capture: %d frames saved in %d" % [_saved, _frame])
		quit(0)
	return false
