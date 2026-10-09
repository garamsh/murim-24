# Mockup of talking to people, proposed for #166, drawn over play.tscn with the HUD's own panel
# frame, font and palette (hud/panel.gd), in each locale, at each window in RUNS:
# - prompt_<run>_<locale>.png: the HUD without its rumour log, and over the nearest living person
#   within TALK_REACH whose faction is not hostile, the prompt to talk to it.
# - refused_<run>_<locale>.png: the nearest person within TALK_REACH is of a hostile faction; the
#   prompt says it will not talk.
# - menu_<run>_<locale>.png: the dialogue window: the person's card, name, faction, sect and age,
#   its greeting, and the menu of what it offers, here asking for news, and farewell.
# - news_<run>_<locale>.png: the person asked for news: its own WorldAdapter.rumours, newest first,
#   worded as hud.gd's rumour_text words the log's.
# - far_<run>_<locale>.png: the same, asked of a person far from the first, who has heard other news.
# - hud_<run>_<locale>.png: on a touch screen, the HUD without its rumour log, and no prompt or
#   button to talk: touch is deferred (scope, 2026-10-09).
# Hostility is WorldAdapter.relation, HOSTILE or AT_WAR. People, names, factions and rumours are the
# world's. The words are the mockup's own, held in WORDS, and hud.csv's where it has them; the
# script prints any word that overruns its room.
# The world is SEED's region with days of five ticks, lived YEARS years, as evidence/pr-177 does.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://dialogue.gd -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const PersonName := preload("res://hud/person_name.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
## The talk reach the mockup assumes, in cells: two, so a person a cell or two away can be talked to.
const TALK_REACH := 2
## Far enough apart, in cells, that two people's rumours differ: twice the region's rumour reach.
const FAR := 64
const LOCALES := ["ko", "en"]
const KEPT := "user://dialogue_mockup.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true},
]
const VIEWS := ["prompt", "refused", "menu", "news", "far"]
const TOUCH_VIEWS := ["hud"]

const WORDS := {
	"TALK_KEY": ["Space", "Space"],
	"TALK": ["대화", "Talk"],
	"REFUSED": ["적대 · 말을 섞지 않는다", "Hostile · will not talk"],
	"CLOSE": ["Esc 닫기", "Esc Close"],
	"GREETING": ["무슨 일이오?", "What is it?"],
	"OFFER_NEWS": ["요즘 소식이 있소?", "Any news?"],
	"FAREWELL": ["잘 있으시오.", "Farewell."],
	"NEWS": ["들은 소문", "What I have heard"],
	"NEWS_NONE": ["요즘 들려온 소문이 없소.", "No news has reached me."],
	"NO_SECT": ["문파 없음", "No sect"],
	"AGE": ["%d세", "Age %d"],
	"KEYS_MENU": ["방향키 고르기 · Enter 말하기 · Esc 닫기", "Arrows choose · Enter say · Esc close"],
	"KEYS_NEWS": ["방향키 넘기기 · Esc 돌아가기", "Arrows scroll · Esc back"],
	"MORE": ["%d / %d", "%d / %d"],
}
# The room each word is given, for the overrun check.
const ROOM := {"TALK": 60, "REFUSED": 150, "CLOSE": 60, "GREETING": 300, "OFFER_NEWS": 280, "FAREWELL": 280,
		"NEWS": 200, "NEWS_NONE": 300, "NO_SECT": 84, "AGE": 84, "KEYS_MENU": 400, "KEYS_NEWS": 400}


