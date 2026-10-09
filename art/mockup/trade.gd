# Mockup of buying elixirs and herbs from merchants with silver, proposed for #167, drawn over
# play.tscn with the HUD's own panel frame, font and palette (hud/panel.gd), in each locale, at each
# window in RUNS:
# - menu_<run>_<locale>.png: the dialogue window on a merchant, a person whose best discipline is
#   medicine: the menu of what it offers, news and now its wares, and farewell.
# - trade_<run>_<locale>.png: the merchant's wares in the window: each ware's icon, name, effect,
#   how many the player holds and its price, and the player's silver.
# - bought_<run>_<locale>.png: the same after buying a healing herb: one more held, its price less
#   silver, and the line saying what was bought.
# - short_<run>_<locale>.png: the elixir chosen with too little silver; the line says so in crimson.
# - hud_<run>_<locale>.png: the HUD, the player wounded: its silver on a strip under the leader's,
#   and the wares it holds on a belt right of the technique bar, each under its key.
# - used_<run>_<locale>.png: the same after the herb's key: health 30 higher, one herb fewer, and
#   the notice saying what was used.
# - full_<run>_<locale>.png: the herb's key at full health: the notice says health is full and
#   nothing is used.
# - touch_<run>_<locale>.png: on a touch screen, the HUD with its silver and no belt: touch is
#   deferred (scope, 2026-10-09).
# The merchant, its faction and its village are the world's. Silver, wares, prices, effects and
# what the player holds are the mockup's own, held in WARES and SILVER, since the core has none
# yet. The words are the mockup's own, held in WORDS, and hud.csv's where it has them; the script
# prints any word that overruns its room.
# The world is SEED's region with days of five ticks, lived YEARS years, as evidence/pr-180 does.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://trade.gd -- <dir>
extends SceneTree

const ViewScale := preload("res://view_scale.gd")
const PersonName := preload("res://hud/person_name.gd")
const SETTLE_FRAMES := 8
const SEED := 2026
const YEARS := 10
const TICKS_PER_DAY := 5
const DAYS_PER_YEAR := 120
const TALK_REACH := 2
const LOCALES := ["ko", "en"]
const KEPT := "user://trade_mockup.cfg"
const RUNS := [
	{"name": "pc_1920x1080", "window": Vector2i(1920, 1080), "touch": false},
	{"name": "touch_1920x1080", "window": Vector2i(1920, 1080), "touch": true},
]
const VIEWS := ["menu", "trade", "bought", "short", "hud", "used", "full"]
const TOUCH_VIEWS := ["touch"]
## The wares the mockup assumes: a healing herb and a qi-restoring elixir, each its price in silver,
## the health or qi it restores, its key on the belt and its icon.
const WARES := [
	{"name": "HERB", "price": 10, "health": 30, "qi": 0, "key": "Z", "icon": "herb"},
	{"name": "ELIXIR", "price": 60, "health": 0, "qi": 2000, "key": "X", "icon": "pill"},
]
## The player's silver and what it holds before buying, by ware.
const SILVER := 45
const HELD := [1, 0]
## The player's health in the wounded views, of a newborn's 120.
const WOUNDED := 70

const WORDS := {
	"OFFER_NEWS": ["요즘 소식이 있소?", "Any news?"],
	"OFFER_TRADE": ["물건 좀 봅시다.", "Show me your wares."],
	"FAREWELL": ["잘 있으시오.", "Farewell."],
	"GREETING": ["무슨 일이오?", "What is it?"],
	"MERCHANT": ["의술 · 약장수", "Medicine · seller"],
	"TRADE_GREETING": ["무엇이 필요하오?", "What do you need?"],
	"SILVER": ["%d냥", "%d silver"],
	"HERB": ["약초", "Healing herb"],
	"ELIXIR": ["소환단", "Lesser Return Pill"],
	"HEALTH_UP": ["체력 +%d", "Health +%d"],
	"QI_UP": ["내공 +%s", "Qi +%s"],
	"HELD": ["%d개", "%d held"],
	"BOUGHT": ["샀다 · %s", "Bought · %s"],
	"SHORT": ["은이 모자라다", "Not enough silver"],
	"USED": ["%s · %s", "%s · %s"],
	"FULL": ["체력이 이미 가득하다", "Health is already full"],
	"CLOSE": ["Esc 닫기", "Esc Close"],
	"KEYS_MENU": ["방향키 고르기 · Enter 말하기 · Esc 닫기", "Arrows choose · Enter say · Esc close"],
	"KEYS_TRADE": ["방향키 고르기 · Enter 사기 · Esc 돌아가기", "Arrows choose · Enter buy · Esc back"],
	"NO_SECT": ["문파 없음", "No sect"],
	"AGE": ["%d세", "Age %d"],
}
# The room each word is given, for the overrun check.
const ROOM := {"OFFER_TRADE": 280, "TRADE_GREETING": 200, "SILVER": 70, "HERB": 104, "ELIXIR": 104,
		"HEALTH_UP": 70, "QI_UP": 70, "HELD": 44, "BOUGHT": 300, "SHORT": 300, "USED": 300, "FULL": 300,
		"KEYS_TRADE": 400, "MERCHANT": 84}

