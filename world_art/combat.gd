## Shows what the fighting of the last ticks did, in palette colours thinned by combat.gdshader's
## dither as they fade, blending nothing: each use of a skill that strikes people the core recorded
## on a tick (WorldAdapter.strikes) flashes its shape, a frontal arc, a line with its projectile, or
## a disc, in its user's faction's accent, the steered person's in jade and wherever they are; a
## person whose health fell since the tick before flashes in the impact core's colour and is knocked
## back from the nearest use, its loss floating up while damage numbers are on, in crimson for the
## steered person's own; a person who died falls and fades where it last stood; and the person the
## steered person's last use went toward is marked while it lives. world_art.gd draws the bodies
## with what it gives. It reads the world and holds no game rule: who struck, what, where and whom
## is the core's.
extends Node2D

const CELL := 16
## How long each thing shows, in milliseconds.
const STRIKE_MS := 160
const FLASH_MS := 120
const DEATH_MS := 700
const NUMBER_MS := 800
## How long the steered person's target stays marked after its last use toward it.
const TARGET_MS := 2000
## How far the struck are knocked back, and a number rises, in pixels.
const KNOCK_PIXELS := 2.0
const NUMBER_RISE := 12.0
## The least radius a disc is drawn at, so a use of radius 0 still shows.
const LEAST_RADIUS := 4.0
## How near a use's point a person stands to be the one it went toward, in sub-cell units.
const TOWARD_REACH := 128.0
const PAPER := Color("efe3c2")
const INK := Color("14101a")
const JADE := Color("4fc49a")
const CRIMSON := Color("d63a2f")
const FONT := preload("res://hud/fonts/Galmuri9.ttf")
const FONT_SIZE := 10

## Whether each loss of health floats up as a number; the settings' damage numbers row sets it.
var damage_numbers := true
## Each army's faction accent, by army index; world_art.gd keeps it as armies form.
var accents: Array[Color] = []
## Each use shown, as WorldAdapter.strikes() gives it, with `ms`, when it was recorded, and
## `steered`, whether the steered person made it.
var strikes: Array[Dictionary] = []
## Each person struck, by person: `ms`, when, `text`, the health it lost as words, and their
## `width`, `at`, where it stood, and `away`, the way it is knocked, all in pixels.
var hits := {}
## Each person who died, in order: `person`, its `army`, `at`, where it last stood in pixels, and
## `ms`, when.
var deaths: Array[Dictionary] = []
## The person the steered person's last use went toward, or -1.
var target := -1
## The view the last refresh showed, in pixels.
var view := Rect2()

var _now := 0
var _target_ms := 0
# The steered person, whose own losses float up in crimson, or -1.
var _lead := -1
# Each living person's health on the last tick, and its army and body index, by person.
var _health := {}
var _place := {}
var _positions: Array[PackedVector2Array] = []
var _target_at := Vector2.ZERO


## Starts on `world` as it stands, showing nothing: what each person holds is what the first tick
## is compared with.
func begin(world: WorldAdapter) -> void:
	accents.clear()
	strikes.clear()
	hits.clear()
	deaths.clear()
	target = -1
	_health.clear()
	_place.clear()
	_positions.clear()
	for army in world.army_count():
		var roster := world.roster(army)
		var strengths := world.body_strengths(army)
		_positions.append(world.body_positions(army))
		for body in roster.size():
			_place[roster[body]] = Vector2i(army, body)
			_health[roster[body]] = strengths[body]


