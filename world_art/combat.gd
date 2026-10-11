## Shows what the fighting of the last ticks did, in palette colours thinned by combat.gdshader's
## dither as they fade, blending nothing: each use of a skill that strikes people the core recorded
## on a tick (WorldAdapter.strikes) flashes its shape, a frontal arc, a line with its projectile, or
## a disc, in its user's faction's accent, the steered person's in jade and wherever they are; a
## person whose health fell since the tick before flashes in the impact core's colour and is knocked
## back from the nearest use that struck it, its loss floating up while the damage numbers row
## names it, in crimson for the steered person's own; a person a use struck who died, by the core's
## events, falls and fades where it last stood, flashing with its last loss, while one who died
## struck by nothing, of hunger or age, is not drawn falling; a hit on or from the steered person
## holds the struck and the striker still a few view frames while the ticks go on (hit_stops) and
## shakes the screen by whole pixels (shake); a
## person a use struck bleeds, a spray
## from its middle in the blow's direction and drops on the ground that fade, while blood is on; a
## person below WOUNDED or MAIMED of the most health it has had is wounded, and a maimed one on
## screen drips now and then; and the person the player selected is marked while it lives, or with
## none selected, the person the steered person's last use struck nearest where it went. Like
## sound.gd, it keeps what it
## compares by person in packed arrays. world_art.gd draws the bodies with what it gives. It reads
## the world and holds no game rule: who struck, what, where and whom is the core's.
extends Node2D

## A cell's width and depth on the ground, in pixels, as world_art.gd draws it.
const CELL := Vector2(16, 12)
## How far above where a person stands the top of its body is drawn, in pixels: its feet are on
## the 22nd of its frame's 24 rows.
const BODY_HEIGHT := 22.0
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
## The shares, per mille of the most health a person has had, below which its body is drawn
## wounded, then maimed: view data read from the core's health.
const WOUNDED := 500
const MAIMED := 250
## How long a spray of blood flies, and a drop lies on the ground, in milliseconds; how many ticks
## apart a maimed person drips; and the most drops kept, the oldest going first.
const SPRAY_MS := 300
## The pixels a spray throws.
const SPRAY_PIXELS := 6
const DROP_MS := 3000
const DRIP_TICKS := 45
const MOST_DROPS := 600
## How many view frames a hit stop holds its bodies, by the health the hit took: under the first of
## each pair, the second; any more, HIT_STOP_MOST.
const HIT_STOP := [[20, 2], [60, 3]]
const HIT_STOP_MOST := 4
## How far the screen shakes on such a hit, in whole screen pixels, by the health the hit took, as
## HIT_STOP steps it; for how long, in milliseconds; and the offsets it steps through, one each
## SHAKE_STEP_MS.
const SHAKE := [[20, 1], [60, 2]]
const SHAKE_MOST := 3
const SHAKE_MS := 150
const SHAKE_STEP_MS := 30
const SHAKE_PATTERN: Array[Vector2i] = [Vector2i(1, 0), Vector2i(-1, 1), Vector2i(0, -1), Vector2i(1, 1), Vector2i(-1, 0)]
## Blood, the crimson ramp's second and third steps: its first is too near black to read on the
## ground, and its fourth stays for hostility.
const BLOOD := [Color("9c1f24"), Color("5c1418")]
## The least radius a disc is drawn at, so a use of radius 0 still shows.
const LEAST_RADIUS := 4.0
const PAPER := Color("efe3c2")
const INK := Color("14101a")
const JADE := Color("4fc49a")
const CRIMSON := Color("d63a2f")
const FONT := preload("res://hud/fonts/Galmuri9.ttf")
const FONT_SIZE := 10
# When a person not yet struck was last struck, in milliseconds: long enough ago to show nothing.
const NEVER := -(1 << 40)

