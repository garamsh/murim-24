# Checks BodyView on snapshots made by hand, two people of two armies two cells apart: a use by the
# first striking the second, which lost health on the tick, flashes the second white, knocks it away
# from the use, draws it wounded and floats its loss up, where the first, unhurt, shows none of it;
# a frame packs both bodies on screen, each with its shadow, and names the second near the first,
# where a view elsewhere packs none; beginning again shows nothing of the hit; and with a pixel scale
# set, a body between two places is drawn on whole screen pixels, where at 1 it is drawn on whole
# world pixels.
extends RefCounted

const CELL_SIZE := WorldAdapter.CELL_SIZE
const HEALTH := 100
const LEFT := 40
const NOW := 5000
const VIEW := Rect2(Vector2(-64, -64), Vector2(256, 128))
const ELSEWHERE := Rect2(Vector2(100000, 100000), Vector2(256, 128))


func run() -> bool:
	return _hit_pass() and _fill_pass() and _begin_pass() and _pixel_scale_pass()


# After the tick the second lost LEFT of HEALTH to the first's use from its left, it flashes fully,
# is knocked rightward, away from the use, is drawn wounded and its loss floats up as a number.
# Control: the first, unhurt, does not flash, is not knocked and is not wounded.
func _hit_pass() -> bool:
	var view := _hit_view()
	var victim := 1
	var striker := 0
	var numbers: Array = view.numbers().map(func(number: Dictionary) -> String: return number.text)
	var passed: bool = view.flash(victim, NOW) == 1.0 and view.knock(victim, NOW).x > 0.0 \
			and view.knock(victim, NOW).y == 0.0 and view.wound(victim) == 1 and numbers == [str(HEALTH - LEFT)] \
			and view.place_of(victim) == Vector2i(1, 0) \
			and view.flash(striker, NOW) == 0.0 and view.knock(striker, NOW) == Vector2.ZERO and view.wound(striker) == 0
	print("body_view_test: the struck at flash %.2f, knocked %s, wound %d, numbers %s, at %s; the striker at flash %.2f, knocked %s, wound %d: %s" % [
		view.flash(victim, NOW), view.knock(victim, NOW), view.wound(victim), numbers, view.place_of(victim),
		view.flash(striker, NOW), view.knock(striker, NOW), view.wound(striker), "pass" if passed else "FAIL"])
	return passed


# A frame over both packs both bodies, each after its shadow, and names the second, two cells from
# the viewer's lead. Control: a view elsewhere packs and names none.
func _fill_pass() -> bool:
	var view := _hit_view()
	view.set_viewer(0, PackedInt64Array([0, 1]))
	var shown: Dictionary = view.fill(VIEW, 1.0, NOW)
	var sprites := 0
	for band: PackedFloat32Array in shown.bands:
		sprites += band.size() / BodyView.BODY_FLOATS
	var away: Dictionary = view.fill(ELSEWHERE, 1.0, NOW)
	var away_sprites := 0
	for band: PackedFloat32Array in away.bands:
		away_sprites += band.size() / BodyView.BODY_FLOATS
	var passed: bool = shown.drawn == 2 and sprites == 4 and shown.near_person == PackedInt64Array([1]) \
			and shown.near_relation == PackedInt64Array([1]) and is_equal_approx(shown.near_cells[0], 2.0) \
			and away.drawn == 0 and away_sprites == 0 and away.near_person.is_empty()
	print("body_view_test: in view %d drawn as %d sprites, named %s at %s cells; elsewhere %d drawn as %d sprites, named %s: %s" % [
		shown.drawn, sprites, shown.near_person, shown.near_cells, away.drawn, away_sprites, away.near_person,
		"pass" if passed else "FAIL"])
	return passed


# Begun again on the same people, nothing of the hit shows: no flash, no number, no one held, no
# target. Control: before beginning again, the hit showed.
func _begin_pass() -> bool:
	var view := _hit_view()
	var before := [view.flash(1, NOW), view.numbers().size()]
	view.begin(_snapshot(LEFT, false))
	var after := [view.flash(1, NOW), view.numbers().size(), view.held_count(), view.target()]
	var passed: bool = before[0] == 1.0 and before[1] == 1 and after == [0.0, 0, 0, -1]
	print("body_view_test: the hit showed flash and numbers %s; begun again %s: %s" % [before, after,
		"pass" if passed else "FAIL"])
	return passed


# A BodyView begun on both at HEALTH, then given the tick the first's use left the second at LEFT,
# with every loss numbered and the first steered.
func _hit_view() -> BodyView:
	var view := BodyView.new()
	view.set_atlas(Vector2(64, 24), PackedInt64Array([0, 1, 1, 2, 0, 1, 0, 1]), PackedInt64Array([2, 3, 3, 3, 2, 3, 2, 3]),
			Vector2i(48, 48), PackedInt64Array([0, 0]), PackedColorArray([Color.WHITE]))
	view.set_damage_numbers(2)
	view.begin(_snapshot(HEALTH, false))
	view.show_at(VIEW, NOW, Vector2.ZERO)
	view.note_tick(_snapshot(LEFT, true), PackedInt64Array(), 0, NOW)
	return view


