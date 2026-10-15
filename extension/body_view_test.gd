# Checks BodyView on snapshots made by hand, two people of two armies two cells apart: a use by the
# first striking the second, which lost health on the tick, flashes the second white, knocks it away
# from the use, draws it wounded and floats its loss up, where the first, unhurt, shows none of it;
# a frame packs both bodies on screen, each with its shadow, and names the second near the first,
# where a view elsewhere packs none; beginning again shows nothing of the hit; and with a pixel scale
# set, a body between two places is drawn on whole screen pixels, where at 1 it is drawn on whole
# world pixels. Each kind of use draws its user in its own motion (#184): a sweep a swing, a use at
# one person a lunge, a dart a throw, an area a cast, a mend and a torch theirs, and the person a use
# struck in its recoil; with no use the user stands idle. A loss rises in its art's colour, the
# player's own in crimson, and one of no art in paper. In a crowd (#355), a use's shape is drawn only
# for the player's own use and one that struck the player or the player's target; another's shows its
# effects alone, and with no one steered no use is shaped.
extends RefCounted

const CELL_SIZE := WorldAdapter.CELL_SIZE
const HEALTH := 100
const LEFT := 40
const NOW := 5000
const VIEW := Rect2(Vector2(-64, -64), Vector2(256, 128))
const ELSEWHERE := Rect2(Vector2(100000, 100000), Vector2(256, 128))


func run() -> bool:
	return _hit_pass() and _fill_pass() and _begin_pass() and _pixel_scale_pass() and _motion_pass() \
			and _number_colour_pass() and _crowd_pass()


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


# The tags in world_art.gd's BODY_TAGS order and, in a sheet made up for the test, the first frame of
# each, so a sprite's frame tells its tag.
const TAGS := ["idle", "walk", "back", "swing", "idle_hurt", "walk_hurt", "idle_maimed", "walk_maimed",
	"lunge", "cast", "throw", "mend", "torch", "recoil"]
const FIRSTS := [0, 1, 5, 9, 12, 13, 17, 18, 22, 24, 26, 28, 30, 32]
const SHEET_FRAMES := 40


# On the tick of each kind of use, its user is drawn in that use's motion: a sweep in swing, a use at
# one person in lunge, a dart in throw, an area in cast, a use that mends structures in mend and one
# that burns them in torch; the person a use struck is drawn in recoil. Control: on a tick with no
# use, the first stands idle.
func _motion_pass() -> bool:
	var cases := [[WorldAdapter.SHAPE_ARC, 20, 0, "swing"], [WorldAdapter.SHAPE_TARGETED, 20, 0, "lunge"],
		[WorldAdapter.SHAPE_LINE, 20, 0, "throw"], [WorldAdapter.SHAPE_AREA, 20, 0, "cast"],
		[WorldAdapter.SHAPE_SELF, 0, -5, "mend"], [WorldAdapter.SHAPE_SELF, 0, 10, "torch"]]
	var faults := []
	var drawn := []
	for case: Array in cases:
		var view := _tagged_view()
		var hits: bool = case[1] > 0
		view.note_tick(_used(case[0], case[1], case[2], hits), PackedInt64Array(), 0, NOW)
		var tags := _tags_of(view.fill(VIEW, 1.0, NOW))
		drawn.append(tags)
		var foe: String = "recoil" if hits else "idle"
		if tags != [case[3], foe]:
			faults.append("%s: %s" % [case[3], tags])
	var calm := _tagged_view()
	calm.note_tick(_used(WorldAdapter.SHAPE_ARC, 0, 0, false, false), PackedInt64Array(), 0, NOW)
	var still := _tags_of(calm.fill(VIEW, 1.0, NOW))
	var passed := faults.is_empty() and still == ["idle", "idle"]
	print("body_view_test: drawn on the use's tick %s; with no use %s; faults %s: %s" % [drawn, still, faults,
		"pass" if passed else "FAIL"])
	return passed