## Whose losses float up as numbers: "off", none; "mine", the steered person's own and those its
## uses struck; "everyone", every loss. The settings' damage numbers row sets it.
var damage_numbers := "mine"
## Whether a hit draws blood; the settings' blood row sets it. Off, a hit shows only its flash.
var blood := true
## Each spray of blood flying, in order: `ms`, when, `at`, the struck body's middle in pixels, `dir`,
## the blow's direction, and `seed`.
var sprays: Array[Dictionary] = []
## The drops of blood on the ground, a ring of at most MOST_DROPS, a new drop taking the oldest's
## place: where each fell, in pixels, when, and its shade of BLOOD. Read by blood_pixels.gd as they are,
## so a frame builds nothing; a drop older than DROP_MS shows no more.
var drop_at := PackedVector2Array()
var drop_ms := PackedInt64Array()
var drop_shade := PackedByteArray()
## How many drops have fallen, the ring's next place being this modulo MOST_DROPS.
var drops_fallen := 0
## Whether a hit on or from the steered person holds the struck and the striker still a few frames
## and shakes the screen; the settings' hit stop row sets it.
var hit_stop := true
## The hit stops of the last tick: each `person` held and for how many view `frames`.
var hit_stops: Array[Dictionary] = []
## Each army's faction accent, by army index; world_art.gd keeps it as armies form.
var accents: Array[Color] = []
## Each use shown, as WorldAdapter.strikes() gives it, with `ms`, when it was recorded, and
## `steered`, whether the steered person made it.
var strikes: Array[Dictionary] = []
## Each loss floating up, in order: `ms`, when, `text`, the health lost as words, and their
## `width`, `at`, where the person stood, in pixels, `own`, whether it was the steered person's
## loss, and `mine`, whether it was that or a loss to the steered person's use.
var numbers: Array[Dictionary] = []
## Each person who died struck, in order: `person`, its `army`, `at`, where it last stood in
## pixels, `ground`, that place on the unraised ground, and `ms`, when.
var deaths: Array[Dictionary] = []
## The person marked: the one selected while it lives, else the one the steered person's last use
## struck nearest where it went, or -1.
var target := -1
## The person the player selected, which play.gd sets, or -1 for none.
var selected := -1
## The view the last refresh showed, in pixels.
var view := Rect2()
## Where a point in sub-cell units is drawn, in pixels: world_art.gd's projection, raised with the
## ground; without one, on the unraised ground.
var to_screen := Callable()

var _now := 0
var _target_ms := 0
var _target_at := Vector2.ZERO
# The ticks read since begin(), the sequence of the next event to read, and the tick being read's
# time in milliseconds and steered person, or -1.
var _tick := 0
var _next_event := 0
var _ms := 0
var _lead := -1
# Each army's bodies' positions on the tick last read.
var _positions: Array[PackedVector2Array] = []
# Each army's roster as the last tick read it, so a body's person is found again.
var _rosters_read: Array[PackedInt64Array] = []
# By person: the tick it was last read alive, its health, army and body index then, the tick a use
# last struck it, whether one of the steered person's did, and how near the nearest that did stood,
# in sub-cell units squared.
var _seen := PackedInt64Array()
var _health := PackedInt64Array()
# The most health each person has had, by person.
var _peak := PackedInt64Array()
var _army := PackedInt64Array()
var _body := PackedInt64Array()
var _struck_tick := PackedInt64Array()
var _struck_by_lead := PackedByteArray()
var _nearest := PackedFloat64Array()
# The person whose use struck each person nearest on the tick it was struck.
var _striker := PackedInt64Array()
# When the screen last began to shake, in milliseconds, and how far, in screen pixels.
var _shake_ms := NEVER
var _shake_pixels := 0
# By person: when it last lost health, in milliseconds, and the way it is knocked, as a unit.
var _hit_ms := PackedInt64Array()
var _away := PackedVector2Array()


## Starts on `world` as it stands, showing nothing: what each person holds is what the first tick
## is compared with, and only the events recorded after now are read.
func begin(world: WorldAdapter) -> void:
	accents.clear()
	strikes.clear()
	numbers.clear()
	deaths.clear()
	target = -1
	_tick = 0
	_resize(0)
	var events := world.events(0)
	_next_event = 0 if events.is_empty() else events[-1].sequence + 1
	var rosters: Array[PackedInt64Array] = []
	var positions: Array[PackedVector2Array] = []
	for army in world.army_count():
		rosters.append(world.roster(army))
		positions.append(world.body_positions(army))
	_read_bodies(world, rosters, positions, false)


