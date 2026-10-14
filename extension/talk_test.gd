# Checks that talking passes through the adapter as the core decides it: in a region made by
# create_region(), whose people talk within 2 cells, the player walked up to the nearest person may
# talk to it, and it offers its news first; it may not talk to a person beyond the reach, nor to itself.
# The person the core names for it to talk to is one it may talk to, none it may talk to standing
# nearer.
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
	var target := world.talk_target(player)
	var target_right := target >= 0 and _talkable(world, player, target)
	var here := world.person_position(player)
	for army in world.army_count():
		for person in world.roster(army):
			if target >= 0 and _talkable(world, player, person):
				target_right = target_right and here.distance_squared_to(world.person_position(person)) \
						>= here.distance_squared_to(world.person_position(target))
	var passed := near_refusal == WorldAdapter.TALK_NONE and far_refusal == WorldAdapter.TALK_OUT_OF_REACH \
			and self_refusal == WorldAdapter.TALK_SELF and offered[0] == WorldAdapter.OFFER_NEWS and target_right
	print("talk_test: person %d within the reach %d, person %d beyond it %d, the player itself %d; person %d offers %s; the core names person %d to talk to, none nearer %s: %s" % [
		near, near_refusal, far, far_refusal, self_refusal, near, offered, target, target_right,
		"pass" if passed else "FAIL"])
	return passed


# Whether `person` may talk to `other`, or would be met as a foe.
func _talkable(world: WorldAdapter, person: int, other: int) -> bool:
	var refusal := world.talk_refusal(person, other)
	return refusal == WorldAdapter.TALK_NONE or refusal == WorldAdapter.TALK_HOSTILE


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