## The proposed dialogue window, drawn from what `view` holds, in the native 480x270 the other panels
## are laid out in: a panel along the foot of the room, over the technique bar, so the two people
## talking stay in view above it at the middle of the screen,
## the person's card and particulars on the left and what it says on the right: its greeting over
## the menu of what it offers, or the news it has heard, newest first.
class Dialogue extends "res://hud/panel.gd":
	const RECT := Rect2(20, 90, 440, 146)
	const CARD_RECT := Rect2(30, 116, 84, 90)
	const SAY_X := 132
	const SAY_Y := 116
	const LINE := 14
	const NEWS_LINES := 5
	const CRIMSON_LIGHT := Color("d63a2f")
	const FACTION_TEXT: Array[Color] = [Color("5b9ae0"), Color("d63a2f"), Color("e8a830")]

	## What the window shows: `words`, `person`, `name`, `faction`, `colour`, `sect`, `age`, `mode`,
	## "menu" or "news", `offers` (words), `chosen`, and `news` (lines, newest first).
	var view := {}

	func w(key: String) -> String:
		return view.words[key]

	func _draw() -> void:
		if view.is_empty():
			return
		_font = get_theme_default_font() if _font == null else _font
		var keys := w("KEYS_MENU") if view.mode == "menu" else w("KEYS_NEWS")
		_dialog(RECT, view.name, w("CLOSE") if view.mode == "menu" else "", keys)
		# The card: the person standing twice its size on a cell of ground, and its particulars.
		var c := CARD_RECT
		draw_rect(Rect2(c.position + Vector2(26, 2), Vector2(32, 48)), Palette.INK_1)
		draw_rect(Rect2(c.position + Vector2(26, 34), Vector2(32, 16)), Palette.GRASS_3)
		draw_rect(Rect2(c.position + Vector2(26, 34), Vector2(32, 1)), Palette.GRASS_4)
		_portrait(view.person, Rect2(c.position + Vector2(26, 2), Vector2(32, 48)), false)
		_text(Vector2(c.position.x, c.position.y + 54), fit(view.faction, c.size.x), view.colour)
		_text(Vector2(c.position.x, c.position.y + 68), fit(view.sect, c.size.x), Palette.STONE_4)
		_text(Vector2(c.position.x, c.position.y + 82), view.age, Palette.STONE_4)
		draw_rect(Rect2(SAY_X - 10, RECT.position.y + 26, 1, RECT.size.y - 52), Palette.INK_3)
		var room := RECT.end.x - 12 - SAY_X
		if view.mode == "menu":
			_text(Vector2(SAY_X, SAY_Y), "“" + w("GREETING") + "”", Palette.GOLD_4)
			var items: Array[Dictionary] = []
			for offer: String in view.offers:
				items.append({"text": offer})
			_menu(Vector2(SAY_X, SAY_Y + 22), room, items, view.chosen)
			return
		_text(Vector2(SAY_X, SAY_Y), w("NEWS"), Palette.GOLD_4)
		var news: Array = view.news
		if news.is_empty():
			_text(Vector2(SAY_X, SAY_Y + 18), "“" + w("NEWS_NONE") + "”", Palette.PAPER_3)
			return
		var more := w("MORE") % [mini(NEWS_LINES, news.size()), news.size()]
		_text(Vector2(RECT.end.x - 12 - _width(more), SAY_Y), more, Palette.STONE_3)
		for n in mini(NEWS_LINES, news.size()):
			var y := SAY_Y + 18 + n * LINE
			draw_rect(Rect2(SAY_X, y + 5, 3, 3), Palette.GOLD_2)
			_text(Vector2(SAY_X + 8, y), fit(news[n], room - 8), Palette.PAPER_3 if n == 0 else Palette.PAPER_2)

	func width_of(text: String) -> float:
		_font = get_theme_default_font() if _font == null else _font
		return _width(text)


## The prompt over the person the player may talk to: the key's cap and the word, on a slip of ink
## centred over its head; or, for a hostile person, the refusal in crimson with no key.
class Prompt extends "res://hud/panel.gd":
	## Where the person's head is, in the HUD's pixels.
	var head := Vector2.ZERO
	var key := ""
	var word := ""
	var refused := false

	func _draw() -> void:
		if word.is_empty():
			return
		_font = get_theme_default_font() if _font == null else _font
		var cap := 0.0 if refused else keycap_width(key) + 4
		var width := cap + _width(word) + 8
		var at := (head - Vector2(width / 2.0, 18)).floor()
		draw_rect(Rect2(at, Vector2(width, 17)), Palette.GOLD_2 if not refused else Color("9c1f24"))
		draw_rect(Rect2(at + Vector2.ONE, Vector2(width - 2, 15)), Palette.INK_2)
		# A tick under the slip points at the person.
		draw_rect(Rect2(head.x - 1, at.y + 17, 3, 1), Palette.GOLD_2 if not refused else Color("9c1f24"))
		draw_rect(Rect2(head.x, at.y + 18, 1, 1), Palette.GOLD_2 if not refused else Color("9c1f24"))
		if not refused:
			_keycap(at + Vector2(4, 2), key)
		_text(at + Vector2(4 + cap, 2), word, Color("d63a2f") if refused else Palette.PAPER_3)


