# Mockup of names over the people near the player, proposed for #164, drawn over play.tscn with the
# HUD's own font and palette (hud/panel.gd), in each locale, at 1920x1080, the HUD at 2x in 960x540:
# - outline_<locale>.png: each living person within REACH cells of the steered person, itself left
#   out, named over its head in the colour of how its faction stands toward the player's, as
#   WorldAdapter.relation reads it: own and ally in jade, neutral in paper, hostile and at war in
#   crimson, each outlined in ink a pixel round. A hostile person's body has its one-pixel outline
#   drawn in crimson in place of ink.
# - ring_<locale>.png: the same, a hostile body ringed in crimson outside its ink outline instead.
# Names never cover one another, a HUD panel or the talk prompt: nearest first, a name that would
# overlap a nearer one's, a panel or the prompt is left out, and the person the prompt names is not
# named again. A person past REACH is never named.
# The crowd is SEED's region with days of five ticks: every two factions hostile by stance make peace,
# so the player's faction stands neutral toward the one hostile to it, and its leader declares war on
# the one allied to it, which stands at war, once the crowd has gathered; armies of each faction are steered to stand round the player, its own
# below it, the neutral to its left and the one at war to its right. The world then stands still, so
# every body is drawn in its standing frame.
# Run, after make build, with this file copied to the project root:
#   godot --path . --script res://nameplates.gd -- <dir>
extends SceneTree

const SETTLE_FRAMES := 8
const SEED := 2026
const TICKS_PER_DAY := 5
const WINDOW := Vector2i(1920, 1080)
const LOCALES := ["ko", "en"]
const KEPT := "user://nameplates_mockup.cfg"
const VARIANTS := ["outline", "ring"]
## The reach the mockup proposes, in cells from the steered person.
const REACH := 8
## Where each relation's armies are walked to, in cells from the player.
const POSTS := {
	WorldAdapter.RELATION_OWN: Vector2i(0, 3),
	WorldAdapter.RELATION_NEUTRAL: Vector2i(-5, 0),
	WorldAdapter.RELATION_ALLY: Vector2i(5, 0),
}
## Armies walked to each post.
const ARMIES_PER_POST := 5
const GATHER_TICKS := 6000
const CELL := 16
const BODY_FRAME := Vector2i(16, 24)
const FACTIONS := ["orthodox", "demonic", "unorthodox"]


## The proposed names, drawn from `plates` in the HUD's pixels.
class Plates extends "res://hud/panel.gd":
	## Each name to draw: `text`, `colour` and `at`, its top-left.
	var plates: Array[Dictionary] = []

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE

	func width_of(text: String) -> float:
		_font = get_theme_default_font() if _font == null else _font
		return _width(text)

	func _draw() -> void:
		for plate: Dictionary in plates:
			var baseline: Vector2 = plate.at + Vector2(1, 1 + _font.get_ascent(FONT_SIZE))
			draw_string_outline(_font, baseline, plate.text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, 2,
					Palette.INK_1)
			draw_string(_font, baseline, plate.text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, plate.colour)


var _dir := "."
var _frame := 0
var _step := 0
var _shots: Array[Dictionary] = []
var _shot_at := 0
var _play: Node2D
var _world: WorldAdapter
var _player := -1
var _plates: Plates


func _initialize() -> void:
	_dir = OS.get_cmdline_user_args()[0]
	for locale: String in LOCALES:
		for variant: String in VARIANTS:
			_shots.append({"locale": locale, "variant": variant})
	_world = WorldAdapter.create_region(SEED, 0, TICKS_PER_DAY)
	_player = _world.player_army()
	# Peace between every two factions hostile by stance first, so no one fights on the way; war once
	# every army stands at its post.
	for faction in _world.faction_count():
		for other in _world.faction_count():
			if _world.relation(faction, other) == WorldAdapter.RELATION_HOSTILE:
				_world.declare(_world.leader(faction), other, WorldAdapter.DECLARATION_PEACE)
	if not _gather():
		quit(1)
		return
	_declare(WorldAdapter.RELATION_ALLY, WorldAdapter.DECLARATION_WAR)
	_open()


# Has the player's faction's leader declare `what` toward each faction standing `relation` toward it.
func _declare(relation: int, what: int) -> void:
	var mine := _world.army_faction(_player)
	for faction in _world.faction_count():
		if _world.relation(mine, faction) == relation:
			_world.declare(_world.leader(mine), faction, what)
		print("nameplates: %s stands %d toward %s" % [_world.faction_names()[mine],
				_world.relation(mine, faction), _world.faction_names()[faction]])


