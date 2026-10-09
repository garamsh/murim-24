# Checks the names over the people near the player. Drawn by world_art.tscn and named by hud.tscn,
# each person within world_art.gd's NAME_REACH of the viewer is named in the colour of how its
# faction stands toward the viewer's, as WorldAdapter.relation reads it, and a hostile body is
# ringed in crimson; a person a cell past the reach is not named. Of more people than fit,
# nameplates.gd names every hostile one before any other, then the nearest, no more than NAME_CAP,
# none over another name, a place the HUD keeps clear or the room's edge. At each PC window in each
# locale, a crowd of the longest names across the room is named whole, inside the room, clear of
# every panel and of one another.
extends RefCounted

const ART_SCENE := "res://world_art/world_art.tscn"
const HUD_SCENE := "res://hud/hud.tscn"
const ViewScale := preload("res://view_scale.gd")
const PersonName := preload("res://hud/person_name.gd")
const Palette := preload("res://hud/palette.gd")
const WorldArt := preload("res://world_art/world_art.gd")
const Nameplates := preload("res://hud/nameplates.gd")
const SEED := 2026
const WINDOW := Vector2i(1920, 1080)
const PC_WINDOWS: Array[Vector2i] = [Vector2i(1280, 720), Vector2i(1920, 1080), Vector2i(2560, 1440)]
const IDLE := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1}
# The factions, by how each stands toward the first: its own, an ally, a neutral one once peace is
# made over a hostile stance, a hostile one, and one at war once war is declared on an ally.
const OWN := 0
const ALLY := 1
const NEUTRAL := 2
const HOSTILE := 3
const AT_WAR := 4
const RULES := {
	"separation": 72,
	"hostile": [
		[false, false, true, true, false],
		[false, false, false, false, false],
		[true, false, false, false, false],
		[true, false, false, false, false],
		[false, false, false, false, false],
	],
	"stage_floors": [75, 25, 1],
}
const LEADERSHIP: Array[Dictionary] = [{"leader": 0}, {}, {}, {}, {}]
# One person to an army, the viewer, person 0, first: each person's cell and faction, and the
# relation it is named by. Person 6 stands NAME_REACH cells from the viewer and person 7 a cell
# further, past the reach.
const VIEWER := Vector2i(10, 10)
const PEOPLE: Array[Dictionary] = [
	{"cell": Vector2i(10, 10), "faction": OWN},
	{"cell": Vector2i(13, 10), "faction": OWN},
	{"cell": Vector2i(10, 13), "faction": ALLY},
	{"cell": Vector2i(6, 10), "faction": NEUTRAL},
	{"cell": Vector2i(10, 6), "faction": HOSTILE},
	{"cell": Vector2i(14, 14), "faction": AT_WAR},
	{"cell": Vector2i(18, 10), "faction": HOSTILE},
	{"cell": Vector2i(19, 12), "faction": HOSTILE},
]
const PAST_REACH := 7


func run() -> bool:
	return _colours_pass() and _order_pass() and _clear_pass() and _fit_pass()


# Each person within the reach is named in its relation's colour and each hostile body ringed,
# read through world_art.gd's near list and the HUD as the play scene passes it on. Control: person
# 7, a cell past the reach and hostile, is ringed and not named. Setup: the relations stand as
# asked, every body is drawn, and person 7 is on screen.
func _colours_pass() -> bool:
	var world := _world()
	var expected := [-1, WorldAdapter.RELATION_OWN, WorldAdapter.RELATION_ALLY, WorldAdapter.RELATION_NEUTRAL,
		WorldAdapter.RELATION_HOSTILE, WorldAdapter.RELATION_AT_WAR, WorldAdapter.RELATION_HOSTILE,
		WorldAdapter.RELATION_HOSTILE]
	var setup := true
	for person in range(1, PEOPLE.size()):
		setup = setup and world.relation(OWN, PEOPLE[person].faction) == expected[person]
	var art: Node2D = load(ART_SCENE).instantiate()
	art.show_world(world, 0)
	var hud: CanvasLayer = load(HUD_SCENE).instantiate()
	hud.fit(WINDOW, false)
	var zoom := ViewScale.world_scale(WINDOW, false)
	var lead: Vector2 = art.body_at(0, 0, 1.0)
	var view := Rect2(lead - Vector2(WINDOW) / zoom / 2.0, Vector2(WINDOW) / zoom)
	art.refresh(view, 1.0)
	setup = setup and art.drawn == PEOPLE.size() and view.has_point(art.body_at(PAST_REACH, 0, 1.0))
	var people: Array[Dictionary] = []
	for person: Dictionary in art.near:
		people.append({"person": person.person, "relation": person.relation,
			"head": hud.from_window((person.head - view.position) * zoom).floor()})
	hud.show_names(world, people)
	var faults := []
	var near := {}
	for person: Dictionary in art.near:
		near[person.person] = person.relation
	var named := {}
	for plate: Dictionary in hud.nameplates.plates:
		named[plate.person] = plate.colour
	for person in range(1, PEOPLE.size()):
		var relation: int = expected[person]
		var ring := Color("d63a2f") if relation in WorldArt.HOSTILE_RELATIONS else Color.WHITE
		if _tint(art, art.body_at(person, 0, 1.0)) != ring:
			faults.append("person %d tinted %s" % [person, _tint(art, art.body_at(person, 0, 1.0))])
		if person == PAST_REACH:
			continue
		if near.get(person, -2) != relation or named.get(person) != Nameplates.COLOURS[relation]:
			faults.append("person %d near as %s, named in %s" % [person, near.get(person), named.get(person)])
	if _tint(art, lead) != Color.WHITE:
		faults.append("the viewer tinted")
	var control: bool = not near.has(PAST_REACH) and not named.has(PAST_REACH) and not near.has(0) \
			and near.size() == PEOPLE.size() - 2
	art.free()
	hud.free()
	var passed := setup and faults.is_empty() and control
	print("nameplates_test: %d of %d within reach named %s; faults %s; person %d past the reach ringed and not named, nor the viewer, %s (setup %s): %s" % [
		named.size(), near.size(), named.values(), faults, PAST_REACH,
		control, setup, "pass" if passed else "FAIL"])
	return passed


