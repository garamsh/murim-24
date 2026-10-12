## The arts panel, drawn as art/mockup/arts_panel.png lays it out on PC and arts_panel_touch.png on
## a touch screen. It shows the steered person's realm on the ladder of realms with the next one
## and the qi it reaches at, its qi against its store, its heart method and the nature of it, and
## every art it knows a skill of, then its army's skills: each skill with its qi cost, the realm it
## needs and its cooldown, greyed while not learned and its realm in crimson while not reached, and
## the key of the slot it is on. Along the foot on PC, the loadout's five slots on Q W E R T; on a
## touch screen, the tapped skill's details, while the touch controls draw the arc beside it. A
## skill chosen, with the arrows or a tap, is put on a slot, or taken off one, in the loadout it is
## given. Every word comes from hud.csv through Godot's translations, and every name from the core
## in the locale's language. It reads the world through the adapter and holds no game rule.
extends "res://hud/panel.gd"

const Loadout := preload("res://hud/loadout.gd")
## The keys of the loadout's slots, as the play scene binds them.
const SLOT_KEYS := ["Q", "W", "E", "R", "T"]
# The PC mockup's layout, in the native 480x270, its left column 16 pixels wider so the longest
# English realm and art names fit beside their labels.
const DIALOG := Rect2(8, 6, 464, 258)
const COLUMN := Vector2(18, 32)
const COLUMN_WIDTH := 118
const DIVIDER_X := 142
const LIST_LEFT := 148
const LIST_RIGHT := 462
const NAME_X := 158
const KEY_X := 451
const COOLDOWN_RIGHT := 446
const REALM_RIGHT := 402
const COST_RIGHT := 320
const LIST_TOP := 30
const ROW := 14
## How many of the list's rows, art headers and skills, show at once under its column titles.
const LIST_ROWS := 10
const FOOT_Y := 190
const SLOT_TILE := 30
const SLOT_STEP := 52
# The touch mockup's layout.
const TOUCH_DIALOG := Rect2(4, 4, 302, 262)
const TILE := 42
const TILE_GAP := 4
const GROUPS_LEFT := 12
const GROUPS_TOP := 26
const GROUP_STEP := 62
## How many groups of tiles, one an art's, show at once on a touch screen.
const GROUPS_SHOWN := 3
const DETAIL := Rect2(158, 26, 140, 0)
const TOUCH_DIVIDER_X := 152
## The room each word and name is given, by what it is, for the fit tests.
const NAME_ROOM := 140
const REALM_ROOM := 78
const NATURE_ROOM := 60
const POLARITY_ROOM := 30
const ROOM := {"ARTS_TITLE": 40, "ARTS_CLOSE": 50, "ARTS_REALM": 40, "ARTS_NEXT": 30, "ARTS_QI": 40,
		"ARTS_HEART": 70, "ARTS_HEART_TAG": 30, "ARTS_WEAKER": 36, "ARTS_UNLEARNED": 84,
		"ARTS_LOCKED": 84, "ARTS_SKILL": 40, "ARTS_COST": 32, "ARTS_REALM_COLUMN": 78,
		"ARTS_COOLDOWN": 40, "ARTS_SHARED": 40, "ARTS_LOADOUT": 44, "ARTS_KEYS": 440,
		"ARTS_TOUCH_KEYS": 290, "ARTS_RANGE": 40, "ARTS_AREA": 40, "ARTS_AREA_ONE": 40,
		"ARTS_CELLS": 40, "ARTS_SECONDS": 40, "ARTS_FACTION": 100, "ARTS_BASIC": NAME_ROOM,
		"ARTS_MEND": NAME_ROOM, "ARTS_TORCH": NAME_ROOM}
