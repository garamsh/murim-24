## Captures the player's fight in the play scene at 3d20657, in Korean, at 1920x1080, the damage
## numbers at their default, "mine": the player, born in the village the second argument names in
## seed 2026, is ordered
## on the smallest hostile army, which it can outfight; the world is stepped without drawing until the player first strikes,
## then drawn frame by frame and every frame saved, the 640x360 round where the player is drawn, with a log line of what combat.gd shows on it. It stops a second after the player's
## first killing blow, or at once if the player dies before one, or after at most FRAME_CAP frames, and exits on
## its own.
extends SceneTree

const SCENE := "res://play/play.tscn"
const SEED := 2026
const MARCH_LIMIT := 12000
const FRAME_CAP := 5000
const AFTER_KILL := 60

var _scene: Node2D
var _frame := 0
var _saved := 0
var _out := ""
var _kill_frame := -1
var _lead := -1
var _centre := Vector2i(960, 540)


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	_scene = load(SCENE).instantiate()
	_scene.birth_village = int(OS.get_cmdline_user_args()[1])
	_scene.save_directory = "user://capture_saves"
	root.add_child(_scene)


func _begin() -> void:
	_scene.start(SEED)
	var world: WorldAdapter = _scene.world
	var target := Vector2i(-1, -1)
	var smallest := 1 << 30
	for army in world.army_count() - 1:
		var roster := world.roster(army)
		if world.hostile(world.army_faction(_scene.steered), world.army_faction(army)) and not roster.is_empty() and roster.size() < smallest:
			smallest = roster.size()
			target = Vector2i((world.person_position(roster[0]) / WorldAdapter.CELL_SIZE).floor())
	print("capture: ordered on an army of %d at %s" % [smallest, target])
	world.order(_scene.steered, target, PackedInt64Array([0]))
	_lead = world.roster(_scene.steered)[0]
	for i in MARCH_LIMIT:
		_scene.step()
		if world.strikes().any(func(strike: Dictionary) -> bool: return strike.person == _lead):
			print("capture: the player first struck after %d ticks; numbers %s, locale %s" % [i,
					_scene.get_node("WorldArt/Combat").damage_numbers, TranslationServer.get_locale()])
			break


# Keeps the choices in a file of the capture's own and sets them to their defaults, in Korean, once
# the scene has read the player's.
func _defaults() -> void:
	var settings = _scene.get_node("Hud").settings
	settings.path = "user://capture_r2_settings.cfg"
	settings.chosen = settings.DEFAULTS.duplicate()
	settings.chosen.language = settings.locales.find("ko")
	settings.apply()
	DisplayServer.window_set_size(Vector2i(1920, 1080))


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 1:
		_defaults()
	if _frame == 2:
		_begin()
	if _frame > 12:
		var combat: Node2D = _scene.get_node("WorldArt/Combat")
		var now := Time.get_ticks_msec()
		var uses = combat.shown_strikes(now)
		var own = uses.filter(func(s: Dictionary) -> bool: return s.steered).map(
				func(s: Dictionary) -> int: return s.shape)
		# Each other person's use: who used it, whom it struck, and where it was used on screen.
		var screen := root.get_canvas_transform() * combat.get_global_transform()
		var others = uses.filter(func(s: Dictionary) -> bool: return not s.steered).map(
				func(s: Dictionary) -> String: return "%d>%s@%s" % [s.person, Array(s.struck),
						Vector2i(screen * combat._pixels(s.origin))])
		var numbers = combat.shown_numbers(now).map(func(n: Dictionary) -> String:
				return "%s%s" % [n.text, "*" if Color(n.colour, 1.0) == combat.CRIMSON else ""])
		var kills = combat.deaths.filter(func(d: Dictionary) -> bool:
				return now - d.ms < 1000 and d.person != _lead)
		var dying = combat.dying(now).size()
		print("frame %d: tick %d, own uses %s, others' uses %s, numbers %s, dying %d, recent deaths %s, target %d, lead health %d, mode %s, crop centre %s" % [
				_saved, _scene.ticks, own, others, numbers, dying,
				kills.map(func(d: Dictionary) -> int: return d.person), combat.target,
				_scene.world.person_health(_lead), combat.damage_numbers, _centre])
		var place: Vector2i = combat.place_of(_lead)
		if place.x >= 0:
			var drawn: Vector2 = screen * _scene.get_node("WorldArt").body_at(place.x, place.y, 1.0)
			_centre = Vector2i(clampi(roundi(drawn.x), 320, 1600), clampi(roundi(drawn.y), 180, 900))
		var image := root.get_texture().get_image().get_region(Rect2i(_centre - Vector2i(320, 180), Vector2i(640, 360)))
		image.save_png("%s/frame_%04d.png" % [_out, _saved])
		if _kill_frame < 0 and kills.size() > 0 and numbers.any(func(t: String) -> bool: return not t.ends_with("*")):
			_kill_frame = _saved
			print("capture: a death with a number shown at frame %d" % _saved)
		_saved += 1
	var lead_dead: bool = _frame > 12 and _kill_frame < 0 and _scene.world.person_health(_lead) == 0
	if (_kill_frame >= 0 and _saved >= _kill_frame + AFTER_KILL) or lead_dead or _frame >= FRAME_CAP:
		print("capture: %d frames saved in %d" % [_saved, _frame])
		quit(0)
	return false