## Reads what the tick just run did, at `now` in milliseconds: the uses the core recorded and whom
## they struck, who lost health since the tick before, and who died, and, for the lead of
## `steered_army` (-1 for none), the person its last use struck. `rosters` and `positions` are each
## army's roster and its bodies' positions as the tick left them, which world_art.gd has read
## already.
func note_tick(world: WorldAdapter, steered_army: int, now: int, rosters: Array[PackedInt64Array],
		positions: Array[PackedVector2Array]) -> void:
	_tick += 1
	_ms = now
	hit_stops.clear()
	_lead = -1
	if steered_army >= 0 and steered_army < rosters.size() and not rosters[steered_army].is_empty():
		_lead = rosters[steered_army][0]
	# A use that strikes only structures, a mend or a torch, shows in the structures' stages alone.
	for strike: Dictionary in world.strikes():
		if strike.body_damage > 0:
			strike.ms = now
			strike.steered = strike.person == _lead
			strikes.append(strike)
			_note_struck(strike)
	# The dead have left their rosters, so each is read as it stood on the tick before; one no use
	# struck died of hunger or age, and does not fall.
	for event: Dictionary in world.events(_next_event):
		_next_event = event.sequence + 1
		var person: int = event.get("person", -1)
		if event.kind == WorldAdapter.EVENT_DIED and person < _seen.size() and _seen[person] == _tick - 1 \
				and _struck_tick[person] == _tick:
			var stood := _positions[_army[person]][_body[person]]
			var at := _pixels(stood)
			_hit(person, _health[person], at)
			deaths.append({"person": person, "army": _army[person], "at": at,
				"ground": stood * CELL / WorldAdapter.CELL_SIZE, "ms": now})
	_read_bodies(world, rosters, positions, true)
	if blood:
		_drip()
	if selected >= 0 and selected < _seen.size() and _seen[selected] == _tick:
		target = selected
	elif target >= 0 and (_seen[target] != _tick or now - _target_ms > TARGET_MS):
		target = -1
	_forget(now)


## Returns the share of person `person`'s pixels drawn in the impact core's colour at `now`, from 1
## as it is struck to 0.
func flash(person: int, now: int) -> float:
	if person >= _hit_ms.size():
		return 0.0
	return clampf(1.0 - float(now - _hit_ms[person]) / FLASH_MS, 0.0, 1.0)


## Returns how far, in pixels, person `person` is drawn from where it stands at `now`.
func knock(person: int, now: int) -> Vector2:
	var left := flash(person, now)
	if left == 0.0:
		return Vector2.ZERO
	return _away[person] * KNOCK_PIXELS * left