# Each orientation's ramp, dark and light, by WorldAdapter.ORIENTATION_* constant.
const NATURE_RAMPS := {
	WorldAdapter.ORIENTATION_ORTHODOX: [Palette.AZURE_2, Palette.AZURE_5],
	WorldAdapter.ORIENTATION_UNORTHODOX: [Palette.AMBER_2, Palette.AMBER_5],
	WorldAdapter.ORIENTATION_DEMONIC: [Palette.CRIMSON_2, Palette.CRIMSON_5],
}
const ORIENTATION_KEYS := {
	WorldAdapter.ORIENTATION_ORTHODOX: "ARTS_ORTHODOX",
	WorldAdapter.ORIENTATION_UNORTHODOX: "ARTS_UNORTHODOX",
	WorldAdapter.ORIENTATION_DEMONIC: "ARTS_DEMONIC",
}
const POLARITY_KEYS := {WorldAdapter.POLARITY_YANG: "ARTS_YANG", WorldAdapter.POLARITY_YIN: "ARTS_YIN"}

## Whether the panel is laid out for a touch screen.
var touch_layout := false
## The keys that put the skill chosen on each slot, the first slot's first, and the key that takes
## it off its slot, as the play scene binds them.
var slot_keys: Array = []
var clear_key := KEY_NONE
## The loadout the panel puts skills on.
var loadout: Loadout
## The skill chosen, as Loadout.identity gives it, or an empty Dictionary for none.
var selected := {}
## The slot a skill was last put on while the panel is open, or -1.
var just := -1
## What the panel shows, read from the world in the current locale: `realm`, `rank`, `next` and
## `next_qi`, the next realm's name and the qi that reaches it, `qi` and `capacity`, `heart` with its `name` and `nature` tags, and `groups`, each art the person
## knows a skill of and then its army's skills: a Dictionary of its `name`, `tags`, `nature` words,
## `faction` for the army's, and `skills`, each a Dictionary of `held`, as Loadout.identity gives
## it, `name`, `icon`, `known`, `locked`, `cost`, `realm`, `cooldown`, `range` and `area`.
var shown := {}

var _army := -1
# The first of the list's rows on PC, and the first group of tiles on a touch screen, shown.
var _first_row := 0
var _first_group := 0


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED and _world != null:
		_compose()


## Returns `entry`, a name the core gives in `korean` and `romanised`, in the current locale.
static func named(entry: Dictionary) -> String:
	return entry.korean if TranslationServer.get_locale().begins_with("ko") else entry.romanised


## Returns the name of realm `rank`, counted from 1, of `world`'s realms in the current locale, or
## a dash for no realm.
static func realm_name(world: WorldAdapter, rank: int) -> String:
	return _realm_of(world.realms(), rank)


## Returns `value` with its thousands grouped by commas.
static func grouped(value: int) -> String:
	var digits := str(absi(value))
	var text := ""
	while digits.length() > 3:
		text = "," + digits.right(3) + text
		digits = digits.left(-3)
	return ("-" if value < 0 else "") + digits + text


## Opens the panel on `army`'s person, putting skills on `chosen`'s slots, laid out for a touch
## screen when `touch` is set. On PC the first skill the person has is chosen; on a touch screen,
## none until one is tapped.
func show_arts(world: WorldAdapter, army: int, chosen: Loadout, touch: bool) -> void:
	_world = world
	_army = army
	loadout = chosen
	touch_layout = touch
	just = -1
	_first_row = 0
	_first_group = 0
	selected = {}
	_compose()
	if not touch:
		var known := _known_skills()
		selected = known.front() if not known.is_empty() else {}
		_compose()
	visible = true


## Reads the panel again from the world, keeping the choice: the qi as it stands, and a skill
## learned since it opened.
func reread() -> void:
	_compose()


## Moves the choice among the skills the person has: down for the next, up for the one before.
## Returns whether it moved.
func move(direction: Vector2i) -> bool:
	var known := _known_skills()
	var at := known.find(selected) + direction.y
	if direction.y == 0 or at < 0 or at >= known.size():
		return false
	selected = known[at]
	_compose()
	return true


