## The saves directory the tests keep their slots in, apart from the game's, and a reading of the
## game's own slot 0 so a test can show it left that slot as it was.
extends RefCounted

const DIRECTORY := "user://test_saves"


## Removes every file in DIRECTORY.
static func clear() -> void:
	for file in DirAccess.get_files_at(DIRECTORY):
		DirAccess.remove_absolute(DIRECTORY.path_join(file))


## Returns the path of slot `slot` in DIRECTORY, as the adapter's files name it.
static func path(slot: int) -> String:
	return DIRECTORY.path_join("slot_%d.sav" % slot)


## Returns the game's slot 0 as it stands: "absent", or the SHA-256 of its bytes.
static func game_slot() -> String:
	var game := WorldAdapter.saves_directory().path_join("slot_0.sav")
	if not FileAccess.file_exists(game):
		return "absent"
	return FileAccess.get_sha256(game)
