## Opens on the title screen, where a new game generates a region from a seed, typed or drawn at
## random, and lets the player steer one person born into it, through the same orders every army
## follows: W, A, S and D order it a few cells their way, a left click toward the cell clicked,
## keys 1 to 9 to strike with that skill, and 0 with all its skills; C challenges its faction's
## leader to a duel, with the same order, when the core allows it; G opens the genealogy panel on
## the person, and M the sect panel on its sect, where Enter applies to the sect shown. When the
## person dies and its soul is born again, the rebirth screen sums up the life just ended. Escape pauses the world under the pause panel,
## whose guide shows the controls as bound here. The camera follows the person. It ticks at the
## core's tick rate times the speed the settings choose, draws the world through world_art.tscn and
## the HUD and its screens through hud.tscn, sounds it through sound.tscn, and holds no game rule.
extends Node2D

const PIXELS_PER_CELL := 16.0
# Ticks one frame may run to catch up at the core's own rate, and as many times more at a higher
# speed; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4
## The keys that steer the person, each with the way it orders it.
const MOVE_KEYS := {KEY_W: Vector2i.UP, KEY_A: Vector2i.LEFT, KEY_S: Vector2i.DOWN, KEY_D: Vector2i.RIGHT}
## How many cells ahead a move key sets the objective; nearer cells are tried when the core
## refuses it.
const STRIDE := 3
## The keys that strike with one skill each, the first skill's first, and the key that strikes with
## all of them.
const SKILL_KEYS := [KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9]
const ALL_SKILLS_KEY := KEY_0
const CHALLENGE_KEY := KEY_C
const GENEALOGY_KEY := KEY_G
const SECT_KEY := KEY_M
const PAUSE_KEY := KEY_ESCAPE

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
# The move keys held, in the order they were pressed.
var _held: Array[Key] = []
# The person last steered, whose death the rebirth screen sums up once its soul is born again.
var _body := -1

@onready var _camera: Camera2D = $Camera
@onready var _art: Node2D = $WorldArt
@onready var _hud: CanvasLayer = $Hud
@onready var _sound: Node2D = $Sound


func _ready() -> void:
	_hud.open_title()
	_hud.settings.read()
	_hud.settings.apply()


## Starts a world generated from `from_seed`, the player born into `birth_village`, and closes
## the title screen. Returns whether the core made the world.
func start(from_seed: int) -> bool:
	var made := WorldAdapter.create_region(from_seed, birth_village)
	if made == null:
		return false
	world = made
	world_seed = from_seed
	steered = world.army_count() - 1
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
	return true


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
				or _hud.sect.visible:
			return false
		_held.clear()
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
		"settings":
			_hud.pause.close()
			_hud.settings.open()
		"back":
			# Back from the settings to the pause panel they were opened from, in a game.
			if world != null:
				_hud.pause.visible = true
		"resume":
			_hud.pause.close()
		"continue":
			_hud.rebirth.close()
		"title":
			get_tree().reload_current_scene()
		"quit":
			get_tree().quit()


## Returns the controls as this scene binds them, for the pause panel's guide: each a Dictionary
## of its `keys`, the words on its caps, and `does`, the hud.csv key of what it does.
func controls() -> Array[Dictionary]:
	var moves: Array[String] = []
	for keycode: Key in MOVE_KEYS:
		moves.append(OS.get_keycode_string(keycode))
	var skills := "%s-%s" % [OS.get_keycode_string(SKILL_KEYS.front()), OS.get_keycode_string(SKILL_KEYS.back())]
	var rows: Array[Dictionary] = [
		{"keys": moves, "does": "CONTROLS_MOVE"},
		{"keys": ["CONTROLS_CLICK"], "does": "CONTROLS_CLICK_MOVE"},
		{"keys": [skills], "does": "CONTROLS_SKILL"},
		{"keys": [OS.get_keycode_string(ALL_SKILLS_KEY)], "does": "CONTROLS_ALL_SKILLS"},
		{"keys": [OS.get_keycode_string(CHALLENGE_KEY)], "does": "CONTROLS_CHALLENGE"},
		{"keys": [OS.get_keycode_string(GENEALOGY_KEY)], "does": "CONTROLS_GENEALOGY"},
		{"keys": [OS.get_keycode_string(SECT_KEY)], "does": "CONTROLS_SECTS"},
		{"keys": [OS.get_keycode_string(_hud.LOCALE_KEY)], "does": "CONTROLS_LANGUAGE"},
		{"keys": [OS.get_keycode_string(PAUSE_KEY)], "does": "CONTROLS_PAUSE"},
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
	var key := event as InputEventKey
	if mouse != null and mouse.pressed and mouse.button_index == MOUSE_BUTTON_LEFT:
		objective = Vector2i((mouse.position / PIXELS_PER_CELL).floor())
	elif key != null and key.pressed and (key.keycode == GENEALOGY_KEY or _hud.genealogy.visible):
		return genealogy(key.keycode)
	elif key != null and key.pressed and (key.keycode == SECT_KEY or _hud.sect.visible):
		return sects(key.keycode)
	elif key != null and key.pressed and key.keycode == CHALLENGE_KEY:
		return challenge() == WorldAdapter.CHALLENGE_NONE
	elif key != null and MOVE_KEYS.has(key.keycode) and not _hud.genealogy.visible and not _hud.sect.visible:
		return walk(key)
	elif key != null and key.pressed and (SKILL_KEYS.has(key.keycode) or key.keycode == ALL_SKILLS_KEY):
		skills = _all_skills() if key.keycode == ALL_SKILLS_KEY else PackedInt64Array([SKILL_KEYS.find(key.keycode)])
	else:
		return false
	if not world.order(steered, objective, skills):
		return false
	_objective = objective
	_skills = skills
	return true


## Steers the person with move key `key`, pressed or released: while move keys are held it is
## ordered STRIDE cells the way they point together, or as many fewer as the core accepts, and
## once none is held, to stop where it stands. The order keeps its skills. Returns whether the
## core accepted an order.
func walk(key: InputEventKey) -> bool:
	if key.pressed and not _held.has(key.keycode):
		_held.append(key.keycode)
	elif not key.pressed:
		_held.erase(key.keycode)
	var way := Vector2i.ZERO
	for held in _held:
		way += MOVE_KEYS[held]
	var here := Vector2i((lead() / WorldAdapter.CELL_SIZE).floor())
	# With no way held, the one objective is where the person stands.
	var strides: Array = [0] if way == Vector2i.ZERO else range(STRIDE, 0, -1)
	for stride: int in strides:
		var objective := here + way * stride
		if world.order(steered, objective, _skills):
			_objective = objective
			return true
	return false


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


## Advances the world one tick, and opens the rebirth screen once the steered person has died
## and its soul is born again as another.
func step() -> void:
	world.tick()
	_art.note_tick()
	_sound.note_tick()
	ticks += 1
	var roster := world.roster(steered)
	if roster.is_empty() or roster[0] == _body:
		return
	if _body >= 0 and world.person_health(_body) == 0:
		_held.clear()
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