## Works the panel with `event`: up and down choose among the skills, a slot's key puts the skill
## chosen on that slot, the clear key takes it off its slot, and the toggle key or Escape closes it.
## Returns "placed" when the loadout changed, "close", "done", or "" when the key did nothing.
func key(event: InputEventKey) -> String:
	if event.keycode == toggle_key or event.keycode == KEY_ESCAPE:
		return "close"
	if DIRECTIONS.has(event.keycode):
		return _done(move(DIRECTIONS[event.keycode]))
	var slot := slot_keys.find(event.keycode)
	if (slot >= 0 and assign(slot)) or (event.keycode == clear_key and clear_selected()):
		return "placed"
	return ""


## Puts the chosen skill on `slot`, taking it off any other. Returns whether a skill was chosen.
func assign(slot: int) -> bool:
	if selected.is_empty():
		return false
	loadout.assign(slot, selected)
	just = slot
	_compose()
	return true


## Takes the chosen skill off the slot it is on. Returns whether it was on one.
func clear_selected() -> bool:
	if selected.is_empty() or loadout.slot_of(selected) < 0:
		return false
	loadout.clear(selected)
	just = -1
	_compose()
	return true


## Chooses the skill whose tile is at `at`, on a touch screen. Returns whether a tile of a skill
## the person has was there.
func tap(at: Vector2) -> bool:
	for tile: Dictionary in tiles():
		if tile.rect.has_point(at) and tile.skill.known:
			selected = tile.skill.held
			_compose()
			return true
	return false


## Shows the next groups of tiles, `way` 1, or those before, -1, on a touch screen. Returns whether
## it moved.
func scroll(way: int) -> bool:
	var first := clampi(_first_group + way, 0, maxi(shown.groups.size() - GROUPS_SHOWN, 0))
	if first == _first_group:
		return false
	_first_group = first
	queue_redraw()
	return true


# The skills the person has, in the order the panel lists them.
func _known_skills() -> Array[Dictionary]:
	var known: Array[Dictionary] = []
	for group: Dictionary in shown.get("groups", []):
		for skill: Dictionary in group.skills:
			if skill.known:
				known.append(skill.held)
	return known


# Reads what the panel shows from the world, in the current locale.
func _compose() -> void:
	var roster := _world.roster(_army)
	shown = {"groups": []} if roster.is_empty() else compose(read(_world, roster[0]))
	_keep_in_view()
	queue_redraw()


## Returns what the panel reads of `person` from `world`: the world's `realms` and `arts`, and the
## person's realm `rank`, `qi`, `capacity`, `nature` and `skills`, as the adapter gives them.
static func read(world: WorldAdapter, person: int) -> Dictionary:
	return {"realms": world.realms(), "arts": world.arts(), "rank": world.person_realm(person),
			"qi": world.person_qi(person), "capacity": world.person_qi_capacity(person),
			"nature": world.person_nature(person), "skills": world.person_skills(person)}