# Of six own people nearest and six hostile further, all apart, the six hostile are named first and
# then the two nearest own, NAME_CAP in all. Control: given the own people alone, the eight nearest
# are named, so the hostile ones above were named for their relation, not for room.
func _order_pass() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var plates := _plates()
	var people: Array[Dictionary] = []
	for n in 12:
		people.append({"person": n, "relation": WorldAdapter.RELATION_OWN if n < 6 else WorldAdapter.RELATION_HOSTILE,
			"head": Vector2(120 + (n % 4) * 180, 120 + (n / 4) * 80)})
	var none: Array[Rect2] = []
	plates.show_names(world, people, none)
	var order: Array = plates.plates.map(func(plate: Dictionary) -> int: return plate.person)
	var hostile_first: bool = order == [6, 7, 8, 9, 10, 11, 0, 1]
	var own: Array[Dictionary] = []
	for n in 10:
		own.append({"person": n, "relation": WorldAdapter.RELATION_OWN,
			"head": Vector2(120 + (n % 4) * 180, 120 + (n / 4) * 80)})
	plates.show_names(world, own, none)
	var own_order: Array = plates.plates.map(func(plate: Dictionary) -> int: return plate.person)
	var control: bool = own_order == [0, 1, 2, 3, 4, 5, 6, 7]
	plates.free()
	var passed := hostile_first and control
	print("nameplates_test: own nearest and hostile further named in the order %s, hostile first %s; own alone %s, the nearest %d %s: %s" % [
		order, hostile_first, own_order, Nameplates.NAME_CAP, control, "pass" if passed else "FAIL"])
	return passed


# A name that would cover a nearer one's, a place kept clear or the room's edge is left out.
# Controls: the same person named apart from the nearer one, with nothing kept clear, and away
# from the edge is named.
func _clear_pass() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var plates := _plates()
	var own := WorldAdapter.RELATION_OWN
	var covered := _named(plates, world, [{"person": 0, "relation": own, "head": Vector2(400, 200)},
			{"person": 1, "relation": own, "head": Vector2(404, 204)}], [])
	var apart := _named(plates, world, [{"person": 0, "relation": own, "head": Vector2(400, 200)},
			{"person": 1, "relation": own, "head": Vector2(400, 260)}], [])
	var kept := _named(plates, world, [{"person": 1, "relation": own, "head": Vector2(400, 260)}],
			[Rect2(380, 240, 40, 10)])
	var free := _named(plates, world, [{"person": 1, "relation": own, "head": Vector2(400, 260)}],
			[Rect2(380, 200, 40, 10)])
	var edge := _named(plates, world, [{"person": 1, "relation": own, "head": Vector2(2, 260)}], [])
	plates.free()
	var passed := covered == [0] and apart == [0, 1] and kept.is_empty() and free == [1] and edge.is_empty()
	print("nameplates_test: over a nearer name %s, apart %s; over a clear place %s, beside it %s; at the edge %s: %s" % [
		covered, apart, kept, free, edge, "pass" if passed else "FAIL"])
	return passed


