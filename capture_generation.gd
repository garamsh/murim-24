# Records play.tscn's genealogy panel for the PR's evidence. Before recording, the region lives
# DAYS days so that families form. At OWN, G opens the panel on the steered person, whose card
# carries the jade frame and the 본인 tag. At FAMILY the panel opens on the first person with a
# spouse and a child, and at CHILD the selection moves down and Enter centres it on the child,
# whose header reads the second generation.
# Run: godot --path . --write-movie <dir>/generation.png --quit-after 120 --script <this file>
extends SceneTree

const DAYS := 50
const TICKS_PER_DAY := 1800
const OWN := 10
const FAMILY := 40
const CHILD := 70
var _frame := 0
var _play: Node


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _key(keycode: Key) -> InputEventKey:
	var key := InputEventKey.new()
	key.keycode = keycode
	key.pressed = true
	return key


func _process(_delta: float) -> bool:
	_frame += 1
	var world: WorldAdapter = _play.world
	var panel: Control = _play.get_node("Hud").genealogy
	if _frame == 1:
		for i in DAYS * TICKS_PER_DAY:
			_play.step()
	if _frame == OWN:
		_play.command(_key(KEY_G))
	if _frame == FAMILY:
		var own: int = world.roster(_play.steered)[0]
		for army in world.army_count():
			for person in world.roster(army):
				if panel.centre == own and world.person_spouse(person) >= 0 and world.person_children(person).size() > 0:
					panel.show_person(world, person, own)
	if _frame == CHILD:
		panel.move(Vector2i.DOWN)
		panel.centre_selected()
	return false