## Returns what the panel shows of `reads`, as read() gives them, in the current locale: `realm`,
## `rank`, `realms`, `next` and `next_qi`, `qi` and `capacity`, `heart`, and `groups`, as `shown`
## holds them. An art of another nature than the person's, in orientation or polarity, carries the
## weaker tag; the art learned first, the heart method's.
static func compose(reads: Dictionary) -> Dictionary:
	var realms: Array = reads.realms
	var arts: Array = reads.arts
	var skills: Array = reads.skills
	var nature: Dictionary = reads.nature
	var rank: int = reads.rank
	var composed := {"groups": [], "rank": rank, "realms": realms.size(), "qi": reads.qi,
			"capacity": reads.capacity, "realm": _realm_of(realms, rank),
			"next": named(realms[rank]) if rank < realms.size() else "—",
			"next_qi": grouped(realms[rank].qi) if rank < realms.size() else ""}
	var order: Array[int] = []
	for skill: Dictionary in skills:
		if skill.art >= 0 and not order.has(int(skill.art)):
			order.append(int(skill.art))
	composed.heart = {}
	if not order.is_empty():
		composed.heart = {"name": named(arts[order[0]]), "nature": _nature_words(arts[order[0]]),
				"ramp": NATURE_RAMPS[int(arts[order[0]].orientation)]}
	for art: int in order:
		var group := {"name": named(arts[art]), "nature": _nature_words(arts[art]),
				"ramp": NATURE_RAMPS[int(arts[art].orientation)], "tags": [], "skills": []}
		if art == order[0]:
			group.tags.append({"text": _tr("ARTS_HEART_TAG"), "fill": Palette.JADE_2})
		elif not nature.is_empty() and (arts[art].orientation != nature.orientation \
				or arts[art].polarity != nature.polarity):
			group.tags.append({"text": _tr("ARTS_WEAKER"), "fill": Palette.CRIMSON_2})
		for index in arts[art].skills.size():
			var taught: Dictionary = arts[art].skills[index]
			var held := {"art": art, "skill": index}
			group.skills.append({"held": held, "name": named(taught),
					"icon": _icon(taught), "known": _held_index(skills, held) >= 0,
					"locked": int(taught.rank) > rank, "cost": grouped(taught.qi_cost),
					"realm": _realm_of(realms, taught.rank),
					"cooldown": _tr("ARTS_SECONDS") % ceili(float(taught.cooldown) / WorldAdapter.TICKS_PER_SECOND),
					"range": _tr("ARTS_CELLS") % (int(taught.range) / WorldAdapter.CELL_SIZE),
					"area": _area(taught)})
		composed.groups.append(group)
	var drilled := {"name": _tr("ARTS_FACTION"), "nature": "", "ramp": [], "tags": [], "faction": true,
			"skills": []}
	for index in skills.size():
		if skills[index].art >= 0:
			continue
		var skill: Dictionary = skills[index]
		drilled.skills.append({"held": Loadout.identity(skills, index), "name": _tr(_drilled_key(skill)),
				"icon": _icon(skill), "known": true, "locked": false, "cost": "-", "realm": "-",
				"cooldown": _tr("ARTS_SHARED"),
				"range": _tr("ARTS_CELLS") % (int(skill.range) / WorldAdapter.CELL_SIZE), "area": _area(skill)})
	if not drilled.skills.is_empty():
		composed.groups.append(drilled)
	return composed


static func _tr(key: String) -> String:
	return TranslationServer.translate(key)


# The name of realm `rank`, counted from 1, among `realms`, or a dash for no realm.
static func _realm_of(realms: Array, rank: int) -> String:
	return named(realms[rank - 1]) if rank >= 1 and rank <= realms.size() else "—"


# The words of `art`'s nature: its orientation and its polarity, joined.
static func _nature_words(art: Dictionary) -> String:
	return "%s · %s" % [_tr(ORIENTATION_KEYS[int(art.orientation)]), _tr(POLARITY_KEYS[int(art.polarity)])]


# The index among `skills` of the skill `held` names, or -1.
static func _held_index(skills: Array, held: Dictionary) -> int:
	for index in skills.size():
		if Loadout.identity(skills, index) == held:
			return index
	return -1


# The hud.csv key naming an army's skill by what it does: mending, burning, or striking.
static func _drilled_key(skill: Dictionary) -> String:
	if int(skill.structure_damage) < 0:
		return "ARTS_MEND"
	if int(skill.body_damage) == 0 and int(skill.structure_damage) > 0:
		return "ARTS_TORCH"
	return "ARTS_BASIC"


# How far a skill's area reaches: one person, or its radius in cells.
static func _area(skill: Dictionary) -> String:
	var cells := int(skill.radius) / WorldAdapter.CELL_SIZE
	return _tr("ARTS_AREA_ONE") if int(skill.radius) <= 0 else _tr("ARTS_CELLS") % maxi(cells, 1)


# A skill's icon tag in art/icons/skill_icons: a skill whose radius reaches past its target bursts,
# any other strikes one body.
static func _icon(skill: Dictionary) -> String:
	return "burst" if int(skill.radius) > 0 else "strike"


