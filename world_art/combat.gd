## Shows what the fighting of the last ticks did, in palette colours thinned by combat.gdshader's
## dither as they fade, blending nothing. What it shows by person is kept and drawn natively by
## `bodies`, a BodyView (extension/body_view.cpp), fed one WorldAdapter.snapshot() a tick: each use
## of a skill that strikes people flashes its shape in its user's faction's accent, the steered
## person's in jade and wherever they are; a person whose health fell flashes, is knocked back from
## the nearest use that struck it and its loss floats up as damage_numbers names it; a person a use
## struck who died falls and fades where it last stood; a hit on or from the steered person holds
## the struck and the striker still a few view frames and shakes the screen; and the person the
## player selected, or else the one the steered person's last use struck, is marked. This keeps
## the blood: a person a use struck bleeds, a spray from its middle in the blow's direction and
## drops on the ground that fade, and a maimed one on screen drips now and then, while blood is on.
## world_art.gd draws the bodies through the same BodyView. It reads the world and holds no game
## rule: who struck, what, where and whom is the core's.
extends Node2D

## A cell's width and depth on the ground, in pixels, as world_art.gd draws it.
const CELL := Vector2(16, 12)
## How far above where a person stands the top of its body is drawn, in pixels: its feet are on
## the 22nd of its frame's 24 rows.
const BODY_HEIGHT := 22.0
## How long a use, a flash, a death and a number show, in milliseconds, and how long a hit shakes
## the screen, as BodyView keeps them.
const STRIKE_MS := BodyView.STRIKE_MS
const FLASH_MS := BodyView.FLASH_MS
const DEATH_MS := BodyView.DEATH_MS
const NUMBER_MS := BodyView.NUMBER_MS
const SHAKE_MS := BodyView.SHAKE_MS
## How many hits a tick bleed beside those on or from the steered person, and how many ticks apart
## a maimed person drips, as BodyView decides them.
const MOST_BLEEDS := BodyView.MOST_BLEEDS
const DRIP_TICKS := BodyView.DRIP_TICKS
## How long a spray of blood flies, and a drop lies on the ground, in milliseconds; and the most
## drops kept, the oldest going first: every drop kept is drawn on every frame, and drawing them is
## what blood costs.
const SPRAY_MS := 400
const DROP_MS := 3000
const MOST_DROPS := 200
## The pixels a spray throws, and the most kept, the oldest going first, as many as MOST_BLEEDS and
## a hit of the steered person's throw over SPRAY_MS; and how far each flies
## from the struck body's side facing the blow, in pixels, between the two, a cell or more.
const SPRAY_PIXELS := 8
const MOST_SPRAY_PIXELS := 320
const SPRAY_REACH := Vector2(12.0, 22.0)
## The drops a hit lets fall beyond the struck body's feet in the blow's direction, and those it
## pools under them; and the shapes a drop takes, of the pixel clusters blood_pixels.gd draws.
const HIT_DROPS := 3
const POOL_DROPS := 2
const DROP_SHAPES := 3
## Blood, the crimson ramp's fourth and second steps, a drop's edge and its core: its first is too
## near black to read on the ground, and its fifth stays for hostility.
const BLOOD := [Color("a01b21"), Color("631648")]
const FONT := preload("res://hud/fonts/Galmuri9.ttf")
const FONT_SIZE := 10
# damage_numbers' modes, in BodyView's order.
const NUMBER_MODES := ["off", "mine", "everyone"]

## The offsets a shaken screen steps through.
static var SHAKE_PATTERN: Array[Vector2i] = BodyView.shake_pattern()

## The bodies and what the fighting does to them, kept natively.
var bodies := BodyView.new()
## Whose losses float up as numbers: "off", none; "mine", the steered person's own and those its
## uses struck; "everyone", every loss. The settings' damage numbers row sets it.
var damage_numbers := "mine":
	set(value):
		damage_numbers = value
		bodies.set_damage_numbers(NUMBER_MODES.find(value))
## Whether a hit draws blood; the settings' blood row sets it. Off, a hit shows only its flash.
var blood := true:
	set(value):
		blood = value
		bodies.set_blood(value)
## Whether a hit on or from the steered person holds the struck and the striker still a few frames
## and shakes the screen; the settings' hit stop row sets it.
var hit_stop := true:
	set(value):
		hit_stop = value
		bodies.set_hit_stop(value)
## The person the player selected, which play.gd sets, or -1 for none.
var selected := -1:
	set(value):
		selected = value
		bodies.set_selected(value)
## The person marked: the one selected while it lives, else the one the steered person's last use
## struck nearest where it went, or -1.
var target: int:
	get:
		return bodies.target()
## The view the last refresh showed, in pixels.
var view: Rect2:
	get:
		return bodies.view()
## The hit stops of the last tick: each `person` held and for how many view `frames`.
var hit_stops: Array:
	get:
		return bodies.hit_stops()
