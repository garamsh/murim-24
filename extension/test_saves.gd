## Where the tests keep what they write, apart from the game's saves and from every other test run:
## a directory of its own under the system's temp folder for each run of run_tests.gd, which
## use_fresh() makes before any test runs and the system removes as the run ends. Two runs at once,
## in two worktrees, never share one. Also a reading of the game's own slot 0, so a test can show it
## left that slot as it was.
extends RefCounted

## The variable naming another directory the run uses instead, for a check that wants two runs to
## share one on purpose.
const OVERRIDE := "MURIM_TEST_SAVES"

## The run's directory, set by use_fresh().
static var directory := ""
# Kept for the whole run: the system removes the directory once this is released.
static var _held: DirAccess


## Makes the run's directory: the one OVERRIDE names, or else a fresh one under the system's temp
## folder.
static func use_fresh() -> void:
	var named := OS.get_environment(OVERRIDE)
	if not named.is_empty():
		DirAccess.make_dir_recursive_absolute(named)
		directory = named
		return
	_held = DirAccess.create_temp("murim_tests", false)
	directory = _held.get_current_dir() if _held != null else ""


## Removes every file in the run's directory.
static func clear() -> void:
	for file in DirAccess.get_files_at(directory):
		DirAccess.remove_absolute(directory.path_join(file))


## Returns the path of slot `slot` in the run's directory, as the adapter's files name it.
static func path(slot: int) -> String:
	return directory.path_join("slot_%d.sav" % slot)


## Returns the path of a file named `name` in the run's directory.
static func file(name: String) -> String:
	return directory.path_join(name)


## Returns the game's slot 0 as it stands: "absent", or the SHA-256 of its bytes.
static func game_slot() -> String:
	var game := WorldAdapter.saves_directory().path_join("slot_0.sav")
	if not FileAccess.file_exists(game):
		return "absent"
	return FileAccess.get_sha256(game)
