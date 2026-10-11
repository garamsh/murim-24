## Captures the world art of one tree for a before-and-after look: the M1 battle, or the village of
## village_bench.gd's region (seed 2026) round the steered person. It ticks the world TICKS times,
## noting each tick and refreshing the view as the game does, the last LIVE_TICKS one a frame so what
## fades by the clock is fresh, after SETTLE_FRAMES frames have let the ground bake. In the battle the view steers the army nearest its centre, as the
## play scene marks the player's, so that army's uses show in jade and its losses and hits float up
## by the default damage numbers; a person near the view's middle is selected. So bodies, outlines,
## damage numbers, the target's brackets and blood show. It saves the window, and a crop round the
## target at twice the size.
## Usage: godot --path <tree> --script <this> -- <m1|village> <out prefix>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const TICKS := {"m1": 420, "village": 240}
const CROP := Vector2i(320, 180)
const LIVE_TICKS := 60
# Frames drawn before the live ticks, while the ground bakes the areas round the view.
const SETTLE_FRAMES := 120

var _scenario := "m1"
var _out := ""
var _world: WorldAdapter
var _art: Node2D
var _camera: Camera2D
var _frame := 0
var _target_at := Vector2.ZERO
var _ticked_ms := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	_scenario = args[0]
	_out = args[1]
	DisplayServer.window_set_size(Vector2i(1920, 1080))
	_art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(_art)
	_camera = Camera2D.new()
	var scale := ViewScale.world_scale(Vector2i(1920, 1080), false)
	_camera.zoom = Vector2(scale, scale)
	root.add_child(_camera)
	var steered := -1
	if _scenario == "m1":
		_world = WorldAdapter.create_m1()
		steered = _nearest_army(Vector2(WorldAdapter.m1_centre()))
	else:
		_world = WorldAdapter.create_region(2026)
		steered = _world.army_count() - 1
	_art.show_world(_world, steered)
	var centre: Vector2 = Vector2(WorldAdapter.m1_centre()) if _scenario == "m1" \
			else _world.body_positions(steered)[0]
	_camera.position = _art.to_screen(centre).round()
	for tick in TICKS[_scenario] - LIVE_TICKS:
		_world.tick()
		_art.note_tick()
		_art.refresh(_view(), 1.0)
	_art.combat.selected = _nearest(centre, steered)


func _process(_delta: float) -> bool:
	_frame += 1
	var live := _frame - SETTLE_FRAMES
	if live >= 1 and live <= LIVE_TICKS:
		_world.tick()
		_art.note_tick()
		_ticked_ms = Time.get_ticks_msec()
	_art.refresh(_view(), 1.0)
	if live == LIVE_TICKS + 1:
		var image := root.get_texture().get_image()
		image.save_png(_out + "_full.png")
		var place: Vector2i = _art.combat.place_of(_art.combat.target)
		var at: Vector2 = _art.body_at(place.x, place.y, 1.0) if place.x >= 0 else _camera.position
		var pixel := Vector2i((at - _view().position) * _camera.zoom) - CROP / 2
		pixel = pixel.clamp(Vector2i.ZERO, image.get_size() - CROP)
		var crop := image.get_region(Rect2i(pixel, CROP))
		crop.resize(CROP.x * 2, CROP.y * 2, Image.INTERPOLATE_NEAREST)
		crop.save_png(_out + "_target_2x.png")
		print("capture: last tick %d uses %d ms before; steered army %d" % [_world.strikes().size(),
			Time.get_ticks_msec() - _ticked_ms, _art.steered])
		print("capture: %s after %d ticks, %d bodies drawn, %d outlined, target %d, %d uses and %d numbers shown, %d drops and %d sprays" % [
			_scenario, TICKS[_scenario], _art.drawn, _art.outlined, _art.combat.target,
			_art.combat.shown_strikes(Time.get_ticks_msec()).size(), _art.combat.shown_numbers(Time.get_ticks_msec()).size(),
			_art.combat.shown_drops(Time.get_ticks_msec()).size(), _art.combat.shown_sprays(Time.get_ticks_msec()).size()])
		quit(0)
	return false


func _view() -> Rect2:
	var screen := Vector2(1920, 1080) / _camera.zoom
	return Rect2(_camera.position - screen / 2.0, screen)


# The army whose lead stands nearest `at`, in sub-cell units.
func _nearest_army(at: Vector2) -> int:
	var best := -1
	var least := INF
	for army in _world.army_count():
		var positions := _world.body_positions(army)
		if not positions.is_empty() and positions[0].distance_to(at) < least:
			least = positions[0].distance_to(at)
			best = army
	return best


# The living person, of an army other than `steered`, standing nearest `at`, in sub-cell units.
func _nearest(at: Vector2, steered: int) -> int:
	var best := -1
	var least := INF
	for army in _world.army_count():
		if army == steered:
			continue
		var roster := _world.roster(army)
		var positions := _world.body_positions(army)
		for body in roster.size():
			var apart := positions[body].distance_to(at)
			if apart < least:
				least = apart
				best = roster[body]
	return best