# A loss dealt by an art's use rises in that art's colour, and the player's own loss in crimson.
# Control: a loss dealt by a use of no art rises in paper.
func _number_colour_pass() -> bool:
	var colours := []
	# Each case's art, and the army steered: the striker's, or the struck's for the player's own loss.
	for case: Array in [[0, 0], [-1, 0], [0, 1]]:
		var view := _tagged_view()
		view.set_damage_numbers(2)
		view.set_palette(PackedColorArray([Color("efe5c9"), Color("171229"), Color("4bc89b"), Color("d6422e")]))
		view.set_technique_colours(PackedColorArray([Color("9ac4cd")]))
		var snapshot := _used(WorldAdapter.SHAPE_ARC, 20, 0, true)
		snapshot.strike_art = PackedInt64Array([case[0]])
		view.note_tick(snapshot, PackedInt64Array(), case[1], NOW)
		view.show_at(VIEW, NOW, Vector2.ZERO)
		var shown: Array = view.shown_numbers(NOW)
		var colour: Color = shown[0].colour if not shown.is_empty() else Color.BLACK
		colours.append(Color(colour.r, colour.g, colour.b).to_html(false))
	var passed: bool = colours == ["9ac4cd", "efe5c9", "d6422e"]
	print("body_view_test: a loss to an art's use rises in %s, to a use of no art in %s, the player's own in %s: %s" % [
		colours[0], colours[1], colours[2], "pass" if passed else "FAIL"])
	return passed


# A BodyView of SHEET_FRAMES frames a faction at the made-up FIRSTS, begun on both people standing.
func _tagged_view() -> BodyView:
	var view := BodyView.new()
	var outlines := PackedInt64Array()
	for first: int in FIRSTS:
		outlines.append(first)
	view.set_atlas(Vector2(SHEET_FRAMES * 16 * 2 + 16, 24), PackedInt64Array(FIRSTS), outlines,
			Vector2i(SHEET_FRAMES * 16, SHEET_FRAMES * 16 * 2), PackedInt64Array([0, 1]), PackedColorArray([Color.WHITE]))
	view.begin(_snapshot(HEALTH, false))
	view.show_at(VIEW, NOW, Vector2.ZERO)
	return view


# The two people as _snapshot() gives them, the first having used a skill of `shape` dealing
# `damage` to bodies and `works` to structures toward the second, which it struck when `hits`,
# leaving it at LEFT; or, without `used`, no use at all.
func _used(shape: int, damage: int, works: int, hits: bool, used := true) -> Dictionary:
	var snapshot := _snapshot(LEFT if hits else HEALTH, false)
	if not used:
		return snapshot
	var at := Vector2(CELL_SIZE / 2, CELL_SIZE / 2)
	var foe := at + Vector2(2 * CELL_SIZE, 0)
	snapshot.strike_person = PackedInt64Array([0])
	snapshot.strike_shape = PackedInt64Array([shape])
	snapshot.strike_range = PackedInt64Array([3 * CELL_SIZE])
	snapshot.strike_radius = PackedInt64Array([CELL_SIZE])
	snapshot.strike_body_damage = PackedInt64Array([damage])
	snapshot.strike_origin = PackedVector2Array([at])
	snapshot.strike_toward = PackedVector2Array([foe])
	snapshot.strike_end = PackedVector2Array([foe])
	snapshot.strike_first = PackedInt64Array([0, 1 if hits else 0])
	snapshot.struck = PackedInt64Array([1]) if hits else PackedInt64Array()
	snapshot.strike_art = PackedInt64Array([-1])
	snapshot.strike_structure_damage = PackedInt64Array([works])
	return snapshot


# The tag each of the two people is drawn in, from its body sprite's frame in `shown`'s bands, the
# first's then the second's by where they stand, left to right.
static func _tags_of(shown: Dictionary) -> Array:
	var found := []
	for band: PackedFloat32Array in shown.bands:
		for at in range(0, band.size(), BodyView.BODY_FLOATS):
			var frame := roundi(band[at + 12] * (SHEET_FRAMES * 16 * 2 + 16)) / 16
			if frame >= SHEET_FRAMES * 2:
				continue
			frame = frame % SHEET_FRAMES
			var tag := ""
			for n in FIRSTS.size():
				if frame >= FIRSTS[n]:
					tag = TAGS[n]
			found.append([band[at + 3], tag])
	found.sort_custom(func(a: Array, b: Array) -> bool: return a[0] < b[0])
	return found.map(func(entry: Array) -> String: return entry[1])