# At each PC window in each locale, people bearing the region's widest names, hostile and own, with
# heads on a grid across the whole room, are named whole, NAME_CAP of them, each inside the room
# and clear of every other name, every panel the HUD shows, the purse and the talk prompt, which
# points at the room's middle. Setup: the names are the widest the region's people bear.
func _fit_pass() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var hud: CanvasLayer = load(HUD_SCENE).instantiate()
	hud.show_player(world, world.player_army())
	hud.talk_prompt.key = "Space"
	var faults := []
	var checks := 0
	var widest := {}
	for locale: String in hud.LOCALES:
		TranslationServer.set_locale(locale)
		var people := _widest(world, hud.nameplates)
		widest[locale] = PersonName.of(world, people[0])
		for window: Vector2i in PC_WINDOWS:
			hud.fit(window, false)
			var room := Rect2(Vector2.ZERO, hud.room)
			hud.talk_prompt.point_at(world, people[0], (room.size / 2.0).floor(), false)
			var crowd: Array[Dictionary] = []
			var n := 0
			for y in range(0, int(room.size.y), 24):
				for x in range(0, int(room.size.x), 40):
					crowd.append({"person": people[n % people.size()], "head": Vector2(x, y),
						"relation": WorldAdapter.RELATION_HOSTILE if n % 2 == 0 else WorldAdapter.RELATION_OWN})
					n += 1
			hud.show_names(world, crowd)
			var plates: Array[Dictionary] = hud.nameplates.plates
			var kept := [hud.purse.strip_rect(), hud.purse.belt_rect(), hud.talk_prompt.slip_rect()]
			for child: Node in hud.get_children():
				if child is Panel and (child as Control).visible:
					kept.append((child as Control).get_rect())
			checks += 1
			if plates.size() != Nameplates.NAME_CAP:
				faults.append("%s %s: %d named" % [window, locale, plates.size()])
			for p in plates.size():
				var plate: Dictionary = plates[p]
				var rect: Rect2 = plate.rect
				checks += 1
				var whole: bool = plate.text == PersonName.of(world, plate.person) \
						and rect.size.x >= hud.nameplates._width(plate.text)
				if not whole or not room.encloses(rect):
					faults.append("%s %s: %s at %s" % [window, locale, plate.text, rect])
				for other: Dictionary in plates.slice(p + 1):
					if rect.intersects(other.rect):
						faults.append("%s %s: %s over %s" % [window, locale, plate.text, other.text])
				for clear: Rect2 in kept:
					if rect.intersects(clear):
						faults.append("%s %s: %s over %s" % [window, locale, plate.text, clear])
	TranslationServer.set_locale(hud.LOCALES[0])
	hud.free()
	var passed := faults.is_empty() and checks > 0
	print("nameplates_test: the widest names %s named over a crowd at %s in both locales, %d checks; faults %s: %s" % [
		widest, PC_WINDOWS, checks, faults.slice(0, 3), "pass" if passed else "FAIL"])
	return passed


# The world PEOPLE stand in, one army each, the own faction at peace with NEUTRAL and at war with
# AT_WAR by its leader's declarations.
func _world() -> WorldAdapter:
	var rows := PackedStringArray()
	for y in 24:
		rows.append(".".repeat(40))
	var armies: Array[Dictionary] = []
	for person: Dictionary in PEOPLE:
		armies.append({"objective": person.cell, "members": [{"cell": person.cell, "health": 1}],
			"faction": person.faction, "skills": [IDLE]})
	var world := WorldAdapter.create(1, rows, armies, [], RULES, LEADERSHIP)
	world.declare(0, NEUTRAL, WorldAdapter.DECLARATION_PEACE)
	world.declare(0, AT_WAR, WorldAdapter.DECLARATION_WAR)
	return world


func _plates() -> Control:
	var plates: Control = Nameplates.new()
	plates.use_font(load("res://hud/fonts/Galmuri9.ttf"))
	plates.size = Vector2(960, 540)
	return plates


func _named(plates: Control, world: WorldAdapter, people: Array[Dictionary], clear: Array[Rect2]) -> Array:
	plates.show_names(world, people, clear)
	return plates.plates.map(func(plate: Dictionary) -> int: return plate.person)


# The region's people, widest name first, in the current locale, as `plates` draws them.
func _widest(world: WorldAdapter, plates: Control) -> Array[int]:
	var people: Array[int] = []
	for army in world.army_count():
		people.append_array(world.roster(army))
	people.sort_custom(func(a: int, b: int) -> bool:
		return plates._width(PersonName.of(world, a)) > plates._width(PersonName.of(world, b)))
	return people.slice(0, 8)


# The colour body `at`, in world pixels, is drawn with in `art`'s bands, or black where none is.
func _tint(art: Node2D, at: Vector2) -> Color:
	var centre := Vector2(at.x, at.y + art.CELL / 2.0 - art.BODY_FRAME.y / 2.0).round()
	for child in art.get_node("Standing").get_children():
		var band := child as MultiMeshInstance2D
		if band == null:
			continue
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			var base := i * WorldArt.BODY_FLOATS
			if Vector2(buffer[base + 3], buffer[base + 7]) + band.position == centre:
				return Color(buffer[base + 8], buffer[base + 9], buffer[base + 10], buffer[base + 11])
	return Color.BLACK
