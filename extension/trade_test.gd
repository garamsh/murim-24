# Checks that trade passes through the adapter as the core decides it: in a region made by
# create_region(), the player, steered, walked up to the nearest person who sells the herb, is
# offered its wares, buys a herb for its price, and is refused the pill it cannot pay for, a herb at
# full health and a pill it holds none of; walked on to the nearest person who does not sell it,
# the same herb asked of that person is refused, where the seller's was sold.
extends RefCounted

const REACH := 2
const WALK_TICKS := 3000


func run() -> bool:
	var world := WorldAdapter.create_region(2026, 0)
	world.steer(world.player_army(), true)
	var player := world.roster(world.player_army())[0]
	var wares := world.wares()
	var names := wares.map(func(ware: Dictionary) -> String: return ware.name)
	if names != ["herb", "elixir"]:
		print("trade_test: the region's wares are %s, not the herb and the elixir: FAIL" % [names])
		return false
	var herb := names.find("herb")
	var pill := names.find("elixir")
	var seller := _nearest(world, player, func(person: int) -> bool: return world.sells(person, herb))
	var skills := PackedInt64Array(range(world.skill_count(world.player_army())))
	_walk_to(world, player, seller, skills)
	# Setup: a seller stands within the reach.
	if seller < 0 or _apart(world, player, seller) > REACH:
		print("trade_test: no seller within %d cells of the player: FAIL" % REACH)
		return false
	var silver := world.person_silver(player)
	var offered := world.offers(seller)
	var bought := world.buy(player, seller, herb)
	var held := world.person_held(player)
	var paid := silver - world.person_silver(player)
	var dear := world.buy_refusal(player, seller, pill)
	var full := world.consume_refusal(player, herb)
	var none := world.consume_refusal(player, pill)
	var other := _nearest(world, player, func(person: int) -> bool: return not world.sells(person, herb))
	_walk_to(world, player, other, skills)
	# Setup: someone who sells no herb stands within the reach in turn.
	if other < 0 or _apart(world, player, other) > REACH:
		print("trade_test: no person who sells no herb within %d cells of the player: FAIL" % REACH)
		return false
	var not_sold := world.buy_refusal(player, other, herb)
	var passed: bool = offered == PackedInt64Array([WorldAdapter.OFFER_NEWS, WorldAdapter.OFFER_TRADE]) and bought \
			and paid == wares[herb].price and held == PackedInt64Array([1, 0]) \
			and not_sold == WorldAdapter.BUY_NOT_SOLD \
			and dear == WorldAdapter.BUY_NO_SILVER and full == WorldAdapter.CONSUME_FULL \
			and none == WorldAdapter.CONSUME_NONE_HELD
	print("trade_test: person %d offers %s; the player paid %d of %d silver for a herb, holds %s; the pill %d, a herb at full health %d, a pill held by none %d; person %d, who sells no herb, %d: %s" % [
		seller, offered, paid, silver, held, dear, full, none, other, not_sold, "pass" if passed else "FAIL"])
	return passed


# Walks the steered player `player` up to `person`, for at most WALK_TICKS ticks, until it stands
# within REACH of it.
func _walk_to(world: WorldAdapter, player: int, person: int, skills: PackedInt64Array) -> void:
	for tick in WALK_TICKS:
		if person < 0 or _apart(world, player, person) <= REACH:
			return
		if tick % 20 == 0:
			world.order(world.player_army(), _cell(world, person), skills)
		world.tick()


# The living person nearest `person` for whom `test` holds, other than itself, or -1.
func _nearest(world: WorldAdapter, person: int, test: Callable) -> int:
	var nearest := -1
	for army in world.army_count():
		for other in world.roster(army):
			if other != person and test.call(other) \
					and (nearest < 0 or _apart(world, person, other) < _apart(world, person, nearest)):
				nearest = other
	return nearest


func _apart(world: WorldAdapter, one: int, other: int) -> int:
	var apart := (_cell(world, one) - _cell(world, other)).abs()
	return maxi(apart.x, apart.y)


func _cell(world: WorldAdapter, person: int) -> Vector2i:
	return Vector2i((world.person_position(person) / WorldAdapter.CELL_SIZE).floor())