# Each icon's pixels, 16 wide, one letter a colour in INKS, and the silver ingot's for the strip.
const ICONS := {
	"herb": ["................", "......gg........", ".....gGGg..gg...", ".....gGGg.gGGg..",
		"......gg..gGGg..", ".......s...gg...", ".gg....s..s.....", "gGGg...s.s......",
		"gGGg....ss......", ".gg.s...s.......", ".....s..s.......", "......s.s.......",
		".......ss.......", "........r.......", ".......rrr......", "......r.r.r....."],
	"pill": ["................", "................", "................", ".....kkkkkk.....",
		"....kRRRRRRk....", "...kRYYRRRRRk...", "...kRYRRRRRRk...", "...kRRRRRRRRk...",
		"...kRRRRRRRrk...", "...kRRRRRRrrk...", "....kRRRrrrk....", ".....kkkkkk.....",
		"................", "................", "................", "................"],
	"silver": ["................", "................", "................", ".....kkkkk......",
		".kk.kSWWWSk.kk..", "kSSkkSSSSSkkSSk.", "kSWSSSSSSSSSSSk.", ".kSSSSSSSSSSSk..",
		"..kSSSSSSSSSk...", "...kkkkkkkkk....", "................", "................",
		"................", "................", "................", "................"],
}
const INKS := {"g": Color("2f8c6e"), "G": Color("4fc49a"), "s": Color("4f7a3f"), "r": Color("c9a36b"),
	"k": Color("14101a"), "R": Color("d63a2f"), "Y": Color("f0d080"), "S": Color("8a94a0"),
	"W": Color("efe3c2")}


## A panel that draws the mockup's icons and numbers.
class Drawn extends "res://hud/panel.gd":
	# Draws icon `name` from `top_left`, a pixel to a pixel, darkened when `dim`.
	func icon(name: String, top_left: Vector2, dim := false) -> void:
		var rows: Array = ICONS[name]
		for y in rows.size():
			var row: String = rows[y]
			for x in row.length():
				var letter := row[x]
				if letter != ".":
					var colour: Color = INKS[letter]
					draw_rect(Rect2(top_left + Vector2(x, y), Vector2.ONE), colour.darkened(0.5) if dim else colour)

	# `n` with a comma before its thousands.
	func thousands(n: int) -> String:
		var s := str(n)
		return s if s.length() <= 3 else s.left(-3) + "," + s.right(3)


