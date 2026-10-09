## Opens on the title screen, where a new game generates a region from a seed, typed or drawn at
## random, and continue resumes the world saved in SAVE_SLOT, and lets the player steer one person
## in it, through the same orders every army follows: a right click orders it toward the cell
## clicked, and while the button is held, toward the cell under the cursor; keys 1 to 9 order its
## automatic strike to use that skill of its army's, and 0 all of them. Q, W, E, R and T cast the
## skills on the HUD's loadout's five slots at the cursor through the core's cast port, at once or,
## as the settings choose, on the key's release after aiming while it is held; the same key again
## or Escape cancels the aim. G opens the genealogy panel on the person, M the sect panel on its
## sect, where Enter applies to the sect shown, K the arts panel, where the arrows choose one
## of its skills, Q to T put it on that slot and Delete takes it off its slot, and F the faction
## window, where the arrows choose a faction and an order, Enter gives a leader's order, war asked
## again first, through the core's declare port, and C challenges the faction's leader to a duel,
## with the same order every army follows, when the core allows it. Space talks to the person the
## HUD's prompt names, the nearest within the core's talk reach, in the dialogue window, where the
## arrows choose what to ask, Enter asks it and Escape goes back or ends the talk; a person the core
## says is hostile refuses, as the prompt shows, and the window closes once the core refuses the
## talk. The loadout is
## written beside the save whenever the world is saved, and read with it. The person is the one
## the core says carries the player's soul,
## found again after a rebirth or a load; when it dies and its soul is born again, the rebirth
## screen sums up the life just ended. Escape pauses the world under the pause panel, which saves
## it and whose guide shows the controls as bound here; it is also saved every AUTOSAVE_DAYS days.
## On a PC, the HUD's shortcut rail lists SHORTCUTS, and a left click on a row does what its key
## does, or for the settings, which no key opens, opens them.
## On a touch screen, or where the settings force it, the HUD's touch controls steer it instead:
## the joystick orders it the way it points, a skill button tapped casts its slot's skill at the
## hostile person the core's aim finds the way the person faces, or toward that way at the skill's
## range, dragged casts at the point it aims, dropped on the cancel zone casts nothing and held
## casts on the person itself, the HUD naming the core's reason when it refuses; the HUD's buttons
## do what SHORTCUTS' keys for them do, opening the genealogy, sect and arts panels and the faction
## window, and pausing, where on the arts panel a skill
## tapped and then a skill button puts it on that button's slot; the screens take taps, and where
## the settings ask, a tap on the ground orders the person there.
## The camera follows the person, at the whole-pixel scale ViewScale gives the window, filling it;
## the settings open over the paused world. It ticks at the core's tick rate times the speed the settings
## choose, draws the world through world_art.tscn and the HUD and its screens through hud.tscn,
## sounds it through sound.tscn, and holds no game rule.
extends Node2D

const ViewScale := preload("res://view_scale.gd")

const PIXELS_PER_CELL := 16.0
# Ticks one frame may run to catch up at the core's own rate, and as many times more at a higher
# speed; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4
## The button that orders the person toward the cell under the cursor, and keeps doing so while held.
const MOVE_BUTTON := MOUSE_BUTTON_RIGHT
## How many cells ahead the joystick sets the objective; nearer cells are tried when the core
## refuses it.
const STRIDE := 3
## The keys that cast the skill on one of the loadout's slots each at the cursor, the first slot's
## first; on the arts panel, they put the skill chosen on that slot.
const CAST_KEYS := [KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T]
## The keys that order the automatic strike to use one of the army's skills each, the first skill's
## first, and the key that orders all of them.
const SKILL_KEYS := [KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9]
const ALL_SKILLS_KEY := KEY_0
const CHALLENGE_KEY := KEY_C
const GENEALOGY_KEY := KEY_G
const SECT_KEY := KEY_M
const ARTS_KEY := KEY_K
const FACTIONS_KEY := KEY_F
## The keys that open a panel over the world and close it again, by the HUD's name for the panel
## each opens, which the touch controls' button for it bears too.
const PANEL_KEYS := {GENEALOGY_KEY: "genealogy", SECT_KEY: "sect", ARTS_KEY: "arts", FACTIONS_KEY: "factions"}
## The key that talks to the person the prompt names.
const TALK_KEY := KEY_SPACE
## The keys that consume the wares, by their index in WorldAdapter.wares.
const WARE_KEYS := [KEY_Z, KEY_X]
## The key that takes the skill chosen on the arts panel off its slot.
const CLEAR_KEY := KEY_DELETE
const PAUSE_KEY := KEY_ESCAPE
## What the HUD's shortcut rail lists, in its order, and the pause panel's guide lists after the
## other controls, the guide only those with a key: each row the `action`, the HUD's name for the
## panel or screen it opens, which a touch button for it bears too; its `key`, or KEY_NONE for the
## settings, which no key opens; its `icon`, a key of the HUD's shortcut icons; and its `label`, the
## hud.csv key of what it does.
const SHORTCUTS: Array[Dictionary] = [
	{"action": "genealogy", "key": GENEALOGY_KEY, "icon": "genealogy", "label": "CONTROLS_GENEALOGY"},
	{"action": "sect", "key": SECT_KEY, "icon": "sect", "label": "CONTROLS_SECTS"},
	{"action": "factions", "key": FACTIONS_KEY, "icon": "factions", "label": "CONTROLS_FACTIONS"},
	{"action": "arts", "key": ARTS_KEY, "icon": "arts", "label": "CONTROLS_ARTS"},
	{"action": "settings", "key": KEY_NONE, "icon": "settings", "label": "SETTINGS_TITLE"},
	{"action": "pause", "key": PAUSE_KEY, "icon": "pause", "label": "CONTROLS_PAUSE_CANCEL"},
]
## The slot the pause panel saves to, the autosave writes and continue loads.
const SAVE_SLOT := 0
## The in-game days between autosaves.
const AUTOSAVE_DAYS := 3
## The file, in the saves' directory, the loadout is kept in beside SAVE_SLOT.
const LOADOUT_FILE := "loadout_%d.cfg" % SAVE_SLOT