# Walks ARMIES_PER_POST armies of each relation in POSTS, the nearest to the player, to their post,
# steered so the rules for people on their own leave them there, the faction to be at war still
# allied.
func _gather() -> bool:
	var mine := _world.army_faction(_player)
	var home := Vector2i(_cell(_world.person_position(_world.roster(_player)[0])))
	var walked := {}
	for relation: int in POSTS:
		var armies := []
		for army in _world.army_count():
			if army != _player and not _world.roster(army).is_empty() \
					and _world.relation(mine, _world.army_faction(army)) == relation:
				armies.append(army)
		armies.sort_custom(func(a: int, b: int) -> bool:
			return _cell(_world.person_position(_world.roster(a)[0])).distance_to(Vector2(home)) \
					< _cell(_world.person_position(_world.roster(b)[0])).distance_to(Vector2(home)))
		if armies.size() < 1:
			push_error("nameplates: no army stands %d toward the player" % relation)
			return false
		for army: int in armies.slice(0, ARMIES_PER_POST):
			_world.steer(army, true)
			walked[army] = _post(army, home + POSTS[relation])
	for i in GATHER_TICKS:
		if walked.keys().all(func(army: int) -> bool: return _world.roster(army).is_empty() \
				or _cell(_world.person_position(_world.roster(army)[0])).distance_to(Vector2(walked[army])) <= 1.0):
			break
		if i % 20 == 0:
			_world.order(_player, home, _skills(_player))
			for army: int in walked:
				if not _world.roster(army).is_empty():
					_world.order(army, walked[army], _skills(army))
		_world.tick()
	for army: int in walked:
		print("nameplates: army %d of %s, %d people, lead %s from its post" % [army,
				_world.faction_names()[_world.army_faction(army)], _world.roster(army).size(),
				_cell(_world.person_position(_world.roster(army)[0])) - Vector2(walked[army])
				if not _world.roster(army).is_empty() else "dead"])
	return true


# The cell nearest `cell` that `army` may be ordered to, which takes it there.
func _post(army: int, cell: Vector2i) -> Vector2i:
	for reach in 6:
		for y in range(cell.y - reach, cell.y + reach + 1):
			for x in range(cell.x - reach, cell.x + reach + 1):
				if maxi(absi(x - cell.x), absi(y - cell.y)) == reach and _world.order(army, Vector2i(x, y), _skills(army)):
					return Vector2i(x, y)
	return cell


func _skills(army: int) -> PackedInt64Array:
	return PackedInt64Array(range(_world.skill_count(army)))


func _cell(at: Vector2) -> Vector2:
	return (at / WorldAdapter.CELL_SIZE).floor()


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
	var shot := _shots[_shot_at]
	var hud: CanvasLayer = _play.get_node("Hud")
	match _step:
		0:
			hud.settings.path = KEPT
			hud.settings.choose("language", LOCALES.find(shot.locale))
			hud.settings.choose("touch", hud.settings.TOUCH_MODES.find("off"))
			DisplayServer.window_set_size(WINDOW)
		1:
			_play._play(_world)
			_play.set_process(false)
			# Two reads of the same positions: no body moved since the last, so each stands.
			_play._art.note_tick()
			_play._art.note_tick()
			_play._process(0.0)
			hud.labels.notice.visible = false
			_plates = Plates.new()
			_plates.use_font(hud.arts._font)
			hud.add_child(_plates)
			hud.move_child(_plates, 0)
		2:
			_compose(hud, shot.variant)
		3:
			var path := "%s/%s_%s.png" % [_dir, shot.variant, shot.locale]
			root.get_viewport().get_texture().get_image().save_png(path)
			print("nameplates: wrote ", path)
			_shot_at += 1
			if _shot_at >= _shots.size():
				print("nameplates: done")
				quit(0)
				return true
			_open()
			return false
	_step += 1
	return false


