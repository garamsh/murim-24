# Checks that a region saved through the adapter to a slot of a directory loads as the same
# world: the same hash, the same hash after more ticks, and the player found again as the army its
# soul's body serves in, and the region's seed; that an empty slot loads nothing, and load_refusal
# names it missing, a slot of bytes no save begins with not a save, and a saved slot none; and
# that a world made without a region is not saved. Every slot is in the tests' own directory, and
# the game's slot 0 is left as it was.
extends RefCounted

const TestSaves := preload("res://extension/test_saves.gd")

const REGION_SEED := 0
const OTHER_SEED := 8
const TICKS_PER_DAY := 5
# Fifteen years of five-tick days: this region's growth forms an army after the player's in its
# fourteenth.
const LIVED_TICKS := 9000
const MORE_TICKS := 1000
const SLOT := 0
const OTHER_SLOT := 1
const EMPTY_SLOT := 2
const STAGE_FLOORS := [75, 25, 1]


func run() -> bool:
	var game_slot := TestSaves.game_slot()
	TestSaves.clear()
	var world := WorldAdapter.create_region(REGION_SEED, 0, TICKS_PER_DAY)
	_tick(world, LIVED_TICKS)
	var player := world.player_army()
	# The player is found from the core, not as the last army: growth has formed armies after it.
	var grown := player >= 0 and world.army_count() > player + 1
	var saved := world.save(SLOT, TestSaves.directory)
	var loaded := WorldAdapter.load(SLOT, TestSaves.directory)
	var same := loaded != null and loaded.hash() == world.hash() \
			and loaded.player_army() == player and loaded.villages() == world.villages() \
			and loaded.terrain() == world.terrain() and loaded.date() == world.date() \
			and loaded.region_seed() == REGION_SEED
	_tick(world, MORE_TICKS)
	if loaded != null:
		_tick(loaded, MORE_TICKS)
	var ticked_on := loaded != null and loaded.hash() == world.hash() \
			and loaded.player_army() == world.player_army()
	# Control: another region saved in another slot loads as itself, so equality is not every
	# load giving one world.
	var other := WorldAdapter.create_region(OTHER_SEED, 0, TICKS_PER_DAY)
	var other_saved := other.save(OTHER_SLOT, TestSaves.directory)
	var other_loaded := WorldAdapter.load(OTHER_SLOT, TestSaves.directory)
	var distinct := other_loaded != null and other_loaded.hash() == other.hash() \
			and other_loaded.hash() != world.hash()
	# An empty slot loads nothing, and a world without a region is not saved; control: the region
	# above was saved and loaded, so these refusals are the slot's and the world's.
	var empty_refused := WorldAdapter.load(EMPTY_SLOT, TestSaves.directory) == null \
			and WorldAdapter.load_refusal(EMPTY_SLOT, TestSaves.directory) == WorldAdapter.LOAD_MISSING
	# The refusal is the bytes', not every slot's: the slot saved above reads, and the same slot of
	# other bytes does not.
	var read := WorldAdapter.load_refusal(SLOT, TestSaves.directory) == WorldAdapter.LOAD_NONE
	var garbage := FileAccess.open(TestSaves.path(OTHER_SLOT), FileAccess.WRITE)
	garbage.store_string("not a save")
	garbage.close()
	var garbage_refused := WorldAdapter.load_refusal(OTHER_SLOT, TestSaves.directory) == WorldAdapter.LOAD_NOT_A_SAVE \
			and WorldAdapter.load(OTHER_SLOT, TestSaves.directory) == null
	var plain := WorldAdapter.create(1, PackedStringArray(["....", "...."]), [], [],
			{"separation": 0, "hostile": [[false]], "stage_floors": STAGE_FLOORS})
	var plain_refused := plain != null and not plain.save(EMPTY_SLOT, TestSaves.directory) and WorldAdapter.load(EMPTY_SLOT, TestSaves.directory) == null
	TestSaves.clear()
	# Every slot above was the test directory's: the game's own slot 0 is as it was.
	var untouched := TestSaves.game_slot() == game_slot
	print("file_saves_test: player army %d of %d armies, grown past it %s; saved %s, loaded with the same hash, player, villages, terrain, date and seed %s; the same after %d more ticks %s; another region saved %s and loaded as itself %s; an empty slot refused as missing %s; a saved slot read %s and other bytes refused as not a save %s; a world without a region refused %s; the game's slot 0 untouched %s: %s" % [
		player, world.army_count(), grown, saved, same, MORE_TICKS, ticked_on, other_saved, distinct,
		empty_refused, read, garbage_refused, plain_refused, untouched,
		"pass" if grown and saved and same and ticked_on and other_saved and distinct and empty_refused and read \
				and garbage_refused and plain_refused and untouched else "FAIL"])
	return grown and saved and same and ticked_on and other_saved and distinct and empty_refused and read \
			and garbage_refused and plain_refused and untouched


func _tick(world: WorldAdapter, ticks: int) -> void:
	for i in ticks:
		world.tick()
