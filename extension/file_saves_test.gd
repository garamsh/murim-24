# Checks that a region saved through the adapter to user://saves loads as the same world: the same
# hash, the same hash after more ticks, and the player found again as the army its soul's body
# serves in; that an empty slot loads nothing; and that a world made without a region is not saved.
extends RefCounted

const REGION_SEED := 7
const OTHER_SEED := 8
const TICKS_PER_DAY := 5
# Fifteen years of five-tick days: this region's growth forms an army after the player's in its
# thirteenth.
const LIVED_TICKS := 9000
const MORE_TICKS := 1000
const SLOT := 0
const OTHER_SLOT := 1
const EMPTY_SLOT := 2
const STAGE_FLOORS := [75, 25, 1]


func run() -> bool:
	_remove(SLOT)
	_remove(OTHER_SLOT)
	_remove(EMPTY_SLOT)
	var world := WorldAdapter.create_region(REGION_SEED, 0, TICKS_PER_DAY)
	_tick(world, LIVED_TICKS)
	var player := world.player_army()
	# The player is found from the core, not as the last army: growth has formed armies after it.
	var grown := player >= 0 and world.army_count() > player + 1
	var saved := world.save(SLOT)
	var loaded := WorldAdapter.load(SLOT)
	var same := loaded != null and loaded.hash() == world.hash() \
			and loaded.player_army() == player and loaded.villages() == world.villages() \
			and loaded.terrain() == world.terrain() and loaded.date() == world.date()
	_tick(world, MORE_TICKS)
	if loaded != null:
		_tick(loaded, MORE_TICKS)
	var ticked_on := loaded != null and loaded.hash() == world.hash() \
			and loaded.player_army() == world.player_army()
	# Control: another region saved in another slot loads as itself, so equality is not every
	# load giving one world.
	var other := WorldAdapter.create_region(OTHER_SEED, 0, TICKS_PER_DAY)
	var other_saved := other.save(OTHER_SLOT)
	var other_loaded := WorldAdapter.load(OTHER_SLOT)
	var distinct := other_loaded != null and other_loaded.hash() == other.hash() \
			and other_loaded.hash() != world.hash()
	# An empty slot loads nothing, and a world without a region is not saved; control: the region
	# above was saved and loaded, so these refusals are the slot's and the world's.
	var empty_refused := WorldAdapter.load(EMPTY_SLOT) == null
	var plain := WorldAdapter.create(1, PackedStringArray(["....", "...."]), [], [],
			{"separation": 0, "hostile": [[false]], "stage_floors": STAGE_FLOORS})
	var plain_refused := plain != null and not plain.save(EMPTY_SLOT) and WorldAdapter.load(EMPTY_SLOT) == null
	_remove(SLOT)
	_remove(OTHER_SLOT)
	print("file_saves_test: player army %d of %d armies, grown past it %s; saved %s, loaded with the same hash, player, villages, terrain and date %s; the same after %d more ticks %s; another region saved %s and loaded as itself %s; an empty slot refused %s; a world without a region refused %s: %s" % [
		player, world.army_count(), grown, saved, same, MORE_TICKS, ticked_on, other_saved, distinct,
		empty_refused, plain_refused,
		"pass" if grown and saved and same and ticked_on and other_saved and distinct and empty_refused and plain_refused else "FAIL"])
	return grown and saved and same and ticked_on and other_saved and distinct and empty_refused and plain_refused


func _tick(world: WorldAdapter, ticks: int) -> void:
	for i in ticks:
		world.tick()


func _remove(slot: int) -> void:
	var path := "user://saves/slot_%d.sav" % slot
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(path)
