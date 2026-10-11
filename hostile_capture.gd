# hostile_capture.gd -- <out dir> <seed>: plays a game of <seed> in Korean under the HUD, walks the
# player to the edge of the nearest village of a faction hostile to its own, and saves the screen
# at 3x every SHOT_EVERY frames from when it arrives, SHOTS of them, for red names, blood and wounds.
# It quits after MAX_FRAMES whatever happens.
extends SceneTree

const SHOT_EVERY := 45
const SHOTS := 6
const MAX_FRAMES := 4000

var _play: Node
var _frame := 0
var _args: PackedStringArray
var _target := Vector2i.ZERO
var _arrived := -1
var _saved := 0


func _initialize() -> void:
	_args = OS.get_cmdline_user_args()
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_d: float) -> bool:
	_frame += 1
	if _frame > MAX_FRAMES:
		print("hostile_capture: out of frames, saved ", _saved)
		quit(1)
		return false
	if _frame == 3:
		_play.start(int(_args[1]))
		var world: WorldAdapter = _play.world
		var own: int = world.army_faction(_play.steered)
		var lead: Vector2 = _play.lead() / WorldAdapter.CELL_SIZE
		var best := INF
		for village: Dictionary in world.villages():
			var faction := int(village.faction)
			if world.relation(own, faction) != WorldAdapter.RELATION_HOSTILE and world.relation(own, faction) != WorldAdapter.RELATION_AT_WAR:
				continue
			var edge := Vector2i(village.corner) + Vector2i(int(village.area.x) / 2, int(village.area.y) + 1)
			var d := Vector2(edge).distance_to(lead)
			if d < best:
				best = d
				_target = edge
		print("hostile_capture: walking from ", lead, " to ", _target, " ", best, " cells")
		world.order(_play.steered, _target, PackedInt64Array([0]))
	if _frame > 3:
		for k in 4:
			_play.step()
		var at: Vector2 = Vector2(_play.lead()) / WorldAdapter.CELL_SIZE
		if _arrived < 0 and at.distance_to(Vector2(_target)) < 6.0:
			_arrived = _frame
			print("hostile_capture: arrived at frame ", _frame)
		if _arrived >= 0 and (_frame - _arrived) % SHOT_EVERY == 0:
			root.get_texture().get_image().save_png("%s/hostile_%d_3x_ko.png" % [_args[0], _saved])
			_saved += 1
			if _saved == SHOTS:
				quit()
	return false