## The directory SAVE_SLOT is kept in, set before the scene is ready; tests keep their own.
var save_directory := WorldAdapter.saves_directory()
## The village the player is born into, set before a game starts.
var birth_village := 0
## The seed the world was generated from.
var world_seed := 0
## The world being played, or null on the title screen.
var world: WorldAdapter
var steered := 0
var ticks := 0

var _objective := Vector2i.ZERO
var _skills := PackedInt64Array()
var _since_tick := 0.0
# Where the steered person last stood, kept for the camera once it has died.
var _last_lead := Vector2.ZERO
# Where the cursor is, in this node's pixels, and whether the move button is held, so the person
# follows it.
var _cursor := Vector2.ZERO
var _moving := false
# The cast key being held to aim, or KEY_NONE.
var _aiming := KEY_NONE
# The person last steered, whose death the rebirth screen sums up once its soul is born again.
var _body := -1
# The date at the last tick, and the days that have begun since the world was last saved.
var _date := {}
var _unsaved_days := 0
## The way the person last moved, or was last steered, as a facing for a tapped skill's aim.
var facing := Vector2.DOWN
## What the last cast the core accepted was: its `skill` and its `target`, as cast() took it.
var last_cast := {}
# The way the joystick orders the person, and the cell it was last ordered from that way.
var _stick := Vector2i.ZERO
var _stick_from := Vector2i.ZERO
# Where the view's top-left corner is in the world, in pixels, for a tap on the ground.
var _view_origin := Vector2.ZERO
# Whether the settings were opened from the pause panel, which going back from them returns to.
var _settings_from_pause := false
## The person the talk prompt names, or -1 for none, and whether the core refuses the talk for
## hostility, as last read.
var talk_to := -1
var talk_hostile := false
# The tick the person to talk to was last read at.
var _talk_read_at := -1

@onready var _camera: Camera2D = $Camera
@onready var _art: Node2D = $WorldArt
@onready var _hud: CanvasLayer = $Hud
@onready var _sound: Node2D = $Sound
@onready var _aim: Node2D = $Aim


func _ready() -> void:
	_hud.open_title(WorldAdapter.load_refusal(SAVE_SLOT, save_directory) != WorldAdapter.LOAD_MISSING)
	_hud.settings.applied.connect(fit_view)
	_hud.settings.applied.connect(show_numbers)
	_hud.show_shortcuts(SHORTCUTS)
	for keycode: Key in PANEL_KEYS:
		_hud.get(PANEL_KEYS[keycode]).toggle_key = keycode
	_hud.arts.slot_keys = CAST_KEYS
	_hud.arts.clear_key = CLEAR_KEY
	_hud.factions.challenge_key = CHALLENGE_KEY
	# Outside a tree, as a test readies it, the scene has no window to fit.
	if is_inside_tree():
		get_viewport().size_changed.connect(fit_view)
	_hud.settings.read()
	_hud.settings.apply()


## Scales the world and the HUD to the window, as ViewScale gives them for its size and for the
## touch controls shown or not.
func fit_view() -> void:
	if not is_inside_tree():
		return
	var window := get_viewport().get_visible_rect().size
	var touch_shown: bool = _hud.settings.touch_shown()
	var scale := ViewScale.world_scale(Vector2i(window), touch_shown)
	_camera.zoom = Vector2(scale, scale)
	_hud.fit(Vector2i(window), touch_shown)


## Shows the damage the struck take as numbers over them, or not, as the settings choose.
func show_numbers() -> void:
	_art.get_node("Combat").damage_numbers = _hud.settings.damage_numbers()