## Reads what the tick just run did, at `now` in milliseconds: the uses the core recorded, who
## lost health and who died since the tick before, and, for the lead of `steered_army` (-1 for
## none), the person its last use went toward. `rosters` and `positions` are each army's roster and
## its bodies' positions as the tick left them, which world_art.gd has read already.
func note_tick(world: WorldAdapter, steered_army: int, now: int, rosters: Array[PackedInt64Array],
		positions: Array[PackedVector2Array]) -> void:
	var place := {}
	var health := {}
	for army in rosters.size():
		var roster := rosters[army]
		var strengths := world.body_strengths(army)
		for body in roster.size():
			place[roster[body]] = Vector2i(army, body)
			health[roster[body]] = strengths[body]
	var lead := -1
	if steered_army >= 0 and steered_army < rosters.size() and not rosters[steered_army].is_empty():
		lead = rosters[steered_army][0]
	_lead = lead
	# A use that strikes only structures, a mend or a torch, shows in the structures' stages alone.
	var used := world.strikes().filter(func(strike: Dictionary) -> bool: return strike.body_damage > 0)
	for strike: Dictionary in used:
		strike.ms = now
		strike.steered = strike.person == lead
		strikes.append(strike)
		if strike.steered:
			var aimed := _person_near(strike.toward, place, positions, lead)
			if aimed >= 0:
				target = aimed
				_target_ms = now
	for person: int in health:
		if _health.has(person) and health[person] < _health[person]:
			var at := _pixels(positions[place[person].x][place[person].y])
			var text := str(_health[person] - health[person])
			hits[person] = {"ms": now, "text": text, "at": at, "away": _away(at, used),
				"width": FONT.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x}
	for person: int in _place:
		if not place.has(person) and world.person_health(person) == 0:
			var was: Vector2i = _place[person]
			deaths.append({"person": person, "army": was.x, "at": _pixels(_positions[was.x][was.y]),
				"ms": now})
	_health = health
	_place = place
	_positions = positions.duplicate()
	if not place.has(target) or now - _target_ms > TARGET_MS:
		target = -1
	_forget(now)


## Returns the share of person `person`'s pixels drawn in the impact core's colour at `now`, from 1
## as it is struck to 0.
func flash(person: int, now: int) -> float:
	var hit: Dictionary = hits.get(person, {})
	if hit.is_empty():
		return 0.0
	return clampf(1.0 - float(now - hit.ms) / FLASH_MS, 0.0, 1.0)


## Returns how far, in pixels, person `person` is drawn from where it stands at `now`.
func knock(person: int, now: int) -> Vector2:
	var hit: Dictionary = hits.get(person, {})
	if hit.is_empty():
		return Vector2.ZERO
	return hit.away * KNOCK_PIXELS * clampf(1.0 - float(now - hit.ms) / FLASH_MS, 0.0, 1.0)


