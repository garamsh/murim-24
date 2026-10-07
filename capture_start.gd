# Records play.tscn as the play session opens on seed 2026, the player born in village 0:
# play_start.png, the first frame after the scene settles, and play_genealogy.png, the same moment
# with G opening the genealogy panel on the player. Prints the player's name and family, and the
# people of its village with their spouses. Fails if the two images repeat.
# Run: godot --path . --script <this file> -- <dir>
extends SceneTree

const SETTLE_FRAMES := 10
var _play: Node
var _dir := "."
var _frame := 0
var _saved := {}


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	var args := OS.get_cmdline_user_args()
	_dir = args[0] if args.size() > 0 else "."
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame == 2:
		_play.set_process(false)
	if _frame == SETTLE_FRAMES:
		_shot("play_start")
		var key := InputEventKey.new()
		key.keycode = KEY_G
		key.pressed = true
		_play.command(key)
	if _frame == 2 * SETTLE_FRAMES:
		_shot("play_genealogy")
		return true
	return false


func _shot(name: String) -> void:
	var world: WorldAdapter = _play.world
	var player: int = world.roster(_play.steered)[0]
	var village: Dictionary = world.villages()[0]
	var people := []
	for person in world.roster(0):
		var named := world.person_name(person)
		people.append("%s %s, spouse %d" % [named.surname_en, named.given_en, world.person_spouse(person)])
	var image := root.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_start: %s.png repeats %s.png" % [name, _saved[digest]])
		quit(1)
	_saved[digest] = name
	image.save_png("%s/%s.png" % [_dir, name])
	var named := world.person_name(player)
	print("capture_start: %s.png at tick 0, sha256 %s; player %d %s %s of family %d in %s, whose residents are %s" % [
		name, digest.left(16), player, named.surname_en, named.given_en, world.person_family(player), village.name, people])
