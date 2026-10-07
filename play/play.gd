## Opens on the title screen, where a new game generates a region from a seed, typed or drawn at
## random, and continue resumes the world saved in SAVE_SLOT, and lets the player steer one person
## in it, through the same orders every army follows: a right click orders it toward the cell
## clicked, and while the button is held, toward the cell under the cursor; keys 1 to 9 order it to
## strike with that skill, and 0 with all its skills. Q, W, E, R and T cast its first five skills
## at the cursor through the core's cast port, at once or, as the settings choose, on the key's
## release after aiming while it is held; the same key again or Escape cancels the aim. C
## challenges its faction's leader to a duel, with the same order, when the core allows it; G
## opens the genealogy panel on the person, and M the sect panel on its sect, where Enter applies
## to the sect shown. The person is the one the core says carries the player's soul,
## found again after a rebirth or a load; when it dies and its soul is born again, the rebirth
## screen sums up the life just ended. Escape pauses the world under the pause panel, which saves
## it and whose guide shows the controls as bound here; it is also saved every AUTOSAVE_DAYS days.
## The camera follows the person. It ticks at the core's tick rate times the speed the settings
## choose, draws the world through world_art.tscn and the HUD and its screens through hud.tscn,
## sounds it through sound.tscn, and holds no game rule.
extends Node2D

const PIXELS_PER_CELL := 16.0
# Ticks one frame may run to catch up at the core's own rate, and as many times more at a higher
# speed; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4
## The button that orders the person toward the cell under the cursor, and keeps doing so while held.
const MOVE_BUTTON := MOUSE_BUTTON_RIGHT
## The keys that cast one skill each at the cursor, the first skill's first.
const CAST_KEYS := [KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T]
## The keys that strike with one skill each, the first skill's first, and the key that strikes with
## all of them.
const SKILL_KEYS := [KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9]
const ALL_SKILLS_KEY := KEY_0
const CHALLENGE_KEY := KEY_C
const GENEALOGY_KEY := KEY_G
const SECT_KEY := KEY_M
const PAUSE_KEY := KEY_ESCAPE
## The slot the pause panel saves to, the autosave writes and continue loads.
const SAVE_SLOT := 0
## The in-game days between autosaves.
const AUTOSAVE_DAYS := 3

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

@onready var _camera: Camera2D = $Camera
@onready var _art: Node2D = $WorldArt
@onready var _hud: CanvasLayer = $Hud
@onready var _sound: Node2D = $Sound
@onready var _aim: Node2D = $Aim


func _ready() -> void:
	_hud.open_title(WorldAdapter.load_refusal(SAVE_SLOT, save_directory) != WorldAdapter.LOAD_MISSING)
	_hud.settings.read()
	_hud.settings.apply()


## Starts a world generated from `from_seed`, the player born into `birth_village`, and closes
## the title screen. Returns whether the core made the world.
func start(from_seed: int) -> bool:
	var made := WorldAdapter.create_region(from_seed, birth_village)
	if made == null:
		return false
	_play(made)
	return true


## Resumes the world saved in SAVE_SLOT and closes the title screen, or shows on it why the save
## was not loaded. Returns whether it was loaded.
func resume_saved() -> bool:
	var loaded := WorldAdapter.load(SAVE_SLOT, save_directory)
	if loaded == null:
		_hud.title_screen.refuse(WorldAdapter.load_refusal(SAVE_SLOT, save_directory))
		return false
	_play(loaded)
	return true


## Saves the world in SAVE_SLOT, unless no body carries the player's soul, since a world loaded
## then would have no person to steer. Returns whether it was written.
func save() -> bool:
	if world.player_army() < 0:
		return false
	_unsaved_days = 0
	return world.save(SAVE_SLOT, save_directory)


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
	if not navigate(event):
		command(make_input_local(event))