## Returns each death still shown at `now`: its `army`, `at`, where it lies in pixels, `fallen`,
## the share of a quarter turn it has fallen, and `alpha`, how opaque it is.
func dying(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	for death: Dictionary in deaths:
		var age := float(now - death.ms) / DEATH_MS
		if age >= 0.0 and age < 1.0:
			shown.append({"army": death.army, "at": death.at, "fallen": minf(age * 3.0, 1.0),
				"alpha": 1.0 - age})
	return shown


## Returns the army and body index the person stood at on the last tick, or (-1, -1) once it is
## not among the living.
func place_of(person: int) -> Vector2i:
	return _place.get(person, Vector2i(-1, -1))


## Draws what shows at `now` within `shown`, the part of the world on screen in pixels, with the
## steered person's target at `target_at`.
func show_at(shown: Rect2, now: int, target_at: Vector2) -> void:
	view = shown
	_now = now
	_target_at = target_at
	queue_redraw()


## Returns the uses shown at `now`: those within the view, and the steered person's wherever they
## are.
func shown_strikes(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	for strike: Dictionary in strikes:
		if now - strike.ms < STRIKE_MS and (strike.steered or view.has_point(_pixels(strike.origin))):
			shown.append(strike)
	return shown


func _draw() -> void:
	for strike: Dictionary in shown_strikes(_now):
		_draw_strike(strike, 1.0 - float(_now - strike.ms) / STRIKE_MS)
	if target >= 0:
		_draw_target(_target_at.round())
	for number: Dictionary in shown_numbers(_now):
		draw_string(FONT, number.at + Vector2.ONE, number.text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE,
				Color(INK, number.colour.a))
		draw_string(FONT, number.at, number.text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE, number.colour)


## Returns the damage numbers shown at `now` within the view, none while damage numbers are off:
## each its `text`, where its baseline starts, `at`, in pixels, and its `colour`.
func shown_numbers(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	if not damage_numbers:
		return shown
	for person: int in hits:
		var hit: Dictionary = hits[person]
		var age := float(now - hit.ms) / NUMBER_MS
		if age < 0.0 or age >= 1.0 or not view.has_point(hit.at):
			continue
		var colour := CRIMSON if person == _lead else PAPER
		colour.a = 1.0 - age * age
		shown.append({"text": hit.text, "colour": colour,
			"at": (hit.at + Vector2(-hit.width / 2.0, -CELL - NUMBER_RISE * age)).round()})
	return shown


func _draw_strike(strike: Dictionary, left: float) -> void:
	var colour := JADE if strike.steered else _accent(strike.person)
	var fill := Color(colour, 0.35 * left)
	var edge := Color(colour, 0.9 * left)
	var origin := _pixels(strike.origin)
	var toward := _pixels(strike.toward)
	var reach: float = strike.range * CELL / float(WorldAdapter.CELL_SIZE)
	var radius := maxf(strike.radius * CELL / float(WorldAdapter.CELL_SIZE), LEAST_RADIUS)
	var way := toward - origin
	match strike.shape:
		WorldAdapter.SHAPE_ARC:
			var facing := way.angle() if way != Vector2.ZERO else 0.0
			var sweep := PI / 2.0 if way != Vector2.ZERO else PI
			var points := PackedVector2Array([origin])
			for n in 13:
				points.append(origin + Vector2.from_angle(facing - sweep + 2.0 * sweep * n / 12.0) * reach)
			draw_colored_polygon(points, fill)
			draw_arc(origin, reach, facing - sweep, facing + sweep, 13, edge, 1.0)
		WorldAdapter.SHAPE_LINE, WorldAdapter.SHAPE_TARGETED:
			var end := toward
			if strike.shape == WorldAdapter.SHAPE_LINE and way != Vector2.ZERO:
				end = origin + way.normalized() * reach
			draw_line(origin, end, fill, maxf(radius * 2.0, 1.0))
			draw_circle(origin.lerp(end, 1.0 - left), 1.5, edge)
		_:
			draw_circle(toward, radius, fill)
			draw_arc(toward, radius, 0.0, TAU, 16, edge, 1.0)


# Corner brackets round the target's body, which stands 24 pixels tall on its cell's middle.
func _draw_target(at: Vector2) -> void:
	var box := Rect2(at + Vector2(-9, -13), Vector2(18, 21))
	var arm := 4.0
	for corner: Vector2 in [box.position, Vector2(box.end.x, box.position.y),
			Vector2(box.position.x, box.end.y), box.end]:
		var sx := 1.0 if corner.x == box.position.x else -1.0
		var sy := 1.0 if corner.y == box.position.y else -1.0
		draw_line(corner, corner + Vector2(arm * sx, 0), CRIMSON, 1.0)
		draw_line(corner, corner + Vector2(0, arm * sy), CRIMSON, 1.0)


func _accent(person: int) -> Color:
	var army: int = place_of(person).x
	return accents[army] if army >= 0 and army < accents.size() else PAPER


# The living person, other than `except`, standing nearest `point` within TOWARD_REACH, or -1.
func _person_near(point: Vector2, place: Dictionary, positions: Array[PackedVector2Array],
		except: int) -> int:
	var nearest := -1
	var best := TOWARD_REACH * TOWARD_REACH
	for person: int in place:
		if person == except:
			continue
		var at: Vector2i = place[person]
		var distance := positions[at.x][at.y].distance_squared_to(point)
		if distance <= best:
			best = distance
			nearest = person
	return nearest


# The way from the nearest of `used`'s origins to `at`, in pixels, as a unit; none without a use.
func _away(at: Vector2, used: Array) -> Vector2:
	var nearest := Vector2.INF
	for strike: Dictionary in used:
		var origin := _pixels(strike.origin)
		if origin != at and at.distance_squared_to(origin) < at.distance_squared_to(nearest):
			nearest = origin
	return Vector2.ZERO if nearest == Vector2.INF else (at - nearest).normalized()


# Drops what no longer shows at `now`.
func _forget(now: int) -> void:
	strikes = strikes.filter(func(strike: Dictionary) -> bool: return now - strike.ms < STRIKE_MS)
	deaths = deaths.filter(func(death: Dictionary) -> bool: return now - death.ms < DEATH_MS)
	for person: int in hits.keys():
		if now - hits[person].ms >= NUMBER_MS:
			hits.erase(person)


func _pixels(sub_cells: Vector2) -> Vector2:
	return sub_cells * CELL / WorldAdapter.CELL_SIZE