## Each use shown, as WorldAdapter.strikes() gives it, with `ms`, when it was recorded, and
## `steered`, whether the steered person made it.
var strikes: Array:
	get:
		return bodies.strikes()
## Each loss floating up: `ms`, `text`, its `width`, `at`, where the person stood, in pixels, `own`
## and `mine`.
var numbers: Array:
	get:
		return bodies.numbers()
## Each person who died struck: `person`, its `army`, `at`, where it last stood in pixels,
## `ground`, that place on the unraised ground, and `ms`, when.
var deaths: Array:
	get:
		return bodies.deaths()

## Each spray of blood flying, in order: `ms`, when, `at`, where it is thrown from in pixels, the
## struck body's side facing the blow, `dir`, the blow's direction, and `seed`.
var sprays: Array[Dictionary] = []
## The drops of blood on the ground, a ring of at most MOST_DROPS, a new drop taking the oldest's
## place: where each fell, in pixels, when, and its shape, below DROP_SHAPES. Read by
## blood_pixels.gd as they are, so a frame builds nothing; a drop older than DROP_MS shows no more.
var drop_at := PackedVector2Array()
var drop_ms := PackedInt64Array()
var drop_shape := PackedByteArray()
## How many drops have fallen, the ring's next place being this modulo MOST_DROPS.
var drops_fallen := 0
## The sprays' pixels, a ring of at most MOST_SPRAY_PIXELS like the drops': where each was thrown
## from, the struck body's middle, and how far it flies from there, in pixels, when, and its shade
## of BLOOD, the lighter, which reads on the ground; and how many have been thrown.
var spray_pixel_at := PackedVector2Array()
var spray_pixel_reach := PackedVector2Array()
var spray_pixel_ms := PackedInt64Array()
var spray_pixel_shade := PackedByteArray()
var spray_pixels_thrown := 0

var _ms := 0
# The sequence of the next event to read.
var _next_event := 0


func _init() -> void:
	bodies.set_font(FONT, FONT_SIZE)


## Starts on `world` as it stands, from its `snapshot` (read here when empty), showing nothing and
## holding no one, with no blood: what each person holds is what the first tick is compared with,
## and only the events recorded after now are read.
func begin(world: WorldAdapter, snapshot := {}) -> void:
	bodies.begin(snapshot if not snapshot.is_empty() else world.snapshot())
	var events := world.events(0)
	_next_event = 0 if events.is_empty() else events[-1].sequence + 1
	sprays.clear()
	drop_at.clear()
	drop_ms.clear()
	drop_shape.clear()
	drops_fallen = 0
	spray_pixel_at.clear()
	spray_pixel_reach.clear()
	spray_pixel_ms.clear()
	spray_pixel_shade.clear()
	spray_pixels_thrown = 0


## Reads what the tick just run did, at `now` in milliseconds, from `snapshot`, the world as the
## tick left it, and the deaths the core recorded since the last read; `steered_army` (-1 for
## none) is the army whose lead's hits are held and marked.
func note_tick(world: WorldAdapter, snapshot: Dictionary, steered_army: int, now: int) -> void:
	_ms = now
	var died := PackedInt64Array()
	for event: Dictionary in world.events(_next_event):
		_next_event = event.sequence + 1
		if event.kind == WorldAdapter.EVENT_DIED:
			died.append(event.get("person", -1))
	var laid: Dictionary = bodies.note_tick(snapshot, died, steered_army, now)
	var bleed_at: PackedVector2Array = laid.bleed_at
	var bleed_dir: PackedVector2Array = laid.bleed_dir
	var bleed_seed: PackedInt64Array = laid.bleed_seed
	for i in bleed_at.size():
		_bleed(bleed_at[i], bleed_dir[i], bleed_seed[i])
	var drip_at: PackedVector2Array = laid.drip_at
	var drip_shape: PackedInt64Array = laid.drip_shape
	for i in drip_at.size():
		_drop(drip_at[i], drip_shape[i])
	sprays = sprays.filter(func(spray: Dictionary) -> bool: return now - spray.ms < SPRAY_MS)


## Returns the share of person `person`'s pixels drawn in the impact core's colour at `now`, from 1
## as it is struck to 0.
func flash(person: int, now: int) -> float:
	return bodies.flash(person, now)


## Returns how far, in pixels, person `person` is drawn from where it stands at `now`.
func knock(person: int, now: int) -> Vector2:
	return bodies.knock(person, now)


## Returns each death still shown at `now`: its `army`, `at`, where it lies in pixels, `ground`,
## that place on the unraised ground, `fallen`, the share of a quarter turn it has fallen,
## `alpha`, how opaque it is, and `flash`, the share drawn in the impact core's colour.
func dying(now: int) -> Array:
	return bodies.dying(now)


## Returns the army and body index the person stood at on the last tick, or (-1, -1) once it is
## not among the living.
func place_of(person: int) -> Vector2i:
	return bodies.place_of(person)