# In a crowd of five, each in an army of its own, on one tick: the steered first sweeps the second,
# making it its target; the third strikes the second; the fourth strikes the first; the fifth strikes
# the fourth. Every use is shown, and the first three are shaped, their shapes drawn, the fifth's not.
# Control: the same tick with no one steered shapes none.
func _crowd_pass() -> bool:
	var shaped := []
	for steered: int in [0, -1]:
		var view := BodyView.new()
		view.set_atlas(Vector2(64, 24), PackedInt64Array([0, 1, 1, 2, 0, 1, 0, 1]), PackedInt64Array([2, 3, 3, 3, 2, 3, 2, 3]),
				Vector2i(48, 48), PackedInt64Array([0, 0, 0, 0, 0]), PackedColorArray([Color.WHITE]))
		view.begin(_crowd(HEALTH, []))
		view.show_at(VIEW, NOW, Vector2.ZERO)
		view.note_tick(_crowd(LEFT, [[0, 1], [2, 1], [3, 0], [4, 3]]), PackedInt64Array(), steered, NOW)
		var shown: Array = view.shown_strikes(NOW)
		shaped.append([shown.size(), shown.map(func(strike: Dictionary) -> bool: return strike.shaped)])
	var passed: bool = shaped[0] == [4, [true, true, true, false]] and shaped[1] == [4, [false, false, false, false]]
	print("body_view_test: in a crowd the uses shown and shaped, the first steered %s, no one steered %s: %s" % [
		shaped[0], shaped[1], "pass" if passed else "FAIL"])
	return passed


# Five people a cell apart in a row, each its army's only body and faction, the struck of `uses`, each
# [user, struck] a sweep of the user's, left at `health`.
func _crowd(health: int, uses: Array) -> Dictionary:
	var positions := PackedVector2Array()
	var healths := PackedInt64Array()
	for person in 5:
		positions.append(Vector2(CELL_SIZE / 2 + person * CELL_SIZE, CELL_SIZE / 2))
		healths.append(health if uses.any(func(use: Array) -> bool: return use[1] == person) else HEALTH)
	var snapshot := {
		"first": PackedInt64Array([0, 1, 2, 3, 4, 5]), "person": PackedInt64Array([0, 1, 2, 3, 4]),
		"position": positions, "health": healths, "ready": PackedInt64Array([0, 0, 0, 0, 0]),
		"faction": PackedInt64Array([0, 1, 2, 3, 4]), "stage": PackedInt64Array(),
	}
	var fields := {"strike_person": PackedInt64Array(), "strike_shape": PackedInt64Array(),
		"strike_range": PackedInt64Array(), "strike_radius": PackedInt64Array(),
		"strike_body_damage": PackedInt64Array(), "strike_origin": PackedVector2Array(),
		"strike_toward": PackedVector2Array(), "strike_end": PackedVector2Array(),
		"strike_first": PackedInt64Array([0]), "struck": PackedInt64Array(), "strike_art": PackedInt64Array(),
		"strike_structure_damage": PackedInt64Array()}
	for use: Array in uses:
		fields.strike_person.append(use[0])
		fields.strike_shape.append(WorldAdapter.SHAPE_ARC)
		fields.strike_range.append(2 * CELL_SIZE)
		fields.strike_radius.append(0)
		fields.strike_body_damage.append(HEALTH - LEFT)
		fields.strike_origin.append(positions[use[0]])
		fields.strike_toward.append(positions[use[1]])
		fields.strike_end.append(positions[use[1]])
		fields.struck.append(use[1])
		fields.strike_first.append(fields.struck.size())
		fields.strike_art.append(-1)
		fields.strike_structure_damage.append(0)
	snapshot.merge(fields)
	return snapshot
