# Captures play.tscn at the native 480x270 over the region of seed 2026, in Korean and English:
# an application admitted (born into village 1, of the Cult) and one refused (village 2, of the
# Unorthodox), from the sect panel with Enter; the rumour log with live entries, after ten years of
# five-tick days; and the rebirth screen, after a Cult body marched on the nearest Orthodox
# village dies there and its soul is born again. Prints what each capture shows.
# Run: godot --path . --script <this file> -- <output dir>
extends SceneTree

const SEED := 2026
const SCENE := "res://play/play.tscn"
var _out := ""
var _step := 0
var _scene: Node2D
var _shots: Array[Callable] = []
var _locale := "ko"


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]
	_shots = [_admitted, _refused, _rumours, _rebirth]


func _process(_delta: float) -> bool:
	# Each shot sets its state on one frame and is saved two frames later, once drawn.
	if _step % 3 == 0:
		if _scene != null:
			_scene.queue_free()
			_scene = null
		var shot := _step / 3
		if shot >= _shots.size() * 2:
			return true
		_locale = "ko" if shot % 2 == 0 else "en"
		_shots[shot / 2].call()
	elif _step % 3 == 2:
		var shot := _step / 3
		var name: String = ["admitted", "refused", "rumours", "rebirth"][shot / 2]
		var path := "%s/%s_%s.png" % [_out, name, "ko" if shot % 2 == 0 else "en"]
		root.get_texture().get_image().save_png(path)
		print("capture_play_loop: saved %s" % path)
	_step += 1
	return false


func _start(village: int, day_ticks := 0) -> Node2D:
	var scene: Node2D = load(SCENE).instantiate()
	scene.birth_village = village
	root.add_child(scene)
	scene.start(SEED)
	# The HUD sets Korean as it is built; the capture's locale is set after.
	TranslationServer.set_locale(_locale)
	if day_ticks > 0:
		# The same region with days of day_ticks ticks, so years pass in a capture.
		var world := WorldAdapter.create_region(SEED, village, day_ticks)
		scene.world = world
		scene.steered = world.army_count() - 1
		scene.get_node("WorldArt").show_world(world, scene.steered)
		scene.get_node("Hud").show_player(world, scene.steered)
	return scene


func _apply(village: int) -> void:
	_scene = _start(village)
	_scene.command(_key(KEY_M))
	var verdict: int = _scene.apply_to_sect()
	var world: WorldAdapter = _scene.world
	print("capture_play_loop: born in village %d, verdict %d, sect now %d" % [
		village, verdict, world.person_sect(world.roster(_scene.steered)[0])])


func _admitted() -> void:
	_apply(1)


func _refused() -> void:
	_apply(2)


func _rumours() -> void:
	_scene = _start(0, 5)
	var world: WorldAdapter = _scene.world
	for tick in 10 * 120 * 5:
		_scene.step()
	_scene._process(0.0)
	var hud: CanvasLayer = _scene.get_node("Hud")
	print("capture_play_loop: %d rumours reach the player; shown %s" % [
		world.rumours(world.roster(_scene.steered)[0]).size(),
		[hud.labels.rumour_0.text, hud.labels.rumour_1.text, hud.labels.rumour_2.text]])


func _rebirth() -> void:
	_scene = _start(1)
	var world: WorldAdapter = _scene.world
	var body := world.roster(_scene.steered)[0]
	var home := world.person_position(body) / WorldAdapter.CELL_SIZE
	var target := Vector2i(-1, -1)
	for army in world.army_count() - 1:
		var roster := world.roster(army)
		if world.army_faction(army) == 0 and not roster.is_empty():
			var cell := Vector2i((world.person_position(roster[0]) / WorldAdapter.CELL_SIZE).floor())
			if target.x < 0 or Vector2(cell).distance_to(home) < Vector2(target).distance_to(home):
				target = cell
	world.order(_scene.steered, target, PackedInt64Array([0]))
	var hud: CanvasLayer = _scene.get_node("Hud")
	var ticks := 0
	while not hud.rebirth.visible and ticks < 12000:
		_scene.step()
		ticks += 1
	print("capture_play_loop: body %d died after %d ticks; the rebirth screen shows %d and %d: %s and %s" % [
		body, ticks, hud.rebirth.past, hud.rebirth.next, hud.rebirth.past_rows, hud.rebirth.next_rows])


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