## Returns how wounded person `person` is drawn: 0 not, 1 wounded and 2 maimed.
func wound(person: int) -> int:
	return bodies.wound(person)


## Returns how far, in whole screen pixels, the screen is shaken at `now` by the last hit on or from
## the steered person; none with the hit stop off.
func shake(now: int) -> Vector2i:
	return bodies.shake(now)


## Returns how far pixel `k` of `spray` flies from where it is thrown, in pixels: along the blow's
## direction turned a little by the spray's seed, SPRAY_REACH's span away.
static func spray_reach(spray: Dictionary, k: int) -> Vector2:
	var turn := (BodyView.unit(spray.seed, k) - 0.5) * 1.2
	var reach := lerpf(SPRAY_REACH.x, SPRAY_REACH.y, BodyView.unit(spray.seed, 40 + k))
	return (spray.dir as Vector2).rotated(turn) * reach


## Returns the sprays of blood flying at `now` within the view.
func shown_sprays(now: int) -> Array[Dictionary]:
	var shown := view.grow(8.0)
	return sprays.filter(func(spray: Dictionary) -> bool:
		return now - spray.ms < SPRAY_MS and shown.has_point(spray.at))


## Returns the drops of blood on the ground at `now` within the view, each with `left`, the share
## of its time it has left, from 1 as it falls to 0.
func shown_drops(now: int) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	var within := view
	for i in drop_at.size():
		if now - drop_ms[i] < DROP_MS and within.has_point(drop_at[i]):
			var left := 1.0 - float(now - drop_ms[i]) / DROP_MS
			shown.append({"at": drop_at[i], "left": left, "shape": drop_shape[i]})
	return shown


## Returns the uses shown at `now`: those within the view, and the steered person's wherever they
## are.
func shown_strikes(now: int) -> Array:
	return bodies.shown_strikes(now)


## Returns the damage numbers shown at `now` within the view, those damage_numbers names: each its
## `text`, where its baseline starts, `at`, in pixels, and its `colour`.
func shown_numbers(now: int) -> Array:
	return bodies.shown_numbers(now)


## Draws what shows at `now` within `shown`, the part of the world on screen in pixels, with the
## steered person's target at `target_at`.
func show_at(shown: Rect2, now: int, target_at: Vector2) -> void:
	bodies.show_at(shown, now, target_at)
	queue_redraw()


func _draw() -> void:
	bodies.draw_effects(get_canvas_item())


# A use struck someone standing at `at` in pixels, the blow going `dir`: a spray from its side
# facing the blow, in the blow's direction, HIT_DROPS drops on the ground beyond its feet that way,
# and POOL_DROPS under them.
func _bleed(at: Vector2, dir: Vector2, seed: int) -> void:
	if dir == Vector2.ZERO:
		dir = Vector2.RIGHT.rotated(BodyView.unit(seed, 9) * TAU)
	var from := at - Vector2(0, BODY_HEIGHT / 2.0) + dir * 4.0
	var spray := {"ms": _ms, "at": from, "dir": dir, "seed": seed}
	sprays.append(spray)
	for k in SPRAY_PIXELS:
		_throw(spray.at, spray_reach(spray, k), 0)
	for k in HIT_DROPS:
		var aside := Vector2(BodyView.unit(seed, 20 + k) * 4.0 - 2.0, BodyView.unit(seed, 30 + k) * 2.0 - 1.0)
		_drop(at + Vector2(dir.x, dir.y * 0.75) * (3.0 + k * 2.5) + aside, k)
	# Pooled just below the feet, where the body standing on them does not hide them.
	for k in POOL_DROPS:
		var side := 1.0 if k % 2 == 0 else -1.0
		_drop(at + Vector2(side * (1.0 + BodyView.unit(seed, 50 + k) * 3.0), 2.0 + BodyView.unit(seed, 60 + k)),
				seed + k)


func _throw(at: Vector2, reach: Vector2, shade: int) -> void:
	if spray_pixel_at.size() < MOST_SPRAY_PIXELS:
		spray_pixel_at.append(at)
		spray_pixel_reach.append(reach)
		spray_pixel_ms.append(_ms)
		spray_pixel_shade.append(shade)
	else:
		var i := spray_pixels_thrown % MOST_SPRAY_PIXELS
		spray_pixel_at[i] = at
		spray_pixel_reach[i] = reach
		spray_pixel_ms[i] = _ms
		spray_pixel_shade[i] = shade
	spray_pixels_thrown += 1


func _drop(at: Vector2, shape: int) -> void:
	if drop_at.size() < MOST_DROPS:
		drop_at.append(at.round())
		drop_ms.append(_ms)
		drop_shape.append(posmod(shape, DROP_SHAPES))
	else:
		var i := drops_fallen % MOST_DROPS
		drop_at[i] = at.round()
		drop_ms[i] = _ms
		drop_shape[i] = posmod(shape, DROP_SHAPES)
	drops_fallen += 1