var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _worlds := {}
var _dialogue: Dialogue
var _prompt: Prompt
# By view: the army steered and the person talked to.
var _pairs := {}
var _checked := false


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
			push_error("dialogue: no pair found")
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
		print("dialogue: %s steers army %d of %s, talks to person %d of %s, %d cells apart, %d rumours" % [view,
				pair.army, world.faction_names()[world.army_faction(pair.army)], pair.person,
				world.faction_names()[world.person_faction(pair.person)], _apart(world, pair),
				world.rumours(pair.person).size()])
	var near_news := near.rumours(friendly.person)
	var far_news := far_world.rumours(far.person)
	print("dialogue: the near person heard %d, the far person %d, %d of them both" % [near_news.size(),
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
	push_error("dialogue: army %d did not reach person %d" % [pair.army, pair.person])


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
			_steer(hud, _pairs[shot.view].army)
			_play._process(0.0)
			hud.labels.notice.visible = false
			# The rumour log goes: talking to people stands in for it.
			hud._rumour_panel.visible = false
			_prompt = Prompt.new()
			_prompt.use_font(hud.arts._font)
			hud.add_child(_prompt)
			_dialogue = Dialogue.new()
			_dialogue.use_font(hud.arts._font)
			_dialogue._world = _world
			hud.add_child(_dialogue)
			if not _checked:
				_check_rooms()
		2:
			var room: Vector2 = hud.room
			var pair: Dictionary = _pairs[shot.view]
			# Centred across the room and standing on its foot, as the technique bar does.
			_dialogue.position = Vector2(floorf((room.x - _dialogue.SCREEN.x) / 2.0), room.y - _dialogue.SCREEN.y)
			_dialogue.visible = shot.view in ["menu", "news", "far"]
			_dialogue.view = _compose(shot.view)
			_prompt.head = _head(hud, pair.person)
			_prompt.key = _word("TALK_KEY")
			_prompt.refused = shot.view == "refused"
			_prompt.word = _word("REFUSED") if _prompt.refused else _word("TALK") + " · " + PersonName.of(_world,
					pair.person, true)
			_prompt.visible = shot.view in ["prompt", "refused"]
			_prompt.queue_redraw()
			_dialogue.queue_redraw()
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("dialogue: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


func _steer(hud: CanvasLayer, army: int) -> void:
	_play.steered = army
	_play._art.steered = army
	_play._body = _world.roster(army)[0]
	hud.show_player(_world, army)


# Where `person`'s head stands on screen, in the HUD's pixels: world_art.gd draws a body from 16
# pixels above its position.
func _head(hud: CanvasLayer, person: int) -> Vector2:
	var at := _world.person_position(person) * 16.0 / WorldAdapter.CELL_SIZE - Vector2(0, 17)
	return hud.from_window(_play.get_viewport().get_canvas_transform() * at).floor()


func _word(key: String) -> String:
	return WORDS[key][LOCALES.find(TranslationServer.get_locale().left(2))]


func _words() -> Dictionary:
	var words := {}
	for key: String in WORDS:
		words[key] = _word(key)
	return words


# What the window shows for `view`, read from the world.
func _compose(view: String) -> Dictionary:
	var person: int = _pairs[view].person
	var faction := _world.person_faction(person)
	var sect := _world.person_sect(person)
	var hud: CanvasLayer = _play.get_node("Hud")
	var news: Array[String] = []
	for event: Dictionary in _world.rumours(person):
		news.append(hud.rumour_text(_world, event))
	var colours: Array[Color] = [Color("5b9ae0"), Color("d63a2f"), Color("e8a830")]
	return {"words": _words(), "person": person, "name": PersonName.of(_world, person, false),
		"faction": TranslationServer.translate("FACTION_" + _world.faction_names()[faction].to_upper()),
		"colour": colours[faction % 3],
		"sect": _word("NO_SECT") if sect < 0 else TranslationServer.translate("SECT_"
				+ _world.sect_name(sect).to_upper().replace(" ", "_")),
		"age": _word("AGE") % _world.person_age(person), "mode": "menu" if view == "menu" else "news",
		"offers": [_word("OFFER_NEWS"), _word("FAREWELL")], "chosen": 0, "news": news}


# Prints every word, in both locales, that overruns the room ROOM gives it.
func _check_rooms() -> void:
	_checked = true
	var over := 0
	for key: String in WORDS:
		for n in LOCALES.size():
			var text: String = WORDS[key][n].replace("%s", "").replace("%d", "00")
			var width := _dialogue.width_of(text)
			if ROOM.has(key) and width > ROOM[key]:
				over += 1
				print("dialogue: %s (%s) is %d past %d" % [key, text, width, ROOM[key]])
	print("dialogue: %d words overrun their room" % over)


func _save(shot: Dictionary) -> void:
	var name: String = "%s_%s_%s" % [shot.view, shot.run.name, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("dialogue: %s.png %dx%d, HUD %dx in %s, sha256 %s" % [name, image.get_width(), image.get_height(),
			ViewScale.hud_scale(shot.run.window, shot.run.touch), ViewScale.hud_room(shot.run.window, shot.run.touch),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
