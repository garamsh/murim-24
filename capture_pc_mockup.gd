# Mockup of the PC HUD sizes proposed for #163, composed from captures of play.tscn:
# - before_<res>_<locale>.png: the game as main draws it, everything at the world's whole-multiple
#   scale (4x at 1920x1080; 2x at 1280x720, letterboxed in the window).
# - after_<res>_<locale>.png: the world at the same scale, the HUD at half the world's scale rounded
#   up (2x at 1920x1080, 1x at 1280x720), its panels anchored to the world's corners and the
#   technique bar to its foot, in a layout of 960x540 HUD pixels; F2's hint gone and a settings
#   button under the map.
# - settings_<res>_<locale>.png: the same with the settings panel open, its dialog centred.
# The world is SEED's region with days of five ticks, lived YEARS years, steering the army of the
# first person who knows an art skill, as evidence/pr-161's capture_arts.gd does. Each image is
# composed from the 480x270 frame: the world alone, the HUD alone on a transparent background, and
# the settings panel alone; the HUD's panels are cut from it by the rects in CROPS and placed in the
# larger layout, scaled by nearest neighbour.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_pc_mockup.cfg"
const NATIVE := Vector2i(480, 270)
const LAYOUT := Vector2i(960, 540)
## Each HUD region in the 480x270 frame, and which way it is anchored: x 0 left, 1 centre, 2 right;
## y 0 top, 2 bottom.
const CROPS := [
	[Rect2i(0, 0, 176, 92), Vector2i(0, 0)],
	[Rect2i(388, 0, 92, 92), Vector2i(2, 0)],
	[Rect2i(0, 180, 228, 60), Vector2i(0, 2)],
	[Rect2i(160, 236, 160, 34), Vector2i(1, 2)],
]
const DIALOG := Rect2i(60, 4, 360, 256)
const SETTINGS_BUTTON := Rect2(436, 70, 40, 16)
const RESOLUTIONS := {"1920x1080": [Vector2i(1920, 1080), 4, 2], "1280x720": [Vector2i(1280, 720), 2, 1]}
var _dir := "."
var _frame := 0
var _step := 0
var _locale := 0
var _play: Node2D
var _fresh := false
var _army := -1
var _images := {}
var _button: Panel


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	_open()


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_fresh = true


func _set_up() -> void:
	_fresh = false
	var hud: CanvasLayer = _play.get_node("Hud")
	hud.settings.path = KEPT
	hud.settings.choose("language", _locale)
	hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		world.tick()
	_play._play(world)
	_play.set_process(false)
	for army in world.army_count():
		var roster := world.roster(army)
		if roster.is_empty():
			continue
		var known := false
		for skill: Dictionary in world.person_skills(roster[0]):
			known = known or skill.art >= 0
		if known:
			_army = army
			break
	_play.steered = _army
	_play._art.steered = _army
	_play._body = world.roster(_army)[0]
	hud.show_player(world, _army)
	print("capture_pc_mockup: steering army %d, person %d" % [_army, world.roster(_army)[0]])


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % SETTLE_FRAMES != 0:
		return false
	if _fresh:
		_set_up()
		return false
	_play._process(0.0)
	var locale: String = LOCALES[_locale]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0, 1:
			hud.labels.notice.visible = false
		2:
			_images.before = _grab()
			hud.visible = false
		3:
			_images.world = _grab()
			hud.visible = true
			hud.labels.locale.visible = false
			_button = hud._panel(SETTINGS_BUTTON)
			var word: Label = hud._label(_button, "SETTINGS_TITLE", Vector2.ZERO, hud.PAPER)
			word.size = SETTINGS_BUTTON.size - Vector2(0, 2)
			word.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			word.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
			_play.get_node("WorldArt").visible = false
			root.transparent_bg = true
		4:
			_images.hud = _grab()
			hud.settings.open()
		5:
			_images.settings = _grab()
			_compose(locale)
			hud.settings.close()
			_button.queue_free()
			root.transparent_bg = false
			_locale += 1
			if _locale >= LOCALES.size():
				print("capture_pc_mockup: done")
				quit(0)
				return true
			_step = 0
			_open()
			return false
	_step += 1
	return false


func _grab() -> Image:
	var image := root.get_texture().get_image()
	image.convert(Image.FORMAT_RGBA8)
	return image


func _compose(locale: String) -> void:
	var layout := Image.create_empty(LAYOUT.x, LAYOUT.y, false, Image.FORMAT_RGBA8)
	var extra := LAYOUT - NATIVE
	for crop: Array in CROPS:
		var rect: Rect2i = crop[0]
		var anchor: Vector2i = crop[1]
		layout.blend_rect(_images.hud, rect, rect.position + extra * anchor / 2)
	var dimmed := layout.duplicate()
	var scrim := Image.create_empty(LAYOUT.x, LAYOUT.y, false, Image.FORMAT_RGBA8)
	for y in LAYOUT.y:
		for x in LAYOUT.x:
			if (x + y) % 2 == 0:
				scrim.set_pixel(x, y, Color("14101a"))
	dimmed.blend_rect(scrim, Rect2i(Vector2i.ZERO, LAYOUT), Vector2i.ZERO)
	dimmed.blend_rect(_images.settings, DIALOG, DIALOG.position + extra / 2)
	for res: String in RESOLUTIONS:
		var size: Vector2i = RESOLUTIONS[res][0]
		var world_scale: int = RESOLUTIONS[res][1]
		var hud_scale: int = RESOLUTIONS[res][2]
		var origin := (size - NATIVE * world_scale) / 2
		_save("before_%s_%s" % [res, locale], _on_black(size, _scaled(_images.before, world_scale), origin))
		var world := _on_black(size, _scaled(_images.world, world_scale), origin)
		var after := world.duplicate()
		after.blend_rect(_scaled(layout, hud_scale), Rect2i(Vector2i.ZERO, LAYOUT * hud_scale), origin)
		_save("after_%s_%s" % [res, locale], after)
		var open := world.duplicate()
		open.blend_rect(_scaled(dimmed, hud_scale), Rect2i(Vector2i.ZERO, LAYOUT * hud_scale), origin)
		_save("settings_%s_%s" % [res, locale], open)


func _scaled(image: Image, factor: int) -> Image:
	var copy := image.duplicate()
	copy.resize(image.get_width() * factor, image.get_height() * factor, Image.INTERPOLATE_NEAREST)
	return copy


func _on_black(size: Vector2i, image: Image, origin: Vector2i) -> Image:
	var canvas := Image.create_empty(size.x, size.y, false, Image.FORMAT_RGBA8)
	canvas.fill(Color.BLACK)
	canvas.blit_rect(image, Rect2i(Vector2i.ZERO, image.get_size()), origin)
	return canvas


func _save(name: String, image: Image) -> void:
	image.save_png(_dir.path_join(name + ".png"))
	print("capture_pc_mockup: %s.png %dx%d, sha256 %s" % [name, image.get_width(), image.get_height(),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