# Names each living person within REACH, nearest first, leaving out a name over a nearer one's or a
# panel, and outlines each hostile body as `variant` draws it.
func _compose(hud: CanvasLayer, variant: String) -> void:
	var art: Node2D = _play._art
	var mine := _world.army_faction(_player)
	var lead := _world.person_position(_world.roster(_player)[0])
	var near: Array[Dictionary] = []
	for army in _world.army_count():
		if army == _player:
			continue
		var relation := _world.relation(mine, _world.army_faction(army))
		var roster := _world.roster(army)
		for body in roster.size():
			var at := _world.person_position(roster[body])
			var cells := at.distance_to(lead) / WorldAdapter.CELL_SIZE
			# The talk prompt names its own person.
			if cells <= REACH and roster[body] != _play.talk_to:
				near.append({"person": roster[body], "army": army, "body": body, "relation": relation,
					"cells": cells})
	near.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a.cells < b.cells)
	var taken: Array[Rect2] = []
	if hud.talk_prompt.visible:
		taken.append(hud.talk_prompt.slip_rect())
	for child: Node in hud.get_children():
		if child is Panel and (child as Control).visible:
			taken.append((child as Control).get_rect())
	var plates: Array[Dictionary] = []
	var counts := {}
	var hostile_bodies := []
	for person: Dictionary in near:
		var hostile: bool = person.relation in [WorldAdapter.RELATION_HOSTILE, WorldAdapter.RELATION_AT_WAR]
		if hostile:
			hostile_bodies.append(person)
		var text: String = _plates.PersonName.of(_world, person.person)
		var width := _plates.width_of(text)
		var head: Vector2 = art.body_at(person.army, person.body, 1.0) - Vector2(0, 17)
		var top: Vector2 = hud.from_window(_play.get_viewport().get_canvas_transform() * head).floor()
		var rect := Rect2(top - Vector2(floorf(width / 2.0) + 1, 13), Vector2(width + 2, 13))
		if taken.any(func(other: Rect2) -> bool: return other.intersects(rect)):
			counts["left out"] = counts.get("left out", 0) + 1
			continue
		taken.append(rect)
		var colour: Color = _plates.Palette.PAPER_6
		if hostile:
			colour = _plates.Palette.CRIMSON_5
		elif person.relation in [WorldAdapter.RELATION_OWN, WorldAdapter.RELATION_ALLY]:
			colour = _plates.Palette.JADE_4
		plates.append({"text": text, "colour": colour, "at": rect.position})
		counts[person.relation] = counts.get(person.relation, 0) + 1
	_plates.plates = plates
	_plates.queue_redraw()
	_outline(art, hostile_bodies, variant)
	print("nameplates: %d within %d cells; named by relation %s" % [near.size(), REACH, counts])


# Lays a crimson outline over each of `bodies`, in their standing frame: on the silhouette's own
# edge for "outline", outside it for "ring".
func _outline(art: Node2D, bodies: Array, variant: String) -> void:
	var textures := {}
	for person: Dictionary in bodies:
		var sprite_set: int = art._army_sprites[person.army]
		if not textures.has(sprite_set):
			textures[sprite_set] = _outline_texture(sprite_set, variant)
		var sprite := Sprite2D.new()
		sprite.texture = textures[sprite_set]
		var at: Vector2 = art.body_at(person.army, person.body, 1.0)
		# Sorted with the bodies' band of its row, over it.
		sprite.position = Vector2(at.x, (int(at.y / CELL) + 1) * CELL).round()
		sprite.offset = Vector2(at.x, at.y + CELL / 2.0 - BODY_FRAME.y / 2.0).round() - sprite.position
		art.get_node("Standing").add_child(sprite)


func _outline_texture(sprite_set: int, variant: String) -> ImageTexture:
	var sheet: Texture2D = load("res://art/bodies/%s.png" % FACTIONS[sprite_set])
	var frame := sheet.get_image().get_region(Rect2i(Vector2i.ZERO, BODY_FRAME))
	frame.convert(Image.FORMAT_RGBA8)
	var out := Image.create_empty(BODY_FRAME.x, BODY_FRAME.y, false, Image.FORMAT_RGBA8)
	var crimson := Color("d63a2f")
	for y in BODY_FRAME.y:
		for x in BODY_FRAME.x:
			var solid := frame.get_pixel(x, y).a > 0.0
			var edge := false
			for step: Vector2i in [Vector2i.LEFT, Vector2i.RIGHT, Vector2i.UP, Vector2i.DOWN]:
				var q := Vector2i(x, y) + step
				var beside := q.x >= 0 and q.y >= 0 and q.x < BODY_FRAME.x and q.y < BODY_FRAME.y \
						and frame.get_pixelv(q).a > 0.0
				edge = edge or (beside != solid)
			if edge and solid == (variant == "outline"):
				out.set_pixel(x, y, crimson)
	return ImageTexture.create_from_image(out)
