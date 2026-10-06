# Draws the whole region of each seed named after `--` through world_art.tscn, terrain, landforms,
# villages and bodies, into a 2048 x 2048 viewport, one 16-pixel tile per cell, and saves it as
# region_<seed>.png in the first argument's directory. Prints each kind's share of the cells and
# the image's hash; the run fails if two images repeat.
# Run: godot --path . --script <this file> -- <dir> <seed>...
extends SceneTree

const SIDE := 2048
var _dir := "."
var _seeds: Array[int] = []
var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _frame := 0
var _saved := {}


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	_dir = args[0]
	for i in range(1, args.size()):
		_seeds.append(int(args[i]))
	_viewport = SubViewport.new()
	_viewport.size = Vector2i(SIDE, SIDE)
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(_viewport)


func _process(_delta: float) -> bool:
	if _frame == 0:
		if _seeds.is_empty():
			return true
		if _art != null:
			_art.queue_free()
		_world = WorldAdapter.create_region(_seeds[0])
		_art = load("res://world_art/world_art.tscn").instantiate()
		_viewport.add_child(_art)
		_art.show_world(_world, -1)
		_art.refresh(Rect2(Vector2.ZERO, Vector2(SIDE, SIDE)))
	_frame += 1
	if _frame < 5:
		return false
	var seed: int = _seeds.pop_front()
	var image := _viewport.get_texture().get_image()
	var digest := image.get_data().hex_encode().sha256_text()
	if _saved.has(digest):
		push_error("capture_region: region_%d.png repeats region_%d.png" % [seed, _saved[digest]])
		quit(1)
	_saved[digest] = seed
	image.save_png("%s/region_%d.png" % [_dir, seed])
	var names := _world.terrain_names()
	var counts := {}
	for kind in _world.terrain():
		counts[names[kind]] = counts.get(names[kind], 0) + 1
	var shares := []
	for name in names:
		shares.append("%s %d%%" % [name, roundi(100.0 * counts.get(name, 0) / _world.terrain().size())])
	print("capture_region: region_%d.png, %d villages, %d structures, sha256 %s, cells %s" % [
		seed, _world.villages().size(), _world.structure_count(), digest.left(16), ", ".join(shares)])
	_frame = 0
	return false