# The list's rows on PC, art headers and skills, each a Dictionary of `group` and, for a skill, `skill`.
func _rows() -> Array[Dictionary]:
	var rows: Array[Dictionary] = []
	for group: Dictionary in shown.get("groups", []):
		rows.append({"group": group})
		for skill: Dictionary in group.skills:
			rows.append({"group": group, "skill": skill})
	return rows


# Scrolls the list on PC so that the chosen skill is among the rows shown.
func _keep_in_view() -> void:
	if touch_layout or selected.is_empty():
		return
	var rows := _rows()
	for at in rows.size():
		if rows[at].has("skill") and rows[at].skill.held == selected:
			if at < _first_row:
				_first_row = maxi(at - 1, 0)
			elif at >= _first_row + LIST_ROWS:
				_first_row = at - LIST_ROWS + 1


## Returns the tiles shown on a touch screen, each a Dictionary of its `rect`, in the HUD's pixels,
## its `skill` and its `group`.
func tiles() -> Array[Dictionary]:
	var shown_tiles: Array[Dictionary] = []
	if not touch_layout:
		return shown_tiles
	var groups: Array = shown.get("groups", [])
	for n in mini(GROUPS_SHOWN, groups.size() - _first_group):
		var group: Dictionary = groups[_first_group + n]
		var top := GROUPS_TOP + n * GROUP_STEP + 14
		for s in group.skills.size():
			shown_tiles.append({"rect": Rect2(GROUPS_LEFT + s * (TILE + TILE_GAP), top, TILE, TILE),
					"skill": group.skills[s], "group": group})
	return shown_tiles


func _draw() -> void:
	if shown.is_empty():
		return
	_dim()
	if touch_layout:
		_draw_touch()
	else:
		_draw_pc()


func _draw_pc() -> void:
	_dialog(DIALOG, tr("ARTS_TITLE"), tr("ARTS_CLOSE"), tr("ARTS_KEYS"))
	var y := _status(COLUMN, COLUMN_WIDTH)
	var x := COLUMN.x
	draw_rect(Rect2(x, y + 4, COLUMN_WIDTH, 1), Palette.INK_4)
	_text(Vector2(x, y + 8), tr("ARTS_HEART"), Palette.GOLD_4)
	if shown.heart.is_empty():
		_text(Vector2(x, y + 22), "—", Palette.STONE_4)
	else:
		_text(Vector2(x, y + 22), fit(shown.heart.name, COLUMN_WIDTH), Palette.PAPER_6)
		var tags: PackedStringArray = shown.heart.nature.split(" · ")
		var tx := x
		for tag in tags:
			tx += _tag_at(Vector2(tx, y + 37), tag, shown.heart.ramp[0]) + 3
	draw_rect(Rect2(x, y + 54, COLUMN_WIDTH, 1), Palette.INK_4)
	draw_rect(Rect2(x, y + 62, 6, 6), Palette.STONE_3)
	_text(Vector2(x + 10, y + 58), tr("ARTS_UNLEARNED"), Palette.STONE_4)
	draw_rect(Rect2(x, y + 76, 6, 6), Palette.CRIMSON_5)
	_text(Vector2(x + 10, y + 72), tr("ARTS_LOCKED"), Palette.STONE_4)
	draw_rect(Rect2(DIVIDER_X, LIST_TOP, 1, 160), Palette.INK_4)

	_text(Vector2(NAME_X, LIST_TOP), tr("ARTS_SKILL"), Palette.STONE_4)
	_right(COST_RIGHT, LIST_TOP, tr("ARTS_COST"), Palette.STONE_4)
	_right(REALM_RIGHT, LIST_TOP, tr("ARTS_REALM_COLUMN"), Palette.STONE_4)
	_right(COOLDOWN_RIGHT, LIST_TOP, tr("ARTS_COOLDOWN"), Palette.STONE_4)
	draw_rect(Rect2(LIST_LEFT, LIST_TOP + 13, LIST_RIGHT - LIST_LEFT, 1), Palette.INK_4)
	var rows := _rows()
	for n in mini(LIST_ROWS, rows.size() - _first_row):
		var row := rows[_first_row + n]
		var ly := LIST_TOP + (n + 1) * ROW
		if row.has("skill"):
			_skill_row(ly, row.skill, row.group.has("faction"))
		else:
			_art_header(ly, row.group)
	if _first_row > 0:
		_text(Vector2(LIST_RIGHT - 6, LIST_TOP + ROW), "▲", Palette.STONE_4)
	if _first_row + LIST_ROWS < rows.size():
		_text(Vector2(LIST_RIGHT - 6, LIST_TOP + LIST_ROWS * ROW), "▼", Palette.STONE_4)

	draw_rect(Rect2(DIALOG.position.x + 3, FOOT_Y - 4, DIALOG.size.x - 6, 1), Palette.INK_4)
	_text(Vector2(x, FOOT_Y + 14), tr("ARTS_LOADOUT"), Palette.GOLD_4)
	var first := LIST_LEFT + floori((LIST_RIGHT - LIST_LEFT - (4 * SLOT_STEP + SLOT_TILE)) / 2.0)
	for slot in Loadout.SLOTS:
		var sx := first + slot * SLOT_STEP
		_keycap(Vector2(sx + floori((SLOT_TILE - keycap_width(SLOT_KEYS[slot])) / 2.0), FOOT_Y), SLOT_KEYS[slot])
		var held: Dictionary = loadout.slots[slot]
		var skill := _skill_of(held)
		_tile(Rect2(sx, FOOT_Y + 15, SLOT_TILE, SLOT_TILE), skill.get("icon", ""), slot == just, false, false)
		if skill.is_empty():
			draw_rect(Rect2(sx + 11, FOOT_Y + 30, 9, 1), Palette.INK_5)
			draw_rect(Rect2(sx + 15, FOOT_Y + 26, 1, 9), Palette.INK_5)


