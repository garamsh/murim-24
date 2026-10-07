# Drives play.tscn, the main scene of the exported Murim24.app, through one play flow in Korean, as an
# autoload an override.cfg beside the executable adds (a release template ignores --script). It
# pushes each key into the root viewport as the window would, so it reaches play.gd's _unhandled_input as a
# player's key does. The world runs on its own frames at the fastest speed the settings offer.
# Saves a still at each step, native and with a 4x nearest-neighbour copy, and logs what it shows
# and both files' SHA-256. Fails if any two stills repeat.
#  01 title, 02 new game with SEED typed, 03 the world after DAYS_TICKS ticks, 04 pause,
#  05 saved, 06 back on the title, 07 Continue chosen, 08 the world resumed RESUMED_TICKS ticks on, 09 the genealogy panel
#  on the person, 10 the sect panel after Enter applies, 11 the rebirth screen after the person,
#  marched on the nearest village of a hostile faction, dies, 12 the new body in play.
# The language and speed are chosen on the settings panel and kept in KEPT, removed at the end.
# It refuses to run over a save already in slot 0, which the title would offer to continue, and
# removes the one it makes.
# Run: with Murim24.app/Contents/MacOS/override.cfg holding
#   [autoload]
#   DrivePlayFlow="*<this file>"
# run Murim24.app/Contents/MacOS/Murim24 -- <dir>
extends Node

const SEED := "2026"
const DAYS_TICKS := 4500
const SETTLE := 10
const DEATH_LIMIT := 20000
const HOSTILE := 1
const RESUMED_TICKS := 120
const KEPT := "user://drive_play_flow.cfg"
const SLOT := "user://saves/slot_0.sav"
var _dir := "."
var _frame := 0
var _step := 0
var _wait := 0
var _play: Node2D
var _shas := {}
var _failed := false
var _saved_hash := 0
var _saved_person := -1
var _saved_ticks := 0
var _body := -1
var _settings_pending := false
var _saved_steered := -1
var _loaded_hash := 0
var _loaded_ticks := -1


func _ready() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	print("drive_play_flow: executable %s, user dir %s" % [OS.get_executable_path(), OS.get_user_data_dir()])
	if FileAccess.file_exists(SLOT):
		print("drive_play_flow: %s already holds a save; not run" % ProjectSettings.globalize_path(SLOT))
		get_tree().quit(1)
		set_process(false)
		return
	_wait = SETTLE


# Chooses Korean and the fastest speed on the settings panel of the scene playing, once it is ready.
func _settings() -> void:
	_settings_pending = false
	var hud: CanvasLayer = _play.get_node("Hud")
	hud.settings.path = KEPT
	hud.settings.choose("language", 0)
	hud.settings.choose("speed", hud.settings.SPEEDS.size() - 1)


func _hud() -> CanvasLayer:
	return _play.get_node("Hud")


