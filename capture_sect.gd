# Records the sect panel in a 480 x 270 window, with 4x nearest-neighbour copies:
# - play_sect.png: play.tscn on seed 2026 at tick 0, the player born in village 0, after M opens
#   the panel through play.gd. The player belongs to no sect yet, so the panel opens on the first
#   sect of the player's faction.
# - sect_ko.png and sect_en.png: the region of seed 2026 with days of five ticks after YEARS years,
#   drawn through world_art.tscn round the shown sect's seat, with the panel opened on the first
#   army whose lead person belongs to a sect, in Korean and then in English.
# - sect_en_orthodox.png: the same panel in English after Tab twice, on the Orthodox column.
# Prints what the panel shows and fails if two images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
const SHORT_DAY := 5
const YEARS := 60
var _dir := "."
var _frame := 0
var _saved := {}
var _play: Node
var _world: WorldAdapter
var _art: Node2D
var _panel: Control
var _camera: Camera2D


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	TranslationServer.set_locale("ko")
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 2:
		_play.set_process(false)
		var key := InputEventKey.new()
		key.keycode = KEY_M
		key.pressed = true
		print("capture_sect: M opens the panel %s" % _play.command(key))
	if _frame == SETTLE_FRAMES:
		_shot("play_sect", _play.get_node("Hud").sect)
		_play.queue_free()
		_lived()
	if _frame == 2 * SETTLE_FRAMES:
		_shot("sect_ko", _panel)
		TranslationServer.set_locale("en")
	if _frame == 3 * SETTLE_FRAMES:
		_shot("sect_en", _panel)
		_panel.next_faction()
		_panel.next_faction()
	if _frame == 4 * SETTLE_FRAMES:
		_shot("sect_en_orthodox", _panel)
		return true
	return false


# The region lived YEARS years, drawn round the seat of the sect the panel opens on.
func _lived() -> void:
	_world = WorldAdapter.create_region(2026, 0, SHORT_DAY)
	for tick in YEARS * 120 * SHORT_DAY:
		_world.tick()
	var army := -1
	for candidate in _world.army_count():
		var roster := _world.roster(candidate)
		if army < 0 and not roster.is_empty() and _world.person_sect(roster[0]) >= 0:
			army = candidate
	_art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(_art)
	_art.show_world(_world, -1)
	var layer := CanvasLayer.new()
	root.add_child(layer)
	_panel = load("res://hud/sect.gd").new()
	_panel.use_font(load("res://hud/fonts/Galmuri9.ttf"))
	layer.add_child(_panel)
	_panel.show_sect(_world, army)
	var seat: Dictionary = _world.villages()[_world.sect_seat(_panel.sect)]
	_camera = Camera2D.new()
	_camera.position = (Vector2(seat.corner) + Vector2(seat.area) / 2) * 16
	root.add_child(_camera)
	_art.refresh(Rect2(_camera.position - Vector2(240, 135), Vector2(480, 270)))
	print("capture_sect: after %d years army %d's lead person %d in sect %d" % [YEARS, army, _world.roster(army)[0], _panel.sect])


func _shot(name: String, panel: Control) -> void:
	var image := root.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_sect: %s.png repeats %s.png" % [name, _saved[digest]])
		quit(1)
	_saved[digest] = name
	image.save_png("%s/%s.png" % [_dir, name])
	var big := image.duplicate()
	big.resize(image.get_width() * 4, image.get_height() * 4, Image.INTERPOLATE_NEAREST)
	big.save_png("%s/%s_4x.png" % [_dir, name])
	print("capture_sect: %s.png %dx%d, sha256 %s, locale %s; faction %d, sect %d, own sect %d; details %s; cards %s; column %s" % [
		name, image.get_width(), image.get_height(), digest.left(16), TranslationServer.get_locale(),
		panel.faction, panel.sect, panel.own_sect, panel.details, panel.cards, panel.column])