# An art's header in the list: its name, its nature and its tag.
func _art_header(ly: float, group: Dictionary) -> void:
	var faction: bool = group.has("faction")
	var hx := LIST_LEFT + _width(group.name) + 6
	_text(Vector2(LIST_LEFT, ly), group.name, Palette.STONE_4 if faction else Palette.GOLD_4)
	if faction:
		return
	_text(Vector2(hx, ly), group.nature, group.ramp[1])
	hx += _width(group.nature) + 6
	for tag: Dictionary in group.tags:
		hx += _tag_at(Vector2(hx, ly + 1), tag.text, tag.fill) + 3


# A skill's row in the list: lit when chosen, greyed when not learned, its realm in crimson when
# not reached, and the key of its slot.
func _skill_row(ly: float, skill: Dictionary, faction: bool) -> void:
	var chosen: bool = skill.held == selected
	if chosen:
		draw_rect(Rect2(LIST_LEFT, ly - 1, LIST_RIGHT - LIST_LEFT + 2, ROW), Palette.INK_4)
		draw_rect(Rect2(LIST_LEFT, ly - 1, 1, ROW), Palette.JADE_4)
		for k in 4:
			draw_rect(Rect2(LIST_LEFT + 3 + k, ly + 3 + k, 1, 7 - 2 * k), Palette.GOLD_6)
	var value := Palette.PAPER_6 if skill.known else Palette.STONE_3
	_text(Vector2(NAME_X, ly), fit(skill.name, COST_RIGHT - 4 - _width(skill.cost) - NAME_X),
			Palette.GOLD_6 if chosen else value)
	_right(COST_RIGHT, ly, skill.cost, value)
	_right(REALM_RIGHT, ly, skill.realm, Palette.CRIMSON_5 if skill.locked and not faction else value)
	_right(COOLDOWN_RIGHT, ly, skill.cooldown, value)
	var slot := loadout.slot_of(skill.held)
	if slot >= 0:
		var lit := slot == just
		draw_rect(Rect2(KEY_X, ly, 11, 12), Palette.JADE_2 if lit else Palette.INK_5)
		draw_rect(Rect2(KEY_X, ly, 11, 11), Palette.JADE_4 if lit else Palette.INK_4)
		_text(Vector2(KEY_X + floori((11 - _width(SLOT_KEYS[slot])) / 2.0), ly), SLOT_KEYS[slot], Palette.PAPER_6)