# The two people, the second at `health`, as WorldAdapter.snapshot() gives them, with the first's
# use striking the second from its left when `struck`.
func _snapshot(health: int, struck: bool) -> Dictionary:
	var at := Vector2(CELL_SIZE / 2, CELL_SIZE / 2)
	var uses := 1 if struck else 0
	return {
		"first": PackedInt64Array([0, 1, 2]),
		"person": PackedInt64Array([0, 1]),
		"position": PackedVector2Array([at, at + Vector2(2 * CELL_SIZE, 0)]),
		"health": PackedInt64Array([HEALTH, health]),
		"ready": PackedInt64Array([0, 0]),
		"faction": PackedInt64Array([0, 1]),
		"stage": PackedInt64Array(),
		"strike_person": PackedInt64Array([0]).slice(0, uses),
		"strike_shape": PackedInt64Array([WorldAdapter.SHAPE_ARC]).slice(0, uses),
		"strike_range": PackedInt64Array([3 * CELL_SIZE]).slice(0, uses),
		"strike_radius": PackedInt64Array([0]).slice(0, uses),
		"strike_body_damage": PackedInt64Array([HEALTH - LEFT]).slice(0, uses),
		"strike_origin": PackedVector2Array([at]).slice(0, uses),
		"strike_toward": PackedVector2Array([at + Vector2(2 * CELL_SIZE, 0)]).slice(0, uses),
		"strike_end": PackedVector2Array([at + Vector2(2 * CELL_SIZE, 0)]).slice(0, uses),
		"strike_first": PackedInt64Array([0, 1]).slice(0, uses + 1),
		"struck": PackedInt64Array([1]).slice(0, uses),
	}


# The steered person moves half a world pixel rightward over a tick and is drawn a quarter of the way
# from there, at 8.25 world pixels: with four screen pixels a world pixel its place and every sprite
# packed for it lie on quarter world pixels, 8.25 among them. Control: at the default scale of 1
# they lie on whole world pixels.
func _pixel_scale_pass() -> bool:
	var view := BodyView.new()
	view.set_atlas(Vector2(64, 24), PackedInt64Array([0, 1, 1, 2, 0, 1, 0, 1]), PackedInt64Array([2, 3, 3, 3, 2, 3, 2, 3]),
			Vector2i(48, 48), PackedInt64Array([0, 0]), PackedColorArray([Color.WHITE]))
	view.set_steered(0)
	var at := Vector2(CELL_SIZE / 2, CELL_SIZE / 2)
	view.begin(_walker(at))
	view.note_tick(_walker(at + Vector2(CELL_SIZE / 32, 0)), PackedInt64Array(), 0, NOW)
	var whole: Dictionary = view.fill(VIEW, 0.5, NOW)
	view.set_pixel_scale(4.0)
	var quarter: Dictionary = view.fill(VIEW, 0.5, NOW)
	var whole_xs := _origins(whole)
	var quarter_xs := _origins(quarter)
	var on := func(xs: PackedFloat32Array, step: float) -> bool:
		return xs.size() > 0 and Array(xs).all(func(x: float) -> bool: return is_equal_approx(x * step, roundf(x * step)))
	var passed: bool = is_equal_approx(whole.lead_at.x, roundf(whole.lead_at.x)) and on.call(whole_xs, 1.0) \
			and is_equal_approx(quarter.lead_at.x, 8.25) and on.call(quarter_xs, 4.0) \
			and not on.call(quarter_xs, 1.0)
	print("body_view_test: drawn a quarter of the way along half a pixel, at scale 1 the lead at %s and sprites at x %s; at scale 4 the lead at %s and sprites at x %s: %s" % [
		whole.lead_at, whole_xs, quarter.lead_at, quarter_xs, "pass" if passed else "FAIL"])
	return passed


# The x of every sprite `shown` packs, from each band's transforms.
static func _origins(shown: Dictionary) -> PackedFloat32Array:
	var xs := PackedFloat32Array()
	for band: PackedFloat32Array in shown.bands:
		for at in range(0, band.size(), BodyView.BODY_FLOATS):
			xs.append(band[at + 3])
	return xs


# The first person alone, standing at `at`, unhurt.
static func _walker(at: Vector2) -> Dictionary:
	return {
		"first": PackedInt64Array([0, 1]),
		"person": PackedInt64Array([0]),
		"position": PackedVector2Array([at]),
		"health": PackedInt64Array([HEALTH]),
		"ready": PackedInt64Array([0]),
		"faction": PackedInt64Array([0]),
		"stage": PackedInt64Array(),
		"strike_person": PackedInt64Array(),
		"strike_shape": PackedInt64Array(),
		"strike_range": PackedInt64Array(),
		"strike_radius": PackedInt64Array(),
		"strike_body_damage": PackedInt64Array(),
		"strike_origin": PackedVector2Array(),
		"strike_toward": PackedVector2Array(),
		"strike_end": PackedVector2Array(),
		"strike_first": PackedInt64Array([0]),
		"struck": PackedInt64Array(),
	}
