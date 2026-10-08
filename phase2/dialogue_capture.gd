# Captures of talking as play.tscn draws it (PR #180, phase 2), in each locale: prompt, refused,
# menu, news and far on PC at 1920x1080, and the touch HUD with no rumour log. The pairs are found
# and walked up as art/mockup/dialogue.gd finds them. Run after make build, copied to the root:
#   godot --path . --script res://dialogue_capture.gd -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const TALK_REACH := 2
const FAR := 64
const LOCALES := ["ko", "en"]
const KEPT := "user://dialogue_capture.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true},
]
const VIEWS := ["prompt", "refused", "menu", "news", "far"]
const TOUCH_VIEWS := ["hud"]

var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _worlds := {}
var _pairs := {}

func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for run: Dictionary in RUNS:
			for view: String in (TOUCH_VIEWS if run.touch else VIEWS):
				_shots.append({"locale": locale, "run": run, "view": view})
	# One world for each pair, so walking one army up to its person leaves the others as they were.
	var near := _lived()
	var player := near.player_army()
	# The person not hostile to the player who has heard the most, so the news fills the window.
	var friendly := _pair(near, func(army: int) -> bool: return army == player, false,
			func(person: int) -> float: return -near.rumours(person).size())
	var hostile_world := _lived()
	var hostile := _pair(hostile_world, func(_army: int) -> bool: return true, true, func(_person: int) -> float: return 0.0)
	var far_world := _lived()
	var there := _cell(far_world.person_position(friendly.person))
	var far := _pair(far_world, func(_army: int) -> bool: return true, false, func(person: int) -> float:
			return 0.0 if there.distance_to(_cell(far_world.person_position(person))) > FAR else INF)
	for pair: Dictionary in [friendly, hostile, far]:
		if pair.is_empty():
			push_error("dialogue_capture: no pair found")
			quit(1)
			return
	_approach(near, friendly)
	_approach(hostile_world, hostile)
	_approach(far_world, far)
	_pairs = {"hud": friendly, "prompt": friendly, "menu": friendly, "news": friendly, "refused": hostile, "far": far}
	_worlds = {"hud": near, "prompt": near, "menu": near, "news": near, "refused": hostile_world, "far": far_world}
	for view: String in VIEWS:
		var pair: Dictionary = _pairs[view]
		var world: WorldAdapter = _worlds[view]
		print("dialogue_capture: %s steers army %d of %s, talks to person %d of %s, %d cells apart, %d rumours" % [view,
				pair.army, world.faction_names()[world.army_faction(pair.army)], pair.person,
				world.faction_names()[world.person_faction(pair.person)], _apart(world, pair),
				world.rumours(pair.person).size()])
	var near_news := near.rumours(friendly.person)
	var far_news := far_world.rumours(far.person)
	print("dialogue_capture: the near person heard %d, the far person %d, %d of them both" % [near_news.size(),
			far_news.size(), near_news.filter(func(event: Dictionary) -> bool: return far_news.has(event)).size()])
	_open()


# SEED's region lived YEARS years.
func _lived() -> WorldAdapter:
	var world := WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	for i in YEARS * DAYS_PER_YEAR * TICKS_PER_DAY:
		world.tick()
	return world


func _cell(at: Vector2) -> Vector2:
	return (at / WorldAdapter.CELL_SIZE).floor()


func _apart(world: WorldAdapter, pair: Dictionary) -> float:
	return _cell(world.person_position(world.roster(pair.army)[0])).distance_to(
			_cell(world.person_position(pair.person)))


# Marks `pair`'s army steered and walks it toward its person until it stands within TALK_REACH,
# for at most three years, its person waiting for it.
func _approach(world: WorldAdapter, pair: Dictionary) -> void:
	world.steer(pair.army, true)
	# The person waits where it stands, its army steered so the rules for people on their own leave it.
	for army in world.army_count():
		if world.roster(army).has(pair.person):
			world.steer(army, true)
			world.order(army, Vector2i(_cell(world.person_position(pair.person))),
					PackedInt64Array(range(world.skill_count(army))))
	for i in 3 * DAYS_PER_YEAR * TICKS_PER_DAY:
		if world.roster(pair.army).is_empty() or world.person_health(pair.person) == 0:
			break
		if _apart(world, pair) <= TALK_REACH:
			return
		if i % 20 == 0:
			var skills := PackedInt64Array(range(world.skill_count(pair.army)))
			world.order(pair.army, Vector2i(_cell(world.person_position(pair.person))), skills)
		world.tick()
	push_error("dialogue_capture: army %d did not reach person %d" % [pair.army, pair.person])


# The army, `test` holding for it, and the living person of another army, hostile to it or not as
# `hostile` asks, with the least `rank` and, among those, the nearest: {army, person}; or empty for
# none. A rank of INF rules a person out.
func _pair(world: WorldAdapter, test: Callable, hostile: bool, rank: Callable) -> Dictionary:
	var best := {}
	var least := [INF, INF]
	for army in world.army_count():
		var roster := world.roster(army)
		if roster.is_empty() or not test.call(army):
			continue
		var here := _cell(world.person_position(roster[0]))
		var mine := world.army_faction(army)
		for other in world.army_count():
			if other == army:
				continue
			var relation := world.relation(mine, world.army_faction(other))
			var is_hostile := relation == WorldAdapter.RELATION_HOSTILE or relation == WorldAdapter.RELATION_AT_WAR
			if is_hostile != hostile:
				continue
			for person in world.roster(other):
				var key := [rank.call(person), here.distance_to(_cell(world.person_position(person)))]
				if key[0] < INF and (key[0] < least[0] or (key[0] == least[0] and key[1] < least[1])):
					least = key
					best = {"army": army, "person": person}
	return best


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	if _step >= 2 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
	_world = _worlds[shot.view]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("on" if shot.run.touch else "off"))
			DisplayServer.window_set_size(shot.run.window)
		1:
			_play._play(_world)
			_play.set_process(false)
			var army: int = _pairs[shot.view].army
			_play.steered = army
			_play._art.steered = army
			_play._body = _world.roster(army)[0]
			hud.show_player(_world, army)
			_play._process(0.0)
			hud.labels.notice.visible = false
		2:
			if shot.view in ["menu", "news", "far"]:
				_play._talk_read_at = -1
				_play.show_talk()
				var opened: bool = _play.talk(_play.TALK_KEY)
				if shot.view != "menu":
					_play.talk(KEY_ENTER)
				print("dialogue_capture: %s talks to %d (wanted %d), opened %s" % [shot.view, _play.talk_to,
						_pairs[shot.view].person, opened])
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("dialogue_capture: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _save(shot: Dictionary) -> void:
	var name: String = "%s_%s_%s" % [shot.view, shot.run.name, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	var hud: CanvasLayer = _play.get_node("Hud")
	print("dialogue_capture: %s.png, prompt on %d refused %s shown %s, window on %d news %s" % [name,
			hud.talk_prompt.person, hud.talk_prompt.refused, hud.talk_prompt.visible, hud.dialogue.person,
			hud.dialogue.showing_news])