## The proposed trade in the dialogue window, drawn from what `view` holds, in the native 480x270:
## the person's card on the left as the dialogue draws it, and on the right, in place of the menu,
## its wares, one row each: icon, name, effect, how many the player holds, and price; the player's
## silver over them, and a line for what was bought or why not.
class Dialogue extends Drawn:
	const RECT := Rect2(20, 90, 440, 146)
	const CARD_RECT := Rect2(30, 116, 84, 90)
	const SAY_X := 132
	const SAY_Y := 116
	const ROW := 20
	const CRIMSON_LIGHT := Color("d63a2f")
	const FACTION_TEXT: Array[Color] = [Color("5b9ae0"), Color("d63a2f"), Color("e8a830")]

	## What the window shows: `words`, `person`, `name`, `faction`, `colour`, `merchant`, `age`,
	## `mode`, "menu" or "trade", `offers` (words), `chosen`, `silver`, `held`, `line` and `refused`.
	var view := {}

	func w(key: String) -> String:
		return view.words[key]

	func _draw() -> void:
		if view.is_empty():
			return
		_font = get_theme_default_font() if _font == null else _font
		var trading: bool = view.mode == "trade"
		_dialog(RECT, view.name, "" if trading else w("CLOSE"), w("KEYS_TRADE") if trading else w("KEYS_MENU"))
		var c := CARD_RECT
		draw_rect(Rect2(c.position + Vector2(26, 2), Vector2(32, 48)), Palette.INK_1)
		draw_rect(Rect2(c.position + Vector2(26, 34), Vector2(32, 16)), Palette.GRASS_3)
		draw_rect(Rect2(c.position + Vector2(26, 34), Vector2(32, 1)), Palette.GRASS_4)
		_portrait(view.person, Rect2(c.position + Vector2(26, 2), Vector2(32, 48)), false)
		_text(Vector2(c.position.x, c.position.y + 54), fit(view.faction, c.size.x), view.colour)
		# A merchant's trade stands where a sect would: its best discipline, medicine, which makes it one.
		_text(Vector2(c.position.x, c.position.y + 68), fit(view.merchant, c.size.x), Palette.JADE_3)
		_text(Vector2(c.position.x, c.position.y + 82), view.age, Palette.STONE_4)
		draw_rect(Rect2(SAY_X - 10, RECT.position.y + 26, 1, RECT.size.y - 52), Palette.INK_3)
		var room := RECT.end.x - 12 - SAY_X
		if not trading:
			_text(Vector2(SAY_X, SAY_Y), "“" + w("GREETING") + "”", Palette.GOLD_4)
			var items: Array[Dictionary] = []
			for offer: String in view.offers:
				items.append({"text": offer})
			_menu(Vector2(SAY_X, SAY_Y + 22), room, items, view.chosen)
			return
		_text(Vector2(SAY_X, SAY_Y), "“" + w("TRADE_GREETING") + "”", Palette.GOLD_4)
		var silver := w("SILVER") % view.silver
		var right := RECT.end.x - 12
		_text(Vector2(right - _width(silver), SAY_Y), silver, Palette.PAPER_3)
		icon("silver", Vector2(right - _width(silver) - 18, SAY_Y - 2))
		for n in WARES.size():
			var ware: Dictionary = WARES[n]
			var y := SAY_Y + 22 + n * ROW
			if n > 0:
				draw_rect(Rect2(SAY_X, y - 3, room, 1), Palette.INK_3)
			var on: bool = n == view.chosen
			if on:
				draw_rect(Rect2(SAY_X, y - 2, room, ROW - 2), Palette.INK_2)
				draw_rect(Rect2(SAY_X, y - 2, 1, ROW - 2), Palette.JADE_3)
			icon(ware.icon, Vector2(SAY_X + 4, y - 1))
			_text(Vector2(SAY_X + 24, y + 1), w(ware.name), Palette.GOLD_4 if on else Palette.PAPER_3)
			var effect := w("HEALTH_UP") % ware.health if ware.health > 0 else w("QI_UP") % thousands(ware.qi)
			_text(Vector2(SAY_X + 132, y + 1), effect, Color("d63a2f") if ware.health > 0 else Palette.AZURE_4)
			_text(Vector2(SAY_X + 204, y + 1), w("HELD") % view.held[n], Palette.STONE_4)
			var price := w("SILVER") % ware.price
			var afford: bool = view.silver >= ware.price
			_text(Vector2(right - 4 - _width(price), y + 1), price, Palette.GOLD_3 if afford else Palette.STONE_3)
		if not view.line.is_empty():
			var y := SAY_Y + 22 + WARES.size() * ROW + 6
			_text(Vector2(SAY_X, y), view.line, CRIMSON_LIGHT if view.refused else Palette.JADE_3)

	func width_of(text: String) -> float:
		_font = get_theme_default_font() if _font == null else _font
		return _width(text)


## The proposed HUD additions, in the native 480x270: the player's silver on a strip under the
## leader's, and, on PC, the wares held on a belt right of the technique bar, each cell its key, its
## icon and how many, greyed at none.
class Purse extends Drawn:
	const STRIP := Rect2(4, 92, 84, 18)
	const BELT_STEP := 32
	var silver := 0
	var held: Array = []
	var belt := true
	var unit := "%d"
	## Where the belt stands: right of the technique bar, on its line.
	var belt_at := Vector2(327, 240)

	func _draw() -> void:
		_font = get_theme_default_font() if _font == null else _font
		_frame(STRIP, Palette.INK_2)
		icon("silver", STRIP.position + Vector2(4, 1))
		var text := unit % silver
		_text(Vector2(STRIP.end.x - 6 - _width(text), STRIP.position.y + 3), text, Palette.PAPER_3)
		if not belt:
			return
		var rect := Rect2(belt_at, Vector2(WARES.size() * BELT_STEP + 6, 26))
		_frame(rect, Palette.INK_2)
		for n in WARES.size():
			var cell := Rect2(rect.position + Vector2(4 + n * BELT_STEP, 3), Vector2(30, 20))
			draw_rect(cell, Palette.INK_2)
			draw_rect(cell.grow(-1), Palette.INK_1)
			var none: bool = held[n] == 0
			icon(WARES[n].icon, cell.position + Vector2(12, 2), none)
			_text(cell.position + Vector2(2, 3), WARES[n].key, Palette.STONE_3 if none else Palette.GOLD_3)
			var count := str(held[n])
			_text(cell.end - Vector2(_width(count) + 1, 12), count, Palette.STONE_3 if none else Palette.PAPER_3, true)