## Opens the settings over the world, which stays paused while they are open; going back from
## them returns to the world.
func open_settings() -> void:
	if world == null or _hud.screen() != null:
		return
	_moving = false
	_stick = Vector2i.ZERO
	_hud.touch.release_all()
	_settings_from_pause = false
	_hud.settings.open()
	_sound.ui("open")


## Starts a world generated from `from_seed`, the player born into `birth_village`, and closes
## the title screen. Returns whether the core made the world.
func start(from_seed: int) -> bool:
	var made := WorldAdapter.create_region(from_seed, birth_village)
	if made == null:
		return false
	_play(made)
	return true


## Resumes the world saved in SAVE_SLOT, with the loadout kept beside it, and closes the title
## screen, or shows on it why the save was not loaded. Returns whether it was loaded.
func resume_saved() -> bool:
	var loaded := WorldAdapter.load(SAVE_SLOT, save_directory)
	if loaded == null:
		_hud.title_screen.refuse(WorldAdapter.load_refusal(SAVE_SLOT, save_directory))
		return false
	_hud.loadout.read(save_directory.path_join(LOADOUT_FILE))
	_play(loaded)
	return true


## Saves the world in SAVE_SLOT, and the loadout beside it, unless no body carries the player's
## soul, since a world loaded then would have no person to steer. Returns whether both were
## written.
func save() -> bool:
	if world.player_army() < 0:
		return false
	_unsaved_days = 0
	return world.save(SAVE_SLOT, save_directory) and _hud.loadout.write(save_directory.path_join(LOADOUT_FILE))


# Plays `made`, steering the person carrying the player's soul, and closes the title screen.
func _play(made: WorldAdapter) -> void:
	world = made
	world_seed = world.region_seed()
	steered = world.player_army()
	_date = world.date()
	_objective = Vector2i((lead() / WorldAdapter.CELL_SIZE).floor())
	_skills = _all_skills()
	_camera.position = _to_pixels(lead()).floor()
	_art.show_world(world, steered)
	_sound.show_world(world)
	_hud.show_player(world, steered)
	_hud.purse.keys.clear()
	for keycode: Key in WARE_KEYS:
		_hud.purse.keys.append(OS.get_keycode_string(keycode))
	_body = world.roster(steered)[0]
	_art.region_changed.connect(_hud.show_map.bind(world))
	# The camera stops at the region's edge rather than showing past it.
	var rows := world.map_rows()
	_camera.limit_left = 0
	_camera.limit_top = 0
	_camera.limit_right = int(rows[0].length() * PIXELS_PER_CELL)
	_camera.limit_bottom = int(rows.size() * PIXELS_PER_CELL)
	_hud.title_screen.close()
	_hud.new_game.close()


func _unhandled_input(event: InputEvent) -> void:
	var on_hud: InputEvent = _hud.to_hud(event)
	if not navigate(on_hud) and not touch(on_hud) and not click(on_hud):
		command(make_input_local(event))


## Works the screens with `event`: the screen open on top takes every key and every tap, and while
## none is open, Escape pauses the world unless a panel is open for it to close. A screen opening
## sounds, and so does a key or a tap a screen takes. Returns whether a screen took the event.
func navigate(event: InputEvent) -> bool:
	var key := event as InputEventKey
	var tapped := event as InputEventScreenTouch
	var open: Control = _hud.screen()
	if open == null:
		if key == null or not key.pressed or key.keycode != PAUSE_KEY or _hud.panel() != null or _aiming != KEY_NONE:
			return false
		pause()
		return true
	if key != null and key.pressed:
		_take(open.key(key))
	elif tapped != null and tapped.pressed:
		_take(open.tap(tapped.position - open.position))
	else:
		return true
	var now: Control = _hud.screen()
	_sound.ui("open" if now != null and now != open else "click")
	return true


## Works the HUD's shortcut rail with `event`, its position in the HUD's pixels, while a world is
## played and the rail is shown: the cursor marks the row under it, and a left click on a row does
## what shortcut() does with its action. Returns whether a click took a row.
func click(event: InputEvent) -> bool:
	var rail: Control = _hud.shortcuts
	if world == null or not rail.visible:
		return false
	var motion := event as InputEventMouseMotion
	if motion != null:
		rail.hover(motion.position - rail.position)
		return false
	var mouse := event as InputEventMouseButton
	if mouse == null or not mouse.pressed or mouse.button_index != MOUSE_BUTTON_LEFT:
		return false
	var action: String = rail.tap(mouse.position - rail.position)
	if action.is_empty():
		return false
	shortcut(action)
	return true


## Does what the row of SHORTCUTS for `action` does, as the shortcut rail or a touch button asks:
## presses its key, taken as a key pressed on the keyboard is, or opens the settings, which no key
## opens.
func shortcut(action: String) -> void:
	for row: Dictionary in SHORTCUTS:
		if row.action != action:
			continue
		if row.key == KEY_NONE:
			open_settings()
			return
		var press := InputEventKey.new()
		press.keycode = row.key
		press.pressed = true
		if not navigate(press):
			command(press)
		return


