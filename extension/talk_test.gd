# Checks that talking passes through the adapter as the core decides it: in a region made by
# create_region(), whose people talk within 2 cells, the player walked up to the nearest person may
# talk to it, and it offers its news; it may not talk to a person beyond the reach, nor to itself.
extends RefCounted

const REACH := 2
# The ticks the player is given to walk up to the nearest person.
const WALK_TICKS := 3000


func run() -> bool:
	var world := WorldAdapter.create_region(2026, 0)
	var player := world.roster(world.player_army())[0]
	var near := _nearest(world, player)
	var skills := PackedInt64Array(range(world.skill_count(world.player_army())))
	for tick in WALK_TICKS:
		if _apart(world, player, near) <= REACH:
			break
		if tick % 20 == 0:
			world.order(world.player_army(), _cell(world, near), skills)
		world.tick()
	var far := -1
	for army in world.army_count():
		for person in world.roster(army):
			if far < 0 and _apart(world, player, person) > REACH:
				far = person
	# Setup: the region holds a person within the reach of the player and one beyond it.
	var found := _apart(world, player, near) <= REACH and far >= 0
	if not found:
		print("talk_test: no person within %d cells and none beyond: FAIL" % REACH)
		return false
	var near_refusal := world.talk_refusal(player, near)
	var far_refusal := world.talk_refusal(player, far)
	var self_refusal := world.talk_refusal(player, player)
	var offered := world.offers(near)
	var passed := near_refusal == WorldAdapter.TALK_NONE and far_refusal == WorldAdapter.TALK_OUT_OF_REACH \
			and self_refusal == WorldAdapter.TALK_SELF and offered == PackedInt64Array([WorldAdapter.OFFER_NEWS])
	print("talk_test: person %d within the reach %d, person %d beyond it %d, the player itself %d; person %d offers %s: %s" % [
		near, near_refusal, far, far_refusal, self_refusal, near, offered, "pass" if passed else "FAIL"])
	return passed


# The living person nearest `person`, other than itself.
func _nearest(world: WorldAdapter, person: int) -> int:
	var nearest := -1
	for army in world.army_count():
		for other in world.roster(army):
			if other != person and (nearest < 0 or _apart(world, person, other) < _apart(world, person, nearest)):
				nearest = other
	return nearest


# How many cells apart two people stand, along the farther axis, as the core measures a reach.
func _apart(world: WorldAdapter, one: int, other: int) -> int:
	var apart := (_cell(world, one) - _cell(world, other)).abs()
	return maxi(apart.x, apart.y)


func _cell(world: WorldAdapter, person: int) -> Vector2i:
	return Vector2i((world.person_position(person) / WorldAdapter.CELL_SIZE).floor())