## Works the screens with `event`: the screen open on top takes every key, and while none is
## open, Escape pauses the world unless a panel is open for it to close. A screen opening sounds,
## and so does a key a screen takes. Returns whether a screen took the event.
func navigate(event: InputEvent) -> bool:
	var key := event as InputEventKey
	var open: Control = _hud.screen()
	if open == null:
		if key == null or not key.pressed or key.keycode != PAUSE_KEY or _hud.genealogy.visible \
				or _hud.sect.visible or _aiming != KEY_NONE:
			return false
		_moving = false
		_hud.pause.open(controls(), world_seed)
		_sound.ui("open")
		return true
	if key != null and key.pressed:
		_take(open.key(key))
		var now: Control = _hud.screen()
		_sound.ui("open" if now != null and now != open else "click")
	return true


# Does what a screen's entry or key asks: one of the actions the screens' key() returns.
func _take(action: String) -> void:
	match action:
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
			_hud.settings.open()
		"back":
			# Back from the settings to the pause panel they were opened from, in a game.
			if world != null:
				_hud.pause.visible = true
		"resume":
			_hud.pause.close()
		"go_on":
			_hud.rebirth.close()
		"title":
			get_tree().reload_current_scene()
		"quit":
			get_tree().quit()


## Returns the controls as this scene binds them, for the pause panel's guide: each a Dictionary
## of its `keys`, the words on its caps, and `does`, the hud.csv key of what it does.
func controls() -> Array[Dictionary]:
	var casts := ""
	for keycode: Key in CAST_KEYS:
		casts += OS.get_keycode_string(keycode)
	var skills := "%s-%s" % [OS.get_keycode_string(SKILL_KEYS.front()), OS.get_keycode_string(SKILL_KEYS.back())]
	var rows: Array[Dictionary] = [
		{"keys": ["CONTROLS_CLICK"], "does": "CONTROLS_CLICK_MOVE"},
		{"keys": [casts], "does": "CONTROLS_CAST"},
		{"keys": [skills], "does": "CONTROLS_SKILL"},
		{"keys": [OS.get_keycode_string(ALL_SKILLS_KEY)], "does": "CONTROLS_ALL_SKILLS"},
		{"keys": [OS.get_keycode_string(CHALLENGE_KEY)], "does": "CONTROLS_CHALLENGE"},
		{"keys": [OS.get_keycode_string(GENEALOGY_KEY)], "does": "CONTROLS_GENEALOGY"},
		{"keys": [OS.get_keycode_string(SECT_KEY)], "does": "CONTROLS_SECTS"},
		{"keys": [OS.get_keycode_string(_hud.LOCALE_KEY)], "does": "CONTROLS_LANGUAGE"},
		{"keys": [OS.get_keycode_string(PAUSE_KEY)], "does": "CONTROLS_PAUSE_CANCEL"},
	]
	return rows


func _process(delta: float) -> void:
	advance(delta)
	if world == null or _hud.screen() != null:
		return
	_camera.position = _to_pixels(lead()).floor()
	var screen := get_viewport_rect().size / _camera.zoom
	var view := Rect2(_camera.get_screen_center_position() - screen / 2.0, screen)
	_art.refresh(view)
	_sound.hearing = view
	# The cursor stays on the screen while the camera moves, so it moves over the world.
	_cursor = get_local_mouse_position()
	if _moving:
		follow(_cursor)
	_show_aim()
	_hud.refresh(world, steered, _skills)


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
	elif key != null and key.pressed and (key.keycode == GENEALOGY_KEY or _hud.genealogy.visible):
		return genealogy(key.keycode)
	elif key != null and key.pressed and (key.keycode == SECT_KEY or _hud.sect.visible):
		return sects(key.keycode)
	elif key != null and key.pressed and key.keycode == CHALLENGE_KEY:
		return challenge() == WorldAdapter.CHALLENGE_NONE
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


