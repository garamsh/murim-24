# Records hud.tscn alone over the region of seed SEED with the player born into its first Cult
# village, whose leader, Hwang Cheonmyeong, bears the widest given name the name data makes. The
# player challenges the leader at the first frame, and the genealogy panel opens on the leader at
# GENEALOGY, in Korean; F2 at ENGLISH switches to English, and G at CLOSED closes the panel so the
# leader and duel panels show. play.tscn's seed is fixed, so the world is not drawn beneath the HUD.
# Run: godot --path . --write-movie <dir>/names.png --quit-after 120 --script <this file>
extends SceneTree

const SEED := 11
const GENEALOGY := 10
const ENGLISH := 40
const CLOSED := 80
var _world: WorldAdapter
var _hud: CanvasLayer
var _steered := 0
var _frame := 0


func _initialize() -> void:
	_world = WorldAdapter.create_region(SEED, 1)
	_steered = _world.army_count() - 1
	_hud = load("res://hud/hud.tscn").instantiate()
	root.add_child(_hud)
	_hud.show_player(_world, _steered)
	var faction := _world.army_faction(_steered)
	var seat := Vector2i((_world.person_position(_world.leader(faction)) / WorldAdapter.CELL_SIZE).floor())
	_world.order(_steered, seat, PackedInt64Array([0]), faction)


func _process(_delta: float) -> bool:
	_frame += 1
	_world.tick()
	_hud.refresh(_world, _steered, PackedInt64Array([0]))
	var leader := _world.leader(_world.army_faction(_steered))
	if _frame == GENEALOGY:
		_hud.genealogy.show_person(_world, leader, _world.roster(_steered)[0])
	if _frame == ENGLISH:
		_hud.switch_locale()
	if _frame == CLOSED:
		_hud.genealogy.close()
	return false
