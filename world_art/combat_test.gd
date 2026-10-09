# Checks that world_art.tscn shows what the fighting did, on the tick the core records it: a
# steered person's sweep at a foe beside it flashes its frontal arc, the foe is drawn white and
# knocked back away from it, its loss floats up as a number, and it is marked as the target; once a
# second sweep kills it, it flashes, its last loss floats up, and it falls and fades where it stood
# and is gone after. Control: the same foe standing out of reach is never swept, flashed, numbered
# or marked, and nothing dies. In a region living its days, those a use struck as they died fall,
# and those who died of hunger or age do not.
extends RefCounted

const SCENE := "res://world_art/world_art.tscn"
const Combat := preload("res://world_art/combat.gd")
const WorldArt := preload("res://world_art/world_art.gd")
const MAP_ROWS: Array[String] = [
	"................",
	"................",
	"................",
	"................",
	"................",
]
# A sweep at arm's length, a cell and a half, of 60 a second apart: two kill a foe of 100.
const SWEEP := {"range": 384, "radius": 0, "body_damage": 60, "structure_damage": 0, "cooldown": 30,
	"shape": WorldAdapter.SHAPE_ARC}
# The foe's one skill reaches no one, so only the steered person ever strikes.
const NO_REACH := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}
const HEALTH := 100
const STRIKER_CELL := Vector2i(2, 2)
const BESIDE := Vector2i(3, 2)
const OUT_OF_REACH := Vector2i(12, 2)
const RULES := {"separation": 256, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}
const TICKS := 90
const REGION_SEED := 2026
const REGION_TICKS := 8000


func run() -> bool:
	return _hit_pass() and _death_pass() and _cause_pass()


# The tick the steered person first sweeps a foe beside it shows the arc, the foe's flash, its
# knock away from the striker, its loss as a number and the mark on it; the striker itself is not
# flashed. Control: the foe out of reach, no use, flash, number or mark on any tick.
func _hit_pass() -> bool:
	var near := _fight(BESIDE)
	var art: Node2D = near.art
	var combat: Combat = art.combat
	var victim: int = near.victim
	var striker: int = near.striker
	var now := Time.get_ticks_msec()
	var arcs := combat.shown_strikes(now).filter(func(strike: Dictionary) -> bool:
		return strike.shape == WorldAdapter.SHAPE_ARC and strike.person == striker and strike.steered)
	var lost: int = HEALTH - near.world.person_health(victim)
	var numbers := combat.shown_numbers(now)
	var whites := _white_bodies(art)
	var struck: bool = near.tick >= 0 and lost == SWEEP.body_damage and arcs.size() == 1 \
			and combat.flash(victim, now) > 0.5 and combat.flash(striker, now) == 0.0 \
			and combat.knock(victim, now).x > 0.0 and numbers.size() == 1 \
			and numbers[0].text == str(SWEEP.body_damage) and combat.target == victim and whites == 1
	# Control: the same world with the foe out of reach.
	var far := _fight(OUT_OF_REACH)
	var far_combat: Combat = far.art.combat
	var later := Time.get_ticks_msec()
	var untouched: bool = far.tick < 0 and far.world.person_health(far.victim) == HEALTH \
			and far_combat.shown_strikes(later).is_empty() and far_combat.flash(far.victim, later) == 0.0 \
			and far_combat.shown_numbers(later).is_empty() and far_combat.target == -1 \
			and _white_bodies(far.art) == 0
	var passed := struck and untouched
	print("combat_test: beside, the sweep on tick %d lost %d of %d, shown as %d arc(s) of the steered, the foe at flash %.2f knocked %s, the striker at %.2f, numbers %s, target %d of %d, %d body drawn white; out of reach no use by tick %d, health %d, %d use(s) shown, flash %.2f, %d number(s), target %d, %d white: %s" % [
		near.tick, lost, HEALTH, arcs.size(), combat.flash(victim, now), combat.knock(victim, now),
		combat.flash(striker, now), numbers.map(func(number: Dictionary) -> String: return number.text),
		combat.target, victim, whites, TICKS, far.world.person_health(far.victim),
		far_combat.shown_strikes(later).size(), far_combat.flash(far.victim, later),
		far_combat.shown_numbers(later).size(), far_combat.target, _white_bodies(far.art),
		"pass" if passed else "FAIL"])
	near.art.free()
	far.art.free()
	return passed


