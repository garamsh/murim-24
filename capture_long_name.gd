# Records hud.tscn alone, in English, over the region of seed SEED with the player born into its
# first Cult village, whose leader's given name, Cheonmyeong, is the widest the name data makes;
# the player challenges the leader at the first frame. play.tscn's seed is fixed, so the HUD is
# shown without the world drawn beneath it.
# Run: godot --path . --write-movie <dir>/long.png --quit-after 30 --script <this file>
extends SceneTree

const SEED := 11
var _world: WorldAdapter
var _hud: CanvasLayer
var _steered := 0


func _initialize() -> void:
	_world = WorldAdapter.create_region(SEED, 1)
	_steered = _world.army_count() - 1
	_hud = load("res://hud/hud.tscn").instantiate()
	root.add_child(_hud)
	_hud.show_player(_world, _steered)
	_hud.switch_locale()
	var faction := _world.army_faction(_steered)
	var seat := Vector2i((_world.person_position(_world.leader(faction)) / WorldAdapter.CELL_SIZE).floor())
	_world.order(_steered, seat, PackedInt64Array([0]), faction)


func _process(_delta: float) -> bool:
	_world.tick()
	_hud.refresh(_world, _steered, PackedInt64Array([0]))
	return false