var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _dialogue: Dialogue
var _purse: Purse
var _pair := {}
var _checked := false


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for run: Dictionary in RUNS:
			for view: String in (TOUCH_VIEWS if run.touch else VIEWS):
				_shots.append({"locale": locale, "run": run, "view": view})
	_world = _lived()
	_pair = _merchant(_world)
	if _pair.is_empty():
		push_error("trade: no merchant found")
		quit(1)
		return
	_approach(_world, _pair)
	print("trade: army %d of %s talks to person %d of %s, %d cells apart" % [
			_pair.army, _world.faction_names()[_world.army_faction(_pair.army)], _pair.person,
			_world.faction_names()[_world.person_faction(_pair.person)], _apart(_world, _pair)])
	_open()


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


# The player's army, or the first army that has one, and the nearest living person of a faction not
# hostile to it whose best discipline, its highest attainment, is medicine: {army, person}; or empty
# for none.
func _merchant(world: WorldAdapter) -> Dictionary:
	var medicine := Array(world.discipline_names()).find("medicine")
	for army in [world.player_army()] + range(world.army_count()):
		var roster := world.roster(army)
		if roster.is_empty():
			continue
		var mine := world.army_faction(army)
		var here := _cell(world.person_position(roster[0]))
		var best := {}
		var nearest := INF
		for other in world.army_count():
			var relation := world.relation(mine, world.army_faction(other))
			if other == army or relation == WorldAdapter.RELATION_HOSTILE or relation == WorldAdapter.RELATION_AT_WAR:
				continue
			for person in world.roster(other):
				var attainment := Array(world.person_attainment(person))
				if attainment.find(attainment.max()) != medicine:
					continue
				var apart := here.distance_to(_cell(world.person_position(person)))
				if apart < nearest:
					nearest = apart
					best = {"army": army, "person": person}
		if not best.is_empty():
			return best
	return {}


# Marks `pair`'s army steered and walks it toward its person until it stands within TALK_REACH,
# its person held where it stands.
func _approach(world: WorldAdapter, pair: Dictionary) -> void:
	world.steer(pair.army, true)
	for army in world.army_count():
		if world.roster(army).has(pair.person):
			world.steer(army, true)
			world.order(army, Vector2i(_cell(world.person_position(pair.person))),
					PackedInt64Array(range(world.skill_count(army))))
	for i in 3 * DAYS_PER_YEAR * TICKS_PER_DAY:
		if _apart(world, pair) <= TALK_REACH:
			return
		if i % 20 == 0:
			world.order(pair.army, Vector2i(_cell(world.person_position(pair.person))),
					PackedInt64Array(range(world.skill_count(pair.army))))
		world.tick()
	push_error("trade: army %d did not reach person %d" % [pair.army, pair.person])


func _open() -> void:
	if _play != null:
		_play.queue_free()
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)
	_step = 0