func _draw_touch() -> void:
	_dialog(TOUCH_DIALOG, tr("ARTS_TITLE"), "", tr("ARTS_TOUCH_KEYS"))
	var groups: Array = shown.groups
	for n in mini(GROUPS_SHOWN, groups.size() - _first_group):
		var group: Dictionary = groups[_first_group + n]
		var gy := GROUPS_TOP + n * GROUP_STEP
		# The name cut short where it would push its tag past the tiles' column.
		var room := TOUCH_DIVIDER_X - 4 - GROUPS_LEFT
		for tag: Dictionary in group.tags:
			room -= _width(tag.text) + 6 + 4
		var name := fit(group.name, room)
		_text(Vector2(GROUPS_LEFT, gy), name, Palette.STONE_4 if group.has("faction") else Palette.GOLD_4)
		var hx := GROUPS_LEFT + _width(name) + 4
		for tag: Dictionary in group.tags:
			hx += _tag_at(Vector2(hx, gy + 1), tag.text, tag.fill) + 3
	for tile: Dictionary in tiles():
		var skill: Dictionary = tile.skill
		var rect: Rect2 = tile.rect
		_tile(rect, skill.icon, skill.held == selected, loadout.slot_of(skill.held) >= 0, not skill.known)
		if skill.locked:
			draw_rect(Rect2(rect.position.x + 8, rect.end.y - 6, TILE - 16, 1), Palette.CRIMSON_5)
	if _first_group > 0:
		_text(Vector2(TOUCH_DIVIDER_X - 10, GROUPS_TOP), "▲", Palette.STONE_4)
	if _first_group + GROUPS_SHOWN < groups.size():
		_text(Vector2(TOUCH_DIVIDER_X - 10, TOUCH_DIALOG.end.y - 34), "▼", Palette.STONE_4)
	var bottom := TOUCH_DIALOG.end.y - 22
	draw_rect(Rect2(TOUCH_DIVIDER_X, GROUPS_TOP, 1, bottom - GROUPS_TOP), Palette.INK_4)
	var y := _status(DETAIL.position, DETAIL.size.x)
	var x := DETAIL.position.x
	var w := DETAIL.size.x
	draw_rect(Rect2(x, y + 3, w, 1), Palette.INK_4)
	var skill := _skill_of(selected)
	if skill.is_empty():
		return
	var group := _group_of(selected)
	y += 7
	_text(Vector2(x, y), fit(skill.name, w), Palette.GOLD_6)
	_text(Vector2(x, y + 13), fit(group.name, w), Palette.PAPER_6)
	var tx := x
	if not group.has("faction"):
		for tag in String(group.nature).split(" · "):
			tx += _tag_at(Vector2(tx, y + 28), tag, group.ramp[0]) + 3
		for tag: Dictionary in group.tags:
			if tag.fill == Palette.CRIMSON_2:
				_tag_at(Vector2(tx, y + 28), tag.text, tag.fill)
	y += 44
	var rows := [["ARTS_COST", skill.cost], ["ARTS_REALM_COLUMN", skill.realm],
			["ARTS_COOLDOWN", skill.cooldown], ["ARTS_RANGE", skill.range], ["ARTS_AREA", skill.area]]
	for n in rows.size():
		_text(Vector2(x, y + n * 13), tr(rows[n][0]), Palette.STONE_4)
		_right(x + w, y + n * 13, rows[n][1], Palette.CRIMSON_5 if n == 1 and skill.locked else Palette.PAPER_6)


