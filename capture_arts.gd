# Records the arts screens of play.tscn in each locale, with 4x nearest-neighbour copies:
# - hud_arts_<locale>.png: the keyboard HUD, its qi against the store, its realm, and the technique
#   bar's five slots on Q W E R T holding the loadout.
# - arts_panel_<locale>.png: K's arts panel, a skill of the second art chosen with the arrows, or
#   of the first where there is no second, and put on T.
# - notice_<locale>.png: the HUD announcing a skill learned or a realm gained, as the core's events
#   record it.
# - pause_<locale>.png: the pause panel, its guide with K's row.
# - mobile_hud_arts_<locale>.png: the touch HUD, its arts button, and the arc holding the loadout.
# - arts_panel_touch_<locale>.png: the arts panel opened by the arts button, a tile tapped, the arc
#   waiting for the button to place it on.
# - arts_panel_touch_placed_<locale>.png: the same skill placed on the fourth button.
# The world is SEED's region with days of five ticks, lived YEARS years, steering the army of the
# first person who knows skills of two arts, or of one where none does. The play scene's own
# ticks are held, so it steers that army: the capture ticks the world itself. The language and the
# touch row are chosen on the settings panel, kept in KEPT, removed at the end. Prints what each
# shows and fails if two images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const NOTICE_LIMIT := 20000
const LOCALES := ["ko", "en"]
const KEPT := "user://capture_arts.cfg"
var _dir := "."
var _frame := 0
var _step := 0
var _locale := 0
var _play: Node2D
var _saved := {}
var _failed := false
var _fresh := false
var _army := -1


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
	var best := -1
	for army in world.army_count():
		var roster := world.roster(army)
		if roster.is_empty():
			continue
		var arts := {}
		for skill: Dictionary in world.person_skills(roster[0]):
			if skill.art >= 0:
				arts[skill.art] = true
		if arts.size() > best:
			best = arts.size()
			_army = army
	_play.steered = _army
	_play._art.steered = _army
	_play._body = world.roster(_army)[0]
	hud.show_player(world, _army)
	print("capture_arts: lived %d years; steering army %d, person %d, knowing %d arts, %d skills, realm %d, qi %d of %d" % [
		YEARS, _army, world.roster(_army)[0], best, world.person_skills(world.roster(_army)[0]).size(),
		world.person_realm(world.roster(_army)[0]), world.person_qi(world.roster(_army)[0]),
		world.person_qi_capacity(world.roster(_army)[0])])


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
	var world: WorldAdapter = _play.world
	var person: int = world.roster(_army)[0]
	match _step:
		0:
			hud.labels.notice.visible = false
		1:
			var slots := []
			for skill: Dictionary in hud.slot_skills:
				slots.append(skill.get("romanised", "army" if not skill.is_empty() else "-"))
			_shot("hud_arts_" + locale, "qi %s, realm %s, slots %s" % [hud.labels.qi_value.text,
					hud.labels.realm_value.text, slots])
			_play.command(_key(KEY_K))
			# Down to the second art's first known skill, or the first art skill where there is no
			# second art.
			var known: Array = hud.arts._known_skills()
			var target := 0
			for n in known.size():
				if known[n].art >= 0 and known[n].art != known[0].art:
					target = n
					break
			for n in target:
				_play.command(_key(KEY_DOWN))
			_play.command(_key(KEY_T))
		2:
			_shot("arts_panel_" + locale, "selected %s, just %d, slots %s" % [hud.arts.selected, hud.arts.just,
					hud.loadout.slots])
			_play.command(_key(KEY_K))
			var ticked := 0
			while hud.waiting_notices().is_empty() and ticked < NOTICE_LIMIT:
				world.tick()
				hud.refresh(world, _army, PackedInt64Array([0]))
				ticked += 1
			print("capture_arts: ticked %d for a notice: %s" % [ticked, hud.waiting_notices()])
			hud.labels.notice.visible = false
		3:
			_shot("notice_" + locale, "notice \"%s\" shown %s" % [hud.labels.notice.text, hud.labels.notice.visible])
			_play.pause()
		4:
			_shot("pause_" + locale, "%d controls in the guide" % hud.pause.controls.size())
			hud.pause.close()
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on"))
			hud.labels.notice.visible = false
		5:
			_shot("mobile_hud_arts_" + locale, "touch shown %s, %d buttons, slots %s" % [hud.touch.visible,
					hud.touch.skill_count, hud.touch.reach.map(func(r: Dictionary) -> bool: return not r.is_empty())])
			_play.touch(_finger(0, hud.touch.BUTTONS.arts.get_center(), true))
			_play.touch(_finger(0, hud.touch.BUTTONS.arts.get_center(), false))
			var tiles: Array = hud.arts._tiles()
			var tapped: Vector2 = tiles[0].rect.get_center()
			for tile: Dictionary in tiles:
				if tile.skill.known and hud.loadout.slot_of(tile.skill.held) < 0:
					tapped = tile.rect.get_center()
					break
			_play.touch(_finger(1, tapped, true))
			_play.touch(_finger(1, tapped, false))
		6:
			_shot("arts_panel_touch_" + locale, "panel open %s, placing %s, waiting %s, selected %s" % [
					hud.arts.visible, hud.touch.placing, hud.touch.waiting, hud.arts.selected])
			_play.touch(_finger(2, hud.touch.SKILLS[3].at, true))
		7:
			_shot("arts_panel_touch_placed_" + locale, "slots %s, just %d" % [hud.loadout.slots, hud.arts.just])
			_play.touch(_finger(2, hud.touch.PAUSE.get_center(), true))
		8:
			_step = 0
			_locale += 1
			if _locale >= LOCALES.size():
				DirAccess.remove_absolute(KEPT)
				print("capture_arts: %d images, none repeated %s" % [_saved.size(), not _failed])
				quit(1 if _failed else 0)
				return true
			_open()
			return false
	_step += 1
	return false


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _finger(index: int, at: Vector2, pressed: bool) -> InputEventScreenTouch:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	return event


func _shot(name: String, note: String) -> void:
	var image := root.get_texture().get_image()
	var sha := image.get_data().hex_encode().sha256_text().left(16)
	if _saved.has(sha):
		_failed = true
	_saved[sha] = name
	image.save_png("%s/%s.png" % [_dir, name])
	var big := image.duplicate()
	big.resize(image.get_width() * 4, image.get_height() * 4, Image.INTERPOLATE_NEAREST)
	big.save_png("%s/%s_4x.png" % [_dir, name])
	print("capture_arts: %s.png %dx%d, sha256 %s, locale %s; %s" % [name, image.get_width(),
			image.get_height(), sha, TranslationServer.get_locale(), note])