## Returns each death still shown at `now`: its `army`, `at`, where it lies in pixels, `ground`,
## that place on the unraised ground, `fallen`,
## the share of a quarter turn it has fallen, `alpha`, how opaque it is, and `flash`, the share
## drawn in the impact core's colour as its killing blow lands.
func dying(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	for death: Dictionary in deaths:
		var age := float(now - death.ms) / DEATH_MS
		if age >= 0.0 and age < 1.0:
			shown.append({"army": death.army, "at": death.at, "ground": death.ground, "fallen": minf(age * 3.0, 1.0),
				"alpha": 1.0 - age, "flash": flash(death.person, now)})
	return shown


## Returns the army and body index the person stood at on the last tick, or (-1, -1) once it is
## not among the living.
func place_of(person: int) -> Vector2i:
	if person < 0 or person >= _seen.size() or _seen[person] != _tick:
		return Vector2i(-1, -1)
	return Vector2i(_army[person], _body[person])


## Draws what shows at `now` within `shown`, the part of the world on screen in pixels, with the
## steered person's target at `target_at`.
## Returns how wounded person `person` is drawn: 0 not, 1 wounded, below WOUNDED of the most health
## it has had, and 2 maimed, below MAIMED.
func wound(person: int) -> int:
	if person >= _peak.size() or _peak[person] <= 0:
		return 0
	var share := _health[person] * 1000 / _peak[person]
	return 2 if share < MAIMED else (1 if share < WOUNDED else 0)


## Returns how far, in whole screen pixels, the screen is shaken at `now` by the last hit on or from
## the steered person: a step of SHAKE_PATTERN, as far as the hit's size, for SHAKE_MS; none with
## the hit stop off.
func shake(now: int) -> Vector2i:
	if not hit_stop or now - _shake_ms >= SHAKE_MS or now < _shake_ms:
		return Vector2i.ZERO
	return SHAKE_PATTERN[((now - _shake_ms) / SHAKE_STEP_MS) % SHAKE_PATTERN.size()] * _shake_pixels


## Returns where pixel `k` of `spray` is at `now`, in pixels: thrown along the blow's direction,
## turned a little by the spray's seed, flying a few pixels further as it fades.
static func spray_pixel(spray: Dictionary, k: int, now: int) -> Vector2:
	var turn := (_unit(spray.seed, k) - 0.5) * 1.2
	var reach := 2.0 + k + clampf(float(now - spray.ms) / SPRAY_MS, 0.0, 1.0) * 4.0
	return (spray.at + (spray.dir as Vector2).rotated(turn) * reach).round()


## Returns the sprays of blood flying at `now` within the view.
func shown_sprays(now: int) -> Array[Dictionary]:
	return sprays.filter(func(spray: Dictionary) -> bool:
		return now - spray.ms < SPRAY_MS and view.grow(8.0).has_point(spray.at))


## Returns the drops of blood on the ground at `now` within the view, each with `left`, the share
## of its time it has left, from 1 as it falls to 0.
func shown_drops(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	for i in drop_at.size():
		if now - drop_ms[i] < DROP_MS and view.has_point(drop_at[i]):
			shown.append({"at": drop_at[i], "left": 1.0 - float(now - drop_ms[i]) / DROP_MS, "shade": drop_shade[i]})
	return shown


# A use struck `person`, standing at `at` in pixels: a spray from its middle in the blow's direction,
# and three drops on the ground beyond its feet that way.
func _bleed(person: int, at: Vector2) -> void:
	var seed := person * 7919 + _tick
	var dir: Vector2 = _away[person]
	if dir == Vector2.ZERO:
		dir = Vector2.RIGHT.rotated(_unit(seed, 9) * TAU)
	sprays.append({"ms": _ms, "at": at - Vector2(0, BODY_HEIGHT / 2.0), "dir": dir, "seed": seed})
	for k in 3:
		var aside := Vector2(_unit(seed, 20 + k) * 4.0 - 2.0, _unit(seed, 30 + k) * 2.0 - 1.0)
		_drop(at + Vector2(dir.x, dir.y * 0.75) * (3.0 + k * 2.5) + aside, k)


# Each maimed person on screen drips a drop at its feet every DRIP_TICKS ticks, its own tick apart.
func _drip() -> void:
	for army in _positions.size():
		var positions: PackedVector2Array = _positions[army]
		for body in positions.size():
			if army >= _rosters_read.size() or body >= _rosters_read[army].size():
				continue
			var person := _rosters_read[army][body]
			if (person + _tick) % DRIP_TICKS != 0 or wound(person) < 2:
				continue
			var at := _pixels(positions[body])
			if view.has_point(at):
				_drop(at + Vector2(_unit(person, _tick) * 4.0 - 2.0, 0.0), person)


func _drop(at: Vector2, shade: int) -> void:
	if drop_at.size() < MOST_DROPS:
		drop_at.append(at.round())
		drop_ms.append(_ms)
		drop_shade.append(shade % BLOOD.size())
	else:
		var i := drops_fallen % MOST_DROPS
		drop_at[i] = at.round()
		drop_ms[i] = _ms
		drop_shade[i] = shade % BLOOD.size()
	drops_fallen += 1


# A number in [0, 1) from `seed` and `k`, the same for the same pair.
static func _unit(seed: int, k: int) -> float:
	var h := (seed * 73856093) ^ (k * 19349663)
	h = (h ^ (h >> 13)) * 1274126177
	return float(absi(h ^ (h >> 16)) % 10007) / 10007.0


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


## Returns the damage numbers shown at `now` within the view, those damage_numbers names: each its
## `text`, where its baseline starts, `at`, in pixels, and its `colour`.
func shown_numbers(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	if damage_numbers == "off":
		return shown
	for number: Dictionary in numbers:
		var age := float(now - number.ms) / NUMBER_MS
		if age < 0.0 or age >= 1.0 or not view.has_point(number.at) \
				or (damage_numbers == "mine" and not number.mine):
			continue
		var colour := CRIMSON if number.own else PAPER
		colour.a = 1.0 - age * age
		shown.append({"text": number.text, "colour": colour,
			"at": (number.at + Vector2(-number.width / 2.0, -BODY_HEIGHT - NUMBER_RISE * age)).round()})
	return shown


func _draw_strike(strike: Dictionary, left: float) -> void:
	var colour := JADE if strike.steered else _accent(strike.person)
	var fill := Color(colour, 0.35 * left)
	var edge := Color(colour, 0.9 * left)
	var origin := _pixels(strike.origin)
	var toward := _pixels(strike.toward)
	var reach: float = strike.range * CELL.x / float(WorldAdapter.CELL_SIZE)
	var radius := maxf(strike.radius * CELL.x / float(WorldAdapter.CELL_SIZE), LEAST_RADIUS)
	# Distances on the ground are foreshortened down the screen: a disc on it is drawn as an ellipse.
	var flat := Vector2(1.0, CELL.y / CELL.x)
	match strike.shape:
		WorldAdapter.SHAPE_ARC:
			# An arc used toward its user's own place sweeps all around.
			var way: Vector2 = strike.toward - strike.origin
			var facing := way.angle() if way != Vector2.ZERO else 0.0
			var sweep := deg_to_rad(WorldAdapter.ARC_HALF_WIDTH_DEGREES) if way != Vector2.ZERO else PI
			var points := PackedVector2Array([origin])
			for n in 13:
				points.append(origin + Vector2.from_angle(facing - sweep + 2.0 * sweep * n / 12.0) * reach * flat)
			draw_colored_polygon(points, fill)
			draw_set_transform(origin, 0.0, flat)
			draw_arc(Vector2.ZERO, reach, facing - sweep, facing + sweep, 13, edge, 1.0)
			draw_set_transform(Vector2.ZERO)
		WorldAdapter.SHAPE_LINE, WorldAdapter.SHAPE_TARGETED:
			var end := _pixels(strike.end)
			draw_line(origin, end, fill, maxf(radius * 2.0 * flat.y, 1.0))
			draw_circle(origin.lerp(end, 1.0 - left), 1.5, edge)
		_:
			draw_set_transform(toward, 0.0, flat)
			draw_circle(Vector2.ZERO, radius, fill)
			draw_arc(Vector2.ZERO, radius, 0.0, TAU, 16, edge, 1.0)
			draw_set_transform(Vector2.ZERO)


# Corner brackets round the target's body, which stands BODY_HEIGHT pixels tall on `at`.
func _draw_target(at: Vector2) -> void:
	var box := Rect2(at + Vector2(-9, 3 - BODY_HEIGHT), Vector2(18, 21))
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


# Notes whom `strike` struck on this tick: the steered person's use marks the one it struck
# nearest where it went, and each person struck is knocked away from the nearest use that struck
# it, all as they stood on the tick before.
func _note_struck(strike: Dictionary) -> void:
	var best := INF
	for person: int in strike.struck:
		if person >= _seen.size() or _seen[person] != _tick - 1:
			continue
		var stood := _positions[_army[person]][_body[person]]
		var near := stood.distance_squared_to(strike.origin)
		if _struck_tick[person] != _tick:
			_struck_tick[person] = _tick
			_struck_by_lead[person] = 0
			_nearest[person] = INF
		if near < _nearest[person]:
			_nearest[person] = near
			_away[person] = (stood - strike.origin).normalized()
			_striker[person] = strike.person
		if strike.steered:
			_struck_by_lead[person] = 1
			if stood.distance_squared_to(strike.toward) < best:
				best = stood.distance_squared_to(strike.toward)
				target = person
				_target_ms = _ms


# Reads every living body as `rosters` and `positions` hold it, noting a loss of health since the
# tick before when `compare` is set.
func _read_bodies(world: WorldAdapter, rosters: Array[PackedInt64Array],
		positions: Array[PackedVector2Array], compare: bool) -> void:
	for army in rosters.size():
		var roster := rosters[army]
		var healths := world.body_strengths(army)
		for body in roster.size():
			var person := roster[body]
			if person >= _seen.size():
				_grow(person + 1)
			if compare and _seen[person] == _tick - 1 and healths[body] < _health[person]:
				_hit(person, _health[person] - healths[body], _pixels(positions[army][body]))
			_seen[person] = _tick
			_health[person] = healths[body]
			_peak[person] = maxi(_peak[person], healths[body])
			_army[person] = army
			_body[person] = body
	_positions = positions.duplicate()
	_rosters_read = rosters.duplicate()


# Notes that `person`, standing at `at` in pixels, lost `lost` health on this tick: it flashes, is
# knocked away when a use struck it, and its loss floats up when damage_numbers names it.
func _hit(person: int, lost: int, at: Vector2) -> void:
	var struck := _struck_tick[person] == _tick
	_hit_ms[person] = _ms
	if not struck:
		_away[person] = Vector2.ZERO
	if struck and blood:
		_bleed(person, at)
	var own := person == _lead
	var mine := own or (struck and _struck_by_lead[person] == 1)
	# A hit on or from the steered person holds the struck, and the one whose use struck it nearest.
	if hit_stop and struck and (own or _struck_by_lead[person] == 1):
		var frames := HIT_STOP_MOST
		for step: Array in HIT_STOP:
			if lost < step[0]:
				frames = step[1]
				break
		hit_stops.append({"person": person, "frames": frames})
		hit_stops.append({"person": _striker[person], "frames": frames})
		var pixels := SHAKE_MOST
		for step: Array in SHAKE:
			if lost < step[0]:
				pixels = step[1]
				break
		if _shake_ms != _ms:
			_shake_pixels = 0
		_shake_ms = _ms
		_shake_pixels = maxi(_shake_pixels, pixels)
	if damage_numbers == "everyone" or (damage_numbers == "mine" and mine):
		var text := str(lost)
		numbers.append({"ms": _ms, "text": text, "at": at, "own": own, "mine": mine,
			"width": FONT.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, FONT_SIZE).x})


# Drops what no longer shows at `now`.
func _forget(now: int) -> void:
	strikes = strikes.filter(func(strike: Dictionary) -> bool: return now - strike.ms < STRIKE_MS)
	sprays = sprays.filter(func(spray: Dictionary) -> bool: return now - spray.ms < SPRAY_MS)
	deaths = deaths.filter(func(death: Dictionary) -> bool: return now - death.ms < DEATH_MS)
	numbers = numbers.filter(func(number: Dictionary) -> bool: return now - number.ms < NUMBER_MS)


func _grow(size: int) -> void:
	var from := _seen.size()
	var to := maxi(size, from * 2)
	_resize(to)
	for person in range(from, to):
		_seen[person] = -2
		_struck_tick[person] = -1
		_hit_ms[person] = NEVER
		_peak[person] = 0


func _resize(to: int) -> void:
	_seen.resize(to)
	_health.resize(to)
	_peak.resize(to)
	_army.resize(to)
	_body.resize(to)
	_struck_tick.resize(to)
	_struck_by_lead.resize(to)
	_nearest.resize(to)
	_striker.resize(to)
	_hit_ms.resize(to)
	_away.resize(to)


func _pixels(sub_cells: Vector2) -> Vector2:
	if to_screen.is_valid():
		return to_screen.call(sub_cells)
	return sub_cells * CELL / WorldAdapter.CELL_SIZE