# The realm and qi block in a column `width` wide from `at`: the realm by name, a pip for each realm,
# gold for one passed, jade for the one held and ink for one ahead, the next realm and the qi that
# reaches it, then the qi left against the store and its bar. Returns the y below it.
func _status(at: Vector2, width: float) -> float:
	var x := at.x
	var y := at.y
	_text(Vector2(x, y), tr("ARTS_REALM"), Palette.GOLD_4)
	_right(x + width, y, shown.realm, Palette.PAPER_6)
	var realms: int = maxi(shown.realms, 1)
	var pip := floori((width + 2) / realms)
	for n in realms:
		var px := x + n * pip
		if n + 1 < shown.rank:
			draw_rect(Rect2(px, y + 15, pip - 2, 5), Palette.GOLD_3)
			draw_rect(Rect2(px, y + 15, pip - 2, 1), Palette.GOLD_6)
		elif n + 1 == shown.rank:
			draw_rect(Rect2(px, y + 15, pip - 2, 5), Palette.JADE_2)
			draw_rect(Rect2(px, y + 15, pip - 2, 1), Palette.JADE_7)
		else:
			draw_rect(Rect2(px, y + 15, pip - 2, 5), Palette.INK_4)
			draw_rect(Rect2(px, y + 19, pip - 2, 1), Palette.INK_5)
	# The next realm and the qi that reaches it, or the qi alone where both overrun the column.
	var next: String = ("%s %s" % [shown.next, shown.next_qi]).strip_edges()
	if _width(tr("ARTS_NEXT")) + 4 + _width(next) > width:
		next = shown.next_qi
	_text(Vector2(x, y + 24), tr("ARTS_NEXT"), Palette.STONE_4)
	_right(x + width, y + 24, next, Palette.STONE_4)
	draw_rect(Rect2(x, y + 40, width, 1), Palette.INK_4)
	_text(Vector2(x, y + 44), tr("ARTS_QI"), Palette.AZURE_5)
	_right(x + width, y + 44, "%s / %s" % [grouped(shown.qi), grouped(shown.capacity)], Palette.PAPER_6)
	draw_rect(Rect2(x, y + 58, width, 5), Palette.INK_1)
	var fill := floori((width - 2) * shown.qi / maxf(shown.capacity, 1))
	draw_rect(Rect2(x + 1, y + 59, fill, 3), Palette.AZURE_4)
	draw_rect(Rect2(x + 1, y + 59, fill, 1), Palette.AZURE_5)
	draw_rect(Rect2(x + 1 + fill, y + 59, width - 2 - fill, 3), Palette.INK_4)
	return y + 64


# A square tile in `rect` holding icon `icon`: an ink well in an ink rim, the rim jade when
# `chosen`, gold when `lit`, and the icon dimmed when `dim`.
func _tile(rect: Rect2, tag: String, chosen: bool, lit: bool, dim: bool) -> void:
	draw_rect(rect, Palette.INK_1)
	draw_rect(rect.grow(-1), Palette.JADE_4 if chosen else (Palette.GOLD_4 if lit else Palette.INK_5))
	draw_rect(rect.grow(-2), Palette.INK_4 if chosen else Palette.INK_2)
	var at := (rect.position + (rect.size - Vector2(16, 16)) / 2.0).floor()
	_draw_icon("skill_icons", tag, Rect2(at, Vector2(16, 16)), Palette.DIM_TINT if dim else Color.WHITE)


# A word on a filled slip whose top-left is `top_left`; returns its width.
func _tag_at(top_left: Vector2, text: String, fill: Color) -> float:
	return _tag(top_left + Vector2(_width(text) + 6, 0), text, fill)


# The skill `held` names among those shown, or an empty Dictionary.
func _skill_of(held: Dictionary) -> Dictionary:
	if held.is_empty():
		return {}
	for group: Dictionary in shown.get("groups", []):
		for skill: Dictionary in group.skills:
			if skill.held == held:
				return skill
	return {}


func _group_of(held: Dictionary) -> Dictionary:
	for group: Dictionary in shown.get("groups", []):
		for skill: Dictionary in group.skills:
			if skill.held == held:
				return group
	return {}
