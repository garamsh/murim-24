## Captures a strip of the player's fight in the play scene, in Korean, at 1920x1080: the player,
## born in the Cult village of seed 2026, is ordered on the largest hostile army; the world is
## stepped without drawing until the player first strikes, then drawn frame by frame and every
## other frame of the next CAPTURED is saved, the 640x360 round the player at the screen's middle. Exits on its own after at most FRAME_CAP frames.
extends SceneTree

const SCENE := "res://play/play.tscn"
const SEED := 2026
const CULT_VILLAGE := 1
const MARCH_LIMIT := 12000
const CAPTURED := 240
const FRAME_CAP := 900

var _scene: Node2D
var _frame := 0
var _saved := 0
var _out := ""


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	TranslationServer.set_locale("ko")
	_scene = load(SCENE).instantiate()
	_scene.birth_village = CULT_VILLAGE
	_scene.save_directory = "user://capture_saves"
	root.add_child(_scene)


# Starts the world once the scene is ready, and steps it until the player first strikes.
func _begin() -> void:
	_scene.start(SEED)
	var world: WorldAdapter = _scene.world
	var target := Vector2i(-1, -1)
	var largest := 0
	for army in world.army_count() - 1:
		var roster := world.roster(army)
		if world.hostile(world.army_faction(_scene.steered), world.army_faction(army)) and roster.size() > largest:
			largest = roster.size()
			target = Vector2i((world.person_position(roster[0]) / WorldAdapter.CELL_SIZE).floor())
	world.order(_scene.steered, target, PackedInt64Array([0]))
	var lead := world.roster(_scene.steered)[0]
	for i in MARCH_LIMIT:
		_scene.step()
		var struck := world.strikes().any(func(strike: Dictionary) -> bool: return strike.person == lead)
		if struck:
			print("capture: the player first struck after %d ticks" % i)
			break


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 2:
		_begin()
	if _frame > 12 and _frame % 2 == 0 and _saved < CAPTURED:
		var combat: Node2D = _scene.get_node("WorldArt/Combat")
		var now := Time.get_ticks_msec()
		print("frame %d: tick %d, %d uses, %d numbers, %d dying, target %d" % [_saved, _scene.ticks,
				combat.shown_strikes(now).size(), combat.shown_numbers(now).size(), combat.dying(now).size(),
				combat.target])
		var image := root.get_texture().get_image().get_region(Rect2i(640, 360, 640, 360))
		image.save_png("%s/frame_%02d.png" % [_out, _saved])
		_saved += 1
	if _saved >= CAPTURED or _frame >= FRAME_CAP:
		print("capture: %d frames saved in %d" % [_saved, _frame])
		quit(0)
	return false