## Pauses the world under the pause panel, letting go of the move button and every finger held.
func pause() -> void:
	_moving = false
	_stick = Vector2i.ZERO
	_hud.touch.release_all()
	_hud.pause.open(controls(), world_seed)
	_sound.ui("open")


# Does what the screen or the panel open asks of a key or a tap: one of the words their key() and
# tap() return. Returns whether it did anything.
func _take(action: String) -> bool:
	match action:
		"":
			return false
		"new":
			_hud.new_game.open()
		"start":
			start(_hud.new_game.world_seed())
		"continue":
			resume_saved()
		"save":
			_hud.pause.note_save(save())
		"settings":
			_hud.pause.close()
			_settings_from_pause = true
			_hud.settings.open()
		"back":
			# Back from the settings to the pause panel, where they were opened from it in a game.
			if world != null and _settings_from_pause:
				_hud.pause.visible = true
		"resume":
			_hud.pause.close()
		"go_on":
			_hud.rebirth.close()
		"title":
			get_tree().reload_current_scene()
		"quit":
			get_tree().quit()
		"close":
			_hud.panel().close()
			_hud.show_placing()
		"placed":
			_hud.show_loadout(world)
		"apply":
			return apply_to_sect() >= 0
		"order":
			declare(_hud.factions.ask())
		"yes":
			declare(_hud.factions.answer(true))
		"no":
			_hud.factions.answer(false)
		"challenge":
			return challenge() == WorldAdapter.CHALLENGE_NONE
		"buy":
			return buy()
		"wares":
			var roster := world.roster(steered)
			if roster.is_empty():
				_hud.dialogue.close()
			else:
				_hud.dialogue.show_wares(roster[0])
	return true


## Returns the controls as this scene binds them, for the pause panel's guide: each a Dictionary
## of its `keys`, the words on its caps, and `does`, the hud.csv key of what it does; the rows of
## SHORTCUTS with a key last, in their order.
func controls() -> Array[Dictionary]:
	var casts := ""
	for keycode: Key in CAST_KEYS:
		casts += OS.get_keycode_string(keycode)
	var skills := "%s-%s" % [OS.get_keycode_string(SKILL_KEYS.front()), OS.get_keycode_string(SKILL_KEYS.back())]
	var wares := ""
	for keycode: Key in WARE_KEYS:
		wares += OS.get_keycode_string(keycode)
	var rows: Array[Dictionary] = [
		{"keys": ["CONTROLS_CLICK"], "does": "CONTROLS_CLICK_MOVE"},
		{"keys": [casts], "does": "CONTROLS_CAST"},
		{"keys": [skills], "does": "CONTROLS_SKILL"},
		{"keys": [OS.get_keycode_string(ALL_SKILLS_KEY)], "does": "CONTROLS_ALL_SKILLS"},
		{"keys": [OS.get_keycode_string(TALK_KEY)], "does": "CONTROLS_TALK"},
		{"keys": [wares], "does": "CONTROLS_WARES"},
	]
	for row: Dictionary in SHORTCUTS:
		if row.key != KEY_NONE:
			rows.append({"keys": [_hud.key_word(row.key)], "does": row.label})
	return rows


func _process(delta: float) -> void:
	advance(delta)
	if world == null or _hud.screen() != null:
		return
	_camera.position = drawn_lead().round()
	var screen := get_viewport_rect().size / _camera.zoom
	var view := Rect2(_camera.get_screen_center_position() - screen / 2.0, screen)
	_art.refresh(view, _between())
	_sound.hearing = view
	# The cursor stays on the screen while the camera moves, so it moves over the world.
	_cursor = get_local_mouse_position()
	if _moving:
		follow(_cursor)
	_show_aim()
	_view_origin = view.position
	_hud.touch.body_at = _hud.from_window((drawn_lead() - view.position) * _camera.zoom).round()
	_hud.refresh(world, steered, _skills)
	show_talk()
	show_names()