# Ticked on after the first sweep, the second kills the foe: on that tick it lies among the dying,
# drawn in a band beside the living, upright, opaque and flashing, its last loss floating up, then
# halfway fallen over and faded at half its time, and gone at its end. Control: before that tick
# nothing was dying.
func _death_pass() -> bool:
	var fight := _fight(BESIDE)
	var world: WorldAdapter = fight.world
	var art: Node2D = fight.art
	var combat: Combat = art.combat
	var view := _view()
	var dying_before := -1
	for i in TICKS:
		dying_before = combat.dying(Time.get_ticks_msec()).size()
		world.tick()
		art.note_tick()
		art.refresh(view)
		if world.person_health(fight.victim) == 0:
			break
	var now := Time.get_ticks_msec()
	var at_death := combat.dying(now)
	var halfway := combat.dying(now + Combat.DEATH_MS / 2)
	var after := combat.dying(now + Combat.DEATH_MS)
	var instances := 0
	for band in _bands(art):
		instances += band.multimesh.visible_instance_count
	var last_loss := str(HEALTH - SWEEP.body_damage)
	var numbers := combat.shown_numbers(now).map(func(number: Dictionary) -> String: return number.text)
	var died: bool = world.person_health(fight.victim) == 0 and dying_before == 0 \
			and combat.deaths.size() == 1 and combat.deaths[0].person == fight.victim \
			and at_death.size() == 1 and at_death[0].alpha > 0.9 and at_death[0].fallen < 0.2 \
			and at_death[0].flash > 0.5 and numbers.has(last_loss) and _white_bodies(art) == 1 \
			and halfway.size() == 1 and halfway[0].fallen == 1.0 and absf(halfway[0].alpha - 0.5) < 0.05 \
			and after.is_empty() and art.drawn == 1 and instances == 2
	print("combat_test: the foe died %s with nothing dying before %s; at its death %s with numbers %s against its last loss %s and %d body drawn white, halfway %s, after %s; %d living drawn and %d bodies in the bands: %s" % [
		world.person_health(fight.victim) == 0, dying_before == 0, at_death, numbers, last_loss,
		_white_bodies(art), halfway, after, art.drawn, instances, "pass" if died else "FAIL"])
	art.free()
	return died


# A region whose days are a tick long lives until someone a use struck has died and someone no use
# struck has, of hunger or age, at most REGION_TICKS: on each tick, those the core records dying
# that a use struck fall, their killing blow numbered for everyone, and the others do not.
func _cause_pass() -> bool:
	var world := WorldAdapter.create_region(REGION_SEED, 0, 1)
	var combat := Combat.new()
	combat.damage_numbers = "everyone"
	combat.show_at(Rect2(-1e9, -1e9, 2e9, 2e9), 0, Vector2.ZERO)
	combat.begin(world)
	var kept := world.events(0)
	var next: int = 0 if kept.is_empty() else kept[-1].sequence + 1
	var struck_dead := 0
	var unstruck_dead := 0
	var right := true
	var ran := 0
	while (struck_dead == 0 or unstruck_dead == 0) and ran < REGION_TICKS:
		world.tick()
		ran += 1
		var now := ran * 1000
		var rosters: Array[PackedInt64Array] = []
		var positions: Array[PackedVector2Array] = []
		for army in world.army_count():
			rosters.append(world.roster(army))
			positions.append(world.body_positions(army))
		combat.note_tick(world, -1, now, rosters, positions)
		var struck := {}
		for strike: Dictionary in world.strikes():
			for person: int in strike.struck:
				struck[person] = true
		var expected := []
		for event: Dictionary in world.events(next):
			next = event.sequence + 1
			if event.kind == WorldAdapter.EVENT_DIED:
				if struck.has(event.person):
					struck_dead += 1
					expected.append(event.person)
				else:
					unstruck_dead += 1
		var fell := combat.deaths.filter(func(death: Dictionary) -> bool: return death.ms == now) \
				.map(func(death: Dictionary) -> int: return death.person)
		right = right and fell == expected and combat.dying(now).all(
				func(death: Dictionary) -> bool: return death.flash == 1.0) \
				and combat.shown_numbers(now).size() >= expected.size()
	combat.free()
	var passed := right and struck_dead > 0 and unstruck_dead > 0
	print("combat_test: a region with a tick's day ran %d ticks; %d died struck and %d of hunger or age, the struck alone falling, flashing and numbered %s: %s" % [
		ran, struck_dead, unstruck_dead, right, "pass" if passed else "FAIL"])
	return passed


# A world of a steered person with SWEEP and a foe of HEALTH at `foe_cell`, drawn by world_art and
# ticked until the first use the core records, at most TICKS: the world, the art, the striker and
# the victim, and the tick of the first use, -1 for none.
func _fight(foe_cell: Vector2i) -> Dictionary:
	var armies: Array[Dictionary] = [
		{"objective": STRIKER_CELL, "faction": 0, "skills": [SWEEP],
			"members": [{"cell": STRIKER_CELL, "health": HEALTH}]},
		{"objective": foe_cell, "faction": 1, "skills": [NO_REACH],
			"members": [{"cell": foe_cell, "health": HEALTH}]},
	]
	var world := WorldAdapter.create(7, PackedStringArray(MAP_ROWS), armies, [], RULES)
	var art: Node2D = load(SCENE).instantiate()
	art.show_world(world, 0)
	var striker := world.roster(0)[0]
	var victim := world.roster(1)[0]
	var view := _view()
	var first := -1
	for i in TICKS:
		world.tick()
		art.note_tick()
		art.refresh(view)
		if not world.strikes().is_empty():
			first = i
			break
	return {"world": world, "art": art, "striker": striker, "victim": victim, "tick": first}


func _view() -> Rect2:
	return Rect2(Vector2.ZERO, Vector2(MAP_ROWS[0].length(), MAP_ROWS.size()) * WorldArt.CELL)


# How many bodies the bands hold drawn any share of the way to white.
func _white_bodies(art: Node2D) -> int:
	var white := 0
	for band in _bands(art):
		var buffer := band.multimesh.buffer
		for i in band.multimesh.visible_instance_count:
			if buffer[i * WorldArt.BODY_FLOATS + 14] > 0.0:
				white += 1
	return white


func _bands(art: Node2D) -> Array[MultiMeshInstance2D]:
	var bands: Array[MultiMeshInstance2D] = []
	for child in art.get_node("Standing").get_children():
		if child is MultiMeshInstance2D:
			bands.append(child)
	return bands