func _process(_delta: float) -> void:
	_frame += 1
	# The main scene, and after the title is reloaded, the scene the tree reloaded.
	var current := get_tree().current_scene
	if current != null and current != _play and is_instance_valid(current):
		_play = current
		_settings_pending = true
	if _play == null:
		return
	if _settings_pending and _play.is_node_ready():
		_settings()
	if _wait > 0:
		_wait -= 1
		return
	var hud := _hud()
	match _step:
		0:
			_shot("01_title", "screen %s, unavailable %s, slot 0 refusal %d" % [_name(hud.screen()),
					hud.title_screen.unavailable, WorldAdapter.load_refusal(0)])
			_press(KEY_ENTER)
			for digit in SEED:
				_press(KEY_0 + int(digit))
		1:
			_shot("02_new_game_seed", "screen %s, typed seed %d" % [_name(hud.screen()), hud.new_game.world_seed()])
			_press(KEY_ENTER)
		2:
			# The world runs on its own frames until DAYS_TICKS ticks have passed.
			if _play.ticks < DAYS_TICKS:
				return
			var person: int = _play.world.roster(_play.steered)[0]
			_shot("03_world_days", "seed %d, ticks %d, date %s, person %d, attainment %s, rumours %d: %s / %s / %s" % [
					_play.world_seed, _play.ticks, _play.world.date(), person, hud.labels.attainment_value.text,
					_play.world.rumours(person).size(), hud.labels.rumour_0.text, hud.labels.rumour_1.text,
					hud.labels.rumour_2.text])
			_press(KEY_ESCAPE)
		3:
			_shot("04_pause", "screen %s, chosen %s, seed shown %d, ticks %d" % [_name(hud.screen()),
					hud.pause.ENTRIES[hud.pause.chosen], hud.pause.world_seed, _play.ticks])
			_press(KEY_DOWN)
			_saved_hash = _play.world.hash()
			_saved_ticks = _play.ticks
			_saved_person = _play.world.roster(_play.steered)[0]
			_saved_steered = _play.steered
			_press(KEY_ENTER)
		4:
			_shot("05_saved", "chosen %s, notice %s, core hash before save %d, ticks %d, person %d, slot 0 refusal %d" % [
					hud.pause.ENTRIES[hud.pause.chosen], hud.pause.notice, _saved_hash, _saved_ticks,
					_saved_person, WorldAdapter.load_refusal(0)])
			_press(KEY_DOWN)
			_press(KEY_DOWN)
			_press(KEY_ENTER)
		5:
			_shot("06_title_again", "screen %s, world %s, chosen %s, unavailable %s" % [_name(hud.screen()),
					_play.world, hud.title_screen.ENTRIES[hud.title_screen.chosen], hud.title_screen.unavailable])
			_press(KEY_DOWN)
		6:
			_shot("07_continue_chosen", "chosen %s, unavailable %s" % [
					hud.title_screen.ENTRIES[hud.title_screen.chosen], hud.title_screen.unavailable])
			_press(KEY_ENTER)
			# The world as loaded, before its first tick.
			_loaded_hash = _play.world.hash()
			_loaded_ticks = _play.ticks
		7:
			# The resumed world runs RESUMED_TICKS ticks first: a few ticks after the load it draws
			# exactly what 03 drew, byte for byte.
			if _play.ticks < RESUMED_TICKS:
				return
			var person: int = _play.world.roster(_play.steered)[0]
			_shot("08_resumed", "seed %d, core hash after load %d at scene tick %d (before save %d, equal %s; slot 0 holds %d), steered %d (saved %d), person %d (saved %d), scene ticks now %d" % [
					_play.world_seed, _loaded_hash, _loaded_ticks, _saved_hash, _loaded_hash == _saved_hash,
					WorldAdapter.load(0).hash(), _play.steered, _saved_steered, person, _saved_person, _play.ticks])
			_press(KEY_G)
		8:
			_shot("09_genealogy", "visible %s, centre %d, own %d, cards %d" % [hud.genealogy.visible,
					hud.genealogy.centre, hud.genealogy.own, hud.genealogy.cards.size()])
			_press(KEY_G)
			_press(KEY_M)
			_press(KEY_ENTER)
		9:
			var person: int = _play.world.roster(_play.steered)[0]
			_shot("10_sect_applied", "visible %s, faction %d, sect %d, verdict %s, person's sect now %d" % [
					hud.sect.visible, hud.sect.faction, hud.sect.sect, hud.sect.verdict,
					_play.world.person_sect(person)])
			_press(KEY_M)
			_march()
		10:
			if not hud.rebirth.visible:
				if _play.ticks > DEATH_LIMIT:
					print("drive_play_flow: no death by tick %d" % _play.ticks)
					_failed = true
					_finish()
					return
				return
			_shot("11_rebirth", "body %d died at tick %d; past %d next %d: %s and %s" % [_body, _play.ticks,
					hud.rebirth.past, hud.rebirth.next, hud.rebirth.past_rows, hud.rebirth.next_rows])
			_press(KEY_ENTER)
		11:
			var person: int = _play.world.roster(_play.steered)[0]
			_shot("12_new_body", "screen %s, steered %d, person %d (was %d), ticks %d" % [_name(hud.screen()),
					_play.steered, person, _body, _play.ticks])
			_finish()
			return
	_step += 1
	_wait = SETTLE


# Orders the person on the nearest army's first person of the Demonic Cult (faction HOSTILE), which
# bodies of the player's Orthodox faction fight, by a left click on that cell as play.gd turns one
# into an order.
func _march() -> void:
	var world: WorldAdapter = _play.world
	_body = world.roster(_play.steered)[0]
	var own := world.army_faction(_play.steered)
	var home: Vector2 = world.person_position(_body) / WorldAdapter.CELL_SIZE
	var target := Vector2i(-1, -1)
	for army in world.army_count():
		var roster := world.roster(army)
		if army == _play.steered or roster.is_empty() or world.army_faction(army) != HOSTILE:
			continue
		var cell := Vector2i((world.person_position(roster[0]) / WorldAdapter.CELL_SIZE).floor())
		if target.x < 0 or Vector2(cell).distance_to(home) < Vector2(target).distance_to(home):
			target = cell
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	click.position = Vector2(target) * _play.PIXELS_PER_CELL + Vector2.ONE * _play.PIXELS_PER_CELL / 2.0
	print("drive_play_flow: frame %d, body %d of faction %d marches on cell %s of faction %d at tick %d; order accepted %s" % [
			_frame, _body, own, target, HOSTILE, _play.ticks, _play.command(click)])


func _finish() -> void:
	DirAccess.remove_absolute(ProjectSettings.globalize_path(SLOT))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(KEPT))
	print("drive_play_flow: %d files, none repeated %s" % [_shas.size(), not _failed])
	set_process(false)
	get_tree().quit(1 if _failed else 0)


func _press(keycode: Key) -> void:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.physical_keycode = keycode
	key.pressed = true
	get_viewport().push_input(key)


func _name(screen: Control) -> String:
	return "<none>" if screen == null else String(screen.name)


func _shot(name: String, note: String) -> void:
	var image := get_viewport().get_texture().get_image()
	var path := "%s/%s.png" % [_dir, name]
	var big_path := "%s/%s_4x.png" % [_dir, name]
	image.save_png(path)
	var big := image.duplicate()
	big.resize(image.get_width() * 4, image.get_height() * 4, Image.INTERPOLATE_NEAREST)
	big.save_png(big_path)
	var sha := FileAccess.get_sha256(path)
	var big_sha := FileAccess.get_sha256(big_path)
	for each in [sha, big_sha]:
		if _shas.has(each):
			_failed = true
		_shas[each] = name
	print("drive_play_flow: frame %d, %s.png %dx%d sha256 %s, %s_4x.png sha256 %s, locale %s; %s" % [
			_frame, name, image.get_width(), image.get_height(), sha, name, big_sha,
			TranslationServer.get_locale(), note])