## Turns `event`, its position in this node's pixels, into an order for the steered person, keeping
## the part of its last order the event does not change. Returns whether it gave an order and the
## core accepted it.
func command(event: InputEvent) -> bool:
	if world == null:
		return false
	var objective := _objective
	var skills := _skills
	var mouse := event as InputEventMouseButton
	var motion := event as InputEventMouseMotion
	var key := event as InputEventKey
	# While the touch controls are shown, a finger gives the orders, and the click a touch is also
	# reported as gives none.
	if mouse != null and _hud.touch.visible:
		return false
	if mouse != null:
		_cursor = mouse.position
	elif motion != null:
		_cursor = motion.position
		_show_aim()
	if mouse != null and mouse.button_index == MOVE_BUTTON:
		_moving = mouse.pressed
		return mouse.pressed and follow(_cursor)
	elif motion != null:
		return _moving and follow(_cursor)
	elif key != null and key.pressed and _hud.panel() != null:
		return _take(_hud.panel().key(key))
	elif key != null and key.pressed and (PANEL_KEYS.has(key.keycode) or key.keycode == TALK_KEY):
		return open_panel(key.keycode)
	elif key != null and key.pressed and not key.echo and WARE_KEYS.has(key.keycode):
		return consume(WARE_KEYS.find(key.keycode))
	elif key != null and CAST_KEYS.has(key.keycode) and not key.echo:
		var cast := quick_cast(key, _cursor)
		_show_aim()
		return cast
	elif key != null and key.pressed and key.keycode == PAUSE_KEY and _aiming != KEY_NONE:
		_aiming = KEY_NONE
		_show_aim()
		return false
	elif key != null and key.pressed and (SKILL_KEYS.has(key.keycode) or key.keycode == ALL_SKILLS_KEY):
		skills = _all_skills() if key.keycode == ALL_SKILLS_KEY else PackedInt64Array([SKILL_KEYS.find(key.keycode)])
	else:
		return false
	if not world.order(steered, objective, skills):
		return false
	_objective = objective
	_skills = skills
	return true


## Orders the person toward the cell under `at`, a position in this node's pixels, keeping its
## skills, unless it is ordered there already. Returns whether the core accepted an order.
func follow(at: Vector2) -> bool:
	var objective := Vector2i((at / PIXELS_PER_CELL).floor())
	if objective == _objective:
		return false
	if not world.order(steered, objective, _skills):
		return false
	_objective = objective
	return true


## Works cast key `key`, pressed or released, with the cursor at `at`, a position in this node's
## pixels. Cast on press, a press casts its skill there at once. Cast on release, a press aims it
## and the release casts it there, unless the same key pressed again or Escape cancelled the aim.
## Returns whether a cast was given and the core accepted it.
func quick_cast(key: InputEventKey, at: Vector2) -> bool:
	if not _hud.settings.cast_on_release():
		return key.pressed and cast_at(CAST_KEYS.find(key.keycode), at) == WorldAdapter.CAST_NONE
	if key.pressed:
		_aiming = KEY_NONE if _aiming == key.keycode else key.keycode
		return false
	if _aiming != key.keycode:
		return false
	_aiming = KEY_NONE
	return cast_at(CAST_KEYS.find(key.keycode), at) == WorldAdapter.CAST_NONE


## Returns the cast key held to aim, or KEY_NONE while none is.
func aiming() -> Key:
	return _aiming


## Has the steered person cast the skill on the loadout's slot `slot` at `at`, a position in this
## node's pixels, as cast() does. Returns its verdict, CAST_NO_SKILL for an empty slot.
func cast_at(slot: int, at: Vector2) -> int:
	var skill: int = _hud.loadout.skill(world, slot)
	if skill < 0:
		_hud.show_cast(WorldAdapter.CAST_NO_SKILL)
		return WorldAdapter.CAST_NO_SKILL
	return cast(skill, at / PIXELS_PER_CELL * WorldAdapter.CELL_SIZE)


# Shows the aimed skill's range round the person and its area at the cursor while a cast key is held
# to aim, and hides it otherwise.
func _show_aim() -> void:
	var roster := world.roster(steered)
	var skill: int = _hud.loadout.skill(world, CAST_KEYS.find(_aiming)) if _aiming != KEY_NONE else -1
	if skill < 0 or roster.is_empty():
		_aim.visible = false
		return
	var skills := world.person_skills(roster[0])
	var reach: float = skills[skill]["range"] * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	var radius: float = skills[skill]["radius"] * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	_aim.show_aim(drawn_lead(), reach, _cursor, radius)


# Orders the person STRIDE cells `way`, or as many fewer as the core accepts, or, with no way, to
# stop where it stands, keeping its skills, and faces it that way. Returns whether the core
# accepted an order.
func _steer(way: Vector2i) -> bool:
	if way != Vector2i.ZERO:
		facing = Vector2(way).normalized()
	var here := Vector2i((lead() / WorldAdapter.CELL_SIZE).floor())
	_stick_from = here
	# With no way, the one objective is where the person stands.
	var strides: Array = [0] if way == Vector2i.ZERO else range(STRIDE, 0, -1)
	for stride: int in strides:
		var objective := here + way * stride
		if world.order(steered, objective, _skills):
			_objective = objective
			return true
	return false


