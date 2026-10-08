# Records play.tscn's faction window (#165), in each locale, at each window in RUNS:
# - hud_<run>_<locale>.png: the HUD, its leader line without "C 도전"; on a touch screen the 세력 button.
# - leader_<run>_<locale>.png: F pressed steering the Orthodox leader, war on the Unorthodox (ally)
#   lit: war offered, peace greyed with its refusal.
# - confirm_<run>_<locale>.png: then Enter (a tap on touch): war asked again.
# - declared_<run>_<locale>.png: then Enter again (yes): the war declared through the core, greyed now,
#   its basis shown.
# - member_<run>_<locale>.png: F pressed steering a Demonic Cult member: no orders, the challenge.
# - refused_<run>_<locale>.png: F then C steering an Orthodox member: the core's refusal's word.
# The world is SEED's region with five-tick days, lived YEARS years, as evidence/pr-176 does; each
# shot reopens the scene on a fresh copy of it, made the same way. The play scene's own ticks are held.
# Run, after make build, copied to the project root: godot --path . --script res://capture_factions.gd -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_factions.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true},
	{"name": "touch_2400x1080", "window": Vector2i(2400, 1080), "touch": true},
]
const VIEWS := ["hud", "leader", "confirm", "declared", "member", "refused"]
var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _at := 0
var _play: Node2D
var _world: WorldAdapter


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for run: Dictionary in RUNS:
			for view: String in VIEWS:
				_shots.append({"locale": locale, "run": run, "view": view})
	_open()


func _lived() -> WorldAdapter:
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		world.tick()
	return world


# The first army with a living lead of faction `faction` for which `leads` says whether its lead leads.
func _army(faction: int, leads: bool) -> int:
	for army in _world.army_count():
		var roster := _world.roster(army)
		if roster.is_empty() or _world.army_faction(army) != faction:
			continue
		if (roster[0] == _world.leader(faction)) == leads:
			return army
	return -1


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _process(_delta: float) -> bool:
	_frame += 1
	if _step >= 2 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if shot.run.touch else "off"))
			DisplayServer.window_set_size(shot.run.window)
			_world = _lived()
		1:
			_play._play(_world)
			_play.set_process(false)
			var orthodox := 0
			var army := _army(orthodox, true)
			if shot.view == "member":
				army = _army(1, false)
			elif shot.view == "refused":
				army = _army(orthodox, false)
			_play.steered = army
			_play._art.steered = army
			_play._body = _world.roster(army)[0]
			hud.show_player(_world, army)
			_world.tick()
			_play._art.note_tick()
			_play._process(0.0)
			hud.labels.notice.visible = false
			if shot.view != "hud":
				_play.factions(KEY_F)
			var panel: Control = hud.factions
			if shot.view in ["leader", "confirm", "declared"]:
				for n in range(1, panel.rows.size()):
					if panel.rows[n].relation == WorldAdapter.RELATION_ALLY and panel.chosen != n:
						while panel.chosen < n:
							_play.factions(KEY_DOWN)
			if shot.view in ["confirm", "declared"]:
				_play.factions(KEY_ENTER)
			if shot.view == "declared":
				_play.factions(KEY_ENTER)
			if shot.view == "refused":
				_play.factions(KEY_C)
		2:
			var panel: Control = hud.factions
			var name: String = "%s_%s_%s" % [shot.view, shot.run.name, shot.locale]
			var image := root.get_texture().get_image()
			image.save_png(_dir.path_join(name + ".png"))
			var steered_lead: int = _world.roster(_play.steered)[0]
			print("capture_factions: %s.png %dx%d, HUD %dx in %s, sha256 %s; steering army %d (person %d, faction %d, leads %s); window open %s, chosen %s, orders %s, asked %d, verdict \"%s\"" % [
				name, image.get_width(), image.get_height(), ViewScale.hud_scale(shot.run.window, shot.run.touch),
				ViewScale.hud_room(shot.run.window, shot.run.touch), FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16),
				_play.steered, steered_lead, _world.army_faction(_play.steered), panel.leads, panel.visible,
				panel.rows[panel.chosen].name if panel.visible else "-", panel.orders if panel.visible else [],
				panel.confirming, panel.verdict])
			_at += 1
			if _at >= _shots.size():
				print("capture_factions: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false