func _process(_delta: float) -> bool:
	_frame += 1
	# The scene runs until the shot is composed, then holds, so what the mockup sets stays drawn.
	if _step == 2 and _play.world != null:
		_play._process(0.0)
	if _frame % SETTLE_FRAMES != 0:
		return false
	var shot: Dictionary = _shots[_shot_at]
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
			_play.steered = _pair.army
			_play._art.steered = _pair.army
			_play._body = _world.roster(_pair.army)[0]
			hud.show_player(_world, _pair.army)
			_play._process(0.0)
			hud.labels.notice.visible = false
			_dialogue = Dialogue.new()
			_dialogue.use_font(hud.arts._font)
			_dialogue._world = _world
			hud.add_child(_dialogue)
			_purse = Purse.new()
			_purse.use_font(hud.arts._font)
			hud.add_child(_purse)
			hud.move_child(_purse, 0)
			if not _checked:
				_check_rooms()
		2:
			var room: Vector2 = hud.room
			_dialogue.position = Vector2(floorf((room.x - _dialogue.SCREEN.x) / 2.0), room.y - _dialogue.SCREEN.y)
			_dialogue.visible = shot.view in ["menu", "trade", "bought", "short"]
			_dialogue.view = _compose(shot.view)
			_purse.visible = true
			_purse.position = Vector2.ZERO
			var bar: Control = hud._skill_bar
			_purse.belt_at = bar.position + Vector2(bar.size.x + 4, 0)
			_purse.unit = _word("SILVER")
			_purse.silver = _dialogue.view.silver
			_purse.held = _dialogue.view.held
			_purse.belt = not shot.run.touch
			_wound(hud, shot.view)
			# The prompt to talk hides while the window is open, as play.gd hides it.
			hud.talk_prompt.visible = not _dialogue.visible and not shot.run.touch
			_purse.queue_redraw()
			_dialogue.queue_redraw()
		3:
			_save(shot)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("trade: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


# The player's health and notice in the HUD views: wounded, healed by a herb, or refused at full.
func _wound(hud: CanvasLayer, view: String) -> void:
	var health: int = {"hud": WOUNDED, "used": WOUNDED + WARES[0].health}.get(view, 120)
	hud.hp_bar.value = health * 100.0 / 120.0
	hud.labels.hp_value.text = "%d%%" % roundi(health * 100.0 / 120.0)
	hud.labels.notice.visible = false
	if view == "used":
		hud._notice(_word("USED") % [_word("HERB"), _word("HEALTH_UP") % WARES[0].health])
	elif view == "full":
		hud._notice(_word("FULL"))


func _word(key: String) -> String:
	return WORDS[key][LOCALES.find(TranslationServer.get_locale().left(2))]


func _words() -> Dictionary:
	var words := {}
	for key: String in WORDS:
		words[key] = _word(key)
	return words


# What the window and purse show for `view`.
func _compose(view: String) -> Dictionary:
	var person: int = _pair.person
	var faction := _world.person_faction(person)
	var colours: Array[Color] = [Color("5b9ae0"), Color("d63a2f"), Color("e8a830")]
	var silver := SILVER
	var held: Array = HELD.duplicate()
	var line := ""
	if view in ["bought", "short", "used", "hud", "touch", "full"]:
		silver -= WARES[0].price
		held[0] += 1
	if view == "used":
		held[0] -= 1
	if view == "bought":
		line = _word("BOUGHT") % _word("HERB")
	elif view == "short":
		line = _word("SHORT")
	return {"words": _words(), "person": person, "name": PersonName.of(_world, person, false),
		"faction": TranslationServer.translate("FACTION_" + _world.faction_names()[faction].to_upper()),
		"colour": colours[faction % 3], "merchant": _word("MERCHANT"),
		"age": _word("AGE") % _world.person_age(person), "mode": "menu" if view == "menu" else "trade",
		"offers": [_word("OFFER_NEWS"), _word("OFFER_TRADE"), _word("FAREWELL")],
		"chosen": {"menu": 1, "short": 1}.get(view, 0), "silver": silver, "held": held, "line": line,
		"refused": view == "short"}


# Prints every word, in both locales, that overruns the room ROOM gives it.
func _check_rooms() -> void:
	_checked = true
	var over := 0
	for key: String in WORDS:
		for n in LOCALES.size():
			var text: String = WORDS[key][n].replace("%s", "2,000" if key == "QI_UP" else WORDS["HERB"][n]).replace("%d", "000")
			var width := _dialogue.width_of(text)
			if ROOM.has(key) and width > ROOM[key]:
				over += 1
				print("trade: %s (%s) is %d past %d" % [key, text, width, ROOM[key]])
	print("trade: %d words overrun their room" % over)


func _save(shot: Dictionary) -> void:
	var name: String = "%s_%s_%s" % [shot.view, shot.run.name, shot.locale]
	var image := root.get_texture().get_image()
	image.save_png(_dir.path_join(name + ".png"))
	print("trade: %s.png %dx%d, HUD %dx in %s, sha256 %s" % [name, image.get_width(), image.get_height(),
			ViewScale.hud_scale(shot.run.window, shot.run.touch), ViewScale.hud_room(shot.run.window, shot.run.touch),
			FileAccess.get_sha256(_dir.path_join(name + ".png")).left(16)])