## Works the HUD's touch controls with `event`, a touch or a drag, while they are shown and a world
## is played: the joystick steers the person the nearest of the eight ways it points, a skill button
## casts as cast_skill() does, the HUD's buttons do what shortcut() does with their names, opening
## the genealogy, sect or arts panel or the faction window, or pausing, and where the settings ask, a tap on the ground orders the person to
## that cell. While a panel is open, a tap works it as its tap() reads it; while the arts panel is,
## a tap on a tile chooses its skill, a skill button tapped then puts it on that button's slot, a
## finger moved up or down scrolls the tiles and the close button closes it. Returns whether the
## touch controls took the event.
func touch(event: InputEvent) -> bool:
	if world == null or not _hud.touch.visible or not (event is InputEventScreenTouch or event is InputEventScreenDrag):
		return false
	var panel: Control = _hud.panel()
	if panel == _hud.arts:
		_place(_hud.touch.take(event))
		return true
	if panel != null:
		var pressed := event as InputEventScreenTouch
		if pressed != null and pressed.pressed:
			_take(panel.tap(pressed.position - panel.position))
		return true
	var action: Dictionary = _hud.touch.take(event)
	match action.get("do", ""):
		"steer":
			var way: Vector2 = action.way
			var nearest := Vector2i(roundi(way.normalized().x), roundi(way.normalized().y)) if way != Vector2.ZERO else Vector2i.ZERO
			if way != Vector2.ZERO:
				facing = way.normalized()
			if nearest != _stick:
				_stick = nearest
				_steer(nearest)
		"cast":
			cast_skill(action.slot, action.how, action.get("aim", Vector2.ZERO))
		"button":
			shortcut(action.button)
		"world":
			if _hud.settings.tap_to_move():
				var on_view: Vector2 = _hud.to_window(action.at) / _camera.zoom
				var cell := Vector2i(((_view_origin + on_view) / PIXELS_PER_CELL).floor())
				if world.order(steered, cell, _skills):
					_objective = cell
	return true


## Has the steered person cast the skill on the loadout's slot `slot`, as the touch controls ask:
## `how` "tap" casts at the hostile person the core's aim finds the way the person faces, or with
## none, at the point that way at the skill's range; "point" at the point `aim`, a share of the
## skill's range each way, from the person; and "self" at the person itself. The core brings a point
## past the range within it. When the core refuses, or the slot is empty, the HUD names the reason.
## Returns the core's verdict, one of the WorldAdapter.CAST_* constants, of which CAST_NONE means
## the cast was made, and CAST_NO_SKILL for an empty slot; or -1 while the person has no body.
func cast_skill(slot: int, how: String, aim := Vector2.ZERO) -> int:
	var roster := world.roster(steered)
	if roster.is_empty():
		return -1
	var person := roster[0]
	var skill: int = _hud.loadout.skill(world, slot)
	if skill < 0:
		_hud.show_cast(WorldAdapter.CAST_NO_SKILL)
		return WorldAdapter.CAST_NO_SKILL
	var reach := float(world.person_skills(person)[skill].range)
	var at := world.person_position(person)
	var target: Variant = null
	match how:
		"tap":
			# The core reads a facing in whole sub-cell units, so it is given a cell long.
			var aimed := world.aim(person, skill, (facing.normalized() * WorldAdapter.CELL_SIZE).round())
			target = aimed if aimed >= 0 else (at + facing.normalized() * reach).floor()
		"point":
			target = (at + aim * reach).floor()
	return cast(skill, target)


## Has the steered person cast skill `skill` at `target`, a point in sub-cell units, a person or,
## with null, itself, and shows on the HUD why the core refuses it, if it does. Every cast, from a
## key or a skill button, is given here. Returns the core's verdict, one of the WorldAdapter.CAST_*
## constants, of which CAST_NONE means the cast was made; or -1 while the person has no body.
func cast(skill: int, target: Variant) -> int:
	var roster := world.roster(steered)
	if roster.is_empty():
		return -1
	var refusal := world.cast_refusal(roster[0], skill, target)
	if refusal != WorldAdapter.CAST_NONE:
		_hud.show_cast(refusal)
	elif world.cast(roster[0], skill, target):
		last_cast = {"skill": skill, "target": target}
	return refusal


## Returns the person the steered person would talk to, the nearest living one the core lets it
## talk to or refuses only for hostility, with whether it refuses: {person, hostile}, or a person of
## -1 for none.
func talk_target() -> Dictionary:
	var roster := world.roster(steered)
	var found := {"person": -1, "hostile": false}
	if roster.is_empty():
		return found
	var speaker := roster[0]
	var here := world.person_position(speaker)
	var nearest := INF
	for army in world.army_count():
		for person in world.roster(army):
			var refusal := world.talk_refusal(speaker, person)
			if refusal != WorldAdapter.TALK_NONE and refusal != WorldAdapter.TALK_HOSTILE:
				continue
			var apart := here.distance_squared_to(world.person_position(person))
			if apart < nearest:
				nearest = apart
				found = {"person": person, "hostile": refusal == WorldAdapter.TALK_HOSTILE}
	return found