## Has the steered person cast skill `skill` at `at`, a position in this node's pixels, and shows
## on the HUD why the core refuses it, if it does. Returns the core's verdict, one of the
## WorldAdapter.CAST_* constants, of which CAST_NONE means it will cast; or -1 while the person
## has no body.
func cast_at(skill: int, at: Vector2) -> int:
	var roster := world.roster(steered)
	if roster.is_empty():
		return -1
	var point := at / PIXELS_PER_CELL * WorldAdapter.CELL_SIZE
	var refusal := world.cast_refusal(roster[0], skill, point)
	if refusal == WorldAdapter.CAST_NONE:
		world.cast(roster[0], skill, point)
	else:
		_hud.show_cast(refusal)
	return refusal


# Shows the aimed skill's range round the person and its area at the cursor while a cast key is held
# to aim, and hides it otherwise.
func _show_aim() -> void:
	var skill := CAST_KEYS.find(_aiming)
	var skills := world.skills(steered)
	if skill < 0 or skill >= skills.size():
		_aim.visible = false
		return
	var reach: float = skills[skill]["range"] * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	var radius: float = skills[skill]["radius"] * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
	_aim.show_aim(_to_pixels(lead()), reach, _cursor, radius)


## Has the steered person challenge its faction's leader, marching on the leader with its current
## skills, and shows the outcome on the HUD. Returns the core's verdict, one of the
## WorldAdapter.CHALLENGE_* constants; CHALLENGE_NONE means the duel began.
func challenge() -> int:
	var faction := world.army_faction(steered)
	var refusal := world.challenge_refusal(steered, faction)
	if refusal == WorldAdapter.CHALLENGE_NONE:
		var seat := Vector2i((world.person_position(world.leader(faction)) / WorldAdapter.CELL_SIZE).floor())
		if world.order(steered, seat, _skills, faction):
			_objective = seat
	_hud.show_challenge(refusal)
	return refusal


## Works the genealogy panel with `keycode`: G opens it on the steered person or closes it,
## arrows move the selection, Enter centres the tree on the selected person, Escape closes it.
## While it is open no key gives an order. Returns whether the key did anything.
func genealogy(keycode: Key) -> bool:
	var panel: Control = _hud.genealogy
	var roster := world.roster(steered)
	if keycode == GENEALOGY_KEY and not panel.visible:
		if roster.is_empty():
			return false
		panel.show_person(world, roster[0], roster[0])
		_sound.ui("open")
		return true
	if keycode == GENEALOGY_KEY or keycode == KEY_ESCAPE:
		panel.close()
		return true
	var directions := {KEY_UP: Vector2i.UP, KEY_DOWN: Vector2i.DOWN, KEY_LEFT: Vector2i.LEFT,
			KEY_RIGHT: Vector2i.RIGHT}
	if directions.has(keycode):
		return panel.move(directions[keycode])
	if keycode == KEY_ENTER:
		panel.centre_selected()
		return true
	return false


## Works the sect panel with `keycode`: M opens it on the steered person's sect or closes it,
## up and down choose among the shown faction's sects, Tab shows the next faction, Enter applies to
## the sect shown, Escape closes it. While it is open no key gives an order. Returns whether the
## key did anything.
func sects(keycode: Key) -> bool:
	var panel: Control = _hud.sect
	if keycode == SECT_KEY and not panel.visible:
		if world.sect_count() == 0 or world.roster(steered).is_empty():
			return false
		panel.show_sect(world, steered)
		_sound.ui("open")
		return true
	if keycode == SECT_KEY or keycode == KEY_ESCAPE:
		panel.close()
		return true
	if keycode == KEY_TAB:
		panel.next_faction()
		return true
	if keycode == KEY_ENTER or keycode == KEY_KP_ENTER:
		return apply_to_sect() >= 0
	if keycode == KEY_UP or keycode == KEY_DOWN:
		return panel.move(Vector2i.UP if keycode == KEY_UP else Vector2i.DOWN)
	return false


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


func _all_skills() -> PackedInt64Array:
	var skills := PackedInt64Array()
	for skill in world.skill_count(steered):
		skills.append(skill)
	return skills


func _to_pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * PIXELS_PER_CELL / WorldAdapter.CELL_SIZE
