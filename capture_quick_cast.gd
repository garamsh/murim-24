# Captures play.tscn at the native 480x270 over the region of seed 2026, in Korean and English:
# the aim shown while Q is held, casting on release, with the cursor east of the person and past
# the bolt's range; a second cast a tick after the first refused on cooldown, its reason on the HUD; the pause panel's guide;
# and the settings panel on its casting row. Prints what each capture shows.
# Run: godot --path . --resolution 480x270 --script <this file> -- <output dir>
extends SceneTree

const SEED := 2026
const SCENE := "res://play/play.tscn"
const NAMES := ["aim", "aim_past_range", "refused", "guide", "settings"]
var _out := ""
var _step := 0
var _scene: Node2D
var _cursor := Vector2.ZERO
var _locale := "ko"


func _initialize() -> void:
	_out = OS.get_cmdline_user_args()[0]


func _process(_delta: float) -> bool:
	if _scene != null and _cursor != Vector2.ZERO:
		root.warp_mouse(_scene.get_global_transform_with_canvas() * _cursor)
	# Each shot sets its state on one frame and is saved three frames later, once drawn.
	if _step % 4 == 0:
		if _scene != null:
			_scene.queue_free()
			_scene = null
		var shot := _step / 4
		if shot >= NAMES.size() * 2:
			return true
		_locale = "ko" if shot % 2 == 0 else "en"
		call("_" + NAMES[shot / 2])
	elif _step % 4 == 3:
		var shot := _step / 4
		var path := "%s/%s_%s.png" % [_out, NAMES[shot / 2], _locale]
		root.get_texture().get_image().save_png(path)
		var aim: Node2D = _scene.get_node("Aim")
		print("capture_quick_cast: saved %s; aiming %s, the aim shown %s from %s to %s, range %.0f px, area %.0f px; notice \"%s\"" % [
			path, OS.get_keycode_string(_scene.aiming()), aim.visible, aim.caster, aim.point, aim.reach,
			aim.area, _scene.get_node("Hud").labels.notice.text if _scene.get_node("Hud").labels.notice.visible else ""])
	_step += 1
	return false


func _start(on_release: bool) -> void:
	_scene = load(SCENE).instantiate()
	root.add_child(_scene)
	_scene.start(SEED)
	_scene.get_node("Hud").settings.chosen.cast = 1 if on_release else 0
	# The HUD sets Korean as it is built; the capture's locale is set after.
	TranslationServer.set_locale(_locale)
	_cursor = Vector2.ZERO


func _person() -> Vector2:
	return _scene.lead() * _scene.PIXELS_PER_CELL / WorldAdapter.CELL_SIZE


func _aim() -> void:
	_start(true)
	_cursor = _person() + Vector2(30, -14)
	_scene.command(_key(KEY_Q))


func _aim_past_range() -> void:
	_start(true)
	_cursor = _person() + Vector2(110, 30)
	_scene.command(_key(KEY_Q))


func _refused() -> void:
	_start(false)
	_cursor = _person() + Vector2(30, 0)
	var first: bool = _scene.cast_at(0, _cursor) == WorldAdapter.CAST_NONE
	# The cast resolves on the next tick, which starts its cooldown.
	_scene.step()
	print("capture_quick_cast: first cast accepted %s, second refused %d" % [first, _scene.cast_at(0, _cursor)])
	_cursor = Vector2.ZERO


func _guide() -> void:
	_start(false)
	_scene.navigate(_key(KEY_ESCAPE))


func _settings() -> void:
	_start(false)
	var hud: CanvasLayer = _scene.get_node("Hud")
	hud.settings.open()
	for n in hud.settings.ROWS.find("cast"):
		hud.settings.key(_key(KEY_DOWN))


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key