## Reads whom the steered person may talk to once a tick, closes the dialogue window once the core
## refuses that talk, and shows the prompt over the person to talk to while no panel is open and the
## touch controls are not shown, whose talk has no button yet.
func show_talk() -> void:
	if world.tick_count() != _talk_read_at:
		_talk_read_at = world.tick_count()
		var target := talk_target()
		talk_to = target.person
		talk_hostile = target.hostile
		var roster := world.roster(steered)
		if _hud.dialogue.visible and (roster.is_empty()
				or world.talk_refusal(roster[0], _hud.dialogue.person) != WorldAdapter.TALK_NONE):
			_hud.dialogue.close()
	var hidden: bool = talk_to < 0 or _hud.touch.visible or _hud.panel() != null
	if hidden:
		_hud.talk_prompt.point_at(world, -1, Vector2.ZERO, false)
		return
	# world_art.gd draws a body from 16 pixels above its position.
	var head := _to_pixels(world.person_position(talk_to)) - Vector2(0, 17)
	_hud.talk_prompt.key = OS.get_keycode_string(TALK_KEY)
	_hud.talk_prompt.point_at(world, talk_to, _hud.from_window((head - _view_origin) * _camera.zoom).floor(),
			talk_hostile)


## Names the people world_art.gd finds within its reach of the steered person over their heads,
## while no panel is open; the HUD keeps the talk prompt's slip, over the head of the one it names,
## clear of names.
func show_names() -> void:
	var people: Array[Dictionary] = []
	if _hud.panel() == null:
		for person: Dictionary in _art.near:
			people.append({"person": person.person, "relation": person.relation,
				"head": _hud.from_window((person.head - _view_origin) * _camera.zoom).floor()})
	_hud.show_names(world, people)


## Opens the panel `keycode` opens over the world, on the steered person while it has a body: the
## genealogy panel; the sect panel while the world has sects; the arts panel; the faction window; or
## the dialogue window, on the person the prompt names when the core lets the steered person talk to
## it. While a panel is open, it takes every key, and no key gives an order or casts. Returns
## whether it opened.
func open_panel(keycode: Key) -> bool:
	var roster := world.roster(steered)
	if roster.is_empty():
		return false
	match keycode:
		GENEALOGY_KEY:
			_hud.genealogy.show_person(world, roster[0], roster[0])
		SECT_KEY:
			if world.sect_count() == 0:
				return false
			_hud.sect.show_sect(world, steered)
		ARTS_KEY:
			_hud.arts.show_arts(world, steered, _hud.loadout, _hud.touch.visible)
			_hud.show_placing()
		FACTIONS_KEY:
			_hud.factions.show_factions(world, steered, _hud.touch.visible)
		TALK_KEY:
			if talk_to < 0 or world.talk_refusal(roster[0], talk_to) != WorldAdapter.TALK_NONE:
				return false
			_hud.dialogue.open(world, talk_to)
		_:
			return false
	_sound.ui("open")
	return true


## Has the steered person buy the ware chosen in the dialogue window from the person it talks to,
## as the core allows, and shows in the window what came of it. Returns whether it bought.
func buy() -> bool:
	var panel: Control = _hud.dialogue
	var refusal := world.buy_refusal(panel.buyer, panel.person, panel.ware)
	if refusal == WorldAdapter.BUY_NONE:
		world.buy(panel.buyer, panel.person, panel.ware)
		_hud.purse.read(world, panel.buyer)
	panel.show_bought(refusal)
	return refusal == WorldAdapter.BUY_NONE


## Has the steered person consume one of ware `ware`, by its index in WorldAdapter.wares, as the
## core allows, and notices what came of it. Returns whether it consumed one.
func consume(ware: int) -> bool:
	var roster := world.roster(steered)
	if roster.is_empty() or ware >= world.wares().size():
		return false
	var refusal := world.consume_refusal(roster[0], ware)
	if refusal == WorldAdapter.CONSUME_NONE:
		world.consume(roster[0], ware)
		_hud.purse.read(world, roster[0])
	_hud.show_consume(world, ware, refusal)
	return refusal == WorldAdapter.CONSUME_NONE


## Has the steered person challenge its faction's leader, marching on the leader with its current
## skills. A duel begun closes the faction window and the HUD announces it; a refusal shows its word
## in the window. Returns the core's verdict, one of the WorldAdapter.CHALLENGE_* constants;
## CHALLENGE_NONE means the duel began.
func challenge() -> int:
	var faction := world.army_faction(steered)
	var refusal := world.challenge_refusal(steered, faction)
	if refusal == WorldAdapter.CHALLENGE_NONE:
		var seat := Vector2i((world.person_position(world.leader(faction)) / WorldAdapter.CELL_SIZE).floor())
		if world.order(steered, seat, _skills, faction):
			_objective = seat
		_hud.factions.close()
		_hud.show_challenge(refusal)
	else:
		_hud.factions.refresh(_hud.CHALLENGE_KEYS[refusal])
	return refusal


## Has the steered person declare `what`, one of the WorldAdapter.DECLARATION_* constants, toward the
## faction the faction window has chosen, through the core's declare port, and reads the window
## again. Returns the core's verdict, one of the WorldAdapter.DECLARE_* constants, of which
## DECLARE_NONE means the order was given; or -1 for no order.
func declare(what: int) -> int:
	var roster := world.roster(steered)
	var target: int = _hud.factions.chosen_faction()
	if what < 0 or roster.is_empty() or target < 0:
		return -1
	var refusal := world.declaration_refusal(roster[0], target, what)
	if refusal == WorldAdapter.DECLARE_NONE:
		world.declare(roster[0], target, what)
	_hud.factions.refresh()
	return refusal


# Does what the touch controls ask while the arts panel is open, `action` as their take() returns
# it: chooses the skill on a tile tapped, puts the skill chosen on a skill button's slot, scrolls the
# tiles, or closes the panel.
func _place(action: Dictionary) -> void:
	match action.get("do", ""):
		"panel":
			_hud.arts.tap(action.at)
		"scroll":
			_hud.arts.scroll(action.way)
		"place":
			if _hud.arts.assign(action.slot):
				_hud.show_loadout(world)
		"button":
			_hud.arts.close()
	_hud.show_placing()


## Has the steered person apply to the sect the sect panel shows, and shows the core's verdict on
## the panel. Returns the verdict, one of the WorldAdapter.APPLICATION_* constants, of which
## APPLICATION_NONE means it joined; or -1 while the panel shows no sect or the person has no body.
func apply_to_sect() -> int:
	var panel: Control = _hud.sect
	var roster := world.roster(steered)
	if panel.sect < 0 or roster.is_empty():
		return -1
	var refusal := world.application_refusal(roster[0], panel.sect)
	if refusal == WorldAdapter.APPLICATION_NONE:
		world.apply(roster[0], panel.sect)
	panel.show_verdict(refusal)
	return refusal


## Runs the ticks `delta` seconds of play call for, at the core's tick rate times the speed the
## settings choose: none on the title screen or while a screen is open over the world.
func advance(delta: float) -> void:
	if world == null or _hud.screen() != null:
		return
	var speed: int = _hud.settings.speed()
	var step_seconds := 1.0 / (WorldAdapter.TICKS_PER_SECOND * speed)
	_since_tick = minf(_since_tick + delta, step_seconds * MAX_TICKS_PER_FRAME * speed)
	# A screen a tick opens, such as the rebirth screen, stops the ticks after it.
	while _since_tick >= step_seconds and _hud.screen() == null:
		step()
		_since_tick -= step_seconds


## Advances the world one tick, follows the player's soul into the army its body serves in now,
## saves the world once AUTOSAVE_DAYS days have begun since it was last saved and a body carries
## the soul, and opens the rebirth screen once the steered person has died and its soul is born
## again as another.
func step() -> void:
	world.tick()
	_art.note_tick()
	_sound.note_tick()
	ticks += 1
	# While the joystick is held, the person is ordered on its way again from each new cell it
	# reaches.
	if _stick != Vector2i.ZERO and Vector2i((lead() / WorldAdapter.CELL_SIZE).floor()) != _stick_from:
		_steer(_stick)
	var army := world.player_army()
	# While no body carries the soul, the camera keeps the army it last served in.
	if army >= 0 and army != steered:
		steered = army
		_art.steered = army
		_hud.show_player(world, army)
	var date := world.date()
	if date != _date:
		_date = date
		_unsaved_days += 1
	if _unsaved_days >= AUTOSAVE_DAYS and army >= 0:
		_hud.show_autosave(save())
	var roster := world.roster(steered)
	if roster.is_empty() or roster[0] == _body:
		return
	if _body >= 0 and world.person_health(_body) == 0:
		_moving = false
		_aiming = KEY_NONE
		_show_aim()
		_stick = Vector2i.ZERO
		_hud.touch.release_all()
		_hud.arts.close()
		_hud.dialogue.close()
		_hud.show_placing()
		_hud.show_player(world, steered)
		_hud.rebirth.open(world, _body, roster[0])
	_body = roster[0]


## Returns the steered person's position, in sub-cell units, or where it last stood while it has
## no body: once dead, until its soul is born again.
func lead() -> Vector2:
	var positions := world.body_positions(steered)
	if not positions.is_empty():
		_last_lead = positions[0]
	return _last_lead


## Returns where the steered person is drawn this frame, in pixels: between its last two ticks'
## positions as WorldArt draws it, or where it last stood while it has no body.
func drawn_lead() -> Vector2:
	if world.body_positions(steered).is_empty():
		return _to_pixels(lead())
	return _art.body_at(steered, 0, _between())


# How far this frame is from the last tick toward the next, as a share of a tick.
func _between() -> float:
	var speed: int = _hud.settings.speed()
	var step_seconds := 1.0 / (WorldAdapter.TICKS_PER_SECOND * speed)
	return clampf(_since_tick / step_seconds, 0.0, 1.0)


func _all_skills() -> PackedInt64Array:
	var skills := PackedInt64Array()
	for skill in world.skill_count(steered):
		skills.append(skill)
	return skills


func _to_pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
